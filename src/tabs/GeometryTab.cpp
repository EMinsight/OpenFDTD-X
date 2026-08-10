// GeometryTab.cpp
#include "GeometryTab.h"
#include "../core/Project.h"
#include "../io/MeshImporter.h"
#include "../io/Voxelizer.h"
#include "../widgets/SectionBox.h"
#include "../widgets/UnitNav.h"
#include "../I18n.h"
#include "TabHelpers.h"

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
#include <QMessageBox>
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
      "▸ STEP/IGES など B-rep CAD の実取込は未実装 — 現在取込できるのは "
      "STL / OBJ / PLY です (STEP/IGES 等の B-rep CAD は OCCT 連携が要るため未対応)。",
      "▸ Importing STEP/IGES and other B-rep CAD is not implemented yet — "
      "STL, OBJ and PLY can be imported (B-rep CAD such as STEP/IGES needs "
      "support is planned)." },
    { "geoc_file_ph", "例: antenna.stl (STL / OBJ / PLY を取込可)",
      "e.g. antenna.stl (STL / OBJ / PLY)" },

    // ── マウス操作 / Mouse shortcuts ────────────────────────────────────────
    // 実装済みの操作のみ掲載する (Viewport3D::mouse*Event / wheelEvent 準拠)。
    // モック由来の選択/ギズモ/スナップ等は未実装のため表から除外し注記に集約。
    { "geoc_mouse_section", "マウス操作 / Mouse shortcuts", "Mouse shortcuts" },
    { "geoc_col_op", "操作", "Action" },
    { "geoc_col_key", "キー / マウス", "Key / mouse" },
    { "geoc_col_effect", "動作", "Effect" },
    { "geoc_ms10_o", "カメラ回転", "Orbit" },
    { "geoc_ms10_k", "左ドラッグ", "Left drag" },
    { "geoc_ms10_a", "視点旋回", "Orbit the view" },
    { "geoc_ms11_o", "パン", "Pan" },
    { "geoc_ms11_k", "中ドラッグ", "Middle drag" },
    { "geoc_ms11_a", "平行移動", "Translate the view" },
    { "geoc_ms13_o", "ズーム", "Zoom" },
    { "geoc_ms13_k", "ホイール (ダブルクリック=全体表示)",
      "Wheel (double-click = fit view)" },
    { "geoc_ms13_a", "拡大縮小", "Zoom in / out" },
    { "geoc_mouse_todo",
      "▸ 3D ビュー内での選択・ギズモ移動/回転/スケール・軸ロック・スナップ・"
      "数字キービュー切替は未実装 (今後追加予定)。ユニットの編集は上の"
      "「ユニット編集」節を使用してください。",
      "▸ In-viewport selection, gizmo translate/rotate/scale, axis lock, "
      "snapping and number-key views are not implemented yet (planned). "
      "Use the Unit transform section above to edit units." },

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
    // STEP の部品階層は取込未実装 (外部 CAD カーネルが必要)。取込済みの
    // STL があればその実測値を単一部品として表示する。
    { "geoc_asm_section", "アセンブリツリー / Assembly tree", "Assembly tree" },
    { "geoc_asm_hint",
      "取込済みメッシュの構成を表示します (実測値)。",
      "Shows the structure of the imported mesh (measured values)." },
    { "geoc_asm_none",
      "取込済みモデルがありません — 上の「3Dモデル取込」または「取込プレビュー」"
      "の「📥 取込実行」でモデルを取り込むと、ここに実測値が表示されます。",
      "No model imported yet — load a mesh with \"📥 Run import\" (in "
      "\"Import 3D CAD model\" or \"Import preview\" above) and the measured "
      "values will appear here." },
    { "geoc_asm_note",
      "▸ STEP/IGES の部品階層 (アセンブリ) の取込は外部 CAD カーネルを要する"
      "ため未実装です。STL / OBJ / PLY は単一メッシュなので 1 部品として表示されます。",
      "▸ Importing the STEP/IGES part hierarchy needs an external CAD kernel "
      "and is not implemented. STL / OBJ / PLY are single meshes, so they show "
      "part." },
    { "geoc_asm_root_fmt", "1 mesh · %1 三角形 · 単位=m",
      "1 mesh · %1 triangles · unit=m" },
    { "geoc_asm_tri", "三角形数", "Triangles" },
    { "geoc_asm_vert", "頂点数 (溶接後)", "Vertices (welded)" },
    { "geoc_asm_vert_na", "— (検査省略)", "— (check skipped)" },
    { "geoc_asm_bbox", "bbox", "bbox" },
    { "geoc_asm_area", "表面積", "Surface area" },
    { "geoc_asm_vol", "体積 (閉メッシュ前提)", "Volume (closed mesh assumed)" },
    { "geoc_asm_placed", "配置・変換の適用後の座標",
      "Coordinates after placement" },
    { "geoc_asm_assign", "材質一括割当", "Assign materials in bulk" },
    { "geoc_asm_autoignore", "小部品を自動無視 (<λ/20)",
      "Auto-ignore small parts (<λ/20)" },

    // ── 配置・変換 / Placement ──────────────────────────────────────────────
    { "geoc_place_section", "配置・変換 / Placement", "Placement" },
    { "geoc_place_unit", "単位", "Unit" },
    { "geoc_place_scale", "× スケール", "× scale" },
    { "geoc_place_offset", "オフセット (x,y,z)", "Offset (x,y,z)" },
    { "geoc_place_rot", "回転 (XYZ)", "Rotation (XYZ)" },
    { "geoc_place_center", "モデル bbox 中心を原点に整列",
      "Align the model bbox centre with the origin" },
    { "geoc_place_autoaxis", "主軸を自動検出", "Auto-detect principal axes" },
    { "geoc_place_hint",
      "取込メッシュの頂点に スケール (単位換算 ×係数) → 中心合わせ → 回転 "
      "(X→Y→Z, 原点基準) → オフセット [m] の順で適用され、プレビュー・計測・"
      "ボクセル化に反映されます。",
      "Applied to the imported mesh vertices as scale (unit conversion × "
      "factor) → centring → rotation (X→Y→Z about the origin) → offset [m]; "
      "reflected in the preview, measurement and voxelization." },

    // ── ジオメトリ修復 / Healing ────────────────────────────────────────────
    // 「検出」は io/MeshDiagnostics が取込メッシュから実計算した値。
    // 「修復」(縫合・法線統一・デシメーション) は未実装 — 検出のみ。
    { "geoc_heal_section", "ジオメトリ検査 / Mesh check",
      "Mesh check" },
    { "geoc_heal_hint",
      "取込メッシュの位相・幾何を実際に検査します。ボクセル化 (内外判定) は"
      "閉じた多様体メッシュを前提とするため、境界エッジ・非多様体エッジが 0 で"
      "あることが目安です。",
      "Runs an actual topological/geometric check on the imported mesh. "
      "Voxelization (inside/outside test) assumes a closed manifold mesh, so "
      "boundary and non-manifold edge counts should be zero." },
    { "geoc_col_process", "検査項目", "Check" },
    { "geoc_col_detect", "検出", "Detected" },
    { "geoc_col_state", "状態", "State" },
    { "geoc_hd1_p", "重複頂点 (溶接対象)", "Duplicate vertices (weldable)" },
    { "geoc_hd2_p", "縮退三角形 (面積 0)", "Degenerate triangles (zero area)" },
    { "geoc_hd3_p", "境界エッジ (穴・隙間)", "Boundary edges (holes / gaps)" },
    { "geoc_hd4_p", "非多様体エッジ (3面以上が共有)",
      "Non-manifold edges (shared by ≥3 faces)" },
    { "geoc_hd5_p", "法線の向きの不一致", "Inconsistent normal orientation" },
    { "geoc_hd6_p", "閉ソリッド判定 (水密)", "Closed-solid check (watertight)" },
    { "geoc_heal_cnt", "%1 件", "%1" },
    { "geoc_heal_ok", "OK", "OK" },
    { "geoc_heal_info", "情報", "info" },
    { "geoc_heal_ng", "要修正", "needs fixing" },
    { "geoc_heal_wt_ok", "水密", "watertight" },
    { "geoc_heal_wt_ng", "非水密", "not watertight" },
    { "geoc_heal_pending", "— (未取込)", "— (not imported)" },
    { "geoc_heal_none",
      "▸ モデル未取込 — モデルを取り込むと、この表は実メッシュから計算した"
      "検出数で埋まります。",
      "▸ No model imported — once a mesh is loaded this table is filled with "
      "counts computed from the actual mesh." },
    { "geoc_heal_note",
      "▸ 検出数はこの表で実計算した値です。自動修復 (縫合・法線統一・"
      "デシメーション) は未実装のため、修正はモデリング側で行ってください。",
      "▸ The counts above are computed here from the mesh. Auto-repair "
      "(sewing, normal unification, decimation) is not implemented — fix the "
      "model in your CAD/mesh tool." },
    { "geoc_heal_skip",
      "▸ 三角形数が %1 を超えるため検査を省略しました "
      "(GUI の応答性確保のため)。",
      "▸ The check was skipped because the mesh exceeds %1 triangles (to keep "
      "the GUI responsive)." },
    { "geoc_heal_tol", "▸ 頂点溶接の許容差: %1 m (bbox 対角 × 1e-6)",
      "▸ Vertex weld tolerance: %1 m (bbox diagonal × 1e-6)" },
    { "geoc_vox_inout_hint",
      "レイの偶奇は閉じたメッシュでは厳密で速い。巻き数は穴があっても崩れ"
      "ませんが、法線の向きが揃っていることが前提です (揃っていなければ"
      "「ジオメトリ修復」を先に実行してください)。",
      "Ray parity is exact and fast on a closed mesh. The winding number "
      "survives holes, but it requires consistently oriented normals — run "
      "\"Repair geometry\" first if they are not." },
    { "geoc_vox_wind_needs_normals",
      "法線の向きが揃っていません (不一致な辺が %1 本)。一般化巻き数は"
      "向きが揃っていることが前提で、このままでは全セルが「外」と判定されます。"
      "「ジオメトリ修復」で法線を統一してから実行してください "
      "(またはレイの偶奇を選んでください)。",
      "The normals are not consistently oriented (%1 inconsistent edges). The "
      "generalized winding number requires a consistent orientation; as it is, "
      "every cell would be classified as outside. Run \"Repair geometry\" to "
      "unify the normals first (or choose ray parity)." },
    { "geoc_vox_engine_note",
      "▸ 内外判定 (レイ/巻き数)・部分体積率・「連続セルをまとめる」は実際に"
      "効きます。表面処理の共形・サブセル、八分木、GPU はエンジン未実装です。",
      "▸ The inside test (ray / winding), the partial volume fraction and "
      "\"merge runs\" really take effect. Conformal and sub-cell surface "
      "handling, the octree and GPU are not implemented in the engine." },
    { "geoc_autoaxis_tip",
      "取込メッシュの面積重み付き慣性主軸を求め、それを X/Y/Z へ揃える回転角を"
      "回転欄へ入れます (以後は手で微調整できます)。頂点の多い面に引きずられ"
      "ないよう、頂点ではなく面積で重み付けします。",
      "Finds the area-weighted principal axes of the imported mesh and fills the "
      "rotation fields with the angles that align them to X/Y/Z (you can still "
      "adjust them by hand). Weighting is by area, not by vertex count, so a "
      "finely tessellated face does not drag the result." },
    { "geoc_autoaxis_done",
      "▸ 主軸を検出しました: 回転 %1° / %2° / %3° (X→Y→Z) を入れました。",
      "▸ Principal axes found: rotation %1° / %2° / %3° (X→Y→Z) applied." },
    { "geoc_autoaxis_degenerate",
      "▸ 主軸が縮退しています (立方体・球のように回しても同じ形) — "
      "向きが一意に決まらないので回転は入れませんでした。",
      "▸ The principal axes are degenerate (the shape looks the same when "
      "rotated, like a cube or a sphere) — no unique orientation exists, so no "
      "rotation was applied." },
    { "geoc_autoaxis_nomesh", "▸ メッシュを取り込むと主軸を検出します。",
      "▸ Import a mesh to detect its principal axes." },
    { "geoc_autoaxis_fail", "▸ 主軸を検出できません (面積が 0)。",
      "▸ Cannot find the principal axes (zero surface area)." },
    { "geoc_heal_run", "🔧 自動修復実行", "🔧 Run auto-heal" },
    { "geoc_heal_tip",
      "頂点溶接・縮退三角形の除去・法線の統一を行い、検査をやり直します。"
      "穴埋め (境界エッジの解消) は行いません — 塞ぎ方が一意でなく、"
      "形状を変えてしまうためです。",
      "Welds vertices, drops degenerate triangles and unifies the normals, "
      "then re-runs the checks. Holes are NOT filled — there is no unique way "
      "to close them and doing so would change the shape." },
    { "geoc_heal_done",
      "▸ 修復しました: 頂点溶接 %1、縮退三角形の除去 %2、法線の反転 %3。",
      "▸ Repaired: %1 vertices welded, %2 degenerate triangles removed, "
      "%3 faces flipped." },
    { "geoc_heal_outward",
      "閉じた成分 %1 個を外向きへ揃えました。",
      "%1 closed component(s) were flipped to face outward." },
    { "geoc_heal_holes",
      "境界エッジが %1 本残っています (穴埋めは未実装 — "
      "元の CAD で塞いでください)。",
      "%1 boundary edge(s) remain (hole filling is not implemented — close "
      "them in the original CAD)." },
    { "geoc_heal_watertight", "水密になりました。", "The mesh is now watertight." },
    { "geoc_heal_toolarge",
      "三角形が多すぎるため修復していません (検査の上限を超えています)。",
      "Not repaired — the mesh exceeds the triangle limit for the checks." },
    { "geoc_heal_failed",
      "修復できませんでした (三角形が残りません)。",
      "Could not repair (no triangle would be left)." },
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
    // 音響 (室内) / 水中: εr 系ではなく ρ/c 系の材料名 (ラベルのみ — 未配線)
    { "geoc_map_ac2", "2 — 空気 (ρ=1.2, c=343)", "2 — air (ρ=1.2, c=343)" },
    { "geoc_map_ac1", "1 — 剛壁 (rigid)", "1 — rigid wall" },
    { "geoc_map_ac3", "3 — 多孔質吸音材", "3 — porous absorber" },
    { "geoc_map_uw2", "2 — 海水 (ρ=1025, c=1500)",
      "2 — seawater (ρ=1025, c=1500)" },
    { "geoc_map_uw3", "3 — 堆積層 (ρ=1600, c=1600)",
      "3 — sediment (ρ=1600, c=1600)" },

    // ── 取込プレビュー / Preview ────────────────────────────────────────────
    // 取込前は実測値が無いので「—」を出す (固定サンプル値は出さない)。
    { "geoc_prev_section", "取込プレビュー / Preview", "Import preview" },
    { "geoc_prev_dash", "—", "—" },
    { "geoc_prev_tri_fmt", "%1 三角形", "%1 triangles" },
    { "geoc_prev_solid_fmt", "1 ソリッド · 表面積 %1 m²",
      "1 solid · area %1 m²" },
    { "geoc_prev_vol_fmt", "体積 %1 cm³", "volume %1 cm³" },
    { "geoc_prev_bbox_fmt", "bbox %1×%2×%3 mm", "bbox %1×%2×%3 mm" },
    { "geoc_prev_none",
      "▸ モデル未取込 — 「📥 取込実行」でモデルを取り込むと、実測した"
      "三角形数・表面積・体積・bbox がここに表示されます。",
      "▸ No model imported — run \"📥 Run import\" on a mesh and the measured "
      "triangle count, area, volume and bbox appear here." },
    { "geoc_prev_import", "📥 取込実行", "📥 Run import" },
    { "geoc_prev_3d", "👁 3Dプレビュー", "👁 3D preview" },
    { "geoc_prev_measure", "📐 寸法測定", "📐 Measure" },

    // ── 寸法測定 / Measure (取込メッシュの実測値ダイアログ) ─────────────────
    { "geoc_meas_title", "寸法測定", "Measure" },
    { "geoc_meas_none",
      "取込済みのモデルがありません。先に「取込実行」でモデルを取り込んで"
      "ください。",
      "No imported model yet — load a mesh with \"Run import\" first." },
    { "geoc_meas_model", "モデル", "Model" },
    { "geoc_meas_tri", "三角形数", "Triangles" },
    { "geoc_meas_bbox", "bbox 寸法", "Bbox size" },
    { "geoc_meas_range", "bbox 範囲 [m]", "Bbox extent [m]" },
    { "geoc_meas_area", "表面積", "Surface area" },
    { "geoc_meas_vol", "体積 (閉メッシュ前提)",
      "Volume (assumes a closed mesh)" },
    { "geoc_meas_placed",
      "※ 配置・変換 (スケール/回転/オフセット) 適用後の値です。",
      "Values are after placement (scale / rotation / offset)." },
    { "geoc_meas_pick",
      "▸ 2点間クリック計測は 3D ピッキング未実装のため未対応です。",
      "▸ Point-to-point click measurement is not available (3D picking is "
      "not implemented)." },

    // ── 取込済みモデル / Imported models ────────────────────────────────────
    { "geoc_models_section", "取込済みモデル / Imported models",
      "Imported models" },
    { "geoc_col_name", "名前", "Name" },
    { "geoc_col_format", "形式", "Format" },
    { "geoc_col_tri", "三角形", "Triangles" },
    { "geoc_col_vol", "体積", "Volume" },
    { "geoc_col_matcol", "物性", "Material" },
    { "geoc_models_hint",
      "▸ 取込後は「ボクセル化」で Yee 格子へ変換 → FDTD 計算へ",
      "▸ After import, convert to the Yee grid in Voxelization → FDTD run" },
    { "geoc_mdl_live", "%1 (現在の取込)", "%1 (live import)" },
    { "geoc_models_none",
      "▸ 取込済みモデルはありません — 「📥 取込実行」でモデルを取り込むと"
      "一覧に追加されます (同時に保持できるのは 1 モデルです)。",
      "▸ No imported models — run \"📥 Run import\" on a mesh to add one "
      "(only one model is held at a time)." },

    // ── ボクセル化 / Voxelization ───────────────────────────────────────────
    { "geoc_vox_section", "ボクセル化 / Voxelization", "Voxelization" },
    { "geoc_vox_hint",
      "取込3Dモデルを Yee グリッドに分解します。FDTDで扱える形状 "
      "(直方体ボクセル列) に変換。",
      "Decomposes the imported 3D model onto the Yee grid — into the brick-"
      "voxel runs FDTD can handle." },
    { "geoc_vox_delta", "分解度 Δ", "Resolution Δ" },
    { "geoc_vox_delta_hint", "→ λ/22 @ 2.5 GHz", "→ λ/22 @ 2.5 GHz" },
    // 分解度Δ の評価点はドメインの代表波長/周波数で切り替える
    { "geoc_vox_delta_hint_opt", "→ λ/22 @ 1550 nm", "→ λ/22 @ 1550 nm" },
    { "geoc_vox_delta_hint_ac",  "→ λ/22 @ 1 kHz",   "→ λ/22 @ 1 kHz" },
    { "geoc_vox_delta_hint_uw",  "→ λ/22 @ 3.5 kHz", "→ λ/22 @ 3.5 kHz" },
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
    // 音響 (室内) では PEC 薄板ではなく剛壁 (rigid) の表現にする
    { "geoc_vox_surf_hint_ac",
      "▸ 階段: 高速・実装容易・形状誤差大　▸ 共形: 境界変形で精度向上・推奨　"
      "▸ サブセル: 厚さ0の剛壁 (rigid) 薄板・薄い仕切りに最適",
      "▸ Staircase: fast, simple, large shape error　▸ Conformal: deformed "
      "boundary, more accurate (recommended)　▸ Sub-cell: best for zero-"
      "thickness rigid plates and thin partitions" },
    { "geoc_vox_pvf_label", "部分容積判定", "Partial volume" },
    { "geoc_vox_pvf", "PVF (Partial Volume Fraction)",
      "PVF (partial volume fraction)" },
    // 実装は「材質の内挿」ではなく「占有判定を体積率で行う」。
    // .ofd の geometry は直方体単位なので、セルを部分的に埋める表現は持てない。
    { "geoc_vox_pvf_hint",
      "面が横切るセルだけを 4³=64 点で再標本化し、体積率 50% 以上を占有とする "
      "(切るとセル中心 1 点判定)。形状誤差の実測にも使う",
      "Re-samples only the cells the surface crosses with 4^3 = 64 points and "
      "marks a cell occupied when at least 50% of it is material (off = a "
      "single centre sample). Also gives the measured shape error" },
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
    { "geoc_stat_bnd_fmt", "%1 (1 セルあたり %2 点で再標本化)",
      "%1 (re-sampled with %2 points per cell)" },
    { "geoc_stat_err_fmt", "階段 %1% / PVF %2% (メッシュ体積との差)",
      "staircase %1% / PVF %2% (vs. the mesh volume)" },
    { "geoc_stat_novol", "— (メッシュの体積が取れない)",
      "— (the mesh has no usable volume)" },
    { "geoc_stat_stair", "0.0% (階段近似)", "0.0% (staircase)" },
    { "geoc_stat_na", "— (PVF 無効)", "— (PVF off)" },
    { "geoc_stat_notrun", "— (未実行)", "— (not run yet)" },

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
    // 音響/水中では εr ではなく音速コントラストが細分化の対象になる
    { "geoc_ref_highc", "高音速コントラスト領域",
      "High sound-speed-contrast regions" },
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
    // セル増加は「細分化領域」表の定義と現在の基本格子から数えた実際の値。
    // 格子そのものは変わらない (細分化エンジン未実装) ので机上値と明記する。
    { "geoc_ref_growth_fmt",
      "セル増加見積り: %1 セル (基本 %2 セル比 %3%) — 領域定義からの机上値",
      "Estimated cell growth: %1 cells (%3% of the %2 base cells) — computed "
      "from the region definitions" },
    { "geoc_ref_growth_none",
      "セル増加見積り: — (細分化領域が未定義)",
      "Estimated cell growth: — (no refined region defined)" },
    { "geoc_ref_growth_nomesh",
      "セル増加見積り: — (格子が未定義)",
      "Estimated cell growth: — (no mesh defined)" },

    // ── 細分化領域 / Refined regions ────────────────────────────────────────
    { "geoc_regions_section", "細分化領域 / Refined regions",
      "Refined regions" },
    { "geoc_col_region", "領域", "Region" },
    { "geoc_col_ratio", "比率 r", "Ratio r" },
    { "geoc_col_dcells", "セル増 (計算)", "ΔCells (computed)" },
    { "geoc_rr_add", "+ 領域追加", "+ Add region" },
    { "geoc_rr_del", "− 削除", "− Delete" },
    { "geoc_rr_new_name", "領域%1", "region%1" },
    { "geoc_rr_nomesh", "— (格子未定義)", "— (no mesh)" },
    { "geoc_rr_note",
      "▸ 領域の定義は .ofdx に保存されます。細分化の実行は未実装のため格子は"
      "変わりません — 「セル増」は現在の基本格子から数えた見積り "
      "(領域内セル数 × (r³−1)) で、r > 1 で細かく・r < 1 で粗くなります。",
      "▸ The region definitions are saved to .ofdx. Refinement itself is not "
      "implemented, so the grid is unchanged — \"ΔCells\" is an estimate "
      "counted on the current base grid (cells inside × (r³−1)); r > 1 "
      "refines, r < 1 coarsens." },

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
    { "geoc_uw_tess", "テセレーション設定 (偏差・角度・品質・並列 / 曲率適応)",
      "the tessellation settings (deviation, angle, quality, parallel / curvature adaptation)" },
    { "geoc_uw_map", "材料マッピングの方式と既定材料の選択",
      "the material-mapping method and the default material" },
    { "geoc_uw_refine", "細分化の設定 (細分化エンジンが未実装のため)",
      "the refinement settings (the refinement engine is not implemented)" },
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

