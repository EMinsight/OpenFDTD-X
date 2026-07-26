// NoiseFloorEstimator.cpp — RIR 末尾区間の RMS によるノイズフロア推定。
#include "NoiseFloorEstimator.h"

#include <cmath>

namespace ofd {
namespace acoustics {

namespace {
// dB 変換の数値下限 (振幅)。20*log10(1e-15) = -300 dB。
const double kMinAmplitude = 1e-15;

double amplitudeDb(double a) {
    if (a < kMinAmplitude) a = kMinAmplitude;
    return 20.0 * std::log10(a);
}
} // namespace

NoiseFloorEstimate estimateNoiseFloor(ArrayView<const double> x,
                                      double tailFraction,
                                      std::size_t minTailSamples) {
    NoiseFloorEstimate est;
    const std::size_t n = x.size();
    if (n == 0) {
        est.warning = "input is empty";
        return est;
    }
    if (tailFraction <= 0.0 || tailFraction > 1.0) tailFraction = 0.10;
    if (minTailSamples == 0) minTailSamples = 1;

    // 末尾区間長: n*tailFraction と minTailSamples の大きい方 (最大 n)
    std::size_t tail = static_cast<std::size_t>(
        std::floor(static_cast<double>(n) * tailFraction + 0.5));
    if (tail < minTailSamples) tail = minTailSamples;
    if (tail > n) {
        tail = n;
        est.warning = "input shorter than requested tail; whole signal used";
    }
    const std::size_t start = n - tail;

    double peak = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double a = std::fabs(x[i]);
        if (a > peak) peak = a;
    }

    double sumSq = 0.0;
    for (std::size_t i = start; i < n; ++i) sumSq += x[i] * x[i];
    const double rms = std::sqrt(sumSq / static_cast<double>(tail));

    est.noiseRms = rms;
    est.noiseFloorDb = amplitudeDb(rms);
    est.peakAbs = peak;
    est.peakDb = amplitudeDb(peak);
    est.dynamicRangeDb = est.peakDb - est.noiseFloorDb;
    est.tailStartIndex = start;
    est.tailSampleCount = tail;
    est.valid = true;
    return est;
}

} // namespace acoustics
} // namespace ofd
