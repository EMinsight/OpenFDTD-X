// RadiatedEmission.cpp — 遠方界から放射妨害波レベルへの換算 (定義は .h)
#include "RadiatedEmission.h"
#include "RadioPropagation.h"

#include <algorithm>
#include <cmath>

namespace ofd {
namespace em {

namespace {
const double kC0 = 2.99792458e8;   // 真空中の光速 [m/s]
} // namespace

double eirpToFieldConstantDb()
{
    // 20·log10(√30 × 10⁶) ≈ 134.771 dB
    return 20.0 * std::log10(std::sqrt(30.0) * 1.0e6);
}

FieldStrength fieldStrength(double gainDbi, double powerW, double distM)
{
    FieldStrength out;
    if (!(powerW > 0) || !(distM > 0) || !std::isfinite(gainDbi))
        return out;
    out.dBuVm = eirpToFieldConstantDb() + 10.0 * std::log10(powerW)
                + gainDbi - 20.0 * std::log10(distM);
    // E[V/m] = √(30·G·P)/d — dB を経由せず直接求める (往復の丸めを避ける)
    const double gLin = std::pow(10.0, gainDbi / 10.0);
    out.vPerM = std::sqrt(30.0 * gLin * powerW) / distM;
    out.eirpDbm = 10.0 * std::log10(gLin * powerW * 1000.0);
    out.valid = true;
    return out;
}

double groundReflectionMaxDb()
{
    return 20.0 * std::log10(2.0);
}

double fraunhoferDistanceM(double maxDimM, double lambdaM)
{
    if (!(maxDimM > 0) || !(lambdaM > 0)) return 0.0;
    return 2.0 * maxDimM * maxDimM / lambdaM;
}

double wavelengthM(double freqHz)
{
    return (freqHz > 0) ? kC0 / freqHz : 0.0;
}

double marginDb(double levelDbuVm, double limitDbuVm)
{
    return limitDbuVm - levelDbuVm;
}

GroundEnhancement groundEnhancement(EmcSite site, double distM,
                                    double antHeightM, double freqHz,
                                    double eutHeightM)
{
    namespace prop = ofd::em::propagation;
    GroundEnhancement g;
    if (!(distM > 0.0) || !(freqHz > 0.0)) return g;
    if (!(antHeightM > 0.0) || !(eutHeightM > 0.0)) return g;
    g.valid = true;
    g.applies = (site == EmcSite::OpenArea || site == EmcSite::SemiAnechoic);
    if (!g.applies) return g;          // 反射が無い = 増分 0

    const double fsl = prop::freeSpacePathLossDb(distM, freqHz);
    const auto gain = [&](double hr) {
        // 自由空間より損失が小さければ強め合っている = 正の増分
        return fsl - prop::twoRayPathLossDb(distM, eutHeightM, hr, freqHz, 1.0);
    };
    g.atHeightDb = gain(antHeightM);

    // 1〜4 m の走査。規格は連続走査で最大を拾うので、細かく刻んで最大を取る
    double best = -1e300;
    const int steps = 601;             // 5 mm 刻み
    for (int i = 0; i < steps; ++i) {
        const double hr = 1.0 + 3.0 * i / (steps - 1);
        best = std::max(best, gain(hr));
    }
    g.scanMaxDb = best;
    return g;
}

} // namespace em
} // namespace ofd