// 「値 + 単位 (+ 補足)」の行 (mock の <input> + <span className="muted">)。
// outHint に補足ラベルを返せる (ドメイン別に文言を差し替える行で使用)。
QWidget *valueRow(QWidget *parent, QWidget *field, const QString &unit,
                  const QString &hint = QString(), QLabel **outHint = nullptr)
{
    auto *w = new QWidget(parent);
    auto *h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(6);
    h->addWidget(field);
    if (!unit.isEmpty()) h->addWidget(makeHint(unit, w));
    if (!hint.isEmpty()) {
        auto *hl = makeHint(hint, w);
        h->addWidget(hl);
        if (outHint) *outHint = hl;
    }
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

// 変換後のメッシュの bbox / 表面積を頂点から取り直す (StlImporter が取込時に
// 求めるのと同じ定義: bbox = 全頂点の min/max、面積 = Σ|外積|/2)。
void recomputeMeshStats(ImportedMesh &m)
{
    const int n = qMin(m.numTriangles, int(m.vertices.size() / 9));
    if (n <= 0) return;
    double lo[3] = { m.vertices[0], m.vertices[1], m.vertices[2] };
    double hi[3] = { lo[0], lo[1], lo[2] };
    double area = 0.0;
    for (int t = 0; t < n; ++t) {
        const float *p = m.vertices.constData() + t * 9;
        for (int k = 0; k < 3; ++k)
            for (int a = 0; a < 3; ++a) {
                lo[a] = std::min(lo[a], double(p[k * 3 + a]));
                hi[a] = std::max(hi[a], double(p[k * 3 + a]));
            }
        const double ux = p[3] - p[0], uy = p[4] - p[1], uz = p[5] - p[2];
        const double vx = p[6] - p[0], vy = p[7] - p[1], vz = p[8] - p[2];
        const double cx = uy * vz - uz * vy;
        const double cy = uz * vx - ux * vz;
        const double cz = ux * vy - uy * vx;
        area += 0.5 * std::sqrt(cx * cx + cy * cy + cz * cz);
    }
    for (int a = 0; a < 3; ++a) {
        m.bbox[a]     = lo[a];
        m.bbox[3 + a] = hi[a];
    }
    m.surfaceArea = area;
}

// 頂点 p[3] を軸 axis まわりに回転 (原点基準・右手系 — rotateGeometry と同じ
// 巡回規約: 軸 a の回転面は (u,v) = ((a+1)%3, (a+2)%3))。
void rotateVertex(float *p, int axis, double cs, double sn)
{
    const int u = (axis + 1) % 3, v = (axis + 2) % 3;
    const double pu = p[u], pv = p[v];
    p[u] = float(pu * cs - pv * sn);
    p[v] = float(pu * sn + pv * cs);
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
    // ボクセル化〜ボクセル統計は Yee 格子 (FDTD) 前提のため、ドメインに
    // よっては隠す (updateDomainVisibility) — ポインタを保持しておく
    m_voxSection = buildVoxelSection();
    v->addWidget(m_voxSection);
    m_voxStatsSection = buildVoxelStatsSection();
    v->addWidget(m_voxStatsSection);
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

    // ドメイン切替 → FDTD 前提セクション/文言の出し分け (表示のみ)
    connect(project, &Project::domainChanged, this,
            [this] { updateDomainVisibility(); });
    updateDomainVisibility();

    refresh();
}

// ── ドメイン別の出し分け ────────────────────────────────────────────────────
// Yee 格子 (FDTD) を前提とするセクション/文言を、選択中ドメインに合わせて
// 隠す・切り替える。**表示のみ**で、モデルへの書き込み・.ofd/.ofdx の
// シリアライズは一切変えない (隠れていても従来どおり動作する)。
void GeometryTab::updateDomainVisibility()
{
    const Domain d = m_p->activeDomain();

    // ボクセル化〜ボクセル統計: 水中音響 (BELLHOP) はレイトレースで
    // Yee 格子を使わないため Underwater では非表示
    const bool useVoxel = (d != Domain::Underwater);
    if (m_voxSection)      m_voxSection->setVisible(useVoxel);
    if (m_voxStatsSection) m_voxStatsSection->setVisible(useVoxel);

    // 分解度Δ の補足 (λ/22 の評価点) はドメインの代表波長/周波数で切替
    if (m_voxDeltaHint) {
        const char *key = "geoc_vox_delta_hint";           // EM: 2.5 GHz
        switch (d) {
            case Domain::Optical:    key = "geoc_vox_delta_hint_opt"; break;
            case Domain::Acoustic:   key = "geoc_vox_delta_hint_ac";  break;
            case Domain::Underwater: key = "geoc_vox_delta_hint_uw";  break;
            default:                 break;
        }
        m_voxDeltaHint->setText(I18n::tr(QLatin1String(key)));
    }

    // 表面処理の解説: Acoustic では PEC 薄板ではなく剛壁 (rigid) の表現
    if (m_voxSurfHint)
        m_voxSurfHint->setText(I18n::tr(
            d == Domain::Acoustic ? "geoc_vox_surf_hint_ac"
                                  : "geoc_vox_surf_hint"));

    // 物性値割当のデフォルト材質: Acoustic/Underwater は εr 系 (PEC/誘電体/
    // FR-4) ではなく ρ/c 系の材料名にする (セクションは未配線 — ラベルのみ)
    if (m_mapDefault) {
        const char *k0 = "geoc_map_m2", *k1 = "geoc_map_m1",
                   *k2 = "geoc_map_m3";
        if (d == Domain::Acoustic) {
            k0 = "geoc_map_ac2"; k1 = "geoc_map_ac1"; k2 = "geoc_map_ac3";
        } else if (d == Domain::Underwater) {
            k0 = "geoc_map_uw2"; k1 = "geoc_map_ac1"; k2 = "geoc_map_uw3";
        }
        m_mapDefault->setItemText(0, I18n::tr(QLatin1String(k0)));
        m_mapDefault->setItemText(1, I18n::tr(QLatin1String(k1)));
        m_mapDefault->setItemText(2, I18n::tr(QLatin1String(k2)));
    }

    // メッシュ細分化の対象: 高εr → 高音速コントラスト (Acoustic/Underwater)
    if (m_refHighEps)
        m_refHighEps->setText(I18n::tr(
            (d == Domain::Acoustic || d == Domain::Underwater)
                ? "geoc_ref_highc" : "geoc_ref_higheps"));
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
// Viewport3D が実際に処理する 3 操作 (+ダブルクリック) だけを表にする。
// モックにあった選択/ギズモ/スナップ等は未実装なので掲載せず、注記で明示。
QWidget *GeometryTab::buildMouseSection()
{
    auto *s = new SectionBox(I18n::tr("geoc_mouse_section"));
    static const char *kRows[3] = { "geoc_ms10_", "geoc_ms11_", "geoc_ms13_" };
    auto *t = makeTable({ I18n::tr("geoc_col_op"), I18n::tr("geoc_col_key"),
                          I18n::tr("geoc_col_effect") }, 3, s);
    for (int r = 0; r < 3; ++r) {
        const QString base = QLatin1String(kRows[r]);
        t->setItem(r, 0, textItem(I18n::tr(base + "o")));
        t->setItem(r, 1, textItem(I18n::tr(base + "k")));
        t->setItem(r, 2, textItem(I18n::tr(base + "a")));
    }
    s->vbox()->addWidget(t);
    s->vbox()->addWidget(makeHint(I18n::tr("geoc_mouse_todo"), s));
    return s;
}

// ── 3Dモデル取込: ファイル行 + 対応形式バッジ + 解説 ────────────────────────
void GeometryTab::addCadImportRows(SectionBox *s)
{
    auto *fileRow = new QWidget(s);
    auto *fh = new QHBoxLayout(fileRow);
    fh->setContentsMargins(0, 0, 0, 0);
    fh->setSpacing(6);
    // 既定値のダミーファイル名は誤解を招くので空 + placeholder にする
    m_cadFile = new QLineEdit(fileRow);
    m_cadFile->setPlaceholderText(I18n::tr("geoc_file_ph"));
    fh->addWidget(m_cadFile, 1);
    auto *browse = new QPushButton(I18n::tr("geoc_browse"), fileRow);
    fh->addWidget(browse);
    s->form()->addRow(I18n::tr("geoc_file"), fileRow);

    // 形式分類: CAD (B-rep)
    auto *cadRow = new QWidget(s);
    auto *ch = new QHBoxLayout(cadRow);
    ch->setContentsMargins(0, 0, 0, 0);
    ch->setSpacing(6);
    // 対応済み (アクセント色) は STL だけ — B-rep CAD 系は全て未対応 (muted)
    ch->addWidget(makeHint(I18n::tr("geoc_fmt_cad"), cadRow));
    ch->addWidget(makeBadge("STEP (.stp/.step)", kMuted, cadRow));
    ch->addWidget(makeBadge("IGES (.igs)", kMuted, cadRow));
    ch->addWidget(makeBadge("BREP", kMuted, cadRow));
    ch->addWidget(makeBadge("Parasolid (.x_t)", kMuted, cadRow));
    ch->addWidget(makeBadge("SAT (ACIS)", kMuted, cadRow));
    ch->addStretch(1);
    s->form()->addRow(I18n::tr("geoc_fmt_class"), cadRow);

    // メッシュ / 2D-EDA 形式
    auto *meshRow = new QHBoxLayout();
    meshRow->setSpacing(6);
    meshRow->addWidget(makeHint(I18n::tr("geoc_fmt_mesh"), s));
    // 対応済み (アクセント色) は自前パーサのある STL / OBJ / PLY。
    // 3MF は ZIP (deflate) の展開が要るので未対応のまま (依存を増やさない)
    for (const char *f : { "STL", "OBJ", "PLY" })
        meshRow->addWidget(makeBadge(QString::fromLatin1(f), kAcc, s));
    meshRow->addWidget(makeBadge("3MF", kMuted, s));
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
            "*.obj *.ply *.3mf);;"
            + MeshImporter::fileDialogFilter());
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
    s->vbox()->addWidget(tabhelp::unwiredNote(s, I18n::tr("geoc_uw_tess")));   // テセレーション自体が未実装
    return s;
}

// ── アセンブリツリー / Assembly tree ───────────────────────────────────────
// STEP の部品階層取込は外部 CAD カーネルを要するため未実装 (依存は増やさない)。
// 代わりに **取込済み STL の実測値** を単一部品として出す。未取込のときは
// 空表示 + 「どこから取り込むか」の導線だけを出す (偽のツリーは出さない)。
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
    s->vbox()->addWidget(m_asmTree);

    // 未取込のあいだ表示する導線 (取込後は refreshAssemblyTree() が隠す)
    m_asmNone = makeHint(I18n::tr("geoc_asm_none"), s);
    s->vbox()->addWidget(m_asmNone);
    s->vbox()->addWidget(makeHint(I18n::tr("geoc_asm_note"), s));
    refreshAssemblyTree();

    auto *br = new QHBoxLayout();
    auto *assignBtn = new QPushButton(I18n::tr("geoc_asm_assign"), s);
    auto *ignoreBtn = new QPushButton(I18n::tr("geoc_asm_autoignore"), s);
    tabhelp::markNotImplemented(assignBtn);
    tabhelp::markNotImplemented(ignoreBtn);
    br->addWidget(assignBtn);
    br->addWidget(ignoreBtn);
    br->addStretch(1);
    s->vbox()->addLayout(br);
    return s;
}

