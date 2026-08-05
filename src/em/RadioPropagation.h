// RadioPropagation.h — 電波伝搬リンクバジェットの確立モデル (Qt 非依存 / C++17)
//
// ChannelTab (電波伝搬・チャネル解析) が表示する数値の計算実体。
// **見通し内 (LOS) の解析モデルだけ**を扱う。建物透過・散乱・多重反射を含む
// チャネル (RMS 遅延スプレッド・角度スプレッドなど) はレイトレース /  FDTD の
// 実行が必要で、ここでは扱わない (GUI 側で「未計算」と表示する)。
//
// 出典 (式はすべて公刊のもの):
//   [1] H. T. Friis, "A note on a simple transmission formula",
//       Proc. IRE 34, 254-256 (1946)。自由空間基本伝送損
//       L_fs = (4πd/λ)²。
//   [2] T. S. Rappaport, "Wireless Communications: Principles and Practice",
//       2nd ed., Prentice Hall (2002), §4.6 (two-ray ground reflection model)
//       と §4.11.3 (経路損失指数)。
//   [3] ITU-R P.525-4 (2019) — 自由空間伝搬の基本式
//       L[dB] = 32.44 + 20log10(f[MHz]) + 20log10(d[km])。
//   [4] C. E. Shannon, "A mathematical theory of communication",
//       Bell Syst. Tech. J. 27, 379-423 (1948)。C = B·log2(1+S/N)。
//   [5] IEEE Std 1149 系で慣用の標準雑音温度 T0 = 290 K。
//       雑音電力 N = kT0B (k = 1.380649e-23 J/K, CODATA 2018)。
//
// 適用範囲 (GUI にも明示すること):
//   - two-ray モデルは「平面大地・完全反射 (Γ = −1)・等方アンテナ」を仮定した
//     2 波の干渉。市街地の建物回折や屋内の壁透過は含まない。
//   - 干渉のヌル点では損失が発散するので、損失には上限 (kMaxPathLossDb) を置く。
#ifndef OFD_EM_RADIOPROPAGATION_H
#define OFD_EM_RADIOPROPAGATION_H

namespace ofd {
namespace em {
namespace propagation {

constexpr double kC0 = 2.99792458e8;        // 真空中の光速 [m/s]
constexpr double kBoltzmann = 1.380649e-23; // ボルツマン定数 [J/K] (CODATA 2018)
constexpr double kT0 = 290.0;               // 標準雑音温度 [K] ([5])
constexpr double kMaxPathLossDb = 300.0;    // 干渉ヌルの発散を切る上限

// 波長 λ = c/f [m]。f ≤ 0 なら 0。
double wavelength(double freq_hz);

// 自由空間基本伝送損 L = 20·log10(4πd/λ) [dB] ([1][3])。
// 距離・周波数が非正なら 0。d < λ/4π (近傍界) でも式は評価するが、
// 遠方界の式であることに注意 (GUI 側で距離の下限を案内する)。
double freeSpacePathLossDb(double dist_m, double freq_hz);

// 2 波モデル (直接波 + 大地反射波) の経路損失 [dB] ([2] §4.6)。
//   P_r/P_t = (λ/4π)²·|e^{-jk·d1}/d1 + Γ·e^{-jk·d2}/d2|²
//   d1 = sqrt(d² + (ht−hr)²)、d2 = sqrt(d² + (ht+hr)²)
// reflection は反射係数の**大きさ** (完全反射 = 1)。位相は π (Γ = −|Γ|) と
// する (水平偏波の grazing 入射)。アンテナ利得は 0 dBi。
double twoRayPathLossDb(double dist_m, double hTx_m, double hRx_m,
                        double freq_hz, double reflection = 1.0);

// ブレークポイント距離 d_bp = 4·ht·hr/λ [m] ([2] §4.6)。
// これより遠方では 2 波モデルの損失が距離の 4 乗 (n = 4) で増える。
double breakpointDistance(double hTx_m, double hRx_m, double freq_hz);

// 2 点の経路損失から求める局所の経路損失指数
//   n = (L2 − L1) / (10·log10(d2/d1))
// 自由空間なら厳密に 2。d1, d2 が非正または等しいときは 0。
double pathLossExponent(double loss1Db, double d1_m, double loss2Db, double d2_m);

// 2 波モデルのライシアン K ファクタ [dB]
//   K = (直接波電力)/(反射波電力) = (d2/(d1·|Γ|))²
// 完全反射・遠距離では d1 ≈ d2 なので 0 dB に漸近する。
double twoRayKFactorDb(double dist_m, double hTx_m, double hRx_m,
                       double reflection = 1.0);

// 2 波モデルの遅延差 τ = (d2 − d1)/c [s]。
// これは 2 波だけの余剰遅延であり、実チャネルの RMS 遅延スプレッドではない。
double twoRayExcessDelay(double dist_m, double hTx_m, double hRx_m);

// 受信電力 [dBm] = EIRP[dBm] − 経路損失[dB] + 受信利得[dBi]
double receivedPowerDbm(double eirpDbm, double pathLossDb, double rxGainDbi);

// 熱雑音電力 [dBm] = 10·log10(k·T0·B / 1 mW) + NF ([5])
// B ≤ 0 なら 0。
double thermalNoiseDbm(double bandwidth_hz, double noiseFigureDb);

// Shannon 容量 [bit/s] = B·log2(1 + SNR) ([4])。B ≤ 0 なら 0。
// SISO (単一入出力) の上限であり、MIMO の多重利得は含まない。
double shannonCapacity(double bandwidth_hz, double snrDb);

} // namespace propagation
} // namespace em
} // namespace ofd

#endif // OFD_EM_RADIOPROPAGATION_H
