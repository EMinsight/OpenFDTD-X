// H5Writer.cpp
#include "H5Writer.h"
#include "../core/Project.h"

#ifdef OFD_USE_HDF5
#include <hdf5.h>
#endif

#include <algorithm>

using namespace ofd;

bool H5Writer::available()
{
#ifdef OFD_USE_HDF5
    return true;
#else
    return false;
#endif
}

#ifdef OFD_USE_HDF5

static bool writeDoubleArray(hid_t file, const char *name,
                             const QVector<double> &data)
{
    const hsize_t dims[1] = { hsize_t(data.size()) };
    hid_t space = H5Screate_simple(1, dims, nullptr);
    hid_t dset = H5Dcreate2(file, name, H5T_NATIVE_DOUBLE, space,
                            H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    const herr_t st = H5Dwrite(dset, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL,
                               H5P_DEFAULT, data.constData());
    H5Dclose(dset);
    H5Sclose(space);
    return st >= 0;
}

bool H5Writer::write(const QString &path, const Project &project,
                     const QVector<int> &steps,
                     const QVector<double> &eAvg,
                     const QVector<double> &hAvg,
                     QString *err)
{
    hid_t file = H5Fcreate(path.toLocal8Bit().constData(),
                           H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
    if (file < 0) {
        if (err) *err = "cannot create " + path;
        return false;
    }

    bool ok = true;
    // /mesh/{x,y,z}_nodes
    H5Gcreate2(file, "/mesh", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    static const char *names[3] = { "/mesh/x_nodes", "/mesh/y_nodes", "/mesh/z_nodes" };
    for (int a = 0; a < 3; ++a)
        ok = ok && writeDoubleArray(file, names[a], project.mesh(a).nodes);

    // /convergence/{step,e_avg,h_avg}
    H5Gcreate2(file, "/convergence", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    QVector<double> stepsD;
    stepsD.reserve(steps.size());
    for (int s : steps) stepsD.push_back(double(s));
    ok = ok && writeDoubleArray(file, "/convergence/step", stepsD);
    ok = ok && writeDoubleArray(file, "/convergence/e_avg", eAvg);
    ok = ok && writeDoubleArray(file, "/convergence/h_avg", hAvg);

    H5Fclose(file);
    if (!ok && err) *err = "HDF5 write failed";
    return ok;
}

// ── 属性 (単位・面名など) ────────────────────────────────────────────────
// 数値だけ入れた .h5 は「この列が何なのか」を外部で読めない。書き出し形式
// なので、単位と面名は属性で必ず添える。
static bool attrDouble(hid_t obj, const char *name, double v)
{
    const hid_t sp = H5Screate(H5S_SCALAR);
    const hid_t at = H5Acreate2(obj, name, H5T_NATIVE_DOUBLE, sp,
                                H5P_DEFAULT, H5P_DEFAULT);
    const herr_t st = (at >= 0) ? H5Awrite(at, H5T_NATIVE_DOUBLE, &v) : -1;
    if (at >= 0) H5Aclose(at);
    H5Sclose(sp);
    return st >= 0;
}

static bool attrInt(hid_t obj, const char *name, int v)
{
    const hid_t sp = H5Screate(H5S_SCALAR);
    const hid_t at = H5Acreate2(obj, name, H5T_NATIVE_INT, sp,
                                H5P_DEFAULT, H5P_DEFAULT);
    const herr_t st = (at >= 0) ? H5Awrite(at, H5T_NATIVE_INT, &v) : -1;
    if (at >= 0) H5Aclose(at);
    H5Sclose(sp);
    return st >= 0;
}

static bool attrText(hid_t obj, const char *name, const QByteArray &v)
{
    const hid_t tp = H5Tcopy(H5T_C_S1);
    // 空文字列でも 0 は渡せない (H5Tset_size がエラーにする)
    H5Tset_size(tp, hsize_t(std::max<qsizetype>(1, v.size())));
    H5Tset_strpad(tp, H5T_STR_NULLPAD);
    const hid_t sp = H5Screate(H5S_SCALAR);
    const hid_t at = H5Acreate2(obj, name, tp, sp, H5P_DEFAULT, H5P_DEFAULT);
    const herr_t st = (at >= 0) ? H5Awrite(at, tp, v.constData()) : -1;
    if (at >= 0) H5Aclose(at);
    H5Sclose(sp);
    H5Tclose(tp);
    return st >= 0;
}

// 1 次元の double 配列 + units 属性
static bool writeCol(hid_t where, const char *name,
                     const QVector<double> &data, const char *units)
{
    const hsize_t dims[1] = { hsize_t(data.size()) };
    const hid_t space = H5Screate_simple(1, dims, nullptr);
    const hid_t dset = H5Dcreate2(where, name, H5T_NATIVE_DOUBLE, space,
                                  H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    herr_t st = -1;
    if (dset >= 0) {
        // 要素 0 個でも H5Dwrite は呼ばない (空の dataspace への書き込みは不要)
        st = data.isEmpty()
                 ? 0
                 : H5Dwrite(dset, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL,
                            H5P_DEFAULT, data.constData());
        if (st >= 0 && units && *units)
            st = attrText(dset, "units", QByteArray(units)) ? st : -1;
        H5Dclose(dset);
    }
    H5Sclose(space);
    return st >= 0;
}

bool H5Writer::writeAntennaPattern(const QString &path,
                                   const QVector<FeedSweep> &feeds,
                                   const QVector<FarPattern> &cuts,
                                   const QString &projectName,
                                   QString *err)
{
    if (feeds.isEmpty() && cuts.isEmpty()) {
        if (err) *err = "no feed sweep and no far-field pattern to write";
        return false;
    }

    const hid_t file = H5Fcreate(path.toLocal8Bit().constData(),
                                 H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
    if (file < 0) {
        if (err) *err = "cannot create " + path;
        return false;
    }

    bool ok = true;
    ok = ok && attrText(file, "format", "OpenFDTD-X antenna pattern");
    ok = ok && attrInt(file, "version", 1);
    if (!projectName.isEmpty())
        ok = ok && attrText(file, "project", projectName.toUtf8());

    if (!feeds.isEmpty()) {
        const hid_t g = H5Gcreate2(file, "/feed", H5P_DEFAULT, H5P_DEFAULT,
                                   H5P_DEFAULT);
        if (g < 0) ok = false;
        for (int i = 0; ok && i < feeds.size(); ++i) {
            const FeedSweep &fs = feeds[i];
            // 群名はファイル内の並び順 (feed_index が重複していても衝突しない)。
            // 元の番号は属性 feed_index に残す。
            const QByteArray gn = QByteArray("feed") + QByteArray::number(i + 1);
            const hid_t fg = H5Gcreate2(g, gn.constData(), H5P_DEFAULT,
                                        H5P_DEFAULT, H5P_DEFAULT);
            if (fg < 0) { ok = false; break; }
            ok = ok && attrInt(fg, "feed_index", fs.feedIndex);
            ok = ok && attrDouble(fg, "z0", fs.z0);
            QVector<double> f, rin, xin, ref, vswr;
            f.reserve(fs.points.size());
            rin.reserve(fs.points.size());
            xin.reserve(fs.points.size());
            ref.reserve(fs.points.size());
            vswr.reserve(fs.points.size());
            for (const FeedSweepPoint &p : fs.points) {
                f.push_back(p.freqHz);
                rin.push_back(p.rin);
                xin.push_back(p.xin);
                ref.push_back(p.refDb);
                vswr.push_back(p.vswr);
            }
            ok = ok && writeCol(fg, "frequency", f, "Hz");
            ok = ok && writeCol(fg, "rin", rin, "ohm");
            ok = ok && writeCol(fg, "xin", xin, "ohm");
            ok = ok && writeCol(fg, "ref_db", ref, "dB");
            ok = ok && writeCol(fg, "vswr", vswr, "");
            H5Gclose(fg);
        }
        if (g >= 0) H5Gclose(g);
    }

    if (ok && !cuts.isEmpty()) {
        const hid_t g = H5Gcreate2(file, "/pattern", H5P_DEFAULT, H5P_DEFAULT,
                                   H5P_DEFAULT);
        if (g < 0) ok = false;
        for (int i = 0; ok && i < cuts.size(); ++i) {
            const FarPattern &c = cuts[i];
            const QByteArray gn = QByteArray("cut") + QByteArray::number(i + 1);
            const hid_t cg = H5Gcreate2(g, gn.constData(), H5P_DEFAULT,
                                        H5P_DEFAULT, H5P_DEFAULT);
            if (cg < 0) { ok = false; break; }
            ok = ok && attrText(cg, "plane", c.plane.toUtf8());
            ok = ok && attrDouble(cg, "frequency_hz", c.freqHz);
            ok = ok && writeCol(cg, "angle_deg", c.deg, "deg");
            ok = ok && writeCol(cg, "e_abs_db", c.eAbsDb, "dB");
            // 偏波成分は far1d.log に列があるときだけ (無い出力では作らない)
            if (ok && !c.eThetaDb.isEmpty())
                ok = writeCol(cg, "e_theta_db", c.eThetaDb, "dB");
            if (ok && !c.ePhiDb.isEmpty())
                ok = writeCol(cg, "e_phi_db", c.ePhiDb, "dB");
            H5Gclose(cg);
        }
        if (g >= 0) H5Gclose(g);
    }

    H5Fclose(file);
    if (!ok && err) *err = "HDF5 write failed";
    return ok;
}

#else

bool H5Writer::write(const QString &, const Project &,
                     const QVector<int> &, const QVector<double> &,
                     const QVector<double> &, QString *err)
{
    if (err) *err = "built without HDF5 — reconfigure with -DUSE_HDF5=ON";
    return false;
}

bool H5Writer::writeAntennaPattern(const QString &, const QVector<FeedSweep> &,
                                   const QVector<FarPattern> &, const QString &,
                                   QString *err)
{
    if (err) *err = "built without HDF5 — reconfigure with -DUSE_HDF5=ON";
    return false;
}

#endif