// ── 配置・変換 / Placement ─────────────────────────────────────────────────
// 取込 STL へのアフィン変換 (applyPlacement) として実配線済み。既定値は
// 恒等変換 (単位 m・×1・回転/オフセット 0・中心合わせ OFF) にして、設定を
// 触らない限り従来の取込動作 (STL 座標を m とみなす) と完全一致させる。
QWidget *GeometryTab::buildPlacementSection()
{
    auto *s = new SectionBox(I18n::tr("geoc_place_section"));
    s->vbox()->addWidget(makeHint(I18n::tr("geoc_place_hint"), s));

    auto *unitRowW = new QWidget(s);
    auto *uh = new QHBoxLayout(unitRowW);
    uh->setContentsMargins(0, 0, 0, 0);
    uh->setSpacing(6);
    // 既定は m (index 1) — 従来どおり STL 座標をそのまま m として扱う
    uh->addWidget(segRow(unitRowW, &m_placeUnit,
                         { "mm", "m", "μm", "nm", "inch" }, 1));
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
    // 既定 OFF — ON にすると取込時に bbox 中心を原点へ移動する (実配線)
    m_placeCenter = makeCheck(I18n::tr("geoc_place_center"), false, s);
    m_placeAutoAxis = makeCheck(I18n::tr("geoc_place_autoaxis"), false, s);
    m_placeAutoAxis->setToolTip(I18n::tr("geoc_autoaxis_tip"));
    cr->addWidget(m_placeCenter);
    cr->addWidget(m_placeAutoAxis);
    cr->addStretch(1);
    s->vbox()->addLayout(cr);
    m_autoAxisNote = makeHint(QString(), s);
    s->vbox()->addWidget(m_autoAxisNote);
    connect(m_placeAutoAxis, &QCheckBox::toggled, this,
            &GeometryTab::applyAutoAxis);

    // 設定変更 → 取込済みメッシュへ即時反映 (取込前は何もしない)
    connect(m_placeUnit, &QButtonGroup::idClicked, this,
            [this](int) { reapplyPlacement(); });
    connect(m_placeScale, &QDoubleSpinBox::valueChanged, this,
            [this](double) { reapplyPlacement(); });
    for (int i = 0; i < 3; ++i) {
        connect(m_placeOffset[i], &QDoubleSpinBox::valueChanged, this,
                [this](double) { reapplyPlacement(); });
        connect(m_placeRot[i], &QDoubleSpinBox::valueChanged, this,
                [this](double) { reapplyPlacement(); });
    }
    connect(m_placeCenter, &QCheckBox::toggled, this,
            [this](bool) { reapplyPlacement(); });
    return s;
}

