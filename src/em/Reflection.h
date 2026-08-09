// Reflection.h — 反射係数とスミスチャートの幾何 (Qt 非依存 / C++17)
//
// 給電点インピーダンス Zin = R + jX (カーネルの <kernel>.log 給電点表) から
// 反射係数 Γ・S11・VSWR を求め、スミスチャートを描くための円を返す。
// 描画側は src/widgets/PlotPanel.cpp。
//
// 定義 (時間因子 e^{jωt}、基準抵抗 Z0 は実数):
//   Γ = (Z − Z0) / (Z + Z0)                 … 電圧反射係数
//   S11 = Γ                                  … 1 ポート (給電点 1 個) では同一
//   |S11|[dB] = 20 log10 |Γ|
//   Return loss[dB] = −20 log10 |Γ|
//   VSWR = (1 + |Γ|) / (1 − |Γ|)
// いずれも教科書式 (Pozar, "Microwave Engineering", 4th ed., §2.3 / §4.3)。
//
// スミスチャート (Γ 平面の単位円) の目盛は円になる。z = Z/Z0 = r + jx として
//   等抵抗円     : 中心 (r/(1+r), 0)、半径 1/(1+r)
//   等リアクタンス円: 中心 (1, 1/x)、半径 |1/x|
// (Pozar 同 §2.4。x = 0 は実軸そのもので半径が発散するため円にしない。)
//
// **無限大の扱い**: 完全整合 (|Γ| = 0) の S11[dB] は −∞、全反射 (|Γ| ≥ 1) の
// VSWR は +∞ になる。丸めて有限値に見せると「整合していないのに整合して
// 見える」ため、std::numeric_limits<double>::infinity() をそのまま返す。
// 表示側は std::isfinite() で分岐すること。
#ifndef OFD_EM_REFLECTION_H
#define OFD_EM_REFLECTION_H

namespace ofd {
namespace em {

// Zin と Z0 から求まる反射量一式
struct Reflection {
    double gammaRe = 0;      // Re Γ
    double gammaIm = 0;      // Im Γ
    double magnitude = 0;    // |Γ|
    double phaseDeg = 0;     // ∠Γ [deg]  (−180 … 180)
    double s11Db = 0;        // 20 log10|Γ|      (|Γ| = 0 なら −∞)
    double returnLossDb = 0; // −20 log10|Γ|     (|Γ| = 0 なら +∞)
    double vswr = 0;         // (1+|Γ|)/(1−|Γ|)  (|Γ| ≥ 1 なら +∞)
    bool   valid = false;    // Z0 > 0 かつ Z + Z0 ≠ 0
};

// Γ = (Z − Z0)/(Z + Z0)。Z0 ≤ 0 や Z = −Z0 (分母 0) は valid = false。
Reflection reflectionFromZ(double r_ohm, double x_ohm, double z0_ohm);

// 逆変換 z = (1 + Γ)/(1 − Γ) → Z = z·Z0。Γ = 1 (分母 0) では false。
bool impedanceFromGamma(double gammaRe, double gammaIm, double z0_ohm,
                        double *r_ohm, double *x_ohm);

// スミスチャートの目盛円 (Γ 平面、単位円が |Γ| = 1)
struct SmithCircle {
    double cx = 0, cy = 0;   // 中心
    double radius = 0;
    bool   valid = false;
};

// 等抵抗円 r = R/Z0 (r ≥ 0)。r < 0 は受動でないので valid = false。
SmithCircle constantResistanceCircle(double rNorm);
// 等リアクタンス円 x = X/Z0 (x ≠ 0)。x = 0 は実軸なので valid = false。
SmithCircle constantReactanceCircle(double xNorm);

} // namespace em
} // namespace ofd

#endif // OFD_EM_REFLECTION_H
