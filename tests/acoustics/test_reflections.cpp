// test_reflections.cpp — 既知の遅延・レベルの反射検出と時間区分集計の検証。
#include <cstdio>
#include <vector>

#include "../../src/acoustics/core/DirectSoundDetector.h"
#include "../../src/acoustics/core/ReflectionDetector.h"
#include "test_common.h"

using namespace ofd::acoustics;

int main() {
    const double fs = 48000.0;

    // ── 1. デルタ列: 直接音 + 4 反射 (各時間区分に 1 つずつ) ──
    // 遅延 [ms] とレベル [dB]: 10/-6, 50/-12, 120/-20, 250/-30
    const double delaysMs[4] = {10.0, 50.0, 120.0, 250.0};
    const double levelsDb[4] = {-6.0, -12.0, -20.0, -30.0};
    {
        std::vector<double> h(static_cast<std::size_t>(0.4 * fs + 0.5), 0.0);
        const std::size_t d0 = static_cast<std::size_t>(0.020 * fs + 0.5);
        h[d0] = 1.0;
        for (int k = 0; k < 4; ++k) {
            const std::size_t idx =
                d0 + static_cast<std::size_t>(delaysMs[k] * 0.001 * fs + 0.5);
            h[idx] = std::pow(10.0, levelsDb[k] / 20.0);
        }
        ArrayView<const double> hv(h.data(), h.size());

        DirectSoundResult d = detectDirectSound(hv, fs);
        CHECK(d.found);
        CHECK(d.sampleIndex == d0);

        ReflectionDetectorConfig cfg;
        cfg.minSeparationSeconds = 0.005;
        cfg.minRelativeLevelDb = -40.0;
        cfg.detectionStartSeconds = 0.001;
        std::vector<ReflectionEvent> ev = detectReflections(hv, fs, d, cfg);

        CHECK(ev.size() == 4); // 偽検出なし・すべて検出
        for (std::size_t k = 0; k < ev.size() && k < 4; ++k) {
            CHECK_NEAR(ev[k].delayFromDirect * 1000.0, delaysMs[k], 1.0);
            CHECK_NEAR(ev[k].relativeLevelDb, levelsDb[k], 1.0);
            CHECK_NEAR(ev[k].arrivalTime,
                       0.020 + delaysMs[k] * 0.001, 0.001);
            CHECK(ev[k].energy > 0.0);
            CHECK(ev[k].confidence > 0.5); // ノイズなし → 高信頼
            CHECK(ev[k].bandIndex == -1);
        }

        // ── 時間区分集計 [0,20)/[20,80)/[80,200)/[200,∞) ms ──
        const double de = directSoundEnergy(hv, fs, d);
        CHECK(de > 0.9 && de < 1.1); // ≈ 1.0
        ReflectionTimeSummary sum = summarizeReflections(ev, de);
        CHECK(sum.counts[0] == 1);
        CHECK(sum.counts[1] == 1);
        CHECK(sum.counts[2] == 1);
        CHECK(sum.counts[3] == 1);
        // 各区分のエネルギー比 ≈ 10^(level/10)
        for (int b = 0; b < 4; ++b) {
            const double expect = std::pow(10.0, levelsDb[b] / 10.0);
            CHECK_REL(sum.energyRatios[b], expect, 0.2);
        }
        std::printf("  4 reflections detected, delays: %.2f %.2f %.2f %.2f ms\n",
                    ev.size() > 0 ? ev[0].delayFromDirect * 1e3 : -1.0,
                    ev.size() > 1 ? ev[1].delayFromDirect * 1e3 : -1.0,
                    ev.size() > 2 ? ev[2].delayFromDirect * 1e3 : -1.0,
                    ev.size() > 3 ? ev[3].delayFromDirect * 1e3 : -1.0);
    }

    // ── 2. 最小相対レベルより弱い反射は検出しない ──
    {
        std::vector<double> h(static_cast<std::size_t>(0.2 * fs + 0.5), 0.0);
        const std::size_t d0 = static_cast<std::size_t>(0.020 * fs + 0.5);
        h[d0] = 1.0;
        h[d0 + static_cast<std::size_t>(0.030 * fs + 0.5)] = 0.1;   // -20 dB
        h[d0 + static_cast<std::size_t>(0.060 * fs + 0.5)] = 0.003; // -50 dB
        ArrayView<const double> hv(h.data(), h.size());
        DirectSoundResult d = detectDirectSound(hv, fs);
        ReflectionDetectorConfig cfg;
        cfg.minRelativeLevelDb = -40.0;
        std::vector<ReflectionEvent> ev = detectReflections(hv, fs, d, cfg);
        CHECK(ev.size() == 1);
        if (ev.size() == 1)
            CHECK_NEAR(ev[0].delayFromDirect * 1000.0, 30.0, 1.0);
    }

    // ── 3. maxReflections で強い順に制限される ──
    {
        std::vector<double> h(static_cast<std::size_t>(0.3 * fs + 0.5), 0.0);
        const std::size_t d0 = static_cast<std::size_t>(0.020 * fs + 0.5);
        h[d0] = 1.0;
        const double dl[3] = {0.030, 0.070, 0.110};
        const double lv[3] = {-18.0, -6.0, -12.0};
        for (int k = 0; k < 3; ++k)
            h[d0 + static_cast<std::size_t>(dl[k] * fs + 0.5)] =
                std::pow(10.0, lv[k] / 20.0);
        ArrayView<const double> hv(h.data(), h.size());
        DirectSoundResult d = detectDirectSound(hv, fs);
        ReflectionDetectorConfig cfg;
        cfg.maxReflections = 2;
        std::vector<ReflectionEvent> ev = detectReflections(hv, fs, d, cfg);
        CHECK(ev.size() == 2);
        // 残るのは強い 2 つ (-6, -12) で時刻昇順
        if (ev.size() == 2) {
            CHECK_NEAR(ev[0].delayFromDirect, 0.070, 0.001);
            CHECK_NEAR(ev[1].delayFromDirect, 0.110, 0.001);
        }
    }

    // ── 4. ノイズフロア突出条件: ノイズに埋もれた反射は検出しない ──
    {
        testutil::SyntheticRirSpec spec;
        spec.rt60 = 0.5;
        spec.decayNoiseRms = 0.0;   // 減衰雑音なし
        spec.noiseFloorDb = -40.0;  // 一定ノイズ
        spec.directDelaySeconds = 0.020;
        spec.durationSeconds = 0.4;
        spec.reflections.push_back(std::make_pair(0.050, -12.0)); // 明確
        spec.reflections.push_back(std::make_pair(0.150, -55.0)); // 埋没
        std::vector<double> h = testutil::makeSyntheticRir(spec);
        ArrayView<const double> hv(h.data(), h.size());
        DirectSoundResult d = detectDirectSound(hv, fs);
        CHECK(d.found);
        ReflectionDetectorConfig cfg;
        cfg.minRelativeLevelDb = -60.0; // 相対レベルでは弾かない
        cfg.noiseFloorMarginDb = 8.0;
        std::vector<ReflectionEvent> ev = detectReflections(hv, fs, d, cfg);
        bool found50 = false, found150 = false;
        for (std::size_t k = 0; k < ev.size(); ++k) {
            if (std::fabs(ev[k].delayFromDirect - 0.050) < 0.002)
                found50 = true;
            if (std::fabs(ev[k].delayFromDirect - 0.150) < 0.002)
                found150 = true;
        }
        CHECK(found50);
        CHECK(!found150);
    }

    return testutil::summary("test_reflections");
}
