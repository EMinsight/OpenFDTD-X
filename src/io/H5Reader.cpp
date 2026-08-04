// H5Reader.cpp
#include "H5Reader.h"

#include <QRegularExpression>
#include <QStringList>
#include <algorithm>
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

namespace {

// ── 旧レイアウト (/data%06d + /metadata) の共通処理 ─────────────────────────
struct OldGrid {
    qlonglong nx = 0, ny = 0, nz = 0, ni = 0, nj = 0, nk = 0, n0 = 0, nn = 0;
};

bool readOldGrid(const QString &path, OldGrid &g)
{
    auto scalarInt = [&](const char *name, qlonglong *out) {
        QVector<double> v;
        QVector<qlonglong> d;
        if (!H5Reader::readAll(path,
                QStringLiteral("/metadata/%1").arg(QLatin1String(name)), v, d)
            || v.size() != 1)
            return false;
        *out = qlonglong(v[0]);
        return true;
    };
    return scalarInt("Nx", &g.nx) && scalarInt("Ny", &g.ny) &&
           scalarInt("Nz", &g.nz) && scalarInt("Ni", &g.ni) &&
           scalarInt("Nj", &g.nj) && scalarInt("Nk", &g.nk) &&
           scalarInt("N0", &g.n0) && scalarInt("NN", &g.nn) &&
           g.nx > 0 && g.ny > 0 && g.nz > 0 && g.nn > 0;
}

// /data%06d/<comp> を反復番号の昇順で列挙する
QStringList oldGroupSeries(const QString &path, const QString &comp)
{
    QVector<H5DatasetInfo> infos;
    if (!H5Reader::listDatasets(path, infos)) return QStringList();
    const QRegularExpression re(
        QStringLiteral("^/data(\\d+)/%1$").arg(comp));
    QVector<QPair<qlonglong, QString>> hits;
    for (const H5DatasetInfo &di : infos) {
        const QRegularExpressionMatch m = re.match(di.path);
        if (m.hasMatch() && di.dims.size() == 4)
            hits.append({ m.captured(1).toLongLong(), di.path });
    }
    std::sort(hits.begin(), hits.end());
    QStringList out;
    for (const auto &h : hits) out << h.second;
    return out;
}

// {1,F,NN,6} のノード場 → z 中央断面 (成分 RSS)。行 0 = +y 側
bool oldSliceFrom(const QString &path, const QString &dset, const OldGrid &g,
                  QVector<double> &cells, int &rows, int &cols, QString *err)
{
    QVector<double> e;
    QVector<qlonglong> dims;
    if (!H5Reader::readAll(path, dset, e, dims, err)) return false;
    if (dims.size() != 4 || dims[2] != g.nn || dims[3] != 6) {
        setErr(err, QStringLiteral("%1: unexpected shape").arg(dset));
        return false;
    }
    const qlonglong k = g.nz / 2;
    cols = int(g.nx + 1);
    rows = int(g.ny + 1);
    cells.resize(cols * rows);
    for (qlonglong j = 0; j <= g.ny; ++j) {
        for (qlonglong i = 0; i <= g.nx; ++i) {
            const qlonglong node = g.ni * i + g.nj * j + g.nk * k + g.n0;
            if (node < 0 || node >= g.nn) {
                setErr(err, QStringLiteral("node index out of range"));
                cells.clear();
                rows = cols = 0;
                return false;
            }
            const qlonglong base = node * 6;
            double s = 0.0;
            for (int c = 0; c < 6; ++c) {
                const double v = e[int(base + c)];
                s += v * v;
            }
            cells[int((g.ny - j) * (g.nx + 1) + i)] = std::sqrt(s);
        }
    }
    return true;
}

} // namespace

