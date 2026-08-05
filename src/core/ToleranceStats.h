// ToleranceStats.h — 製造ばらつきの入力分布 (解析形) — Qt 非依存 / C++17
//
// ToleranceTab (製造ばらつき・歩留まり解析) のうち、**モンテカルロを走らせ
// なくても決まる量** = 入力変数そのものの確率分布をここで計算する。
// GUI に式を直書きしないための計算実体で、tests/selftest.cpp から解析解
// (ピーク値・モーメント・被覆確率) と直接突き合わせる。
//
// ここに **性能 (FoM) の分布は無い**。FoM の分布・歩留まりは各サンプルに
// ついてソルバーを回して初めて得られるもので、本モジュールの対象外。
//
// 出典 (いずれも標準的な確率分布の定義):
//   [1] NIST/SEMATECH e-Handbook of Statistical Methods, §1.3.6.6
//       (Gallery of Distributions: Normal / Uniform / Rayleigh)。
//       https://doi.org/10.18434/M32189
//   [2] JCGM 100:2008 (GUM), §4.3.7 / §4.4.5 — 一様分布の標準偏差 a/√3、
//       正規分布の包含区間 (k=1,2,3 → 68.27 / 95.45 / 99.73 %)。
//   [3] N. L. Johnson, S. Kotz, N. Balakrishnan, "Continuous Univariate
//       Distributions", Vol. 1, 2nd ed., Wiley (1994), Ch. 18 (Rayleigh)。
#ifndef OFD_CORE_TOLERANCESTATS_H
#define OFD_CORE_TOLERANCESTATS_H

#include <vector>

namespace ofd {
namespace tolstat {

// 分布の種類。Discrete は「離散的な選択肢 (底質の砂/泥/岩 など)」で、
// 連続分布としては扱えない = 曲線も区間も出さない。
enum class Dist { Normal, Uniform, Rayleigh, Discrete };

// ばらつき変数 1 個。
//   Normal   : center = 平均 μ、spread = 標準偏差 σ
//   Uniform  : center = 中心、spread = 半幅 a (台は [center−a, center+a])
//   Rayleigh : center = 位置 (下限)、spread = 尺度 σ (台は [center, ∞))
//   Discrete : 連続分布ではない (両方とも未使用)
struct Variable {
    Dist   dist   = Dist::Normal;
    double center = 0.0;
    double spread = 0.0;
};

struct Point { double x = 0.0, y = 0.0; };
struct Interval { double lo = 0.0, hi = 0.0; };

// 連続分布として扱えるか (Discrete / spread <= 0 は false)
bool isContinuous(const Variable &v);

// 確率密度 f(x)。連続分布でない場合は 0。
double pdf(const Variable &v, double x);

// 標準偏差。Normal: σ、Uniform: a/√3 [2]、Rayleigh: σ·√(2 − π/2) [3]。
double stdDev(const Variable &v);

// 平均。Normal: μ、Uniform: center、Rayleigh: center + σ·√(π/2) [3]。
double mean(const Variable &v);

// 正規分布で ±kσ が覆う確率 P = erf(k/√2) ([2] §4.3.7)。k ≤ 0 は 0。
double normalCoverage(double k);

// 「正規分布の ±kσ と同じ被覆確率 P」を持つ中央被覆区間。
// 分布の形が違っても比較できるよう、確率で揃える (幅で揃えない)。
//   Normal   : [μ − kσ, μ + kσ]
//   Uniform  : [center − P·a, center + P·a]  (CDF が線形なので厳密)
//   Rayleigh : 分位点 Q((1−P)/2), Q((1+P)/2)、Q(p) = center + σ√(−2·ln(1−p))
// 連続分布でなければ {0, 0}。
Interval coverageInterval(const Variable &v, double k);

// 表示用の密度曲線 (x 昇順, n 点)。描画範囲は分布ごとに
//   Normal   : μ ± 4σ
//   Uniform  : center ± 1.25a (台の外側も少し含めて箱の縁を見せる)
//   Rayleigh : [center, center + 4σ]
// 連続分布でなければ空。
std::vector<Point> pdfCurve(const Variable &v, int n = 121);

} // namespace tolstat
} // namespace ofd

#endif // OFD_CORE_TOLERANCESTATS_H
