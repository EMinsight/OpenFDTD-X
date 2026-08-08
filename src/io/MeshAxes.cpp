// MeshAxes.cpp — 取込メッシュの主軸検出
#include "MeshAxes.h"

#include <algorithm>
#include <cmath>

namespace ofd {
namespace {

const double kPi = 3.14159265358979323846;

// 対称 3×3 の固有値分解 (Jacobi 法)。a は破壊され、v に固有ベクトル
// (列ベクトル) が入る。反復回数は固定上限で、乱数も時刻も使わない。
void jacobiEigen(double a[3][3], double v[3][3], double w[3])
{
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) v[i][j] = (i == j) ? 1.0 : 0.0;

    for (int sweep = 0; sweep < 64; ++sweep) {
        double off = 0.0;
        for (int i = 0; i < 3; ++i)
            for (int j = i + 1; j < 3; ++j) off += a[i][j] * a[i][j];
        if (off <= 1e-30) break;

        for (int p = 0; p < 2; ++p)
            for (int q = p + 1; q < 3; ++q) {
                if (std::fabs(a[p][q]) <= 1e-300) continue;
                const double theta = (a[q][q] - a[p][p]) / (2.0 * a[p][q]);
                const double t = (theta >= 0.0 ? 1.0 : -1.0)
                                 / (std::fabs(theta)
                                    + std::sqrt(theta * theta + 1.0));
                const double c = 1.0 / std::sqrt(t * t + 1.0);
                const double s = t * c;
                const double app = a[p][p], aqq = a[q][q], apq = a[p][q];
                a[p][p] = app - t * apq;
                a[q][q] = aqq + t * apq;
                a[p][q] = a[q][p] = 0.0;
                for (int k = 0; k < 3; ++k) {
                    if (k == p || k == q) continue;
                    const double akp = a[k][p], akq = a[k][q];
                    a[k][p] = a[p][k] = c * akp - s * akq;
                    a[k][q] = a[q][k] = s * akp + c * akq;
                }
                for (int k = 0; k < 3; ++k) {
                    const double vkp = v[k][p], vkq = v[k][q];
                    v[k][p] = c * vkp - s * vkq;
                    v[k][q] = s * vkp + c * vkq;
                }
            }
    }
    for (int i = 0; i < 3; ++i) w[i] = a[i][i];
}

} // namespace

