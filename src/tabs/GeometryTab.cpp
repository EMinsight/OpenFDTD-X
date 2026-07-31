// GeometryTab.cpp
#include "GeometryTab.h"
#include "../core/Project.h"
#include "../io/StlImporter.h"
#include "../io/Voxelizer.h"
#include "../widgets/SectionBox.h"
#include "../widgets/UnitNav.h"
#include "../I18n.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QTableWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

using namespace ofd;

// ── タブ固有の翻訳キー (geoc_) — file-local 登録 (既存 ge_ は I18n.cpp) ───────
// モック tabs.jsx GeometryTab の import / voxel / refine サブタブ由来の語彙。
namespace {

struct Tr { const char *key; const char *ja; const char *en; };

const Tr kTr[] = {
    // ── 3Dモデル取込 / Import 3D CAD model ─────────────────────────────────
    { "geoc_import_section", "3Dモデル取込 / Import 3D CAD model",
      "Import 3D CAD model" },
    { "geoc_file", "ファイル", "File" },
    { "geoc_browse", "📁 参照…", "📁 Browse…" },
    { "geoc_fmt_class", "形式分類", "Format class" },
    { "geoc_fmt_cad", "CAD (B-rep):", "CAD (B-rep):" },
    { "geoc_fmt_mesh", "メッシュ:", "Mesh:" },
    { "geoc_fmt_eda", "2D/EDA:", "2D/EDA:" },
    { "geoc_cad_hint",
      "▸ STEP/IGES は B-rep (境界表現) CAD — FDTD用に内部でテセレーション "
      "(三角形分割) します。OpenCASCADE (OCCT) エンジン経由でアセンブリ・"
      "ソリッド・材質を保持。",
      "▸ STEP/IGES are B-rep (boundary representation) CAD — they are "
      "tessellated internally for FDTD. Assemblies, solids and materials are "
      "kept through the OpenCASCADE (OCCT) engine." },

    // ── マウス操作 / Mouse shortcuts ────────────────────────────────────────
    { "geoc_mouse_section", "マウス操作 / Mouse shortcuts", "Mouse shortcuts" },
    { "geoc_col_op", "操作", "Action" },
    { "geoc_col_key", "キー / マウス", "Key / mouse" },
    { "geoc_col_effect", "動作", "Effect" },
    { "geoc_ms1_o", "選択", "Select" },
    { "geoc_ms1_k", "左クリック", "Left click" },
    { "geoc_ms1_a", "ユニット選択", "Select a unit" },
    { "geoc_ms2_o", "複数選択", "Multi-select" },
    { "geoc_ms2_k", "Shift+クリック / 矩形ドラッグ",
      "Shift+click / rubber-band drag" },
    { "geoc_ms2_a", "追加・矩形選択", "Add to / box-select" },
    { "geoc_ms3_o", "平行移動", "Translate" },
    { "geoc_ms3_k", "G または ギズモ赤緑青軸", "G or the RGB gizmo axes" },
    { "geoc_ms3_a", "選択軸でドラッグ", "Drag along the locked axis" },
    { "geoc_ms4_o", "回転", "Rotate" },
    { "geoc_ms4_k", "R または 回転ギズモ", "R or the rotate gizmo" },
    { "geoc_ms4_a", "選択軸を中心に回転", "Rotate about the locked axis" },
    { "geoc_ms5_o", "スケール", "Scale" },
    { "geoc_ms5_k", "S または スケールギズモ", "S or the scale gizmo" },
    { "geoc_ms5_a", "サイズ変更", "Resize" },
    { "geoc_ms6_o", "軸ロック", "Axis lock" },
    { "geoc_ms6_k", "X / Y / Z", "X / Y / Z" },
    { "geoc_ms6_a", "指定軸のみ", "Restrict to one axis" },
    { "geoc_ms7_o", "グリッドスナップ", "Grid snap" },
    { "geoc_ms7_k", "Ctrl+ドラッグ", "Ctrl+drag" },
    { "geoc_ms7_a", "Δxにスナップ", "Snap to Δx" },
    { "geoc_ms8_o", "頂点スナップ", "Vertex snap" },
    { "geoc_ms8_k", "V+ドラッグ", "V+drag" },
    { "geoc_ms8_a", "他形状の頂点へ", "Snap to another shape's vertex" },
    { "geoc_ms9_o", "新規ブロック", "New block" },
    { "geoc_ms9_k", "空所で B+ドラッグ", "B+drag on empty space" },
    { "geoc_ms9_a", "直方体を即時作成", "Create a brick immediately" },
    { "geoc_ms10_o", "カメラ回転", "Orbit" },
    { "geoc_ms10_k", "中ドラッグ", "Middle drag" },
    { "geoc_ms10_a", "視点旋回", "Orbit the view" },
    { "geoc_ms11_o", "パン", "Pan" },
    { "geoc_ms11_k", "Shift+中ドラッグ", "Shift+middle drag" },
    { "geoc_ms11_a", "平行移動", "Translate the view" },
    { "geoc_ms12_o", "正面ビュー", "Axis views" },
    { "geoc_ms12_k", "1 / 3 / 7", "1 / 3 / 7" },
    { "geoc_ms12_a", "+X / +Y / +Z", "+X / +Y / +Z" },

    // ── STEP テセレーション / Tessellation (OCCT) ───────────────────────────
    { "geoc_tess_section", "STEP テセレーション / Tessellation (OCCT)",
      "Tessellation (OCCT)" },
    { "geoc_tess_hint",
      "B-rep の滑らかな曲面を三角形メッシュに変換。FDTDのボクセル化に必要。",
      "Converts smooth B-rep surfaces into a triangle mesh — required before "
      "FDTD voxelization." },
    { "geoc_tess_dev", "線形偏差 (弦高)", "Linear deflection (sag)" },
    { "geoc_tess_dev_hint", "小さいほど曲面が滑らか・三角形増",
      "Smaller = smoother surfaces, more triangles" },
    { "geoc_tess_angle", "角度偏差", "Angular deflection" },
    { "geoc_tess_quality", "品質プリセット", "Quality preset" },
    { "geoc_tess_coarse", "粗 (高速)", "Coarse (fast)" },
    { "geoc_tess_med", "標準", "Standard" },
    { "geoc_tess_fine", "高精細", "Fine" },
    { "geoc_tess_adapt", "波長適応", "λ-adaptive" },
    { "geoc_tess_parallel", "並列テセレーション", "Parallel tessellation" },
    { "geoc_tess_curv", "曲率適応細分", "Curvature-adaptive refinement" },

    // ── アセンブリツリー / Assembly tree ────────────────────────────────────
    { "geoc_asm_section", "アセンブリツリー / Assembly tree", "Assembly tree" },
    { "geoc_asm_hint",
      "STEPファイルの階層構造。部品ごとに材質・有効/無効を設定。",
      "The STEP file hierarchy — set material and enable/disable per part." },
    { "geoc_asm_root_info", "3 solids · 1 unit=mm", "3 solids · 1 unit=mm" },
    { "geoc_asm_ignore", "無視", "ignored" },
    { "geoc_asm_csys", "座標系: Z-up, 原点=底面中心",
      "Coordinate system: Z-up, origin = bottom centre" },
    { "geoc_asm_assign", "材質一括割当", "Assign materials in bulk" },
    { "geoc_asm_autoignore", "小部品を自動無視 (<λ/20)",
      "Auto-ignore small parts (<λ/20)" },

    // ── 配置・変換 / Placement ──────────────────────────────────────────────
    { "geoc_place_section", "配置・変換 / Placement", "Placement" },
    { "geoc_place_unit", "単位", "Unit" },
    { "geoc_place_scale", "× スケール", "× scale" },
    { "geoc_place_offset", "オフセット (x,y,z)", "Offset (x,y,z)" },
    { "geoc_place_rot", "回転 (XYZ)", "Rotation (XYZ)" },
    { "geoc_place_center", "モデル原点を計算領域中心に整列",
      "Align the model origin with the domain centre" },
    { "geoc_place_autoaxis", "主軸を自動検出", "Auto-detect principal axes" },

    // ── ジオメトリ修復 / Healing ────────────────────────────────────────────
    { "geoc_heal_section", "ジオメトリ修復 / Healing (CAD cleanup)",
      "Healing (CAD cleanup)" },
    { "geoc_heal_hint",
      "取込CADにありがちな不具合を自動修復。FDTD成立に必須。",
      "Auto-repairs the defects typical of imported CAD — required for a valid "
      "FDTD model." },
    { "geoc_col_process", "処理", "Process" },
    { "geoc_col_detect", "検出", "Detected" },
    { "geoc_col_state", "状態", "State" },
    { "geoc_hl1_p", "隙間/重複面の縫合 (sew)", "Sew gaps / duplicate faces" },
    { "geoc_hl1_d", "3 箇所", "3 spots" },
    { "geoc_hl1_s", "修復可", "fixable" },
    { "geoc_hl2_p", "微小面・スリバー除去", "Remove tiny / sliver faces" },
    { "geoc_hl2_d", "12 面", "12 faces" },
    { "geoc_hl2_s", "修復可", "fixable" },
    { "geoc_hl3_p", "法線方向の統一", "Unify normal orientation" },
    { "geoc_hl3_d", "—", "—" },
    { "geoc_hl3_s", "OK", "OK" },
    { "geoc_hl4_p", "フィレット/面取り簡略化", "Simplify fillets / chamfers" },
    { "geoc_hl4_d", "8 箇所", "8 spots" },
    { "geoc_hl4_s", "任意", "optional" },
    { "geoc_hl5_p", "閉ソリッド化チェック", "Closed-solid check" },
    { "geoc_hl5_d", "—", "—" },
    { "geoc_hl5_s", "水密 OK", "watertight OK" },
    { "geoc_hl6_p", "デシメーション (三角形削減)",
      "Decimation (triangle reduction)" },
    { "geoc_hl6_d", "→ 60%", "→ 60%" },
    { "geoc_hl6_s", "任意", "optional" },
    { "geoc_heal_run", "🔧 自動修復実行", "🔧 Run auto-heal" },
    { "geoc_heal_next", "後処理: ボクセル化へ", "Next: voxelization" },

    // ── 物性値割当 / Material mapping ───────────────────────────────────────
    { "geoc_map_section", "物性値割当 / Material mapping",
      "Material mapping" },
    { "geoc_map_method", "割当方法", "Method" },
    { "geoc_map_single", "単一材質", "Single material" },
    { "geoc_map_byname", "部品名から自動", "From part name" },
    { "geoc_map_bycolor", "色から", "From colour" },
    { "geoc_map_manual", "手動 (ツリー)", "Manual (tree)" },
    { "geoc_map_default", "デフォルト材質", "Default material" },
    { "geoc_map_m2", "2 — 誘電体 (εr=3.5)", "2 — dielectric (εr=3.5)" },
    { "geoc_map_m1", "1 — PEC", "1 — PEC" },
    { "geoc_map_m3", "3 — FR-4", "3 — FR-4" },

    // ── 取込プレビュー / Preview ────────────────────────────────────────────
    { "geoc_prev_section", "取込プレビュー / Preview", "Import preview" },
    { "geoc_prev_tri", "18,440 三角形", "18,440 triangles" },
    { "geoc_prev_tri_fmt", "%1 三角形", "%1 triangles" },
    { "geoc_prev_solid", "3 ソリッド · 水密", "3 solids · watertight" },
    { "geoc_prev_solid_fmt", "1 ソリッド · 表面積 %1 m²",
      "1 solid · area %1 m²" },
    { "geoc_prev_vol", "体積 2.27 cm³", "volume 2.27 cm³" },
    { "geoc_prev_vol_fmt", "体積 %1 cm³", "volume %1 cm³" },
    { "geoc_prev_bbox", "bbox 60×60×32 mm", "bbox 60×60×32 mm" },
    { "geoc_prev_bbox_fmt", "bbox %1×%2×%3 mm", "bbox %1×%2×%3 mm" },
    { "geoc_prev_import", "📥 取込実行", "📥 Run import" },
    { "geoc_prev_3d", "👁 3Dプレビュー", "👁 3D preview" },
    { "geoc_prev_measure", "📐 寸法測定", "📐 Measure" },

    // ── 取込済みモデル / Imported models ────────────────────────────────────
    { "geoc_models_section", "取込済みモデル / Imported models",
      "Imported models" },
    { "geoc_col_name", "名前", "Name" },
    { "geoc_col_format", "形式", "Format" },
    { "geoc_col_tri", "三角形", "Triangles" },
    { "geoc_col_vol", "体積", "Volume" },
    { "geoc_col_matcol", "物性", "Material" },
    { "geoc_models_edit", "編集", "Edit" },
    { "geoc_mdl_multi", "多材質", "multi-material" },
    { "geoc_mdl_diel", "2 誘電体", "2 dielectric" },
    { "geoc_mdl_pec", "1 PEC", "1 PEC" },
    { "geoc_models_hint",
      "▸ 取込後は「ボクセル化」で Yee 格子へ変換 → FDTD 計算へ",
      "▸ After import, convert to the Yee grid in Voxelization → FDTD run" },
    { "geoc_mdl_live", "%1 (現在の取込)", "%1 (live import)" },

    // ── ボクセル化 / Voxelization ───────────────────────────────────────────
    { "geoc_vox_section", "ボクセル化 / Voxelization", "Voxelization" },
    { "geoc_vox_hint",
      "取込3Dモデルを Yee グリッドに分解します。FDTDで扱える形状 "
      "(直方体ボクセル列) に変換。",
      "Decomposes the imported 3D model onto the Yee grid — into the brick-"
      "voxel runs FDTD can handle." },
    { "geoc_vox_delta", "分解度 Δ", "Resolution Δ" },
    { "geoc_vox_delta_hint", "→ λ/22 @ 2.5 GHz", "→ λ/22 @ 2.5 GHz" },
    { "geoc_vox_inout", "内外判定", "Inside/outside test" },
    { "geoc_vox_ray", "レイキャスト", "Ray cast" },
    { "geoc_vox_winding", "巻数 (Winding)", "Winding number" },
    { "geoc_vox_sdf", "SDF (符号付距離)", "SDF (signed distance)" },
    { "geoc_vox_surface", "表面処理", "Surface treatment" },
    { "geoc_vox_stair", "階段近似", "Staircase" },
    { "geoc_vox_conformal", "共形 (Conformal FDTD)", "Conformal FDTD" },
    { "geoc_vox_subcell", "サブセル (SI-FDTD)", "Sub-cell (SI-FDTD)" },
    { "geoc_vox_surf_hint",
      "▸ 階段: 高速・実装容易・形状誤差大　▸ 共形: Yee境界変形で精度向上・推奨　"
      "▸ サブセル: 厚さ0の薄膜・PEC薄板に最適",
      "▸ Staircase: fast, simple, large shape error　▸ Conformal: deformed Yee "
      "boundary, more accurate (recommended)　▸ Sub-cell: best for zero-"
      "thickness films and thin PEC plates" },
    { "geoc_vox_pvf_label", "部分容積判定", "Partial volume" },
    { "geoc_vox_pvf", "PVF (Partial Volume Fraction)",
      "PVF (partial volume fraction)" },
    { "geoc_vox_pvf_hint", "境界セルの占有率で物性値を内挿",
      "Interpolates the material from the boundary-cell occupancy" },
    { "geoc_vox_multi", "マルチマテリアル", "Multi-material" },
    { "geoc_vox_merge", "優先度順マージ", "Merge by priority" },
    { "geoc_vox_merge_hint", "後置オブジェクトが優先 (OpenFDTD仕様準拠)",
      "Later objects win (per the OpenFDTD spec)" },
    { "geoc_vox_octree", "八分木 (Octree)", "Octree" },
    { "geoc_vox_oct_adapt", "階層適応分解", "Hierarchical adaptive split" },
    { "geoc_vox_oct_level", "最大レベル", "Max level" },
    { "geoc_vox_gpu_label", "GPU加速", "GPU acceleration" },
    { "geoc_vox_gpu", "ボクセル化をGPUで実行",
      "Run the voxelization on the GPU" },
    { "geoc_vox_preview", "プレビュー", "Preview" },
    { "geoc_vox_badge", "27,900 セル中 4,221 セルが占有",
      "4,221 of 27,900 cells occupied" },
    { "geoc_vox_badge_fmt", "%1 セル中 %2 セルが占有",
      "%2 of %1 cells occupied" },

    // ── ボクセル統計 / Voxel statistics ─────────────────────────────────────
    { "geoc_stat_section", "ボクセル統計 / Voxel statistics",
      "Voxel statistics" },
    { "geoc_stat_occ", "占有セル数", "Occupied cells" },
    { "geoc_stat_occ_val", "4,221 / 27,900 (15.1%)", "4,221 / 27,900 (15.1%)" },
    { "geoc_stat_occ_fmt", "%1 / %2 (%3%)", "%1 / %2 (%3%)" },
    { "geoc_stat_bnd", "境界セル数", "Boundary cells" },
    { "geoc_stat_bnd_val", "1,083 (PVF適用)", "1,083 (PVF applied)" },
    { "geoc_stat_err", "形状誤差", "Shape error" },
    { "geoc_stat_err_val", "0.18% RMS", "0.18% RMS" },
    { "geoc_stat_ok", "許容", "acceptable" },
    { "geoc_stat_conf", "共形セル比率", "Conformal cell ratio" },
    { "geoc_stat_conf_val", "62.3%", "62.3%" },
    { "geoc_stat_stair", "0.0% (階段近似)", "0.0% (staircase)" },
    { "geoc_stat_na", "— (階段近似 / PVF 無効)", "— (staircase, PVF off)" },

    // ── メッシュ細分化 / Mesh refinement ────────────────────────────────────
    { "geoc_ref_section", "メッシュ細分化 / Mesh refinement",
      "Mesh refinement" },
    { "geoc_ref_hint",
      "障害物・微細構造の周辺で局所的にメッシュを細かくします。"
      "サブグリッド法 (Subgridding) または非均一メッシュ。",
      "Refines the mesh locally around obstacles and fine structures — either "
      "subgridding or a non-uniform mesh." },
    { "geoc_ref_method", "細分化手法", "Method" },
    { "geoc_ref_local", "非均一メッシュ", "Non-uniform mesh" },
    { "geoc_ref_subgrid", "サブグリッド", "Subgridding" },
    { "geoc_ref_amr", "AMR (適応細分化)", "AMR (adaptive)" },
    { "geoc_ref_target", "対象", "Targets" },
    { "geoc_ref_edge", "エッジ周辺 (回折)", "Around edges (diffraction)" },
    { "geoc_ref_curve", "湾曲面", "Curved surfaces" },
    { "geoc_ref_thin", "薄膜・薄板", "Thin films / plates" },
    { "geoc_ref_higheps", "高εr 領域", "High-εr regions" },
    { "geoc_ref_ratio", "細分化比率", "Refinement ratio" },
    { "geoc_ref_ratio_hint", "x (1セルを3x3x3に分割)",
      "x (one cell → 3×3×3)" },
    { "geoc_ref_trans", "遷移幅", "Transition width" },
    { "geoc_ref_trans_hint", "滑らかなセルサイズ変化",
      "Smooth cell-size grading" },
    { "geoc_ref_lambda", "λ/N 規則", "λ/N rule" },
    { "geoc_ref_lambda_unit", "cells/λ_最小", "cells/λ_min" },
    { "geoc_ref_autocheck", "自動チェック", "Auto check" },
    { "geoc_ref_showviol", "違反箇所を赤表示", "Highlight violations in red" },
    { "geoc_ref_run", "▶ 自動細分化", "▶ Auto-refine" },
    { "geoc_ref_badge_fmt", "セル増加: +%1% (推定)",
      "Cell growth: +%1% (est.)" },

    // ── 細分化領域 / Refined regions ────────────────────────────────────────
    { "geoc_regions_section", "細分化領域 / Refined regions",
      "Refined regions" },
    { "geoc_col_region", "領域", "Region" },
    { "geoc_col_range", "範囲", "Range" },
    { "geoc_col_ratio", "比率", "Ratio" },
    { "geoc_col_dcells", "セル増", "ΔCells" },
    { "geoc_rr2_range", "球φ2mm @ origin", "sphere ⌀2 mm @ origin" },

    // ── ユニット編集 / Unit transform (tabs.jsx:409-494) ────────────────────
    { "geoc_xf_section", "ユニット編集 / Unit transform", "Unit transform" },
    { "geoc_xf_hint",
      "スライダーはドラッグ中の増分を選択ユニットへ適用し、離すと 0 に戻る "
      "(変更は確定)。回転: 6パラメータ形状は AABB 近似 (90°倍数で厳密)、"
      "三角柱は自軸厳密、角錐台/円錐台は自軸のみ。",
      "Sliders apply a drag-relative delta to the selected unit and snap back "
      "to 0 on release (the change is kept). Rotation: 6-parameter shapes use "
      "an AABB approximation (exact at multiples of 90°); triangular prisms "
      "are exact about their own axis; pyramid/cone frusta support their own "
      "axis only." },
    { "geoc_xf_translate", "平行移動 %1 (±30 mm)", "Translate %1 (±30 mm)" },
    { "geoc_xf_rotate", "回転 %1 (±180°)", "Rotate %1 (±180°)" },
    { "geoc_xf_insert", "挿入", "Insert" },
    { "geoc_xf_dup", "複製", "Duplicate" },
    { "geoc_xf_mirror", "ミラー", "Mirror" },
    { "geoc_xf_mirror_axis", "軸", "Axis" },
    { "geoc_xf_no_unit", "⚠ ユニットが選択されていません",
      "⚠ No unit selected" },
    { "geoc_xf_rot_unsupported",
      "⚠ この形状は %1 軸回転に未対応 (三角柱/角錐台/円錐台は自軸のみ)",
      "⚠ This shape cannot rotate about %1 (prisms/frusta: own axis only)" },
};

const bool s_i18n = [] {
    for (const Tr &t : kTr) ofd::I18n::reg(t.key, t.ja, t.en);
    return true;
}();

// mock の CSS クラス色 (badge acc / ok / warn) + muted
const char kAcc[]   = "#0078D4";
const char kOk[]    = "#2E8B57";
const char kWarn[]  = "#B45309";
const char kMuted[] = "#888888";

QLabel *makeHint(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setWordWrap(true);
    l->setStyleSheet(QStringLiteral("color:%1;").arg(QLatin1String(kMuted)));
    return l;
}

QLabel *makeBadge(const QString &text, const char *color, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setStyleSheet(
        QStringLiteral("color:%1; font-weight:600;").arg(QLatin1String(color)));
    return l;
}

QLabel *makeMono(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return l;
}

QCheckBox *makeCheck(const QString &text, bool on, QWidget *parent)
{
    auto *c = new QCheckBox(text, parent);
    c->setChecked(on);
    return c;
}

// 読取専用テーブル (mock の q-table 相当)
QTableWidget *makeTable(const QStringList &headers, int rows, QWidget *parent,
                        int minHeight = 0)
{
    auto *t = new QTableWidget(rows, headers.size(), parent);
    t->setHorizontalHeaderLabels(headers);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    t->horizontalHeader()->setStretchLastSection(true);
    t->verticalHeader()->setVisible(false);
    t->verticalHeader()->setDefaultSectionSize(24);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setMinimumHeight(minHeight > 0 ? minHeight : rows * 26 + 34);
    return t;
}

QTableWidgetItem *textItem(const QString &s)
{
    return new QTableWidgetItem(s);
}

QTableWidgetItem *numItem(const QString &s)
{
    auto *it = new QTableWidgetItem(s);
    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return it;
}

QTableWidgetItem *badgeItem(const QString &s, const char *color)
{
    auto *it = new QTableWidgetItem(s);
    it->setForeground(QColor(color));
    return it;
}

// 先頭列のチェックボックスセル (mock の <input type="checkbox">)
QTableWidgetItem *checkItem(bool on)
{
    auto *it = new QTableWidgetItem;
    it->setCheckState(on ? Qt::Checked : Qt::Unchecked);
    it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
    return it;
}

// <Seg> 相当: 排他 checkable QPushButton 行を 1 ウィジェットに畳む
QWidget *segRow(QWidget *parent, QButtonGroup **out, const QStringList &labels,
                int current)
{
    auto *w = new QWidget(parent);
    auto *h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(1);
    auto *grp = new QButtonGroup(w);
    grp->setExclusive(true);
    for (int i = 0; i < labels.size(); ++i) {
        auto *b = new QPushButton(labels[i], w);
        b->setCheckable(true);
        b->setStyleSheet("padding:2px 10px;");
        grp->addButton(b, i);
        h->addWidget(b);
    }
    if (auto *b = grp->button(current)) b->setChecked(true);
    h->addStretch(1);
    if (out) *out = grp;
    return w;
}

QDoubleSpinBox *makeSpin(QWidget *parent, double lo, double hi, int decimals,
                         double value, double step = 0.1)
{
    auto *w = new QDoubleSpinBox(parent);
    w->setRange(lo, hi);
    w->setDecimals(decimals);
    w->setSingleStep(step);
    w->setValue(value);
    w->setMaximumWidth(110);
    return w;
}

QSpinBox *makeIntSpin(QWidget *parent, int lo, int hi, int value)
{
    auto *w = new QSpinBox(parent);
    w->setRange(lo, hi);
    w->setValue(value);
    w->setMaximumWidth(90);
    return w;
}

// 「値 + 単位 (+ 補足)」の行 (mock の <input> + <span className="muted">)
QWidget *valueRow(QWidget *parent, QWidget *field, const QString &unit,
                  const QString &hint = QString())
{
    auto *w = new QWidget(parent);
    auto *h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(6);
    h->addWidget(field);
    if (!unit.isEmpty()) h->addWidget(makeHint(unit, w));
    if (!hint.isEmpty()) h->addWidget(makeHint(hint, w));
    h->addStretch(1);
    return w;
}

// 3桁区切り (mock の toLocaleString 相当)
QString groupNum(qint64 v)
{
    return QLocale(QLocale::English).toString(v);
}

// 取込メッシュの体積 (発散定理: Σ v0·(v1×v2)/6)。ImportedMesh は体積を
// 持たないため、プレビュー表示用にここで求める。
double meshVolume(const ImportedMesh &m)
{
    double vol = 0.0;
    const int n = qMin(m.numTriangles, int(m.vertices.size() / 9));
    for (int t = 0; t < n; ++t) {
        const float *p = m.vertices.constData() + t * 9;
        const double x1 = p[0], y1 = p[1], z1 = p[2];
        const double x2 = p[3], y2 = p[4], z2 = p[5];
        const double x3 = p[6], y3 = p[7], z3 = p[8];
        vol += (x1 * (y2 * z3 - z2 * y3)
              - y1 * (x2 * z3 - z2 * x3)
              + z1 * (x2 * y3 - y2 * x3)) / 6.0;
    }
    return std::fabs(vol);
}

// ── ユニット編集の座標変換 (Geometry::coordIndices = sol/ingeometry.c 準拠) ──

// 軸 axis 方向へ d [m] 平行移動 (全形状で厳密)
void translateGeometry(ofd::Geometry &g, int axis, double d)
{
    int idx[3];
    const int n = ofd::Geometry::coordIndices(g.shape, axis, idx);
    for (int i = 0; i < n; ++i) g.g[idx[i]] += d;
}

// ユニットの AABB 中心 (回転の基準点)
void bboxCenter(const ofd::Geometry &g, double c[3])
{
    for (int a = 0; a < 3; ++a) {
        int idx[3];
        const int n = ofd::Geometry::coordIndices(g.shape, a, idx);
        if (n == 0) { c[a] = 0; continue; }
        double lo = g.g[idx[0]], hi = g.g[idx[0]];
        for (int i = 1; i < n; ++i) {
            lo = std::min(lo, g.g[idx[i]]);
            hi = std::max(hi, g.g[idx[i]]);
        }
        c[a] = (lo + hi) / 2.0;
    }
}

// 軸 axis まわりに deg [°] 回転 (基準 = ユニットの AABB 中心)。
//   6 パラメータ形状 : 8 頂点を回して AABB を取り直す近似 (90°倍数で厳密)
//   三角柱 31..33    : 自軸のみ・断面 3 頂点の厳密回転
//   角錐台/円錐台    : 自軸のみ・断面中心を回転、奇数×90° で断面寸法を入替
// 対応できない軸/形状では false (呼び出し側が警告表示)。
bool rotateGeometry(ofd::Geometry &g, int axis, double deg)
{
    double c[3];
    bboxCenter(g, c);
    const double th = deg * M_PI / 180.0;
    const double cs = std::cos(th), sn = std::sin(th);
    // 右手系: 軸 a まわりの回転は巡回面 (u,v) = ((a+1)%3, (a+2)%3) の
    // 標準 2D 回転になる
    const int u = (axis + 1) % 3, v = (axis + 2) % 3;
    auto rotUV = [&](double &pu, double &pv) {
        const double du = pu - c[u], dv = pv - c[v];
        pu = c[u] + du * cs - dv * sn;
        pv = c[v] + du * sn + dv * cs;
    };

    switch (g.shape) {
        case 1: case 2: case 11: case 12: case 13: {
            // AABB 近似: 8 頂点を回して外接直方体を取り直す
            double lo[3], hi[3];
            for (int i = 0; i < 8; ++i) {
                double p[3] = { g.g[i & 1], g.g[2 + ((i >> 1) & 1)],
                                g.g[4 + ((i >> 2) & 1)] };
                rotUV(p[u], p[v]);
                for (int a = 0; a < 3; ++a) {
                    if (i == 0) { lo[a] = hi[a] = p[a]; continue; }
                    lo[a] = std::min(lo[a], p[a]);
                    hi[a] = std::max(hi[a], p[a]);
                }
            }
            for (int a = 0; a < 3; ++a) {
                g.g[2 * a] = lo[a];
                g.g[2 * a + 1] = hi[a];
            }
            return true;
        }
        case 31: case 32: case 33: {
            if (axis != g.shape - 31) return false;   // 自軸のみ (厳密)
            // g[2..4] = 断面第1軸 (=u), g[5..7] = 第2軸 (=v) — inout3 の順
            for (int i = 0; i < 3; ++i) rotUV(g.g[2 + i], g.g[5 + i]);
            return true;
        }
        case 41: case 42: case 43:
        case 51: case 52: case 53: {
            if (axis != g.shape % 10 - 1) return false;   // 自軸のみ
            rotUV(g.g[2], g.g[3]);                        // 断面中心 (u,v)
            // 断面寸法 (u,v の半幅/径ペア) は 90° の奇数倍で入替 (厳密)。
            // それ以外の角度では軸整列のまま = 近似。
            if (qRound(deg / 90.0) & 1) {
                std::swap(g.g[4], g.g[5]);
                std::swap(g.g[6], g.g[7]);
            }
            return true;
        }
    }
    return false;   // 未対応形状
}

// 軸 axis に対して座標符号を反転 (ミラー)。寸法パラメータは触らない。
void mirrorGeometry(ofd::Geometry &g, int axis)
{
    int idx[3];
    const int n = ofd::Geometry::coordIndices(g.shape, axis, idx);
    for (int i = 0; i < n; ++i) g.g[idx[i]] = -g.g[idx[i]];
}

} // namespace