// ── ジオメトリ検査 / Mesh check ────────────────────────────────────────────
// 検出は io/MeshDiagnostics による実計算 (取込メッシュの位相・幾何)。
// 修復 (縫合・法線統一・デシメーション) は未実装なので検出のみと明記する。
QWidget *GeometryTab::buildHealingSection()
{
    auto *s = new SectionBox(I18n::tr("geoc_heal_section"));
    s->vbox()->addWidget(makeHint(I18n::tr("geoc_heal_hint"), s));

    m_healTable = makeTable({ I18n::tr("geoc_col_process"),
                              I18n::tr("geoc_col_detect"),
                              I18n::tr("geoc_col_state") }, 6, s);
    for (int r = 0; r < 6; ++r) {
        m_healTable->setItem(r, 0, textItem(
            I18n::tr(QStringLiteral("geoc_hd%1_p").arg(r + 1))));
        m_healTable->setItem(r, 1, numItem(I18n::tr("geoc_heal_pending")));
        m_healTable->setItem(r, 2, badgeItem(I18n::tr("geoc_heal_pending"),
                                             kMuted));
    }
    s->vbox()->addWidget(m_healTable);
    // 未取込 / 検査省略の説明 (取込後は検出条件の補足に差し替わる)
    m_healNone = makeHint(I18n::tr("geoc_heal_none"), s);
    s->vbox()->addWidget(m_healNone);
    s->vbox()->addWidget(makeHint(I18n::tr("geoc_heal_note"), s));

    auto *br = new QHBoxLayout();
    m_healBtn = new QPushButton(I18n::tr("geoc_heal_run"), s);
    m_healBtn->setToolTip(I18n::tr("geoc_heal_tip"));
    m_healBtn->setEnabled(false);           // 取込・検査が済むまで押せない
    connect(m_healBtn, &QPushButton::clicked, this, &GeometryTab::runHealing);
    br->addWidget(m_healBtn);
    br->addStretch(1);
    br->addWidget(makeHint(I18n::tr("geoc_heal_next"), s));
    s->vbox()->addLayout(br);
    m_healResult = makeHint(QString(), s);  // 修復の結果 (実行後のみ)
    s->vbox()->addWidget(m_healResult);
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
    s->vbox()->addWidget(tabhelp::unwiredNote(s, I18n::tr("geoc_uw_map")));   // 取込材質は m_voxMat のみ有効
    return s;
}

