#include "DensityField.h"

#include <algorithm>
#include <cmath>

namespace ofd {
namespace topo {

namespace {

// 反転して書かれた区間にも耐える
inline void ordered(double a, double b, double *lo, double *hi)
{
    *lo = std::min(a, b);
    *hi = std::max(a, b);
}

inline bool inSpan(double v, double a, double b)
{
    double lo = 0.0, hi = 0.0;
    ordered(a, b, &lo, &hi);
    return v >= lo && v <= hi;
}

// 外接矩形に内接する楕円の内側か (2 軸ぶん)
inline bool inEllipse2(double u, double v, double u1, double u2,
                       double v1, double v2)
{
    double ulo = 0.0, uhi = 0.0, vlo = 0.0, vhi = 0.0;
    ordered(u1, u2, &ulo, &uhi);
    ordered(v1, v2, &vlo, &vhi);
    const double ru = 0.5 * (uhi - ulo), rv = 0.5 * (vhi - vlo);
    if (ru <= 0.0 || rv <= 0.0) return false;
    const double du = (u - 0.5 * (ulo + uhi)) / ru;
    const double dv = (v - 0.5 * (vlo + vhi)) / rv;
    return du * du + dv * dv <= 1.0;
}

// 内外判定を持つ形状か
inline bool shapeSupported(int shape)
{
    return shape == 1 || shape == 2 || shape == 11 || shape == 12 || shape == 13;
}

bool insideUnit(const Geometry &u, double x, double y, double z)
{
    const double *g = u.g;
    switch (u.shape) {
        case 1:   // 直方体
            return inSpan(x, g[0], g[1]) && inSpan(y, g[2], g[3])
                && inSpan(z, g[4], g[5]);
        case 2: {  // 楕円体 (外接直方体に内接)
            double xlo = 0, xhi = 0, ylo = 0, yhi = 0, zlo = 0, zhi = 0;
            ordered(g[0], g[1], &xlo, &xhi);
            ordered(g[2], g[3], &ylo, &yhi);
            ordered(g[4], g[5], &zlo, &zhi);
            const double rx = 0.5 * (xhi - xlo), ry = 0.5 * (yhi - ylo),
                         rz = 0.5 * (zhi - zlo);
            if (rx <= 0.0 || ry <= 0.0 || rz <= 0.0) return false;
            const double dx = (x - 0.5 * (xlo + xhi)) / rx;
            const double dy = (y - 0.5 * (ylo + yhi)) / ry;
            const double dz = (z - 0.5 * (zlo + zhi)) / rz;
            return dx * dx + dy * dy + dz * dz <= 1.0;
        }
        case 11:  // 円柱 X (断面は y-z)
            return inSpan(x, g[0], g[1]) && inEllipse2(y, z, g[2], g[3], g[4], g[5]);
        case 12:  // 円柱 Y (断面は z-x)
            return inSpan(y, g[2], g[3]) && inEllipse2(z, x, g[4], g[5], g[0], g[1]);
        case 13:  // 円柱 Z (断面は x-y)
            return inSpan(z, g[4], g[5]) && inEllipse2(x, y, g[0], g[1], g[2], g[3]);
        default:
            return false;
    }
}

} // namespace

Grid gridFor(const Region &r, double resolution_m)
{
    Grid g;
    if (!r.valid() || !(resolution_m > 0.0)) return g;
    // 端数の画素を作らないよう切り上げてから等分する。
    // 割り切れる寸法 (5 μm を 20 nm など) で丸め誤差により 1 画素増えるのを
    // 防ぐため、相対 1e-9 の余裕を引いてから切り上げる。
    auto countFor = [resolution_m](double len) {
        const double q = len / resolution_m;
        return std::max(1, static_cast<int>(std::ceil(q - 1e-9 * std::fabs(q))));
    };
    g.nx = countFor(r.width_m());
    g.ny = countFor(r.depth_m());
    g.pitchX_m = r.width_m() / g.nx;
    g.pitchY_m = r.depth_m() / g.ny;
    return g;
}

std::vector<double> filter(const std::vector<double> &rho, const Grid &g,
                           double radius_m)
{
    const int n = g.count();
    if (!g.valid() || static_cast<int>(rho.size()) != n) return rho;
    if (!(radius_m > 0.0)) return rho;

    // 重みが乗る近傍の広がり (画素数)
    const int kx = static_cast<int>(std::floor(radius_m / g.pitchX_m));
    const int ky = static_cast<int>(std::floor(radius_m / g.pitchY_m));

    std::vector<double> out(static_cast<size_t>(n), 0.0);
    for (int j = 0; j < g.ny; ++j) {
        for (int i = 0; i < g.nx; ++i) {
            double num = 0.0, den = 0.0;
            const int j0 = std::max(0, j - ky), j1 = std::min(g.ny - 1, j + ky);
            const int i0 = std::max(0, i - kx), i1 = std::min(g.nx - 1, i + kx);
            for (int jj = j0; jj <= j1; ++jj) {
                const double dy = (jj - j) * g.pitchY_m;
                for (int ii = i0; ii <= i1; ++ii) {
                    const double dx = (ii - i) * g.pitchX_m;
                    const double d = std::sqrt(dx * dx + dy * dy);
                    const double w = radius_m - d;
                    if (w <= 0.0) continue;
                    num += w * rho[static_cast<size_t>(jj) * g.nx + ii];
                    den += w;
                }
            }
            out[static_cast<size_t>(j) * g.nx + i] =
                (den > 0.0) ? num / den : rho[static_cast<size_t>(j) * g.nx + i];
        }
    }
    return out;
}

double project(double rho, double beta, double eta)
{
    if (!(beta > 0.0)) return rho;                 // β = 0 は恒等写像
    const double te = std::tanh(beta * eta);
    const double den = te + std::tanh(beta * (1.0 - eta));
    if (!(den > 0.0)) return rho;
    return (te + std::tanh(beta * (rho - eta))) / den;
}

std::vector<double> project(const std::vector<double> &rho, double beta, double eta)
{
    std::vector<double> out(rho.size());
    for (size_t k = 0; k < rho.size(); ++k) out[k] = project(rho[k], beta, eta);
    return out;
}

double volumeFraction(const std::vector<double> &rho)
{
    if (rho.empty()) return 0.0;
    double s = 0.0;
    for (double v : rho) s += v;
    return s / static_cast<double>(rho.size());
}

double nonDiscreteness(const std::vector<double> &rho)
{
    if (rho.empty()) return 0.0;
    double s = 0.0;
    for (double v : rho) s += 4.0 * v * (1.0 - v);
    return s / static_cast<double>(rho.size());
}

double epsFromDensity(double rho, double eps1, double eps2, double p)
{
    if (rho <= 0.0) return eps1;
    if (rho >= 1.0) return eps2;
    const double f = (p == 1.0) ? rho : std::pow(rho, p);
    return eps1 + f * (eps2 - eps1);
}

std::vector<Rect> rectangles(const std::vector<double> &rho, const Grid &g,
                             double threshold)
{
    std::vector<Rect> out;
    const int n = g.count();
    if (!g.valid() || static_cast<int>(rho.size()) != n) return out;

    std::vector<char> on(static_cast<size_t>(n), 0);
    for (int k = 0; k < n; ++k) on[static_cast<size_t>(k)] = (rho[static_cast<size_t>(k)] >= threshold) ? 1 : 0;

    // 走査順に「まだ覆われていない画素」を見つけたら、まず右へ伸ばし、
    // 次にその幅を保ったまま下へ伸ばす (貪欲な極大矩形)。
    for (int j = 0; j < g.ny; ++j) {
        for (int i = 0; i < g.nx; ++i) {
            if (!on[static_cast<size_t>(j) * g.nx + i]) continue;
            int i1 = i;
            while (i1 < g.nx && on[static_cast<size_t>(j) * g.nx + i1]) ++i1;
            int j1 = j + 1;
            for (; j1 < g.ny; ++j1) {
                bool full = true;
                for (int ii = i; ii < i1; ++ii) {
                    if (!on[static_cast<size_t>(j1) * g.nx + ii]) { full = false; break; }
                }
                if (!full) break;
            }
            for (int jj = j; jj < j1; ++jj)
                for (int ii = i; ii < i1; ++ii)
                    on[static_cast<size_t>(jj) * g.nx + ii] = 0;
            Rect r;
            r.i0 = i; r.i1 = i1; r.j0 = j; r.j1 = j1;
            out.push_back(r);
        }
    }
    return out;
}

std::vector<double> rasterize(const Geometry *units, int count,
                              const Region &r, const Grid &g, int *skipped)
{
    std::vector<double> rho;
    if (!g.valid()) { if (skipped) *skipped = 0; return rho; }
    rho.assign(static_cast<size_t>(g.count()), 0.0);

    int skip = 0;
    for (int u = 0; u < count; ++u)
        if (!shapeSupported(units[u].shape)) ++skip;
    if (skipped) *skipped = skip;

    const double z = 0.5 * (r.z0_m + r.z1_m);
    for (int j = 0; j < g.ny; ++j) {
        const double y = g.cy_m(r, j);
        for (int i = 0; i < g.nx; ++i) {
            const double x = g.cx_m(r, i);
            int mat = 0;                       // 背景 = 空気
            for (int u = 0; u < count; ++u) {  // 後のユニットが優先
                if (!shapeSupported(units[u].shape)) continue;
                if (insideUnit(units[u], x, y, z)) mat = units[u].materialId;
            }
            rho[static_cast<size_t>(j) * g.nx + i] = (mat != 0) ? 1.0 : 0.0;
        }
    }
    return rho;
}

std::vector<Geometry> toGeometry(const std::vector<Rect> &rects,
                                 const Region &r, const Grid &g, int materialId)
{
    std::vector<Geometry> out;
    if (!g.valid()) return out;
    out.reserve(rects.size());
    for (const Rect &q : rects) {
        Geometry u;
        u.shape = 1;
        u.materialId = materialId;
        u.g[0] = r.x0_m + q.i0 * g.pitchX_m;
        u.g[1] = r.x0_m + q.i1 * g.pitchX_m;
        u.g[2] = r.y0_m + q.j0 * g.pitchY_m;
        u.g[3] = r.y0_m + q.j1 * g.pitchY_m;
        u.g[4] = r.z0_m;
        u.g[5] = r.z1_m;
        out.push_back(u);
    }
    return out;
}

} // namespace topo
} // namespace ofd
