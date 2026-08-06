// HybridRir.h — 低域 FDTD + 高域 幾何音響 のハイブリッド RIR 合成。
// Qt 非依存 / C++14。
//
// ── なぜ必要か ──────────────────────────────────────────────────────────────
// 音響 FDTD の有効帯域は格子分解能で決まる (OpenAcoustics: fmax = c/(10·dx))。
// 7,200 m³ のホールで歌声の可聴化に要る 4 kHz を得るには dx = 8.6 mm =
// 114 億セルとなり原理的に不可能なので、低域を FDTD、高域を幾何音響が担い、
// 帯域分割して足す (実務の定石)。ここはその「足す」側を担当する。
//
// ── 3 つの前処理 (これをやらないと物理的に正しく足せない) ──────────────────
// 1. 音源パルスの逆フィルタ: FDTD の出力は Green 関数 h ではなく
//    「音源パルス s との畳み込み p = h*s」。s (ガウシアン微分、σ) の
//    スペクトルが乗ったままなので、正則化つき逆フィルタで h に戻す。
//    s は遅延 t0 を含むので、**この 1 手で t0 の除去も同時に済む**。
// 2. サンプルレート合わせ: FDTD の fs は CFL で決まる端数 (例 1201 Hz)。
//    幾何音響側 (48 kHz) へ core/Resampler で合わせる。
// 3. レベル整合: FDTD の振幅規約は音源注入の仕方に依存して任意性がある。
//    幾何音響側は自由音場 1/(4πr) 規約なので、クロスオーバー帯のバンド
//    エネルギーが一致するよう **FDTD 側**にゲインを掛ける (掛けた量は報告)。
//
// ── クロスオーバー ──────────────────────────────────────────────────────────
// 線形位相 FIR の相補対を使う: h_hp = δ[center] − h_lp。**厳密に相補**
// (LP + HP = 単位インパルスの遅延) なので、
//   - 合成でエネルギーが増減しない (山や谷を作らない)
//   - 両ブランチの群遅延が同一 → 引き算で共通遅延を落とせば**時間原点が動かない**
//     (直接音の到達時刻・ITDG が保たれる = 音響指標を壊さない)
// という 2 点が保証される。合成後は共通遅延 center を先頭から取り除く。
#pragma once
#include <cstddef>
#include <string>
#include <vector>

#include "AcousticError.h"
#include "ArrayView.h"
#include "AudioBuffer.h"

namespace ofd {
namespace acoustics {

struct HybridRirConfig {
    // クロスオーバー周波数 [Hz]。0 なら fdtdFmaxHz を使う
    double crossoverHz;
    // FDTD の有効帯域上限 [Hz] (metadata.json の source.fmax_hz)。
    // crossoverHz が 0 のときの自動決定に使う
    double fdtdFmaxHz;
    // FDTD 音源 (ガウシアン微分パルス) の σ [s] と遅延 t0 [s]
    // (metadata.json の source.sigma_s / t0_s)。σ = 0 なら逆フィルタしない
    double sourceSigmaS;
    double sourceT0S;
    // 逆フィルタの正則化係数 (最大 |S|² に対する比)。小さいほど帯域端まで
    // 復元するがノイズも増える。既定 1e-6 は増幅の上限が 1/(2√ε) = 500 倍
    // (54 dB) — 音源スペクトルが fmax で −53 dB なので過不足がない
    double deconvEpsilon;
    // クロスオーバーの遷移帯域幅 (crossoverHz に対する比)
    double transitionWidth;
    // クロスオーバー帯で FDTD 側のレベルを幾何音響側に合わせるか
    bool matchLevels;

    HybridRirConfig()
        : crossoverHz(0.0), fdtdFmaxHz(0.0), sourceSigmaS(0.0),
          sourceT0S(0.0), deconvEpsilon(1e-6), transitionWidth(0.5),
          matchLevels(true) {}
};

struct HybridRirInfo {
    double crossoverHz;       // 実際に使ったクロスオーバー [Hz]
    double outputRateHz;      // 出力 fs (= 幾何音響側の fs)
    double fdtdGainDb;        // レベル整合で FDTD に掛けた量 [dB]
    double removedDelayS;     // 逆フィルタで除去した音源遅延 t0 [s]
    std::size_t filterLength; // クロスオーバー FIR 長 (奇数)
    std::size_t outputLength; // 出力サンプル数
    bool deconvolved;         // 音源パルスの逆フィルタを行ったか
    bool resampled;           // FDTD 側をリサンプリングしたか
    std::vector<std::string> warnings;

    HybridRirInfo()
        : crossoverHz(0.0), outputRateHz(0.0), fdtdGainDb(0.0),
          removedDelayS(0.0), filterLength(0), outputLength(0),
          deconvolved(false), resampled(false), warnings() {}
};

// 音源パルス (ガウシアン微分 s(t) = -u·exp(1/2 - u²/2), u = (t-t0)/σ) の
// 正則化つき逆フィルタ。出力は Green 関数相当で、t0 の遅延も取り除かれる
// (直接音は t = r/c に立つ)。絶対振幅は保存しない (レベル整合が担当)。
AcousticResult<std::vector<double>>
deconvolveSourcePulse(ArrayView<const double> rir, double sampleRateHz,
                      double sigmaS, double t0S, double epsilon = 1e-6);

// 相補対の低域側 (線形位相・奇数長・DC 利得が厳密に 1)。
// 高域側は h_hp = δ[(N-1)/2] − h_lp で作れる (厳密に相補)。
AcousticResult<std::vector<double>>
designComplementaryLowpass(double sampleRateHz, double crossoverHz,
                           double transitionWidth);

// 低域 = FDTD、高域 = 幾何音響 の RIR を 1 本に合成する。
// 入力は各 1 本 (多チャンネルは先頭チャンネルを使い warning を付ける)。
// 出力 fs は幾何音響側に合わせる。
AcousticResult<AudioBuffer>
buildHybridRir(const AudioBuffer &fdtdRir, const AudioBuffer &gaRir,
               const HybridRirConfig &config, HybridRirInfo *outInfo = nullptr);

} // namespace acoustics
} // namespace ofd
