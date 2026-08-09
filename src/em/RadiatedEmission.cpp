// RadiatedEmission.cpp — 遠方界から放射妨害波レベルへの換算 (定義は .h)
#include "RadiatedEmission.h"

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

} // namespace em
} // namespace ofd
