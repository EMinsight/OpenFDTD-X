// test_convolution.cpp — ConvolutionEngine (Overlap-Add FFT 畳み込み) の検証。
//
//   (a) デルタ RIR → 出力が入力と一致 (< 1e-12)
//   (b) 2 デルタ RIR (遅延 d, ゲイン g) → y[n] = x[n] + g·x[n-d]
//   (c) ランダム系列 (977 × 313) を直接畳み込み convolveDirect と比較 (< 1e-9)
//   (d) サンプルレート不一致 → kSampleRateMismatch エラー (暗黙リサンプリングなし)
//   (e) クリップする出力 → info.outputPeak > 1 かつ suggestedGainDb < 0
//   (f) 10 s × 2 s (48 kHz) の実サイズ入力が完走する
#include <cmath>
#include <cstdio>
#include <vector>

#include "../../src/acoustics/core/ConvolutionEngine.h"
#include "../../src/acoustics/core/Fft.h" // isPowerOfTwo
#include "test_common.h"

using namespace ofd::acoustics;

namespace {

// モノラル AudioBuffer を作る
AudioBuffer makeMono(const std::vector<double> &x, double fs) {
    AudioBuffer b;
    b.sampleRateHz = fs;
    b.channels.push_back(x);
    return b;
}

// 決定的乱数列 ([-1, 1])
std::vector<double> makeRandom(std::size_t n, unsigned seed) {
    std::vector<double> x(n);
    unsigned st = seed;
    for (std::size_t i = 0; i < n; ++i) x[i] = testutil::lcgUniform(st);
    return x;
}

// 2 系列の最大絶対差
double maxAbsDiff(const std::vector<double> &a, const std::vector<double> &b) {
    double m = 0.0;
    const std::size_t n = a.size() < b.size() ? a.size() : b.size();
    for (std::size_t i = 0; i < n; ++i) {
        const double e = std::fabs(a[i] - b[i]);
        if (e > m) m = e;
    }
    return m;
}

} // namespace

