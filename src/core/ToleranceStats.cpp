// ToleranceStats.cpp — ToleranceStats.h の実装 (Qt 非依存)
#include "ToleranceStats.h"

#include <cmath>

namespace ofd {
namespace tolstat {

namespace {
constexpr double kPi = 3.14159265358979323846;
bool posFinite(double v) { return std::isfinite(v) && v > 0.0; }
} // namespace

bool isContinuous(const Variable &v)
{
    if (v.dist == Dist::Discrete) return false;
    if (!std::isfinite(v.center)) return false;
    return posFinite(v.spread);
}

double pdf(const Variable &v, double x)
{
    if (!isContinuous(v) || !std::isfinite(x)) return 0.0;
    const double s = v.spread;
    switch (v.dist) {
    case Dist::Normal: {
        const double z = (x - v.center) / s;
        return std::exp(-0.5 * z * z) / (s * std::sqrt(2.0 * kPi));
    }
    case Dist::Uniform:
        return (x >= v.center - s && x <= v.center + s) ? 1.0 / (2.0 * s) : 0.0;
    case Dist::Rayleigh: {
        const double t = x - v.center;
        if (t < 0.0) return 0.0;
        return (t / (s * s)) * std::exp(-0.5 * t * t / (s * s));
    }
    case Dist::Discrete:
        break;
    }
    return 0.0;
}

double stdDev(const Variable &v)
{
    if (!isContinuous(v)) return 0.0;
    switch (v.dist) {
    case Dist::Normal:   return v.spread;
    case Dist::Uniform:  return v.spread / std::sqrt(3.0);      // a/√3 [2]
    case Dist::Rayleigh: return v.spread * std::sqrt(2.0 - kPi / 2.0);  // [3]
    case Dist::Discrete: break;
    }
    return 0.0;
}

double mean(const Variable &v)
{
    if (!isContinuous(v)) return 0.0;
    switch (v.dist) {
    case Dist::Normal:
    case Dist::Uniform:  return v.center;
    case Dist::Rayleigh: return v.center + v.spread * std::sqrt(kPi / 2.0); // [3]
    case Dist::Discrete: break;
    }
    return 0.0;
}

double normalCoverage(double k)
{
    if (!std::isfinite(k) || k <= 0.0) return 0.0;
    return std::erf(k / std::sqrt(2.0));
}

Interval coverageInterval(const Variable &v, double k)
{
    Interval iv;
    if (!isContinuous(v) || !posFinite(k)) return iv;
    const double p = normalCoverage(k);
    const double s = v.spread;
    switch (v.dist) {
    case Dist::Normal:
        iv.lo = v.center - k * s;
        iv.hi = v.center + k * s;
        break;
    case Dist::Uniform:
        // CDF が線形なので、中央被覆確率 p の区間は台を p 倍したもの
        iv.lo = v.center - p * s;
        iv.hi = v.center + p * s;
        break;
    case Dist::Rayleigh: {
        // Q(q) = center + σ·√(−2·ln(1−q))
        const auto q = [&](double prob) {
            const double u = 1.0 - prob;
            return (u <= 0.0) ? v.center
                              : v.center + s * std::sqrt(-2.0 * std::log(u));
        };
        iv.lo = q(0.5 * (1.0 - p));
        iv.hi = q(0.5 * (1.0 + p));
        break;
    }
    case Dist::Discrete:
        break;
    }
    return iv;
}

std::vector<Point> pdfCurve(const Variable &v, int n)
{
    std::vector<Point> out;
    if (!isContinuous(v) || n < 2) return out;

    double lo = 0.0, hi = 0.0;
    switch (v.dist) {
    case Dist::Normal:
        lo = v.center - 4.0 * v.spread;
        hi = v.center + 4.0 * v.spread;
        break;
    case Dist::Uniform:
        lo = v.center - 1.25 * v.spread;
        hi = v.center + 1.25 * v.spread;
        break;
    case Dist::Rayleigh:
        lo = v.center;
        hi = v.center + 4.0 * v.spread;
        break;
    case Dist::Discrete:
        return out;
    }

    out.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        const double x = lo + (hi - lo) * i / (n - 1);
        Point p;
        p.x = x;
        p.y = pdf(v, x);
        out.push_back(p);
    }
    return out;
}

} // namespace tolstat
} // namespace ofd
