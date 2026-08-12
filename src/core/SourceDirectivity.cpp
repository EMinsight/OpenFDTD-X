#include "SourceDirectivity.h"

#include <algorithm>
#include <cmath>

namespace ofd {
namespace dir {

namespace {

const double kPi = 3.14159265358979323846;

double sinc(double x)
{
    if (std::fabs(x) < 1.0e-9) return 1.0 - x * x / 6.0;
    return std::sin(x) / x;
}

// 単調な区間での二分法 (60 回で倍精度の分解能に達する)
double bisect(double (*f)(double), double lo, double hi)
{
    double flo = f(lo);
    for (int i = 0; i < 200; ++i) {
        const double mid = 0.5 * (lo + hi);
        const double fm = f(mid);
        if ((flo < 0.0) == (fm < 0.0)) { lo = mid; flo = fm; }
        else                            { hi = mid; }
    }
    return 0.5 * (lo + hi);
}

double fHalfPower(double x) { return sinc(x) - 0.70710678118654752440; }
// tan x = x を tan の極を跨がない形 (sin x − x cos x) で解く
double fSidelobe(double x)  { return std::sin(x) - x * std::cos(x); }

bool usable(double w3_deg) { return w3_deg > 0.0 && w3_deg < 180.0; }

// 半値点の sinθ
double halfSin(double w3_deg)
{
    return std::sin(0.5 * w3_deg * kPi / 180.0);
}

} // namespace

double sincHalfPowerRoot()
{
    // sin(x)/x は (0, π) で単調減少。1/√2 になる点はこの区間に 1 つだけある。
    static const double r = bisect(&fHalfPower, 1.0e-6, kPi);
    return r;
}

double sincFirstSidelobeRoot()
{
    // sin x − x cos x は π と 3π/2 の間で符号を変える (tan x = x の第 1 根)。
    static const double r = bisect(&fSidelobe, kPi + 1.0e-9, 1.5 * kPi);
    return r;
}

double firstSidelobeDb(Shape s)
{
    if (s != Shape::LineAperture) return 0.0;   // サイドローブを持たない
    const double x = sincFirstSidelobeRoot();
    return 20.0 * std::log10(std::fabs(sinc(x)));
}

double amplitude(Shape s, double w3_deg, double angle_deg)
{
    if (s == Shape::Uniform || !usable(w3_deg)) return 1.0;
    const double sh = halfSin(w3_deg);
    if (!(sh > 0.0)) return 1.0;
    const double t = std::sin(angle_deg * kPi / 180.0) / sh;
    if (s == Shape::Gaussian) {
        // b = exp(−(ln2/2)·t²) — t = 1 で 2^(−1/2) = 1/√2 (半値)
        return std::exp(-0.5 * std::log(2.0) * t * t);
    }
    // LineAperture: |sin(x)/x|。位相 (符号) は .sbp が dB なので残せない。
    return std::fabs(sinc(sincHalfPowerRoot() * t));
}

double amplitudeDb(Shape s, double w3_deg, double angle_deg, double floorDb)
{
    const double b = amplitude(s, w3_deg, angle_deg);
    if (!(b > 0.0)) return floorDb;
    const double db = 20.0 * std::log10(b);
    return std::max(db, floorDb);
}

int recommendedPoints(double w3_deg, double span_deg)
{
    if (!(span_deg > 0.0)) return 0;
    // 刻みを θ₃/8 以下に (BELLHOP は表の間を振幅で線形補間するため)
    const double step = usable(w3_deg) ? w3_deg / 8.0 : 1.0;
    double n = std::ceil(span_deg / std::max(step, 1.0e-3)) + 1.0;
    n = std::min(std::max(n, 181.0), 3601.0);
    int ni = static_cast<int>(n);
    if ((ni % 2) == 0) ++ni;          // 奇数にして対称な範囲の中心 (0°) を含める
    return ni;
}

Pattern sample(Shape s, double w3_deg, double a0_deg, double a1_deg, int n,
               double floorDb)
{
    Pattern p;
    if (n < 2 || !(a1_deg > a0_deg)) return p;
    p.angle_deg.reserve(static_cast<std::size_t>(n));
    p.db.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        const double a = a0_deg + (a1_deg - a0_deg) * i / (n - 1);
        p.angle_deg.push_back(a);
        p.db.push_back(amplitudeDb(s, w3_deg, a, floorDb));
    }
    return p;
}

double firstNullSin(Shape s, double w3_deg)
{
    if (s != Shape::LineAperture || !usable(w3_deg)) return 0.0;
    const double sh = halfSin(w3_deg);
    if (!(sh > 0.0)) return 0.0;
    // x = π が第 1 ヌル ⇒ sinθ = (π/x₃)·sin(θ₃/2)
    const double sn = kPi / sincHalfPowerRoot() * sh;
    return (sn <= 1.0) ? sn : 0.0;    // 可視域の外なら「ヌルは見えない」
}

double equivalentWidthSin(Shape s, double w3_deg)
{
    if (s == Shape::Uniform || !usable(w3_deg)) return 2.0;   // |sinθ| ≤ 1 の幅
    const double sh = halfSin(w3_deg);
    if (!(sh > 0.0)) return 2.0;
    if (s == Shape::Gaussian) {
        // ∫exp(−ln2·t²)dt = √(π/ln2)
        return sh * std::sqrt(kPi / std::log(2.0));
    }
    // ∫sinc²(x₃t)dt = π/x₃
    return sh * kPi / sincHalfPowerRoot();
}

} // namespace dir
} // namespace ofd
