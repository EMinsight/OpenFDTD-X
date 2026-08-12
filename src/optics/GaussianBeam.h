// GaussianBeam.h — ガウシアンビームの伝搬 (Qt 非依存 / C++17)。
//
// 波動ソルバー (FDTD / BPM / RCWA) の結果を幾何光学の追跡へ引き渡すときの
// **橋渡し**。境界での界分布をそのまま光線束に置き換えることはできない
// (回折が落ちる) が、**等価なガウシアンビーム 1 本に落とせば** 回折の広がりを
// 保ったまま遠方まで解析的に運べる。ハイブリッド連携で「境界モード変換 →
// ガウシアンビーム」を選んだときの計算実体。
//
// ── 基本式 (すべて公刊の閉形式) ────────────────────────────────────────────
//
//   レイリー長      z_R = π w₀² n / λ
//   ビーム半径      w(z) = w₀ √(1 + (z/z_R)²)          (1/e² 強度半径)
//   波面曲率        R(z) = z (1 + (z_R/z)²)            (z = 0 で無限大)
//   遠方発散半角    θ = λ / (π w₀ n)                   (w(z) → θ·z)
//   Gouy 位相       ψ(z) = atan(z/z_R)
//   複素ビーム因子  1/q(z) = 1/R(z) − i λ / (π n w(z)²)
//
// 出典:
//   [1] A. E. Siegman, "Lasers", University Science Books (1986), Ch.16-17。
//   [2] H. Kogelnik and T. Li, "Laser beams and resonators",
//       Appl. Opt. 5(10), 1550-1567 (1966)。q パラメータと ABCD 則。
//   [3] ISO 11146-1:2021 — 2 次モーメント (D4σ) によるビーム径の定義と M²。
//
// ── 厳密に成り立つ恒等式 (検証はこれで行う) ────────────────────────────────
//
//   * w(z_R) = √2 w₀                (レイリー長でちょうど √2 倍)
//   * R(z) は z = z_R で最小 R = 2 z_R
//   * w₀ θ = λ / (π n)              ビームパラメータ積 — 伝搬の不変量
//   * 平行光を焦点距離 f のレンズで絞ると w₀' = λ f / (π n w)  (f ≫ z_R)
//   * 自由空間 ABCD と w(z) の式は同じ答えを出す
//
// ── 扱わないもの (絶対規則 5) ──────────────────────────────────────────────
//   * 非点収差ビーム (x と y で別の w₀) は扱わない — 軸対称の 1 本のみ。
//   * M² > 1 の実ビームは「w₀ を M² 倍に読み替える」近似で、ここでは
//     M² を入力として受け取るだけで伝搬式そのものは理想ビームのまま。
//   * 高次エルミート/ラゲール モードの重ね合わせは扱わない。
#ifndef OFD_OPTICS_GAUSSIANBEAM_H
#define OFD_OPTICS_GAUSSIANBEAM_H

#include <vector>

namespace ofd {
namespace gauss {

// レイリー長 z_R = π w₀² n / λ [m]。引数が非正なら 0。
double rayleighRange(double w0_m, double lambda_m, double n = 1.0);

// ビーム半径 w(z) [m] (1/e² 強度半径)
double beamRadius(double w0_m, double z_m, double lambda_m, double n = 1.0);

// 波面の曲率半径 R(z) [m]。z = 0 では平面波なので 0 を返す (無限大の代わり)。
double radiusOfCurvature(double z_m, double w0_m, double lambda_m, double n = 1.0);

// 遠方発散半角 θ [rad] = λ/(π w₀ n)
double divergence(double w0_m, double lambda_m, double n = 1.0);

// Gouy 位相 ψ(z) = atan(z/z_R) [rad]
double gouyPhase(double z_m, double w0_m, double lambda_m, double n = 1.0);

// ビームパラメータ積 w₀·θ [m·rad]。理想ビームでは λ/(π n) に厳密に等しい。
double beamParameterProduct(double w0_m, double lambda_m, double n = 1.0);

// 幾何光学が使える距離の目安。z ≫ z_R で w(z) → θ·z の直線に漸近するので、
// 「直線近似の誤差が tol 以下になる z」を返す。
//   w(z)/(θz) − 1 = √(1+(z_R/z)²) − 1 ≤ tol
// を解いて z = z_R / √((1+tol)² − 1)。tol <= 0 なら 0。
double geometricValidDistance(double w0_m, double lambda_m, double n = 1.0,
                              double tol = 0.01);

// ── ABCD 則 (Kogelnik & Li) ────────────────────────────────────────────────
// 薄肉レンズ f で絞ったあとのウエスト。入射側のウエスト w₀ がレンズから
// 距離 d にあるとき、出射側のウエスト半径と、レンズからウエストまでの距離を
// 返す。f <= 0 (発散レンズ) でも式は同じ。
struct Waist { double w0_m = 0.0, z_m = 0.0; };
Waist lensWaist(double w0_m, double d_m, double focal_m, double lambda_m,
                double n = 1.0);

// ── 界分布からウエストを求める (ISO 11146 の 2 次モーメント) ───────────────
// 等間隔に並んだ強度サンプル I(x) (中心が配列の中央) から D4σ 径を求め、
// その半分 (= 1/e² 半径に相当) を返す。総和が 0 なら 0。
// **強度 (|E|²) を渡すこと** — 振幅を渡すと √2 倍ずれる。
// **2 次モーメントは裾に敏感である。** 窓が足りずに裾を切ると必ず小さく出る
// (selftest で固定してある)。窓はビーム径の 3 倍以上を目安に取ること。
double waistFromIntensity(const std::vector<double> &intensity, double pitch_m);

} // namespace gauss
} // namespace ofd

#endif // OFD_OPTICS_GAUSSIANBEAM_H
