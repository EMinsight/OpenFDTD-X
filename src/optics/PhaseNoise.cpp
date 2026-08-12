// PhaseNoise.cpp
#include "PhaseNoise.h"

#include <cmath>
#include <limits>

namespace ofd {
namespace optics {

double phaseVariance(double linewidth_Hz, double delay_s)
{
    if (!(linewidth_Hz > 0.0) || !(delay_s > 0.0)) return 0.0;
    // Wiener 過程 (白色周波数雑音の積分) の厳密解
    return 2.0 * M_PI * linewidth_Hz * delay_s;
}

double visibility(double linewidth_Hz, double delay_s)
{
    // V = e^{−σ²/2} = e^{−π·Δν·τ}
    return std::exp(-0.5 * phaseVariance(linewidth_Hz, delay_s));
}

PhaseNoiseResult analyse(const PhaseNoiseInput &in)
{
    PhaseNoiseResult r;
    if (!in.valid()) return r;
    r.valid = true;

    const double s2 = phaseVariance(in.linewidth_Hz, in.delay_s);
    r.phaseVariance = s2;
    r.phaseRms_rad = std::sqrt(s2);
    r.coherenceTime_s = (in.linewidth_Hz > 0.0)
                            ? 1.0 / (M_PI * in.linewidth_Hz)
                            : std::numeric_limits<double>::infinity();
    r.visibility = std::exp(-0.5 * s2);

    // P = (P₀/2)(1 + cos(φ₀ + Δφ))、Δφ ~ N(0, σ²)
    //   ⟨cos(φ₀+Δφ)⟩  = cos φ₀ · e^{−σ²/2}
    //   ⟨cos²(φ₀+Δφ)⟩ = 1/2 + (1/2)·cos 2φ₀ · e^{−2σ²}
    // (⟨sin Δφ⟩ = 0、正規分布の特性関数から。どちらも厳密)
    const double half = 0.5 * in.power_W;
    const double c1 = std::cos(in.bias_rad);
    const double c2 = std::cos(2.0 * in.bias_rad);
    const double mean = c1 * std::exp(-0.5 * s2);
    const double meanSq = 0.5 + 0.5 * c2 * std::exp(-2.0 * s2);
    double var = meanSq - mean * mean;
    if (var < 0.0) var = 0.0;          // 丸めで僅かに負になることがある

    r.meanPower_W = half * (1.0 + mean);
    r.rmsPower_W = half * std::sqrt(var);
    r.relativeIntensityNoise =
        (r.meanPower_W > 0.0) ? r.rmsPower_W / r.meanPower_W : 0.0;
    r.rin_dB = (r.relativeIntensityNoise > 0.0)
                   ? 20.0 * std::log10(r.relativeIntensityNoise)
                   : -std::numeric_limits<double>::infinity();
    return r;
}

} // namespace optics
} // namespace ofd
