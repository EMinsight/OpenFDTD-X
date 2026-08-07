// test_inr.cpp — 帯域内 INR (impulse-to-noise ratio) と ISO 3382-2 の
// 動的範囲要求の検証。
//
// 検証の芯は「INR の判定と、減衰指標が実際に評価できたかが食い違わない」こと。
// INR が要求値を下回る帯域で T30 が valid になっていたら、どちらかが嘘である。
//
// 期待値の出所:
//   - 要求値 20/35/45 dB は ISO 3382-2:2008 §5.3 / Table 1 (評価区間
//     0〜-10 / -5〜-25 / -5〜-35 dB に 10 dB のマージンを足したもの)。
//   - 合成 RIR のノイズフロアはテスト側が指定した値そのもの
//     (makeSyntheticRir の noiseFloorDb)。ピークは直接音の振幅 1.0 = 0 dBFS
//     なので、INR ≈ |noiseFloorDb| になるはず。
#include <cmath>
#include <cstdio>
#include <vector>

#include "../../src/acoustics/core/AcousticMetrics.h"
#include "../../src/acoustics/core/RirAnalyzer.h"
#include "test_common.h"

using namespace ofd::acoustics;

namespace {

// ── ISO 3382-2 の要求値 ────────────────────────────────────────────────────
void testRequirements() {
    std::printf("-- ISO 3382-2 dynamic range requirements\n");
    CHECK_NEAR(requiredInrDb(DecayMetricKind::EDT), 20.0, 1e-12);
    CHECK_NEAR(requiredInrDb(DecayMetricKind::T20), 35.0, 1e-12);
    CHECK_NEAR(requiredInrDb(DecayMetricKind::T30), 45.0, 1e-12);

    // 境界はちょうどで足りている扱い (>=)
    CHECK(inrSufficient(45.0, DecayMetricKind::T30));
    CHECK(!inrSufficient(44.9, DecayMetricKind::T30));
    CHECK(inrSufficient(35.0, DecayMetricKind::T20));
    CHECK(!inrSufficient(34.9, DecayMetricKind::T20));
    CHECK(inrSufficient(20.0, DecayMetricKind::EDT));
    CHECK(!inrSufficient(19.9, DecayMetricKind::EDT));

    // 要求は EDT < T20 < T30 の順に厳しい
    CHECK(requiredInrDb(DecayMetricKind::EDT) <
          requiredInrDb(DecayMetricKind::T20));
    CHECK(requiredInrDb(DecayMetricKind::T20) <
          requiredInrDb(DecayMetricKind::T30));
}

RirAnalysisResult analyze(double noiseDb, unsigned seed) {
    testutil::SyntheticRirSpec spec;
    spec.rt60 = 1.2;
    spec.noiseFloorDb = noiseDb;
    spec.seed = seed;
    const std::vector<double> h = testutil::makeSyntheticRir(spec);
    RirAnalyzerConfig cfg;
    cfg.minDynamicRangeDb = 0.0;   // 低 SNR ケースもエラーにせず解析させる
    RirAnalyzer an(cfg);
    const AcousticResult<RirAnalysisResult> r =
        an.analyze(ArrayView<const double>(h.data(), h.size()),
                   spec.sampleRateHz);
    CHECK(r.success());
    return r.success() ? r.value() : RirAnalysisResult();
}

// ── 帯域 INR が指定したノイズフロアを反映すること ──────────────────────────
void testInrTracksNoiseFloor() {
    std::printf("-- per-band INR follows the injected noise floor\n");
    const RirAnalysisResult quiet = analyze(-70.0, 11u);
    const RirAnalysisResult noisy = analyze(-30.0, 11u);
    CHECK(!quiet.bands.empty() && quiet.bands.size() == noisy.bands.size());

    int compared = 0;
    for (std::size_t b = 0; b < quiet.bands.size(); ++b) {
        const BandMetricsResult &q = quiet.bands[b];
        const BandMetricsResult &n = noisy.bands[b];
        if (!q.noiseOk || !n.noiseOk) continue;
        // 同じ減衰にノイズだけ 40 dB 積んだので、INR は必ず下がる
        CHECK(n.inrDb < q.inrDb);
        // INR は定義どおり peak − noiseFloor
        CHECK_NEAR(q.inrDb, q.peakDb - q.noiseFloorDb, 1e-9);
        CHECK_NEAR(n.inrDb, n.peakDb - n.noiseFloorDb, 1e-9);
        ++compared;
        std::printf("   %-5s  quiet INR = %6.1f dB   noisy INR = %6.1f dB\n",
                    q.band.label.c_str(), q.inrDb, n.inrDb);
    }
    CHECK(compared > 0);
}

// ── INR の判定と指標の valid が食い違わないこと (本題) ─────────────────────
// INR が要求を下回る帯域で減衰時間が valid になっていたら、表示している
// 「評価できない理由」が嘘になる。
void testVerdictAgreesWithMetrics() {
    std::printf("-- INR verdict agrees with metric validity\n");
    const double noise[3] = { -70.0, -45.0, -28.0 };
    for (int i = 0; i < 3; ++i) {
        const RirAnalysisResult r = analyze(noise[i], 20260807u + unsigned(i));
        for (std::size_t b = 0; b < r.bands.size(); ++b) {
            const BandMetricsResult &bm = r.bands[b];
            if (!bm.filterOk || !bm.noiseOk) continue;
            // 十分な INR があるのに評価できない、は起こってよい (回帰の質など)。
            // 逆 — INR が足りないのに valid — は矛盾なので許さない。
            if (bm.metrics.t30.valid)
                CHECK(inrSufficient(bm.inrDb, DecayMetricKind::T30));
            if (bm.metrics.t20.valid)
                CHECK(inrSufficient(bm.inrDb, DecayMetricKind::T20));
            if (bm.metrics.edt.valid)
                CHECK(inrSufficient(bm.inrDb, DecayMetricKind::EDT));
        }
        std::printf("   noise floor %+.0f dB: checked %zu bands\n", noise[i],
                    r.bands.size());
    }
}

// ── 極端に低い SNR では T30 がどの帯域でも評価できないこと ─────────────────
void testLowSnrKillsT30() {
    std::printf("-- a poor SNR leaves no valid T30\n");
    const RirAnalysisResult r = analyze(-25.0, 7u);
    int validT30 = 0, bandsWithInr = 0;
    double worst = 1e9;
    for (std::size_t b = 0; b < r.bands.size(); ++b) {
        const BandMetricsResult &bm = r.bands[b];
        if (bm.noiseOk) { ++bandsWithInr; if (bm.inrDb < worst) worst = bm.inrDb; }
        if (bm.metrics.t30.valid) ++validT30;
    }
    CHECK(bandsWithInr > 0);
    CHECK(worst < requiredInrDb(DecayMetricKind::T30));
    CHECK(validT30 == 0);
    std::printf("   lowest band INR = %.1f dB, valid T30 count = %d\n", worst,
                validT30);
}

// ── 帯域 INR は広帯域の動的範囲と別物であること ────────────────────────────
// 低域は帯域幅が狭くノイズも減衰も減るため、広帯域値と一致しない。
// 「広帯域で足りているから全帯域大丈夫」とは言えないのがこの機能の理由。
void testBandInrDiffersFromBroadband() {
    std::printf("-- band INR is not the broadband dynamic range\n");
    const RirAnalysisResult r = analyze(-50.0, 3u);
    bool anyDifferent = false;
    for (std::size_t b = 0; b < r.bands.size(); ++b) {
        const BandMetricsResult &bm = r.bands[b];
        if (!bm.noiseOk) continue;
        if (std::fabs(bm.inrDb - r.preprocess.dynamicRangeDb) > 1.0)
            anyDifferent = true;
    }
    CHECK(anyDifferent);
    std::printf("   broadband dynamic range = %.1f dB\n",
                r.preprocess.dynamicRangeDb);
}

} // namespace

int main() {
    std::printf("== INR / ISO 3382-2 dynamic range ==\n");
    testRequirements();
    testInrTracksNoiseFloor();
    testVerdictAgreesWithMetrics();
    testLowSnrKillsT30();
    testBandInrDiffersFromBroadband();
    return testutil::summary("inr");
}
