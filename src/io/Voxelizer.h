// Voxelizer.h — staircase voxelization of a triangle mesh onto the Yee grid.
//
// docs/libigl-integration.md が設計する「STL→Yee格子」のうち、libigl 非依存で
// 動作する staircase (階段近似) 版を実装する。各 Yee セル中心から +X 方向に
// レイを飛ばし、メッシュ三角形との交差回数の偶奇で内外を判定する
// (ray casting parity test)。占有セルは X 方向に連続する区間ごとに直方体
// (geometry shape=1) へまとめ、Project に追加できる形で返す。
//
// 出力は必ず Yee セル単位の直方体 (.ofd の geometry) である。部分体積率
// (VoxelOptions::pvf) は **セルを部分的に埋める表現ではなく**、占有/非占有の
// 判定をセル中心 1 点から N³ 点の体積率へ置き換えるもの。併せて形状の体積を
// 精度よく見積もり、階段近似の形状誤差を実測できるようにする
// (材質の内挿 = 共形/サブセル FDTD はカーネル側の機能で、ここでは扱わない)。
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

    // ── 部分体積率 (PVF: partial volume fraction) ─────────────────────────
    // 三角形が横切るセル (境界セル) だけを pvfSamples³ 点で再標本化し、
    // セル内の材質の占有率 f を求める。占有判定は f >= pvfThreshold。
    // 境界セル以外は面が通らないので中心 1 点で厳密に決まる (再標本化しない)。
    //
    // pvfSamples = 1 は中心 1 点そのものなので、**PVF 無効時と完全に同じ結果**
    // になる (従来判定は N=1 の特別な場合)。
    // ── 空間索引 (探索の高速化) ───────────────────────────────────────────
    // レイの偶奇 (RayParity) はセル 1 個ごとに全三角形を走査するため
    // O(セル数 × 三角形数) で、三角形数に比例して遅くなる (実測: 60³ セルで
    // 三角形 256 → 0.27 s、9,216 → 10.2 s)。レイの向きはほぼ +X 固定なので、
    // Y-Z 平面を格子に切って三角形を割り振っておけば、レイが通り得る格子だけを
    // 調べれば済む。**判定する三角形の集合は必ず超集合**なので、
    // 交差回数 (整数) は索引の有無で完全に一致する — 結果は変わらない。
    //
    // WindingNumber は全三角形の立体角の総和なので枝刈りできず、この設定は
    // 無視される (RayParity 専用)。
    bool   spatialIndex = true;

    bool   pvf = false;
    int    pvfSamples = 4;          // 1..8 へクランプ
    double pvfThreshold = 0.5;      // (0, 1] へクランプ
    // 再標本化の作業量 (境界セル数 × 標本数 × 三角形数) の上限。
    // 超えたら黙って粗くせずエラーにする (何を計算したか分からなくしない)。
    qint64 pvfWorkCap = 200'000'000;
};

struct VoxelResult {
    bool    ok = false;
    QString error;
    int     nx = 0, ny = 0, nz = 0;     // mesh cell counts used
    qint64  occupied = 0;               // number of occupied Yee cells
    QVector<Geometry> bricks;           // staircase geometry (X-runs merged)

    // 占有セルの総体積 [m³] (= 階段近似した形状の体積)。常に算出する。
    double  stairVolume = 0.0;
    // ↓ PVF 有効時のみ意味を持つ (無効時は 0)
    qint64  boundaryCells = 0;          // 三角形が横切るセル = 再標本化した数
    double  pvfVolume = 0.0;            // Σ f·セル体積 [m³] (体積の推定値)

    // ↓ 空間索引 (使わなかった場合は 0)
    int     indexBins = 0;              // Y-Z 平面の分割数 (片側)
    qint64  indexEntries = 0;           // 索引に入った (三角形, 格子) の組数
    // 実際に行ったレイ-三角形交差判定の回数。索引の有無で結果は変わらないので、
    // 速くなったことはこの**決定的な数値**で確かめる (時間計測は環境に依る)。
    qint64  triangleTests = 0;
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
