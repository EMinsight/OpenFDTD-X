// VoxelSlice.h — ボクセル化結果 (直方体の集まり) の 1 断面を占有マスクにする。
//
// GeometryTab「ボクセル化プレビュー」の実体。`io/Voxelizer` が返す
// `VoxelResult::bricks` は **X 方向に連続する占有セルを 1 個の直方体へ
// まとめたもの** なので、そのままでは「どのセルが埋まっているか」が分から
// ない。ここで指定した断面のセル占有を復元して、描画側は塗るだけにする。
//
// **描画から計算を分離する理由**: 断面ごとのセル数を全断面ぶん足すと
// `VoxelResult::occupied` にちょうど一致しなければならない。この一致は
// 描いた絵と数値が同じものを指している証拠になるので、selftest から
// (ウィジェットを作らずに) 判定する。
//
// ── 面の指定は「固定する軸」で受ける ──────────────────────────────────────
// 「面番号」を受け取る形にしていたが、面番号の意味がタブごとに違う
// (MeshPreview は 0=XY/1=YZ/2=ZX、H5アニメは 0=XY/1=XZ/2=YZ) ので、
// **どちらの流儀か分からない引数になってしまう** (実際にこれで取り違えた)。
// 固定軸 (0=X, 1=Y, 2=Z) なら解釈は 1 つしかないので、こちらで受ける。
// 面内 2 軸の対応は H5Reader::seriesSliceAxes と共用する — 断面の
// 「列 = u 軸 / 行 = v 軸」という規約をリポジトリ全体で 1 つに保つため。
#pragma once
#include <QVector>

#include "../core/Geometry.h"
#include "../core/MeshAxis.h"

namespace ofd {

struct VoxelSliceMask {
    bool ok = false;
    int  cols = 0, rows = 0;      // 列 (u 軸) ・行 (v 軸) のセル数
    int  uAxis = 0, vAxis = 1;    // 面内 2 軸 (0=X, 1=Y, 2=Z)
    QVector<bool> cell;           // cols*rows。true = 占有 (row-major)
    qint64 occupied = 0;          // この断面の占有セル数
    // 断面の座標範囲 [m] (描画の軸ラベル用)
    double colMin = 0.0, colMax = 0.0, rowMin = 0.0, rowMax = 0.0;
    // 固定した軸の位置 [m] (セル中心)
    double sliceCoord = 0.0;

    bool at(int c, int r) const {
        return (c >= 0 && c < cols && r >= 0 && r < rows)
                   ? cell[qsizetype(r) * cols + c] : false;
    }
};

// bricks のうち materialId が一致するものだけを見る (materialId < 0 で全部)。
// axis は固定する軸 (0=X, 1=Y, 2=Z)、index はその軸のセル番号 (0 始まり)。
// 範囲外なら ok = false (端の断面を代わりに返さない)。
VoxelSliceMask voxelSlice(const QVector<Geometry> &bricks,
                          const MeshAxis &mx, const MeshAxis &my,
                          const MeshAxis &mz,
                          int axis, int index, int materialId = -1);

// 固定軸 axis (0=X, 1=Y, 2=Z) のセル数 = その方向の断面の枚数。
int voxelSliceCount(const MeshAxis &mx, const MeshAxis &my, const MeshAxis &mz,
                    int axis);

} // namespace ofd
