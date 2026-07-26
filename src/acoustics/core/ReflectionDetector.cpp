// ReflectionDetector.cpp — 移動 RMS 包絡線による反射音ピーク検出。
#include "ReflectionDetector.h"

#include <algorithm>
#include <cmath>

#include "NoiseFloorEstimator.h"

namespace ofd {
namespace acoustics {

namespace {
const double kTiny = 1e-15;

// 移動 RMS 包絡線 (中心づけ、端は窓を縮めずに切り詰めた区間平均)
std::vector<double> movingRmsEnvelope(ArrayView<const double> x,
                                      std::size_t window) {
    const std::size_t n = x.size();
    std::vector<double> env(n, 0.0);
    if (n == 0) return env;
    if (window < 1) window = 1;
    const std::size_t h = window / 2;
    // x² の累積和
    std::vector<double> cum(n + 1, 0.0);
    for (std::size_t i = 0; i < n; ++i) cum[i + 1] = cum[i] + x[i] * x[i];
    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t lo = (i >= h) ? i - h : 0;
        std::size_t hi = i + h + 1;
        if (hi > n) hi = n;
        const double mean = (cum[hi] - cum[lo]) / static_cast<double>(hi - lo);
        env[i] = std::sqrt(mean);
    }
    return env;
}

struct Candidate {
    std::size_t index;
    double envValue;
};

bool byEnvDesc(const Candidate &a, const Candidate &b) {
    return a.envValue > b.envValue;
}
bool byIndexAsc(const Candidate &a, const Candidate &b) {
    return a.index < b.index;
}

double clamp01(double v) {
    if (v < 0.0) return 0.0;
    if (v > 1.0) return 1.0;
    return v;
}
} // namespace

double directSoundEnergy(ArrayView<const double> rir, double sampleRateHz,
                         const DirectSoundResult &direct,
                         double halfWindowSeconds) {
    if (rir.empty() || !direct.found || sampleRateHz <= 0.0) return 0.0;
    std::size_t h = static_cast<std::size_t>(
        std::floor(halfWindowSeconds * sampleRateHz + 0.5));
    if (h < 1) h = 1;
    const std::size_t c = direct.sampleIndex;
    const std::size_t lo = (c >= h) ? c - h : 0;
    std::size_t hi = c + h + 1;
    if (hi > rir.size()) hi = rir.size();
    double e = 0.0;
    for (std::size_t i = lo; i < hi; ++i) e += rir[i] * rir[i];
    return e;
}

std::vector<ReflectionEvent> detectReflections(
    ArrayView<const double> rir, double sampleRateHz,
    const DirectSoundResult &direct, const ReflectionDetectorConfig &config) {
    std::vector<ReflectionEvent> events;
    const std::size_t n = rir.size();
    if (n == 0 || sampleRateHz <= 0.0 || !direct.found) return events;

    std::size_t w = static_cast<std::size_t>(
        std::floor(config.smoothingWindowSeconds * sampleRateHz + 0.5));
    if (w < 1) w = 1;
    if ((w % 2) == 0) ++w; // 中心づけのため奇数に
    const std::size_t h = w / 2;

    const std::vector<double> env = movingRmsEnvelope(rir, w);

    // 直接音の包絡線レベル (直接音位置の近傍最大)
    double envDirect = 0.0;
    {
        const std::size_t c = direct.sampleIndex;
        const std::size_t lo = (c >= h) ? c - h : 0;
        std::size_t hi = c + h + 1;
        if (hi > n) hi = n;
        for (std::size_t i = lo; i < hi; ++i)
            envDirect = std::max(envDirect, env[i]);
    }
    if (envDirect <= kTiny) return events;

    // ノイズフロア (末尾 10%)
    const NoiseFloorEstimate nf = estimateNoiseFloor(rir);
    const double noiseRms = nf.valid ? nf.noiseRms : 0.0;

    // 検出しきい値: 直接音比の下限とノイズフロア突出の大きい方
    const double thrRel =
        envDirect * std::pow(10.0, config.minRelativeLevelDb / 20.0);
    const double thrNoise =
        noiseRms * std::pow(10.0, config.noiseFloorMarginDb / 20.0);
    const double threshold = std::max(thrRel, thrNoise);

    std::size_t start = direct.sampleIndex +
                        static_cast<std::size_t>(std::floor(
                            config.detectionStartSeconds * sampleRateHz + 0.5));
    if (start < 1) start = 1;

    // 局所最大 (立ち上がりで等値プラトー先頭を採る)
    std::vector<Candidate> cands;
    for (std::size_t i = start; i + 1 < n; ++i) {
        if (env[i] >= threshold && env[i] > env[i - 1] && env[i] >= env[i + 1]) {
            Candidate c;
            c.index = i;
            c.envValue = env[i];
            cands.push_back(c);
        }
    }
    if (cands.empty()) return events;

    // 強い順に採用し、最小間隔と直接音との間隔を満たすものだけ残す
    std::size_t sep = static_cast<std::size_t>(
        std::floor(config.minSeparationSeconds * sampleRateHz + 0.5));
    if (sep < 1) sep = 1;
    std::sort(cands.begin(), cands.end(), byEnvDesc);
    std::vector<Candidate> accepted;
    for (std::size_t i = 0; i < cands.size(); ++i) {
        if (accepted.size() >= config.maxReflections) break;
        const std::size_t idx = cands[i].index;
        const std::size_t dDir = (idx > direct.sampleIndex)
                                     ? idx - direct.sampleIndex
                                     : direct.sampleIndex - idx;
        if (dDir < sep) continue; // 直接音自身の裾
        bool ok = true;
        for (std::size_t k = 0; k < accepted.size(); ++k) {
            const std::size_t d = (idx > accepted[k].index)
                                      ? idx - accepted[k].index
                                      : accepted[k].index - idx;
            if (d < sep) {
                ok = false;
                break;
            }
        }
        if (ok) accepted.push_back(cands[i]);
    }
    std::sort(accepted.begin(), accepted.end(), byIndexAsc);

    // イベント生成 (到達時刻は近傍の x² 最大点に精密化)
    const double noiseDb =
        20.0 * std::log10(std::max(noiseRms, kTiny));
    for (std::size_t k = 0; k < accepted.size(); ++k) {
        const std::size_t c = accepted[k].index;
        const std::size_t lo = (c >= h) ? c - h : 0;
        std::size_t hi = c + h + 1;
        if (hi > n) hi = n;
        std::size_t best = c;
        double bestVal = -1.0;
        double e = 0.0;
        for (std::size_t i = lo; i < hi; ++i) {
            const double p2 = rir[i] * rir[i];
            e += p2;
            if (p2 > bestVal) {
                bestVal = p2;
                best = i;
            }
        }
        ReflectionEvent ev;
        ev.arrivalTime = static_cast<double>(best) / sampleRateHz;
        ev.delayFromDirect =
            (static_cast<double>(best) -
             static_cast<double>(direct.sampleIndex)) / sampleRateHz;
        ev.relativeLevelDb =
            20.0 * std::log10(std::max(accepted[k].envValue, kTiny) / envDirect);
        ev.energy = e;
        if (noiseRms <= kTiny) {
            ev.confidence = 1.0; // ノイズが実質ゼロなら確実
        } else {
            const double envDb =
                20.0 * std::log10(std::max(accepted[k].envValue, kTiny));
            ev.confidence = clamp01(
                (envDb - noiseDb - config.noiseFloorMarginDb) / 30.0);
        }
        ev.bandIndex = config.bandIndex;
        events.push_back(ev);
    }
    return events;
}

ReflectionTimeSummary summarizeReflections(
    const std::vector<ReflectionEvent> &events, double directEnergy) {
    ReflectionTimeSummary s;
    s.directEnergy = directEnergy;
    for (std::size_t i = 0; i < events.size(); ++i) {
        const double ms = events[i].delayFromDirect * 1000.0;
        int bin;
        if (ms < 20.0) bin = 0;        // 初期反射 (0-20 ms)
        else if (ms < 80.0) bin = 1;   // 早期反射 (20-80 ms)
        else if (ms < 200.0) bin = 2;  // 後期反射 (80-200 ms)
        else bin = 3;                  // 残響部 (200 ms 以降)
        s.counts[bin] += 1;
        s.energies[bin] += events[i].energy;
    }
    if (directEnergy > 0.0) {
        for (int b = 0; b < ReflectionTimeSummary::kBinCount; ++b)
            s.energyRatios[b] = s.energies[b] / directEnergy;
    }
    return s;
}

} // namespace acoustics
} // namespace ofd
