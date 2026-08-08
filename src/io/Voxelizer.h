// Voxelizer.h — staircase voxelization of a triangle mesh onto the Yee grid.
//
// docs/libigl-integration.md が設計する「STL→Yee格子」のうち、libigl 非依存で
// 動作する staircase (階段近似) 版を実装する。各 Yee セル中心から +X 方向に
// レイを飛ばし、メッシュ三角形との交差回数の偶奇で内外を判定する
// (ray casting parity test)。占有セルは X 方向に連続する区間ごとに直方体
// (geometry shape=1) へまとめ、Project に追加できる形で返す。
//
// libigl ビルド (-DUSE_LIBIGL=ON) では fast_winding_number による
// より正確な内外判定・共形メッシュへ差し替え可能 (将来拡張)。
#pragma once
#include <QVector>
#include "../core/Geometry.h"

namespace ofd {

struct MeshAxis;
struct ImportedMesh;

// 内外判定の方法
enum class InsideTest {
    // ① レイの交差回数の偶奇 (Möller-Trumbore)。閉じた多様体なら厳密で速い。
    //    穴・自己交差があると、その穴を通るレイの列がまるごと誤判定になる。
    RayParity,
    // ② 一般化巻き数 (J. A. Bärentzen & H. Aanæs / A. Jacobson et al.,
    //    "Robust Inside-Outside Segmentation using Generalized Winding
    //    Numbers", ACM TOG 32(4), 2013)。各三角形が張る立体角の和 / 4π で、
    //    閉じた面の内側で 1、外側で 0 になる。穴があっても連続的に劣化する
    //    だけで、レイのように列単位で壊れない。判定は w > 0.5。
    //    立体角は Van Oosterom & Strackee, IEEE TBME 30(2), 125 (1983) の
    //    閉形式で求める (数値的に安定)。
    //
    //    **前提: 法線の向きが首尾一貫していること。** 揃っていないと + と −
    //    が打ち消し合い、内側でも w ≈ 0 になって全セルが「外」になる
    //    (STL は面ごとに独立なので、向きが揃っていない実データは珍しくない)。
    //    io/MeshRepair の法線統一を通してから使うこと。呼び出し側は
    //    MeshDiagnostics::inconsistentEdges で事前に確認できる。
    WindingNumber
};

struct VoxelOptions {
    InsideTest inside = InsideTest::RayParity;
    // X 方向に連続する占有セルを 1 個の直方体へまとめる。
    // 切ると 1 セル = 1 直方体になり、.ofd の geometry 行数が跳ね上がる
    // (代わりにセルとの対応が 1:1 になる)。
    bool mergeRuns = true;
};

struct VoxelResult {
    bool    ok = false;
    QString error;
    int     nx = 0, ny = 0, nz = 0;     // mesh cell counts used
    qint64  occupied = 0;               // number of occupied Yee cells
    QVector<Geometry> bricks;           // staircase geometry (X-runs merged)
};

class Voxelizer {
public:
    // Voxelize `mesh` onto the project's Yee grid (mx/my/mz).
    // Occupied cells are assigned `materialId`. `cellCap` guards against
    // runaway grids (returns ok=false with an error if exceeded).
    static VoxelResult voxelize(const ImportedMesh &mesh,
                                const MeshAxis &mx,
                                const MeshAxis &my,
                                const MeshAxis &mz,
                                int materialId,
                                qint64 cellCap = 8'000'000,
                                const VoxelOptions &opt = VoxelOptions());

    // 一般化巻き数 w(p)。閉じた外向きメッシュの内側で ≈ 1、外側で ≈ 0。
    // (内外判定そのものだけでなく、メッシュの閉じ具合の診断にも使える)
    static double windingNumber(const ImportedMesh &mesh,
                                double px, double py, double pz);
};

} // namespace ofd
