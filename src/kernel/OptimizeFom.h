// OptimizeFom.h — 掃引結果から目的関数 (FoM) を評価する
//
// 最適化タブの「掃引」は `kernel/SweepRunner` が 1 点ずつカーネルを回し、
// 各点の `SweepResult` (給電点表 + 遠方界パターン) を返す。ここはその 1 点を
// **1 個のスカラー**に落とす部分で、どの点が良いかを決める唯一の場所。
//
// FoM は「大きいほど良い」ものと「小さいほど良い」ものが混ざるので、
// 種別ごとに `fomMaximizes()` で向きを持たせ、最良点の選択は
// `bestPointIndex()` に集約する。**向きを呼び出し側で書かない** —
// 書くと必ずどこかで逆になる。
//
// 値が取れない点 (カーネルが失敗した / その量を出していない) は
// `valid = false` にして**最良点の候補から外す**。0 や NaN を混ぜない。
#ifndef OFD_KERNEL_OPTIMIZEFOM_H
#define OFD_KERNEL_OPTIMIZEFOM_H

#include <QVector>

#include "SweepRunner.h"

namespace ofd {

enum class FomKind {
    MinReflectionDb,   // 給電点の反射 Ref[dB] (小さいほど良い)
    MinVswr,           // 給電点の VSWR       (小さいほど良い)
    MaxPeakGainDb,     // 遠方界 E-abs の最大 [dB] (大きいほど良い)
    MaxFrontToBackDb,  // 前後比 F/B [dB]      (大きいほど良い)
};

struct FomValue {
    double value = 0.0;
    bool   valid = false;
};

// 大きいほど良い FoM か
bool fomMaximizes(FomKind kind);

// 1 点の結果を評価する。freqHz > 0 ならその周波数に最も近い点を使い、
// 0 以下なら「その点で最も良い周波数」を採る (掃引の各点を最良条件で比べる)。
FomValue evaluateFom(FomKind kind, const SweepResult &r, double freqHz);

// 最良点の添字。有効な点が 1 つも無ければ -1。
// 同値のときは**先に出てきた点**を採る (掃引の順序で決まるので再現する)。
int bestPointIndex(FomKind kind, const QVector<FomValue> &values);

} // namespace ofd

#endif // OFD_KERNEL_OPTIMIZEFOM_H
