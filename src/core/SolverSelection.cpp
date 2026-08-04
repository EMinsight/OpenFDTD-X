// SolverSelection.cpp — ソルバ選定の目安 (式の出典はヘッダ参照)
#include "core/SolverSelection.h"

#include <cmath>

namespace ofd {
namespace selsolver {

double wavelength(double speed_mps, double freq_hz)
{
    if (!(speed_mps > 0.0) || !(freq_hz > 0.0)) return 0.0;
    return speed_mps / freq_hz;
}

double electricalSize(double length_m, double lambda_m)
{
    if (!(lambda_m > 0.0) || !(length_m >= 0.0)) return 0.0;
    return length_m / lambda_m;
}

double cellsPerWavelength(double lambda_m, double dx_m)
{
    if (!(lambda_m > 0.0) || !(dx_m > 0.0)) return 0.0;
    return lambda_m / dx_m;
}

// DFT の周波数分解能 Δf = 1/T が共振の半値全幅 f/Q 以下、すなわち Q ≤ f·T。
double maxResolvableQ(double freq_hz, double duration_s)
{
    if (!(freq_hz > 0.0) || !(duration_s > 0.0)) return 0.0;
    return freq_hz * duration_s;
}

// Schroeder 1996: f_c = 2000·sqrt(T/V) (SI 単位。定数 2000 は s^-1/2·m^3/2)
double schroederFrequency(double rt60_s, double volume_m3)
{
    if (!(rt60_s > 0.0) || !(volume_m3 > 0.0)) return 0.0;
    return 2000.0 * std::sqrt(rt60_s / volume_m3);
}

// Thorp (Urick §5.3)。f は kHz、返り値は dB/km。
double thorpAbsorption_dBkm(double freq_kHz)
{
    if (!(freq_kHz >= 0.0)) return 0.0;
    const double f2 = freq_kHz * freq_kHz;
    return 0.11 * f2 / (1.0 + f2)
         + 44.0 * f2 / (4100.0 + f2)
         + 2.75e-4 * f2
         + 0.003;
}

double sphericalTransmissionLoss_dB(double range_km, double alpha_dBkm)
{
    if (!(range_km > 0.0)) return 0.0;
    const double r_m = range_km * 1000.0;
    return 20.0 * std::log10(r_m) + alpha_dBkm * range_km;
}

} // namespace selsolver
} // namespace ofd
