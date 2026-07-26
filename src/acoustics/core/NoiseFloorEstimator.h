// NoiseFloorEstimator.h — RIR 末尾区間の RMS からノイズフロアを推定する。
// Qt 非依存 / C++14。
#pragma once
#include <cstddef>
#include <string>

#include "ArrayView.h"

namespace ofd {
namespace acoustics {

struct NoiseFloorEstimate {
    double noiseRms;             // 末尾区間の RMS (線形振幅)
    double noiseFloorDb;         // 20*log10(noiseRms) [dBFS]
    double peakAbs;              // 信号全体の絶対値ピーク (線形振幅)
    double peakDb;               // 20*log10(peakAbs) [dBFS]
    double dynamicRangeDb;       // peakDb - noiseFloorDb
    std::size_t tailStartIndex;  // ノイズ推定に使った末尾区間の先頭
    std::size_t tailSampleCount; // 同区間のサンプル数
    bool valid;
    std::string warning;

    NoiseFloorEstimate()
        : noiseRms(0.0), noiseFloorDb(-300.0), peakAbs(0.0), peakDb(-300.0),
          dynamicRangeDb(0.0), tailStartIndex(0), tailSampleCount(0),
          valid(false), warning() {}
};

// RIR 末尾区間 (既定: 最後の 10%、最小 minTailSamples サンプル) の RMS から
// ノイズフロア [dB] を推定し、ピークとの差から動的範囲を算出する。
// 入力が短い場合は全体を使い、warning を付ける。
NoiseFloorEstimate estimateNoiseFloor(ArrayView<const double> x,
                                      double tailFraction = 0.10,
                                      std::size_t minTailSamples = 256);

} // namespace acoustics
} // namespace ofd
