// AcousticMetrics.h — ISO 3382-1 に基づく室内音響指標。Qt 非依存 / C++14。
//
// 直接音時刻を t = 0 として以下を算出する:
//   EDT  : 0〜-10 dB 区間の回帰 ×6         (ISO 3382-1 A.2.2)
//   T20  : -5〜-25 dB 区間の回帰 ×3        (ISO 3382-1 6.3)
//   T30  : -5〜-35 dB 区間の回帰 ×2        (ISO 3382-1 6.3)
//   C50/C80 : 明瞭度 10*log10(早期/後期)   (ISO 3382-1 A.2.4)
//   D50  : Definition 早期/全体            (ISO 3382-1 A.2.3)
//   Ts   : 重心時間 ∫t p² dt / ∫p² dt      (ISO 3382-1 A.2.5)
//   Early/Late 比 (50ms / 80ms, 線形エネルギー比)
//
// 動的範囲不足 (評価下限 + 10 dB マージンがノイズフロアを下回る) の場合は
// valid = false とし理由を warning に記す。決定係数 rSquared < 0.95 の場合は
// quality = Warning。
#pragma once
#include <cstddef>

#include "AnalysisQuality.h"
#include "ArrayView.h"
#include "SchroederDecay.h"

namespace ofd {
namespace acoustics {

struct MetricsOptions {
    SchroederOptions schroeder;      // 減衰カーブ計算オプション
    double validityMarginDb;         // 評価下限に上乗せするマージン (既定 10 dB)
    double rSquaredWarningThreshold; // これ未満で quality = Warning (既定 0.95)

    MetricsOptions()
        : schroeder(), validityMarginDb(10.0), rSquaredWarningThreshold(0.95) {
        // 残響指標の既定はノイズ補正あり (長い RT で後方積分にノイズが
        // 蓄積するのを防ぐ)。不要なら呼び出し側で false にする。
        schroeder.noiseCompensation = true;
    }
};

struct AcousticMetricsSet {
    MetricValue edt;         // [s]
    MetricValue t20;         // [s]
    MetricValue t30;         // [s]
    MetricValue c50;         // [dB]
    MetricValue c80;         // [dB]
    MetricValue d50;         // [0..1]
    MetricValue ts;          // [s]
    MetricValue earlyLate50; // 線形比 (早期 0-50ms / 後期 50ms-)
    MetricValue earlyLate80; // 線形比 (早期 0-80ms / 後期 80ms-)
    RegressionResult edtRegression;
    RegressionResult t20Regression;
    RegressionResult t30Regression;
    double decayNoiseFloorDb; // 減衰カーブ基準のノイズフロア [dB]

    AcousticMetricsSet()
        : edt(), t20(), t30(), c50(), c80(), d50(), ts(), earlyLate50(),
          earlyLate80(), edtRegression(), t20Regression(), t30Regression(),
          decayNoiseFloorDb(-300.0) {}
};

// rir[directIndex] を t = 0 として全指標を計算する。
AcousticMetricsSet computeAcousticMetrics(ArrayView<const double> rir,
                                          double sampleRateHz,
                                          std::size_t directIndex,
                                          const MetricsOptions &options =
                                              MetricsOptions());

} // namespace acoustics
} // namespace ofd
