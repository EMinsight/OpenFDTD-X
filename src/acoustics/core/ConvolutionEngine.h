// ConvolutionEngine.h — Overlap-Add FFT 畳み込み (乾いた音源 × RIR)。
// Qt 非依存 / C++14。
//
// 方針:
//   - 自動正規化は行わない。出力は畳み込み値そのままで、クリッピング情報と
//     推奨ゲイン (適用は呼び出し側の判断) を ConvolutionInfo で併せて返す。
//   - サンプルレート不一致は黙ってリサンプリングせずエラーにする。
//   - チャンネル: dry モノ × RIR モノ/ステレオ → 出力は RIR と同チャンネル数。
//     dry がステレオ以上の場合は平均でモノ化し警告を付ける。
#pragma once
#include <cstddef>
#include <string>
#include <vector>

#include "AcousticError.h"
#include "ArrayView.h"
#include "AudioBuffer.h"

namespace ofd {
namespace acoustics {

// サンプルレート不一致のエラーコード (SampleRateMismatch)。
// 既存の AcousticErrorCode 体系 (16 値、変更不可) の UnsupportedSampleRate を
// 充てる。メッセージに "sample rate mismatch" を含める。
const AcousticErrorCode kSampleRateMismatch =
    AcousticErrorCode::UnsupportedSampleRate;

struct ConvolutionEngineConfig {
    std::size_t minFftLength; // FFT 長の下限 (2 の冪に丸める、既定 1024)
    double clipThreshold;     // クリッピング判定振幅 (既定 1.0)

    ConvolutionEngineConfig() : minFftLength(1024), clipThreshold(1.0) {}
};

// 畳み込み結果の付帯情報 (正規化は行わないため、呼び出し側の判断材料)
struct ConvolutionInfo {
    double outputPeak;              // 全チャンネル最大絶対値 (線形)
    double outputPeakDbfs;          // 20*log10(outputPeak) [dBFS]
    double suggestedGainDb;         // ピークをフルスケール (1.0) に合わせる
                                    // ゲイン = -20*log10(outputPeak)。適用は
                                    // 呼び出し側 (自動では掛けない)
    std::size_t clippedSampleCount; // |x| > clipThreshold のサンプル総数
    bool clipped;                   // clippedSampleCount > 0
    bool dryDownmixed;              // dry を平均モノ化したか
    std::size_t blockLength;        // Overlap-Add ブロック長 L
    std::size_t fftLength;          // FFT 長 N (= L + rirLength - 1 以上の 2 の冪)
    std::size_t rirLength;          // RIR サンプル数
    std::size_t outputLength;       // 出力サンプル数 (dry + rir - 1)
    std::vector<std::string> warnings;

    ConvolutionInfo()
        : outputPeak(0.0), outputPeakDbfs(-300.0), suggestedGainDb(0.0),
          clippedSampleCount(0), clipped(false), dryDownmixed(false),
          blockLength(0), fftLength(0), rirLength(0), outputLength(0),
          warnings() {}
};

struct ConvolvedAudio {
    AudioBuffer audio;   // 畳み込み結果 (正規化なし)
    ConvolutionInfo info;

    ConvolvedAudio() : audio(), info() {}
};

class ConvolutionEngine {
public:
    explicit ConvolutionEngine(
        const ConvolutionEngineConfig &config = ConvolutionEngineConfig());

    // dry × rir の線形畳み込み。出力長 = dry長 + rir長 - 1。
    // サンプルレート不一致は kSampleRateMismatch エラー。
    AcousticResult<ConvolvedAudio> convolve(const AudioBuffer &dry,
                                            const AudioBuffer &rir) const;

    // 直接畳み込み (O(N·M))。小ケースの内部検証・テスト用。
    static std::vector<double> convolveDirect(ArrayView<const double> x,
                                              ArrayView<const double> h);

    const ConvolutionEngineConfig &config() const { return m_config; }

private:
    ConvolutionEngineConfig m_config;
};

} // namespace acoustics
} // namespace ofd
