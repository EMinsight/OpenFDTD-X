// FocusedField.h — 集束超音波源の軸上音場・安全指標・非線形指標 (Qt 非依存 / C++14)
//
// 超音波タブ (src/tabs/UltrasoundTab.cpp) が表示する焦点音圧などの計算実体。
// GUI に式を書かず、selftest から解析解・極限値と直接突き合わせられるように
// ここへ集約する。
//
// 収録するものはすべて公刊の閉形式・定義式:
//   [1] H. T. O'Neil, "Theory of focusing radiators",
//       J. Acoust. Soc. Am. 21, 516-526 (1949).
//       球面集束開口 (spherical cap) の軸上音圧の厳密閉形式と、
//       幾何焦点での値 |p| = ρ c u0 k h (h = 開口の深さ)。
//   [2] M. F. Hamilton & D. T. Blackstock (eds.), "Nonlinear Acoustics",
//       Academic Press (1998), Ch. 1・4:
//       非線形係数 β = 1 + B/(2A)、平面進行波の衝撃形成距離
//       x_sh = 1/(β ε k) (ε = p/(ρc²))、Gol'dberg 数 Γ = 1/(α x_sh)
//       (L. Gol'dberg, Sov. Phys. Acoust. 3, 340 (1957))。
//   [3] IEC 62359:2010+AMD1:2017 / NEMA UD 2-2004:
//       機械指標 MI = p_r,α [MPa] / √(f_awf [MHz])。
//       p_r,α は 0.3 dB/cm/MHz でデレーティングした最大希薄音圧。
//   [4] T. L. Szabo, "Diagnostic Ultrasound Imaging: Inside Out", 2nd ed.,
//       Academic Press (2014), Ch. 4・6:
//       べき乗則吸収 α(f) = α0·f^y、
//       焦点の −6 dB (強度) 横方向幅 ≈ 1.028·λ·F#  (Airy パターンの半値全幅)。
//
// 前提 (UI 側にも明示すること):
//   - 線形音響 (小振幅) の Rayleigh 積分。非線形飽和は含まない。
//     Γ ≫ 1 のときは実際の焦点音圧はここで得る値より低くなる。
//   - 音源は開口全面が一様法線速度 u0 で振動する球面キャップ。
//   - 音響出力 → u0 の換算は放射インピーダンス ≈ ρc (ka ≫ 1) を仮定。
//   - 吸収は音源から焦点までの片道分を振幅に掛けるだけの単純減衰。
#pragma once

namespace ofd {
namespace acoustics {
namespace ultrasound {

// ── 媒質 ────────────────────────────────────────────────────────────────────
// 吸収はべき乗則 α(f) = alpha0_dBcmMHz · (f[MHz])^alphaExponent  [dB/cm]。
// bOverA < 0 は「B/A 不明」(非線形指標を計算しない) を表す。
struct Medium {
    double rho;              // 密度 ρ [kg/m³]
    double c;                // 音速 c [m/s]
    double alpha0_dBcmMHz;   // 吸収係数 α0 [dB/cm/MHz^y]
    double alphaExponent;    // べき乗則の指数 y (水 2.0 / 軟組織 ≈ 1.1)
    double bOverA;           // 非線形パラメータ B/A (負 = 不明)

    Medium()
        : rho(1000.0), c(1500.0), alpha0_dBcmMHz(0.0), alphaExponent(1.0),
          bOverA(-1.0) {}
};

// ── 音源 (球面集束開口) ─────────────────────────────────────────────────────
struct FocusedSource {
    double apertureRadius_m;  // 開口半径 a (a < R)
    double focalLength_m;     // 曲率半径 = 幾何焦点距離 R
    double frequency_Hz;      // 駆動周波数 f
    double power_W;           // 音響出力 (放射電力) W

    FocusedSource()
        : apertureRadius_m(0.032), focalLength_m(0.0626),
          frequency_Hz(1.0e6), power_W(150.0) {}
};

// 非線形の卓越度 (Gol'dberg 数による慣用的な区分 — [2])
enum NonlinearRegime {
    RegimeUnknown = 0,     // B/A 不明 → 判定しない
    RegimeQuasiLinear,     // Γ ≤ 0.1  吸収が支配的
    RegimeTransitional,    // 0.1 < Γ < 10
    RegimeShock            // Γ ≥ 10   衝撃波形成が支配的
};

struct FocusedFieldResult {
    bool   valid;                // 入力が有効か (a<R, f>0, ρ,c>0 …)
    double capHeight_m;          // 開口の深さ h = R − √(R²−a²)
    double capArea_m2;           // 開口面積 S = 2πRh
    double surfaceVelocity_mps;  // 一様法線速度 u0
    double surfaceIntensity_Wm2; // 開口面の平均強度 W/S
    double pressureGain;         // 焦点音圧利得 k·h (= p_focus /(ρ c u0))
    double focalPressureLossless_Pa;  // 無損失・線形の焦点音圧振幅
    double attenuation_dB;       // 音源→焦点の媒質減衰 [dB]
    double focalPressure_Pa;     // 媒質減衰込みの焦点音圧振幅 (線形)
    double focalIntensity_Wm2;   // I = p²/(2ρc)  (連続波の時間平均)
    double fNumber;              // F# = R/(2a)
    double beamWidth6dB_m;       // 焦点の −6 dB (強度) 横方向幅 ≈ 1.028 λ F#
    double mechanicalIndex;      // MI (IEC 62359 のデレーティング 0.3 dB/cm/MHz)
    bool   nonlinearValid;       // B/A が既知で非線形指標を出せるか
    double betaNonlinear;        // β = 1 + B/(2A)
    double machNumber;           // ε = p_focus /(ρc²)
    double shockDistance_m;      // x_sh = 1/(β ε k)
    // Γ = 1/(α x_sh) (α は Np/m)。α = 0 (無吸収) では Γ = ∞ となるため
    // 負値 (−1) を「∞」の表現として返す (GUI は "∞" と表示する)。
    double goldberg;
    int    regime;               // NonlinearRegime

