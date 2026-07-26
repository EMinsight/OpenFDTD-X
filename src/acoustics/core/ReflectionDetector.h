// ReflectionDetector.h — 包絡線 (移動 RMS) ピーク検出による反射音検出。
// Qt 非依存 / C++14。
#pragma once
#include <cstddef>
#include <vector>

#include "ArrayView.h"
#include "DirectSoundDetector.h"

namespace ofd {
namespace acoustics {

struct ReflectionDetectorConfig {
    double smoothingWindowSeconds; // 移動 RMS の平滑化幅 (既定 0.5 ms)
    double minSeparationSeconds;   // 最小ピーク間隔 (既定 2 ms)
    double minRelativeLevelDb;     // 直接音比の最小相対レベル (既定 -40 dB)
    double noiseFloorMarginDb;     // ノイズフロアからの最小突出 (既定 6 dB)
    double detectionStartSeconds;  // 直接音後、検出を始めるまでの時間 (既定 1 ms)
    std::size_t maxReflections;    // 最大検出数 (既定 64、強い順に残す)
    int bandIndex;                 // 結果に付与する帯域番号 (-1 = 広帯域)

    ReflectionDetectorConfig()
        : smoothingWindowSeconds(0.0005), minSeparationSeconds(0.002),
          minRelativeLevelDb(-40.0), noiseFloorMarginDb(6.0),
          detectionStartSeconds(0.001), maxReflections(64), bandIndex(-1) {}
};

// §12: 反射音イベント
struct ReflectionEvent {
    double arrivalTime;     // 信号先頭を 0 とした到達時刻 [s]
    double delayFromDirect; // 直接音からの遅延 [s]
    double relativeLevelDb; // 直接音比レベル [dB]
    double energy;          // ピーク近傍のエネルギー (x² の和)
    double confidence;      // 0..1 (ノイズフロアからの突出量に基づく)
    int bandIndex;          // 帯域番号 (-1 = 広帯域)

    ReflectionEvent()
        : arrivalTime(0.0), delayFromDirect(0.0), relativeLevelDb(0.0),
          energy(0.0), confidence(0.0), bandIndex(-1) {}
};

// 時間区分ごとの集計: [0,20) / [20,80) / [80,200) / [200,∞) ms
struct ReflectionTimeSummary {
    static const int kBinCount = 4;
    int counts[4];          // 区分ごとの反射数
    double energies[4];     // 区分ごとの合計エネルギー
    double energyRatios[4]; // 直接音エネルギーに対する比
    double directEnergy;    // 参照した直接音エネルギー

    ReflectionTimeSummary() : directEnergy(0.0) {
        for (int i = 0; i < 4; ++i) {
            counts[i] = 0;
            energies[i] = 0.0;
            energyRatios[i] = 0.0;
        }
    }
};

// 直接音以降の包絡線ピークを反射音として検出する (時刻昇順で返す)。
std::vector<ReflectionEvent> detectReflections(
    ArrayView<const double> rir, double sampleRateHz,
    const DirectSoundResult &direct,
    const ReflectionDetectorConfig &config = ReflectionDetectorConfig());

// 直接音近傍 (±halfWindow) のエネルギー (x² の和)。集計の基準に使う。
double directSoundEnergy(ArrayView<const double> rir, double sampleRateHz,
                         const DirectSoundResult &direct,
                         double halfWindowSeconds = 0.0005);

// 遅延時間による区分集計。
ReflectionTimeSummary summarizeReflections(
    const std::vector<ReflectionEvent> &events, double directEnergy);

} // namespace acoustics
} // namespace ofd