// ── 取込プレビュー / Preview ───────────────────────────────────────────────
QWidget *GeometryTab::buildPreviewSection()
{
    auto *s = new SectionBox(I18n::tr("geoc_prev_section"));

    // 取込前は実測値が無いので「—」を出す (固定サンプル値は出さない)。
    // 実取込後に refreshImportBadges() が実測値へ上書きし、下の注記を隠す。
    auto *br = new QHBoxLayout();
    const QString dash = I18n::tr("geoc_prev_dash");
    m_prevTri   = makeBadge(dash, kMuted, s);
    m_prevSolid = makeBadge(dash, kMuted, s);
    m_prevVol   = makeBadge(dash, kMuted, s);
    m_prevBbox  = makeBadge(dash, kMuted, s);
    br->addWidget(m_prevTri);
    br->addWidget(m_prevSolid);
    br->addWidget(m_prevVol);
    br->addWidget(m_prevBbox);
    br->addStretch(1);
    s->vbox()->addLayout(br);

    m_prevNone = makeHint(I18n::tr("geoc_prev_none"), s);
    s->vbox()->addWidget(m_prevNone);

    auto *hr = new QHBoxLayout();
    auto *runImport = new QPushButton(I18n::tr("geoc_prev_import"), s);
    hr->addWidget(runImport);
    auto *prev3dBtn = new QPushButton(I18n::tr("geoc_prev_3d"), s);
    auto *measureBtn = new QPushButton(I18n::tr("geoc_prev_measure"), s);
    tabhelp::markNotImplemented(prev3dBtn);
    hr->addWidget(prev3dBtn);
    hr->addWidget(measureBtn);
    hr->addStretch(1);
    s->vbox()->addLayout(hr);

    // 取込実行 = 実際の STL 取込 (io/MeshImporter)
    connect(runImport, &QPushButton::clicked, this, &GeometryTab::importStl);
    // 寸法測定 = 取込メッシュの実測値ダイアログ
    connect(measureBtn, &QPushButton::clicked, this,
            &GeometryTab::showMeasureDialog);
    return s;
}

// ── 取込済みモデル / Imported models ───────────────────────────────────────
// モック由来の「(例)」行は廃止。実際に取り込んだ STL の行だけを出す
// (refreshImportBadges() が挿入・更新する)。取込前は空表示 + 導線。
QWidget *GeometryTab::buildImportedSection()
{
    auto *s = new SectionBox(I18n::tr("geoc_models_section"));

    m_modelTable = makeTable({ QString(), I18n::tr("geoc_col_name"),
                               I18n::tr("geoc_col_format"),
                               I18n::tr("geoc_col_tri"),
                               I18n::tr("geoc_col_vol"),
                               I18n::tr("geoc_col_matcol") },
                             0, s, 80);
    s->vbox()->addWidget(m_modelTable);
    m_modelNone = makeHint(I18n::tr("geoc_models_none"), s);
    s->vbox()->addWidget(m_modelNone);
    s->vbox()->addWidget(makeHint(I18n::tr("geoc_models_hint"), s));
    return s;
}

// ── ボクセル化 / Voxelization (実行は io/Voxelizer の staircase 版) ─────────
QWidget *GeometryTab::buildVoxelSection()
{
    auto *s = new SectionBox(I18n::tr("geoc_vox_section"));
    s->vbox()->addWidget(makeHint(I18n::tr("geoc_vox_hint"), s));

    m_voxDelta = makeSpin(s, 0.001, 1000.0, 3, 0.5, 0.1);
    // 補足ラベルはドメイン別に文言を差し替える (updateDomainVisibility)
    s->form()->addRow(I18n::tr("geoc_vox_delta"),
                      valueRow(s, m_voxDelta, "mm",
                               I18n::tr("geoc_vox_delta_hint"),
                               &m_voxDeltaHint));
    // 既定はレイの偶奇 (閉じたメッシュでは厳密で速い)。巻き数は穴に強いが
    // 法線の向きが揃っていることが前提。SDF は未実装。
    s->form()->addRow(I18n::tr("geoc_vox_inout"),
                      segRow(s, &m_voxInside,
                             { I18n::tr("geoc_vox_ray"),
                               I18n::tr("geoc_vox_winding"),
                               I18n::tr("geoc_vox_sdf") }, 0));
    if (m_voxInside)
        for (QAbstractButton *b : m_voxInside->buttons())
            if (m_voxInside->id(b) == 2) tabhelp::markNotImplemented(b);
    s->form()->addRow(makeHint(I18n::tr("geoc_vox_inout_hint"), s));
    // 実装済みの表面処理は階段近似のみ (io/Voxelizer) なので既定もそれに合わせる
    s->form()->addRow(I18n::tr("geoc_vox_surface"),
                      segRow(s, &m_voxSurface,
                             { I18n::tr("geoc_vox_stair"),
                               I18n::tr("geoc_vox_conformal"),
                               I18n::tr("geoc_vox_subcell") }, 0));
    // 行順を守るため、フォーム内に全幅行として差し込む
    // (Acoustic では剛壁 (rigid) の文言に切り替える — updateDomainVisibility)
    m_voxSurfHint = makeHint(I18n::tr("geoc_vox_surf_hint"), s);
    s->form()->addRow(m_voxSurfHint);

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

    // 内外判定と「まとめる」は Voxelizer が読む。表面処理 (共形/サブセル)・
    // PVF・八分木・GPU はエンジン側が未実装なので、それだけを明示する
    for (QAbstractButton *b : m_voxSurface->buttons())
        if (m_voxSurface->id(b) != 0) tabhelp::markNotImplemented(b);
    tabhelp::markNotImplemented(m_voxOctree);
    tabhelp::markNotImplemented(m_voxGpu);
    s->vbox()->addWidget(makeHint(I18n::tr("geoc_vox_engine_note"), s));

    // 実行行: ボクセル化 (実処理) + 材質番号 + 占有セルバッジ
    auto *runRow = new QHBoxLayout();
    m_voxBtn = new QPushButton(I18n::tr("ge_voxelize_btn"), s);
    m_voxBtn->setEnabled(false);
    runRow->addWidget(m_voxBtn);
    auto *voxPrevBtn = new QPushButton(I18n::tr("geoc_vox_preview"), s);
    tabhelp::markNotImplemented(voxPrevBtn);
    runRow->addWidget(voxPrevBtn);
    runRow->addWidget(new QLabel(I18n::tr("ge_voxel_mat"), s));
    m_voxMat = new QSpinBox(s);
    m_voxMat->setRange(1, 9999);
    m_voxMat->setValue(2);
    runRow->addWidget(m_voxMat);
    // 実行前は固定サンプル値ではなく「未実行」を出す (実行後に実測値で上書き)
    m_voxBadge = makeBadge(I18n::tr("geoc_stat_notrun"), kMuted, s);
    runRow->addWidget(m_voxBadge);
    runRow->addStretch(1);
    s->vbox()->addLayout(runRow);
    return s;
}

// ── ボクセル統計 / Voxel statistics ────────────────────────────────────────
// 実行前はモックの固定値ではなく「— (未実行)」を出す。実行後は
// refreshVoxelStats() が実測値 (staircase で算出できる範囲) に上書きする。
QWidget *GeometryTab::buildVoxelStatsSection()
{
    auto *s = new SectionBox(I18n::tr("geoc_stat_section"));

    m_statOcc = makeMono(I18n::tr("geoc_stat_notrun"), s);
    s->form()->addRow(I18n::tr("geoc_stat_occ"), m_statOcc);
    m_statBnd = makeMono(I18n::tr("geoc_stat_notrun"), s);
    s->form()->addRow(I18n::tr("geoc_stat_bnd"), m_statBnd);
    m_statErr = makeMono(I18n::tr("geoc_stat_notrun"), s);
    s->form()->addRow(I18n::tr("geoc_stat_err"), m_statErr);
    m_statConf = makeMono(I18n::tr("geoc_stat_notrun"), s);
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

    // 細分化はエンジン未実装 — 設定はどこにも反映されない
    s->vbox()->addWidget(tabhelp::unwiredNote(s, I18n::tr("geoc_uw_refine")));

    auto *rr = new QHBoxLayout();
    auto *refineBtn = new QPushButton(I18n::tr("geoc_ref_run"), s);
    tabhelp::markNotImplemented(refineBtn);   // 細分化エンジンは未実装
    rr->addWidget(refineBtn);
    m_refBadge = makeBadge(QString(), kMuted, s);
    rr->addWidget(m_refBadge);
    rr->addStretch(1);
    s->vbox()->addLayout(rr);
    return s;
}

