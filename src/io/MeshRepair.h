// MeshRepair.h — 取込メッシュ (STL) の修復。MeshDiagnostics の *修復* 側。
//
// GeometryTab「ジオメトリ修復」節の「▶ 修復を実行」の実体。Qt Widgets 非依存
// (selftest から直接検証する)。
//
// 実装している修復は 3 つで、いずれも決定的 (乱数・時刻に依存しない):
//
//   ① 頂点溶接      許容差内の頂点を 1 点にまとめる。三角形スープ (STL は
//                   各三角形が自前の頂点を持つ) を位相のある網にする前提工程。
//   ② 縮退三角形除去 溶接後に頂点が重複する / 面積が 0 の三角形を捨てる。
//                   これらは法線を持たず、ボクセル化でも寄与しない。
//   ③ 法線の統一    面の隣接グラフを幅優先で辿り、辺を共有する 2 枚が
//                   **逆向きに** 辿るよう裏返す。連結成分ごとに行い、
//                   成分の符号付き体積が負なら成分全体を反転して外向きに
//                   揃える (閉じた成分のみ — 開いた成分は基準が無いので
//                   最初の面の向きに合わせるだけ)。
//
// **穴埋め (境界エッジの解消) は実装していない**。穴の塞ぎ方は一意でなく、
// 形状を変える操作なので、黙って埋めると利用者の形状と違うものを解析して
// しまう。検出して報告するに留める (RepairReport::boundaryEdgesLeft)。
#pragma once
#include "MeshDiagnostics.h"
#include "StlImporter.h"

namespace ofd {

struct RepairOptions {
    // 頂点溶接は常に行う (位相を作る前提工程で、切れる選択肢ではない)。
    bool dropDegenerate = true;   // ② 縮退三角形の除去
    bool unifyNormals = true;     // ③ 法線の統一
    // 溶接の許容差 [m]。0 以下なら bbox 対角 × 1e-6 (MeshDiagnostics と同じ)
    double weldTolerance = 0.0;
};

struct RepairReport {
    bool valid = false;              // 修復を実行したか
    bool skippedTooLarge = false;    // 三角形数の上限超過で省略した
    int  weldedVertices = 0;         // 溶接で消えた頂点数
    int  removedTriangles = 0;       // 捨てた縮退三角形の数
    int  flippedTriangles = 0;       // 裏返した三角形の数
    int  componentsFlipped = 0;      // 全体反転した連結成分の数 (外向き化)
    int  boundaryEdgesLeft = 0;      // 残った境界エッジ (穴埋めは未実装)
    MeshDiagnostics before;          // 修復前の検査結果
    MeshDiagnostics after;           // 修復後の検査結果
};

// mesh を修復して out へ書く (mesh は変更しない)。
// 上限を超えるメッシュは修復せず skippedTooLarge を立てて false を返す
// (中途半端に触らない)。三角形が 0 になる場合も false。
bool repairMesh(const ImportedMesh &mesh, const RepairOptions &opt,
                ImportedMesh &out, RepairReport &report,
                int maxTriangles = kMeshDiagnosticsMaxTriangles);

} // namespace ofd
