// ReceiverNoise.cpp — 受光器の雑音収支 (詳細は .h)
#include "ReceiverNoise.h"

#include <algorithm>
#include <cmath>

namespace ofd {
namespace rxnoise {

namespace {
constexpr double kQ = 1.602176634e-19;   // 素電荷 [C] (SI 定義値)
constexpr double kB = 1.380649e-23;      // ボルツマン定数 [J/K] (SI 定義値)
constexpr double kZeroC = 273.15;        // 0 ℃ の絶対温度
} // namespace

Noise analyze(const Receiver &rx)
{
    Noise n;
    if (!(rx.bandwidth_Hz > 0.0) || !(rx.loadResistance_ohm > 0.0)
        || !(rx.responsivity_A_W > 0.0) || rx.opticalPower_W < 0.0)
        return n;
    const double tK = rx.temperature_C + kZeroC;
    if (!(tK > 0.0)) return n;    // 絶対零度以下は熱雑音が定義できない

    n.signalCurrent_A = rx.responsivity_A_W * rx.opticalPower_W;
    n.photocurrent_A = n.signalCurrent_A + std::max(0.0, rx.darkCurrent_A);

    if (rx.shot)
        n.shot_A2 = 2.0 * kQ * n.photocurrent_A * rx.bandwidth_Hz;
    if (rx.thermal)
        n.thermal_A2 = 4.0 * kB * tK * rx.bandwidth_Hz / rx.loadResistance_ohm;
    if (rx.rin) {
        const double rinLin = std::pow(10.0, rx.rin_dBHz / 10.0);
        n.rin_A2 = rinLin * n.signalCurrent_A * n.signalCurrent_A
                 * rx.bandwidth_Hz;
    }
    n.total_A2 = n.shot_A2 + n.thermal_A2 + n.rin_A2;
    n.rms_A = std::sqrt(n.total_A2);

    if (n.total_A2 > 0.0) {
        n.snr_dB = 10.0 * std::log10(n.signalCurrent_A * n.signalCurrent_A
                                     / n.total_A2);
        n.snrValid = (n.signalCurrent_A > 0.0);
        n.nep_W_rtHz = std::sqrt(n.total_A2 / rx.bandwidth_Hz)
                     / rx.responsivity_A_W;
    }
    n.valid = true;
    return n;
}

double rinLimitedSnrDb(double rin_dBHz, double bandwidth_Hz)
{
    if (!(bandwidth_Hz > 0.0)) return 0.0;
    // SNR = (RP)² / (rin (RP)² B) = 1/(rin B) — 光パワーに依らない
    return -rin_dBHz - 10.0 * std::log10(bandwidth_Hz);
}

} // namespace rxnoise
} // namespace ofd
