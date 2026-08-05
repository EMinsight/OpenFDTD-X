// PlasmaDispersion.h — 自由キャリア (プラズマ) 分散による屈折率・吸収変化
//                      — Qt 非依存 / C++17
//
// 連成解析タブ (src/tabs/MultiphysicsTab.cpp) の「プラズマ効果 (Drude)」節が
// 表示する Δn / Δα の計算実体。GUI に式を書かず、selftest から解析解・極限値と
// 直接突き合わせられるようにここへ集約する。
//
// 収録する式はすべて公刊のもの:
//   [1] R. A. Soref and B. R. Bennett, "Electrooptical effects in silicon",
//       IEEE J. Quantum Electron. QE-23(1), 123-129 (1987).
//       - Drude 型の閉形式 (式 (1),(2)):
//           Δn = −(e²λ²)/(8π²c²ε0 n)·(ΔN/m_ce* + ΔP/m_ch*)
//           Δα =  (e³λ²)/(4π²c³ε0 n)·(ΔN/(m_ce*²μ_e) + ΔP/(m_ch*²μ_h))
//       - Si の実測フィット (λ = 1.55 μm):
//           Δn = −[8.8e-22·ΔN + 8.5e-18·(ΔP)^0.8]
//           Δα =   8.5e-18·ΔN + 6.0e-18·ΔP        (ΔN,ΔP は cm⁻³, Δα は cm⁻¹)
//       - 同 (λ = 1.3 μm):
//           Δn = −[6.2e-22·ΔN + 6.0e-18·(ΔP)^0.8]
//           Δα =   6.0e-18·ΔN + 4.0e-18·ΔP
//   [2] P. Drude, Ann. Phys. 306, 566 (1900) / N. W. Ashcroft & N. D. Mermin,
//       "Solid State Physics" (1976) Ch. 1:
//           ε(ω) = ε∞ − ω_p²/(ω² + iωγ),  ω_p² = N e²/(ε0 m*)
//       小摂動では Δε = −ω_p²/ω²、Δn = Δε/(2n) = −ω_p²/(2 n ω²)。
//
// 適用範囲 (UI にも明示すること):
//   - Drude 式は自由キャリアのみを扱う。バンドギャップ近傍のバンドフィリング
//     (Burstein-Moss) やバンドギャップ収縮は含まない。
//   - Soref-Bennett の実測フィットは結晶 Si・室温・記載波長でのみ有効。
//   - Drude の Δα に直流移動度 μ を入れると、光周波数での実効散乱時間が
//     直流より短いため吸収を過小評価する (Si・1.3〜1.55 μm で実測比 ≈ 1/20)。
//     Si では [1] の実測フィットを優先すること。Δn 側は両者が同程度 (±40 %)。
//   - いずれも「材料の光学定数モデル」であり、FDTD/CHARGE の連成計算ではない。
#ifndef OFD_OPTICS_PLASMADISPERSION_H
#define OFD_OPTICS_PLASMADISPERSION_H

namespace ofd {
namespace optics {

// ── 物理定数 (CODATA 2018) ──────────────────────────────────────────────────
constexpr double kElementaryCharge = 1.602176634e-19;   // e [C]
constexpr double kElectronMass     = 9.1093837015e-31;  // m_e [kg]
constexpr double kVacuumPermittivity = 8.8541878128e-12;// ε0 [F/m]
constexpr double kSpeedOfLight     = 2.99792458e8;      // c [m/s]

// プラズマ周波数 ω_p = √(N e² /(ε0 m* m_e))  [rad/s]
//   density_m3   : キャリア密度 N [m⁻³]
//   effectiveMass: 有効質量比 m*/m_e (Si の電子 0.26、正孔 0.39 が慣用値)
double plasmaAngularFrequency(double density_m3, double effectiveMass);

// Drude の比誘電率 ε(ω) = ε∞ − ω_p²/(ω² + iωγ)。
// 時間因子 exp(−iωt) 規約 (損失は正の虚部)。γ = 1/τ は散乱レート [rad/s]。
struct ComplexEps { double re = 0.0; double im = 0.0; };
ComplexEps drudePermittivity(double epsInf, double omega_rad_s,
                             double omegaP_rad_s, double gamma_rad_s);

// ── Drude の自由キャリア屈折率・吸収変化 ────────────────────────────────────
struct CarrierState {
    double deltaN_cm3 = 0.0;   // 電子密度の増分 ΔN [cm⁻³]
    double deltaP_cm3 = 0.0;   // 正孔密度の増分 ΔP [cm⁻³]
    double meffElectron = 0.26;// m_ce*/m_e (c-Si)
    double meffHole     = 0.39;// m_ch*/m_e (c-Si)
    double muElectron_cm2Vs = 1350.0;  // 電子移動度 [cm²/(V·s)] (c-Si, 室温)
    double muHole_cm2Vs     = 480.0;   // 正孔移動度 [cm²/(V·s)]
};

struct PlasmaResult {
    bool   valid = false;
    double omega_rad_s = 0.0;       // 角周波数 ω = 2πc/λ
    double omegaP_e_rad_s = 0.0;    // 電子のプラズマ周波数
    double omegaP_h_rad_s = 0.0;    // 正孔のプラズマ周波数
    double deltaN_index = 0.0;      // Δn (負 = 屈折率低下)
    double deltaAlpha_per_cm = 0.0; // Δα [cm⁻¹]
    double deltaAlpha_dB_per_cm = 0.0;  // Δα [dB/cm] = 4.343·Δα[cm⁻¹]
};

// Drude モデルによる Δn / Δα ([1] 式 (1),(2) = [2] の小摂動展開)。
// lambda_nm ≤ 0 / nBackground ≤ 0 / 負のキャリア密度では valid = false。
PlasmaResult drudeFreeCarrier(double lambda_nm, double nBackground,
                              const CarrierState &carriers);

// ── Soref-Bennett の c-Si 実測フィット ──────────────────────────────────────
enum class SorefBennettBand { Lambda1550nm, Lambda1310nm };

// 与えた波長に最も近いフィット波長を返す (1.43 μm を境に切り替え)。
SorefBennettBand nearestSorefBennettBand(double lambda_nm);

// 実測フィット式による Δn / Δα ([1])。lambda_nm は帯の選択にのみ使う。
// 負のキャリア密度では valid = false。
PlasmaResult sorefBennettSilicon(double lambda_nm, double deltaN_cm3,
                                 double deltaP_cm3);

// 与えた波長がフィットの有効範囲 (±5 %) 内か。範囲外なら GUI は
// 「外挿である」旨を表示する。
bool sorefBennettApplicable(double lambda_nm);

} // namespace optics
} // namespace ofd

#endif // OFD_OPTICS_PLASMADISPERSION_H