    FocusedFieldResult()
        : valid(false), capHeight_m(0), capArea_m2(0), surfaceVelocity_mps(0),
          surfaceIntensity_Wm2(0), pressureGain(0), focalPressureLossless_Pa(0),
          attenuation_dB(0), focalPressure_Pa(0), focalIntensity_Wm2(0),
          fNumber(0), beamWidth6dB_m(0), mechanicalIndex(0),
          nonlinearValid(false), betaNonlinear(0), machNumber(0),
          shockDistance_m(0), goldberg(0), regime(RegimeUnknown) {}
};

// ── 基本量 ──────────────────────────────────────────────────────────────────
// 球面キャップの深さ h = R − √(R² − a²)。a ≥ R / 非正入力では 0。
double capHeight(double apertureRadius_m, double focalLength_m);
// 球面キャップの面積 S = 2πRh (厳密)。
double capArea(double apertureRadius_m, double focalLength_m);

// 音響出力 W から一様法線速度 u0 を求める: W = ½ ρ c u0² S  (放射抵抗 ≈ ρcS)。
double surfaceVelocity(const FocusedSource &src, const Medium &med);

// べき乗則吸収 α(f) を返す。単位はそれぞれ [dB/m] と [Np/m]。
double attenuation_dB_per_m(const Medium &med, double frequency_Hz);
double attenuation_Np_per_m(const Medium &med, double frequency_Hz);

// ── 軸上音圧 (O'Neil の厳密閉形式, 無損失・線形) ────────────────────────────
// 音源頂点から軸上距離 z [m] の点の音圧振幅 |p(z)|。u0 は法線速度振幅。
//   ζ = R − z (幾何焦点からの符号付き距離) として
//   |p| = 2 ρ c u0 (R/|ζ|)·|sin(k(r2−r1)/2)|,
//   r1 = |ζ|, r2 = √(R² + ζ² − 2Rζcosα),  a = R sinα
// ζ → 0 (焦点) で |p| → ρ c u0 k h に連続的に一致する。
double axialPressureAmplitude(const FocusedSource &src, const Medium &med,
                              double surfaceVelocity_mps, double z_m);

// 幾何焦点の音圧振幅 (無損失・線形): |p| = ρ c u0 k h  ([1])
double focalPressureLossless(const FocusedSource &src, const Medium &med,
                             double surfaceVelocity_mps);

// ── まとめて評価 ────────────────────────────────────────────────────────────
FocusedFieldResult evaluateFocus(const FocusedSource &src, const Medium &med);

// ── 文献値データベース (表示用) ─────────────────────────────────────────────
// 出典:
//   F. A. Duck, "Physical Properties of Tissue: A Comprehensive Reference
//   Book", Academic Press (1990), Ch. 4 (音速・密度・吸収)。
//   IT'IS Foundation, "Tissue Properties Database" V4.1 (2022)。
//   T. L. Szabo (2014) Table 4.1 (べき乗則の指数 y)。
//   B/A: Hamilton & Blackstock (1998) Table 1.1。
//   鋼 / CFRP: J. Krautkrämer & H. Krautkrämer, "Ultrasonic Testing of
//   Materials", 4th ed., Springer (1990), Appendix。
// id は GUI の表示名キーへ引き当てるための不変トークン (変更しないこと)。
struct MediumEntry {
    const char *id;
    Medium      medium;
};

// 生体 (医療イメージング / HIFU) 用 — 水・軟組織・脂肪・肝臓・骨
int                 bioMediumCount();
const MediumEntry & bioMedium(int index);
// NDT (非破壊検査) 用 — 鋼 (縦波/横波)・CFRP・水 (接触媒質)
int                 ndtMediumCount();
const MediumEntry & ndtMedium(int index);

// 特性音響インピーダンス Z = ρc [Pa·s/m] (1 MRayl = 1e6 Pa·s/m)
double acousticImpedance(const Medium &med);

} // namespace ultrasound
} // namespace acoustics
} // namespace ofd
