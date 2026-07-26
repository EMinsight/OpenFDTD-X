// test_direct_sound.cpp — 直接音検出の検証。
// デルタ位置の検出、強い先行反射がある場合の先行側採用と警告、
// 弱い先行成分では警告が出ないことを確認する。
#include <cstdio>
#include <vector>

#include "../../src/acoustics/core/DirectSoundDetector.h"
#include "test_common.h"

using namespace ofd::acoustics;

int main() {
    const double fs = 48000.0;

    // ── 1. 単一デルタの位置検出 (Peak) ──
    {
        std::vector<double> x(4800, 0.0); // 100 ms
        const std::size_t pos = 4800 / 2; // 50 ms
        x[pos] = 0.8;
        DirectSoundResult r = detectDirectSound(
            ArrayView<const double>(x.data(), x.size()), fs);
        CHECK(r.found);
        CHECK(r.sampleIndex == pos);
        CHECK_NEAR(r.timeSeconds, 0.050, 1e-9);
        CHECK_NEAR(r.amplitude, 0.8, 1e-12);
        CHECK(r.quality == AnalysisQuality::Valid);
        CHECK(r.warning.empty());
    }

    // ── 2. 最大ピークに先行する強いピーク → 先行側を採用し警告 ──
    // 直接音 0.4 (50 ms)、後続の強い集中反射 1.0 (120 ms)。
    // 0.4 は最大の -7.96 dB (≥ -20 dB) かつ 70 ms 先行 (≥ 1 ms)。
    {
        std::vector<double> x(9600, 0.0); // 200 ms
        const std::size_t direct = static_cast<std::size_t>(0.050 * fs + 0.5);
        const std::size_t big = static_cast<std::size_t>(0.120 * fs + 0.5);
        x[direct] = 0.4;
        x[big] = 1.0;
        DirectSoundResult r = detectDirectSound(
            ArrayView<const double>(x.data(), x.size()), fs);
        CHECK(r.found);
        CHECK(r.sampleIndex == direct);
        CHECK(!r.warning.empty());
        CHECK(r.quality == AnalysisQuality::Warning);
    }

    // ── 3. 弱い先行成分 (-26 dB) は採用せず、最大ピークが直接音 ──
    {
        std::vector<double> x(9600, 0.0);
        const std::size_t weak = static_cast<std::size_t>(0.090 * fs + 0.5);
        const std::size_t peak = static_cast<std::size_t>(0.100 * fs + 0.5);
        x[weak] = 0.05; // -26 dB rel 1.0
        x[peak] = 1.0;
        DirectSoundResult r = detectDirectSound(
            ArrayView<const double>(x.data(), x.size()), fs);
        CHECK(r.found);
        CHECK(r.sampleIndex == peak);
        CHECK(r.warning.empty());
        CHECK(r.quality == AnalysisQuality::Valid);
    }

    // ── 4. 1 ms 未満しか先行しない強いピークは採用しない ──
    {
        std::vector<double> x(9600, 0.0);
        const std::size_t peak = static_cast<std::size_t>(0.100 * fs + 0.5);
        const std::size_t close = peak - 24; // 0.5 ms 先行
        x[close] = 0.5;
        x[peak] = 1.0;
        DirectSoundResult r = detectDirectSound(
            ArrayView<const double>(x.data(), x.size()), fs);
        CHECK(r.found);
        CHECK(r.sampleIndex == peak);
        CHECK(r.warning.empty());
    }

    // ── 5. EnvelopeThreshold: 立ち上がり点の検出 ──
    {
        std::vector<double> x(9600, 0.0);
        const std::size_t onset = static_cast<std::size_t>(0.080 * fs + 0.5);
        x[onset] = 0.2;      // 最大 1.0 の -14 dB (> -20 dB) → ここで交差
        x[onset + 48] = 1.0; // 最大ピークは 1 ms 後
        DirectSoundConfig cfg;
        cfg.method = DirectSoundMethod::EnvelopeThreshold;
        DirectSoundResult r = detectDirectSound(
            ArrayView<const double>(x.data(), x.size()), fs, cfg);
        CHECK(r.found);
        CHECK(r.sampleIndex == onset);
    }

    // ── 6. MovingRmsThreshold: デルタ位置の検出 (窓内精密化) ──
    {
        std::vector<double> x(9600, 0.0);
        const std::size_t pos = static_cast<std::size_t>(0.075 * fs + 0.5);
        x[pos] = 1.0;
        DirectSoundConfig cfg;
        cfg.method = DirectSoundMethod::MovingRmsThreshold;
        DirectSoundResult r = detectDirectSound(
            ArrayView<const double>(x.data(), x.size()), fs, cfg);
        CHECK(r.found);
        CHECK(r.sampleIndex == pos);
    }

    // ── 7. エラー系: 空入力 / 無音 ──
    {
        DirectSoundResult r =
            detectDirectSound(ArrayView<const double>(), fs);
        CHECK(!r.found);
    }
    {
        std::vector<double> x(1000, 0.0);
        DirectSoundResult r = detectDirectSound(
            ArrayView<const double>(x.data(), x.size()), fs);
        CHECK(!r.found);
    }

    // ── 8. 人工 RIR (デルタ + 減衰雑音) でもデルタ位置が検出できる ──
    {
        testutil::SyntheticRirSpec spec;
        spec.rt60 = 1.0;
        spec.decayNoiseRms = 0.3; // 一様乱数の振幅上限 0.52 < 1.0
        std::vector<double> h = testutil::makeSyntheticRir(spec);
        const std::size_t expect = static_cast<std::size_t>(
            spec.directDelaySeconds * spec.sampleRateHz + 0.5);
        DirectSoundResult r = detectDirectSound(
            ArrayView<const double>(h.data(), h.size()), spec.sampleRateHz);
        CHECK(r.found);
        CHECK(r.sampleIndex == expect);
    }

    return testutil::summary("test_direct_sound");
}
