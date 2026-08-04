// EmcStandards.h — EMC 規格の公表限度値と対策効果の古典式 (Qt 非依存 / C++17)
//
// EMC/EMI タブ (src/tabs/EmcTab.cpp) が表示する数値の実体。GUI に規格値や式を
// 直書きせず、selftest から公表値・定義式と直接突き合わせられるようにここへ
// 集約する。
//
// 収録するもの:
//   (a) 放射妨害波の限度値 (**規格の公表値そのもの**)
//       - CISPR 32:2015 / EN 55032 Table A.3 (Class A) / A.4 (Class B)
//         30 MHz–1 GHz、準尖頭値 (QP)、規定測定距離 10 m。
//       - FCC 47 CFR §15.109(a) Class B (3 m) / (b) Class A (10 m)。
//         規格は電界強度 [μV/m] で規定されており、ここで dBμV/m に換算する。
//       - 距離換算は逆距離則 E(d) = E(d_ref) + 20log10(d_ref/d)
//         (CISPR 16-2-3 / ANSI C63.4-2014 が認める外挿。実サイトでは
//          グランド反射と近傍界の影響で誤差が出るため目安)。
//   (b) 対策効果の古典式
//       - 平面波に対する金属シールドの遮蔽効果 SE = A + R + B
//         S. A. Schelkunoff, "Electromagnetic Waves", Van Nostrand (1943);
//         H. W. Ott, "Electromagnetic Compatibility Engineering",
//         Wiley (2009), §6.4 (eq. 6-9, 6-11, 6-12)。
//       - 開口 (スリット) による遮蔽効果 SE = 20log10(λ/2L) − 10log10(n)
//         Ott (2009) §6.7 (eq. 6-33)。L は開口の最長寸法 (λ/2 共振長)。
//       - 直列素子 (フェライトコア / コモンモードチョーク) の挿入損失
//         IL = 20log10|1 + Z/(Z_source+Z_load)| — 挿入損失の定義
//         (CISPR 17:2011 §4、Ott (2009) §5.1)。
//   (c) IEC 61000-4-2:2008 Table 2 の接触放電電流 (公表値)
//       第 1 ピーク 3.75 A/kV、30 ns 2 A/kV、60 ns 1 A/kV。
//
// 適用範囲 (UI にも明示すること):
//   - 限度値は「規格が定める上限」であって被測定値ではない。判定には実測
//     または解析で得たエミッションレベルが要る (このリポジトリの GUI からは
//     まだ得られない)。
//   - SE の式は平面波・無限平板の 1 次元問題。実筐体では開口・継ぎ目・
//     ケーブル貫通が支配的になる。
//   - 挿入損失の式は理想素子 (寄生容量・自己共振なし) を仮定する。
#ifndef OFD_EM_EMCSTANDARDS_H
#define OFD_EM_EMCSTANDARDS_H

