// Colorimetry.cpp
#include "Colorimetry.h"

#include <algorithm>
#include <cmath>

namespace ofd {
namespace colorimetry {

namespace {

// 区分ガウシアン g(x; μ, σ1, σ2): x < μ で σ1、x >= μ で σ2 を使う
// (Wyman et al. 2013, eq. 3)。
inline double pieceGauss(double x, double mu, double s1, double s2)
{
    const double t = (x - mu) * ((x < mu) ? (1.0 / s1) : (1.0 / s2));
    return std::exp(-0.5 * t * t);
}

} // namespace

// ── CIE 1931 2° 等色関数 (Wyman et al. 2013 "multi-lobe fit", Table 1) ───────
// 最大誤差は数 % 程度。数表を持ち込まずに色度・CCT を計算するために使う。
double cieXbar(double l)
{
    return 1.056 * pieceGauss(l, 599.8, 37.9, 31.0)
         + 0.362 * pieceGauss(l, 442.0, 16.0, 26.7)
         - 0.065 * pieceGauss(l, 501.1, 20.4, 26.2);
}

double cieYbar(double l)
{
    return 0.821 * pieceGauss(l, 568.8, 46.9, 40.5)
         + 0.286 * pieceGauss(l, 530.9, 16.3, 31.1);
}

double cieZbar(double l)
{
    return 1.217 * pieceGauss(l, 437.0, 11.8, 36.0)
         + 0.681 * pieceGauss(l, 459.0, 26.0, 13.8);
}

// ── 分光分布モデル ──────────────────────────────────────────────────────────
double lobeSpectrum(const std::vector<GaussLobe> &lobes, double l)
{
    double s = 0.0;
    for (const GaussLobe &g : lobes) {
        if (g.weight <= 0.0 || g.fwhm_nm <= 0.0) continue;
        // FWHM → 標準偏差
        const double sigma = g.fwhm_nm / 2.354820045030949;
        const double t = (l - g.peak_nm) / sigma;
        s += g.weight * std::exp(-0.5 * t * t);
    }
    return s;
}

double planckSpectrum(double l, double T)
{
    if (T <= 0.0 || l <= 0.0) return 0.0;
    // M(λ,T) = c1 / (λ^5 (exp(c2/(λT)) - 1))。定数倍は色度に効かないので
    // c1 は省き、c2 = hc/k = 1.4388e-2 m·K を nm·K で使う。
    const double c2_nmK = 1.4388e7;
    const double x = c2_nmK / (l * T);
    // exp のオーバーフロー回避 (短波長側は寄与が 0 に落ちる)
    if (x > 700.0) return 0.0;
    const double denom = std::expm1(x);
    if (denom <= 0.0) return 0.0;
    const double l5 = l * l * l * l * l;
    return 1.0 / (l5 * denom);
}

// ── 三刺激値 ────────────────────────────────────────────────────────────────
XYZ integrate(const std::function<double(double)> &spd)
{
    XYZ c;
    if (!spd) return c;
    for (double l = kLambdaMin; l <= kLambdaMax + 1e-9; l += kLambdaStep) {
        const double s = spd(l);
        if (!(s > 0.0)) continue;
        c.X += s * cieXbar(l);
        c.Y += s * cieYbar(l);
        c.Z += s * cieZbar(l);
    }
    c.X *= kLambdaStep;
    c.Y *= kLambdaStep;
    c.Z *= kLambdaStep;
    return c;
}

Chromaticity chromaticity(const XYZ &c)
{
    Chromaticity r;
    const double sum = c.X + c.Y + c.Z;
    if (!(sum > 0.0)) return r;
    r.x = c.X / sum;
    r.y = c.Y / sum;
    const double d = c.X + 15.0 * c.Y + 3.0 * c.Z;
    if (!(d > 0.0)) return r;
    r.u1960 = 4.0 * c.X / d;
    r.v1960 = 6.0 * c.Y / d;
    r.up = r.u1960;
    r.vp = 1.5 * r.v1960;
    r.valid = true;
    return r;
}

double deltaUV(const Chromaticity &a, const Chromaticity &b)
{
    if (!a.valid || !b.valid) return 0.0;
    return std::hypot(a.up - b.up, a.vp - b.vp);
}

namespace {

// 黒体軌跡上の温度 T の点 (CIE 1960 UCS)
Chromaticity planckianPoint(double T)
{
    return chromaticity(integrate([T](double l) { return planckSpectrum(l, T); }));
}

} // namespace

CctResult correlatedColorTemperature(const Chromaticity &c)
{
    CctResult r;
    if (!c.valid) return r;

    // 1) 対数刻みの粗探索で最小距離の温度を掴む
    const double logMin = std::log(1000.0), logMax = std::log(25000.0);
    // 粗探索の刻みは対数で約 2.7% (GUI から編集のたびに呼ばれるので、
    // 表示精度に対して十分な範囲で評価回数を抑える)
    const int coarse = 120;
    double best = -1.0, bestT = 0.0;
    for (int i = 0; i <= coarse; ++i) {
        const double T = std::exp(logMin + (logMax - logMin) * i / coarse);
        const Chromaticity p = planckianPoint(T);
        if (!p.valid) continue;
        const double d = std::hypot(c.u1960 - p.u1960, c.v1960 - p.v1960);
        if (best < 0.0 || d < best) { best = d; bestT = T; }
    }
    if (best < 0.0) return r;

    // 2) 黄金分割で温度を絞る (距離は温度に対し単峰)
    double lo = bestT / 1.2, hi = bestT * 1.2;
    const double phi = 0.6180339887498949;
    double x1 = hi - (hi - lo) * phi, x2 = lo + (hi - lo) * phi;
    auto dist = [&c](double T) {
        const Chromaticity p = planckianPoint(T);
        if (!p.valid) return 1e9;
        return std::hypot(c.u1960 - p.u1960, c.v1960 - p.v1960);
    };
    double f1 = dist(x1), f2 = dist(x2);
    // 0.5 K まで絞れば表示精度 (1 K) には十分
    for (int it = 0; it < 60 && (hi - lo) > 0.5; ++it) {
        if (f1 < f2) { hi = x2; x2 = x1; f2 = f1; x1 = hi - (hi - lo) * phi; f1 = dist(x1); }
        else         { lo = x1; x1 = x2; f1 = f2; x2 = lo + (hi - lo) * phi; f2 = dist(x2); }
    }
    const double T = 0.5 * (lo + hi);
    const Chromaticity p = planckianPoint(T);
    if (!p.valid) return r;

    // 3) Duv の符号: 軌跡の接線に対する法線方向 (v が大きい側 = 緑寄りを +)
    const Chromaticity pa = planckianPoint(T * 0.999);
    const Chromaticity pb = planckianPoint(T * 1.001);
    const double tu = pb.u1960 - pa.u1960, tv = pb.v1960 - pa.v1960;
    const double tl = std::hypot(tu, tv);
    const double du = c.u1960 - p.u1960, dv = c.v1960 - p.v1960;
    double duv = std::hypot(du, dv);
    if (tl > 0.0) {
        // 法線 (-tv, tu) を正規化し、v 成分が正になる向きに揃える
        double nu = -tv / tl, nv = tu / tl;
        if (nv < 0.0) { nu = -nu; nv = -nv; }
        duv = du * nu + dv * nv;
    }

    // 黒体軌跡から離れすぎている光源 (単色光など) では CCT を定義しない。
    if (T <= 1000.5 || T >= 24990.0 || std::fabs(duv) > 0.05) return r;

    r.valid = true;
    r.cct_K = T;
    r.duv = duv;
    return r;
}

double luminousEfficacyOfRadiation(const std::function<double(double)> &spd)
{
    if (!spd) return 0.0;
    double num = 0.0, den = 0.0;
    for (double l = kLambdaMin; l <= kLambdaMax + 1e-9; l += kLambdaStep) {
        const double s = spd(l);
        if (!(s > 0.0)) continue;
        num += s * cieYbar(l);
        den += s;
    }
    if (!(den > 0.0)) return 0.0;
    return 683.0 * num / den;
}

double peakWavelength(const std::function<double(double)> &spd)
{
    if (!spd) return 0.0;
    double best = -1.0, bestL = 0.0;
    for (double l = kLambdaMin; l <= kLambdaMax + 1e-9; l += kLambdaStep) {
        const double s = spd(l);
        if (s > best) { best = s; bestL = l; }
    }
    return (best > 0.0) ? bestL : 0.0;
}

} // namespace colorimetry
} // namespace ofd
