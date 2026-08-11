// MeshRefine.h — 細分化領域 → 非均一メッシュ (Qt Core のみ / C++17)
//
// ジオメトリタブの「細分化領域」表を、実際の格子 (xmesh / ymesh / zmesh) へ
// 適用する。**新しい .ofd キーは要らない** — 本家の `xmesh = x0 d1 x1 d2 …`
// は元から非均一を表せるので、区間を割って分割数を増やすだけで済む。
// (サブグリッド法や AMR は別の仕組みが要るので、ここでは扱わない。)
//
// 手順は 1 軸ずつ:
//   1. 既存の節点に、領域の下限・上限を **節点として挿入**する
//      (領域の境目でセルサイズが切り替わるようにするため)
//   2. 各区間について、元の区間のセル幅を保ったまま分割数を決め直す
//   3. 領域の内側にある区間は分割数を ratio 倍する
//
// **比率が 1 の領域、無効な領域、範囲外の領域は何も変えない**。
// 細分化を掛けていない軸は入力の MeshAxis をそのまま返す (ビット等価) —
// 「有効にしていない機能は出力を 1 バイトも変えない」ため。
//
// 隣り合う区間のセル幅が急に変わると、そこで数値反射が起きる。どれだけ
// 変わったかは `maxStepRatio` で返すので、画面で警告できる (FDTD の実務では
// 隣接セル比 2 倍以内が目安)。
#ifndef OFD_CORE_MESHREFINE_H
#define OFD_CORE_MESHREFINE_H

#include <QVector>

#include "MeshAxis.h"

namespace ofd {

// 1 軸ぶんの細分化区間 (Project::RefineRegion から軸ごとに取り出したもの)
struct RefineSpan {
    double lo = 0.0, hi = 0.0;
    double ratio = 1.0;
};

struct MeshRefineResult {
    bool     valid = false;
    MeshAxis axis;                 // 細分化後の格子
    int      cellsBefore = 0;
    int      cellsAfter = 0;
    double   minSpacingBefore = 0.0;
    double   minSpacingAfter = 0.0;
    // 隣り合う区間のセル幅の比 (大きい方 / 小さい方) の最大値。
    // 1.0 = 一様。2 を超えると数値反射が無視できなくなる (実務の目安)。
    double   maxStepRatio = 1.0;
    int      spansApplied = 0;     // 実際に効いた区間の数
};

// axis へ spans を適用する。spans が空 / 全て ratio = 1 / 全て範囲外なら
// 入力をそのまま返す (cellsAfter == cellsBefore)。
// axis が不正 (isValid() == false) なら valid = false。
MeshRefineResult refineAxis(const MeshAxis &axis,
                            const QVector<RefineSpan> &spans);

} // namespace ofd

#endif // OFD_CORE_MESHREFINE_H
