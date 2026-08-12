#include "SeriesCompare.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ofd {
namespace cmp {

double toLinear(double v, Scale s)
{
    switch (s) {
        case Scale::PowerDb:     return std::pow(10.0, v / 10.0);
        case Scale::AmplitudeDb: return std::pow(10.0, v / 20.0);
        default:                 return v;
    }
}

double fromLinear(double v, Scale s)
{
    switch (s) {
        case Scale::PowerDb:
            return (v > 0.0) ? 10.0 * std::log10(v)
                             : -std::numeric_limits<double>::infinity();
        case Scale::AmplitudeDb:
            return (v > 0.0) ? 20.0 * std::log10(v)
                             : -std::numeric_limits<double>::infinity();
        default:
            return v;
    }
}

Series convert(const Series &s, Scale from, Scale to)
{
    Series out = s;
    if (from == to) return out;                 // 恒等 (1 ビットも動かさない)
    for (double &v : out.y) v = fromLinear(toLinear(v, from), to);
    return out;
}

Series resampleTo(const Series &b, const std::vector<double> &xs)
{
    Series out;
    if (b.x.size() != b.y.size() || b.x.size() < 2) return out;
    const double lo = b.x.front(), hi = b.x.back();
    out.x.reserve(xs.size());
    out.y.reserve(xs.size());
    std::size_t k = 0;
    for (double x : xs) {
        if (x < lo || x > hi) continue;         // 外挿はしない
        while (k + 2 < b.x.size() && b.x[k + 1] < x) ++k;
        // 節点にちょうど乗っていればその値をそのまま使う (補間で汚さない)
        if (x == b.x[k])         { out.x.push_back(x); out.y.push_back(b.y[k]); continue; }
        if (x == b.x[k + 1])     { out.x.push_back(x); out.y.push_back(b.y[k + 1]); continue; }
        const double dx = b.x[k + 1] - b.x[k];
        const double t = (dx != 0.0) ? (x - b.x[k]) / dx : 0.0;
        out.x.push_back(x);
        out.y.push_back(b.y[k] + t * (b.y[k + 1] - b.y[k]));
    }
    return out;
}

Agreement compare(const Series &a, const Series &b)
{
    Agreement g;
    if (!a.valid() || !b.valid()) return g;
    const Series r = resampleTo(b, a.x);
    if (r.x.size() < 2) return g;

    // a のうち載せ替えられた x だけを取り出す (r.x は a.x の部分列)
    std::vector<double> av;
    av.reserve(r.x.size());
    std::size_t k = 0;
    for (std::size_t i = 0; i < a.x.size() && k < r.x.size(); ++i)
        if (a.x[i] == r.x[k]) { av.push_back(a.y[i]); ++k; }
    if (av.size() != r.y.size() || av.size() < 2) return g;

    const std::size_t n = av.size();
    double sumD = 0.0, sumD2 = 0.0, maxAbs = 0.0, sumA2 = 0.0;
    double ma = 0.0, mb = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double d = r.y[i] - av[i];
        sumD += d;
        sumD2 += d * d;
        maxAbs = std::max(maxAbs, std::fabs(d));
        sumA2 += av[i] * av[i];
        ma += av[i];
        mb += r.y[i];
    }
    ma /= double(n);
    mb /= double(n);
    double ca = 0.0, cb = 0.0, cab = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double da = av[i] - ma, db = r.y[i] - mb;
        ca += da * da;
        cb += db * db;
        cab += da * db;
    }

    g.n = int(n);
    g.maxAbs = maxAbs;
    g.rms = std::sqrt(sumD2 / double(n));
    g.bias = sumD / double(n);
    g.relL2 = (sumA2 > 0.0) ? std::sqrt(sumD2 / sumA2) : 0.0;
    g.correlation = (ca > 0.0 && cb > 0.0) ? cab / std::sqrt(ca * cb) : 0.0;
    g.valid = true;
    return g;
}

double overlapFraction(const Series &a, const Series &b)
{
    if (!a.valid() || !b.valid()) return 0.0;
    const double lo = b.x.front(), hi = b.x.back();
    std::size_t in = 0;
    for (double x : a.x) if (x >= lo && x <= hi) ++in;
    return double(in) / double(a.x.size());
}

} // namespace cmp
} // namespace ofd
