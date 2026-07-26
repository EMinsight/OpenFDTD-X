// DirectSoundDetector.h — RIR からの直接音 (最初の強い到来) 検出。
// Qt 非依存 / C++14。
//
// 最大ピークを無条件に直接音とはみなさない: 実測 RIR では後続の集中反射が
// 直接音より大きくなることがあるため、最大ピークに先行する「強いピーク」
// (最大の -20 dB 以上かつ 1 ms 以上先行) があれば先行側を直接音として採用し
// warning を付ける。
#pragma once
#include <cstddef>
#include <string>

#include "AnalysisQuality.h"
#include "ArrayView.h"

namespace ofd {
namespace acoustics {

// 検出方式
enum class DirectSoundMethod {
    Peak,               // 絶対値最大ピーク (+ 先行強ピーク補正)
    EnvelopeThreshold,  // |x| が最大値比の閾値を最初に超えた点
    MovingRmsThreshold  // 移動 RMS が最大値比の閾値を最初に超えた点
};

struct DirectSoundConfig {
    DirectSoundMethod method;
    double thresholdDb;                 // 閾値法: 最大値に対する相対閾値 [dB]
    double movingRmsWindowSeconds;      // MovingRmsThreshold の窓幅 [s]
    double precedingPeakThresholdDb;    // 先行ピーク採用条件: 最大比 [dB]
    double precedingPeakMinLeadSeconds; // 先行ピーク採用条件: 先行時間 [s]

    DirectSoundConfig()
        : method(DirectSoundMethod::Peak), thresholdDb(-20.0),
          movingRmsWindowSeconds(0.0005), precedingPeakThresholdDb(-20.0),
          precedingPeakMinLeadSeconds(0.001) {}
};

// §10.2: 直接音検出結果
struct DirectSoundResult {
    bool found;              // 検出できたか
    std::size_t sampleIndex; // 直接音のサンプル位置
    double timeSeconds;      // sampleIndex / fs [s]
    double amplitude;        // 直接音サンプルの振幅 (符号つき)
    double levelDb;          // 20*log10(|amplitude|) [dBFS]
    AnalysisQuality quality;
    std::string warning;

    DirectSoundResult()
        : found(false), sampleIndex(0), timeSeconds(0.0), amplitude(0.0),
          levelDb(-300.0), quality(AnalysisQuality::Invalid), warning() {}
};

DirectSoundResult detectDirectSound(ArrayView<const double> x,
                                    double sampleRateHz,
                                    const DirectSoundConfig &config =
                                        DirectSoundConfig());

} // namespace acoustics
} // namespace ofd