bool H5Reader::readOfdMidSlice(const QString &path, QVector<double> &cells,
                               int &rows, int &cols, QString *groupName,
                               QString *err)
{
    cells.clear();
    rows = cols = 0;

    // ── (a) 新レイアウト: /freqdomain/E {F, Nx+1, Ny+1, Nz+1, 3, 2} ────────
    // 既に (i,j,k) の空間形状なので k 中央断面だけをハイパースラブで読む
    // (大規模格子で全体を読まない)
    {
        Ids id;
        id.file = H5Fopen(path.toLocal8Bit().constData(), H5F_ACC_RDONLY,
                          H5P_DEFAULT);
        if (id.file < 0) {
            setErr(err, QStringLiteral("cannot open %1").arg(path));
            return false;
        }
        if (H5Lexists(id.file, "/freqdomain", H5P_DEFAULT) > 0 &&
            H5Lexists(id.file, "/freqdomain/E", H5P_DEFAULT) > 0) {
            id.dset = H5Dopen2(id.file, "/freqdomain/E", H5P_DEFAULT);
            if (id.dset >= 0) {
                id.space = H5Dget_space(id.dset);
                hsize_t d[6];
                if (H5Sget_simple_extent_ndims(id.space) == 6 &&
                    H5Sget_simple_extent_dims(id.space, d, nullptr) == 6 &&
                    d[0] >= 1 && d[4] == 3 && d[5] == 2) {
                    const hsize_t nx1 = d[1], ny1 = d[2], nz1 = d[3];
                    const hsize_t kmid = (nz1 > 0) ? (nz1 - 1) / 2 : 0;
                    const hsize_t start[6] = { 0, 0, 0, kmid, 0, 0 };
                    const hsize_t count[6] = { 1, nx1, ny1, 1, 3, 2 };
                    H5Sselect_hyperslab(id.space, H5S_SELECT_SET, start,
                                        nullptr, count, nullptr);
                    const hsize_t mdims[3] = { nx1, ny1, 6 };
                    id.mem = H5Screate_simple(3, mdims, nullptr);
                    QVector<double> buf(int(nx1 * ny1 * 6));
                    if (H5Dread(id.dset, H5T_NATIVE_DOUBLE, id.mem, id.space,
                                H5P_DEFAULT, buf.data()) < 0) {
                        setErr(err,
                               QStringLiteral("read failed: /freqdomain/E"));
                        return false;
                    }
                    cols = int(nx1);
                    rows = int(ny1);
                    cells.resize(cols * rows);
                    for (hsize_t i = 0; i < nx1; ++i) {
                        for (hsize_t j = 0; j < ny1; ++j) {
                            const qlonglong base =
                                (qlonglong(i) * qlonglong(ny1) +
                                 qlonglong(j)) * 6;
                            double s = 0.0;
                            for (int c = 0; c < 6; ++c) {
                                const double v = buf[int(base + c)];
                                s += v * v;
                            }
                            // 行 0 = +y 側 (ヒートマップは行順で描画)
                            cells[int((ny1 - 1 - j) * nx1 + i)] =
                                std::sqrt(s);
                        }
                    }
                    if (groupName)
                        *groupName = QStringLiteral("freqdomain");
                    return true;
                }
            }
        }
    }

    // ── (b) 旧レイアウト: 最終 /data%06d/E + /metadata 格子定数 ─────────────
    const QStringList groups = oldGroupSeries(path, QStringLiteral("E"));
    if (groups.isEmpty()) {
        setErr(err, QStringLiteral("no /data%06d/E dataset"));
        return false;
    }
    OldGrid g;
    if (!readOldGrid(path, g)) {
        setErr(err, QStringLiteral("missing /metadata grid constants"));
        return false;
    }
    if (!oldSliceFrom(path, groups.last(), g, cells, rows, cols, err))
        return false;
    if (groupName)
        *groupName = groups.last().section(QLatin1Char('/'), 1, 1);
    return true;
}

bool H5Reader::ofdSeriesInfo(const QString &path, const QString &comp,
                             H5OfdSeriesInfo &out, QString *err)
{
    out = H5OfdSeriesInfo();

    // ── 新: /timeseries/<comp> {Nt, Nx+1, Ny+1, Nz+1, 3} ───────────────────
    {
        Ids id;
        id.file = H5Fopen(path.toLocal8Bit().constData(), H5F_ACC_RDONLY,
                          H5P_DEFAULT);
        if (id.file < 0) {
            setErr(err, QStringLiteral("cannot open %1").arg(path));
            return false;
        }
        const QByteArray ds = (QStringLiteral("/timeseries/") + comp).toUtf8();
        if (H5Lexists(id.file, "/timeseries", H5P_DEFAULT) > 0 &&
            H5Lexists(id.file, ds.constData(), H5P_DEFAULT) > 0) {
            id.dset = H5Dopen2(id.file, ds.constData(), H5P_DEFAULT);
            if (id.dset >= 0) {
                id.space = H5Dget_space(id.dset);
                hsize_t d[5];
                if (H5Sget_simple_extent_ndims(id.space) == 5 &&
                    H5Sget_simple_extent_dims(id.space, d, nullptr) == 5 &&
                    d[4] == 3) {
                    out.frames = int(d[0]);
                    out.cols = int(d[1]);
                    out.rows = int(d[2]);
                    out.instantaneous = true;
                    return true;
                }
            }
        }
    }

    // ── 旧: /data%06d/<comp> のグループ列 ───────────────────────────────────
    const QStringList groups = oldGroupSeries(path, comp);
    if (groups.isEmpty()) {
        setErr(err, QStringLiteral("no time series for %1").arg(comp));
        return false;
    }
    OldGrid g;
    if (!readOldGrid(path, g)) {
        setErr(err, QStringLiteral("missing /metadata grid constants"));
        return false;
    }
    out.frames = groups.size();
    out.cols = int(g.nx + 1);
    out.rows = int(g.ny + 1);
    out.instantaneous = false;
    return true;
}

