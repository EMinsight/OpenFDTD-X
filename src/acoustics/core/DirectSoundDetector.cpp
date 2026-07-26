// DirectSoundDetector.cpp — 直接音検出の実装。
#include "DirectSoundDetector.h"

#include <cmath>
#include <cstdio>

namespace ofd {
namespace acoustics {

namespace {
const double kMinAmplitude = 1e-15;

double amplitudeDb(double a) {
    a = std::fabs(a);
    if (a < kMinAmplitude) a = kMinAmplitude;
    return 20.0 * std::log10(a);
}

// 絶対値最大のサンプル位置
std::size_t argmaxAbs(ArrayView<const double> x) {
    std::size_t idx = 0;
    double best = -1.0;
    for (std::size_t i = 0; i < x.size(); ++i) {
        const double a = std::fabs(x[i]);
        if (a > best) {
            best = a;
            idx = i;
        }
    }
    return idx;
}

DirectSoundResult makeResult(ArrayView<const double> x, double fs,
                             std::size_t index) {
    DirectSoundResult r;
    r.found = true;
    r.sampleIndex = index;
    r.timeSeconds = static_cast<double>(index) / fs;
    r.amplitude = x[index];
    r.levelDb = amplitudeDb(x[index]);
    r.quality = AnalysisQuality::Valid;
    return r;
}
} // namespace

DirectSoundResult detectDirectSound(ArrayView<const double> x,
                                    double sampleRateHz,
                                    const DirectSoundConfig &config) {
    DirectSoundResult bad;
    if (x.empty()) {
        bad.warning = "input is empty";
        return bad;
    }
    if (sampleRateHz <= 0.0) {
        bad.warning = "invalid sample rate";
        return bad;
    }

    const std::size_t peakIdx = argmaxAbs(x);
    const double peakAbs = std::fabs(x[peakIdx]);
    if (peakAbs <= 0.0) {
        bad.warning = "signal is silent";
        return bad;
    }

    switch (config.method) {
    case DirectSoundMethod::Peak: {
        // 最大ピークより minLead 以上先行し、最大の thresholdDb 以内にある
        // 局所ピークがあれば、その最初のものを直接音とみなす。
        const double thr =
            peakAbs * std::pow(10.0, config.precedingPeakThresholdDb / 20.0);
        const std::size_t minLead = static_cast<std::size_t>(
            std::floor(config.precedingPeakMinLeadSeconds * sampleRateHz + 0.5));
        std::size_t chosen = peakIdx;
        if (peakIdx > minLead && minLead > 0) {
            const std::size_t limit = peakIdx - minLead;
            for (std::size_t i = 1; i + 1 < x.size() && i <= limit; ++i) {
                const double a = std::fabs(x[i]);
                if (a >= thr && a >= std::fabs(x[i - 1]) &&
                    a >= std::fabs(x[i + 1])) {
                    chosen = i;
                    break;
                }
            }
            // 端 (i=0) が強い場合も先行到来として扱う
            if (chosen == peakIdx && std::fabs(x[0]) >= thr &&
                std::fabs(x[0]) >= std::fabs(x[1 < x.size() ? 1 : 0])) {
                chosen = 0;
            }
        }
        DirectSoundResult r = makeResult(x, sampleRateHz, chosen);
        if (chosen != peakIdx) {
            char buf[160];
            std::snprintf(buf, sizeof(buf),
                          "strong arrival %.2f ms before the largest peak was "
                          "adopted as direct sound",
                          (static_cast<double>(peakIdx) -
                           static_cast<double>(chosen)) *
                              1000.0 / sampleRateHz);
            r.warning = buf;
            r.quality = AnalysisQuality::Warning;
        }
        return r;
    }

    case DirectSoundMethod::EnvelopeThreshold: {
        // |x| が最大値の thresholdDb 以内に最初に達した点を直接音とする
        const double thr = peakAbs * std::pow(10.0, config.thresholdDb / 20.0);
        for (std::size_t i = 0; i < x.size(); ++i) {
            if (std::fabs(x[i]) >= thr)
                return makeResult(x, sampleRateHz, i);
        }
        break; // 到達しない
    }

    case DirectSoundMethod::MovingRmsThreshold: {
        std::size_t w = static_cast<std::size_t>(
            std::floor(config.movingRmsWindowSeconds * sampleRateHz + 0.5));
        if (w < 1) w = 1;
        if (w > x.size()) w = x.size();
        // 移動二乗和 (窓 [i, i+w)) の前進走査
        double sum = 0.0;
        for (std::size_t i = 0; i < w; ++i) sum += x[i] * x[i];
        double maxSum = sum;
        {
            double s = sum;
            for (std::size_t i = w; i < x.size(); ++i) {
                s += x[i] * x[i] - x[i - w] * x[i - w];
                if (s > maxSum) maxSum = s;
            }
        }
        const double ratio = std::pow(10.0, config.thresholdDb / 10.0); // パワー比
        const double thr = maxSum * ratio;
        double s = sum;
        std::size_t hit = 0;
        bool hitFound = false;
        if (s >= thr) {
            hit = 0;
            hitFound = true;
        }
        for (std::size_t i = w; i < x.size() && !hitFound; ++i) {
            s += x[i] * x[i] - x[i - w] * x[i - w];
            if (s >= thr) {
                hit = i - w + 1; // 窓の先頭
                hitFound = true;
            }
        }
        if (hitFound) {
            // 窓内の |x| 最大点に精密化する
            ArrayView<const double> win = x.subview(hit, w);
            const std::size_t refined = hit + argmaxAbs(win);
            return makeResult(x, sampleRateHz, refined);
        }
        break; // 到達しない
    }
    }

    bad.warning = "direct sound not found";
    return bad;
}

} // namespace acoustics
} // namespace ofd
