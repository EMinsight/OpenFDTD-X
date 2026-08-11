// MieSphere.cpp — 完全導体球の Mie 厳密解 (仕様は MieSphere.h)
#include "em/MieSphere.h"

#include <cmath>
#include <complex>
#include <vector>

namespace ofd {
namespace em {

namespace {

constexpr double kC0 = 2.99792458e8;   // 真空中の光速 [m/s]
// MSVC は <cmath> で M_PI を出さない (_USE_MATH_DEFINES 依存) ので自前で持つ
constexpr double kPi = 3.14159265358979323846;

// 球ベッセル j_0..j_N。**下向き漸化 (Miller)** で求める。
// 上向きだと n > x の領域で j_n が急激に小さくなるため丸め誤差に埋もれる。
// 適当な高次から下ろして、最後に j_0 = sin(x)/x で規格化する。
std::vector<double> sphericalJ(double x, int nMax)
{
    const int nStart = nMax + 25;
    std::vector<double> j(std::size_t(nStart) + 2, 0.0);
    j[std::size_t(nStart)] = 1e-30;          // 種 (大きさは規格化で消える)
    for (int n = nStart; n >= 1; --n)
        j[std::size_t(n - 1)] =
            (2.0 * n + 1.0) / x * j[std::size_t(n)] - j[std::size_t(n + 1)];
    const double scale = (std::sin(x) / x) / j[0];
    std::vector<double> out(std::size_t(nMax) + 1);
    for (int n = 0; n <= nMax; ++n) out[std::size_t(n)] = j[std::size_t(n)] * scale;
    return out;
}

// 球ノイマン y_0..y_N。こちらは**上向き漸化が安定**(n とともに増える)。
std::vector<double> sphericalY(double x, int nMax)
{
    std::vector<double> y(std::size_t(nMax) + 1, 0.0);
    y[0] = -std::cos(x) / x;
    if (nMax >= 1) y[1] = -std::cos(x) / (x * x) - std::sin(x) / x;
    for (int n = 1; n < nMax; ++n)
        y[std::size_t(n + 1)] =
            (2.0 * n + 1.0) / x * y[std::size_t(n)] - y[std::size_t(n - 1)];
    return y;
}

} // namespace

MieSphereRcs pecSphereRcs(double radius_m, double freq_hz)
{
    MieSphereRcs r;
    if (!(radius_m > 0.0) || !(freq_hz > 0.0)) return r;

    const double lambda = kC0 / freq_hz;
    const double x = 2.0 * kPi * radius_m / lambda;
    if (!(x > 0.0) || !std::isfinite(x)) return r;
    r.ka = x;

    // Wiscombe の目安 + 余裕。x が小さいときも最低 3 項は取る
    int nMax = int(x + 4.0 * std::cbrt(x) + 2.0) + 5;
    if (nMax < 3) nMax = 3;
    r.terms = nMax;

    const std::vector<double> j = sphericalJ(x, nMax + 1);
    const std::vector<double> y = sphericalY(x, nMax + 1);

    std::complex<double> sumBack(0.0, 0.0), sumFwd(0.0, 0.0);
    for (int n = 1; n <= nMax; ++n) {
        const std::complex<double> hn(j[std::size_t(n)], y[std::size_t(n)]);
        const std::complex<double> hm(j[std::size_t(n - 1)],
                                      y[std::size_t(n - 1)]);
        const std::complex<double> an = std::complex<double>(j[std::size_t(n)])
                                        / hn;
        const std::complex<double> bnNum(x * j[std::size_t(n - 1)]
                                         - n * j[std::size_t(n)], 0.0);
        const std::complex<double> bnDen = x * hm - double(n) * hn;
        const std::complex<double> bn = bnNum / bnDen;
        const double sign = (n % 2 == 0) ? 1.0 : -1.0;   // (−1)ⁿ
        sumBack += sign * (2.0 * n + 1.0) * (an - bn);
        sumFwd  += (2.0 * n + 1.0) * (an + bn);
    }

    const double pref = kPi * radius_m * radius_m / (x * x);
    r.backward_m2 = pref * std::norm(sumBack);
    r.forward_m2  = pref * std::norm(sumFwd);
    r.valid = std::isfinite(r.backward_m2) && std::isfinite(r.forward_m2);
    return r;
}

} // namespace em
} // namespace ofd