// ── 細分化領域 / Refined regions ───────────────────────────────────────────
// Project::refineRegions() (.ofdx へ永続化) を編集する表。領域の定義自体は
// 利用者の入力データなので実データとして持ち、「セル増」は現在の基本格子
// (xmesh/ymesh/zmesh) から数えた見積りを表示する。細分化の実行そのものは
// 未実装で、格子・.ofd の出力は一切変わらない (注記で明示)。
QWidget *GeometryTab::buildRefinedRegionsSection()
{
    auto *s = new SectionBox(I18n::tr("geoc_regions_section"));

    QStringList headers { QString(), I18n::tr("geoc_col_region") };
    static const char *kAxis[3] = { "X", "Y", "Z" };
    for (const char *ax : kAxis) {
        headers << QStringLiteral("%1min [mm]").arg(QLatin1String(ax));
        headers << QStringLiteral("%1max [mm]").arg(QLatin1String(ax));
    }
    headers << I18n::tr("geoc_col_ratio") << I18n::tr("geoc_col_dcells");

    m_refTable = makeTable(headers, 0, s, 120);
    // 一覧は編集可能 (セル増の列だけ読取専用にする — refreshRegionTable)
    m_refTable->setEditTriggers(QAbstractItemView::DoubleClicked
                                | QAbstractItemView::EditKeyPressed
                                | QAbstractItemView::AnyKeyPressed);
    s->vbox()->addWidget(m_refTable);

    auto *br = new QHBoxLayout();
    auto *addBtn = new QPushButton(I18n::tr("geoc_rr_add"), s);
    auto *delBtn = new QPushButton(I18n::tr("geoc_rr_del"), s);
    br->addWidget(addBtn);
    br->addWidget(delBtn);
    br->addStretch(1);
    s->vbox()->addLayout(br);
    s->vbox()->addWidget(makeHint(I18n::tr("geoc_rr_note"), s));

    // 追加: 現在の格子いっぱいの領域を既定にする (ユニット追加と同じ流儀)。
    // 分割比の初期値は上の「細分化比率」スピンの現在値。
    connect(addBtn, &QPushButton::clicked, this, [this] {
        auto &regs = m_p->refineRegions();
        RefineRegion r;
        r.name = I18n::tr("geoc_rr_new_name").arg(regs.size() + 1);
        for (int a = 0; a < 3; ++a) {
            r.min_m[a] = m_p->mesh(a).min();
            r.max_m[a] = m_p->mesh(a).max();
        }
        if (m_refRatio) r.ratio = m_refRatio->value();
        regs.push_back(r);
        refreshRegionTable();
        updateRefineBadge();
        m_p->touch();
    });
    connect(delBtn, &QPushButton::clicked, this, [this] {
        const int r = m_refTable->currentRow();
        auto &regs = m_p->refineRegions();
        if (r < 0 || r >= regs.size()) return;
        regs.removeAt(r);
        refreshRegionTable();
        updateRefineBadge();
        m_p->touch();
    });
    connect(m_refTable, &QTableWidget::cellChanged, this, [this](int, int) {
        if (m_updating) return;
        applyRegionTable();
        refreshRegionTable();   // 正規化した値とセル増を表示に反映
        updateRefineBadge();
        m_p->touch();
    });
    return s;
}

// タブ表示時: 別タブで格子 (xmesh/ymesh/zmesh) が変わっていることがあるので
// セル増の見積りを取り直す (古い数字を残さない)。
void GeometryTab::showEvent(QShowEvent *e)
{
    QScrollArea::showEvent(e);
    refreshRegionTable();
    updateRefineBadge();
}

// 領域内に中心を持つ基本セルの数 (現在の xmesh/ymesh/zmesh から数える)。
// 格子が不正 (未定義) なら -1 を返す。
qint64 GeometryTab::cellsInRegion(const RefineRegion &r) const
{
    qint64 n = 1;
    for (int a = 0; a < 3; ++a) {
        const MeshAxis &m = m_p->mesh(a);
        if (!m.isValid()) return -1;
        const double lo = qMin(r.min_m[a], r.max_m[a]);
        const double hi = qMax(r.min_m[a], r.max_m[a]);
        qint64 cnt = 0;
        for (int i = 0; i < m.divs.size(); ++i) {
            const double d = (m.nodes[i + 1] - m.nodes[i]) / m.divs[i];
            for (int k = 0; k < m.divs[i]; ++k) {
                const double c = m.nodes[i] + d * (k + 0.5);   // セル中心
                if (c >= lo && c <= hi) ++cnt;
            }
        }
        n *= cnt;
        if (n == 0) return 0;
    }
    return n;
}

// 細分化領域の一覧 (model → widgets)。セル増の列は基本格子から数えた値。
void GeometryTab::refreshRegionTable()
{
    if (!m_refTable) return;
    const bool prev = m_updating;
    m_updating = true;
    const auto &regs = m_p->refineRegions();
    m_refTable->setRowCount(regs.size());
    for (int i = 0; i < regs.size(); ++i) {
        const RefineRegion &r = regs[i];
        m_refTable->setItem(i, 0, checkItem(r.enabled));
        m_refTable->setItem(i, 1, textItem(r.name));
        for (int a = 0; a < 3; ++a) {
            m_refTable->setItem(i, 2 + a * 2,
                                numItem(QString::number(r.min_m[a] * 1e3, 'g', 8)));
            m_refTable->setItem(i, 3 + a * 2,
                                numItem(QString::number(r.max_m[a] * 1e3, 'g', 8)));
        }
        m_refTable->setItem(i, 8, numItem(QString::number(r.ratio, 'g', 4)));

        const qint64 base = cellsInRegion(r);
        QString delta;
        if (base < 0) {
            delta = I18n::tr("geoc_rr_nomesh");
        } else {
            const double d = double(base) * (r.ratio * r.ratio * r.ratio - 1.0);
            const qint64 n = qint64(qRound(d));
            delta = (n > 0 ? QStringLiteral("+") : QString()) + groupNum(n);
        }
        auto *it = numItem(delta);
        it->setFlags(it->flags() & ~Qt::ItemIsEditable);   // 計算値 (読取専用)
        m_refTable->setItem(i, 9, it);
    }
    m_updating = prev;
}

// 細分化領域の一覧 (widgets → model)。不正値は前の値を保つ。
void GeometryTab::applyRegionTable()
{
    if (!m_refTable) return;
    auto &regs = m_p->refineRegions();
    for (int i = 0; i < m_refTable->rowCount() && i < regs.size(); ++i) {
        RefineRegion &r = regs[i];
        auto cell = [this, i](int c) {
            auto *it = m_refTable->item(i, c);
            return it ? it->text() : QString();
        };
        if (auto *chk = m_refTable->item(i, 0))
            r.enabled = (chk->checkState() == Qt::Checked);
        r.name = cell(1);
        for (int a = 0; a < 3; ++a) {
            bool ok = false;
            const double lo = cell(2 + a * 2).toDouble(&ok);
            if (ok) r.min_m[a] = lo * 1e-3;             // mm → m
            const double hi = cell(3 + a * 2).toDouble(&ok);
            if (ok) r.max_m[a] = hi * 1e-3;
        }
        bool ok = false;
        const double ratio = cell(8).toDouble(&ok);
        if (ok && ratio > 0.0) r.ratio = ratio;         // r ≤ 0 は無意味
    }
}

// 「メッシュ細分化」節のセル増加見積り。有効な領域の (領域内セル数 ×
// (r³−1)) の総和を、基本格子の総セル数と比べて示す。細分化の実行は未実装
// なので、あくまで領域定義から計算した机上値であることを文言で明示する。
void GeometryTab::updateRefineBadge()
{
    if (!m_refBadge) return;
    const auto &regs = m_p->refineRegions();
    bool any = false;
    double delta = 0.0;
    for (const RefineRegion &r : regs) {
        if (!r.enabled) continue;
        const qint64 base = cellsInRegion(r);
        if (base < 0) {   // 格子が未定義 → 計算できない
            m_refBadge->setText(I18n::tr("geoc_ref_growth_nomesh"));
            return;
        }
        any = true;
        delta += double(base) * (r.ratio * r.ratio * r.ratio - 1.0);
    }
    if (!any) {
        m_refBadge->setText(I18n::tr("geoc_ref_growth_none"));
        return;
    }
    const qint64 total = m_p->totalCells();
    const double pct = total > 0 ? 100.0 * delta / double(total) : 0.0;
    const qint64 d = qint64(qRound(delta));
    m_refBadge->setText(I18n::tr("geoc_ref_growth_fmt")
                            .arg((d > 0 ? QStringLiteral("+") : QString())
                                     + groupNum(d),
                                 groupNum(total),
                                 QString::number(pct, 'f', 1)));
}

void GeometryTab::importStl()
{
    const QString path = QFileDialog::getOpenFileName(
        this, I18n::tr("ge_import_btn"), {}, MeshImporter::fileDialogFilter());
    if (path.isEmpty()) return;

    ImportedMesh mesh;
    QString err;
    if (!MeshImporter::load(path, mesh, &err)) {
        m_importInfo->setText("error: " + err);
        return;
    }

    // 変換前の生メッシュを保持し、配置・変換 (placement) を適用したものを
    // プレビュー・計測・ボクセル化に使う (恒等変換なら取込そのまま)。
    m_rawMesh = mesh;
    m_lastMesh = applyPlacement(mesh);
    m_hasMesh = true;
    // 位相検査は取込時に 1 度だけ (アフィン変換で位相は変わらないので
    // 配置・変換のたびに数え直さない)。
    m_diag = analyzeMesh(m_rawMesh);
    m_voxBtn->setEnabled(true);
    if (m_cadFile) m_cadFile->setText(path);

    const ImportedMesh &shown = m_lastMesh;   // 配置・変換適用後の実測値
    m_importInfo->setText(QStringLiteral(
        "%1 — %2 triangles, area %3 m², bbox [%4, %5]×[%6, %7]×[%8, %9]\n%10")
        .arg(shown.name).arg(shown.numTriangles)
        .arg(QString::number(shown.surfaceArea, 'g', 4))
        .arg(QString::number(shown.bbox[0], 'g', 4), QString::number(shown.bbox[3], 'g', 4),
             QString::number(shown.bbox[1], 'g', 4), QString::number(shown.bbox[4], 'g', 4),
             QString::number(shown.bbox[2], 'g', 4), QString::number(shown.bbox[5], 'g', 4))
        .arg(I18n::tr("ge_voxelize_hint")));

    refreshImportBadges();
    refreshAssemblyTree();
    refreshHealing();
    applyAutoAxis();      // 主軸検出が ON なら新しいメッシュで求め直す
}

