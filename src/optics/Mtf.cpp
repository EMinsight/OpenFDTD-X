// Mtf.cpp
#include "Mtf.h"

#include <algorithm>
#include <cmath>
#include <complex>

namespace ofd {
namespace optics {

namespace {
// MSVC は <cmath> だけでは円周率のマクロを定義しない (.claude/rules/cpp-qt.md)
const double kPi = 3.14159265358979323846;
} // namespace

double mtfCutoff_cyc_per_mm(double lambda_nm, double fNumber)
{
    if (!(lambda_nm > 0.0) || !(fNumber > 0.0)) return 0.0;
    // λ [nm] → [mm] にして 1/(λ·F#)
    return 1.0 / (lambda_nm * 1.0e-6 * fNumber);
}

double diffractionLimitedMtf(double nu_cyc_per_mm, double lambda_nm,
                             double fNumber)
{
    const double nc = mtfCutoff_cyc_per_mm(lambda_nm, fNumber);
    if (!(nc > 0.0)) return 0.0;
    if (nu_cyc_per_mm <= 0.0) return 1.0;
    if (nu_cyc_per_mm >= nc) return 0.0;
    // 2 つの単位円を 2r = ν/νc だけずらしたときの重なり面積 / 円の面積
    const double phi = std::acos(nu_cyc_per_mm / nc);
    return (2.0 / kPi) * (phi - std::cos(phi) * std::sin(phi));
}

double geometricMtf(const std::vector<double> &coord_mm, double nu_cyc_per_mm)
{
    if (coord_mm.empty()) return 0.0;
    if (nu_cyc_per_mm == 0.0) return 1.0;
    // MTF(ν) = |(1/N) Σ exp(−i2πν x)| — 交点分布の特性関数の絶対値。
    // ヒストグラムに刻まないので、ビン幅による誤差が入らない。
    double re = 0.0, im = 0.0;
    for (double x : coord_mm) {
        const double ph = -2.0 * kPi * nu_cyc_per_mm * x;
        re += std::cos(ph);
        im += std::sin(ph);
    }
    const double n = static_cast<double>(coord_mm.size());
    return std::sqrt(re * re + im * im) / n;
}

MtfCurve mtfCurve(const std::vector<double> &coord_mm, double lambda_nm,
                  double fNumber, int points)
{
    MtfCurve c;
    const double nc = mtfCutoff_cyc_per_mm(lambda_nm, fNumber);
    if (!(nc > 0.0) || points < 2) return c;
    c.nu.reserve(points);
    c.diffraction.reserve(points);
    if (!coord_mm.empty()) c.geometric.reserve(points);
    for (int i = 0; i < points; ++i) {
        const double nu = nc * static_cast<double>(i)
                              / static_cast<double>(points - 1);
        c.nu.push_back(nu);
        c.diffraction.push_back(diffractionLimitedMtf(nu, lambda_nm, fNumber));
        if (!coord_mm.empty())
            c.geometric.push_back(geometricMtf(coord_mm, nu));
    }
    return c;
}

double frequencyAtMtf(const std::vector<double> &nu,
                      const std::vector<double> &mtf, double target)
{
    if (nu.size() != mtf.size() || nu.size() < 2) return 0.0;
    for (std::size_t i = 1; i < nu.size(); ++i) {
        if (mtf[i] <= target && mtf[i - 1] > target) {
            const double d = mtf[i - 1] - mtf[i];
            const double f = (d > 0.0) ? (mtf[i - 1] - target) / d : 0.0;
            return nu[i - 1] + f * (nu[i] - nu[i - 1]);
        }
    }
    return 0.0;
}

} // namespace optics
} // namespace ofd
