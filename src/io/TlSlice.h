// TlSlice.h — BELLHOP の TL 断面 (.shd) を 3D シーンの 1 枚の鉛直面へ載せる。
//
// `Viewport3D::setResultSlice()` は「3 次元空間内の 1 平面」として実データを
// 重ねて描く仕組みで、これまで FDTD の HDF5 断面だけが使っていた。BELLHOP の
// 解は**距離 × 深度の鉛直面そのもの**なので、そのまま同じ仕組みに載る。
// ここはその**座標と値の対応づけだけ**を持つ (ウィジェットを include しない
// ので selftest から直接判定できる)。
//
// ── 座標の対応 ────────────────────────────────────────────────────────────
//   x = 受波器距離 [m] (.env の R 行と同じ範囲)
//   y = 0            — BELLHOP (2D) の解に横方向の広がりは無い。
//                      **面として描くのが正しく、厚みを持たせない**。
//   z = 深度 [m] を**下向き負**で置く (海面 z = 0、海底 z = −水深)
//   ⇒ axis = 1 (Y 一定 = XZ 鉛直面)、面内 第1軸 = x、第2軸 = z
//
// `ShdField` の行 0 は海面側、`setResultSlice` の行 0 は「第 2 軸の +側」。
// 第 2 軸は z で、海面が最大 (0) なので**そのまま一致する** (反転しない)。
//
// ── 値の対応 (ここを間違えると色が裏返る) ────────────────────────────────
// TL [dB] は**小さいほど大きい音**で、ウィジェットは値の絶対値を最大値で
// 正規化して jet を塗る。そのまま渡すと遠方 (TL 大) が赤くなって逆になるので、
// **基準 TL からの差 (refTl − TL) = 相対レベル**へ直して渡す。
// 基準は有効値の最大 (最も静かな点) なので、相対レベルは 0 以上になる。
//
// レイの届かない格子 (`ShdField::kNoField`) は **NaN** にする。ウィジェットは
// 有限でない値を透明にするので、「静か」と塗り分けられずに**下の海底や海面が
// 透けて見える**。0 dB を入れて「そこは無音」と描くのは誤り。
#pragma once
#include <QString>
#include <QVector>

namespace ofd {

struct ShdField;

namespace io {

struct TlSlice3D {
    QVector<double> cells;      // rows*cols (row-major)、行 0 = 海面側
    int    rows = 0, cols = 0;
    int    axis = 1;            // Y 一定 (XZ 鉛直面)
    double pos_m = 0.0;         // y = 0
    double u0_m = 0.0, u1_m = 0.0;   // x (距離) の範囲
    double v0_m = 0.0, v1_m = 0.0;   // z (深度、下向き負) の範囲
    double refTl_dB = 0.0;      // cells = refTl_dB − TL
    double spanTl_dB = 0.0;     // 有効値の dB 幅 (= refTl − minTL)
    int    noFieldCells = 0;    // レイが届かず NaN にした格子数

    bool valid() const
    { return rows > 0 && cols > 0
             && cells.size() == qsizetype(rows) * cols; }
};

// range0/range1: 受波器距離の範囲 [m] (.env の R 行と同じもの)
// depth_m: 海底までの深さ [m] (受波器深度の下端。z = −depth_m になる)
// 断面が無効・距離や深さが非正・TL に幅が無いときは valid() == false を返す
// (色の付けようが無いものを無理に描かない)。
TlSlice3D tlSlice3D(const ShdField &f, double range0_m, double range1_m,
                    double depth_m);

} // namespace io
} // namespace ofd