// ── 配置・変換 (placement) の適用 ──────────────────────────────────────────
// 取込 STL の頂点へ スケール (単位換算 ×係数) → 中心合わせ (bbox 中心を
// 原点へ) → 回転 (X→Y→Z, 原点基準) → オフセット [m] の順にアフィン変換を
// かけ、bbox / 表面積を頂点から取り直す。恒等変換ならメッシュをそのまま返す
// (既定値では従来の取込動作と完全一致)。純幾何処理 — StlImporter / Voxelizer
// は変更しない。
ImportedMesh GeometryTab::applyPlacement(const ImportedMesh &src) const
{
    // 単位ボタン (mm / m / μm / nm / inch) → m 換算係数
    static const double kUnit[5] = { 1e-3, 1.0, 1e-6, 1e-9, 0.0254 };
    int ui = m_placeUnit ? m_placeUnit->checkedId() : 1;
    if (ui < 0 || ui >= 5) ui = 1;
    const double s = kUnit[ui] * (m_placeScale ? m_placeScale->value() : 1.0);

    double off[3], rot[3];
    bool hasOff = false, hasRot = false;
    for (int a = 0; a < 3; ++a) {
        off[a] = m_placeOffset[a] ? m_placeOffset[a]->value() : 0.0;
        rot[a] = m_placeRot[a]    ? m_placeRot[a]->value()    : 0.0;
        hasOff = hasOff || off[a] != 0.0;
        hasRot = hasRot || rot[a] != 0.0;
    }
    const bool center = m_placeCenter && m_placeCenter->isChecked();
    if (s == 1.0 && !center && !hasRot && !hasOff)
        return src;   // 恒等変換 — 従来どおり無変換

    ImportedMesh m = src;
    const int nv = m.vertices.size() / 3;   // 頂点数 (3 float / 頂点)
    float *vp = m.vertices.data();

    // 1) スケール (単位換算 × 任意係数)
    if (s != 1.0)
        for (int i = 0; i < nv * 3; ++i)
            vp[i] = float(double(vp[i]) * s);

    // 2) 中心合わせ: スケール後の bbox 中心を原点へ
    if (center) {
        recomputeMeshStats(m);
        const double c[3] = { (m.bbox[0] + m.bbox[3]) / 2.0,
                              (m.bbox[1] + m.bbox[4]) / 2.0,
                              (m.bbox[2] + m.bbox[5]) / 2.0 };
        for (int i = 0; i < nv; ++i)
            for (int a = 0; a < 3; ++a)
                vp[i * 3 + a] = float(double(vp[i * 3 + a]) - c[a]);
    }

    // 3) 回転 X → Y → Z (原点基準 — 中心合わせ ON なら bbox 中心基準になる)
    if (hasRot)
        for (int axis = 0; axis < 3; ++axis) {
            if (rot[axis] == 0.0) continue;
            const double th = rot[axis] * M_PI / 180.0;
            const double cs = std::cos(th), sn = std::sin(th);
            for (int i = 0; i < nv; ++i)
                rotateVertex(vp + i * 3, axis, cs, sn);
        }

    // 4) オフセット [m]
    if (hasOff)
        for (int i = 0; i < nv; ++i)
            for (int a = 0; a < 3; ++a)
                vp[i * 3 + a] = float(double(vp[i * 3 + a]) + off[a]);

    recomputeMeshStats(m);
    return m;
}

// 配置・変換の設定変更 → 取込済みメッシュへ変換をかけ直して表示を更新。
// ボクセル化は m_lastMesh を読むので、次回実行から自動的に反映される。
void GeometryTab::reapplyPlacement()
{
    if (!m_hasMesh) return;
    m_lastMesh = applyPlacement(m_rawMesh);
    refreshImportBadges();
    refreshAssemblyTree();   // bbox / 面積 / 体積は変換で変わる (位相は不変)
}

// ── 寸法測定 / Measure ─────────────────────────────────────────────────────
// 取込メッシュ (配置・変換適用後) の bbox 寸法・表面積 (StlImporter と同じ
// 定義で再計算) と meshVolume() の体積 (発散定理 — 閉メッシュ前提) をまとめて
// 表示する。2点間クリック計測は 3D ピッキング未実装のため対象外 (明記)。
void GeometryTab::showMeasureDialog()
{
    if (!m_hasMesh) {
        QMessageBox::information(this, I18n::tr("geoc_meas_title"),
                                 I18n::tr("geoc_meas_none"));
        return;
    }
    const ImportedMesh &m = m_lastMesh;
    const double sx = (m.bbox[3] - m.bbox[0]) * 1e3;   // m → mm
    const double sy = (m.bbox[4] - m.bbox[1]) * 1e3;
    const double sz = (m.bbox[5] - m.bbox[2]) * 1e3;
    const double volCm3 = meshVolume(m) * 1e6;         // m³ → cm³

    auto line = [](const QString &label, const QString &value) {
        return label + QStringLiteral(": ") + value;
    };
    const QString range =
        QString::fromUtf8("X [%1, %2] · Y [%3, %4] · Z [%5, %6]")
            .arg(QString::number(m.bbox[0], 'g', 4), QString::number(m.bbox[3], 'g', 4),
                 QString::number(m.bbox[1], 'g', 4), QString::number(m.bbox[4], 'g', 4),
                 QString::number(m.bbox[2], 'g', 4), QString::number(m.bbox[5], 'g', 4));
    const QString body =
        line(I18n::tr("geoc_meas_model"), m.name) + QLatin1Char('\n')
      + line(I18n::tr("geoc_meas_tri"), groupNum(m.numTriangles)) + QLatin1Char('\n')
      + line(I18n::tr("geoc_meas_bbox"),
             QString::fromUtf8("%1 × %2 × %3 mm")
                 .arg(QString::number(sx, 'f', 1), QString::number(sy, 'f', 1),
                      QString::number(sz, 'f', 1))) + QLatin1Char('\n')
      + line(I18n::tr("geoc_meas_range"), range) + QLatin1Char('\n')
      + line(I18n::tr("geoc_meas_area"),
             QString::number(m.surfaceArea, 'g', 4)
                 + QString::fromUtf8(" m²")) + QLatin1Char('\n')
      + line(I18n::tr("geoc_meas_vol"),
             QString::number(volCm3, 'f', 2) + QString::fromUtf8(" cm³"))
      + QStringLiteral("\n\n")
      + I18n::tr("geoc_meas_placed") + QLatin1Char('\n')
      + I18n::tr("geoc_meas_pick");
    QMessageBox::information(this, I18n::tr("geoc_meas_title"), body);
}

void GeometryTab::voxelizeImported()
{
    if (!m_hasMesh) return;

    VoxelOptions opt;
    // 内外判定: 0 = レイの偶奇 / 1 = 一般化巻き数 (2 = SDF は未実装)
    const int inTest = m_voxInside ? m_voxInside->checkedId() : 0;
    opt.inside = (inTest == 1) ? InsideTest::WindingNumber
                               : InsideTest::RayParity;
    opt.mergeRuns = !m_voxMerge || m_voxMerge->isChecked();
    // 部分体積率: 境界セルだけを 4³ = 64 点で再標本化し、体積率 50% 以上を
    // 占有とする (切ると従来どおりセル中心 1 点の判定)
    opt.pvf = m_voxPvf && m_voxPvf->isChecked();
    opt.pvfSamples = 4;

    // 巻き数は法線の向きが揃っていることが前提。揃っていないと + と − が
    // 打ち消し合って全セルが「外」になるので、黙って走らせず修復へ誘導する。
    if (opt.inside == InsideTest::WindingNumber && m_diag.valid
        && m_diag.inconsistentEdges > 0) {
        QMessageBox::warning(this, I18n::tr("ge_voxelize_btn"),
                             I18n::tr("geoc_vox_wind_needs_normals")
                                 .arg(m_diag.inconsistentEdges));
        return;
    }

    const VoxelResult res = Voxelizer::voxelize(
        m_lastMesh, m_p->mesh(0), m_p->mesh(1), m_p->mesh(2),
        m_voxMat->value(), 8'000'000, opt);
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
    m_voxHasPvf   = opt.pvf;
    m_voxBoundary = res.boundaryCells;
    m_voxPvfN     = opt.pvfSamples;
    m_voxStairVol = res.stairVolume;
    m_voxPvfVol   = res.pvfVolume;
    m_voxMeshVol  = meshVolume(m_lastMesh);
    refreshVoxelStats();
}

// 取込プレビュー / 取込済みモデル を実メッシュの診断値で置き換える。
void GeometryTab::refreshImportBadges()
{
    if (!m_hasMesh || !m_prevTri) return;

    // 実測値で置き換わるので「未取込」の導線を隠す
    if (m_prevNone)  m_prevNone->hide();
    if (m_modelNone) m_modelNone->hide();

    // 実測値になったのでバッジの色も muted → ok にする
    for (QLabel *b : { m_prevTri, m_prevSolid })
        b->setStyleSheet(QStringLiteral("color:%1; font-weight:600;")
                             .arg(QLatin1String(kOk)));

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
    // 形式は取込元の拡張子から出す (STL 決め打ちにしない — OBJ / PLY も読む)
    const QString fmt = QFileInfo(m_lastMesh.sourcePath).suffix().toUpper();
    m_modelTable->setItem(m_liveModelRow, 2,
                          textItem(fmt.isEmpty() ? QStringLiteral("STL") : fmt));
    m_modelTable->setItem(m_liveModelRow, 3,
                          numItem(groupNum(m_lastMesh.numTriangles)));
    m_modelTable->setItem(m_liveModelRow, 4,
                          numItem(QString::number(volCm3, 'f', 2) + " cm³"));
    m_modelTable->setItem(m_liveModelRow, 5,
                          textItem(QString::number(m_voxMat->value())));
}

// ── アセンブリツリーを取込メッシュの実測値で作り直す ───────────────────────
// STEP の部品階層は未実装なので「1 メッシュ = 1 部品」として、実際に測った
// 値 (三角形数・溶接後頂点数・bbox・表面積・体積) だけを並べる。
// 未取込のときはツリーを空にして導線ラベルだけを出す。
void GeometryTab::refreshAssemblyTree()
{
    if (!m_asmTree) return;
    m_asmTree->clear();
    if (!m_hasMesh) {
        m_asmTree->setVisible(false);
        if (m_asmNone) m_asmNone->setVisible(true);
        return;
    }
    m_asmTree->setVisible(true);
    if (m_asmNone) m_asmNone->setVisible(false);

    const ImportedMesh &m = m_lastMesh;
    const QString file = QFileInfo(m.sourcePath).fileName();
    auto *root = new QTreeWidgetItem(m_asmTree,
        { "📦 " + (file.isEmpty() ? m.name : file),
          I18n::tr("geoc_asm_root_fmt").arg(groupNum(m.numTriangles)) });
    root->setForeground(1, QColor(kMuted));

    auto row = [root](const QString &label, const QString &value) {
        auto *it = new QTreeWidgetItem(root, { "🔧 " + label, value });
        it->setForeground(1, QColor(kAcc));
        return it;
    };
    row(I18n::tr("geoc_asm_tri"), groupNum(m.numTriangles));
    row(I18n::tr("geoc_asm_vert"),
        m_diag.valid ? groupNum(m_diag.uniqueVertices)
                     : I18n::tr("geoc_asm_vert_na"));
    row(I18n::tr("geoc_asm_bbox"),
        QString::fromUtf8("%1 × %2 × %3 mm")
            .arg(QString::number((m.bbox[3] - m.bbox[0]) * 1e3, 'f', 1),
                 QString::number((m.bbox[4] - m.bbox[1]) * 1e3, 'f', 1),
                 QString::number((m.bbox[5] - m.bbox[2]) * 1e3, 'f', 1)));
    row(I18n::tr("geoc_asm_area"),
        QString::number(m.surfaceArea, 'g', 4) + QString::fromUtf8(" m²"));
    row(I18n::tr("geoc_asm_vol"),
        QString::number(meshVolume(m) * 1e6, 'f', 2) + QString::fromUtf8(" cm³"));

    auto *note = new QTreeWidgetItem(root, { "📐 " + I18n::tr("geoc_asm_placed") });
    note->setForeground(0, QColor(kMuted));

    m_asmTree->expandAll();
    m_asmTree->resizeColumnToContents(0);
}