static const int kShapeCodes[] = { 1, 2, 11, 12, 13, 31, 32, 33,
                                   41, 42, 43, 51, 52, 53 };

static int shapeIndex(int code)
{
    for (size_t i = 0; i < sizeof(kShapeCodes)/sizeof(int); ++i)
        if (kShapeCodes[i] == code) return int(i);
    return 0;
}

GeometryTab::GeometryTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // ── ユニット一覧 / Unit list (既存: geometry = 行と 1:1) ────────────────
    auto *s = new SectionBox(I18n::tr("ge_section"), body);

    auto *navRow = new QHBoxLayout();
    navRow->addWidget(new QLabel(I18n::tr("ge_unit"), s));
    m_nav = new UnitNav(s);
    navRow->addWidget(m_nav);
    navRow->addStretch(1);
    s->vbox()->addLayout(navRow);

    m_table = new QTableWidget(0, 11, s);
    QStringList headers { I18n::tr("ge_mat"), I18n::tr("ge_shape") };
    for (int i = 1; i <= 8; ++i) headers << QStringLiteral("g%1").arg(i);
    headers << I18n::tr("ma_name");
    m_table->setHorizontalHeaderLabels(headers);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setDefaultSectionSize(24);
    m_table->setMinimumHeight(200);
    s->vbox()->addWidget(m_table);

    auto *row = new QHBoxLayout();
    auto *add = new QPushButton(I18n::tr("ge_add"), s);
    auto *del = new QPushButton(I18n::tr("ge_del"), s);
    row->addWidget(add);
    row->addWidget(del);
    row->addStretch(1);
    s->vbox()->addLayout(row);
    v->addWidget(s);

    // ── ユニット編集 / Unit transform (mock tabs.jsx:409-494) ───────────────
    v->addWidget(buildTransformSection());

    // ── マウス操作 / Mouse shortcuts ────────────────────────────────────────
    v->addWidget(buildMouseSection());

    // ── 3Dモデル取込 / Import 3D CAD model (実データは STL 取込) ────────────
    auto *si = new SectionBox(I18n::tr("geoc_import_section"), body);
    addCadImportRows(si);
    auto *importBtn = new QPushButton(I18n::tr("ge_import_btn"), si);
    si->vbox()->addWidget(importBtn);

    m_importInfo = new QLabel(I18n::tr("ge_import_hint"), si);
    m_importInfo->setWordWrap(true);
    si->vbox()->addWidget(m_importInfo);
    v->addWidget(si);

    // ── CAD パイプライン (モック tabs.jsx の import / voxel / refine 節) ────
    v->addWidget(buildTessellationSection());
    v->addWidget(buildAssemblySection());
    v->addWidget(buildPlacementSection());
    v->addWidget(buildHealingSection());
    v->addWidget(buildMaterialMapSection());
    v->addWidget(buildPreviewSection());
    v->addWidget(buildImportedSection());
    v->addWidget(buildVoxelSection());
    v->addWidget(buildVoxelStatsSection());
    v->addWidget(buildRefineSection());
    v->addWidget(buildRefinedRegionsSection());

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    connect(add, &QPushButton::clicked, this, [this] {
        Geometry g;
        // default to the mesh region so the new unit is visible
        for (int a = 0; a < 3; ++a) {
            g.g[2*a]   = m_p->mesh(a).min();
            g.g[2*a+1] = m_p->mesh(a).max();
        }
        m_p->geometries().push_back(g);
        refresh();
        m_p->touch();
    });
    connect(del, &QPushButton::clicked, this, [this] {
        const int r = m_table->currentRow();
        auto &gs = m_p->geometries();
        if (r >= 0 && r < gs.size()) {
            gs.removeAt(r);
            refresh();
            m_p->touch();
        }
    });
    connect(m_table, &QTableWidget::cellChanged, this, [this] {
        if (m_updating) return;
        applyTable();
        m_p->touch();
    });
    connect(m_table, &QTableWidget::currentCellChanged, this,
            [this](int r, int, int, int) { m_nav->setCurrent(r); });
    connect(m_nav, &UnitNav::currentChanged, this, [this](int i) {
        m_table->selectRow(i);
    });
    connect(importBtn, &QPushButton::clicked, this, &GeometryTab::importStl);
    connect(m_voxBtn, &QPushButton::clicked, this, &GeometryTab::voxelizeImported);

    connect(project, &Project::loaded, this, &GeometryTab::refresh);
    refresh();
}

