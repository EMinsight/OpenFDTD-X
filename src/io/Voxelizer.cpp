// Voxelizer.cpp
#include "Voxelizer.h"
#include "StlImporter.h"
#include "../core/MeshAxis.h"

#include <QObject>
#include <algorithm>
#include <cmath>
#include <vector>

using namespace ofd;

// Expand a MeshAxis into the list of cell boundary coordinates
// (totalCells()+1 entries) and the matching cell centers.
static void expandAxis(const MeshAxis &ax,
                       QVector<double> &bounds, QVector<double> &centers)
{
    bounds.clear();
    centers.clear();
    if (ax.nodes.isEmpty()) return;
    bounds.push_back(ax.nodes[0]);
    for (int i = 0; i < ax.divs.size(); ++i) {
        const double a = ax.nodes[i], b = ax.nodes[i + 1];
        const int n = ax.divs[i];
        for (int k = 1; k <= n; ++k)
            bounds.push_back(a + (b - a) * k / n);
    }
    centers.reserve(bounds.size() - 1);
    for (int i = 0; i + 1 < bounds.size(); ++i)
        centers.push_back(0.5 * (bounds[i] + bounds[i + 1]));
}

// Ray (origin o, direction RAY_DIR) vs triangle (v0,v1,v2), Möller–Trumbore.
// Returns true if the ray crosses the triangle at parametric distance > 0.
//
// RAY_DIR is tilted slightly off the X axis so that for axis-aligned input
// meshes (e.g. a box STL) the ray does not graze a face's shared edges or
// vertices — the classic degeneracy that makes a pure (1,0,0) ray miss the
// triangulation diagonal and mis-classify whole rows of cells.
static const double RAY_DIR[3] = { 1.0, 7.3e-4, 3.1e-4 };

static bool rayHitsForward(double px, double py, double pz, const float *t)
{
    const double e1x = t[3] - t[0], e1y = t[4] - t[1], e1z = t[5] - t[2];
    const double e2x = t[6] - t[0], e2y = t[7] - t[1], e2z = t[8] - t[2];
    const double dx = RAY_DIR[0], dy = RAY_DIR[1], dz = RAY_DIR[2];
    const double hx = dy * e2z - dz * e2y;
    const double hy = dz * e2x - dx * e2z;
    const double hz = dx * e2y - dy * e2x;
    const double a = e1x * hx + e1y * hy + e1z * hz;
    if (std::fabs(a) < 1e-18) return false;             // ray parallel to tri
    const double f = 1.0 / a;
    const double sx = px - t[0], sy = py - t[1], sz = pz - t[2];
    const double u = f * (sx * hx + sy * hy + sz * hz);
    if (u < 0.0 || u > 1.0) return false;
    const double qx = sy * e1z - sz * e1y;
    const double qy = sz * e1x - sx * e1z;
    const double qz = sx * e1y - sy * e1x;
    const double v = f * (dx * qx + dy * qy + dz * qz);
    if (v < 0.0 || u + v > 1.0) return false;
    const double dist = f * (e2x * qx + e2y * qy + e2z * qz);
    return dist > 1e-12;                                 // forward only
}

// 一般化巻き数 (generalized winding number)。
// 各三角形が点 p に張る符号付き立体角 Ω を足して 4π で割る。
// Ω は Van Oosterom & Strackee (1983) の閉形式:
//   tan(Ω/2) = a·(b×c) / (|a||b||c| + (a·b)|c| + (a·c)|b| + (b·c)|a|)
// (a,b,c は p から見た 3 頂点のベクトル)。atan2 を使うので分母の符号も
// 込みで一意に決まり、p が面に近くても破綻しない。
double Voxelizer::windingNumber(const ImportedMesh &mesh,
                                double px, double py, double pz)
{
    const float *V = mesh.vertices.constData();
    const int T = qMin(mesh.numTriangles, int(mesh.vertices.size() / 9));
    double sum = 0.0;
    for (int t = 0; t < T; ++t) {
        const float *q = V + 9 * t;
        const double a[3] = { q[0] - px, q[1] - py, q[2] - pz };
        const double b[3] = { q[3] - px, q[4] - py, q[5] - pz };
        const double c[3] = { q[6] - px, q[7] - py, q[8] - pz };
        const double la = std::sqrt(a[0]*a[0] + a[1]*a[1] + a[2]*a[2]);
        const double lb = std::sqrt(b[0]*b[0] + b[1]*b[1] + b[2]*b[2]);
        const double lc = std::sqrt(c[0]*c[0] + c[1]*c[1] + c[2]*c[2]);
        if (la < 1e-300 || lb < 1e-300 || lc < 1e-300)
            continue;                       // p が頂点そのもの — 寄与を飛ばす
        const double cr[3] = { b[1]*c[2] - b[2]*c[1],
                               b[2]*c[0] - b[0]*c[2],
                               b[0]*c[1] - b[1]*c[0] };
        const double num = a[0]*cr[0] + a[1]*cr[1] + a[2]*cr[2];
        const double ab = a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
        const double ac = a[0]*c[0] + a[1]*c[1] + a[2]*c[2];
        const double bc = b[0]*c[0] + b[1]*c[1] + b[2]*c[2];
        const double den = la*lb*lc + ab*lc + ac*lb + bc*la;
        sum += 2.0 * std::atan2(num, den);
    }
    return sum / (4.0 * 3.14159265358979323846);
}

