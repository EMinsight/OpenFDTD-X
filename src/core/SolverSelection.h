// SolverSelection.h — ソルバ選定の目安になる無次元量・特性周波数 (Qt 非依存 / C++17)
//
// SolverSelectorTab (ソルバ選択) が表示する「選定の目安」の計算実体。
// **プロジェクト設定 (寸法・メッシュ・周波数・室容積…) だけから決まる量**
// のみを扱い、ソルバーを走らせないと分からない量 (Q 値そのもの、実際の
// 伝搬損失など) はここでは扱わない。GUI に式を直書きしないための分離で、
// tests/selftest.cpp から手計算値・極限値と直接突き合わせる。
//
// 出典 (式はすべて公刊のもの):
//   [1] A. Taflove, S. C. Hagness, "Computational Electrodynamics: The
//       Finite-Difference Time-Domain Method", 3rd ed., Artech House (2005).
//       §4.7 (1 波長あたりのセル数と数値分散), §4.5 (Courant 安定条件)。
//   [2] M. R. Schroeder, "The 'Schroeder frequency' revisited",
//       J. Acoust. Soc. Am. 99, 3240-3241 (1996)。
//       f_c = 2000·sqrt(T/V) [Hz] (T [s], V [m³])。この周波数より上では
//       モード重なりが 3 を超え、統計/幾何音響が使える。
//   [3] R. J. Urick, "Principles of Underwater Sound", 3rd ed.,
//       McGraw-Hill (1983), §5.3 — Thorp の吸収係数の実験式。
//   [4] 離散フーリエ変換の周波数分解能 Δf = 1/T (時間長 T の観測)。
//       共振の半値全幅 f/Q を分解する条件 f/Q ≥ 1/T から Q ≤ f·T。
#ifndef OFD_CORE_SOLVERSELECTION_H
#define OFD_CORE_SOLVERSELECTION_H

namespace ofd {
namespace selsolver {

// 真空中の光速 [m/s] (CODATA)
constexpr double kC0 = 2.99792458e8;

// 波長 λ = v/f [m]。速度・周波数が非正なら 0 (未計算) を返す。
double wavelength(double speed_mps, double freq_hz);

// 電気的サイズ L/λ (無次元)。λ ≤ 0 / L < 0 なら 0。
double electricalSize(double length_m, double lambda_m);

// 1 波長あたりのセル数 λ/Δx (無次元)。[1] §4.7 の目安は 10〜20。
// Δx ≤ 0 / λ ≤ 0 なら 0。
double cellsPerWavelength(double lambda_m, double dx_m);

// 時間長 T [s] の時間領域解析で分解できる Q の上限 Q ≤ f·T ([4])。
// これは「その実行で共振の鋭さを識別できる上限」であって、構造の Q の
// 推定値ではない。f ≤ 0 / T ≤ 0 なら 0。
double maxResolvableQ(double freq_hz, double duration_s);

// Schroeder 周波数 f_c = 2000·sqrt(T60/V) [Hz] ([2])。
// これ未満は波動論 (FDTD/モーダル)、以上は統計/幾何音響の領域。
// T60 ≤ 0 / V ≤ 0 なら 0。
double schroederFrequency(double rt60_s, double volume_m3);

// 海水の吸収係数 [dB/km] (Thorp の実験式, f は kHz) ([3]):
//   α = 0.11 f²/(1+f²) + 44 f²/(4100+f²) + 2.75e-4 f² + 0.003
// 適用範囲は概ね 0.1〜100 kHz、水温 ~4 degC・深度数百 m の代表値。
// f < 0 なら 0。
double thorpAbsorption_dBkm(double freq_kHz);

// 球面拡散 + 吸収による伝搬損失 TL = 20·log10(r[m]) + α·r[km] [dB]。
// 距離 ≤ 0 なら 0。
double sphericalTransmissionLoss_dB(double range_km, double alpha_dBkm);

} // namespace selsolver
} // namespace ofd

#endif // OFD_CORE_SOLVERSELECTION_H