// ── ユニット編集 / Unit transform ───────────────────────────────────────────
// 平行移動 X/Y/Z (±30mm 増分) と回転 X/Y/Z (±180°) の 6 スライダー +
// 挿入/複製/ミラー。スライダーはドラッグ基準 (押下時のユニットを基準に
// 現在値ぶんの変換を適用) で、離すと 0 に戻り変更が確定する。
QWidget *GeometryTab::buildTransformSection()
{
    auto *s = new SectionBox(I18n::tr("geoc_xf_section"));
    s->vbox()->addWidget(makeHint(I18n::tr("geoc_xf_hint"), s));

    static const char *kAxis[3] = { "X", "Y", "Z" };
    auto makeSliderRow = [this, s](QSlider **outSl, QLabel **outVal,
                                   const QString &label, int lo, int hi,
                                   const QString &unit) {
        auto *rowW = new QWidget(s);
        auto *h = new QHBoxLayout(rowW);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(6);
        auto *sl = new QSlider(Qt::Horizontal, rowW);
        sl->setRange(lo, hi);
        sl->setValue(0);
        sl->setMinimumWidth(160);
        auto *val = new QLabel(QStringLiteral("0 ") + unit, rowW);
        val->setMinimumWidth(60);
        h->addWidget(sl, 1);
        h->addWidget(val);
        s->form()->addRow(label, rowW);
        *outSl = sl;
        *outVal = val;
    };
    for (int a = 0; a < 3; ++a)
        makeSliderRow(&m_trSlider[a], &m_trValue[a],
                      I18n::tr("geoc_xf_translate")
                          .arg(QLatin1String(kAxis[a])),
                      -30, 30, "mm");
    for (int a = 0; a < 3; ++a)
        makeSliderRow(&m_rotSlider[a], &m_rotValue[a],
                      I18n::tr("geoc_xf_rotate").arg(QLatin1String(kAxis[a])),
                      -180, 180, QString::fromUtf8("°"));

    // 挿入 / 複製 / ミラー (軸コンボ付き)
    auto *btnRow = new QHBoxLayout();
    auto *insBtn = new QPushButton(I18n::tr("geoc_xf_insert"), s);
    auto *dupBtn = new QPushButton(I18n::tr("geoc_xf_dup"), s);
    auto *mirBtn = new QPushButton(I18n::tr("geoc_xf_mirror"), s);
    m_mirrorAxis = new QComboBox(s);
    for (const char *ax : kAxis)
        m_mirrorAxis->addItem(QLatin1String(ax));
    btnRow->addWidget(insBtn);
    btnRow->addWidget(dupBtn);
    btnRow->addWidget(mirBtn);
    btnRow->addWidget(new QLabel(I18n::tr("geoc_xf_mirror_axis"), s));
    btnRow->addWidget(m_mirrorAxis);
    btnRow->addStretch(1);
    s->vbox()->addLayout(btnRow);

    m_xformWarn = makeBadge(QString(), kWarn, s);
    m_xformWarn->setVisible(false);
    s->vbox()->addWidget(m_xformWarn);

    auto warn = [this](const QString &text) {
        m_xformWarn->setText(text);
        m_xformWarn->setVisible(true);
    };
    auto clearWarn = [this] { m_xformWarn->setVisible(false); };

    // スライダー共通配線 (rotate=false: mm → m 平行移動)
    auto hookSlider = [this, warn, clearWarn](QSlider *sl, QLabel *val,
                                              int axis, bool rotate,
                                              const QString &unit) {
        connect(sl, &QSlider::sliderPressed, this,
                [this, axis, rotate, warn, clearWarn] {
            clearWarn();
            m_dragUnit = currentUnit();
            if (m_dragUnit < 0) {
                warn(I18n::tr("geoc_xf_no_unit"));
                return;
            }
            m_dragBase = m_p->geometries()[m_dragUnit];
        });
        connect(sl, &QSlider::sliderMoved, this,
                [this, axis, rotate, val, unit, warn](int v) {
            val->setText(QStringLiteral("%1 %2").arg(v).arg(unit));
            if (m_dragUnit < 0 || m_dragUnit >= m_p->geometries().size())
                return;
            Geometry g = m_dragBase;   // 常にドラッグ開始時点が基準
            if (rotate) {
                if (!rotateGeometry(g, axis, v)) {
                    warn(I18n::tr("geoc_xf_rot_unsupported")
                             .arg(QLatin1String(kAxis[axis])));
                    return;
                }
            } else {
                translateGeometry(g, axis, v * 1e-3);   // mm → m
            }
            m_p->geometries()[m_dragUnit] = g;
            const int keep = m_dragUnit;
            refresh();
            m_nav->setCurrent(keep);
            m_p->touch();
        });
        connect(sl, &QSlider::sliderReleased, this, [this, sl, val, unit] {
            m_dragUnit = -1;           // 確定 — 基準を捨てて 0 へ戻す
            QSignalBlocker b(sl);
            sl->setValue(0);
            val->setText(QStringLiteral("0 %1").arg(unit));
        });
    };
    for (int a = 0; a < 3; ++a) {
        hookSlider(m_trSlider[a], m_trValue[a], a, false,
                   QStringLiteral("mm"));
        hookSlider(m_rotSlider[a], m_rotValue[a], a, true,
                   QString::fromUtf8("°"));
    }

    connect(insBtn, &QPushButton::clicked, this, [this, clearWarn] {
        clearWarn();
        Geometry g;   // メッシュ領域いっぱいの直方体 (追加ボタンと同じ既定)
        for (int a = 0; a < 3; ++a) {
            g.g[2 * a]     = m_p->mesh(a).min();
            g.g[2 * a + 1] = m_p->mesh(a).max();
        }
        insertUnitAfterCurrent(g);
    });
    connect(dupBtn, &QPushButton::clicked, this, [this, warn, clearWarn] {
        clearWarn();
        const int cur = currentUnit();
        if (cur < 0) {
            warn(I18n::tr("geoc_xf_no_unit"));
            return;
        }
        insertUnitAfterCurrent(m_p->geometries()[cur]);
    });
    connect(mirBtn, &QPushButton::clicked, this, [this, warn, clearWarn] {
        clearWarn();
        const int cur = currentUnit();
        if (cur < 0) {
            warn(I18n::tr("geoc_xf_no_unit"));
            return;
        }
        mirrorGeometry(m_p->geometries()[cur],
                       m_mirrorAxis->currentIndex());
        refresh();
        m_nav->setCurrent(cur);
        m_p->touch();
    });
    return s;
}

