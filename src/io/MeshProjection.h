// MeshProjection.h — 三角形メッシュを 3D プレビューへ落とす投影 (計算側)。
//
// 形状タブ「3D プレビュー」の実体のうち、**どこに写るか**を決める部分。
// 描画側は返ってきた座標を塗るだけにする (io/VoxelSlice・io/SliceOverlay と
// 同じ分け方 — 描画に幾何を書かない)。
//
// ── 投影 ──────────────────────────────────────────────────────────────────
// Viewport3D と同じ「方位角 az / 仰角 el の正射影」。カメラは原点を向き、
//     u =  −x·sin(az) + y·cos(az)
//     v =  −x·cos(az)·sin(el) − y·sin(az)·sin(el) + z·cos(el)
//     depth = x·cos(az)·cos(el) + y·sin(az)·cos(el) + z·sin(el)
// で、u が画面右、v が画面上、depth が手前ほど大きい。透視は掛けない
// (寸法を読む用途なので、遠近で大きさが変わらないほうがよい)。
//
// ── 面の並べ替えは呼び出し側 ──────────────────────────────────────────────
// 隠面消去は「奥から順に塗る」(画家のアルゴリズム) で行う。三角形ごとの
// depth (3 頂点の平均) を返すので、呼び出し側が昇順に並べて塗る。厳密な
// 隠面消去ではないので、**交差する面がある形状では前後が入れ替わりうる**
// (プレビューであることを画面に明記する)。
#pragma once
#include "MeshImporter.h"

namespace ofd {

struct ProjectedTri {
    double u[3] = { 0, 0, 0 };     // 画面座標 (右が +)
    double v[3] = { 0, 0, 0 };     // 画面座標 (上が +)
    double depth = 0.0;            // 3 頂点の平均 (大きいほど手前)
    double shade = 0.0;            // 0..1 の陰影 (法線と視線の角度から)
};

// 1 点の正射影。az / el は度。
void projectPoint(double x, double y, double z, double azDeg, double elDeg,
                  double *u, double *v, double *depth);

// メッシュ全体を投影する。返るのは三角形ごとの画面座標・深さ・陰影で、
// **並べ替えはしない** (呼び出し側が depth 昇順に塗る)。
QVector<ProjectedTri> projectMesh(const ImportedMesh &mesh,
                                  double azDeg, double elDeg);

} // namespace ofd
