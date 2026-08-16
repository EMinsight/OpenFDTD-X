// SoundStrength.h — G (音の強さ、ISO 3382-1 A.2.6)。Qt 非依存 / C++14。
//
//   G = 10·log10( ∫p²(t)dt / ∫p₁₀²(t)dt )   [dB]
//
// 分母 p₁₀ は「同じ音源を自由音場で 10 m 離れて測ったインパルス応答」。
// つまり G は**基準録音との比**であり、dBFS → dB SPL の絶対校正
// (CalibrationState::Absolute / calibrationOffsetDb) は要らない。
// 要るのは「基準録音と実測 RIR が同じ利得系 (音源出力・マイク感度・
// プリアンプ・AD) で録られていること」の 1 点だけである。
// この前提はデータからは確認できないので、表示側が必ず注記する
// (値そのものは物理量として正しい)。
//
// ── 基準録音が 10 m でないとき ──────────────────────────────────────────
// 自由音場では p ∝ 1/r なので、距離 r で測った基準エネルギー E_r から
//   E₁₀ = E_r·(r/10)²
// したがって
//   G = 10·log10(E/E_r) − 20·log10(r/10)
// 距離補正 −20log10(r/10) を distanceCorrectionDb に持つ。
//
// ── サンプリング周波数 ────────────────────────────────────────────────
// エネルギーは Σx²/fs (= ∫p²dt の矩形近似) で求める。実測と基準で fs が
// 違っても比が正しくなるため、この 1/fs を落としてはいけない。
//
// ── 早期 / 後期 ────────────────────────────────────────────────────────
// G_early (直接音〜80 ms) / G_late (80 ms 以降) は同じ E₁₀ を分母にした
// 派生量 (ISO 3382-1 本体が定義する必須量ではない)。定義上
//   10^(G_early/10) + 10^(G_late/10) = 10^(G/10)
// が厳密に成り立つ (test_strength がこの恒等式を検査する)。
// 窓が欠ける (RIR が直接音後 80 ms に満たない) 場合は invalid とする。
#pragma once
#include <cstddef>
#include <string>

#include "AcousticError.h"
#include "AnalysisQuality.h"
#include "ArrayView.h"

namespace ofd {
namespace acoustics {

// 基準録音 (自由音場の p₁₀) から得た分母。
struct SoundStrengthReference {
    bool   available;   // false なら G を計算しない (黙って 0 dB を出さない)
    double energy;      // ∫p_ref²dt = Σx²/fs [FS²·s]
    double distanceM;   // 基準録音の音源距離 [m] (規格値 10)

    SoundStrengthReference()
        : available(false), energy(0.0), distanceM(10.0) {}
};

// 基準インパルス応答から SoundStrengthReference を作る。
// refIr は自由音場 (無響室 / 屋外) で距離 distanceM に置いたマイクの応答。
// 反射を含む録音を渡してはいけない (分母が過大になり G が小さく出る) —
// これはデータからは判定できないので呼び出し側の責任。
AcousticResult<SoundStrengthReference> makeSoundStrengthReference(
        ArrayView<const double> refIr, double sampleRateHz,
        double distanceM = 10.0);

// 基準エネルギーを dB で直接与える経路 (基準録音を持たず、音源の
// 校正値だけがある場合)。energyDb = 10·log10(∫p_ref²dt)。
AcousticResult<SoundStrengthReference> makeSoundStrengthReferenceDb(
        double energyDb, double distanceM = 10.0);

struct SoundStrengthResult {
    MetricValue g;          // [dB] 音の強さ
    MetricValue gEarly;     // [dB] 直接音〜80 ms
    MetricValue gLate;      // [dB] 80 ms 以降
    double measuredEnergyDb;     // 10·log10(∫p²dt) — 基準が無くても出る参考値
    double referenceEnergyDb;    // 同 基準側 (基準が無ければ -300)
    double distanceCorrectionDb; // −20·log10(r/10) (10 m 基準なら 0)
    AnalysisQuality quality;
    std::string warning;

    SoundStrengthResult()
        : g(), gEarly(), gLate(), measuredEnergyDb(-300.0),
          referenceEnergyDb(-300.0), distanceCorrectionDb(0.0),
          quality(AnalysisQuality::Invalid), warning() {}
};

// rir[directIndex] を t = 0 として G を求める。
// ref.available == false のときは measuredEnergyDb だけ埋めて G を invalid に
// する (基準が無いのに 0 dB を出すと「基準と同じ強さ」に見えるため)。
SoundStrengthResult computeSoundStrength(ArrayView<const double> rir,
                                         double sampleRateHz,
                                         std::size_t directIndex,
                                         const SoundStrengthReference &ref);

} // namespace acoustics
} // namespace ofd
