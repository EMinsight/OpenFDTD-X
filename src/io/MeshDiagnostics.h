// MeshDiagnostics.h — 取込メッシュ (STL) の位相・幾何検査
//
// GeometryTab「ジオメトリ修復」節の *検出* 側の実体。固定サンプルではなく
// 実際に取り込んだ三角形メッシュから数えるための Qt Widgets 非依存の関数群
// (selftest から直接検証する)。
//
// 検査内容 (いずれも三角形スープ → 頂点溶接後の半辺位相から数える):
//   - 重複頂点     : 溶接で 1 点にまとまった頂点の数
//   - 縮退三角形   : 溶接後に頂点が重複する / 面積が 0 の三角形
//   - 境界エッジ   : 1 枚の三角形にしか使われない辺 (穴・隙間)
//   - 非多様体エッジ: 3 枚以上の三角形が共有する辺
//   - 法線不一致   : 2 枚が共有するが同じ向きにたどられる辺 (裏返った面)
//
// 「水密 (watertight)」の判定は 境界エッジ = 0 かつ 非多様体エッジ = 0。
// これは閉多様体の必要条件で、ボクセル化 (内外判定) が成立する前提になる。
//
// 体積 (signedVolume) は発散定理で数える: V = Σ (v0 · (v1 × v2)) / 6。
// **閉じたメッシュでしか意味を持たない** (穴があると足し合わせが打ち切られ、
// 数字は出るが体積ではない) ので、使う側は watertight() を必ず見ること。
// 符号は面の向きで決まり、負なら法線が内向き — 大きさは同じなので、
// 「裏返っている」ことの検出に使える。
//
// *修復* (縫合・法線統一・デシメーション) は未実装 — ここは検出のみ。
#pragma once
#include "MeshImporter.h"

namespace ofd {

struct MeshDiagnostics {
    bool   valid = false;             // 検査を実行したか (false = 未取込/省略)
    bool   skippedTooLarge = false;   // 三角形数の上限超過で省略した
    int    triangles = 0;             // 検査した三角形数
    int    rawVertices = 0;           // 溶接前の頂点数 (= triangles × 3)
    int    uniqueVertices = 0;        // 溶接後の頂点数
    int    duplicateVertices = 0;     // rawVertices − uniqueVertices
    int    degenerateTriangles = 0;   // 面積 0 / 頂点重複
    int    boundaryEdges = 0;         // 使用回数 1 の辺 (穴)
    int    nonManifoldEdges = 0;      // 使用回数 3 以上の辺
    int    inconsistentEdges = 0;     // 2 枚が同じ向きにたどる辺 (法線不一致)
    double weldTolerance = 0.0;       // 頂点溶接の許容差 [m]
    // 符号付き体積 (発散定理)。**watertight() が真のときだけ体積として
    // 意味を持つ**。負 = 面の向きが内向き (大きさは同じ)
    double signedVolume = 0.0;
    double volume() const { return signedVolume < 0.0 ? -signedVolume
                                                      : signedVolume; }

    // 閉多様体の必要条件 (穴が無く、辺を共有するのが常に 2 枚)
    bool watertight() const {
        return valid && boundaryEdges == 0 && nonManifoldEdges == 0;
    }
};

// 検査の既定上限。GUI スレッドでの同期実行を前提にした保険で、これを超える
// メッシュは検査せず skippedTooLarge を立てる (偽の「問題なし」を出さない)。
constexpr int kMeshDiagnosticsMaxTriangles = 200000;

// mesh を検査する。頂点溶接の許容差は bbox 対角 × 1e-6 (0 なら 1e-12)。
MeshDiagnostics analyzeMesh(const ImportedMesh &mesh,
                            int maxTriangles = kMeshDiagnosticsMaxTriangles);

} // namespace ofd
