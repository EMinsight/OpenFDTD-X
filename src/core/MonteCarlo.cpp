// MonteCarlo.cpp
#include "MonteCarlo.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <random>

namespace ofd {
namespace montecarlo {

namespace {

const double kPi = 3.14159265358979323846;

// 標準正規の分位関数。Acklam の有理近似 (|誤差| < 1.15e-9)。
// std:: に逆誤差関数が無いため自前で持つ (外部ライブラリを増やさない)。
double normalQuantile(double p)
{
    if (p <= 0.0) return -std::numeric_limits<double>::infinity();
    if (p >= 1.0) return  std::numeric_limits<double>::infinity();

    static const double a[6] = { -3.969683028665376e+01,  2.209460984245205e+02,
                                 -2.759285104469687e+02,  1.383577518672690e+02,
                                 -3.066479806614716e+01,  2.506628277459239e+00 };
    static const double b[5] = { -5.447609879822406e+01,  1.615858368580409e+02,
                                 -1.556989798598866e+02,  6.680131188771972e+01,
                                 -1.328068155288572e+01 };
    static const double c[6] = { -7.784894002430293e-03, -3.223964580411365e-01,
                                 -2.400758277161838e+00, -2.549732539343734e+00,
                                  4.374664141464968e+00,  2.938163982698783e+00 };
    static const double d[4] = {  7.784695709041462e-03,  3.224671290700398e-01,
                                  2.445134137142996e+00,  3.754408661907416e+00 };
    const double pLow = 0.02425, pHigh = 1.0 - pLow;

    if (p < pLow) {
        const double q = std::sqrt(-2.0 * std::log(p));
        return (((((c[0]*q + c[1])*q + c[2])*q + c[3])*q + c[4])*q + c[5]) /
               ((((d[0]*q + d[1])*q + d[2])*q + d[3])*q + 1.0);
    }
    if (p > pHigh) {
        const double q = std::sqrt(-2.0 * std::log(1.0 - p));
        return -(((((c[0]*q + c[1])*q + c[2])*q + c[3])*q + c[4])*q + c[5]) /
                ((((d[0]*q + d[1])*q + d[2])*q + d[3])*q + 1.0);
    }
    const double q = p - 0.5, r = q * q;
    return (((((a[0]*r + a[1])*r + a[2])*r + a[3])*r + a[4])*r + a[5]) * q /
           (((((b[0]*r + b[1])*r + b[2])*r + b[3])*r + b[4])*r + 1.0);
}

// 有限値だけを集めて昇順に並べる
std::vector<double> finiteSorted(const std::vector<double> &v)
{
    std::vector<double> s;
    s.reserve(v.size());
    for (const double x : v) if (std::isfinite(x)) s.push_back(x);
    std::sort(s.begin(), s.end());
    return s;
}

// 昇順列の p 分位 (線形補間)。空なら NaN。
double quantileOf(const std::vector<double> &sorted, double p)
{
    if (sorted.empty()) return std::numeric_limits<double>::quiet_NaN();
    if (sorted.size() == 1) return sorted.front();
    const double x = p * double(sorted.size() - 1);
    const double f = std::floor(x);
    const std::size_t i = std::size_t(f);
    if (i + 1 >= sorted.size()) return sorted.back();
    const double t = x - f;
    return sorted[i] * (1.0 - t) + sorted[i + 1] * t;
}

} // namespace

double quantile(const tolstat::Variable &v, double p)
{
    if (!tolstat::isContinuous(v)) return v.center;
    p = std::min(std::max(p, 1e-12), 1.0 - 1e-12);
    switch (v.dist) {
    case tolstat::Dist::Normal:
        return v.center + v.spread * normalQuantile(p);
    case tolstat::Dist::Uniform:
        // 台 [center − a, center + a] の一様分布 — CDF は線形
        return v.center + v.spread * (2.0 * p - 1.0);
    case tolstat::Dist::Rayleigh:
        // Q(p) = center + σ·√(−2·ln(1−p))  [3]
        return v.center + v.spread * std::sqrt(-2.0 * std::log(1.0 - p));
    case tolstat::Dist::Discrete:
        break;
    }
    return v.center;
}

std::vector<double> sample(const std::vector<tolstat::Variable> &vars,
                           int n, Method method, std::uint64_t seed)
{
    std::vector<double> out;
    if (vars.empty() || n < 1) return out;
    const std::size_t m = vars.size();
    out.resize(std::size_t(n) * m);

    std::mt19937_64 rng(seed);
    std::uniform_real_distribution<double> u01(0.0, 1.0);

    if (method == Method::Latin) {
        // 変数ごとに [0,1) を n 層に分け、各層から 1 点を取って層順を
        // 独立に並べ替える [1]。各変数の周辺分布が n 点で必ず覆われる。
        // 丸括弧 + std::size_t(n) は関数宣言に解釈される (most vexing parse)
        std::vector<int> order;
        order.resize(std::size_t(n));
        for (std::size_t j = 0; j < m; ++j) {
            for (int i = 0; i < n; ++i) order[std::size_t(i)] = i;
            std::shuffle(order.begin(), order.end(), rng);
            for (int i = 0; i < n; ++i) {
                const double p = (double(order[std::size_t(i)]) + u01(rng))
                                 / double(n);
                out[std::size_t(i) * m + j] = quantile(vars[j], p);
            }
        }
    } else {
        for (int i = 0; i < n; ++i)
            for (std::size_t j = 0; j < m; ++j)
                out[std::size_t(i) * m + j] = quantile(vars[j], u01(rng));
    }
    return out;
}

Stats summarize(const std::vector<double> &fom)
{
    Stats s;
    const std::vector<double> v = finiteSorted(fom);
    s.count = int(v.size());
    if (v.size() < 2) return s;      // σ が定義できない

    double sum = 0.0;
    for (const double x : v) sum += x;
    s.mean = sum / double(v.size());
    double acc = 0.0;
    for (const double x : v) acc += (x - s.mean) * (x - s.mean);
    s.stdDev = std::sqrt(acc / double(v.size() - 1));   // 不偏推定
    s.min = v.front();
    s.max = v.back();
    s.median = quantileOf(v, 0.5);
    // ±3σ に相当する被覆 (正規分布で 99.73 %) の両側分位
    s.p3sigmaLo = quantileOf(v, 0.00135);
    s.p3sigmaHi = quantileOf(v, 0.99865);
    s.valid = true;
    return s;
}

Yield yieldOf(const std::vector<double> &fom, double threshold, Goal goal)
{
    Yield y;
    for (const double x : fom) {
        if (!std::isfinite(x)) continue;
        ++y.count;
        const bool ok = (goal == Goal::LessOrEqual) ? (x <= threshold)
                                                    : (x >= threshold);
        if (ok) ++y.pass;
    }
    if (y.count > 0) y.fraction = double(y.pass) / double(y.count);
    return y;
}

std::vector<Bin> histogram(const std::vector<double> &fom, int bins)
{
    std::vector<Bin> h;
    if (bins < 1) return h;
    const std::vector<double> v = finiteSorted(fom);
    if (v.empty()) return h;

    double lo = v.front(), hi = v.back();
    if (!(hi > lo)) {
        // 全て同じ値 — 幅 0 のヒストグラムは作れないので 1 本にまとめる
        h.push_back({ lo, double(v.size()) });
        return h;
    }
    const double w = (hi - lo) / double(bins);
    h.resize(std::size_t(bins));
    for (int b = 0; b < bins; ++b)
        h[std::size_t(b)].center = lo + w * (double(b) + 0.5);
    for (const double x : v) {
        int b = int((x - lo) / w);
        b = std::min(std::max(b, 0), bins - 1);   // 上端は最後のビンへ
        h[std::size_t(b)].count += 1.0;
    }
    return h;
}

} // namespace montecarlo
} // namespace ofd