// 選択中ユニット (UnitNav 優先、無ければテーブル選択行)。-1 = 無し。
int GeometryTab::currentUnit() const
{
    int cur = m_nav->current();
    if (cur < 0) cur = m_table->currentRow();
    if (cur < 0 || cur >= m_p->geometries().size()) return -1;
    return cur;
}

// 挿入/複製: 現在ユニットの直後へ入れて選択を移す (無選択時は末尾)
void GeometryTab::insertUnitAfterCurrent(const Geometry &g)
{
    const int cur = currentUnit();
    const int at = cur < 0 ? m_p->geometries().size() : cur + 1;
    m_p->geometries().insert(at, g);
    refresh();
    m_nav->setCurrent(at);
    m_table->selectRow(at);
    m_p->touch();
}

// ── マウス操作 / Mouse shortcuts ────────────────────────────────────────────
QWidget *GeometryTab::buildMouseSection()
{
    auto *s = new SectionBox(I18n::tr("geoc_mouse_section"));
    auto *t = makeTable({ I18n::tr("geoc_col_op"), I18n::tr("geoc_col_key"),
                          I18n::tr("geoc_col_effect") }, 12, s);
    for (int r = 0; r < 12; ++r) {
        const QString base = QStringLiteral("geoc_ms%1_").arg(r + 1);
        t->setItem(r, 0, textItem(I18n::tr(base + "o")));
        t->setItem(r, 1, textItem(I18n::tr(base + "k")));
        t->setItem(r, 2, textItem(I18n::tr(base + "a")));
    }
    s->vbox()->addWidget(t);
    return s;
}

