// ParetoFront.h — 2 目的の非劣解集合 (Pareto フロント) — Qt 非依存 / C++17。
//
// 最適化タブの「Pareto フロント」出力の計算実体。掃引や PSO / GA が回した
// 各点について**評価量を 2 つ**求めておき、片方を良くするともう片方が悪く
// なる境目 (トレードオフ) を取り出す。
//
// ── 支配関係 ────────────────────────────────────────────────────────────────
// 点 p が点 q を**支配する**とは、**両方の目的で p が q 以上**であり、かつ
// **少なくとも一方で真に良い**こと。どの点にも支配されない点が非劣解で、
// その集合が Pareto フロント。
//
// 目的の向き (大きいほど良い / 小さいほど良い) は `FomKind` ごとに決まって
// いるので、**呼び出し側で符号を反転させない** — maxA / maxB で渡す
// (`kernel/OptimizeFom` の `fomMaximizes()` の値をそのまま渡す)。
//
// ── 同値の扱い ──────────────────────────────────────────────────────────────
// 「両目的で完全に同じ」点どうしは互いに支配しないので、**どちらもフロントに
// 残る**。掃引では同じ点が 2 回出ることがあり、片方を落とすと「消えた点は
// 何だったのか」が分からなくなるため。
//
// ── 適用範囲 ────────────────────────────────────────────────────────────────
// 目的は 2 つまで。3 目的以上は支配関係の定義自体は同じだが、フロントの
// 提示 (作図) が別物になるのでここでは扱わない。
#pragma once
#include <vector>

namespace ofd {
namespace pareto {

struct Point {
    double a = 0.0, b = 0.0;
    bool   valid = false;   // 評価できなかった点は false (フロントに入れない)
};

// p が q を支配するか
bool dominates(const Point &p, const Point &q, bool maxA, bool maxB);

// 非劣解の添字 (**入力順**を保つ)。無効な点は除く。
std::vector<int> front(const std::vector<Point> &pts, bool maxA, bool maxB);

// フロントを目的 A について並べ替えた添字 (作図用)。
// 並べ替えた列では**目的 B が単調**になる (トレードオフの定義そのもの)。
std::vector<int> frontSortedByA(const std::vector<Point> &pts,
                                bool maxA, bool maxB);

// 参照点 (refA, refB) に対する 2 次元ハイパーボリューム。
// 「参照点よりどちらの目的でも良い」領域の面積で、フロントの良さを 1 個の
// 数にした指標。参照点より悪い点は寄与しない。
double hypervolume(const std::vector<Point> &pts, bool maxA, bool maxB,
                   double refA, double refB);

} // namespace pareto
} // namespace ofd
