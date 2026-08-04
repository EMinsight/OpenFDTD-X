// H5Reader.cpp
#include "H5Reader.h"

#include <QRegularExpression>
#include <cmath>

#ifdef OFD_USE_HDF5
#include <hdf5.h>
#endif

using namespace ofd;

bool H5Reader::available()
{
#ifdef OFD_USE_HDF5
    return true;
#else
    return false;
#endif
}

#ifdef OFD_USE_HDF5

namespace {

void setErr(QString *err, const QString &msg)
{
    if (err) *err = msg;
}

QString typeNameOf(hid_t dset)
{
    const hid_t t = H5Dget_type(dset);
    QString name = QStringLiteral("other");
    const H5T_class_t cls = H5Tget_class(t);
    const size_t sz = H5Tget_size(t);
    if (cls == H5T_FLOAT)
        name = (sz == 4) ? QStringLiteral("float32") : QStringLiteral("float64");
    else if (cls == H5T_INTEGER)
        name = QStringLiteral("int%1").arg(qulonglong(sz * 8));
    else if (cls == H5T_STRING)
        name = QStringLiteral("string");
    else if (cls == H5T_COMPOUND)
        name = QStringLiteral("compound");
    H5Tclose(t);
    return name;
}

// H5Ovisit のコールバック: データセットだけを収集する
herr_t visitCb(hid_t obj, const char *name, const H5O_info_t *info, void *op)
{
    if (info->type != H5O_TYPE_DATASET) return 0;
    auto *out = static_cast<QVector<H5DatasetInfo> *>(op);

    // ルートからの絶対パスに正規化 (visit は "." を返すことがある)
    QString path = QString::fromUtf8(name);
    if (path == QLatin1String(".")) return 0;
    if (!path.startsWith(QLatin1Char('/'))) path.prepend(QLatin1Char('/'));

    const hid_t dset = H5Dopen2(obj, name, H5P_DEFAULT);
    if (dset < 0) return 0;
    H5DatasetInfo di;
    di.path = path;
    di.typeName = typeNameOf(dset);
    const hid_t space = H5Dget_space(dset);
    const int nd = H5Sget_simple_extent_ndims(space);
    if (nd > 0) {
        QVector<hsize_t> dims(nd);
        H5Sget_simple_extent_dims(space, dims.data(), nullptr);
        for (const hsize_t d : dims) di.dims.push_back(qlonglong(d));
    }
    H5Sclose(space);
    H5Dclose(dset);
    out->push_back(di);
    return 0;
}

// 開いた HDF5 の id をスコープ終了時にまとめて閉じる (エラー経路の後始末)
struct Ids {
    hid_t file = -1, dset = -1, space = -1, mem = -1;
    ~Ids()
    {
        if (mem >= 0) H5Sclose(mem);
        if (space >= 0) H5Sclose(space);
        if (dset >= 0) H5Dclose(dset);
        if (file >= 0) H5Fclose(file);
    }
};

} // namespace

bool H5Reader::listDatasets(const QString &path, QVector<H5DatasetInfo> &out,
                            QString *err)
{
    out.clear();
    const hid_t file = H5Fopen(path.toLocal8Bit().constData(),
                               H5F_ACC_RDONLY, H5P_DEFAULT);
    if (file < 0) {
        setErr(err, QStringLiteral("cannot open %1").arg(path));
        return false;
    }
#if H5_VERSION_GE(1, 12, 0)
    H5Ovisit3(file, H5_INDEX_NAME, H5_ITER_NATIVE, visitCb, &out,
              H5O_INFO_BASIC);
#else
    H5Ovisit(file, H5_INDEX_NAME, H5_ITER_NATIVE, visitCb, &out);
#endif
    H5Fclose(file);
    return true;
}

bool H5Reader::read2D(const QString &path, const QString &dset,
                      QVector<double> &out, int &rows, int &cols, QString *err)
{
    Ids id;
    id.file = H5Fopen(path.toLocal8Bit().constData(), H5F_ACC_RDONLY,
                      H5P_DEFAULT);
    if (id.file < 0) { setErr(err, QStringLiteral("cannot open %1").arg(path)); return false; }
    id.dset = H5Dopen2(id.file, dset.toUtf8().constData(), H5P_DEFAULT);
    if (id.dset < 0) { setErr(err, QStringLiteral("no dataset %1").arg(dset)); return false; }
    id.space = H5Dget_space(id.dset);
    if (H5Sget_simple_extent_ndims(id.space) != 2) {
        setErr(err, QStringLiteral("%1 is not 2-D").arg(dset));
        return false;
    }
    hsize_t dims[2];
    H5Sget_simple_extent_dims(id.space, dims, nullptr);
    rows = int(dims[0]);
    cols = int(dims[1]);
    out.resize(rows * cols);
    // 型は HDF5 側が double へ変換してくれる (float32 でも可)
    if (H5Dread(id.dset, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT,
                out.data()) < 0) {
        setErr(err, QStringLiteral("read failed: %1").arg(dset));
        return false;
    }
    return true;
}