// ── 3Dモデル取込: ファイル行 + 対応形式バッジ + 解説 ────────────────────────
void GeometryTab::addCadImportRows(SectionBox *s)
{
    auto *fileRow = new QWidget(s);
    auto *fh = new QHBoxLayout(fileRow);
    fh->setContentsMargins(0, 0, 0, 0);
    fh->setSpacing(6);
    m_cadFile = new QLineEdit("radome_assembly.step", fileRow);
    fh->addWidget(m_cadFile, 1);
    auto *browse = new QPushButton(I18n::tr("geoc_browse"), fileRow);
    fh->addWidget(browse);
    s->form()->addRow(I18n::tr("geoc_file"), fileRow);

    // 形式分類: CAD (B-rep)
    auto *cadRow = new QWidget(s);
    auto *ch = new QHBoxLayout(cadRow);
    ch->setContentsMargins(0, 0, 0, 0);
    ch->setSpacing(6);
    ch->addWidget(makeHint(I18n::tr("geoc_fmt_cad"), cadRow));
    ch->addWidget(makeBadge("STEP (.stp/.step)", kAcc, cadRow));
    ch->addWidget(makeBadge("IGES (.igs)", kAcc, cadRow));
    ch->addWidget(makeBadge("BREP", kMuted, cadRow));
    ch->addWidget(makeBadge("Parasolid (.x_t)", kMuted, cadRow));
    ch->addWidget(makeBadge("SAT (ACIS)", kMuted, cadRow));
    ch->addStretch(1);
    s->form()->addRow(I18n::tr("geoc_fmt_class"), cadRow);

    // メッシュ / 2D-EDA 形式
    auto *meshRow = new QHBoxLayout();
    meshRow->setSpacing(6);
    meshRow->addWidget(makeHint(I18n::tr("geoc_fmt_mesh"), s));
    for (const char *f : { "STL", "OBJ", "PLY", "3MF" })
        meshRow->addWidget(makeBadge(QString::fromLatin1(f), kMuted, s));
    meshRow->addSpacing(8);
    meshRow->addWidget(makeHint(I18n::tr("geoc_fmt_eda"), s));
    for (const char *f : { "GDSII", "DXF", "OASIS" })
        meshRow->addWidget(makeBadge(QString::fromLatin1(f), kMuted, s));
    meshRow->addStretch(1);
    s->vbox()->addLayout(meshRow);

    s->vbox()->addWidget(makeHint(I18n::tr("geoc_cad_hint"), s));

    // 参照… は取込ファイル名のみ差し替える (STEP/IGES パースは OCCT 連携で拡張)
    connect(browse, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, I18n::tr("geoc_import_section"), {},
            "CAD/mesh (*.step *.stp *.igs *.iges *.brep *.x_t *.sat *.stl "
            "*.obj *.ply *.3mf);;All files (*)");
        if (!path.isEmpty()) m_cadFile->setText(path);
    });
}

// ── STEP テセレーション / Tessellation (OCCT) ───────────────────────────────
QWidget *GeometryTab::buildTessellationSection()
{
    auto *s = new SectionBox(I18n::tr("geoc_tess_section"));
    s->vbox()->addWidget(makeHint(I18n::tr("geoc_tess_hint"), s));

    m_tessDev = makeSpin(s, 0.0001, 10.0, 4, 0.010, 0.005);
    s->form()->addRow(I18n::tr("geoc_tess_dev"),
                      valueRow(s, m_tessDev, "mm",
                               I18n::tr("geoc_tess_dev_hint")));
    m_tessAngle = makeSpin(s, 1.0, 90.0, 0, 15.0, 1.0);
    s->form()->addRow(I18n::tr("geoc_tess_angle"),
                      valueRow(s, m_tessAngle, "°"));
    s->form()->addRow(I18n::tr("geoc_tess_quality"),
                      segRow(s, &m_tessQuality,
                             { I18n::tr("geoc_tess_coarse"),
                               I18n::tr("geoc_tess_med"),
                               I18n::tr("geoc_tess_fine"),
                               I18n::tr("geoc_tess_adapt") }, 1));

    auto *cr = new QHBoxLayout();
    m_tessParallel = makeCheck(I18n::tr("geoc_tess_parallel"), true, s);
    m_tessCurvature = makeCheck(I18n::tr("geoc_tess_curv"), true, s);
    cr->addWidget(m_tessParallel);
    cr->addWidget(m_tessCurvature);
    cr->addStretch(1);
    s->vbox()->addLayout(cr);
    return s;
}

