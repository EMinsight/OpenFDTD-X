// MonteCarlo.h — 製造ばらつきのサンプリングと結果統計 — Qt 非依存 / C++17
//
// ToleranceStats.h が扱うのは **入力変数の解析的な分布** で、性能 (FoM) の
// 分布は「各サンプルについてソルバーを回して初めて得られる」ものだった。
// ここはその不足を埋める 2 つの計算実体を持つ:
//
//   ① サンプリング — 入力分布から N 個の標本を作る (乱数 / ラテン超方格)
//   ② 結果統計     — 得られた FoM 標本から平均・σ・分位点・歩留まりを出す
//
// 乱数は決定論的 (seed を与えれば同じ標本列)。再現できない解析結果を出さない
// ため、および selftest が厳密な期待値と突き合わせられるようにするため。
//
// 出典:
//   [1] M. D. McKay, R. J. Beckman, W. J. Conover, "A Comparison of Three
//       Methods for Selecting Values of Input Variables in the Analysis of
//       Output from a Computer Code", Technometrics 21(2), 239-245 (1979).
//       — ラテン超方格 (LHS): 各変数の [0,1) を N 等分し、各層から 1 点ずつ
//         取って層ごとに独立に並べ替える。
//   [2] JCGM 100:2008 (GUM) §4.3.7 — 一様分布の標準偏差 a/√3。
//   [3] Johnson, Kotz, Balakrishnan, "Continuous Univariate Distributions",
//       Vol. 1, 2nd ed., Wiley (1994), Ch. 18 — Rayleigh の分位点。
#ifndef OFD_CORE_MONTECARLO_H
#define OFD_CORE_MONTECARLO_H

#include <cstdint>
#include <vector>

#include "ToleranceStats.h"

namespace ofd {
namespace montecarlo {

// サンプリング法。Sobol' 列は未実装 — 選ばせない (絶対規則 5)。
enum class Method { Random, Latin };

// 分位関数 Q(p) (0 < p < 1)。分布ごとの逆 CDF。
// 連続分布でなければ center を返す (標本が動かない = ばらつき無し)。
double quantile(const tolstat::Variable &v, double p);

// N 標本 × M 変数の行列 (row-major: sample i の変数 j は s[i*M + j])。
// vars が空、または n < 1 なら空を返す。
// Method::Latin は各変数について [0,1) を n 等分し、各層から 1 点を取って
// 層順を変数ごとに独立に並べ替える [1]。n が小さくても分布を覆える。
std::vector<double> sample(const std::vector<tolstat::Variable> &vars,
                           int n, Method method, std::uint64_t seed);

// FoM 標本の要約。
struct Stats {
    bool   valid = false;
    int    count = 0;
    double mean = 0.0;
    double stdDev = 0.0;      // 標本標準偏差 (n-1 で割る不偏推定)
    double min = 0.0, max = 0.0;
    double median = 0.0;
    double p3sigmaLo = 0.0;   // 0.135 % 分位 (±3σ に相当する被覆の下側)
    double p3sigmaHi = 0.0;   // 99.865 % 分位
};

// 有限値だけを使って要約する (失敗したサンプルは NaN で渡してよい)。
// 有限値が 2 個未満なら valid = false (σ が定義できない)。
Stats summarize(const std::vector<double> &fom);

// 合格判定の向き
enum class Goal { LessOrEqual, GreaterOrEqual };

// 歩留まり = 判定を満たす標本の割合 [0,1]。有限値のみを母数にする。
// 有限値が 0 個なら count = 0 を返し、割合は 0 (「歩留まり 0 %」ではなく
// 「母数なし」— 呼び出し側は count で区別する)。
struct Yield {
    int    count = 0;      // 母数 (有限値の数)
    int    pass = 0;
    double fraction = 0.0;
};
Yield yieldOf(const std::vector<double> &fom, double threshold, Goal goal);

// 経験累積分布から等幅ヒストグラムを作る (描画用)。
// bins < 1 か有限値が無ければ空。返すのは各ビンの [中心, 個数]。
struct Bin { double center = 0.0; double count = 0.0; };
std::vector<Bin> histogram(const std::vector<double> &fom, int bins);

} // namespace montecarlo
} // namespace ofd

#endif // OFD_CORE_MONTECARLO_H
