// H5Reader.cpp
#include "H5Reader.h"

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

#endif