PrincipalAxes principalAxes(const ImportedMesh &mesh)
{
    PrincipalAxes out;
    const int n = std::min(mesh.numTriangles, int(mesh.vertices.size() / 9));
    if (n <= 0) return out;

    // ── ① 面積重み付き重心 ────────────────────────────────────────────────
    double totalArea = 0.0;
    double g[3] = { 0, 0, 0 };
    for (int t = 0; t < n; ++t) {
        const float *p = mesh.vertices.constData() + t * 9;
        const double ux = double(p[3]) - p[0], uy = double(p[4]) - p[1],
                     uz = double(p[5]) - p[2];
        const double vx = double(p[6]) - p[0], vy = double(p[7]) - p[1],
                     vz = double(p[8]) - p[2];
        const double cx = uy * vz - uz * vy;
        const double cy = uz * vx - ux * vz;
        const double cz = ux * vy - uy * vx;
        const double A = 0.5 * std::sqrt(cx * cx + cy * cy + cz * cz);
        if (!(A > 0.0)) continue;
        totalArea += A;
        for (int a = 0; a < 3; ++a)
            g[a] += A * (double(p[a]) + double(p[3 + a]) + double(p[6 + a])) / 3.0;
    }
    if (!(totalArea > 0.0)) return out;
    for (int a = 0; a < 3; ++a) g[a] /= totalArea;

    // ── ② 面積重み付き共分散 (三角形の閉形式 + 平行軸の定理) ──────────────
    double C[3][3] = { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } };
    for (int t = 0; t < n; ++t) {
        const float *p = mesh.vertices.constData() + t * 9;
        const double ux = double(p[3]) - p[0], uy = double(p[4]) - p[1],
                     uz = double(p[5]) - p[2];
        const double vx = double(p[6]) - p[0], vy = double(p[7]) - p[1],
                     vz = double(p[8]) - p[2];
        const double cx = uy * vz - uz * vy;
        const double cy = uz * vx - ux * vz;
        const double cz = ux * vy - uy * vx;
        const double A = 0.5 * std::sqrt(cx * cx + cy * cy + cz * cz);
        if (!(A > 0.0)) continue;

        double gt[3];
        for (int a = 0; a < 3; ++a)
            gt[a] = (double(p[a]) + double(p[3 + a]) + double(p[6 + a])) / 3.0;

        // 三角形内の 2 次モーメント: (A/12) Σ (p_i − g_t)(p_i − g_t)ᵀ
        for (int k = 0; k < 3; ++k) {
            double d[3];
            for (int a = 0; a < 3; ++a) d[a] = double(p[k * 3 + a]) - gt[a];
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j) C[i][j] += (A / 12.0) * d[i] * d[j];
        }
        // 平行軸: A (g_t − G)(g_t − G)ᵀ
        double e[3];
        for (int a = 0; a < 3; ++a) e[a] = gt[a] - g[a];
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) C[i][j] += A * e[i] * e[j];
    }

    double a3[3][3], v[3][3], w[3];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) a3[i][j] = C[i][j];
    jacobiEigen(a3, v, w);

    // 固有値の降順に並べ替える
    int ord[3] = { 0, 1, 2 };
    for (int i = 0; i < 3; ++i)
        for (int j = i + 1; j < 3; ++j)
            if (w[ord[j]] > w[ord[i]]) std::swap(ord[i], ord[j]);

    for (int k = 0; k < 3; ++k) {
        const int c = ord[k];
        double ax[3] = { v[0][c], v[1][c], v[2][c] };
        const double len = std::sqrt(ax[0] * ax[0] + ax[1] * ax[1] + ax[2] * ax[2]);
        if (!(len > 0.0)) return out;
        for (int a = 0; a < 3; ++a) ax[a] /= len;
        // 符号の規約: 絶対値最大の成分を正にする (同じ形から同じ結果を出す)
        int big = 0;
        for (int a = 1; a < 3; ++a)
            if (std::fabs(ax[a]) > std::fabs(ax[big])) big = a;
        if (ax[big] < 0.0) for (int a = 0; a < 3; ++a) ax[a] = -ax[a];
        for (int a = 0; a < 3; ++a) out.axis[k][a] = ax[a];
        out.moment[k] = w[c];
    }

    // 右手系へ (第 3 軸 = 第 1 × 第 2)
    out.axis[2][0] = out.axis[0][1] * out.axis[1][2] - out.axis[0][2] * out.axis[1][1];
    out.axis[2][1] = out.axis[0][2] * out.axis[1][0] - out.axis[0][0] * out.axis[1][2];
    out.axis[2][2] = out.axis[0][0] * out.axis[1][1] - out.axis[0][1] * out.axis[1][0];

    for (int a = 0; a < 3; ++a) out.centroid[a] = g[a];

    // 縮退判定: 固有値が相対 1e-3 以内で並ぶと向きが一意に決まらない
    const double wmax = std::max(out.moment[0], 1e-300);
    out.degenerate = (std::fabs(out.moment[0] - out.moment[1]) <= 1e-3 * wmax)
                  || (std::fabs(out.moment[1] - out.moment[2]) <= 1e-3 * wmax);

    // ── ③ X→Y→Z の順に回して主軸を X/Y/Z へ揃える角度 ────────────────────
    // 求める回転行列 R は「行 = 主軸」(R·p が主軸座標)。
    // applyPlacement は Rz(c)·Ry(b)·Rx(a) の順に掛けるので、その形から抽出:
    //   b = asin(−R20), a = atan2(R21, R22), c = atan2(R10, R00)
    const double R[3][3] = {
        { out.axis[0][0], out.axis[0][1], out.axis[0][2] },
        { out.axis[1][0], out.axis[1][1], out.axis[1][2] },
        { out.axis[2][0], out.axis[2][1], out.axis[2][2] } };
    const double sb = std::min(1.0, std::max(-1.0, -R[2][0]));
    const double b = std::asin(sb);
    double aa, cc;
    if (std::fabs(R[2][0]) < 1.0 - 1e-9) {
        aa = std::atan2(R[2][1], R[2][2]);
        cc = std::atan2(R[1][0], R[0][0]);
    } else {
        // ジンバルロック (b = ±90°) — a と c は縮退するので c = 0 に固定
        aa = std::atan2(-R[1][2], R[1][1]);
        cc = 0.0;
    }
    out.eulerXYZ_deg[0] = aa * 180.0 / kPi;
    out.eulerXYZ_deg[1] = b * 180.0 / kPi;
    out.eulerXYZ_deg[2] = cc * 180.0 / kPi;

    out.valid = true;
    return out;
}

} // namespace ofd