// ── アセンブリツリー / Assembly tree ───────────────────────────────────────
QWidget *GeometryTab::buildAssemblySection()
{
    auto *s = new SectionBox(I18n::tr("geoc_asm_section"));
    s->vbox()->addWidget(makeHint(I18n::tr("geoc_asm_hint"), s));

    m_asmTree = new QTreeWidget(s);
    m_asmTree->setColumnCount(2);
    m_asmTree->setHeaderHidden(true);
    m_asmTree->setRootIsDecorated(true);
    m_asmTree->setIndentation(16);
    m_asmTree->setMaximumHeight(200);
    m_asmTree->setMinimumHeight(140);

    auto *root = new QTreeWidgetItem(m_asmTree,
        { "📦 radome_assembly.step", I18n::tr("geoc_asm_root_info") });
    root->setCheckState(0, Qt::Checked);
    root->setForeground(1, QColor(kMuted));

    auto part = [root](const QString &name, const QString &tag, bool on) {
        auto *it = new QTreeWidgetItem(root, { "🔧 " + name, tag });
        it->setCheckState(0, on ? Qt::Checked : Qt::Unchecked);
        it->setForeground(1, QColor(on ? kAcc : kMuted));
        return it;
    };
    part("radome_shell", "FR-4", true);
    part("mounting_ring", "PEC", true);
    part("bolt_set (×6)", I18n::tr("geoc_asm_ignore"), false);
    auto *csys = new QTreeWidgetItem(root, { "📐 " + I18n::tr("geoc_asm_csys") });
    csys->setForeground(0, QColor(kMuted));

    m_asmTree->expandAll();
    m_asmTree->resizeColumnToContents(0);
    s->vbox()->addWidget(m_asmTree);

    auto *br = new QHBoxLayout();
    br->addWidget(new QPushButton(I18n::tr("geoc_asm_assign"), s));
    br->addWidget(new QPushButton(I18n::tr("geoc_asm_autoignore"), s));
    br->addStretch(1);
    s->vbox()->addLayout(br);
    return s;
}

// ── 配置・変換 / Placement ─────────────────────────────────────────────────
QWidget *GeometryTab::buildPlacementSection()
{
    auto *s = new SectionBox(I18n::tr("geoc_place_section"));

    auto *unitRowW = new QWidget(s);
    auto *uh = new QHBoxLayout(unitRowW);
    uh->setContentsMargins(0, 0, 0, 0);
    uh->setSpacing(6);
    uh->addWidget(segRow(unitRowW, &m_placeUnit,
                         { "mm", "m", "μm", "nm", "inch" }, 0));
    uh->addWidget(makeHint(I18n::tr("geoc_place_scale"), unitRowW));
    m_placeScale = makeSpin(unitRowW, 1e-6, 1e6, 3, 1.0, 0.1);
    uh->addWidget(m_placeScale);
    uh->addStretch(1);
    s->form()->addRow(I18n::tr("geoc_place_unit"), unitRowW);

    auto *offRow = new QWidget(s);
    auto *oh = new QHBoxLayout(offRow);
    oh->setContentsMargins(0, 0, 0, 0);
    oh->setSpacing(4);
    for (int i = 0; i < 3; ++i) {
        m_placeOffset[i] = makeSpin(offRow, -1e6, 1e6, 3, 0.0, 0.001);
        oh->addWidget(m_placeOffset[i]);
    }
    oh->addWidget(makeHint("m", offRow));
    oh->addStretch(1);
    s->form()->addRow(I18n::tr("geoc_place_offset"), offRow);

    auto *rotRow = new QWidget(s);
    auto *rh = new QHBoxLayout(rotRow);
    rh->setContentsMargins(0, 0, 0, 0);
    rh->setSpacing(4);
    for (int i = 0; i < 3; ++i) {
        m_placeRot[i] = makeSpin(rotRow, -360.0, 360.0, 0, 0.0, 1.0);
        rh->addWidget(m_placeRot[i]);
    }
    rh->addWidget(makeHint("°", rotRow));
    rh->addStretch(1);
    s->form()->addRow(I18n::tr("geoc_place_rot"), rotRow);

    auto *cr = new QHBoxLayout();
    m_placeCenter = makeCheck(I18n::tr("geoc_place_center"), true, s);
    m_placeAutoAxis = makeCheck(I18n::tr("geoc_place_autoaxis"), false, s);
    cr->addWidget(m_placeCenter);
    cr->addWidget(m_placeAutoAxis);
    cr->addStretch(1);
    s->vbox()->addLayout(cr);
    return s;
}

// ── ジオメトリ修復 / Healing (CAD cleanup) ─────────────────────────────────
QWidget *GeometryTab::buildHealingSection()
{
    auto *s = new SectionBox(I18n::tr("geoc_heal_section"));
    s->vbox()->addWidget(makeHint(I18n::tr("geoc_heal_hint"), s));

    m_healTable = makeTable({ QString(), I18n::tr("geoc_col_process"),
                              I18n::tr("geoc_col_detect"),
                              I18n::tr("geoc_col_state") }, 6, s);
    static const bool kOn[6] = { true, true, true, false, true, false };
    static const char *kColor[6] = { kOk, kOk, kOk, kWarn, kOk, kMuted };
    for (int r = 0; r < 6; ++r) {
        const QString base = QStringLiteral("geoc_hl%1_").arg(r + 1);
        m_healTable->setItem(r, 0, checkItem(kOn[r]));
        m_healTable->setItem(r, 1, textItem(I18n::tr(base + "p")));
        m_healTable->setItem(r, 2, numItem(I18n::tr(base + "d")));
        m_healTable->setItem(r, 3, badgeItem(I18n::tr(base + "s"), kColor[r]));
    }
    s->vbox()->addWidget(m_healTable);

    auto *br = new QHBoxLayout();
    br->addWidget(new QPushButton(I18n::tr("geoc_heal_run"), s));
    br->addStretch(1);
    br->addWidget(makeHint(I18n::tr("geoc_heal_next"), s));
    s->vbox()->addLayout(br);
    return s;
}

// ── 物性値割当 / Material mapping ──────────────────────────────────────────
QWidget *GeometryTab::buildMaterialMapSection()
{
    auto *s = new SectionBox(I18n::tr("geoc_map_section"));
    s->form()->addRow(I18n::tr("geoc_map_method"),
                      segRow(s, &m_mapMethod,
                             { I18n::tr("geoc_map_single"),
                               I18n::tr("geoc_map_byname"),
                               I18n::tr("geoc_map_bycolor"),
                               I18n::tr("geoc_map_manual") }, 1));
    m_mapDefault = new QComboBox(s);
    m_mapDefault->addItem(I18n::tr("geoc_map_m2"));
    m_mapDefault->addItem(I18n::tr("geoc_map_m1"));
    m_mapDefault->addItem(I18n::tr("geoc_map_m3"));
    s->form()->addRow(I18n::tr("geoc_map_default"), m_mapDefault);
    return s;
}

// ── 取込プレビュー / Preview ───────────────────────────────────────────────
QWidget *GeometryTab::buildPreviewSection()
{
    auto *s = new SectionBox(I18n::tr("geoc_prev_section"));

    auto *br = new QHBoxLayout();
    m_prevTri   = makeBadge(I18n::tr("geoc_prev_tri"), kOk, s);
    m_prevSolid = makeBadge(I18n::tr("geoc_prev_solid"), kOk, s);
    m_prevVol   = makeBadge(I18n::tr("geoc_prev_vol"), kMuted, s);
    m_prevBbox  = makeBadge(I18n::tr("geoc_prev_bbox"), kMuted, s);
    br->addWidget(m_prevTri);
    br->addWidget(m_prevSolid);
    br->addWidget(m_prevVol);
    br->addWidget(m_prevBbox);
    br->addStretch(1);
    s->vbox()->addLayout(br);

    auto *hr = new QHBoxLayout();
    auto *runImport = new QPushButton(I18n::tr("geoc_prev_import"), s);
    hr->addWidget(runImport);
    hr->addWidget(new QPushButton(I18n::tr("geoc_prev_3d"), s));
    hr->addWidget(new QPushButton(I18n::tr("geoc_prev_measure"), s));
    hr->addStretch(1);
    s->vbox()->addLayout(hr);

    // 取込実行 = 実際の STL 取込 (io/StlImporter)
    connect(runImport, &QPushButton::clicked, this, &GeometryTab::importStl);
    return s;
}

