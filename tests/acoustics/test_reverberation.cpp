// test_reverberation.cpp — 人工 RIR による EDT / T20 / T30 の検証。
// RT = 0.5 / 1.0 / 2.0 / 3.0 s、ノイズなし / 低ノイズ (-60 dB) /
// 高ノイズ (-35 dB) で ±5% 以内。動的範囲不足ケースでは valid = false。
#include <cstdio>
#include <limits>
#include <vector>

#include "../../src/acoustics/core/AcousticMetrics.h"
#include "../../src/acoustics/core/DirectSoundDetector.h"
#include "../../src/acoustics/core/RirAnalyzer.h"
#include "test_common.h"

using namespace ofd::acoustics;

namespace {

// 人工 RIR を生成し、直接音検出 → 指標計算まで行う
AcousticMetricsSet analyzeSynthetic(double rt, double noiseDb, unsigned seed) {
    testutil::SyntheticRirSpec spec;
    spec.rt60 = rt;
    spec.noiseFloorDb = noiseDb;
    spec.seed = seed;
    std::vector<double> h = testutil::makeSyntheticRir(spec);
    ArrayView<const double> hv(h.data(), h.size());
    DirectSoundResult d = detectDirectSound(hv, spec.sampleRateHz);
    CHECK(d.found);
    return computeAcousticMetrics(hv, spec.sampleRateHz, d.sampleIndex);
}

void expectRt(const MetricValue &m, double rt, const char *name) {
    CHECK(m.valid);
    if (!m.valid) {
        std::printf("  %s invalid: %s\n", name, m.warning.c_str());
        return;
    }
    CHECK_REL(m.value, rt, 0.05); // ±5%
}

void testAccuracy(double rt, double noiseDb, unsigned seed) {
    AcousticMetricsSet m = analyzeSynthetic(rt, noiseDb, seed);
    expectRt(m.edt, rt, "EDT");
    expectRt(m.t20, rt, "T20");
    expectRt(m.t30, rt, "T30");
    std::printf("  RT=%.2f noise=%.0f dB: EDT=%.4f (%+.2f%%)  "
                "T20=%.4f (%+.2f%%)  T30=%.4f (%+.2f%%)\n",
                rt, noiseDb, m.edt.value, 100.0 * (m.edt.value - rt) / rt,
                m.t20.value, 100.0 * (m.t20.value - rt) / rt, m.t30.value,
                100.0 * (m.t30.value - rt) / rt);
}

} // namespace