bool H5Reader::readFrame(const QString &path, const QString &dset, int frame,
                         QVector<double> &out, int &rows, int &cols,
                         QString *err)
{
    Ids id;
    id.file = H5Fopen(path.toLocal8Bit().constData(), H5F_ACC_RDONLY,
                      H5P_DEFAULT);
    if (id.file < 0) { setErr(err, QStringLiteral("cannot open %1").arg(path)); return false; }
    id.dset = H5Dopen2(id.file, dset.toUtf8().constData(), H5P_DEFAULT);
    if (id.dset < 0) { setErr(err, QStringLiteral("no dataset %1").arg(dset)); return false; }
    id.space = H5Dget_space(id.dset);
    if (H5Sget_simple_extent_ndims(id.space) != 3) {
        setErr(err, QStringLiteral("%1 is not 3-D").arg(dset));
        return false;
    }
    hsize_t dims[3];
    H5Sget_simple_extent_dims(id.space, dims, nullptr);
    if (frame < 0 || hsize_t(frame) >= dims[0]) {
        setErr(err, QStringLiteral("frame %1 out of range").arg(frame));
        return false;
    }
    rows = int(dims[1]);
    cols = int(dims[2]);
    out.resize(rows * cols);

    const hsize_t start[3] = { hsize_t(frame), 0, 0 };
    const hsize_t count[3] = { 1, dims[1], dims[2] };
    H5Sselect_hyperslab(id.space, H5S_SELECT_SET, start, nullptr, count,
                        nullptr);
    const hsize_t memDims[2] = { dims[1], dims[2] };
    id.mem = H5Screate_simple(2, memDims, nullptr);
    if (H5Dread(id.dset, H5T_NATIVE_DOUBLE, id.mem, id.space, H5P_DEFAULT,
                out.data()) < 0) {
        setErr(err, QStringLiteral("read failed: %1").arg(dset));
        return false;
    }
    return true;
}

bool H5Reader::readAll(const QString &path, const QString &dset,
                       QVector<double> &out, QVector<qlonglong> &dims,
                       QString *err)
{
    out.clear();
    dims.clear();
    Ids id;
    id.file = H5Fopen(path.toLocal8Bit().constData(), H5F_ACC_RDONLY,
                      H5P_DEFAULT);
    if (id.file < 0) { setErr(err, QStringLiteral("cannot open %1").arg(path)); return false; }
    id.dset = H5Dopen2(id.file, dset.toUtf8().constData(), H5P_DEFAULT);
    if (id.dset < 0) { setErr(err, QStringLiteral("no dataset %1").arg(dset)); return false; }
    id.space = H5Dget_space(id.dset);
    const int nd = H5Sget_simple_extent_ndims(id.space);
    if (nd < 0 || nd > 4) {
        setErr(err, QStringLiteral("%1: unsupported rank %2").arg(dset).arg(nd));
        return false;
    }
    qlonglong total = 1;
    if (nd > 0) {
        QVector<hsize_t> hdims(nd);
        H5Sget_simple_extent_dims(id.space, hdims.data(), nullptr);
        for (const hsize_t d : hdims) {
            dims.push_back(qlonglong(d));
            total *= qlonglong(d);
        }
    }
    if (total <= 0) return true;    // 空データセット (dims は返す)
    out.resize(int(total));
    if (H5Dread(id.dset, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT,
                out.data()) < 0) {
        setErr(err, QStringLiteral("read failed: %1").arg(dset));
        return false;
    }
    return true;
}