// ── 取込済みモデル / Imported models ───────────────────────────────────────
QWidget *GeometryTab::buildImportedSection()
{
    auto *s = new SectionBox(I18n::tr("geoc_models_section"));

    m_modelTable = makeTable({ QString(), I18n::tr("geoc_col_name"),
                               I18n::tr("geoc_col_format"),
                               I18n::tr("geoc_col_tri"),
                               I18n::tr("geoc_col_vol"),
                               I18n::tr("geoc_col_matcol"), QString() },
                             4, s);
    struct Mdl { const char *name; const char *fmt; const char *tri;
                 const char *vol; const char *mat; bool on; };
    static const Mdl kMdl[4] = {
        { "radome_assembly.step", "STEP", "18,440", "2.27 cm³",
          "geoc_mdl_multi", true },
        { "antenna_housing.stl",  "STL",  "12,847", "1.45 cm³",
          "geoc_mdl_diel", true },
        { "connector.igs",        "IGES", "5,210",  "0.08 cm³",
          "geoc_mdl_pec", false },
        { "obstacle_metal.obj",   "OBJ",  "3,890",  "0.12 cm³",
          "geoc_mdl_pec", false },
    };
    for (int r = 0; r < 4; ++r) {
        m_modelTable->setItem(r, 0, checkItem(kMdl[r].on));
        m_modelTable->setItem(r, 1, textItem(QString::fromUtf8(kMdl[r].name)));
        m_modelTable->setItem(r, 2, textItem(QString::fromLatin1(kMdl[r].fmt)));
        m_modelTable->setItem(r, 3, numItem(QString::fromLatin1(kMdl[r].tri)));
        m_modelTable->setItem(r, 4, numItem(QString::fromUtf8(kMdl[r].vol)));
        m_modelTable->setItem(r, 5, textItem(I18n::tr(kMdl[r].mat)));
        m_modelTable->setCellWidget(r, 6,
            new QPushButton(I18n::tr("geoc_models_edit"), m_modelTable));
    }
    s->vbox()->addWidget(m_modelTable);
    s->vbox()->addWidget(makeHint(I18n::tr("geoc_models_hint"), s));
    return s;
}

// ── ボクセル化 / Voxelization (実行は io/Voxelizer の staircase 版) ─────────
QWidget *GeometryTab::buildVoxelSection()
{
    auto *s = new SectionBox(I18n::tr("geoc_vox_section"));
    s->vbox()->addWidget(makeHint(I18n::tr("geoc_vox_hint"), s));

    m_voxDelta = makeSpin(s, 0.001, 1000.0, 3, 0.5, 0.1);
    s->form()->addRow(I18n::tr("geoc_vox_delta"),
                      valueRow(s, m_voxDelta, "mm",
                               I18n::tr("geoc_vox_delta_hint")));
    s->form()->addRow(I18n::tr("geoc_vox_inout"),
                      segRow(s, &m_voxInside,
                             { I18n::tr("geoc_vox_ray"),
                               I18n::tr("geoc_vox_winding"),
                               I18n::tr("geoc_vox_sdf") }, 1));
    s->form()->addRow(I18n::tr("geoc_vox_surface"),
                      segRow(s, &m_voxSurface,
                             { I18n::tr("geoc_vox_stair"),
                               I18n::tr("geoc_vox_conformal"),
                               I18n::tr("geoc_vox_subcell") }, 1));
    // 行順を守るため、フォーム内に全幅行として差し込む
    s->form()->addRow(makeHint(I18n::tr("geoc_vox_surf_hint"), s));

    m_voxPvf = makeCheck(I18n::tr("geoc_vox_pvf"), true, s);
    s->form()->addRow(I18n::tr("geoc_vox_pvf_label"),
                      valueRow(s, m_voxPvf, QString(),
                               I18n::tr("geoc_vox_pvf_hint")));
    m_voxMerge = makeCheck(I18n::tr("geoc_vox_merge"), true, s);
    s->form()->addRow(I18n::tr("geoc_vox_multi"),
                      valueRow(s, m_voxMerge, QString(),
                               I18n::tr("geoc_vox_merge_hint")));

    auto *octRow = new QWidget(s);
    auto *oh = new QHBoxLayout(octRow);
    oh->setContentsMargins(0, 0, 0, 0);
    oh->setSpacing(6);
    m_voxOctree = makeCheck(I18n::tr("geoc_vox_oct_adapt"), false, octRow);
    oh->addWidget(m_voxOctree);
    oh->addWidget(makeHint(I18n::tr("geoc_vox_oct_level"), octRow));
    m_voxOctLevel = makeIntSpin(octRow, 1, 8, 3);
    oh->addWidget(m_voxOctLevel);
    oh->addStretch(1);
    s->form()->addRow(I18n::tr("geoc_vox_octree"), octRow);

    m_voxGpu = makeCheck(I18n::tr("geoc_vox_gpu"), true, s);
    s->form()->addRow(I18n::tr("geoc_vox_gpu_label"), m_voxGpu);

    // 実行行: ボクセル化 (実処理) + 材質番号 + 占有セルバッジ
    auto *runRow = new QHBoxLayout();
    m_voxBtn = new QPushButton(I18n::tr("ge_voxelize_btn"), s);
    m_voxBtn->setEnabled(false);
    runRow->addWidget(m_voxBtn);
    runRow->addWidget(new QPushButton(I18n::tr("geoc_vox_preview"), s));
    runRow->addWidget(new QLabel(I18n::tr("ge_voxel_mat"), s));
    m_voxMat = new QSpinBox(s);
    m_voxMat->setRange(1, 9999);
    m_voxMat->setValue(2);
    runRow->addWidget(m_voxMat);
    m_voxBadge = makeBadge(I18n::tr("geoc_vox_badge"), kOk, s);
    runRow->addWidget(m_voxBadge);
    runRow->addStretch(1);
    s->vbox()->addLayout(runRow);
    return s;
}

// ── ボクセル統計 / Voxel statistics ────────────────────────────────────────
QWidget *GeometryTab::buildVoxelStatsSection()
{
    auto *s = new SectionBox(I18n::tr("geoc_stat_section"));

    m_statOcc = makeMono(I18n::tr("geoc_stat_occ_val"), s);
    s->form()->addRow(I18n::tr("geoc_stat_occ"), m_statOcc);
    m_statBnd = makeMono(I18n::tr("geoc_stat_bnd_val"), s);
    s->form()->addRow(I18n::tr("geoc_stat_bnd"), m_statBnd);

    auto *errRow = new QWidget(s);
    auto *eh = new QHBoxLayout(errRow);
    eh->setContentsMargins(0, 0, 0, 0);
    eh->setSpacing(6);
    m_statErr = makeMono(I18n::tr("geoc_stat_err_val"), errRow);
    eh->addWidget(m_statErr);
    eh->addWidget(makeBadge(I18n::tr("geoc_stat_ok"), kOk, errRow));
    eh->addStretch(1);
    s->form()->addRow(I18n::tr("geoc_stat_err"), errRow);

    m_statConf = makeMono(I18n::tr("geoc_stat_conf_val"), s);
    s->form()->addRow(I18n::tr("geoc_stat_conf"), m_statConf);
    return s;
}

// ── メッシュ細分化 / Mesh refinement ───────────────────────────────────────
QWidget *GeometryTab::buildRefineSection()
{
    auto *s = new SectionBox(I18n::tr("geoc_ref_section"));
    s->vbox()->addWidget(makeHint(I18n::tr("geoc_ref_hint"), s));

    s->form()->addRow(I18n::tr("geoc_ref_method"),
                      segRow(s, &m_refMethod,
                             { I18n::tr("geoc_ref_local"),
                               I18n::tr("geoc_ref_subgrid"),
                               I18n::tr("geoc_ref_amr") }, 0));

    auto *tgtRow = new QWidget(s);
    auto *th = new QHBoxLayout(tgtRow);
    th->setContentsMargins(0, 0, 0, 0);
    th->setSpacing(6);
    m_refEdge    = makeCheck(I18n::tr("geoc_ref_edge"), true, tgtRow);
    m_refCurve   = makeCheck(I18n::tr("geoc_ref_curve"), true, tgtRow);
    m_refThin    = makeCheck(I18n::tr("geoc_ref_thin"), false, tgtRow);
    m_refHighEps = makeCheck(I18n::tr("geoc_ref_higheps"), false, tgtRow);
    th->addWidget(m_refEdge);
    th->addWidget(m_refCurve);
    th->addWidget(m_refThin);
    th->addWidget(m_refHighEps);
    th->addStretch(1);
    s->form()->addRow(I18n::tr("geoc_ref_target"), tgtRow);

    m_refRatio = makeIntSpin(s, 1, 9, 3);
    s->form()->addRow(I18n::tr("geoc_ref_ratio"),
                      valueRow(s, m_refRatio, QString(),
                               I18n::tr("geoc_ref_ratio_hint")));
    m_refTransition = makeIntSpin(s, 1, 50, 5);
    s->form()->addRow(I18n::tr("geoc_ref_trans"),
                      valueRow(s, m_refTransition, "cells",
                               I18n::tr("geoc_ref_trans_hint")));
    m_refLambdaN = makeIntSpin(s, 5, 200, 20);
    s->form()->addRow(I18n::tr("geoc_ref_lambda"),
                      valueRow(s, m_refLambdaN,
                               I18n::tr("geoc_ref_lambda_unit")));

    auto *cr = new QHBoxLayout();
    m_refAutoCheck = makeCheck(I18n::tr("geoc_ref_autocheck"), true, s);
    m_refShowViol  = makeCheck(I18n::tr("geoc_ref_showviol"), true, s);
    cr->addWidget(m_refAutoCheck);
    cr->addWidget(m_refShowViol);
    cr->addStretch(1);
    s->vbox()->addLayout(cr);

    auto *rr = new QHBoxLayout();
    rr->addWidget(new QPushButton(I18n::tr("geoc_ref_run"), s));
    m_refBadge = makeBadge(QString(), kWarn, s);
    rr->addWidget(m_refBadge);
    rr->addStretch(1);
    s->vbox()->addLayout(rr);

    // モックの "+312% (推定)" は「全セルの 12% を r³ に分割」した増加率
    //   増加率 = 0.12 · (r³ − 1) · 100  →  r = 3 で +312%
    auto updateBadge = [this] {
        const double r = m_refRatio->value();
        const double inc = 0.12 * (r * r * r - 1.0) * 100.0;
        m_refBadge->setText(
            I18n::tr("geoc_ref_badge_fmt").arg(qRound(inc)));
    };
    updateBadge();
    connect(m_refRatio, &QSpinBox::valueChanged, this,
            [updateBadge](int) { updateBadge(); });
    return s;
}

