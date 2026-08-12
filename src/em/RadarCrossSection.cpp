// RadarCrossSection.cpp — RCS の単位換算 (定義は RadarCrossSection.h)
#include "RadarCrossSection.h"

#include <cmath>
#include <limits>

namespace ofd {
namespace em {

namespace {
const double kPi = 3.14159265358979323846;   // MSVC 対策 (M_PI を使わない)
const double kC0 = 2.99792458e8;             // 真空中の光速 [m/s]
} // namespace

double rcsDbsm(double sigma_m2)
{
    if (!(sigma_m2 > 0))
        return -std::numeric_limits<double>::infinity();
    // σ は電力次元 → 10log10 (20log10 ではない)
    return 10.0 * std::log10(sigma_m2);
}

double rcsFromDbsm(double dbsm)
{
    return std::pow(10.0, dbsm / 10.0);
}

double rcsPerWavelengthSq(double sigma_m2, double freqHz)
{
    if (!(freqHz > 0)) return 0.0;
    const double lambda = kC0 / freqHz;
    return sigma_m2 / (lambda * lambda);
}

double sphereGeometricArea(double radius_m)
{
    if (!(radius_m > 0)) return 0.0;
    return kPi * radius_m * radius_m;
}

double rcsPerGeometric(double sigma_m2, double radius_m)
{
    const double a = sphereGeometricArea(radius_m);
    return (a > 0) ? sigma_m2 / a : 0.0;
}

double sphereKa(double radius_m, double freqHz)
{
    if (!(radius_m > 0) || !(freqHz > 0)) return 0.0;
    const double lambda = kC0 / freqHz;
    return 2.0 * kPi * radius_m / lambda;
}


double rcsFromFar1dDbsm(double dbsm)
{
    // σ = 10^(dBsm/10)。dB は電力次元なので 10 で割る (20 ではない)。
    return std::pow(10.0, dbsm / 10.0);
}

bool far1dIsRcs(bool hasPlanewave, int feedCount)
{
    // カーネルは給電点があると遠方界を入力電力で正規化した相対利得にする。
    // その場合 far1d.log の値は RCS ではない (単位ラベルも [dB] になる)。
    return hasPlanewave && feedCount <= 0;
}

} // namespace em
} // namespace ofd