int main() {
    const double rts[4] = {0.5, 1.0, 2.0, 3.0};

    // ── ノイズなし ──
    std::printf("== noiseless ==\n");
    for (int i = 0; i < 4; ++i) testAccuracy(rts[i], -1000.0, 11u + i);

    // ── 低ノイズ (-60 dBFS) ──
    std::printf("== noise floor -60 dB ==\n");
    for (int i = 0; i < 4; ++i) testAccuracy(rts[i], -60.0, 22u + i);

    // ── 高ノイズ (-35 dBFS): 動的範囲不足 → T20/T30 は valid=false ──
    std::printf("== noise floor -35 dB (insufficient range) ==\n");
    {
        AcousticMetricsSet m = analyzeSynthetic(2.0, -35.0, 33u);
        CHECK(!m.t30.valid);
        CHECK(!m.t30.warning.empty());
        CHECK(m.t30.quality == AnalysisQuality::Invalid);
        CHECK(!m.t20.valid);
        CHECK(!m.t20.warning.empty());
        std::printf("  T30 warning: %s\n", m.t30.warning.c_str());
    }

    // ── RirAnalyzer 統合パイプライン (全帯域) ──
    std::printf("== RirAnalyzer full-band ==\n");
    {
        testutil::SyntheticRirSpec spec;
        spec.rt60 = 1.0;
        spec.noiseFloorDb = -60.0;
        std::vector<double> h = testutil::makeSyntheticRir(spec);

        RirAnalyzerConfig cfg;
        cfg.bandSet = BandSet::FullBandOnly;
        RirAnalyzer analyzer(cfg);
        AcousticResult<RirAnalysisResult> r = analyzer.analyze(
            ArrayView<const double>(h.data(), h.size()), spec.sampleRateHz);
        CHECK(r.success());
        if (r.success()) {
            const RirAnalysisResult &res = r.value();
            CHECK(res.directSound.found);
            CHECK(res.preprocess.nonFiniteCount == 0);
            CHECK(!res.preprocess.clippingDetected);
            CHECK(res.preprocess.dynamicRangeDb > 40.0);
            CHECK(res.bands.size() == 1);
            if (res.bands.size() == 1) {
                expectRt(res.bands[0].metrics.t30, spec.rt60, "T30(full)");
                expectRt(res.bands[0].metrics.t20, spec.rt60, "T20(full)");
            }
            CHECK(res.overallQuality != AnalysisQuality::Invalid);
            // 未校正なので絶対 SPL は valid にならない
            CHECK(!res.absoluteSplDb.valid);
        }
    }

    // ── RirAnalyzer 帯域分割 (既存互換 6 帯域, ゼロ位相) ──
    std::printf("== RirAnalyzer Compat6 bands ==\n");
    {
        testutil::SyntheticRirSpec spec;
        spec.rt60 = 1.0;
        std::vector<double> h = testutil::makeSyntheticRir(spec);

        RirAnalyzerConfig cfg;
        cfg.bandSet = BandSet::Compat6;
        cfg.zeroPhaseFiltering = true;
        RirAnalyzer analyzer(cfg);
        AcousticResult<RirAnalysisResult> r = analyzer.analyze(
            ArrayView<const double>(h.data(), h.size()), spec.sampleRateHz);
        CHECK(r.success());
        if (r.success()) {
            const RirAnalysisResult &res = r.value();
            CHECK(res.bands.size() == 6);
            // 白色雑音×指数包絡なので各帯域とも同じ RT になるはず。
            // 帯域分割後は統計ゆらぎが増えるため 1 kHz / 2 kHz 帯で確認。
            for (std::size_t b = 3; b <= 4 && b < res.bands.size(); ++b) {
                CHECK(res.bands[b].filterOk);
                const MetricValue &t30 = res.bands[b].metrics.t30;
                CHECK(t30.valid);
                if (t30.valid) {
                    CHECK_REL(t30.value, spec.rt60, 0.05);
                    std::printf("  band %s: T30=%.4f (%+.2f%%)\n",
                                res.bands[b].band.label.c_str(), t30.value,
                                100.0 * (t30.value - spec.rt60) / spec.rt60);
                }
            }
        }
    }

    // ── 校正状態: Absolute のときのみ絶対 SPL が valid ──
    {
        testutil::SyntheticRirSpec spec;
        spec.rt60 = 0.5;
        std::vector<double> h = testutil::makeSyntheticRir(spec);
        RirAnalyzerConfig cfg;
        cfg.bandSet = BandSet::FullBandOnly;
        cfg.calibration = CalibrationState::Absolute;
        cfg.calibrationOffsetDb = 94.0;
        RirAnalyzer analyzer(cfg);
        AcousticResult<RirAnalysisResult> r = analyzer.analyze(
            ArrayView<const double>(h.data(), h.size()), spec.sampleRateHz);
        CHECK(r.success());
        if (r.success()) CHECK(r.value().absoluteSplDb.valid);
    }

    // ── エラー系: 短すぎる入力 / 非有限値 ──
    {
        std::vector<double> shortSig(100, 0.0);
        shortSig[10] = 1.0;
        RirAnalyzer analyzer;
        AcousticResult<RirAnalysisResult> r = analyzer.analyze(
            ArrayView<const double>(shortSig.data(), shortSig.size()),
            48000.0);
        CHECK(!r.success());
        CHECK(r.errorCode() == AcousticErrorCode::InputTooShort);
    }
    {
        testutil::SyntheticRirSpec spec;
        spec.rt60 = 0.5;
        std::vector<double> h = testutil::makeSyntheticRir(spec);
        h[1000] = std::numeric_limits<double>::quiet_NaN();
        RirAnalyzer analyzer;
        AcousticResult<RirAnalysisResult> r = analyzer.analyze(
            ArrayView<const double>(h.data(), h.size()), spec.sampleRateHz);
        CHECK(!r.success());
        CHECK(r.errorCode() == AcousticErrorCode::NonFiniteSample);
    }

    return testutil::summary("test_reverberation");
}