bool H5Reader::readOfdMidSlice(const QString &path, QVector<double> &cells,
                               int &rows, int &cols, QString *groupName,
                               QString *err)
{
    cells.clear();
    rows = cols = 0;

    QVector<H5DatasetInfo> infos;
    if (!listDatasets(path, infos, err)) return false;

    // 最終 /dataNNNNNN/E を選ぶ (グループ名の反復番号が最大のもの)
    static const QRegularExpression re(QStringLiteral("^/data(\\d+)/E$"));
    QString ePath;
    qlonglong best = -1;
    for (const H5DatasetInfo &di : infos) {
        const QRegularExpressionMatch m = re.match(di.path);
        if (!m.hasMatch() || di.dims.size() != 4) continue;
        const qlonglong n = m.captured(1).toLongLong();
        if (n > best) { best = n; ePath = di.path; }
    }
    if (ePath.isEmpty()) {
        setErr(err, QStringLiteral("no /data%06d/E dataset"));
        return false;
    }

    // 格子メタデータ (整数スカラー) — 揃っていなければ再構成できない
    auto scalarInt = [&](const char *name, qlonglong *out) {
        QVector<double> v;
        QVector<qlonglong> d;
        if (!readAll(path,
                     QStringLiteral("/metadata/%1").arg(QLatin1String(name)),
                     v, d)
            || v.size() != 1)
            return false;
        *out = qlonglong(v[0]);
        return true;
    };
    qlonglong nx = 0, ny = 0, nz = 0, ni = 0, nj = 0, nk = 0, n0 = 0, nn = 0;
    if (!scalarInt("Nx", &nx) || !scalarInt("Ny", &ny) ||
        !scalarInt("Nz", &nz) || !scalarInt("Ni", &ni) ||
        !scalarInt("Nj", &nj) || !scalarInt("Nk", &nk) ||
        !scalarInt("N0", &n0) || !scalarInt("NN", &nn)) {
        setErr(err, QStringLiteral("missing /metadata grid constants"));
        return false;
    }
    if (nx <= 0 || ny <= 0 || nz <= 0 || nn <= 0) {
        setErr(err, QStringLiteral("invalid grid dimensions"));
        return false;
    }

    QVector<double> e;
    QVector<qlonglong> dims;
    if (!readAll(path, ePath, e, dims, err)) return false;
    if (dims.size() != 4 || dims[2] != nn || dims[3] != 6) {
        setErr(err, QStringLiteral("%1: unexpected shape").arg(ePath));
        return false;
    }
    const qlonglong f = 0;                     // frequency2 の第 1 点
    const qlonglong k = nz / 2;                // z 中央断面

    cols = int(nx + 1);
    rows = int(ny + 1);
    cells.resize(cols * rows);
    for (qlonglong j = 0; j <= ny; ++j) {
        for (qlonglong i = 0; i <= nx; ++i) {
            const qlonglong node = ni * i + nj * j + nk * k + n0;
            if (node < 0 || node >= nn) {
                setErr(err, QStringLiteral("node index out of range"));
                cells.clear();
                rows = cols = 0;
                return false;
            }
            const qlonglong base = (f * nn + node) * 6;
            double s = 0.0;
            for (int c = 0; c < 6; ++c) {
                const double v = e[int(base + c)];
                s += v * v;
            }
            // 行 0 を画面上側 = +y にする (ヒートマップは行順で描画)
            cells[int((ny - j) * (nx + 1) + i)] = std::sqrt(s);
        }
    }
    if (groupName) *groupName = ePath.section(QLatin1Char('/'), 1, 1);
    return true;
}

#else // !OFD_USE_HDF5 — スタブ (available() = false を見て呼び出し側が抑止)

bool H5Reader::listDatasets(const QString &, QVector<H5DatasetInfo> &out,
                            QString *err)
{
    out.clear();
    if (err) *err = QStringLiteral("built without HDF5 (USE_HDF5=OFF)");
    return false;
}

bool H5Reader::read2D(const QString &, const QString &, QVector<double> &,
                      int &, int &, QString *err)
{
    if (err) *err = QStringLiteral("built without HDF5 (USE_HDF5=OFF)");
    return false;
}

bool H5Reader::readFrame(const QString &, const QString &, int,
                         QVector<double> &, int &, int &, QString *err)
{
    if (err) *err = QStringLiteral("built without HDF5 (USE_HDF5=OFF)");
    return false;
}

bool H5Reader::readAll(const QString &, const QString &, QVector<double> &,
                       QVector<qlonglong> &, QString *err)
{
    if (err) *err = QStringLiteral("built without HDF5 (USE_HDF5=OFF)");
    return false;
}

bool H5Reader::readOfdMidSlice(const QString &, QVector<double> &,
                               int &, int &, QString *, QString *err)
{
    if (err) *err = QStringLiteral("built without HDF5 (USE_HDF5=OFF)");
    return false;
}

#endif
