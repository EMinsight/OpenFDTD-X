// MeshAxes.h — 取込メッシュの主軸検出 (面積重み付き慣性主軸)
//
// GeometryTab「配置・変換」の「主軸を自動検出」の実体。Qt Widgets 非依存
// (selftest から直接検証する)。
//
// 手法: 表面の面積重み付き共分散行列 (2 次モーメント) の固有ベクトル。
// 頂点をそのまま使う PCA は **頂点密度に引きずられる** (細かく切った面が
// 重く効く) ので採らない。三角形ごとに閉形式の 2 次モーメント
//
//   ∫_T (p−g)(p−g)ᵀ dA = (A/12) Σ_{i=0..2} (p_i − g)(p_i − g)ᵀ
//
// (g = 三角形の重心) を使い、平行軸の定理で全体の重心まわりへ積む。
// 固有値分解は対称 3×3 の Jacobi 法 (反復は決定的で乱数を使わない)。
//
// 軸の並びは固有値の降順 (最も広がっている方向が第 1 軸)。符号は
// 「絶対値最大の成分を正にする」規約で一意にし、右手系 (det = +1) に揃える。
// これにより同じメッシュからは常に同じ結果が出る。
#pragma once
#include "MeshImporter.h"

namespace ofd {

struct PrincipalAxes {
    bool   valid = false;
    double centroid[3] = { 0, 0, 0 };   // 面積重み付き重心 [m]
    // 主軸 (行 = 軸。axis[0] が最も広がっている方向)。正規直交・右手系
    double axis[3][3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
    // 各主軸方向の 2 次モーメント (固有値、降順) [m⁴]
    double moment[3] = { 0, 0, 0 };
    // GeometryTab の配置・変換 (X→Y→Z の順に回す) で主軸を X/Y/Z へ
    // 揃えるための角度 [deg]
    double eulerXYZ_deg[3] = { 0, 0, 0 };
    // 主軸が縮退している (固有値がほぼ等しい) と向きが一意に決まらない。
    // 立方体や球のような形で、回転させても同じに見えるケース。
    bool   degenerate = false;
};

// mesh の主軸を求める。三角形が無い / 総面積が 0 なら valid = false。
PrincipalAxes principalAxes(const ImportedMesh &mesh);

} // namespace ofd