int main() {
    const double fs = 48000.0;
    ConvolutionEngine engine;

    // ── (a) デルタ RIR: h = {1} → 出力は入力そのもの ──
    {
        const std::vector<double> x = makeRandom(5000, 1u);
        const std::vector<double> h(1, 1.0);
        AcousticResult<ConvolvedAudio> r =
            engine.convolve(makeMono(x, fs), makeMono(h, fs));
        CHECK(r.success());
        const ConvolvedAudio &out = r.value();
        CHECK(out.audio.channelCount() == 1);
        CHECK(out.audio.sampleCount() == x.size()); // 5000 + 1 - 1
        CHECK(out.info.outputLength == x.size());
        const double err = maxAbsDiff(out.audio.channels[0], x);
        CHECK(err < 1e-12);
        std::printf("  (a) delta RIR max err=%.3e\n", err);
    }

    // ── (b) 2 デルタ RIR: h[0]=1, h[d]=g → y[n] = x[n] + g·x[n-d] ──
    {
        const std::size_t d = 100;
        const double g = 0.5;
        const std::vector<double> x = makeRandom(4000, 2u);
        std::vector<double> h(d + 1, 0.0);
        h[0] = 1.0;
        h[d] = g;
        AcousticResult<ConvolvedAudio> r =
            engine.convolve(makeMono(x, fs), makeMono(h, fs));
        CHECK(r.success());
        const std::vector<double> &y = r.value().audio.channels[0];
        CHECK(y.size() == x.size() + d); // x + h - 1

        std::vector<double> expected(x.size() + d, 0.0);
        for (std::size_t i = 0; i < x.size(); ++i) {
            expected[i] += x[i];
            expected[i + d] += g * x[i];
        }
        const double err = maxAbsDiff(y, expected);
        CHECK(err < 1e-12);
        std::printf("  (b) two-delta RIR max err=%.3e\n", err);
    }

    // ── (c) ランダム系列 977 × 313: FFT 畳み込み vs 直接畳み込み ──
    {
        const std::vector<double> x = makeRandom(977, 3u);
        const std::vector<double> h = makeRandom(313, 4u);
        AcousticResult<ConvolvedAudio> r =
            engine.convolve(makeMono(x, fs), makeMono(h, fs));
        CHECK(r.success());
        const std::vector<double> &y = r.value().audio.channels[0];
        const std::vector<double> ref = ConvolutionEngine::convolveDirect(
            ArrayView<const double>(x.data(), x.size()),
            ArrayView<const double>(h.data(), h.size()));
        CHECK(y.size() == 977 + 313 - 1);
        CHECK(ref.size() == y.size());
        const double err = maxAbsDiff(y, ref);
        CHECK(err < 1e-9);
        // 付帯情報の整合性
        const ConvolutionInfo &info = r.value().info;
        CHECK(info.rirLength == 313);
        CHECK(info.outputLength == y.size());
        CHECK(isPowerOfTwo(info.fftLength));
        CHECK(info.blockLength == info.fftLength - info.rirLength + 1);
        std::printf("  (c) 977x313 vs direct max err=%.3e\n", err);
    }

    // ── (d) サンプルレート不一致 → エラー (黙ってリサンプリングしない) ──
    {
        const std::vector<double> x = makeRandom(1000, 5u);
        const std::vector<double> h = makeRandom(100, 6u);
        AcousticResult<ConvolvedAudio> r =
            engine.convolve(makeMono(x, 48000.0), makeMono(h, 44100.0));
        CHECK(!r.success());
        CHECK(r.errorCode() == kSampleRateMismatch);
        // kSampleRateMismatch は既存コード体系の UnsupportedSampleRate を充てる
        CHECK(r.errorCode() == AcousticErrorCode::UnsupportedSampleRate);
        CHECK(r.message().find("sample rate mismatch") != std::string::npos);
    }

    // ── (e) クリップする入力: ピーク > 1 → 情報と推奨ゲインを返す ──
    {
        // 定数 0.9 × ゲイン 2 のデルタ → ピーク 1.8 (自動正規化はしない)
        const std::vector<double> x(1000, 0.9);
        const std::vector<double> h(1, 2.0);
        AcousticResult<ConvolvedAudio> r =
            engine.convolve(makeMono(x, fs), makeMono(h, fs));
        CHECK(r.success());
        const ConvolutionInfo &info = r.value().info;
        CHECK(info.outputPeak > 1.0);
        CHECK_NEAR(info.outputPeak, 1.8, 1e-9);
        CHECK(info.suggestedGainDb < 0.0);
        CHECK_NEAR(info.suggestedGainDb, -20.0 * std::log10(1.8), 1e-9);
        CHECK(info.clipped);
        CHECK(info.clippedSampleCount > 0);
        // 出力自体は畳み込み値のまま (正規化されていない)
        CHECK_NEAR(r.value().audio.channels[0][500], 1.8, 1e-9);
    }

    // ── (f) 実サイズ: 10 s dry × 2 s RIR (48 kHz) が完走する ──
    {
        const std::size_t dryLen = static_cast<std::size_t>(10.0 * fs + 0.5);
        const std::vector<double> x = makeRandom(dryLen, 7u);
        testutil::SyntheticRirSpec spec;
        spec.rt60 = 1.0;
        spec.sampleRateHz = fs;
        spec.durationSeconds = 2.0;
        const std::vector<double> h = testutil::makeSyntheticRir(spec);
        CHECK(h.size() == static_cast<std::size_t>(2.0 * fs + 0.5));

        AcousticResult<ConvolvedAudio> r =
            engine.convolve(makeMono(x, fs), makeMono(h, fs));
        CHECK(r.success());
        const ConvolvedAudio &out = r.value();
        CHECK(out.audio.sampleCount() == x.size() + h.size() - 1);
        CHECK(out.info.outputLength == out.audio.sampleCount());
        // 出力が有限値でピーク情報と矛盾しないこと
        double peak = 0.0;
        bool finite = true;
        const std::vector<double> &y = out.audio.channels[0];
        for (std::size_t i = 0; i < y.size(); ++i) {
            if (!std::isfinite(y[i])) finite = false;
            const double a = std::fabs(y[i]);
            if (a > peak) peak = a;
        }
        CHECK(finite);
        CHECK_NEAR(peak, out.info.outputPeak, 1e-9);
        std::printf("  (f) 10s x 2s done: out=%zu samples peak=%.3f\n",
                    y.size(), peak);
    }

    return testutil::summary("test_convolution");
}
