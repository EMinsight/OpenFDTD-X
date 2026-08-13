// DampedLeastSquares.cpp
#include "DampedLeastSquares.h"

#include <algorithm>
#include <cmath>

namespace ofd {
namespace optics {

namespace {

double rmsOf(const std::vector<double> &r)
{
    if (r.empty()) return 0.0;
    double s = 0.0;
    for (double v : r) s += v * v;
    return std::sqrt(s / static_cast<double>(r.size()));
}

void clampToBox(std::vector<double> &x, const DlsOptions &o)
{
    for (std::size_t i = 0; i < x.size(); ++i) {
        if (i < o.lower.size() && x[i] < o.lower[i]) x[i] = o.lower[i];
        if (i < o.upper.size() && x[i] > o.upper[i]) x[i] = o.upper[i];
    }
}

} // namespace

bool solveLinear(std::vector<std::vector<double>> A, std::vector<double> b,
                 std::vector<double> *x)
{
    const std::size_t n = b.size();
    if (A.size() != n || !x) return false;
    for (std::size_t i = 0; i < n; ++i)
        if (A[i].size() != n) return false;

    for (std::size_t k = 0; k < n; ++k) {
        // 部分ピボット
        std::size_t p = k;
        for (std::size_t i = k + 1; i < n; ++i)
            if (std::fabs(A[i][k]) > std::fabs(A[p][k])) p = i;
        if (!(std::fabs(A[p][k]) > 0.0)) return false;      // 特異
        std::swap(A[k], A[p]);
        std::swap(b[k], b[p]);
        for (std::size_t i = k + 1; i < n; ++i) {
            const double f = A[i][k] / A[k][k];
            if (f == 0.0) continue;
            for (std::size_t j = k; j < n; ++j) A[i][j] -= f * A[k][j];
            b[i] -= f * b[k];
        }
    }
    x->assign(n, 0.0);
    for (std::size_t ii = 0; ii < n; ++ii) {
        const std::size_t i = n - 1 - ii;
        double s = b[i];
        for (std::size_t j = i + 1; j < n; ++j) s -= A[i][j] * (*x)[j];
        (*x)[i] = s / A[i][i];
    }
    return true;
}

DlsResult solve(const ResidualFn &f, const std::vector<double> &x0,
                const DlsOptions &opt)
{
    DlsResult res;
    if (!f || x0.empty()) { res.note = "no residual function or no variables";
                            return res; }
    const std::size_t n = x0.size();

    std::vector<double> x = x0;
    clampToBox(x, opt);
    std::vector<double> r;
    if (!f(x, r) || r.empty()) { res.note = "the residual could not be built "
                                            "at the starting point";
                                 return res; }
    ++res.evaluations;
    const std::size_t m = r.size();
    res.rms0 = rmsOf(r);
    double best = res.rms0;
    res.x = x;
    res.rms = best;
    res.ok = true;
    res.note = "reached the iteration limit";

    double lambda = (opt.lambda0 > 0.0) ? opt.lambda0 : 1.0e-3;
    std::vector<std::vector<double>> J(m, std::vector<double>(n, 0.0));

    for (int it = 0; it < opt.maxIterations; ++it) {
        res.iterations = it + 1;
        // ── ヤコビアン (中心差分) ──────────────────────────────────────
        bool jacOk = true;
        for (std::size_t j = 0; j < n && jacOk; ++j) {
            const double h = (j < opt.step.size() && opt.step[j] > 0.0)
                                 ? opt.step[j]
                                 : std::max(1.0e-6, 1.0e-6 * std::fabs(x[j]));
            std::vector<double> xp = x, xm = x, rp, rm;
            xp[j] += h;
            xm[j] -= h;
            clampToBox(xp, opt);
            clampToBox(xm, opt);
            const double dx = xp[j] - xm[j];
            if (!(std::fabs(dx) > 0.0)) { jacOk = false; break; }
            if (!f(xp, rp) || !f(xm, rm) || rp.size() != m || rm.size() != m) {
                jacOk = false; break;
            }
            res.evaluations += 2;
            for (std::size_t i = 0; i < m; ++i) J[i][j] = (rp[i] - rm[i]) / dx;
        }
        if (!jacOk) { res.note = "the residual could not be evaluated while "
                                 "building the Jacobian";
                      break; }

        // ── 正規方程式 ────────────────────────────────────────────────
        std::vector<std::vector<double>> A(n, std::vector<double>(n, 0.0));
        std::vector<double> g(n, 0.0);
        for (std::size_t a = 0; a < n; ++a) {
            for (std::size_t b2 = 0; b2 < n; ++b2) {
                double s = 0.0;
                for (std::size_t i = 0; i < m; ++i) s += J[i][a] * J[i][b2];
                A[a][b2] = s;
            }
            double s = 0.0;
            for (std::size_t i = 0; i < m; ++i) s += J[i][a] * r[i];
            g[a] = -s;
        }

        bool improved = false;
        for (int trial = 0; trial < 8 && !improved; ++trial) {
            std::vector<std::vector<double>> Ad = A;
            for (std::size_t a = 0; a < n; ++a) {
                // Marquardt の対角スケーリング (単位の違う変数に強い)
                const double d = (A[a][a] > 0.0) ? A[a][a] : 1.0;
                Ad[a][a] += lambda * d;
            }
            std::vector<double> dx;
            if (!solveLinear(Ad, g, &dx)) { lambda *= 10.0; continue; }

            std::vector<double> xt(n);
            for (std::size_t a = 0; a < n; ++a) xt[a] = x[a] + dx[a];
            clampToBox(xt, opt);
            std::vector<double> rt;
            if (!f(xt, rt) || rt.size() != m) { lambda *= 10.0; continue; }
            ++res.evaluations;
            const double q = rmsOf(rt);
            if (q < best) {
                const double gain = best - q;
                x = xt; r = rt; best = q;
                res.x = x; res.rms = best;
                lambda = std::max(1.0e-12, lambda * 0.3);
                improved = true;
                if (gain < opt.tolerance) {
                    res.note = "converged (the improvement fell below the "
                               "tolerance)";
                    return res;
                }
            } else {
                lambda *= 10.0;
            }
        }
        if (!improved) {
            res.note = "no further improvement (the damping could not find a "
                       "better step)";
            return res;
        }
    }
    return res;
}

} // namespace optics
} // namespace ofd