// ── 細分化領域 / Refined regions ───────────────────────────────────────────
QWidget *GeometryTab::buildRefinedRegionsSection()
{
    auto *s = new SectionBox(I18n::tr("geoc_regions_section"));
    m_refTable = makeTable({ QString(), I18n::tr("geoc_col_region"),
                             I18n::tr("geoc_col_range"),
                             I18n::tr("geoc_col_ratio"),
                             I18n::tr("geoc_col_dcells") }, 3, s);
    struct Reg { bool on; const char *name; const char *range;
                 const char *ratio; const char *dcells; };
    static const Reg kReg[3] = {
        { true,  "patch_edge", "[-5,5]×[-1,1]×[0,1] mm", "3x",   "+1,420" },
        { true,  "via_region", nullptr,                  "5x",   "+800"   },
        { false, "far_region", "[-30,30]³ mm",           "0.5x", "-2,100" },
    };
    for (int r = 0; r < 3; ++r) {
        m_refTable->setItem(r, 0, checkItem(kReg[r].on));
        m_refTable->setItem(r, 1, textItem(QString::fromLatin1(kReg[r].name)));
        m_refTable->setItem(r, 2, textItem(kReg[r].range
            ? QString::fromUtf8(kReg[r].range) : I18n::tr("geoc_rr2_range")));
        m_refTable->setItem(r, 3, numItem(QString::fromLatin1(kReg[r].ratio)));
        m_refTable->setItem(r, 4, numItem(QString::fromLatin1(kReg[r].dcells)));
    }
    s->vbox()->addWidget(m_refTable);
    return s;
}

void GeometryTab::importStl()
{
    const QString path = QFileDialog::getOpenFileName(
        this, I18n::tr("ge_import_btn"), {}, "STL (*.stl);;All files (*)");
    if (path.isEmpty()) return;

    ImportedMesh mesh;
    QString err;
    if (!StlImporter::load(path, mesh, &err)) {
        m_importInfo->setText("error: " + err);
        return;
    }

    // Keep the mesh so the user can voxelize it onto the Yee grid.
    m_lastMesh = mesh;
    m_hasMesh = true;
    m_voxBtn->setEnabled(true);
    if (m_cadFile) m_cadFile->setText(path);

    m_importInfo->setText(QStringLiteral(
        "%1 — %2 triangles, area %3 m², bbox [%4, %5]×[%6, %7]×[%8, %9]\n%10")
        .arg(mesh.name).arg(mesh.numTriangles)
        .arg(QString::number(mesh.surfaceArea, 'g', 4))
        .arg(QString::number(mesh.bbox[0], 'g', 4), QString::number(mesh.bbox[3], 'g', 4),
             QString::number(mesh.bbox[1], 'g', 4), QString::number(mesh.bbox[4], 'g', 4),
             QString::number(mesh.bbox[2], 'g', 4), QString::number(mesh.bbox[5], 'g', 4))
        .arg(I18n::tr("ge_voxelize_hint")));

    refreshImportBadges();
}

void GeometryTab::voxelizeImported()
{
    if (!m_hasMesh) return;

    const VoxelResult res = Voxelizer::voxelize(
        m_lastMesh, m_p->mesh(0), m_p->mesh(1), m_p->mesh(2),
        m_voxMat->value());
    if (!res.ok) {
        m_importInfo->setText("voxelize error: " + res.error);
        return;
    }

    for (Geometry g : res.bricks) {
        g.name = m_lastMesh.name + " (voxel)";
        m_p->geometries().push_back(g);
    }
    refresh();
    m_p->touch();

    m_importInfo->setText(QStringLiteral(
        "%1: %2×%3×%4 grid → %L5 occupied cells (%L6 bricks) → material %7")
        .arg(m_lastMesh.name).arg(res.nx).arg(res.ny).arg(res.nz)
        .arg(res.occupied).arg(res.bricks.size()).arg(m_voxMat->value()));

    m_voxOccupied = res.occupied;
    m_voxTotal    = qint64(res.nx) * res.ny * res.nz;
    m_hasVox      = true;
    refreshVoxelStats();
}

// 取込プレビュー / 取込済みモデル を実メッシュの診断値で置き換える。
void GeometryTab::refreshImportBadges()
{
    if (!m_hasMesh || !m_prevTri) return;

    const double volCm3 = meshVolume(m_lastMesh) * 1e6;   // m³ → cm³
    const double sx = (m_lastMesh.bbox[3] - m_lastMesh.bbox[0]) * 1e3;
    const double sy = (m_lastMesh.bbox[4] - m_lastMesh.bbox[1]) * 1e3;
    const double sz = (m_lastMesh.bbox[5] - m_lastMesh.bbox[2]) * 1e3;

    m_prevTri->setText(I18n::tr("geoc_prev_tri_fmt")
                           .arg(groupNum(m_lastMesh.numTriangles)));
    m_prevSolid->setText(I18n::tr("geoc_prev_solid_fmt")
                             .arg(QString::number(m_lastMesh.surfaceArea, 'g', 4)));
    m_prevVol->setText(I18n::tr("geoc_prev_vol_fmt")
                           .arg(QString::number(volCm3, 'f', 2)));
    m_prevBbox->setText(I18n::tr("geoc_prev_bbox_fmt")
                            .arg(QString::number(sx, 'f', 1),
                                 QString::number(sy, 'f', 1),
                                 QString::number(sz, 'f', 1)));

    // 取込済みモデル表の先頭行を「現在の取込」として更新 (無ければ挿入)
    if (!m_modelTable) return;
    if (m_liveModelRow < 0) {
        m_modelTable->insertRow(0);
        m_modelTable->setItem(0, 0, checkItem(true));
        m_liveModelRow = 0;
    }
    const QString base = QFileInfo(m_lastMesh.sourcePath).fileName();
    m_modelTable->setItem(m_liveModelRow, 1, textItem(
        I18n::tr("geoc_mdl_live").arg(base.isEmpty() ? m_lastMesh.name : base)));
    m_modelTable->setItem(m_liveModelRow, 2, textItem("STL"));
    m_modelTable->setItem(m_liveModelRow, 3,
                          numItem(groupNum(m_lastMesh.numTriangles)));
    m_modelTable->setItem(m_liveModelRow, 4,
                          numItem(QString::number(volCm3, 'f', 2) + " cm³"));
    m_modelTable->setItem(m_liveModelRow, 5,
                          textItem(QString::number(m_voxMat->value())));
}

// ボクセル統計を実際の staircase ボクセル化結果で置き換える。
// 境界セル数 / 形状誤差 / 共形セル比率 は libigl 版 (PVF・共形) の担当なので
// staircase では未算出であることを明示する (docs/libigl-integration.md)。
void GeometryTab::refreshVoxelStats()
{
    if (!m_hasVox || !m_statOcc) return;

    const double pct = m_voxTotal > 0
        ? 100.0 * double(m_voxOccupied) / double(m_voxTotal) : 0.0;
    m_statOcc->setText(I18n::tr("geoc_stat_occ_fmt")
                           .arg(groupNum(m_voxOccupied), groupNum(m_voxTotal),
                                QString::number(pct, 'f', 1)));
    m_statBnd->setText(I18n::tr("geoc_stat_na"));
    m_statErr->setText(QString::fromUtf8("—"));
    m_statConf->setText(I18n::tr("geoc_stat_stair"));

    if (m_voxBadge)
        m_voxBadge->setText(I18n::tr("geoc_vox_badge_fmt")
                                .arg(groupNum(m_voxTotal),
                                     groupNum(m_voxOccupied)));
}

void GeometryTab::applyTable()
{
    auto &gs = m_p->geometries();
    for (int r = 0; r < m_table->rowCount() && r < gs.size(); ++r) {
        Geometry &g = gs[r];
        auto cell = [this, r](int c) {
            auto *it = m_table->item(r, c);
            return it ? it->text() : QString();
        };
        g.materialId = cell(0).toInt();
        if (auto *cb = qobject_cast<QComboBox *>(m_table->cellWidget(r, 1)))
            g.shape = kShapeCodes[cb->currentIndex()];
        for (int i = 0; i < 8; ++i)
            g.g[i] = cell(2 + i).toDouble();
        g.name = cell(10);
    }
}

void GeometryTab::refresh()
{
    m_updating = true;
    const auto &gs = m_p->geometries();
    m_table->setRowCount(gs.size());
    for (int r = 0; r < gs.size(); ++r) {
        const Geometry &g = gs[r];
        m_table->setItem(r, 0, new QTableWidgetItem(QString::number(g.materialId)));

        auto *shape = new QComboBox(m_table);
        for (int code : kShapeCodes)
            shape->addItem(I18n::tr("ge_shape_" + QString::number(code)));
        shape->setCurrentIndex(shapeIndex(g.shape));
        connect(shape, &QComboBox::currentIndexChanged, this, [this] {
            if (m_updating) return;
            applyTable();
            m_p->touch();
        });
        m_table->setCellWidget(r, 1, shape);

        const int np = Geometry::paramCount(g.shape);
        for (int i = 0; i < 8; ++i) {
            auto *it = new QTableWidgetItem(
                i < np ? QString::number(g.g[i], 'g', 8) : QString());
            if (i >= np) it->setFlags(it->flags() & ~Qt::ItemIsEditable);
            m_table->setItem(r, 2 + i, it);
        }
        m_table->setItem(r, 10, new QTableWidgetItem(g.name));
    }
    m_nav->setRange(gs.size());
    m_updating = false;
}
