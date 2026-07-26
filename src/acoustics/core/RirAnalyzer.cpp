// RirAnalyzer.cpp — RIR 分析パイプラインの実装。
#include "RirAnalyzer.h"

#include <cmath>
#include <cstdio>

#include "NoiseFloorEstimator.h"

namespace ofd {
namespace acoustics {

RirAnalyzer::RirAnalyzer(const RirAnalyzerConfig &config) : m_config(config) {}

AcousticResult<RirAnalysisResult>
RirAnalyzer::analyze(ArrayView<const double> rir, double sampleRateHz) const {
    typedef AcousticResult<RirAnalysisResult> Result;
    if (sampleRateHz <= 0.0)
        return Result::error(AcousticErrorCode::UnsupportedSampleRate,
                             "sample rate must be positive");
    if (rir.empty())
        return Result::error(AcousticErrorCode::EmptyInput, "input is empty");

    RirAnalysisResult res;
    const std::size_t n = rir.size();
    res.preprocess.sampleCount = n;
    res.preprocess.durationSeconds = static_cast<double>(n) / sampleRateHz;

    // ── 入力長検査 ──
    if (res.preprocess.durationSeconds < m_config.minDurationSeconds) {
        char buf[128];
        std::snprintf(buf, sizeof(buf),
                      "input too short: %.4f s < %.4f s",
                      res.preprocess.durationSeconds,
                      m_config.minDurationSeconds);
        return Result::error(AcousticErrorCode::InputTooShort, buf);
    }

    // ── 非有限値検出 ──
    for (std::size_t i = 0; i < n; ++i) {
        if (!std::isfinite(rir[i])) ++res.preprocess.nonFiniteCount;
    }
    if (res.preprocess.nonFiniteCount > 0) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "input contains %u non-finite samples",
                      static_cast<unsigned>(res.preprocess.nonFiniteCount));
        return Result::error(AcousticErrorCode::NonFiniteSample, buf);
    }

    // ── クリッピング検出 (|x| > clipThreshold が clipRunLength 連続) ──
    {
        int run = 0;
        bool inRun = false;
        for (std::size_t i = 0; i < n; ++i) {
            if (std::fabs(rir[i]) > m_config.clipThreshold) {
                ++run;
                if (run >= m_config.clipRunLength && !inRun) {
                    res.preprocess.clippingDetected = true;
                    ++res.preprocess.clippedRunCount;
                    inRun = true;
                }
            } else {
                run = 0;
                inRun = false;
            }
        }
        if (res.preprocess.clippingDetected)
            res.warnings.push_back(
                "clipping detected: results may be unreliable");
    }

    // ── DC 除去 ──
    std::vector<double> x(rir.begin(), rir.end());
    if (m_config.removeDc) {
        double mean = 0.0;
        for (std::size_t i = 0; i < n; ++i) mean += x[i];
        mean /= static_cast<double>(n);
        for (std::size_t i = 0; i < n; ++i) x[i] -= mean;
        res.preprocess.dcOffset = mean;
        res.preprocess.dcRemoved = true;
    }
    ArrayView<const double> xv(x.data(), x.size());

    // ── 動的範囲検査 (末尾ノイズフロア vs ピーク) ──
    {
        const NoiseFloorEstimate nf = estimateNoiseFloor(xv);
        res.preprocess.noiseFloorDb = nf.noiseFloorDb;
        res.preprocess.peakDb = nf.peakDb;
        res.preprocess.dynamicRangeDb = nf.dynamicRangeDb;
        if (!nf.valid || nf.peakAbs <= 0.0)
            return Result::error(AcousticErrorCode::EmptyInput,
                                 "signal is silent");
        if (nf.dynamicRangeDb < m_config.minDynamicRangeDb) {
            char buf[128];
            std::snprintf(buf, sizeof(buf),
                          "dynamic range %.1f dB below minimum %.1f dB",
                          nf.dynamicRangeDb, m_config.minDynamicRangeDb);
            return Result::error(AcousticErrorCode::InsufficientDynamicRange,
                                 buf);
        }
        if (nf.dynamicRangeDb < m_config.warnDynamicRangeDb) {
            char buf[128];
            std::snprintf(buf, sizeof(buf),
                          "low dynamic range: %.1f dB (< %.1f dB)",
                          nf.dynamicRangeDb, m_config.warnDynamicRangeDb);
            res.warnings.push_back(buf);
        }
    }

