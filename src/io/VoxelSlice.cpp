// VoxelSlice.cpp
#include "VoxelSlice.h"

#include "H5Reader.h"

#include <algorithm>
#include <cmath>

namespace ofd {

namespace {

// MeshAxis → セル境界 (io/Voxelizer の expandAxis と同じ刻み方。
// **ここがずれると絵とボクセル化が別の格子を指す**ので、式を揃えてある)
void expandBounds(const MeshAxis &ax, QVector<double> &bounds)
{
    bounds.clear();
    if (ax.nodes.isEmpty()) return;
    bounds.push_back(ax.nodes[0]);
    for (int i = 0; i < ax.divs.size(); ++i) {
        const double a = ax.nodes[i], b = ax.nodes[i + 1];
        const int n = ax.divs[i];
        for (int k = 1; k <= n; ++k)
            bounds.push_back(a + (b - a) * k / n);
    }
}

// セル中心が [lo, hi] に入るか。境界そのものに乗る場合を取りこぼさないよう
// セル幅に対する相対の余裕を持たせる (直方体は必ずセル境界に一致するので、
// 中心は境界から半セル離れている — 余裕は幅の 1/4 で十分)。
bool centerInside(double c, double lo, double hi, double cellW)
{
    const double eps = 0.25 * cellW;
    return c > lo - eps && c < hi + eps;
}

} // namespace

int voxelSliceCount(const MeshAxis &mx, const MeshAxis &my, const MeshAxis &mz,
                    int axis)
{
    const MeshAxis *a = (axis == 0) ? &mx : (axis == 1) ? &my
                      : (axis == 2) ? &mz : nullptr;
    return (a && a->isValid()) ? a->totalCells() : 0;
}

VoxelSliceMask voxelSlice(const QVector<Geometry> &bricks,
                          const MeshAxis &mx, const MeshAxis &my,
                          const MeshAxis &mz,
                          int axis, int index, int materialId)
{
    VoxelSliceMask m;
    if (!mx.isValid() || !my.isValid() || !mz.isValid()) return m;
    if (axis < 0 || axis > 2) return m;

    QVector<double> xb, yb, zb;
    expandBounds(mx, xb);
    expandBounds(my, yb);
    expandBounds(mz, zb);
    if (xb.size() < 2 || yb.size() < 2 || zb.size() < 2) return m;

    // 面内 2 軸は H5Reader と同じ規約 (列 = u 軸 / 行 = v 軸)。
    // 対応表を持つのはあちらだけにして、ここでは借りる
    int cAxis = 0, rAxis = 1;
    H5Reader::seriesSliceAxes(axis, &cAxis, &rAxis);
    const int sAxis = axis;
    const QVector<double> *const bnd[3] = { &xb, &yb, &zb };
    const QVector<double> *cb = bnd[cAxis], *rb = bnd[rAxis], *sb = bnd[sAxis];
    m.uAxis = cAxis;
    m.vAxis = rAxis;
    const int nc = cb->size() - 1, nr = rb->size() - 1, ns = sb->size() - 1;
    if (index < 0 || index >= ns) return m;

    m.cols = nc;
    m.rows = nr;
    m.cell.fill(false, int(qsizetype(nc) * nr));
    m.colMin = cb->first(); m.colMax = cb->last();
    m.rowMin = rb->first(); m.rowMax = rb->last();
    m.sliceCoord = 0.5 * ((*sb)[index] + (*sb)[index + 1]);
    const double sw = (*sb)[index + 1] - (*sb)[index];

    for (const Geometry &g : bricks) {
        if (g.shape != 1) continue;                   // 直方体のみ
        if (materialId >= 0 && g.materialId != materialId) continue;
        // shape=1 の g[] は xmin xmax ymin ymax zmin zmax
        const double slo = g.g[sAxis * 2], shi = g.g[sAxis * 2 + 1];
        if (!centerInside(m.sliceCoord, std::min(slo, shi),
                          std::max(slo, shi), sw))
            continue;                                  // この断面に無い

        const double clo = std::min(g.g[cAxis * 2], g.g[cAxis * 2 + 1]);
        const double chi = std::max(g.g[cAxis * 2], g.g[cAxis * 2 + 1]);
        const double rlo = std::min(g.g[rAxis * 2], g.g[rAxis * 2 + 1]);
        const double rhi = std::max(g.g[rAxis * 2], g.g[rAxis * 2 + 1]);

        for (int r = 0; r < nr; ++r) {
            const double rc = 0.5 * ((*rb)[r] + (*rb)[r + 1]);
            const double rw = (*rb)[r + 1] - (*rb)[r];
            if (!centerInside(rc, rlo, rhi, rw)) continue;
            for (int c = 0; c < nc; ++c) {
                const double cc = 0.5 * ((*cb)[c] + (*cb)[c + 1]);
                const double cw = (*cb)[c + 1] - (*cb)[c];
                if (!centerInside(cc, clo, chi, cw)) continue;
                bool &v = m.cell[qsizetype(r) * nc + c];
                if (!v) { v = true; ++m.occupied; }    // 重複は二重に数えない
            }
        }
    }
    m.ok = true;
    return m;
}

} // namespace ofd