// ── ジオメトリ検査の表を実検出数で更新する ─────────────────────────────────
// 6 行はすべて io/MeshDiagnostics が実メッシュから数えた値。取込前は「—」、
// 三角形数の上限を超えて検査を省略した場合はその旨を出す (偽の OK は出さない)。
// ── 主軸の自動検出 ─────────────────────────────────────────────────────────
// 取込メッシュ (変換前) の面積重み付き慣性主軸を求め、それを X/Y/Z へ揃える
// 回転角を回転欄へ入れる。回転欄は編集可能なままにする (自動検出は「良い
// 初期値を入れる」機能で、以後は利用者が微調整できる)。
void GeometryTab::applyAutoAxis()
{
    if (!m_placeAutoAxis || !m_autoAxisNote) return;
    if (!m_placeAutoAxis->isChecked()) {
        m_autoAxisNote->setText(QString());
        return;                       // OFF は回転欄を触らない (勝手に戻さない)
    }
    if (!m_hasMesh) {
        m_autoAxisNote->setText(I18n::tr("geoc_autoaxis_nomesh"));
        return;
    }
    const PrincipalAxes pa = principalAxes(m_rawMesh);
    if (!pa.valid) {
        m_autoAxisNote->setText(I18n::tr("geoc_autoaxis_fail"));
        return;
    }
    if (pa.degenerate) {
        // 立方体・球のような形は向きが一意に決まらない。適当な回転を
        // 入れると「検出できた」と誤解させるので、入れずに理由を出す。
        m_autoAxisNote->setText(I18n::tr("geoc_autoaxis_degenerate"));
        return;
    }
    for (int a = 0; a < 3; ++a)
        if (m_placeRot[a]) m_placeRot[a]->setValue(pa.eulerXYZ_deg[a]);
    m_autoAxisNote->setText(I18n::tr("geoc_autoaxis_done")
        .arg(QString::number(pa.eulerXYZ_deg[0], 'f', 2),
             QString::number(pa.eulerXYZ_deg[1], 'f', 2),
             QString::number(pa.eulerXYZ_deg[2], 'f', 2)));
}

// ── 修復の実行 ─────────────────────────────────────────────────────────────
// 取込メッシュ (変換前) を修復して置き換え、配置・変換をかけ直してから
// 検査をやり直す。穴埋めは実装していないので、残った境界エッジは結果に出す。
void GeometryTab::runHealing()
{
    if (!m_hasMesh || !m_diag.valid) return;
    ImportedMesh fixed;
    RepairReport rep;
    if (!repairMesh(m_rawMesh, RepairOptions(), fixed, rep)) {
        m_healResult->setText(rep.skippedTooLarge
                                  ? I18n::tr("geoc_heal_toolarge")
                                  : I18n::tr("geoc_heal_failed"));
        return;
    }
    m_rawMesh = fixed;
    m_lastMesh = applyPlacement(fixed);
    m_diag = rep.after;

    QStringList parts;
    parts << I18n::tr("geoc_heal_done")
                 .arg(rep.weldedVertices)
                 .arg(rep.removedTriangles)
                 .arg(rep.flippedTriangles);
    if (rep.componentsFlipped > 0)
        parts << I18n::tr("geoc_heal_outward").arg(rep.componentsFlipped);
    if (rep.boundaryEdgesLeft > 0)
        parts << I18n::tr("geoc_heal_holes").arg(rep.boundaryEdgesLeft);
    else if (rep.after.watertight())
        parts << I18n::tr("geoc_heal_watertight");
    m_healResult->setText(parts.join(QStringLiteral(" ")));

    refreshImportBadges();
    refreshAssemblyTree();
    refreshHealing();
}

void GeometryTab::refreshHealing()
{
    if (m_healBtn) m_healBtn->setEnabled(m_diag.valid);
    if (!m_healTable) return;

    // 取込前 / 検査省略: 数値を出さずに理由を示す
    if (!m_diag.valid) {
        for (int r = 0; r < 6; ++r) {
            m_healTable->item(r, 1)->setText(I18n::tr("geoc_heal_pending"));
            auto *st = m_healTable->item(r, 2);
            st->setText(I18n::tr("geoc_heal_pending"));
            st->setForeground(QColor(kMuted));
        }
        if (m_healNone)
            m_healNone->setText(m_diag.skippedTooLarge
                ? I18n::tr("geoc_heal_skip")
                      .arg(groupNum(kMeshDiagnosticsMaxTriangles))
                : I18n::tr("geoc_heal_none"));
        return;
    }

    // 検出数と状態。重複頂点・縮退三角形は「あっても致命的ではない」ので
    // 情報扱い、境界/非多様体/法線不一致はボクセル化を壊すので要修正扱い。
    struct Row { int count; bool critical; };
    const Row rows[5] = {
        { m_diag.duplicateVertices,   false },
        { m_diag.degenerateTriangles, false },
        { m_diag.boundaryEdges,       true  },
        { m_diag.nonManifoldEdges,    true  },
        { m_diag.inconsistentEdges,   true  },
    };
    for (int r = 0; r < 5; ++r) {
        m_healTable->item(r, 1)->setText(
            I18n::tr("geoc_heal_cnt").arg(groupNum(rows[r].count)));
        auto *st = m_healTable->item(r, 2);
        const bool ok = rows[r].count == 0;
        st->setText(ok ? I18n::tr("geoc_heal_ok")
                       : I18n::tr(rows[r].critical ? "geoc_heal_ng"
                                                   : "geoc_heal_info"));
        st->setForeground(QColor(ok ? kOk : (rows[r].critical ? kWarn : kMuted)));
    }
    // 6 行目: 閉ソリッド (水密) 判定 — 境界/非多様体エッジが 0 かどうか
    const bool wt = m_diag.watertight();
    m_healTable->item(5, 1)->setText(
        I18n::tr("geoc_heal_cnt").arg(groupNum(m_diag.boundaryEdges
                                               + m_diag.nonManifoldEdges)));
    m_healTable->item(5, 2)->setText(
        I18n::tr(wt ? "geoc_heal_wt_ok" : "geoc_heal_wt_ng"));
    m_healTable->item(5, 2)->setForeground(QColor(wt ? kOk : kWarn));

    if (m_healNone)
        m_healNone->setText(I18n::tr("geoc_heal_tol")
                                .arg(QString::number(m_diag.weldTolerance,
                                                     'g', 3)));
}

// ボクセル統計を実際のボクセル化結果で置き換える。
// 境界セル数と形状誤差は部分体積率 (PVF) を有効にしたときだけ算出できる
// (境界セルの体積率を数えるのが PVF そのものなので)。共形セル比率は
// 共形 FDTD 用の量で、こちらは未実装 (docs/libigl-integration.md)。
void GeometryTab::refreshVoxelStats()
{
    if (!m_hasVox || !m_statOcc) return;

    const double pct = m_voxTotal > 0
        ? 100.0 * double(m_voxOccupied) / double(m_voxTotal) : 0.0;
    m_statOcc->setText(I18n::tr("geoc_stat_occ_fmt")
                           .arg(groupNum(m_voxOccupied), groupNum(m_voxTotal),
                                QString::number(pct, 'f', 1)));
    if (m_voxHasPvf) {
        m_statBnd->setText(I18n::tr("geoc_stat_bnd_fmt")
                               .arg(groupNum(m_voxBoundary))
                               .arg(m_voxPvfN * m_voxPvfN * m_voxPvfN));
        // 形状誤差はメッシュ本来の体積を基準にした階段近似 / PVF の体積差。
        // 体積が取れないメッシュ (開いている等) では比を出さない。
        if (m_voxMeshVol > 0.0)
            m_statErr->setText(
                I18n::tr("geoc_stat_err_fmt")
                    .arg(QString::number(
                             100.0 * (m_voxStairVol - m_voxMeshVol) / m_voxMeshVol,
                             'f', 1),
                         QString::number(
                             100.0 * (m_voxPvfVol - m_voxMeshVol) / m_voxMeshVol,
                             'f', 2)));
        else
            m_statErr->setText(I18n::tr("geoc_stat_novol"));
    } else {
        m_statBnd->setText(I18n::tr("geoc_stat_na"));
        m_statErr->setText(I18n::tr("geoc_stat_na"));
    }
    m_statConf->setText(I18n::tr("geoc_stat_stair"));

    if (m_voxBadge) {
        m_voxBadge->setText(I18n::tr("geoc_vox_badge_fmt")
                                .arg(groupNum(m_voxTotal),
                                     groupNum(m_voxOccupied)));
        // 「未実行」の muted 表示から実測値の ok 表示へ
        m_voxBadge->setStyleSheet(QStringLiteral("color:%1; font-weight:600;")
                                      .arg(QLatin1String(kOk)));
    }
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

    // 細分化領域 (.ofdx) はファイル読込で入れ替わるので併せて再表示し、
    // セル増の見積り (格子に依存) も取り直す
    refreshRegionTable();
    updateRefineBadge();
}