    // ── 直接音検出 ──
    res.directSound = detectDirectSound(xv, sampleRateHz, m_config.directSound);
    if (!res.directSound.found)
        return Result::error(AcousticErrorCode::DirectSoundNotFound,
                             "direct sound not found: " +
                                 res.directSound.warning);
    if (!res.directSound.warning.empty())
        res.warnings.push_back("direct sound: " + res.directSound.warning);

    // ── 絶対 SPL (校正済みのみ有効。自動正規化はしない) ──
    {
        const double peakSpl =
            res.preprocess.peakDb + m_config.calibrationOffsetDb;
        if (m_config.calibration == CalibrationState::Absolute) {
            res.absoluteSplDb = makeValidMetric(peakSpl);
        } else {
            res.absoluteSplDb = makeInvalidMetric(
                "absolute SPL requires Absolute calibration state");
        }
    }

    // ── 帯域分割と帯域別指標 ──
    const std::vector<Band> bands = makeBands(m_config.bandSet);
    for (std::size_t b = 0; b < bands.size(); ++b) {
        BandMetricsResult bm;
        bm.band = bands[b];
        AcousticResult<std::vector<double>> fx = filterBand(
            xv, sampleRateHz, bands[b], m_config.zeroPhaseFiltering);
        if (!fx.success()) {
            bm.filterOk = false;
            bm.filterWarning = fx.message();
            res.warnings.push_back("band '" + bands[b].label +
                                   "' filter failed: " + fx.message());
        } else {
            bm.filterOk = true;
            ArrayView<const double> fv(fx.value().data(), fx.value().size());
            bm.metrics = computeAcousticMetrics(
                fv, sampleRateHz, res.directSound.sampleIndex,
                m_config.metrics);
        }
        res.bands.push_back(bm);
    }

    // ── 反射音検出 (広帯域) ──
    res.reflections =
        detectReflections(xv, sampleRateHz, res.directSound,
                          m_config.reflections);
    const double dirEnergy = directSoundEnergy(
        xv, sampleRateHz, res.directSound,
        m_config.reflections.smoothingWindowSeconds);
    res.reflectionSummary = summarizeReflections(res.reflections, dirEnergy);

    // ── 総合品質 ──
    bool anyValidMetric = false;
    bool anyWarnMetric = false;
    for (std::size_t b = 0; b < res.bands.size(); ++b) {
        const AcousticMetricsSet &m = res.bands[b].metrics;
        const MetricValue *vals[9] = {&m.edt, &m.t20, &m.t30,
                                      &m.c50, &m.c80, &m.d50,
                                      &m.ts,  &m.earlyLate50, &m.earlyLate80};
        for (int k = 0; k < 9; ++k) {
            if (vals[k]->valid) anyValidMetric = true;
            if (vals[k]->quality == AnalysisQuality::Warning)
                anyWarnMetric = true;
        }
    }
    if (!anyValidMetric) {
        res.overallQuality = AnalysisQuality::Invalid;
    } else if (anyWarnMetric || !res.warnings.empty() ||
               res.directSound.quality == AnalysisQuality::Warning) {
        res.overallQuality = AnalysisQuality::Warning;
    } else {
        res.overallQuality = AnalysisQuality::Valid;
    }

    return Result::ok(res);
}

AcousticResult<RirAnalysisResult>
RirAnalyzer::analyze(const AudioBuffer &buffer, std::size_t channel) const {
    typedef AcousticResult<RirAnalysisResult> Result;
    if (channel >= buffer.channelCount())
        return Result::error(AcousticErrorCode::InvalidArgument,
                             "channel index out of range");
    const std::vector<double> &ch = buffer.channels[channel];
    return analyze(ArrayView<const double>(ch.data(), ch.size()),
                   buffer.sampleRateHz);
}

} // namespace acoustics
} // namespace ofd