namespace ofd {
namespace em {
namespace emc {

constexpr double kC0 = 2.99792458e8;      // 真空中の光速 [m/s]
constexpr double kMu0 = 1.25663706212e-6; // 真空の透磁率 [H/m] (CODATA 2018)
constexpr double kSigmaCu = 5.80e7;       // 焼鈍銅の導電率 [S/m] (IACS 100%)
constexpr double kZ0 = 376.730313668;     // 自由空間の波動インピーダンス [Ω]

// ── (a) 放射妨害波の限度値 ──────────────────────────────────────────────────
// 限度値表を収載している規格。ここに無い規格 (CISPR 25 / IEC 61000-4-3 /
// IEC 61000-4-2 / DO-160 / MIL-STD-461) は None を返し、GUI は「限度値表
// 未収載」と表示する (数値を捏造しない)。
enum class Standard {
    None = 0,
    Cispr32,   // CISPR 32:2015 / EN 55032 (マルチメディア機器)
    Fcc15      // FCC 47 CFR §15.109 (unintentional radiators)
};

enum class EmClass { A = 0, B = 1 };

// 限度値の 1 区間。適用範囲は f1_MHz < f ≤ f2_MHz (規格の境界の扱いに合わせ、
// 境界周波数では厳しい方 = 下の区間の値を採る)。
struct LimitSegment {
    double f1_MHz = 0;          // 区間下限 [MHz]
    double f2_MHz = 0;          // 区間上限 [MHz]
    double limit_dBuVm = 0;     // 限度値 [dBμV/m] @ refDist_m
    double refDist_m = 10.0;    // 規格が定める測定距離 [m]
};

constexpr int kMaxLimitSegments = 8;

// 規格 s / クラス c の放射妨害波限度値を out[0..n) へ書き、件数 n を返す。
// 収載が無ければ 0 (out は書かない)。
int radiatedLimits(Standard s, EmClass c, LimitSegment *out, int max);

// 逆距離則による測定距離換算 [dBμV/m]。d_m ≤ 0 なら規定距離の値をそのまま返す。
double limitAtDistance(const LimitSegment &seg, double d_m);

// 周波数 f_MHz に適用される区間の添字を返す (該当なしは -1)。
int limitSegmentIndex(const LimitSegment *seg, int n, double f_MHz);

// ── (b) 金属シールドの遮蔽効果 (平面波) ─────────────────────────────────────
// 代表的なシールド材の電気定数 (Ott (2009) Table 6-1 の相対導電率・
// 相対透磁率)。μr は低周波の代表値で、高周波では低下する。
struct ShieldMaterial {
    double sigmaRel;   // 銅を 1 とした相対導電率
    double muRel;      // 比透磁率
};

constexpr int kShieldMaterialCount = 5;
// 0=銅 1=アルミニウム 2=鋼(低炭素) 3=ステンレス(SUS304) 4=パーマロイ(ミューメタル)
const ShieldMaterial &shieldMaterial(int index);

struct ShieldSE {
    double skinDepth_m = 0;    // 表皮深さ δ = 1/√(πfμσ)
    double absorption_dB = 0;  // 吸収損 A = 8.686 t/δ
    double reflection_dB = 0;  // 反射損 R = 168 + 10log10(σr/μr f)
    double multiRefl_dB = 0;   // 多重反射補正 B (薄い板では負)
    double total_dB = 0;       // SE = A + R + B
    bool   valid = false;      // 入力が有効か (無効なら全て 0)
};

// 平面波 (遠方界) に対する無限平板のシールド効果。
//   f_Hz > 0, thickness_m > 0, sigmaRel > 0, muRel > 0 でないと valid=false。
ShieldSE shieldEffectiveness(double f_Hz, double thickness_m,
                             double sigmaRel, double muRel);

// 開口 (最長寸法 longest_m) が count 個ある面の遮蔽効果 [dB]。
// SE = 20log10(λ/2L) − 10log10(n)、λ/2 以上の開口では 0 (遮蔽なし)。
// 入力が無効 (f ≤ 0 / L ≤ 0 / n < 1) なら 0 を返す。
double apertureSE_dB(double f_Hz, double longest_m, int count);

// ── (c) 直列素子の挿入損失 / ESD 電流 / 電力密度 ─────────────────────────────
// IL = 20log10(1 + Z_series/Z_circuit) [dB]。Z_circuit ≤ 0 なら 0。
double insertionLoss_dB(double zSeries_ohm, double zCircuit_ohm);

// 理想インダクタのリアクタンス |X| = 2πfL [Ω] (f, L ≤ 0 なら 0)。
double inductiveReactance(double f_Hz, double l_H);

// 寸法比 ratio = L_after/L_before の開口を縮めたときの遮蔽効果の改善量 [dB]。
// ΔSE = 20log10(1/ratio) (λ/2 未満の開口では SE ∝ 20log10(1/L))。
// ratio が 0 以下または 1 以上なら 0 (改善なし)。
double apertureShrinkGain_dB(double ratio);

// IEC 61000-4-2:2008 Table 2 の接触放電電流 (試験電圧に比例)。
struct EsdContactCurrent {
    double firstPeak_A = 0;   // 第 1 ピーク (3.75 A/kV, ±15%)
    double at30ns_A = 0;      // 30 ns 時点 (2 A/kV, ±30%)
    double at60ns_A = 0;      // 60 ns 時点 (1 A/kV, ±30%)
};
EsdContactCurrent esdContactCurrent(double kV);

// 平面波の電力密度 S = E_rms²/Z0 [W/m²]
double powerDensity_Wm2(double eRms_Vm);

// IEC 61000-4-3 の 80% AM (1 kHz) 変調時の尖頭包絡線電界 = 1.8 × 無変調試験レベル
double amModulatedPeakField(double eRms_Vm);

} // namespace emc
} // namespace em
} // namespace ofd

#endif // OFD_EM_EMCSTANDARDS_H
