// SliceOverlay.cpp
#include "SliceOverlay.h"

#include "H5Reader.h"

#include <algorithm>
#include <cmath>

namespace ofd {

namespace {

// v は上から数える (v = 0 が vMax 側)。範囲は 0..1 へ丸める
double normU(double x, double lo, double hi)
{
    const double t = (x - lo) / (hi - lo);
    return std::min(std::max(t, 0.0), 1.0);
}
double normV(double x, double lo, double hi)
{
    const double t = (hi - x) / (hi - lo);
    return std::min(std::max(t, 0.0), 1.0);
}

bool usableRange(double lo, double hi)
{
    return std::isfinite(lo) && std::isfinite(hi) && hi > lo;
}

} // namespace

bool boxOnSlice(const Geometry &g, int axis, double sliceCoord,
                double uMin, double uMax, double vMin, double vMax,
                SliceRectNorm *out)
{
    if (!out || g.shape != 1) return false;              // 直方体のみ
    if (axis < 0 || axis > 2) return false;
    if (!usableRange(uMin, uMax) || !usableRange(vMin, vMax)) return false;
    if (!std::isfinite(sliceCoord)) return false;

    int uAxis = 0, vAxis = 1;
    H5Reader::seriesSliceAxes(axis, &uAxis, &vAxis);

    // 固定軸で断面と交わるか。**交わらないものは描かない** (別の深さに
    // ある物体が、今見ている断面に在るように見えてしまう)
    const double slo = std::min(g.g[axis * 2], g.g[axis * 2 + 1]);
    const double shi = std::max(g.g[axis * 2], g.g[axis * 2 + 1]);
    if (sliceCoord < slo || sliceCoord > shi) return false;

    const double ulo = std::min(g.g[uAxis * 2], g.g[uAxis * 2 + 1]);
    const double uhi = std::max(g.g[uAxis * 2], g.g[uAxis * 2 + 1]);
    const double vlo = std::min(g.g[vAxis * 2], g.g[vAxis * 2 + 1]);
    const double vhi = std::max(g.g[vAxis * 2], g.g[vAxis * 2 + 1]);
    // 面内で表示範囲とまったく重ならないものも描かない
    if (uhi < uMin || ulo > uMax || vhi < vMin || vlo > vMax) return false;

    out->u0 = normU(ulo, uMin, uMax);
    out->u1 = normU(uhi, uMin, uMax);
    out->v0 = normV(vhi, vMin, vMax);   // vhi (+ 側) が上 = v0
    out->v1 = normV(vlo, vMin, vMax);
    return true;
}

bool pointOnSlice(double x, double y, double z, int axis, double sliceCoord,
                  double tol, double uMin, double uMax,
                  double vMin, double vMax, SlicePointNorm *out)
{
    if (!out) return false;
    if (axis < 0 || axis > 2) return false;
    if (!usableRange(uMin, uMax) || !usableRange(vMin, vMax)) return false;
    if (!std::isfinite(sliceCoord)) return false;
    const double p[3] = { x, y, z };
    for (const double v : p) if (!std::isfinite(v)) return false;

    if (tol < 0.0) tol = 0.0;
    if (std::fabs(p[axis] - sliceCoord) > tol) return false;   // この断面に居ない

    int uAxis = 0, vAxis = 1;
    H5Reader::seriesSliceAxes(axis, &uAxis, &vAxis);
    // 表示範囲の外にある点は描かない (端へ貼り付けると位置の誤読になる)
    if (p[uAxis] < uMin || p[uAxis] > uMax
        || p[vAxis] < vMin || p[vAxis] > vMax) return false;

    out->u = normU(p[uAxis], uMin, uMax);
    out->v = normV(p[vAxis], vMin, vMax);
    return true;
}

} // namespace ofd