bool H5Reader::readOfdSeriesFrame(const QString &path, const QString &comp,
                                  int frame, QVector<double> &cells,
                                  int &rows, int &cols, QString *label,
                                  QString *err)
{
    cells.clear();
    rows = cols = 0;

    // ── 新: /timeseries/<comp> の 1 フレームをハイパースラブで読む ──────────
    {
        Ids id;
        id.file = H5Fopen(path.toLocal8Bit().constData(), H5F_ACC_RDONLY,
                          H5P_DEFAULT);
        if (id.file < 0) {
            setErr(err, QStringLiteral("cannot open %1").arg(path));
            return false;
        }
        const QByteArray ds = (QStringLiteral("/timeseries/") + comp).toUtf8();
        if (H5Lexists(id.file, "/timeseries", H5P_DEFAULT) > 0 &&
            H5Lexists(id.file, ds.constData(), H5P_DEFAULT) > 0) {
            id.dset = H5Dopen2(id.file, ds.constData(), H5P_DEFAULT);
            if (id.dset >= 0) {
                id.space = H5Dget_space(id.dset);
                hsize_t d[5];
                if (H5Sget_simple_extent_ndims(id.space) == 5 &&
                    H5Sget_simple_extent_dims(id.space, d, nullptr) == 5 &&
                    d[4] == 3) {
                    if (frame < 0 || hsize_t(frame) >= d[0]) {
                        setErr(err, QStringLiteral("frame %1 out of range")
                                        .arg(frame));
                        return false;
                    }
                    const hsize_t nx1 = d[1], ny1 = d[2], nz1 = d[3];
                    const hsize_t kmid = (nz1 > 0) ? (nz1 - 1) / 2 : 0;
                    const hsize_t start[5] = { hsize_t(frame), 0, 0, kmid, 0 };
                    const hsize_t count[5] = { 1, nx1, ny1, 1, 3 };
                    H5Sselect_hyperslab(id.space, H5S_SELECT_SET, start,
                                        nullptr, count, nullptr);
                    const hsize_t mdims[3] = { nx1, ny1, 3 };
                    id.mem = H5Screate_simple(3, mdims, nullptr);
                    QVector<double> buf(int(nx1 * ny1 * 3));
                    if (H5Dread(id.dset, H5T_NATIVE_DOUBLE, id.mem, id.space,
                                H5P_DEFAULT, buf.data()) < 0) {
                        setErr(err, QStringLiteral("read failed: %1")
                                        .arg(QString::fromUtf8(ds)));
                        return false;
                    }
                    cols = int(nx1);
                    rows = int(ny1);
                    cells.resize(cols * rows);
                    for (hsize_t i = 0; i < nx1; ++i) {
                        for (hsize_t j = 0; j < ny1; ++j) {
                            const qlonglong base =
                                (qlonglong(i) * qlonglong(ny1) +
                                 qlonglong(j)) * 3;
                            double s = 0.0;
                            for (int c = 0; c < 3; ++c) {
                                const double v = buf[int(base + c)];
                                s += v * v;
                            }
                            cells[int((ny1 - 1 - j) * nx1 + i)] =
                                std::sqrt(s);
                        }
                    }
                    if (label) {
                        // 時刻 [s] (E は time、H は time_H — 半ステップずれ)
                        QVector<double> t;
                        QVector<qlonglong> td;
                        const QString tname = (comp == QLatin1String("H"))
                            ? QStringLiteral("/timeseries/time_H")
                            : QStringLiteral("/timeseries/time");
                        if (readAll(path, tname, t, td) && frame < t.size())
                            *label = QStringLiteral("t = %1 s")
                                .arg(QString::number(t[frame], 'g', 4));
                        else
                            *label = QStringLiteral("frame %1").arg(frame);
                    }
                    return true;
                }
            }
        }
    }

    // ── 旧: frame 番目の /data%06d/<comp> グループ ──────────────────────────
    const QStringList groups = oldGroupSeries(path, comp);
    if (groups.isEmpty()) {
        setErr(err, QStringLiteral("no time series for %1").arg(comp));
        return false;
    }
    if (frame < 0 || frame >= groups.size()) {
        setErr(err, QStringLiteral("frame %1 out of range").arg(frame));
        return false;
    }
    OldGrid g;
    if (!readOldGrid(path, g)) {
        setErr(err, QStringLiteral("missing /metadata grid constants"));
        return false;
    }
    if (!oldSliceFrom(path, groups[frame], g, cells, rows, cols, err))
        return false;
    if (label)
        *label = groups[frame].section(QLatin1Char('/'), 1, 1);
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

bool H5Reader::ofdSeriesInfo(const QString &, const QString &,
                             H5OfdSeriesInfo &, QString *err)
{
    if (err) *err = QStringLiteral("built without HDF5 (USE_HDF5=OFF)");
    return false;
}

bool H5Reader::readOfdSeriesFrame(const QString &, const QString &, int,
                                  QVector<double> &, int &, int &,
                                  QString *, QString *err)
{
    if (err) *err = QStringLiteral("built without HDF5 (USE_HDF5=OFF)");
    return false;
}

#endif
