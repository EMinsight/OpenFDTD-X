// ConvolutionEngine.cpp — Overlap-Add FFT 畳み込みの実装。
//
// 手順 (Oppenheim & Schafer, Discrete-Time Signal Processing の overlap-add):
//   1. FFT 長 N = max(minFftLength, nextPow2(2·rir長)) を選ぶ
//      (ブロック長 L = N - rir長 + 1 ≥ 1 が保証される)
//   2. RIR の各チャンネルを長さ N でスペクトル化 (1 回のみ)
//   3. dry を L サンプルずつ区切り、FFT → 積 → 逆 FFT → 出力へ重ね加算
#include "ConvolutionEngine.h"

#include <cmath>
#include <complex>
#include <cstdio>

#include "Fft.h"

namespace ofd {
namespace acoustics {

namespace {

// NaN / Inf の個数を数える
std::size_t countNonFinite(const std::vector<std::vector<double>> &channels) {
    std::size_t count = 0;
    for (std::size_t c = 0; c < channels.size(); ++c) {
        for (std::size_t i = 0; i < channels[c].size(); ++i) {
            if (!std::isfinite(channels[c][i])) ++count;
        }
    }
    return count;
}

// 全チャンネル同一長かを確認する
bool channelsSameLength(const AudioBuffer &b) {
    for (std::size_t c = 1; c < b.channels.size(); ++c) {
        if (b.channels[c].size() != b.channels[0].size()) return false;
    }
    return true;
}

} // namespace

ConvolutionEngine::ConvolutionEngine(const ConvolutionEngineConfig &config)
    : m_config(config) {}

std::vector<double> ConvolutionEngine::convolveDirect(
    ArrayView<const double> x, ArrayView<const double> h) {
    if (x.empty() || h.empty()) return std::vector<double>();
    std::vector<double> y(x.size() + h.size() - 1, 0.0);
    for (std::size_t i = 0; i < x.size(); ++i) {
        const double xi = x[i];
        for (std::size_t j = 0; j < h.size(); ++j) y[i + j] += xi * h[j];
    }
    return y;
}

AcousticResult<ConvolvedAudio>
ConvolutionEngine::convolve(const AudioBuffer &dry,
                            const AudioBuffer &rir) const {
    typedef AcousticResult<ConvolvedAudio> Result;

    // ── 入力検査 ──
    if (dry.channelCount() == 0 || dry.sampleCount() == 0)
        return Result::error(AcousticErrorCode::EmptyInput, "dry signal is empty");
    if (rir.channelCount() == 0 || rir.sampleCount() == 0)
        return Result::error(AcousticErrorCode::EmptyInput, "RIR is empty");
    if (!channelsSameLength(dry) || !channelsSameLength(rir))
        return Result::error(AcousticErrorCode::InvalidArgument,
                             "channel lengths differ within a buffer");
    if (!(dry.sampleRateHz > 0.0) || !(rir.sampleRateHz > 0.0))
        return Result::error(AcousticErrorCode::UnsupportedSampleRate,
                             "sample rate must be positive");
    // サンプルレート不一致 → SampleRateMismatch (黙ってリサンプリングしない)
    if (dry.sampleRateHz != rir.sampleRateHz) {
        char msg[128];
        std::snprintf(msg, sizeof(msg),
                      "sample rate mismatch: dry %.9g Hz vs RIR %.9g Hz "
                      "(resampling is not performed implicitly)",
                      dry.sampleRateHz, rir.sampleRateHz);
        return Result::error(kSampleRateMismatch, msg);
    }
    if (countNonFinite(dry.channels) > 0)
        return Result::error(AcousticErrorCode::NonFiniteSample,
                             "dry signal contains NaN/Inf");
    if (countNonFinite(rir.channels) > 0)
        return Result::error(AcousticErrorCode::NonFiniteSample,
                             "RIR contains NaN/Inf");

    ConvolvedAudio out;
    ConvolutionInfo &info = out.info;

    // ── dry のモノ化 (チャンネル平均)。モノ入力はそのまま ──
    std::vector<double> mono;
    if (dry.channelCount() == 1) {
        mono = dry.channels[0];
    } else {
        const std::size_t n = dry.sampleCount();
        mono.assign(n, 0.0);
        const double inv = 1.0 / static_cast<double>(dry.channelCount());
        for (std::size_t c = 0; c < dry.channelCount(); ++c) {
            for (std::size_t i = 0; i < n; ++i) mono[i] += dry.channels[c][i];
        }
        for (std::size_t i = 0; i < n; ++i) mono[i] *= inv;
        info.dryDownmixed = true;
        info.warnings.push_back(
            "dry がマルチチャンネルのため平均でモノ化しました");
    }

    const std::size_t dryLen = mono.size();
    const std::size_t rirLen = rir.sampleCount();
    const std::size_t outLen = dryLen + rirLen - 1;

    // ── FFT 長とブロック長 ──
    std::size_t fftLen = nextPowerOfTwo(2 * rirLen);
    const std::size_t minFft = nextPowerOfTwo(
        m_config.minFftLength > 0 ? m_config.minFftLength : 1024);
    if (fftLen < minFft) fftLen = minFft;
    const std::size_t blockLen = fftLen - rirLen + 1; // >= rirLen + 1 - 保証
    info.blockLength = blockLen;
    info.fftLength = fftLen;
    info.rirLength = rirLen;
    info.outputLength = outLen;

    // ── 出力バッファ (RIR と同チャンネル数) ──
    out.audio.sampleRateHz = dry.sampleRateHz;
    out.audio.channels.assign(rir.channelCount(),
                              std::vector<double>(outLen, 0.0));

    typedef std::vector<std::complex<double>> Spectrum;
    Spectrum X(fftLen), H(fftLen);

    for (std::size_t c = 0; c < rir.channelCount(); ++c) {
        // RIR スペクトル (チャンネルごとに 1 回)
        for (std::size_t i = 0; i < fftLen; ++i) {
            H[i] = std::complex<double>(i < rirLen ? rir.channels[c][i] : 0.0,
                                        0.0);
        }
        if (!fftForward(H)) {
            return Result::error(AcousticErrorCode::InvalidArgument,
                                 "internal error: FFT length is not a power of two");
        }

        std::vector<double> &y = out.audio.channels[c];
        for (std::size_t start = 0; start < dryLen; start += blockLen) {
            const std::size_t count =
                (dryLen - start < blockLen) ? (dryLen - start) : blockLen;
            for (std::size_t i = 0; i < fftLen; ++i) {
                X[i] = std::complex<double>(i < count ? mono[start + i] : 0.0,
                                            0.0);
            }
            fftForward(X);
            for (std::size_t i = 0; i < fftLen; ++i) X[i] *= H[i];
            fftInverse(X);
            // 重ね加算 (出力末尾で切り詰め)
            const std::size_t remain = outLen - start;
            const std::size_t addLen = (fftLen < remain) ? fftLen : remain;
            for (std::size_t i = 0; i < addLen; ++i)
                y[start + i] += X[i].real();
        }
    }

    // ── ピーク / クリッピング情報 (正規化はしない) ──
    double peak = 0.0;
    std::size_t clippedCount = 0;
    for (std::size_t c = 0; c < out.audio.channels.size(); ++c) {
        const std::vector<double> &y = out.audio.channels[c];
        for (std::size_t i = 0; i < y.size(); ++i) {
            const double a = std::fabs(y[i]);
            if (a > peak) peak = a;
            if (a > m_config.clipThreshold) ++clippedCount;
        }
    }
    info.outputPeak = peak;
    info.outputPeakDbfs = (peak > 0.0) ? 20.0 * std::log10(peak) : -300.0;
    info.suggestedGainDb = (peak > 0.0) ? -20.0 * std::log10(peak) : 0.0;
    info.clippedSampleCount = clippedCount;
    info.clipped = clippedCount > 0;
    if (info.clipped) {
        char msg[160];
        std::snprintf(msg, sizeof(msg),
                      "出力が %zu サンプルでフルスケールを超過 (ピーク %.3f)。"
                      "推奨ゲイン %.2f dB を適用してください",
                      clippedCount, peak, info.suggestedGainDb);
        info.warnings.push_back(msg);
    }

    return Result::ok(std::move(out));
}

} // namespace acoustics
} // namespace ofd
