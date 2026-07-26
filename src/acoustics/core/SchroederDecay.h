// SchroederDecay.h — Schroeder 二乗後方積分による減衰カーブと線形回帰。
// Qt 非依存 / C++14。
//
// 出典: M. R. Schroeder, "New Method of Measuring Reverberation Time",
//       J. Acoust. Soc. Am. 37 (1965). ノイズ補正は Chu (1978) の
//       末尾ノイズエネルギー減算に相当。評価区間は ISO 3382-1 に従う。
#pragma once
#include <cstddef>
#include <string>
#include <vector>

#include "ArrayView.h"

namespace ofd {
namespace acoustics {

struct SchroederOptions {
    bool noiseCompensation;        // 末尾ノイズエネルギー減算 (Chu 1978)
    double tailFraction;           // ノイズ推定に使う末尾割合 (既定 10%)
    std::size_t minTailSamples;    // 同・最小サンプル数 (既定 256)
    double analysisEndMarginDb;    // 分析終了点: ノイズフロア + margin 交差
    double smoothingWindowSeconds; // 包絡線平滑化窓 (終了点判定用, 既定 5ms)

    SchroederOptions()
        : noiseCompensation(false), tailFraction(0.10), minTailSamples(256),
          analysisEndMarginDb(10.0), smoothingWindowSeconds(0.005) {}
};

struct SchroederResult {
    std::vector<double> decayDb;  // 正規化減衰カーブ [dB] (decayDb[0] = 0)
    std::size_t analysisEndIndex; // 回帰に使える末尾 (これ以降はノイズ支配)
    double noiseFloorDb;          // 平滑化包絡線基準のノイズフロア [dB]
                                  // (指数減衰では減衰カーブと同一スケール)
    double noisePower;            // 推定ノイズパワー (x^2 の平均)
    double totalEnergy;           // 正規化に使った全エネルギー (補正後)
    bool valid;
    std::string warning;

    SchroederResult()
        : decayDb(), analysisEndIndex(0), noiseFloorDb(-300.0), noisePower(0.0),
          totalEnergy(0.0), valid(false), warning() {}
};

// 二乗後方積分 E(n) = Σ_{k≥n} x[k]^2 を dB 化した減衰カーブを返す。
// - ノイズ補正 (オプション): E(n) から推定ノイズ p*(N-n) を減算
// - 数値下限 1e-30 (エネルギー比) でクランプ
// - 分析終了点: 平滑化包絡線がノイズフロア + margin を下回る点 (なければ末尾)
SchroederResult computeSchroederDecay(ArrayView<const double> rir,
                                      double sampleRateHz,
                                      const SchroederOptions &options =
                                          SchroederOptions());

// §10.4: 減衰カーブ回帰の結果
struct RegressionResult {
    double slope;         // 傾き [dB/s]
    double intercept;     // 切片 [dB] (t=0 での外挿値)
    double rSquared;      // 決定係数
    double standardError; // 傾きの標準誤差 [dB/s]
    double startDb;       // 実際に使用した区間の開始レベル [dB]
    double endDb;         // 同・終了レベル [dB]
    bool valid;
    std::string warning;

    RegressionResult()
        : slope(0.0), intercept(0.0), rSquared(0.0), standardError(0.0),
          startDb(0.0), endDb(0.0), valid(false), warning() {}
};

// 減衰カーブ decayDb のうち startDb〜endDb (例: -5〜-25 dB) の区間を
// 最小二乗直線回帰する。区間が analysisEndIndex を超える場合は無効。
RegressionResult regressDecaySegment(ArrayView<const double> decayDb,
                                     double sampleRateHz, double startDb,
                                     double endDb,
                                     std::size_t analysisEndIndex);

} // namespace acoustics
} // namespace ofd
