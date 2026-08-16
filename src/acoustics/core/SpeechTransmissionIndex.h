// SpeechTransmissionIndex.h — RIR からの STI (IEC 60268-16 間接法)。
// Qt 非依存 / C++14。
//
// 間接法 (RIR からの MTF):
//   m(fm) = |∫ h²(t) e^{-j2πfm t} dt| / ∫ h²(t) dt
// を 7 オクターブ帯域 (125 Hz〜8 kHz) × 14 変調周波数
// (0.63〜12.5 Hz, 1/3 オクターブ) について求め、
//   SNR_eff = 10·log10(m / (1-m))   [-15, +15] dB でクリップ
//   TI = (SNR_eff + 15) / 30        [0, 1]
//   MTI_k = 14 変調周波数の TI 平均
//   STI = Σ α_k·MTI_k − Σ β_k·√(MTI_k·MTI_{k+1})
// で求める (IEC 60268-16 第 4 版、男声の重み係数)。
//
// **できること / できないこと**
// - 背景雑音や音声帯域外の妨害は考慮しない (RIR だけから求める「室の
//   伝送性能」)。実測 STI として雑音下の値が要る場合は雑音スペクトルの
//   入力が別途必要 — 本実装は雑音項なし (m の雑音補正を掛けない) である
//   ことを呼び出し側が明示すること。
// - 8 kHz 帯を含むので fs は 16 kHz 以上を要求する (足りなければ無効)。
//
// 検証は指数減衰 RIR の MTF 閉形式
//   m(fm) = 1 / √(1 + (2π fm T / 13.8)²)     (T = 残響時間)
// と突き合わせる (tests/acoustics/test_sti.cpp)。
#pragma once
#include <cstddef>
#include <string>
#include <vector>

#include "AnalysisQuality.h"
#include "ArrayView.h"

namespace ofd {
namespace acoustics {

// IEC 60268-16 の 14 変調周波数 [Hz] (1/3 オクターブ 0.63〜12.5)
const double *stiModulationFrequencies(std::size_t *count);
// STI の 7 オクターブ帯域中心 [Hz] (125〜8000)
const double *stiOctaveBands(std::size_t *count);

struct StiBandResult {
    double centerHz;
    std::vector<double> mtf;      // 変調周波数ごとの m (14 個)
    std::vector<double> ti;       // 同 TI (14 個)
    double mti;                   // 帯域の変調伝達指数 (TI の平均)
    bool   valid;
    std::string warning;

    StiBandResult()
        : centerHz(0.0), mtf(), ti(), mti(0.0), valid(false), warning() {}
};

struct StiResult {
    MetricValue sti;                   // 0..1
    std::vector<StiBandResult> bands;  // 7 帯域
    AnalysisQuality quality;
    std::string warning;

    StiResult()
        : sti(), bands(), quality(AnalysisQuality::Invalid), warning() {}
};

// rir[directIndex] を t = 0 として STI を求める。
// 帯域通過には既存の BandFilter (4 次バターワース) を使う。
StiResult computeSti(ArrayView<const double> rir, double sampleRateHz,
                     std::size_t directIndex);

} // namespace acoustics
} // namespace ofd
