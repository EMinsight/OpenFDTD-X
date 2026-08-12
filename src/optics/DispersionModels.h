// DispersionModels.h — 分散モデルの閉形式 (Lorentz / Sellmeier) — Qt 非依存。
//
// 光タブの「分散モデル」の選択 (Drude / Lorentz / Sellmeier) を実際の曲線に
// する部分。**Drude は既にある** (`optics/PlasmaDispersion` の
// `drudePermittivity()`) ので、ここには足さない — 同じ物理の実装を 2 つ持つと
// 必ず食い違う。ここは残る 2 つと、3 つを同じ土俵に載せる換算だけを持つ。
//
// ── 時間因子は exp(−iωt) ──────────────────────────────────────────────────
// リポジトリ全体の規約に合わせる (`OpenRCWA/.claude/rules` と同じ)。
// **損失は誘電率の正の虚部**。負の虚部は利得になるので、符号を反転させない。
//
// ── 式と出典 ──────────────────────────────────────────────────────────────
//
//   Lorentz    ε(ω) = ε∞ + Δε·ω₀² / (ω₀² − ω² − iγω)
//              [1] C. F. Bohren and D. R. Huffman, "Absorption and Scattering
//                  of Light by Small Particles", Wiley (1983), §9.1。
//              ε(0) = ε∞ + Δε、ε(∞) = ε∞、ω = ω₀ で Re ε = ε∞。
//
//   Sellmeier  n²(λ) = 1 + Σ Bᵢλ² / (λ² − Cᵢ)          (λ は μm、Cᵢ は μm²)
//              [2] W. Sellmeier, Ann. Phys. Chem. 219, 272 (1871)。
//              λ → ∞ で n² → 1 + ΣBᵢ。λ² = Cᵢ が極 (吸収線) で、
//              **透明域の式なので極の近くでは使えない** (虚部を持たない)。
//
//   群屈折率   n_g = n − λ dn/dλ                       [3] 定義そのもの
//              (Sellmeier は解析微分できるので数値差分を使わない)
//
// ── 扱わないもの (絶対規則 5) ──────────────────────────────────────────────
//   * Sellmeier は**吸収を持たない** (実数の n だけ)。吸収帯に入る波長では
//     この式を使ってはいけない — 呼び出し側で範囲を示すこと。
//   * 温度・応力依存は扱わない (材料タブの `MaterialDispersion` の担当)。
#ifndef OFD_OPTICS_DISPERSIONMODELS_H
#define OFD_OPTICS_DISPERSIONMODELS_H

#include <vector>

namespace ofd {
namespace disp {

struct Complex { double re = 0.0, im = 0.0; };

// ── Lorentz ────────────────────────────────────────────────────────────────
// epsInf: ε∞、deltaEps: Δε (振動子強度)、omega0/gamma: [rad/s]
Complex lorentzPermittivity(double epsInf, double deltaEps,
                            double omega0_rad_s, double gamma_rad_s,
                            double omega_rad_s);

// ── Sellmeier ──────────────────────────────────────────────────────────────
// 項ごとの (B, C)。C は μm²。
struct SellmeierTerm { double b = 0.0, c_um2 = 0.0; };

// n(λ)。n² が負になる (極の内側) ときは 0 を返す — 虚数を実数として返さない。
double sellmeierIndex(const std::vector<SellmeierTerm> &terms, double lambda_um);
// dn/dλ [1/μm] (解析微分)
double sellmeierIndexSlope(const std::vector<SellmeierTerm> &terms,
                           double lambda_um);
// 群屈折率 n_g = n − λ dn/dλ
double sellmeierGroupIndex(const std::vector<SellmeierTerm> &terms,
                           double lambda_um);
// λ → ∞ の極限 n² → 1 + ΣB (透明域の下限)
double sellmeierLongWaveIndex(const std::vector<SellmeierTerm> &terms);

// ── 換算 ───────────────────────────────────────────────────────────────────
// 比誘電率 → 複素屈折率 n + ik (時間因子 exp(−iωt)、k ≥ 0)
Complex indexFromPermittivity(const Complex &eps);
// 波長 [μm] → 角周波数 [rad/s]
double angularFrequency(double lambda_um);

} // namespace disp
} // namespace ofd

#endif // OFD_OPTICS_DISPERSIONMODELS_H
