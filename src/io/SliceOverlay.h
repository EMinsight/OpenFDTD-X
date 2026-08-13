// SliceOverlay.h — 物体形状・観測点を「断面上の位置」へ投影する (描画の計算側)。
//
// H5アニメの場マップに形状と観測点を重ねるための計算。**描画側は塗るだけ**に
// して、どこに何が写るかの判断はここに閉じ込める (io/VoxelSlice と同じ分け方)。
//
// ── 断面と交わらないものは描かない ────────────────────────────────────────
// いちばん大事なのはここ。z = 0.01 m の断面を見ているのに z = 0.05 m にある
// 物体の輪郭が重なって見えたら、**その断面に無いものを在るように見せる**
// ことになる。交わらなければ false を返し、呼び出し側は何も描かない。
//
// 点 (観測点) は面積を持たないので、固定軸の座標が断面からどれだけ離れて
// いたら「この断面に居ない」と見なすかを呼び出し側が tol で決める
// (断面 1 枚の厚み = セル幅の半分を渡すのが自然)。
//
// ── 座標系 ────────────────────────────────────────────────────────────────
// 面内 2 軸 (u = 列, v = 行) の対応は H5Reader::seriesSliceAxes が唯一の出所。
// 返す値は 0..1 に正規化した位置で、**v は上から下へ数える** (v = 0 が
// vMax 側 = 行 0)。場マップの行 0 が第 2 軸の + 側という規約に合わせてある。
// 表示範囲からはみ出す部分は 0..1 へ丸める (画面外まで描かないため)。
#pragma once
#include "../core/Geometry.h"

namespace ofd {

struct SliceRectNorm {
    double u0 = 0.0, v0 = 0.0, u1 = 0.0, v1 = 0.0;   // 0..1 (v は上から)
};

struct SlicePointNorm {
    double u = 0.0, v = 0.0;                          // 0..1 (v は上から)
};

// 直方体 (shape = 1) を断面へ投影する。断面と交わらない / 引数が縮退して
// いる場合は false (out は触らない)。
bool boxOnSlice(const Geometry &g, int axis, double sliceCoord,
                double uMin, double uMax, double vMin, double vMax,
                SliceRectNorm *out);

// 点 (x, y, z) を断面へ投影する。固定軸の座標が断面から tol より離れて
// いれば false。tol < 0 は 0 として扱う (真上の点だけ)。
bool pointOnSlice(double x, double y, double z, int axis, double sliceCoord,
                  double tol, double uMin, double uMax,
                  double vMin, double vMax, SlicePointNorm *out);

} // namespace ofd