VoxelResult Voxelizer::voxelize(const ImportedMesh &mesh,
                                const MeshAxis &mx,
                                const MeshAxis &my,
                                const MeshAxis &mz,
                                int materialId,
                                qint64 cellCap,
                                const VoxelOptions &opt)
{
    VoxelResult r;
    if (mesh.numTriangles <= 0) { r.error = QObject::tr("empty mesh"); return r; }
    if (!mx.isValid() || !my.isValid() || !mz.isValid()) {
        r.error = QObject::tr("invalid mesh axes");
        return r;
    }

    QVector<double> xb, yb, zb, xc, yc, zc;
    expandAxis(mx, xb, xc);
    expandAxis(my, yb, yc);
    expandAxis(mz, zb, zc);
    r.nx = xc.size(); r.ny = yc.size(); r.nz = zc.size();

    const qint64 cells = qint64(r.nx) * r.ny * r.nz;
    if (cells > cellCap) {
        r.error = QObject::tr("grid too large for staircase voxelization "
                              "(%1 cells > cap %2); coarsen the mesh or build "
                              "with -DUSE_LIBIGL=ON").arg(cells).arg(cellCap);
        return r;
    }

    const float *V = mesh.vertices.constData();
    const int T = mesh.numTriangles;

    // 1 点の内外判定。メッシュ bbox の外は自明に外 (高速棄却)。
    auto insideAt = [&](double px, double py, double pz) -> bool {
        if (px < mesh.bbox[0] || px > mesh.bbox[3]) return false;
        if (py < mesh.bbox[1] || py > mesh.bbox[4]) return false;
        if (pz < mesh.bbox[2] || pz > mesh.bbox[5]) return false;
        if (opt.inside == InsideTest::WindingNumber)
            return windingNumber(mesh, px, py, pz) > 0.5;
        int crossings = 0;
        for (int t = 0; t < T; ++t)
            if (rayHitsForward(px, py, pz, V + 9 * t)) ++crossings;
        return (crossings & 1) != 0;
    };

    const int    ns  = qBound(1, opt.pvfSamples, 8);
    const double thr = qBound(1e-9, opt.pvfThreshold, 1.0);

    // ── 境界セル (三角形が横切るセル) の抽出 ──────────────────────────────
    // 三角形の AABB が重なるセルを立てる。厳密な三角形-直方体交差ではなく
    // AABB なので **必ず超集合** になる (取りこぼしが無い)。立たなかった
    // セルには面が通らないので、中心 1 点で内外が厳密に決まる。
    std::vector<unsigned char> bnd;
    if (opt.pvf) {
        bnd.assign(std::size_t(cells), 0);
        auto cellOf = [](const QVector<double> &b, double v, int n) {
            const int i =
                int(std::upper_bound(b.begin(), b.end(), v) - b.begin()) - 1;
            return qBound(0, i, n - 1);
        };
        for (int t = 0; t < T; ++t) {
            const float *q = V + 9 * t;
            double lo[3], hi[3];
            for (int a = 0; a < 3; ++a) {
                lo[a] = hi[a] = q[a];
                for (int c = 1; c < 3; ++c) {
                    const double v = q[3 * c + a];
                    if (v < lo[a]) lo[a] = v;
                    if (v > hi[a]) hi[a] = v;
                }
            }
            const int i0 = cellOf(xb, lo[0], r.nx), i1 = cellOf(xb, hi[0], r.nx);
            const int j0 = cellOf(yb, lo[1], r.ny), j1 = cellOf(yb, hi[1], r.ny);
            const int k0 = cellOf(zb, lo[2], r.nz), k1 = cellOf(zb, hi[2], r.nz);
            for (int k = k0; k <= k1; ++k)
                for (int j = j0; j <= j1; ++j) {
                    unsigned char *row =
                        &bnd[(std::size_t(k) * r.ny + j) * r.nx];
                    for (int i = i0; i <= i1; ++i) row[i] = 1;
                }
        }
        for (unsigned char c : bnd) if (c) ++r.boundaryCells;

        const qint64 work = r.boundaryCells * qint64(ns) * ns * ns * qint64(T);
        if (opt.pvfWorkCap > 0 && work > opt.pvfWorkCap) {
            r.error = QObject::tr(
                "partial volume fraction needs %1 triangle tests "
                "(%2 boundary cells x %3 samples x %4 triangles) which exceeds "
                "the cap %5; turn the partial volume fraction off, use a "
                "coarser grid, or simplify the mesh")
                .arg(work).arg(r.boundaryCells).arg(qint64(ns) * ns * ns)
                .arg(T).arg(opt.pvfWorkCap);
            return r;
        }
    }

    for (int k = 0; k < r.nz; ++k) {
        // セル全体が bbox の外なら飛ばす (中心ではなくセル範囲で見る —
        // PVF ではセルの一部だけが中に入る場合があるため)
        if (zb[k + 1] < mesh.bbox[2] || zb[k] > mesh.bbox[5]) continue;
        const double dz = zb[k + 1] - zb[k];
        for (int j = 0; j < r.ny; ++j) {
            if (yb[j + 1] < mesh.bbox[1] || yb[j] > mesh.bbox[4]) continue;
            const double dy = yb[j + 1] - yb[j];

            // Count forward crossings once per (j,k) ray is not possible
            // because parity depends on x; instead test each cell's center.
            int runStart = -1;
            for (int i = 0; i < r.nx; ++i) {
                const double cellVol = (xb[i + 1] - xb[i]) * dy * dz;
                double frac;
                if (opt.pvf && bnd[(std::size_t(k) * r.ny + j) * r.nx + i]) {
                    int hit = 0;
                    for (int kk = 0; kk < ns; ++kk) {
                        const double pz = zb[k] + dz * (kk + 0.5) / ns;
                        for (int jj = 0; jj < ns; ++jj) {
                            const double py = yb[j] + dy * (jj + 0.5) / ns;
                            for (int ii = 0; ii < ns; ++ii) {
                                const double px = xb[i]
                                    + (xb[i + 1] - xb[i]) * (ii + 0.5) / ns;
                                if (insideAt(px, py, pz)) ++hit;
                            }
                        }
                    }
                    frac = double(hit) / (double(ns) * ns * ns);
                } else {
                    frac = insideAt(xc[i], yc[j], zc[k]) ? 1.0 : 0.0;
                }
                if (opt.pvf) r.pvfVolume += frac * cellVol;
                const bool inside = (frac >= thr);
                if (inside) r.stairVolume += cellVol;

                // まとめない設定は 1 セル = 1 直方体 (区間を作らない)
                if (!opt.mergeRuns) {
                    if (!inside) continue;
                    Geometry g;
                    g.shape = 1;
                    g.materialId = materialId;
                    g.g[0] = xb[i];  g.g[1] = xb[i + 1];
                    g.g[2] = yb[j];  g.g[3] = yb[j + 1];
                    g.g[4] = zb[k];  g.g[5] = zb[k + 1];
                    r.bricks.push_back(g);
                    r.occupied += 1;
                    continue;
                }
                if (inside && runStart < 0) {
                    runStart = i;
                } else if (!inside && runStart >= 0) {
                    Geometry g;
                    g.shape = 1;
                    g.materialId = materialId;
                    g.g[0] = xb[runStart]; g.g[1] = xb[i];
                    g.g[2] = yb[j];        g.g[3] = yb[j + 1];
                    g.g[4] = zb[k];        g.g[5] = zb[k + 1];
                    r.bricks.push_back(g);
                    r.occupied += i - runStart;
                    runStart = -1;
                }
            }
            if (runStart >= 0) {
                Geometry g;
                g.shape = 1;
                g.materialId = materialId;
                g.g[0] = xb[runStart]; g.g[1] = xb[r.nx];
                g.g[2] = yb[j];        g.g[3] = yb[j + 1];
                g.g[4] = zb[k];        g.g[5] = zb[k + 1];
                r.bricks.push_back(g);
                r.occupied += r.nx - runStart;
            }
        }
    }

    r.ok = true;
    return r;
}
