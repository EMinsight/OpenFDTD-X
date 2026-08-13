// LensEditorTab.cpp
#include "LensEditorTab.h"
#include "TabHelpers.h"
#include "../core/GlassCatalog.h"
#include "../core/Project.h"
#include "../optics/ParaxialTrace.h"
#include "../optics/DampedLeastSquares.h"
#include "../optics/DistortionGrid.h"
#include "../optics/FieldCurvature.h"
#include "../optics/EncircledEnergy.h"
#include "../optics/Mtf.h"
#include "../optics/RayTrace.h"
#include "../optics/SeidelAberration.h"
#include "../widgets/MiniPlot.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"

#include <QBrush>
#include <QColor>
#include <QComboBox>
#include <QCompleter>
#include <QFont>
#include <QFontDatabase>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QStandardItemModel>
#include <QStringList>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

using namespace ofd;

// ── タブ専用の翻訳キー (接頭辞 lde_) ────────────────────────────────────────
namespace {
const bool s_i18n = [] {
    using ofd::I18n;
    I18n::reg("lde_lde_section", "Lens Data Editor (Zemax風)",
              "Lens Data Editor (Zemax style)");
    I18n::reg("lde_table_hint",
              "順次光線追跡用の光学系定義。各行が1つの面 (R=曲率半径、厚さ、ガラス材質、半径)。最上行=物体、最下行=像面。\n"
              "ガラス欄はカタログ自動補完 (N-BK7, S-LAH64… と入力)。「🔷 ガラスカタログ」タブでガラスマップから選択も可能。",
              "Optical system definition for sequential ray tracing. One row per surface (R=radius, thickness, glass, semi-diameter). Top row = object, bottom row = image plane.\n"
              "The glass column auto-completes from the catalog (type N-BK7, S-LAH64…). You can also pick from the glass map in the 🔷 Glass Catalog tab.");
    I18n::reg("lde_col_type",    "種別",        "Type");
    I18n::reg("lde_col_r",       "曲率R [mm]",  "Radius R [mm]");
    I18n::reg("lde_col_thick",   "厚さ [mm]",   "Thickness [mm]");
    I18n::reg("lde_col_glass",   "ガラス",      "Glass");
    I18n::reg("lde_col_semid",   "半径",        "Semi-dia.");
    I18n::reg("lde_col_conic",   "コーニック",  "Conic");
    I18n::reg("lde_col_comment", "コメント",    "Comment");
    I18n::reg("lde_ins_tip",     "次に挿入",    "Insert after");
    I18n::reg("lde_del_tip",     "削除",        "Delete");
    I18n::reg("lde_types_label", "面種別:",     "Surface types:");
    I18n::reg("lde_type_obj",    "OBJ=物体",    "OBJ=object");
    I18n::reg("lde_type_std",    "STD=球面",    "STD=sphere");
    I18n::reg("lde_type_sto",    "STO=絞り",    "STO=stop");
    I18n::reg("lde_type_asp",    "ASP=非球面",  "ASP=asphere");
    I18n::reg("lde_type_fre",    "FRE=自由曲面","FRE=freeform");
    I18n::reg("lde_type_img",    "IMG=像面",    "IMG=image");
    I18n::reg("lde_sys_section", "システム諸元 / System spec", "System spec");
    I18n::reg("lde_epd",         "入射瞳径",    "Entrance pupil dia.");
    I18n::reg("lde_field",       "視野",        "Field");
    I18n::reg("lde_field_unit",  "° (半角)",    "° (half angle)");
    I18n::reg("lde_waves",       "波長サンプル", "Wavelength samples");
    I18n::reg("lde_wave_add",    "+ 追加",      "+ Add");
    I18n::reg("lde_wave_note",
              "(断面プレビューと 3 次収差は d 線。解析プロットは全波長を使います)",
              "(the layout preview and the Seidel sums use the d line; the "
              "analyses use every wavelength)");
    I18n::reg("lde_wave_add_title", "波長サンプルの追加", "Add a wavelength");
    I18n::reg("lde_wave_add_label", "波長 [nm]", "Wavelength [nm]");
    I18n::reg("lde_wave_dup", "その波長は既に入っています。",
              "That wavelength is already in the list.");
    I18n::reg("lde_wave_tip",
              "クリックで削除 (残り 1 本のときは消せません)",
              "Click to remove (the last one cannot be removed)");
    I18n::reg("lde_coord",       "座標系",      "Coordinate system");
    I18n::reg("lde_coord_seq",   "順次光線追跡", "Sequential ray tracing");
    I18n::reg("lde_coord_nonseq","非順次 (LightTools/TracePro) (未実装)",
              "Non-sequential (LightTools/TracePro) (not implemented)");
    I18n::reg("lde_coord_hybrid","ハイブリッド (未実装)",
              "Hybrid (not implemented)");
    I18n::reg("lde_merit_section","Merit Function (FoM)", "Merit Function (FoM)");
    I18n::reg("lde_merit_hint",
              "最適化評価関数の定義 (目標・重みは編集可能。最適化そのものは未実装)。"
              "「値」列は面テーブルから計算する — 近軸オペランド "
              "(EFFL / PIMH / ISFN) は y-nu 追跡、収差オペランド "
              "(SPHA / COMA / ASTI / DIST) は 3 次収差 (ザイデル和) による。"
              "5 次以上の収差は含まないので、大口径・広角では実光線追跡との差が出る。",
              "Merit-function definition (target and weight are editable; the "
              "optimizer itself is not implemented). The value column is computed "
              "from the surface table: the paraxial operands (EFFL / PIMH / ISFN) "
              "from the y-nu trace, the aberration operands (SPHA / COMA / ASTI / "
              "DIST) from the third-order (Seidel) sums. Fifth- and higher-order "
              "terms are not included, so fast or wide-angle systems will differ "
              "from a real ray trace.");
    I18n::reg("lde_col_operand", "オペランド",  "Operand");
    I18n::reg("lde_col_target",  "目標",        "Target");
    I18n::reg("lde_col_weight",  "重み",        "Weight");
    I18n::reg("lde_col_value",   "値",          "Value");
    I18n::reg("lde_col_basis",   "根拠 / 必要な計算", "Basis / required computation");
    I18n::reg("lde_op_spha",     "SPHA (球面収差)", "SPHA (spherical aberration)");
    I18n::reg("lde_op_coma",     "COMA (コマ収差)", "COMA (coma)");
    I18n::reg("lde_op_asti",     "ASTI (非点)",     "ASTI (astigmatism)");
    I18n::reg("lde_op_effl",     "EFFL (有効焦点)", "EFFL (effective focal length)");
    I18n::reg("lde_op_dist",     "DIST (歪曲)",     "DIST (distortion)");
    I18n::reg("lde_op_pimh",     "PIMH (近軸像高)", "PIMH (paraxial image height)");
    I18n::reg("lde_op_isfn",     "ISFN (像側F値)",  "ISFN (image-space F/#)");
    I18n::reg("lde_b_paraxial",  "近軸 y-nu 追跡 (面テーブルから計算)",
              "Paraxial y-nu trace of the surface table");
    I18n::reg("lde_b_needreal",
              "実光線追跡 (収差計算) が必要 — 未実装",
              "Requires real ray tracing (aberration calculation) — not implemented");
    I18n::reg("lde_b_nofield",
              "視野半角が 0 なので恒等的に 0 — 視野を入れると値が出ます",
              "Identically zero because the half-field angle is zero — enter a "
              "field to get a value");
    I18n::reg("lde_b_seidel",
              "3 次収差 (ザイデル和) — 近軸追跡から計算。単位は波長 (d線)",
              "Third-order (Seidel) sums from the paraxial trace, in waves at "
              "the d line");
    // ── 3 次収差 (ザイデル) ──────────────────────────────────────────────
    I18n::reg("lde_sd_section", "3 次収差 (ザイデル) / Seidel aberrations",
              "Seidel (third-order) aberrations");
    I18n::reg("lde_sd_hint",
              "縁光線と主光線の近軸追跡だけで決まる 3 次収差。実光線追跡は要らない "
              "(W. T. Welford, Aberrations of Optical Systems §8)。"
              "値は波面収差の峰値 [波長, d線] で、瞳端・視野端での量。"
              "5 次以上・色収差・非球面項は含まない。",
              "The third-order aberrations follow from the paraxial marginal and "
              "chief rays alone — no real ray trace is needed (W. T. Welford, "
              "Aberrations of Optical Systems §8). Values are peak wavefront "
              "coefficients in waves at the d line, at the edge of the pupil and "
              "of the field. Fifth-order, chromatic and aspheric terms are not "
              "included.");
    I18n::reg("lde_sd_surf",   "面",             "Surface");
    I18n::reg("lde_sd_total",  "総和",           "Total");
    I18n::reg("lde_sd_si",     "球面収差 SI",    "Spherical SI");
    I18n::reg("lde_sd_sii",    "コマ SII",       "Coma SII");
    I18n::reg("lde_sd_siii",   "非点 SIII",      "Astigmatism SIII");
    I18n::reg("lde_sd_siv",    "像面湾曲 SIV",   "Field curvature SIV");
    I18n::reg("lde_sd_sv",     "歪曲 SV",        "Distortion SV");
    I18n::reg("lde_sd_petz",   "ペッツバール半径", "Petzval radius");
    I18n::reg("lde_sd_lagr",   "ラグランジュ不変量 H", "Lagrange invariant H");
    I18n::reg("lde_sd_nofield",
              "視野半角が 0 なので、球面収差以外は 0 になります "
              "(H = 0)。視野を入れると全項が出ます。",
              "With a zero half-field angle every term except spherical "
              "aberration vanishes (H = 0). Enter a field angle to get them "
              "all.");
    I18n::reg("lde_dash",        "—",           "—");
    I18n::reg("lde_parax_section", "近軸諸元 / Paraxial data", "Paraxial data");
    I18n::reg("lde_parax_hint",
              "面テーブル (有効行) の屈折率・曲率・厚さから y-nu 近軸追跡で計算。"
              "ガラス名がカタログにない場合は n=1.6 と仮定するため注意。",
              "Computed by a y-nu paraxial trace from the enabled rows of the "
              "surface table. Note that glasses missing from the catalog are "
              "assumed to have n = 1.6.");
    I18n::reg("lde_parax_item",  "項目",  "Quantity");
    I18n::reg("lde_parax_value", "値",    "Value");
    I18n::reg("lde_px_efl",   "有効焦点距離 f'", "Effective focal length f'");
    I18n::reg("lde_px_bfl",   "バックフォーカス (最終面→後側焦点)",
              "Back focal length (last vertex → rear focus)");
    I18n::reg("lde_px_ffl",   "フロントフォーカス (第1面→前側焦点)",
              "Front focal length (first vertex → front focus)");
    I18n::reg("lde_px_hp",    "後側主点 H' (最終面から)",
              "Rear principal point H' (from the last vertex)");
    I18n::reg("lde_px_h",     "前側主点 H (第1面から)",
              "Front principal point H (from the first vertex)");
    I18n::reg("lde_px_fno",   "像側 F 値 (f'/EPD)", "Image-space F/# (f'/EPD)");
    I18n::reg("lde_px_imh",   "近軸像高 f'·tanθ", "Paraxial image height f'·tanθ");
    I18n::reg("lde_px_track", "第1面→最終面の長さ", "First → last surface length");
    I18n::reg("lde_px_defocus", "像面デフォーカス (近軸焦点 − 像面)",
              "Image-plane defocus (paraxial focus − image plane)");
    I18n::reg("lde_px_assumed", "n=1.6 と仮定したガラス (カタログ外)",
              "Glasses assumed to have n = 1.6 (not in the catalog)");
    I18n::reg("lde_px_invalid",
              "面テーブルが近軸的に解けない (アフォーカル / 面が無い)",
              "The surface table is not paraxially solvable (afocal / no surfaces)");
    I18n::reg("lde_optimize",    "▶ 最適化実行",  "▶ Run optimization");
    I18n::reg("lde_opt_target",  "目標 f' [mm]",  "Target f' [mm]");
    I18n::reg("lde_opt_apply",   "面テーブルへ適用", "Apply to the table");
    I18n::reg("lde_opt_res",
              "曲率半径 %1 面を変数にして f' を %2 mm に合わせました: "
              "%3 mm → %4 mm (残差 %5 mm、%6 回の反復で %7 回評価)。%8",
              "Optimised %1 curved surfaces to bring f' to %2 mm: %3 mm -> "
              "%4 mm (residual %5 mm, %6 iterations and %7 evaluations). %8");
    I18n::reg("lde_opt_apply_hint",
              "結果はまだ面テーブルに書き戻していません — 「面テーブルへ適用」で"
              "反映します。",
              "The result has not been written back to the surface table yet; "
              "press \"Apply to the table\" to do that.");
    I18n::reg("lde_opt_miss",
              " ⚠ 目標に届きませんでした (残差 %1 mm)。変数が足りないか、"
              "目標がこの構成では実現できません。値は書き戻していません。",
              " [!] The target was not reached (residual %1 mm). There may be "
              "too few variables, or the target is not achievable with this "
              "configuration. The values were not written back.");
    I18n::reg("lde_opt_none",
              "曲率半径が有限の面がありません。最適化する変数がありません。",
              "There is no surface with a finite radius, so there is nothing "
              "to optimise.");
    I18n::reg("lde_opt_applied", "面テーブルへ適用しました。",
              "Applied to the surface table.");
    I18n::reg("lde_analyses_section", "解析プロット / Analyses", "Analyses");
    I18n::reg("lde_an_mtf",      "MTF (変調伝達関数)",
              "MTF (modulation transfer function)");
    I18n::reg("lde_an_result", "解析結果 (実光線追跡)",
              "Analysis result (real ray trace)");
    I18n::reg("lde_an_why",
              "波面 (Zernike) は未実装です。スポットダイアグラム・レイファン・"
              "色収差・MTF・包絡エネルギー・歪曲格子・像面湾曲は実光線追跡で"
              "計算します。",
              "The wavefront (Zernike) map is not implemented. The spot "
              "diagram, the ray fan, the chromatic focal shift, the MTF, the "
              "encircled energy, the distortion grid and the field curvature "
              "are computed by real ray tracing.");
    I18n::reg("lde_fc_x", "近軸像面からのずれ [mm]",
              "Shift from the paraxial image plane [mm]");
    I18n::reg("lde_fc_y", "視野半角 [deg]", "Half-field angle [deg]");
    I18n::reg("lde_fc_res",
              "視野端 (%1°) で サジタル %2 mm・タンジェンシャル %3 mm、"
              "非点隔差の最大は %4 mm です。像空間では光線が直線なので、像面を "
              "2 枚置いた外挿で交点を厳密に解いています (焦点探索なし)。",
              "At the edge of the field (%1 deg) the sagittal focus is at "
              "%2 mm and the tangential at %3 mm; the largest astigmatic "
              "separation is %4 mm. Rays are straight in image space, so the "
              "crossings are solved exactly by extrapolating between two image "
              "planes (no focus search).");
    I18n::reg("lde_fc_note",
              " 実光線の交点なので、3 次収差 (ザイデル) の像面湾曲とは"
              "別ものです (視野が大きいほど離れます)。同じ図には混ぜていません。",
              " These are real-ray crossings, not the third-order (Seidel) "
              "field curvature; the two diverge as the field grows and are not "
              "mixed in one plot.");
    I18n::reg("lde_fc_cut",
              " ⚠ 指定した視野半角は %1° ですが、%2° より外では瞳端の光線が"
              "けられて追跡できませんでした。曲線はそこまでです "
              "(足りない部分を外挿してはいません)。",
              " [!] The requested half-field is %1 deg, but beyond %2 deg the "
              "marginal rays are vignetted and could not be traced. The curve "
              "stops there (the missing part is not extrapolated).");
    I18n::reg("lde_fc_bad",
              "像面湾曲は視野と実光線追跡が要ります。視野が 0 か、上下・左右の"
              "光線を追跡できませんでした。",
              "The field-curvature plot needs a non-zero field and a successful "
              "ray trace of the upper/lower and left/right rays.");
    I18n::reg("lde_dg_x", "像面 x [mm]", "Image plane x [mm]");
    I18n::reg("lde_dg_y", "像面 y [mm]", "Image plane y [mm]");
    I18n::reg("lde_dg_res",
              "隅 (視野半角 %1°) での歪曲は %2 %%、格子内の最大は %3 %% です。"
              "薄い線が理想の格子 (y = f'·tanθ、f' = %4 mm)、濃い線が主光線の"
              "実追跡です。",
              "At the corner (half-field %1 deg) the distortion is %2 %%; the "
              "largest in the grid is %3 %%. The faint lines are the ideal grid "
              "(y = f'*tan(theta), f' = %4 mm) and the solid lines are the real "
              "chief-ray trace.");
    I18n::reg("lde_dg_note",
              " 歪曲は主光線の像高だけで決まるので、ぼけ (収差) の大きさとは"
              "別ものです。正が糸巻き型、負が樽型。",
              " Distortion depends only on where the chief ray lands, so it is "
              "independent of how blurred the spot is. Positive is pincushion, "
              "negative is barrel.");
    I18n::reg("lde_dg_bad",
              "歪曲格子は近軸の焦点距離と主光線の実追跡が要ります。視野が 0 か、"
              "主光線を追跡できませんでした。",
              "The distortion grid needs a paraxial focal length and a chief-ray "
              "trace. The field is zero, or the chief ray could not be traced.");
    I18n::reg("lde_ee_x", "重心からの半径 [mm]", "Radius from the centroid [mm]");
    I18n::reg("lde_ee_y", "包絡エネルギー", "Encircled energy");
    I18n::reg("lde_ee_res",
              "光線 %1 本、RMS スポット半径 %2 mm。重心から見て 50 %% が "
              "%3 mm、80 %% が %4 mm、90 %% が %5 mm の円に入ります "
              "(最外は %6 mm)。",
              "%1 rays, RMS spot radius %2 mm. Measured from the centroid, "
              "50 %% falls inside %3 mm, 80 %% inside %4 mm and 90 %% inside "
              "%5 mm (the outermost ray is at %6 mm).");
    I18n::reg("lde_ee_note",
              " 光線 1 本を等しい重みとして数えた幾何の値で、回折は含みません "
              "(エアリーの包絡エネルギーは第 1 種ベッセル関数が要り、MSVC に"
              "無いため未実装)。半径は補間せず、実際にその割合を包む光線の"
              "距離をそのまま出しています。",
              " These are geometric values counting each ray equally; "
              "diffraction is not included (the Airy encircled energy needs "
              "Bessel functions, which MSVC lacks). Radii are not interpolated "
              "-- each is the distance of the ray that actually encloses that "
              "fraction.");
    I18n::reg("lde_mtf_x", "空間周波数 [cycles/mm]", "Spatial frequency [cyc/mm]");
    I18n::reg("lde_mtf_y", "MTF", "MTF");
    I18n::reg("lde_mtf_res",
              "λ = %1 nm・F/%2 → カットオフ %3 cyc/mm。回折限界が 50 %% に"
              "落ちるのは %4 cyc/mm、この系の幾何 MTF は %5 cyc/mm です "
              "(光線 %6 本、RMS スポット半径 %7 mm)。",
              "lambda = %1 nm, F/%2 -> cutoff %3 cyc/mm. The diffraction limit "
              "falls to 50 %% at %4 cyc/mm; this system's geometric MTF does so "
              "at %5 cyc/mm (%6 rays, RMS spot radius %7 mm).");
    I18n::reg("lde_mtf_note",
              " 幾何 MTF は光線の当たり方だけから出しているので回折を含みま"
              "せん。高い空間周波数では実際より良く出るので、回折限界の曲線"
              "と併せて読んでください (収差込みの回折 MTF は瞳での位相が要る"
              "ため未実装)。光線本数が有限なので、幾何 MTF は細かく振動します "
              "(平滑化していません — 本数を増やすと落ち着きます)。",
              " The geometric MTF comes only from where the rays land, so it "
              "does not include diffraction and looks better than reality at "
              "high frequencies; read it together with the diffraction-limit "
              "curve. (A diffraction MTF including aberrations needs the pupil "
              "phase and is not implemented.) The ray count is finite, so the "
              "geometric curve ripples; it is not smoothed (tracing more rays "
              "settles it).");
    I18n::reg("lde_mtf_bad",
              "MTF は近軸の F 値と実光線追跡が要ります。面テーブルが解けないか、"
              "追跡できた光線がありません。",
              "The MTF needs a paraxial f-number and a successful ray trace. "
              "The surface table cannot be solved, or no ray was traced.");
    I18n::reg("lde_an_idle",
              "「スポットダイアグラム」または「レイファン」を押すと、面テーブルの"
              "系を実光線追跡して結果を描きます。",
              "Press \"Spot Diagram\" or \"Ray Fan\" to trace the system in the "
              "surface table and draw the result.");
    I18n::reg("lde_an_fail",
              "実光線追跡できませんでした (面が無い / 主光線が像面に届かない)。",
              "The real ray trace failed (no surfaces, or the chief ray does "
              "not reach the image plane).");
    I18n::reg("lde_an_spot",
              "軸上: RMS %1 µm / 最大 %2 µm、視野端 (%3°): RMS %4 µm / 最大 %5 µm"
              "、追跡 %6 本 (失敗 %7 本)",
              "On axis: RMS %1 µm / max %2 µm; edge of field (%3°): RMS %4 µm / "
              "max %5 µm; %6 rays traced (%7 failed)");
    I18n::reg("lde_an_airy",
              "エアリー半径 1.22·λ·F = %1 µm (λ = 587.6 nm, F/%2)",
              "Airy radius 1.22·λ·F = %1 µm (λ = 587.6 nm, F/%2)");
    I18n::reg("lde_an_fan",
              "横収差の最大値 — 軸上 %1 µm、視野端 (%2°) 子午 %3 µm / 球欠 %4 µm",
              "Peak transverse aberration — on axis %1 µm; edge of field (%2°) "
              "tangential %3 µm / sagittal %4 µm");
    I18n::reg("lde_an_note",
              "光線は「絞りを近軸で物体空間へ結像した入射瞳」へ向けて放ちます"
              "(実光線が絞りを通る位置まで反復して合わせてはいません)。"
              "面の有効半径を超えた光線はけられとして数え、全反射した光線は"
              "捨てます。物体は無限遠。屈折率はカタログの値で、d 線は実測 nd、"
              "他の波長は Sellmeier 分散式から求めます。",
              "Rays are aimed at the entrance pupil obtained by imaging the "
              "stop into object space paraxially (there is no iterative real "
              "ray aiming). Rays outside a surface's clear semi-diameter are "
              "counted as vignetted and totally reflected rays are dropped. "
              "The object is at infinity. Refractive indices come from the "
              "catalog: the measured nd at the d line and the Sellmeier "
              "dispersion formula at every other wavelength.");
    I18n::reg("lde_an_defocus",
              "像面が近軸焦点から %1 mm ずれています — この結果は離焦が"
              "支配的です (近軸諸元の「像面デフォーカス」と同じ値)。",
              "The image plane is %1 mm away from the paraxial focus, so this "
              "result is dominated by defocus (the same number as \"image-plane "
              "defocus\" in the paraxial data).");
    I18n::reg("lde_an_onaxis", "(軸上)", "(on axis)");
    I18n::reg("lde_an_atfield", "(視野端)", "(edge of field)");
    I18n::reg("lde_an_poly",
              "%1 波長を合成したスポット (主波長の重心が基準): "
              "軸上 RMS %2 µm、視野端 RMS %3 µm",
              "Polychromatic spot over %1 wavelengths (referenced to the "
              "primary centroid): on axis RMS %2 µm, edge of field RMS %3 µm");
    I18n::reg("lde_an_focshift", "焦点移動", "Focal shift");
    I18n::reg("lde_an_xwave", "波長 [nm]", "Wavelength [nm]");
    I18n::reg("lde_an_yshift", "バックフォーカスのずれ [µm]",
              "Back-focus shift [µm]");
    I18n::reg("lde_an_chrom",
              "主波長 %1 nm からのバックフォーカスのずれ — 選んだ波長での"
              "最大 %2 µm",
              "Back-focus shift from the primary wavelength %1 nm — at most "
              "%2 µm over the chosen wavelengths");
    I18n::reg("lde_an_lca",
              "軸上色収差 (C 線 − F 線) = %1 µm。薄レンズなら f/V に等しく"
              "なります (アッベ数の定義)。",
              "Longitudinal chromatic aberration (C − F) = %1 µm. For a thin "
              "lens this equals f/V, which is the definition of the Abbe "
              "number.");
    I18n::reg("lde_an_xpupil", "正規化瞳座標", "Normalised pupil coordinate");
    I18n::reg("lde_an_yta", "横収差 [µm]", "Transverse aberration [µm]");
    I18n::reg("lde_an_tan_ax", "子午 (軸上)", "Tangential (axis)");
    I18n::reg("lde_an_sag_ax", "球欠 (軸上)", "Sagittal (axis)");
    I18n::reg("lde_an_tan_fld", "子午 (視野端)", "Tangential (field)");
    I18n::reg("lde_an_sag_fld", "球欠 (視野端)", "Sagittal (field)");
    I18n::reg("lde_preview_section", "レイトレース プレビュー / Raytrace preview",
              "Raytrace preview");
    I18n::reg("lde_preview_hint",
              "面テーブルから子午面 2D 光線追跡 (軸上=マゼンタ、±視野=赤/青、瞳内5本)。テーブル編集で自動更新。"
              "カタログ外ガラスは n=1.6 と仮定。",
              "2D meridional ray trace from the surface table (on-axis = magenta, ±field = red/blue, 5 pupil rays). Updates as you edit the table. "
              "Glasses not in the catalog are assumed to have n = 1.6.");
    I18n::reg("lde_air",         "空気",        "Air");
    return true;
}();

// mock の <span className="badge"> 相当
QLabel *makeBadge(const QString &text, const char *variant, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    QString ss = QStringLiteral("border-radius:8px; padding:1px 7px; font-size:11px;");
    if (qstrcmp(variant, "acc") == 0)
        ss += "background:#B83280; color:white; border:1px solid transparent;";
    else
        ss += "border:1px solid palette(mid);";
    l->setStyleSheet(ss);
    return l;
}

// muted text-sm 相当
QLabel *mutedLabel(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setWordWrap(true);
    l->setStyleSheet("color:#7A7A7A; font-size:11px;");
    return l;
}

// ガラス名 → 屈折率 (nd)。AIR/空欄/"-" は 1.0、カタログ外は代表値 1.6
double indexAfter(const QString &glass)
{
    const QString g = glass.trimmed();
    if (g.isEmpty() || g == QStringLiteral("-") || g == QString::fromUtf8("—")
        || g.compare(QStringLiteral("AIR"), Qt::CaseInsensitive) == 0
        || g == QString::fromUtf8("空気"))
        return 1.0;
    for (const Glass &c : GlassCatalog::all())
        if (c.name.compare(g, Qt::CaseInsensitive) == 0)
            return c.nd;
    return 1.6;
}

// λ [µm] における屈折率。**lambda_um <= 0 は「カタログの実測 nd (d 線)」**
// を意味する。Sellmeier は当てはめなので下位桁が動き (実測 nd と 7e-4 ずれる
// 銘柄が実際にある)、d 線の既存表示を変えないためと、色収差の曲線を
// 「全点 Sellmeier」で自己整合させるために、この 2 つを明示的に分けている。
// 係数の無い銘柄は Glass::n が nd/vd の 1 次近似で代用する。カタログ外の
// 銘柄は波長に依らず 1.6 と仮定する (分散が分からないので色収差を作らない)。
double indexAfterAt(const QString &glass, double lambda_um)
{
    const QString g = glass.trimmed();
    if (g.isEmpty() || g == QStringLiteral("-") || g == QString::fromUtf8("—")
        || g.compare(QStringLiteral("AIR"), Qt::CaseInsensitive) == 0
        || g == QString::fromUtf8("空気"))
        return 1.0;
    for (const Glass &c : GlassCatalog::all())
        if (c.name.compare(g, Qt::CaseInsensitive) == 0)
            return (lambda_um <= 0.0) ? c.nd : c.n(lambda_um);
    return 1.6;
}

// カタログに載っていないガラス名か (空気は false)。近軸諸元では n=1.6 と
// 仮定するため、どの銘柄が仮定値なのかを画面に出す (数値の出所を隠さない)。
bool isUnknownGlass(const QString &glass)
{
    const QString g = glass.trimmed();
    if (g.isEmpty() || g == QStringLiteral("-") || g == QString::fromUtf8("—")
        || g.compare(QStringLiteral("AIR"), Qt::CaseInsensitive) == 0
        || g == QString::fromUtf8("空気"))
        return false;
    for (const Glass &c : GlassCatalog::all())
        if (c.name.compare(g, Qt::CaseInsensitive) == 0)
            return false;
    return true;
}
} // namespace

// ── LensLayoutView ──────────────────────────────────────────────────────────
LensLayoutView::LensLayoutView(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(360, 200);
}

void LensLayoutView::setSystem(const QVector<LensSurface> &rows,
                               double epd, double fieldDeg)
{
    m_surfs.clear();
    m_epd = epd > 0 ? epd : 12.0;
    m_field = fieldDeg;
    double z = 0.0;
    for (const LensSurface &r : rows) {
        if (!r.enabled) continue;
        if (r.type == QStringLiteral("OBJ")) continue;   // 無限遠物体 → 平行光
        Surf s;
        s.z = z;
        bool ok = false;
        const double R = r.R.toDouble(&ok);
        s.plane = !ok || R == 0.0;                        // "Infinity" → 平面
        s.R = s.plane ? 0.0 : R;
        const double sd = r.semiD.toDouble(&ok);
        s.semiD = (ok && sd > 0) ? sd : 7.0;
        s.conic = r.conic.toDouble();
        s.stop  = r.type == QStringLiteral("STO");
        s.image = r.type == QStringLiteral("IMG");
        s.n2 = indexAfter(r.glass);
        m_surfs.push_back(s);
        if (s.image) break;
        const double t = r.thick.toDouble(&ok);
        z += ok ? t : 0.0;
    }
    update();
}

void LensLayoutView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), palette().base());
    p.setPen(QPen(palette().mid().color(), 1));
    p.drawRect(rect().adjusted(0, 0, -1, -1));
    if (m_surfs.size() < 2) return;

    const double zEnd = m_surfs.last().z;
    if (zEnd <= 0) return;
    double yMax = 1.0;
    for (const Surf &s : m_surfs) yMax = std::max(yMax, s.semiD);
    yMax *= 1.3;
    const double z0 = -std::max(4.0, zEnd * 0.06);   // 入射光の助走区間
    const double W = width(), H = height();
    const double sx = (W - 24) / (zEnd - z0);
    const double sy = (H - 16) / (2.0 * yMax);
    auto X = [&](double z) { return 12.0 + (z - z0) * sx; };
    auto Y = [&](double y) { return H / 2.0 - y * sy; };
    auto sag = [](const Surf &s, double y) {
        if (s.plane) return 0.0;
        const double c = 1.0 / s.R;
        const double arg = 1.0 - (1.0 + s.conic) * c * c * y * y;
        if (arg <= 0.0) return 0.0;
        return c * y * y / (1.0 + std::sqrt(arg));
    };

    // 光軸
    p.setPen(QPen(palette().midlight().color(), 1, Qt::DashLine));
    p.drawLine(QPointF(X(z0), Y(0)), QPointF(X(zEnd), Y(0)));

    // レンズ縁 (ガラス区間の上下端を接続)
    p.setPen(QPen(palette().text().color(), 1.0));
    for (int i = 0; i + 1 < m_surfs.size(); ++i) {
        const Surf &a = m_surfs[i], &b = m_surfs[i + 1];
        if (a.n2 <= 1.001) continue;
        for (double sgn : { 1.0, -1.0 })
            p.drawLine(QPointF(X(a.z + sag(a, a.semiD)), Y(sgn * a.semiD)),
                       QPointF(X(b.z + sag(b, b.semiD)), Y(sgn * b.semiD)));
    }

    // 面
    for (const Surf &s : m_surfs) {
        if (s.image) {                                   // 像面: 縦実線
            p.setPen(QPen(palette().text().color(), 1.6));
            p.drawLine(QPointF(X(s.z), Y(-s.semiD)), QPointF(X(s.z), Y(s.semiD)));
            continue;
        }
        if (s.stop && s.plane) {                         // 絞り: 上下のティック
            p.setPen(QPen(palette().text().color(), 2.0));
            for (double sgn : { 1.0, -1.0 })
                p.drawLine(QPointF(X(s.z), Y(sgn * s.semiD)),
                           QPointF(X(s.z), Y(sgn * s.semiD * 1.35)));
            continue;
        }
        QPolygonF arc;
        const int N = 32;
        for (int k = 0; k <= N; ++k) {
            const double y = -s.semiD + 2.0 * s.semiD * k / N;
            if (!s.plane) {
                const double c = 1.0 / s.R;
                if (1.0 - (1.0 + s.conic) * c * c * y * y <= 0.0) continue;
            }
            arc << QPointF(X(s.z + sag(s, y)), Y(y));
        }
        p.setPen(QPen(palette().text().color(), 1.2));
        p.drawPolyline(arc);
    }

    // 光線追跡 (軸上 + ±視野、各5本)
    struct FieldSpec { double deg; QColor col; };
    const FieldSpec fields[3] = {
        {       0.0, QColor("#B83280") },
        {  m_field,  QColor("#DC2626") },
        { -m_field,  QColor("#2563EB") },
    };
    double zStop = m_surfs[0].z;
    for (const Surf &s : m_surfs)
        if (s.stop) { zStop = s.z; break; }

    for (const FieldSpec &fs : fields) {
        const double a = fs.deg * M_PI / 180.0;
        const double slope = -std::tan(a);
        p.setPen(QPen(QColor(fs.col.red(), fs.col.green(), fs.col.blue(), 170), 1.1));
        for (int k = 0; k < 5; ++k) {
            const double h = (k - 2) / 2.0 * m_epd / 2.0;   // 瞳内高さ
            QPointF pos(z0, h + slope * (z0 - zStop));      // 絞り位置で h を通す
            QPointF dir(std::cos(a), -std::sin(a));
            double n1 = 1.0;
            QPolygonF poly;
            poly << QPointF(X(pos.x()), Y(pos.y()));
            for (const Surf &s : m_surfs) {
                double t = -1;
                QPointF hit, nrm;
                if (s.plane) {
                    if (std::fabs(dir.x()) < 1e-9) break;
                    t = (s.z - pos.x()) / dir.x();
                    if (t < 1e-9) break;
                    hit = pos + t * dir;
                    nrm = QPointF(-1, 0);
                } else {
                    const QPointF C(s.z + s.R, 0.0);
                    const QPointF oc = pos - C;
                    const double b = 2.0 * QPointF::dotProduct(dir, oc);
                    const double c = QPointF::dotProduct(oc, oc) - s.R * s.R;
                    const double disc = b * b - 4.0 * c;
                    if (disc < 0) break;
                    const double sq = std::sqrt(disc);
                    const double t1 = (-b - sq) / 2.0, t2 = (-b + sq) / 2.0;
                    t = (s.R > 0) ? t1 : t2;                // 頂点側の交点
                    if (t < 1e-9) t = (s.R > 0) ? t2 : t1;
                    if (t < 1e-9) break;
                    hit = pos + t * dir;
                    nrm = (hit - C) / s.R;
                }
                poly << QPointF(X(hit.x()), Y(hit.y()));
                if (s.image) break;
                // Snell (2D ベクトル形)
                QPointF n = nrm;
                double cosi = -(dir.x() * n.x() + dir.y() * n.y());
                if (cosi < 0) { n = -n; cosi = -cosi; }
                const double eta = n1 / s.n2;
                const double sin2t = eta * eta * (1.0 - cosi * cosi);
                if (sin2t > 1.0) break;                     // 全反射
                const double cost = std::sqrt(1.0 - sin2t);
                dir = eta * dir + (eta * cosi - cost) * n;
                const double len = std::hypot(dir.x(), dir.y());
                if (len < 1e-12) break;
                dir /= len;
                pos = hit;
                n1 = s.n2;
            }
            p.drawPolyline(poly);
        }
    }
}

// ── LensEditorTab ───────────────────────────────────────────────────────────
LensEditorTab::LensEditorTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    // 面データはプロジェクト (.ofdx) から。空なら既定の設計例 (Cooke triplet)
    loadRowsFromProject();

    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // Lens Data Editor
    auto *sLde = new SectionBox(I18n::tr("lde_lde_section"), body);
    sLde->vbox()->addWidget(mutedLabel(I18n::tr("lde_table_hint"), sLde));

    m_table = new QTableWidget(0, 10, sLde);
    m_table->setHorizontalHeaderLabels({
        "", "#", I18n::tr("lde_col_type"), I18n::tr("lde_col_r"),
        I18n::tr("lde_col_thick"), I18n::tr("lde_col_glass"),
        I18n::tr("lde_col_semid"), I18n::tr("lde_col_conic"),
        I18n::tr("lde_col_comment"), "" });
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(30);
    m_table->setColumnWidth(0, 26);
    m_table->setColumnWidth(1, 26);
    m_table->setColumnWidth(2, 70);
    m_table->setColumnWidth(3, 82);
    m_table->setColumnWidth(4, 82);
    m_table->setColumnWidth(5, 96);
    m_table->setColumnWidth(6, 60);
    m_table->setColumnWidth(7, 64);
    m_table->setColumnWidth(9, 56);
    m_table->horizontalHeader()->setSectionResizeMode(8, QHeaderView::Stretch);
    m_table->setMinimumHeight(320);
    sLde->vbox()->addWidget(m_table);

    auto *badges = new QHBoxLayout();
    badges->setSpacing(4);
    badges->addWidget(mutedLabel(I18n::tr("lde_types_label"), sLde));
    for (const char *key : { "lde_type_obj", "lde_type_std", "lde_type_sto",
                             "lde_type_asp", "lde_type_fre", "lde_type_img" })
        badges->addWidget(makeBadge(I18n::tr(key), "", sLde));
    badges->addStretch(1);
    sLde->vbox()->addLayout(badges);
    v->addWidget(sLde);

    // システム諸元 / System spec
    auto *sSys = new SectionBox(I18n::tr("lde_sys_section"), body);
    m_epd = new QLineEdit("12.0", sSys);
    m_epd->setMaximumWidth(100);
    auto *epdRow = new QHBoxLayout();
    epdRow->addWidget(m_epd);
    epdRow->addWidget(new QLabel("mm", sSys));
    epdRow->addStretch(1);
    sSys->form()->addRow(I18n::tr("lde_epd"), epdRow);

    m_field = new QLineEdit("20", sSys);
    m_field->setMaximumWidth(60);
    auto *fieldRow = new QHBoxLayout();
    fieldRow->addWidget(m_field);
    fieldRow->addWidget(new QLabel(I18n::tr("lde_field_unit"), sSys));
    fieldRow->addStretch(1);
    sSys->form()->addRow(I18n::tr("lde_field"), fieldRow);

    // 波長サンプル — 既定は F / d / C の 3 本 (色消しの基準線)。
    // 追加した波長は解析プロット (スポット・色収差) で実際に使う。
    m_waves = { 486.1, 587.6, 656.3 };
    auto *waveRow = new QHBoxLayout();
    waveRow->setSpacing(4);
    m_waveBadges = new QWidget(sSys);
    auto *badgeBox = new QHBoxLayout(m_waveBadges);
    badgeBox->setContentsMargins(0, 0, 0, 0);
    badgeBox->setSpacing(4);
    waveRow->addWidget(m_waveBadges);
    auto *waveAdd = new QPushButton(I18n::tr("lde_wave_add"), sSys);
    waveAdd->setFixedHeight(22);
    connect(waveAdd, &QPushButton::clicked, this, &LensEditorTab::addWavelength);
    waveRow->addWidget(waveAdd);
    // 断面プレビューと 3 次収差は d 線単一 (解析プロットは全波長を使う)
    waveRow->addWidget(mutedLabel(I18n::tr("lde_wave_note"), sSys));
    waveRow->addStretch(1);
    sSys->form()->addRow(I18n::tr("lde_waves"), waveRow);
    rebuildWaveBadges();

    m_coord = new QComboBox(sSys);
    m_coord->addItem(I18n::tr("lde_coord_seq"));
    m_coord->addItem(I18n::tr("lde_coord_nonseq"));
    m_coord->addItem(I18n::tr("lde_coord_hybrid"));
    // 実装があるのは順次のみ — 未実装の選択肢は選べなくする (絶対規則 5)
    if (auto *model = qobject_cast<QStandardItemModel *>(m_coord->model())) {
        for (int i : { 1, 2 })
            if (auto *item = model->item(i))
                item->setEnabled(false);
    }
    sSys->form()->addRow(I18n::tr("lde_coord"), m_coord);
    v->addWidget(sSys);

    // 近軸諸元 / Paraxial data (面テーブルから y-nu 追跡で実計算)
    auto *sPx = new SectionBox(I18n::tr("lde_parax_section"), body);
    sPx->vbox()->addWidget(mutedLabel(I18n::tr("lde_parax_hint"), sPx));
    m_paraxial = new QTableWidget(0, 2, sPx);
    m_paraxial->setHorizontalHeaderLabels({ I18n::tr("lde_parax_item"),
                                            I18n::tr("lde_parax_value") });
    m_paraxial->verticalHeader()->setVisible(false);
    m_paraxial->verticalHeader()->setDefaultSectionSize(24);
    m_paraxial->setEditTriggers(QAbstractItemView::NoEditTriggers);
    // 左パネルが狭くても「値」列が隠れないよう、項目名は固定幅 + 省略表示にする
    m_paraxial->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_paraxial->setColumnWidth(0, 190);
    m_paraxial->horizontalHeader()
        ->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_paraxial->horizontalHeader()->setStretchLastSection(false);
    m_paraxial->setTextElideMode(Qt::ElideRight);
    m_paraxial->setMinimumHeight(240);
    sPx->vbox()->addWidget(m_paraxial);
    v->addWidget(sPx);

    // 3 次収差 (ザイデル) — 面ごとの寄与 + 総和。近軸追跡だけで決まる。
    auto *sSd = new SectionBox(I18n::tr("lde_sd_section"), body);
    sSd->vbox()->addWidget(mutedLabel(I18n::tr("lde_sd_hint"), sSd));
    m_seidel = new QTableWidget(0, 6, sSd);
    m_seidel->setHorizontalHeaderLabels({ I18n::tr("lde_sd_surf"),
                                          I18n::tr("lde_sd_si"),
                                          I18n::tr("lde_sd_sii"),
                                          I18n::tr("lde_sd_siii"),
                                          I18n::tr("lde_sd_siv"),
                                          I18n::tr("lde_sd_sv") });
    m_seidel->verticalHeader()->setVisible(false);
    m_seidel->verticalHeader()->setDefaultSectionSize(24);
    m_seidel->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_seidel->horizontalHeader()
        ->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_seidel->setMinimumHeight(200);
    sSd->vbox()->addWidget(m_seidel);
    v->addWidget(sSd);

    // Merit Function (FoM)
    auto *sMerit = new SectionBox(I18n::tr("lde_merit_section"), body);
    sMerit->vbox()->addWidget(mutedLabel(I18n::tr("lde_merit_hint"), sMerit));
    // 目標・重みは編集可能な定義値。既定値は Cooke triplet の設計目標。
    m_fom = {
        { "EFFL", I18n::tr("lde_op_effl"), "50.000", "1.0" },
        { "PIMH", I18n::tr("lde_op_pimh"), "18.200", "1.0" },
        { "ISFN", I18n::tr("lde_op_isfn"), "4.200",  "0.5" },
        { "SPHA", I18n::tr("lde_op_spha"), "0.000",  "1.0" },
        { "COMA", I18n::tr("lde_op_coma"), "0.000",  "1.0" },
        { "ASTI", I18n::tr("lde_op_asti"), "0.000",  "0.8" },
        { "DIST", I18n::tr("lde_op_dist"), "0.000",  "0.5" },
    };
    m_merit = new QTableWidget(0, 6, sMerit);
    m_merit->setHorizontalHeaderLabels({
        "#", I18n::tr("lde_col_operand"), I18n::tr("lde_col_target"),
        I18n::tr("lde_col_weight"), I18n::tr("lde_col_value"),
        I18n::tr("lde_col_basis") });
    m_merit->verticalHeader()->setVisible(false);
    m_merit->horizontalHeader()
        ->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_merit->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
    m_merit->setMinimumHeight(210);
    sMerit->vbox()->addWidget(m_merit);
    auto *optRow = new QHBoxLayout();
    // 最適化は減衰最小二乗 (optics/DampedLeastSquares) で実行する。
    // **結果は自動で書き戻さない** — 「適用」を別操作にして、利用者の設計値を
    // 黙って書き換えないようにする。
    auto *optBtn = new QPushButton(I18n::tr("lde_optimize"), sMerit);
    connect(optBtn, &QPushButton::clicked, this, &LensEditorTab::runOptimize);
    m_optTarget = new QLineEdit(QStringLiteral("100.0"), sMerit);
    m_optTarget->setMaximumWidth(90);
    m_optApply = new QPushButton(I18n::tr("lde_opt_apply"), sMerit);
    m_optApply->setEnabled(false);       // 結果が出るまで押せない
    connect(m_optApply, &QPushButton::clicked,
            this, &LensEditorTab::applyOptimize);
    optRow->addWidget(new QLabel(I18n::tr("lde_opt_target"), sMerit));
    optRow->addWidget(m_optTarget);
    optRow->addWidget(optBtn);
    optRow->addWidget(m_optApply);
    optRow->addWidget(mutedLabel("Damped Least-Squares", sMerit));
    optRow->addStretch(1);
    sMerit->vbox()->addLayout(optRow);
    m_optInfo = mutedLabel(QString(), sMerit);
    m_optInfo->setWordWrap(true);
    sMerit->vbox()->addWidget(m_optInfo);
    v->addWidget(sMerit);

    // 解析プロット / Analyses
    auto *sAn = new SectionBox(I18n::tr("lde_analyses_section"), body);
    auto *grid = new QGridLayout();
    grid->setSpacing(6);
    const QString analyses[8] = {
        QString::fromUtf8("⊙ Spot Diagram"),
        QString::fromUtf8("📐 Ray Fan"),
        QString::fromUtf8("📊 ") + I18n::tr("lde_an_mtf"),
        QString::fromUtf8("⬡ Encircled Energy"),
        QString::fromUtf8("⌖ Field Curvature"),
        QString::fromUtf8("▦ Distortion grid"),
        QString::fromUtf8("🌈 Chromatic Focal Shift"),
        QString::fromUtf8("⊕ Wavefront map (Zernike)"),
    };
    for (int i = 0; i < 8; ++i) {
        auto *b = new QPushButton(analyses[i], sAn);
        b->setFixedHeight(30);
        b->setStyleSheet("text-align:left; padding-left:8px;");
        // スポットダイアグラムとレイファンは実光線追跡 (optics/RayTrace) で
        // 計算する。残りは未実装なので無効化して理由を出す (絶対規則 5)。
        if (i == 0) connect(b, &QPushButton::clicked,
                            this, &LensEditorTab::runSpotDiagram);
        else if (i == 1) connect(b, &QPushButton::clicked,
                                 this, &LensEditorTab::runRayFan);
        else if (i == 2) connect(b, &QPushButton::clicked,
                                 this, &LensEditorTab::runMtf);
        else if (i == 3) connect(b, &QPushButton::clicked,
                                 this, &LensEditorTab::runEncircled);
        else if (i == 4) connect(b, &QPushButton::clicked,
                                 this, &LensEditorTab::runFieldCurvature);
        else if (i == 5) connect(b, &QPushButton::clicked,
                                 this, &LensEditorTab::runDistortion);
        else if (i == 6) connect(b, &QPushButton::clicked,
                                 this, &LensEditorTab::runChromatic);
        else {
            tabhelp::markNotImplemented(b, I18n::tr(tabhelp::notimpl::kEngine));
            b->setToolTip(I18n::tr("lde_an_why"));
        }
        grid->addWidget(b, i / 2, i % 2);
    }
    sAn->vbox()->addLayout(grid);
    v->addWidget(sAn);

    // 解析結果 (実光線追跡) — スポットは等倍散布図、レイファンは折れ線
    auto *sAr = new SectionBox(I18n::tr("lde_an_result"), body);
    m_spotView = new SpotDiagramView(sAr);
    m_spotView->setVisible(false);
    sAr->vbox()->addWidget(m_spotView);
    m_fanPlot = new MiniPlot(sAr);
    m_fanPlot->setMinimumHeight(190);
    m_fanPlot->setLabels(I18n::tr("lde_an_xpupil"), I18n::tr("lde_an_yta"));
    m_fanPlot->setVisible(false);
    sAr->vbox()->addWidget(m_fanPlot);
    m_anInfo = new QLabel(I18n::tr("lde_an_idle"), sAr);
    m_anInfo->setWordWrap(true);
    sAr->vbox()->addWidget(m_anInfo);
    m_anNote = mutedLabel(I18n::tr("lde_an_note"), sAr);
    m_anNote->setWordWrap(true);
    sAr->vbox()->addWidget(m_anNote);
    v->addWidget(sAr);

    // レイトレース プレビュー (面テーブル連動)
    auto *sPre = new SectionBox(I18n::tr("lde_preview_section"), body);
    sPre->vbox()->addWidget(mutedLabel(I18n::tr("lde_preview_hint"), sPre));
    m_layout = new LensLayoutView(sPre);
    sPre->vbox()->addWidget(m_layout);
    v->addWidget(sPre);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    // 別ファイルを開いたら面テーブルを読み直す (タブはモデルの View)
    connect(project, &Project::loaded, this, [this] {
        loadRowsFromProject();
        rebuildTable();
        retrace();
    });

    connect(m_table, &QTableWidget::cellChanged, this, [this](int row, int) {
        if (m_updating) return;
        if (row < 0 || row >= m_rows.size()) return;
        syncRowFromTable(row);
        retrace();
    });
    connect(m_epd, &QLineEdit::editingFinished, this, &LensEditorTab::retrace);
    connect(m_field, &QLineEdit::editingFinished, this, &LensEditorTab::retrace);
    // Merit 表の目標/重みは編集可能 (定義値)。編集したら値列も再計算する。
    connect(m_merit, &QTableWidget::cellChanged, this, [this](int row, int col) {
        if (m_updating) return;
        if (row < 0 || row >= m_fom.size()) return;
        auto *it = m_merit->item(row, col);
        if (!it) return;
        if (col == 2) m_fom[row].target = it->text();
        else if (col == 3) m_fom[row].weight = it->text();
    });

    rebuildTable();
    rebuildMeritTable();
    retrace();
}

// m_fom → Merit 表。目標/重みだけ編集可能にし、値列は recomputeParaxial() が埋める
void LensEditorTab::rebuildMeritTable()
{
    const bool was = m_updating;
    m_updating = true;
    const QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    m_merit->clearContents();
    m_merit->setRowCount(m_fom.size());
    for (int i = 0; i < m_fom.size(); ++i) {
        auto num = [](const QString &t, bool editable) {
            auto *it = new QTableWidgetItem(t);
            it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            if (!editable)
                it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            return it;
        };
        m_merit->setItem(i, 0, num(QString::number(i + 1), false));
        auto *op = new QTableWidgetItem(m_fom[i].label);
        op->setFont(mono);
        op->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        m_merit->setItem(i, 1, op);
        m_merit->setItem(i, 2, num(m_fom[i].target, true));
        m_merit->setItem(i, 3, num(m_fom[i].weight, true));
        m_merit->setItem(i, 4, num(I18n::tr("lde_dash"), false));
        auto *basis = new QTableWidgetItem(I18n::tr("lde_b_needreal"));
        basis->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        m_merit->setItem(i, 5, basis);
    }
    m_updating = was;
}

// m_rows → QTableWidget。行の挿入/削除時のみ全再構築する
void LensEditorTab::rebuildTable()
{
    m_updating = true;
    m_table->clearContents();
    m_table->setRowCount(m_rows.size());

    // ガラス補完候補 (mock の datalist 相当): カタログ全銘柄 + AIR
    QStringList glassNames;
    for (const Glass &g : GlassCatalog::all())
        glassNames << g.name;
    glassNames << QStringLiteral("AIR");

    for (int i = 0; i < m_rows.size(); ++i) {
        const LensSurface &r = m_rows[i];

        auto *en = new QTableWidgetItem;
        en->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        en->setCheckState(r.enabled ? Qt::Checked : Qt::Unchecked);
        m_table->setItem(i, 0, en);

        auto *no = new QTableWidgetItem(QString::number(i));
        no->setFlags(Qt::ItemIsEnabled);
        no->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(i, 1, no);

        auto *type = new QComboBox(m_table);
        type->addItems({ "OBJ", "STD", "STO", "ASP", "BIN", "EVN",
                         "HOL", "FRE", "IMG" });
        type->setCurrentText(r.type);
        m_table->setCellWidget(i, 2, type);
        connect(type, &QComboBox::currentTextChanged, this,
                [this, i](const QString &t) {
            if (m_updating || i >= m_rows.size()) return;
            m_rows[i].type = t;
            applyStopHighlight();
            retrace();
        });

        auto num = [](const QString &t) {
            auto *it = new QTableWidgetItem(t);
            it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            return it;
        };
        m_table->setItem(i, 3, num(r.R));
        m_table->setItem(i, 4, num(r.thick));

        auto *gl = new QLineEdit(r.glass, m_table);
        gl->setFrame(false);
        auto *comp = new QCompleter(glassNames, gl);
        comp->setCaseSensitivity(Qt::CaseInsensitive);
        gl->setCompleter(comp);
        m_table->setCellWidget(i, 5, gl);
        connect(gl, &QLineEdit::textChanged, this, [this, i](const QString &t) {
            if (m_updating || i >= m_rows.size()) return;
            m_rows[i].glass = t;
            retrace();
        });

        m_table->setItem(i, 6, num(r.semiD));
        m_table->setItem(i, 7, num(r.conic));
        m_table->setItem(i, 8, new QTableWidgetItem(r.comment));

        // 挿入/削除ボタン (再構築はシグナル脱出後に遅延実行)
        auto *ops = new QWidget(m_table);
        auto *oh = new QHBoxLayout(ops);
        oh->setContentsMargins(2, 2, 2, 2);
        oh->setSpacing(2);
        auto *bIns = new QPushButton("+", ops);
        bIns->setFixedSize(22, 20);
        bIns->setToolTip(I18n::tr("lde_ins_tip"));
        auto *bDel = new QPushButton(QString::fromUtf8("×"), ops);
        bDel->setFixedSize(22, 20);
        bDel->setToolTip(I18n::tr("lde_del_tip"));
        oh->addWidget(bIns);
        oh->addWidget(bDel);
        m_table->setCellWidget(i, 9, ops);
        connect(bIns, &QPushButton::clicked, this, [this, i] {
            QTimer::singleShot(0, this, [this, i] {
                LensSurface s;
                s.type = "STD"; s.R = "Infinity"; s.thick = "0";
                s.glass = "AIR"; s.semiD = "-"; s.conic = "0";
                m_rows.insert(qMin(i + 1, int(m_rows.size())), s);
                rebuildTable();
                retrace();
            });
        });
        connect(bDel, &QPushButton::clicked, this, [this, i] {
            QTimer::singleShot(0, this, [this, i] {
                if (i < 0 || i >= m_rows.size()) return;
                m_rows.remove(i);
                rebuildTable();
                retrace();
            });
        });
    }
    m_updating = false;
    applyStopHighlight();
}

void LensEditorTab::syncRowFromTable(int row)
{
    LensSurface &r = m_rows[row];
    if (auto *en = m_table->item(row, 0))
        r.enabled = en->checkState() == Qt::Checked;
    auto cell = [this, row](int col) {
        auto *it = m_table->item(row, col);
        return it ? it->text() : QString();
    };
    r.R       = cell(3);
    r.thick   = cell(4);
    r.semiD   = cell(6);
    r.conic   = cell(7);
    r.comment = cell(8);
}

// mock の tr className="sel" 相当: STO 行をアクセント色でハイライト
void LensEditorTab::applyStopHighlight()
{
    const bool was = m_updating;
    m_updating = true;
    for (int i = 0; i < m_rows.size(); ++i) {
        const QBrush bg = (m_rows[i].type == QStringLiteral("STO"))
            ? QBrush(QColor(184, 50, 128, 36)) : QBrush();
        for (int c : { 0, 1, 3, 4, 6, 7, 8 })
            if (auto *it = m_table->item(i, c))
                it->setBackground(bg);
    }
    m_updating = was;
}

void LensEditorTab::retrace()
{
    bool ok = false;
    double epd = m_epd->text().toDouble(&ok);
    if (!ok || epd <= 0) epd = 12.0;
    double field = m_field->text().toDouble(&ok);
    if (!ok) field = 20.0;
    m_layout->setSystem(m_rows, epd, field);
    pushRowsToProject();      // 編集後の状態を保存 (既定のままなら書かない)
    recomputeParaxial();
}

// 面テーブル → 近軸諸元 (y-nu 追跡) と Merit の近軸オペランド値.
// 収差オペランド (SPHA/COMA/ASTI/DIST) は実光線追跡が要るため「—」のまま。
// 入射瞳径 / 視野半角 (入力欄が読めないときはモックの既定値)
double LensEditorTab::epdValue() const
{
    bool ok = false;
    const double v = m_epd->text().toDouble(&ok);
    return (ok && v > 0.0) ? v : 12.0;
}

double LensEditorTab::fieldValue() const
{
    bool ok = false;
    const double v = m_field->text().toDouble(&ok);
    return ok ? v : 20.0;
}

// 面テーブル → 近軸面の並び。
// 有効行のうち OBJ (無限遠物体) と IMG (像面) を除いたものが屈折面。
// 各行の「厚さ」は次の面までの距離なので、最後の面の厚さが像面までの距離。
std::vector<paraxial::Surface>
LensEditorTab::collectSurfaces(double *imageDistance,
                               QStringList *assumedGlass,
                               double lambda_um) const
{
    std::vector<paraxial::Surface> surfs;
    if (imageDistance) *imageDistance = -1.0;
    for (const LensSurface &r : m_rows) {
        if (!r.enabled) continue;
        if (r.type == QStringLiteral("OBJ")) continue;
        if (r.type == QStringLiteral("IMG")) {
            if (!surfs.empty() && imageDistance)
                *imageDistance = surfs.back().thickness;
            break;
        }
        paraxial::Surface s;
        bool rok = false;
        const double R = r.R.toDouble(&rok);
        s.R = rok ? R : 0.0;                       // "Infinity" → 平面 (R=0)
        bool tok = false;
        const double t = r.thick.toDouble(&tok);
        s.thickness = tok ? t : 0.0;
        s.nAfter = indexAfterAt(r.glass, lambda_um);
        if (assumedGlass && isUnknownGlass(r.glass)
            && !assumedGlass->contains(r.glass.trimmed()))
            *assumedGlass << r.glass.trimmed();
        bool dok = false;
        const double sd = r.semiD.toDouble(&dok);
        s.semiD = (dok && sd > 0) ? sd : 0.0;
        s.stop = (r.type == QStringLiteral("STO"));
        surfs.push_back(s);
    }
    return surfs;
}

void LensEditorTab::recomputeParaxial()
{
    const double epd = epdValue();
    const double field = fieldValue();

    double imageDistance = -1.0;
    QStringList unknownGlass;
    const std::vector<paraxial::Surface> surfs =
        collectSurfaces(&imageDistance, &unknownGlass);

    const paraxial::SystemData d =
        paraxial::analyze(surfs, imageDistance, epd, field);

    // 近軸諸元表
    auto roItem = [](const QString &t, bool rightAlign) {
        auto *it = new QTableWidgetItem(t);
        it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        if (rightAlign) it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        return it;
    };
    struct Line { QString name, value; };
    QVector<Line> lines;
    const QString dash = I18n::tr("lde_dash");
    auto mm = [](double v) {
        return QString::number(v, 'f', 3) + QString::fromUtf8(" mm");
    };
    if (d.valid) {
        lines.push_back({ I18n::tr("lde_px_efl"), mm(d.efl) });
        lines.push_back({ I18n::tr("lde_px_bfl"), mm(d.bfl) });
        lines.push_back({ I18n::tr("lde_px_ffl"), mm(d.ffl) });
        lines.push_back({ I18n::tr("lde_px_hp"),  mm(d.backPrincipal) });
        lines.push_back({ I18n::tr("lde_px_h"),   mm(d.frontPrincipal) });
        lines.push_back({ I18n::tr("lde_px_fno"),
                          (d.fnumber > 0)
                              ? QString::fromUtf8("F/")
                                    + QString::number(d.fnumber, 'f', 3)
                              : dash });
        lines.push_back({ I18n::tr("lde_px_imh"),
                          (field > 0) ? mm(d.imageHeight) : dash });
        lines.push_back({ I18n::tr("lde_px_track"), mm(d.totalTrack) });
        lines.push_back({ I18n::tr("lde_px_defocus"),
                          d.hasImagePlane ? mm(d.defocus) : dash });
    } else {
        lines.push_back({ I18n::tr("lde_px_invalid"), dash });
    }
    // 屈折率が仮定値の銘柄を明示する (計算値の出所を隠さない)
    if (!unknownGlass.isEmpty())
        lines.push_back({ I18n::tr("lde_px_assumed"),
                          unknownGlass.join(QStringLiteral(", ")) });
    m_paraxial->clearContents();
    m_paraxial->setRowCount(lines.size());
    for (int i = 0; i < lines.size(); ++i) {
        m_paraxial->setItem(i, 0, roItem(lines[i].name, false));
        m_paraxial->setItem(i, 1, roItem(lines[i].value, true));
    }

    // ── 3 次収差 (ザイデル和) ────────────────────────────────────────────
    // 近軸追跡だけで決まるので、ここで実計算できる (実光線追跡は不要)。
    // 単位は d 線 (587.6 nm) の波長で割った波面収差の峰値。
    const double lambda_mm = 587.6e-6;
    const seidel::Result sd = seidel::analyze(surfs, epd, field);
    const seidel::Waves wv = seidel::toWaves(sd, lambda_mm);
    {
        auto num = [](double v) { return QString::number(v, 'f', 4); };
        m_seidel->clearContents();
        const int nSurf = int(sd.perSurface.size());
        m_seidel->setRowCount(sd.valid ? nSurf + 3 : 1);
        if (!sd.valid) {
            m_seidel->setItem(0, 0, roItem(I18n::tr("lde_px_invalid"), false));
            for (int c = 1; c < 6; ++c)
                m_seidel->setItem(0, c, roItem(dash, true));
        } else {
            // 面ごとの寄与も波長単位へ揃える (総和と足し算が合うように)
            for (int i = 0; i < nSurf; ++i) {
                const seidel::SurfaceTerms &t = sd.perSurface[std::size_t(i)];
                m_seidel->setItem(i, 0, roItem(QString::number(i + 1), true));
                m_seidel->setItem(i, 1,
                                  roItem(num(t.sI / (8.0 * lambda_mm)), true));
                m_seidel->setItem(i, 2,
                                  roItem(num(t.sII / (2.0 * lambda_mm)), true));
                m_seidel->setItem(i, 3,
                                  roItem(num(t.sIII / (2.0 * lambda_mm)), true));
                m_seidel->setItem(i, 4,
                                  roItem(num(t.sIV / (4.0 * lambda_mm)), true));
                m_seidel->setItem(i, 5,
                                  roItem(num(t.sV / (2.0 * lambda_mm)), true));
            }
            m_seidel->setItem(nSurf, 0, roItem(I18n::tr("lde_sd_total"), false));
            m_seidel->setItem(nSurf, 1, roItem(num(wv.spherical), true));
            m_seidel->setItem(nSurf, 2, roItem(num(wv.coma), true));
            m_seidel->setItem(nSurf, 3, roItem(num(wv.astigmatism), true));
            m_seidel->setItem(nSurf, 4, roItem(num(wv.fieldCurv), true));
            m_seidel->setItem(nSurf, 5, roItem(num(wv.distortion), true));
            // ペッツバール半径とラグランジュ不変量 (どちらも系の量)
            m_seidel->setItem(nSurf + 1, 0,
                              roItem(I18n::tr("lde_sd_petz"), false));
            m_seidel->setItem(nSurf + 1, 1,
                              roItem(sd.hasPetzval ? mm(sd.petzvalRadius)
                                                   : dash, true));
            m_seidel->setItem(nSurf + 2, 0,
                              roItem(I18n::tr("lde_sd_lagr"), false));
            m_seidel->setItem(nSurf + 2, 1,
                              roItem(QString::number(sd.lagrange, 'f', 5),
                                     true));
            for (int r = nSurf + 1; r <= nSurf + 2; ++r)
                for (int c = 2; c < 6; ++c)
                    m_seidel->setItem(r, c, roItem(QString(), true));
            // 視野 0 のときは「なぜ 0 なのか」を出す (絶対規則 5 の趣旨)
            if (!sd.hasField)
                m_seidel->setItem(nSurf + 2, 2,
                                  roItem(I18n::tr("lde_sd_nofield"), false));
        }
    }

    // Merit の値列 (近軸オペランド + 3 次収差オペランド)
    const bool was = m_updating;
    m_updating = true;
    for (int i = 0; i < m_fom.size(); ++i) {
        QString value = dash;
        QString basis = I18n::tr("lde_b_needreal");
        if (wv.valid) {
            const QString code = m_fom[i].code;
            const bool isAberr =
                code == QStringLiteral("SPHA") || code == QStringLiteral("COMA")
                || code == QStringLiteral("ASTI")
                || code == QStringLiteral("DIST");
            // 視野が 0 だと球面収差以外は恒等的に 0。値ではなく理由を出す
            // (「実光線追跡が必要」という別の理由を出してはいけない)
            if (isAberr && !sd.hasField && code != QStringLiteral("SPHA")) {
                basis = I18n::tr("lde_b_nofield");
            } else if (isAberr) {
                const double v = (code == QStringLiteral("SPHA"))
                                     ? wv.spherical
                                     : (code == QStringLiteral("COMA"))
                                           ? wv.coma
                                           : (code == QStringLiteral("ASTI"))
                                                 ? wv.astigmatism
                                                 : wv.distortion;
                value = QString::number(v, 'f', 4);
                basis = I18n::tr("lde_b_seidel");
            }
        }
        if (d.valid) {
            if (m_fom[i].code == QStringLiteral("EFFL")) {
                value = QString::number(d.efl, 'f', 4);
                basis = I18n::tr("lde_b_paraxial");
            } else if (m_fom[i].code == QStringLiteral("PIMH")) {
                if (field > 0) {
                    value = QString::number(d.imageHeight, 'f', 4);
                    basis = I18n::tr("lde_b_paraxial");
                }
            } else if (m_fom[i].code == QStringLiteral("ISFN")) {
                if (d.fnumber > 0) {
                    value = QString::number(d.fnumber, 'f', 4);
                    basis = I18n::tr("lde_b_paraxial");
                }
            }
        }
        if (auto *it = m_merit->item(i, 4)) it->setText(value);
        if (auto *it = m_merit->item(i, 5)) it->setText(basis);
    }
    m_updating = was;
}

// ── スポットダイアグラム表示 (等倍散布図 + エアリー円) ─────────────────────
SpotDiagramView::SpotDiagramView(QWidget *parent) : QWidget(parent)
{
    setMinimumHeight(220);
}

void SpotDiagramView::setClouds(const QVector<Cloud> &clouds, double airyRadius)
{
    m_clouds = clouds;
    m_airy = airyRadius;
    update();
}

void SpotDiagramView::clear()
{
    m_clouds.clear();
    m_airy = 0.0;
    update();
}

void SpotDiagramView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), palette().base());
    p.setPen(QPen(palette().mid().color(), 1));
    p.drawRect(rect().adjusted(0, 0, -1, -1));
    if (m_clouds.isEmpty()) return;

    // 各点群は自分の重心を原点にして重ねる (視野が違うと像高が桁違いに
    // 離れるため、絶対位置で並べるとスポットの形が見えない)
    double half = 0.0;
    for (const Cloud &c : m_clouds)
        for (const QPointF &q : c.pts) {
            half = qMax(half, qAbs(q.x() - c.centroid.x()));
            half = qMax(half, qAbs(q.y() - c.centroid.y()));
        }
    half = qMax(half, m_airy);
    if (!(half > 0.0)) half = 1e-3;
    half *= 1.15;                       // 余白

    const int side = qMin(width(), height()) - 30;
    if (side <= 10) return;
    const QPointF org(width() * 0.5, height() * 0.5);
    const double scale = 0.5 * side / half;      // [px/mm]

    // 十字線
    p.setPen(QPen(palette().mid().color(), 1, Qt::DotLine));
    p.drawLine(QPointF(org.x() - 0.5 * side, org.y()),
               QPointF(org.x() + 0.5 * side, org.y()));
    p.drawLine(QPointF(org.x(), org.y() - 0.5 * side),
               QPointF(org.x(), org.y() + 0.5 * side));

    // エアリー円 (回折限界の目安)
    if (m_airy > 0.0) {
        p.setPen(QPen(QColor(0x88, 0x88, 0x88), 1, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        const double r = m_airy * scale;
        p.drawEllipse(org, r, r);
    }

    for (const Cloud &c : m_clouds) {
        p.setPen(Qt::NoPen);
        p.setBrush(c.color);
        for (const QPointF &q : c.pts) {
            const double dx = (q.x() - c.centroid.x()) * scale;
            const double dy = (q.y() - c.centroid.y()) * scale;
            p.drawEllipse(QPointF(org.x() + dx, org.y() - dy), 1.4, 1.4);
        }
    }

    // 目盛り (半幅を µm で書く)
    p.setPen(palette().text().color());
    QFont f = p.font();
    f.setPointSizeF(qMax(7.0, f.pointSizeF() - 1.0));
    p.setFont(f);
    p.drawText(rect().adjusted(6, 4, -6, -4), Qt::AlignTop | Qt::AlignLeft,
               QStringLiteral("±%1 µm").arg(QString::number(half * 1000.0, 'g', 3)));
    int row = 0;
    for (const Cloud &c : m_clouds) {
        p.setPen(c.color);
        p.drawText(rect().adjusted(6, 4 + 14 * (row + 1), -6, -4),
                   Qt::AlignTop | Qt::AlignLeft, c.label);
        ++row;
    }
}

// 波長 [nm] → 表示色 (可視域のおおよその色。判別のための着色で、
// 測色的な正しさは求めていない)
static QColor wavelengthColor(double nm)
{
    if (nm < 440.0) return QColor(0x7A, 0x33, 0xB8);   // 紫
    if (nm < 490.0) return QColor(0x1F, 0x5F, 0xD0);   // 青
    if (nm < 510.0) return QColor(0x00, 0x99, 0xA8);   // 青緑
    if (nm < 560.0) return QColor(0x2E, 0x8B, 0x57);   // 緑
    if (nm < 590.0) return QColor(0xB8, 0x32, 0x80);   // 主波長帯 (強調色)
    if (nm < 640.0) return QColor(0xD2, 0x69, 0x1E);   // 橙
    if (nm < 780.0) return QColor(0xC0, 0x30, 0x30);   // 赤
    return QColor(0x88, 0x44, 0x44);                   // 近赤外
}

// 像面が近軸焦点からずれているときの注記。ずれによるボケ (|defocus|/(2F))
// がエアリー半径を超えていれば、結果は収差ではなく離焦で決まっている。
static QString defocusNote(const paraxial::SystemData &d, double airy)
{
    if (!d.valid || !d.hasImagePlane) return QString();
    const double blur = (d.fnumber > 0.0)
                            ? std::fabs(d.defocus) / (2.0 * d.fnumber) : 0.0;
    const double limit = (airy > 0.0) ? airy : 1e-3;
    if (!(blur > limit)) return QString();
    return QStringLiteral("\n") + I18n::tr("lde_an_defocus")
                                       .arg(QString::number(d.defocus, 'f', 3));
}

// ── 解析プロット (実光線追跡) ──────────────────────────────────────────────
// 面テーブルの系をそのまま optics/RayTrace へ渡す。物体は無限遠、波長は
// d 線 1 本 (色収差は含まない — その旨を注記に出す)。
void LensEditorTab::runSpotDiagram()
{
    const double epd = epdValue();
    const double field = fieldValue();
    if (m_waves.isEmpty()) m_waves = { 587.6 };
    double primary = m_waves.first();
    for (double w : m_waves)
        if (std::fabs(w - 587.6) < std::fabs(primary - 587.6)) primary = w;

    // 波長ごとに屈折率を引き直して系を作る
    auto systemAt = [&](double nm, double *imgDist) {
        double id = -1.0;
        const std::vector<paraxial::Surface> surfs =
            collectSurfaces(&id, nullptr, nm * 1e-3);
        raytrace::System sys;
        sys.surfaces = raytrace::fromParaxial(surfs);
        sys.imageDistance = (id >= 0.0)
                                ? id
                                : (surfs.empty() ? 0.0 : surfs.back().thickness);
        if (imgDist) *imgDist = id;
        return sys;
    };

    double imageDistance = -1.0;
    const raytrace::System primarySys = systemAt(primary, &imageDistance);
    const raytrace::SpotResult ref0 =
        raytrace::spotDiagram(primarySys, epd, 0.0, 8);
    if (!ref0.valid) {
        m_spotView->clear();
        m_spotView->setVisible(false);
        m_fanPlot->setVisible(false);
        m_anInfo->setText(I18n::tr("lde_an_fail"));
        return;
    }
    const bool hasField = (field != 0.0);
    const raytrace::SpotResult refF =
        hasField ? raytrace::spotDiagram(primarySys, epd, field, 8) : ref0;

    // 軸上と視野端をそれぞれ「主波長の重心」を基準にして重ねる。
    // 波長ごとに自分の重心へ寄せると **色ずれそのものが消える**ので、
    // 同じ視野の中では基準を 1 つに揃える。
    QVector<SpotDiagramView::Cloud> clouds;
    int traced = 0, failed = 0;
    double polyAx2 = 0.0, polyFl2 = 0.0;
    int polyAxN = 0, polyFlN = 0;
    for (int f = 0; f < (hasField ? 2 : 1); ++f) {
        const double fld = (f == 0) ? 0.0 : field;
        const raytrace::SpotResult &ref = (f == 0) ? ref0 : refF;
        if (!ref.valid) continue;
        for (double nm : m_waves) {
            const raytrace::System sys = systemAt(nm, nullptr);
            const raytrace::SpotResult r =
                raytrace::spotDiagram(sys, epd, fld, 8);
            traced += r.traced;
            failed += r.failed;
            if (!r.valid) continue;
            SpotDiagramView::Cloud c;
            c.color = wavelengthColor(nm);
            c.centroid = QPointF(ref.centroidX, ref.centroidY);
            c.label = QString::number(nm, 'f', 1) + QStringLiteral(" nm ")
                      + ((f == 0) ? I18n::tr("lde_an_onaxis")
                                  : I18n::tr("lde_an_atfield"));
            for (int i = 0; i < r.traced; ++i) {
                c.pts.push_back(QPointF(r.x[i], r.y[i]));
                const double dx = r.x[i] - ref.centroidX;
                const double dy = r.y[i] - ref.centroidY;
                if (f == 0) { polyAx2 += dx * dx + dy * dy; ++polyAxN; }
                else        { polyFl2 += dx * dx + dy * dy; ++polyFlN; }
            }
            clouds.push_back(c);
        }
    }
    if (clouds.isEmpty()) {
        m_anInfo->setText(I18n::tr("lde_an_fail"));
        return;
    }

    // エアリー半径 1.22·λ·F/# (回折限界の目安 — 主波長で出す)
    const std::vector<paraxial::Surface> pxs =
        collectSurfaces(&imageDistance, nullptr, primary * 1e-3);
    const paraxial::SystemData d =
        paraxial::analyze(pxs, imageDistance, epd, field);
    const double airy = (d.valid && d.fnumber > 0.0)
                            ? 1.22 * primary * 1e-6 * d.fnumber : 0.0;
    m_spotView->setClouds(clouds, airy);
    m_spotView->setVisible(true);
    m_fanPlot->setVisible(false);

    const auto um = [](double mm) { return QString::number(mm * 1000.0, 'f', 2); };
    QString info = I18n::tr("lde_an_spot")
                       .arg(um(ref0.rmsRadius), um(ref0.geoRadius),
                            QString::number(field, 'g', 4),
                            um(refF.valid ? refF.rmsRadius : 0.0),
                            um(refF.valid ? refF.geoRadius : 0.0),
                            QString::number(traced),
                            QString::number(failed));
    if (m_waves.size() > 1 && polyAxN > 0)
        info += QStringLiteral("\n")
                + I18n::tr("lde_an_poly")
                      .arg(QString::number(m_waves.size()),
                           um(std::sqrt(polyAx2 / polyAxN)),
                           (polyFlN > 0) ? um(std::sqrt(polyFl2 / polyFlN))
                                         : QStringLiteral("—"));
    if (airy > 0.0)
        info += QStringLiteral("\n") + I18n::tr("lde_an_airy")
                                           .arg(um(airy),
                                                QString::number(d.fnumber, 'f', 2));
    info += defocusNote(d, airy);
    m_anInfo->setText(info);
}

void LensEditorTab::runRayFan()
{
    const double epd = epdValue();
    const double field = fieldValue();
    // レイファンは主波長 1 本で描く (波長を重ねると子午/球欠と混ざって
    // 読めなくなる。色収差は「色収差」ボタンとスポットで見る)
    double primary = -1.0;
    for (double w : m_waves)
        if (primary < 0.0
            || std::fabs(w - 587.6) < std::fabs(primary - 587.6)) primary = w;
    double imageDistance = -1.0;
    const std::vector<paraxial::Surface> surfs =
        collectSurfaces(&imageDistance, nullptr,
                        (primary > 0.0) ? primary * 1e-3 : 0.0);

    raytrace::System sys;
    sys.surfaces = raytrace::fromParaxial(surfs);
    sys.imageDistance = (imageDistance >= 0.0)
                            ? imageDistance
                            : (surfs.empty() ? 0.0 : surfs.back().thickness);

    const raytrace::FanResult axis = raytrace::rayFan(sys, epd, 0.0, 24);
    const raytrace::FanResult edge =
        (field != 0.0) ? raytrace::rayFan(sys, epd, field, 24) : axis;
    if (!axis.valid) {
        m_spotView->setVisible(false);
        m_fanPlot->setVisible(false);
        m_anInfo->setText(I18n::tr("lde_an_fail"));
        return;
    }

    // 横収差は µm で描く (mm だと数値が潰れる)
    auto series = [](const std::vector<raytrace::FanPoint> &pts, bool useY,
                     const QColor &col, bool dashed, const QString &label,
                     double *peak) {
        MiniSeries s;
        s.color = col;
        s.dashed = dashed;
        s.label = label;
        for (const raytrace::FanPoint &q : pts) {
            if (!q.ok) continue;
            const double v = (useY ? q.dy : q.dx) * 1000.0;
            s.pts.push_back(QPointF(q.pupil, v));
            if (peak) *peak = qMax(*peak, qAbs(v));
        }
        return s;
    };
    double pAxis = 0.0, pTan = 0.0, pSag = 0.0;
    QVector<MiniSeries> all;
    all.push_back(series(axis.tangential, true, QColor(0xB8, 0x32, 0x80), false,
                         I18n::tr("lde_an_tan_ax"), &pAxis));
    all.push_back(series(axis.sagittal, false, QColor(0xB8, 0x32, 0x80), true,
                         I18n::tr("lde_an_sag_ax"), &pAxis));
    if (field != 0.0 && edge.valid) {
        all.push_back(series(edge.tangential, true, QColor(0x00, 0x78, 0xD4),
                             false, I18n::tr("lde_an_tan_fld"), &pTan));
        all.push_back(series(edge.sagittal, false, QColor(0x00, 0x78, 0xD4),
                             true, I18n::tr("lde_an_sag_fld"), &pSag));
    }
    m_fanPlot->setSeries(all);
    m_fanPlot->setVisible(true);
    m_spotView->setVisible(false);

    const auto num = [](double v) { return QString::number(v, 'f', 2); };
    const paraxial::SystemData d =
        paraxial::analyze(surfs, imageDistance, epd, field);
    const double airy = (d.valid && d.fnumber > 0.0)
                            ? 1.22 * 587.6e-6 * d.fnumber : 0.0;
    m_anInfo->setText(I18n::tr("lde_an_fan")
                          .arg(num(pAxis), QString::number(field, 'g', 4),
                               num(pTan), num(pSag))
                      + defocusNote(d, airy));
}

// ── 波長サンプル ──────────────────────────────────────────────────────────
// バッジは押すと消せる (最後の 1 本は残す)。d 線に最も近いものを強調表示。
void LensEditorTab::rebuildWaveBadges()
{
    if (!m_waveBadges) return;
    auto *box = qobject_cast<QHBoxLayout *>(m_waveBadges->layout());
    if (!box) return;
    while (QLayoutItem *it = box->takeAt(0)) {
        if (QWidget *w = it->widget()) w->deleteLater();
        delete it;
    }
    std::sort(m_waves.begin(), m_waves.end());
    // d 線に最も近い 1 本を主波長として強調する
    int primary = 0;
    for (int i = 1; i < m_waves.size(); ++i)
        if (std::fabs(m_waves[i] - 587.6) < std::fabs(m_waves[primary] - 587.6))
            primary = i;
    for (int i = 0; i < m_waves.size(); ++i) {
        const double nm = m_waves[i];
        QString label = QString::number(nm, 'f', 1) + QStringLiteral("nm");
        // 標準線には記号を添える (F/d/C は色消しの基準)
        if (std::fabs(nm - 486.1) < 0.2) label += QStringLiteral(" (F)");
        else if (std::fabs(nm - 587.6) < 0.2) label += QStringLiteral(" (d)");
        else if (std::fabs(nm - 656.3) < 0.2) label += QStringLiteral(" (C)");
        auto *b = new QPushButton(label, m_waveBadges);
        b->setFlat(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setToolTip(I18n::tr("lde_wave_tip"));
        b->setFixedHeight(20);
        QString ss = QStringLiteral("border-radius:8px; padding:1px 7px; "
                                    "font-size:11px;");
        ss += (i == primary)
                  ? QStringLiteral("background:#B83280; color:white; "
                                   "border:1px solid transparent;")
                  : QStringLiteral("border:1px solid palette(mid);");
        b->setStyleSheet(ss);
        connect(b, &QPushButton::clicked, this, [this, nm] {
            if (m_waves.size() <= 1) return;
            m_waves.removeAll(nm);
            rebuildWaveBadges();
        });
        box->addWidget(b);
    }
}

void LensEditorTab::addWavelength()
{
    bool ok = false;
    const double nm = QInputDialog::getDouble(
        this, I18n::tr("lde_wave_add_title"), I18n::tr("lde_wave_add_label"),
        550.0, 200.0, 20000.0, 1, &ok);
    if (!ok) return;
    for (double w : m_waves)
        if (std::fabs(w - nm) < 0.05) {
            QMessageBox::information(this, I18n::tr("lde_wave_add_title"),
                                     I18n::tr("lde_wave_dup"));
            return;
        }
    m_waves.push_back(nm);
    rebuildWaveBadges();
}

// ── 色収差 (波長ごとの焦点移動) ────────────────────────────────────────────
// 各波長で面テーブルの屈折率を引き直して近軸追跡し、バックフォーカスの
// 主波長からのずれを描く。薄レンズなら C 線と F 線の差は f/V (アッベ数の
// 定義そのもの) になる — selftest でその恒等式を検証している。
// ── 最適化 (optics/DampedLeastSquares) ─────────────────────────────────────
// 曲率半径が有限の面をすべて変数にして、目標の有効焦点距離 f' に合わせる。
// **結果は自動で書き戻さない** — 提案値を出すだけにして、「適用」で初めて
// 面テーブルへ入れる (利用者の設計値を黙って書き換えない)。
void LensEditorTab::runOptimize()
{
    m_optSolution.clear();
    m_optApply->setEnabled(false);

    bool okT = false;
    const double target = m_optTarget->text().trimmed().toDouble(&okT);
    if (!okT || !(target > 0.0)) { m_optInfo->setText(I18n::tr("lde_opt_none"));
                                   return; }

    // 変数にする行 (曲率が有限の面) を拾う
    QVector<int> varRows;
    QVector<double> x0;
    for (int i = 0; i < m_rows.size(); ++i) {
        const QString r = m_rows[i].R.trimmed();
        bool okR = false;
        const double v = r.toDouble(&okR);
        if (!m_rows[i].enabled || !okR || !(std::fabs(v) > 0.0)) continue;
        varRows.push_back(i);
        x0.push_back(v);
    }
    if (varRows.isEmpty()) { m_optInfo->setText(I18n::tr("lde_opt_none"));
                             return; }

    const double epd = epdValue();
    const double field = fieldValue();
    // 主波長は d 線に最も近いもの — このタブの他の解析と同じ選び方にする
    // (最初の波長を使うと F 線が選ばれ、Merit 表の EFFL と食い違う)
    double lam = m_waves.isEmpty() ? 587.6 : m_waves.first();
    for (double w : m_waves)
        if (std::fabs(w - 587.6) < std::fabs(lam - 587.6)) lam = w;

    // 変数 → 系 → 残差 (目標 f' との差)。**元の m_rows は触らない**
    const QVector<LensSurfaceRow> saved = m_rows;
    const auto eflOf = [&](const std::vector<double> &v, double *efl) {
        LensEditorTab *self = const_cast<LensEditorTab *>(this);
        for (int k = 0; k < varRows.size(); ++k)
            self->m_rows[varRows[k]].R = QString::number(v[k], 'g', 12);
        double imageDistance = -1.0;
        const std::vector<paraxial::Surface> surfs =
            self->collectSurfaces(&imageDistance, nullptr, lam * 1e-3);
        const paraxial::SystemData d =
            paraxial::analyze(surfs, imageDistance, epd, field);
        self->m_rows = saved;              // 必ず戻す
        if (!d.valid || !(std::fabs(d.efl) > 0.0)) return false;
        *efl = d.efl;
        return true;
    };

    double efl0 = 0.0;
    const bool haveStart = eflOf(std::vector<double>(x0.begin(), x0.end()),
                                 &efl0);

    optics::DlsOptions o;
    o.maxIterations = 60;
    o.step.assign(x0.size(), 1.0e-3);      // 曲率半径 [mm] の差分刻み
    const optics::ResidualFn fn = [&](const std::vector<double> &v,
                                      std::vector<double> &r) {
        double efl = 0.0;
        if (!eflOf(v, &efl)) return false;
        r.assign(1, efl - target);
        return true;
    };
    const optics::DlsResult res =
        optics::solve(fn, std::vector<double>(x0.begin(), x0.end()), o);
    if (!res.ok) { m_optInfo->setText(I18n::tr("lde_opt_none")); return; }

    double efl1 = 0.0;
    eflOf(res.x, &efl1);

    QString note = I18n::tr("lde_opt_res")
                       .arg(varRows.size())
                       .arg(QString::number(target, 'f', 3),
                            QString::number(haveStart ? efl0 : 0.0, 'f', 3),
                            QString::number(efl1, 'f', 3),
                            QString::number(res.rms, 'g', 3))
                       .arg(res.iterations).arg(res.evaluations)
                       .arg(QString());
    // 目標に届いたときだけ「適用」を押せるようにする
    const bool reached = (res.rms < 1.0e-3);
    if (reached) {
        m_optSolution.clear();
        for (int k = 0; k < varRows.size(); ++k)
            m_optSolution.push_back({ varRows[k], res.x[k] });
        m_optApply->setEnabled(true);
        note += I18n::tr("lde_opt_apply_hint");
    } else {
        note += I18n::tr("lde_opt_miss").arg(QString::number(res.rms, 'g', 3));
    }
    m_optInfo->setText(note);
}

// 提案値を面テーブルへ書き戻す (押されて初めて実行する)
void LensEditorTab::applyOptimize()
{
    if (m_optSolution.isEmpty()) return;
    for (const auto &pr : m_optSolution) {
        if (pr.first < 0 || pr.first >= m_rows.size()) continue;
        m_rows[pr.first].R = QString::number(pr.second, 'f', 4);
    }
    m_optSolution.clear();
    m_optApply->setEnabled(false);
    rebuildTable();
    pushRowsToProject();
    m_optInfo->setText(I18n::tr("lde_opt_applied"));
}

// ── 像面湾曲 (optics/FieldCurvature) ───────────────────────────────────────
// 視野ごとに、瞳の上下 2 本 (タンジェンシャル) と左右 2 本 (サジタル) が
// 交わる位置を求める。**像空間では光線が直線**なので、像面を 2 枚置いた
// 外挿で厳密に解ける (焦点探索は要らない)。
void LensEditorTab::runFieldCurvature()
{
    const double epd = epdValue();
    const double field = fieldValue();
    if (m_waves.isEmpty()) m_waves = { 587.6 };
    double primary = m_waves.first();
    for (double w : m_waves)
        if (std::fabs(w - 587.6) < std::fabs(primary - 587.6)) primary = w;

    double imageDistance = -1.0;
    const std::vector<paraxial::Surface> surfs =
        collectSurfaces(&imageDistance, nullptr, primary * 1e-3);
    raytrace::System base;
    base.surfaces = raytrace::fromParaxial(surfs);
    base.imageDistance = (imageDistance >= 0.0)
                             ? imageDistance
                             : (surfs.empty() ? 0.0 : surfs.back().thickness);
    if (!(field > 0.0) || base.surfaces.empty()) {
        m_fanPlot->setVisible(false);
        m_spotView->setVisible(false);
        m_anInfo->setText(I18n::tr("lde_fc_bad"));
        return;
    }

    // 追跡役 — 像面を dz ずらして上下 / 左右の 4 本を返す
    struct Ctx { const raytrace::System *sys; double epd; };
    Ctx ctx{ &base, epd };
    const auto tracer = [](double th, double dz, double out[4],
                           void *user) -> bool {
        Ctx *c = static_cast<Ctx *>(user);
        raytrace::System s = *c->sys;
        s.imageDistance += dz;
        const raytrace::RayResult up = raytrace::traceRay(s, c->epd, th, 0.0,  0.9);
        const raytrace::RayResult dn = raytrace::traceRay(s, c->epd, th, 0.0, -0.9);
        const raytrace::RayResult lf = raytrace::traceRay(s, c->epd, th, -0.9, 0.0);
        const raytrace::RayResult rt = raytrace::traceRay(s, c->epd, th,  0.9, 0.0);
        if (!up.ok() || !dn.ok() || !lf.ok() || !rt.ok()) return false;
        out[0] = up.y; out[1] = dn.y;      // タンジェンシャル (y で解く)
        out[2] = lf.x; out[3] = rt.x;      // サジタル (x で解く)
        return true;
    };

    // 2 枚目の像面のずらし量は答えに影響しない (外挿が厳密なため)。
    // 数値の桁が落ちない程度に取る。
    const double dz = 1.0;
    const optics::FieldCurvatureResult fc =
        optics::fieldCurvature(tracer, &ctx, field, 21, dz);
    if (!fc.valid()) {
        m_fanPlot->setVisible(false);
        m_spotView->setVisible(false);
        m_anInfo->setText(I18n::tr("lde_fc_bad"));
        return;
    }

    MiniSeries sag, tan;
    sag.color = QColor("#0078D4");
    sag.label = "sagittal";
    tan.color = QColor("#E8A33D");
    tan.label = "tangential";
    for (const optics::FieldCurvaturePoint &p : fc.points) {
        // 横軸をずれ、縦軸を視野にする (レンズ設計の慣用)
        if (p.sagittalOk)   sag.pts.push_back(QPointF(p.sagittal_mm, p.field_deg));
        if (p.tangentialOk) tan.pts.push_back(QPointF(p.tangential_mm, p.field_deg));
    }
    if (sag.pts.isEmpty() && tan.pts.isEmpty()) {
        m_fanPlot->setVisible(false);
        m_anInfo->setText(I18n::tr("lde_fc_bad"));
        return;
    }
    m_fanPlot->setSeries({ sag, tan });
    m_fanPlot->setLabels(I18n::tr("lde_fc_x"), I18n::tr("lde_fc_y"));
    m_fanPlot->setVisible(true);
    m_spotView->setVisible(false);

    const optics::FieldCurvaturePoint &edge = fc.points.back();
    QString cut;
    if (edge.field_deg < field - 1e-9)
        cut = I18n::tr("lde_fc_cut")
                  .arg(QString::number(field, 'f', 1),
                       QString::number(edge.field_deg, 'f', 1));
    m_anInfo->setText(I18n::tr("lde_fc_res")
                          .arg(QString::number(edge.field_deg, 'f', 1),
                               QString::number(edge.sagittal_mm, 'f', 4),
                               QString::number(edge.tangential_mm, 'f', 4),
                               QString::number(fc.maxAstigmatism_mm, 'f', 4))
                      + I18n::tr("lde_fc_note") + cut);
}

// ── 歪曲格子 (optics/DistortionGrid) ───────────────────────────────────────
// 主光線 (瞳中心 px = py = 0) を視野角ごとに追跡し、近軸の理想像高
// y = f'·tanθ と比べる。**歪曲は主光線の当たる位置だけで決まる**ので、
// スポットのぼけとは別の量。
void LensEditorTab::runDistortion()
{
    const double epd = epdValue();
    const double field = fieldValue();
    if (m_waves.isEmpty()) m_waves = { 587.6 };
    double primary = m_waves.first();
    for (double w : m_waves)
        if (std::fabs(w - 587.6) < std::fabs(primary - 587.6)) primary = w;

    double imageDistance = -1.0;
    const std::vector<paraxial::Surface> surfs =
        collectSurfaces(&imageDistance, nullptr, primary * 1e-3);
    const paraxial::SystemData pd =
        paraxial::analyze(surfs, imageDistance, epd, field);
    raytrace::System sys;
    sys.surfaces = raytrace::fromParaxial(surfs);
    sys.imageDistance = (imageDistance >= 0.0)
                            ? imageDistance
                            : (surfs.empty() ? 0.0 : surfs.back().thickness);

    if (!pd.valid || !(pd.efl > 0.0) || !(field > 0.0)) {
        m_fanPlot->setVisible(false);
        m_spotView->setVisible(false);
        m_anInfo->setText(I18n::tr("lde_dg_bad"));
        return;
    }

    // 視野半角 → 主光線の像高。追跡に失敗したら理想値を返さず 0 を返す
    // (失敗を「歪曲 −100 %」として描かないよう、下で本数を数えて弾く)
    int failed = 0;
    const optics::FieldMapping map = [&](double th) -> double {
        const raytrace::RayResult r = raytrace::traceRay(sys, epd, th, 0.0, 0.0);
        if (!r.ok()) { ++failed; return 0.0; }
        return std::hypot(r.x, r.y);
    };
    const optics::DistortionGridResult g =
        optics::distortionGrid(map, pd.efl, field, 9);
    if (!g.valid() || failed > 0) {
        m_fanPlot->setVisible(false);
        m_spotView->setVisible(false);
        m_anInfo->setText(I18n::tr("lde_dg_bad"));
        return;
    }

    // 行と列を折れ線にして描く (理想は薄く、実際は濃く)
    QVector<MiniSeries> series;
    for (int pass = 0; pass < 2; ++pass) {
        const bool ideal = (pass == 0);
        for (int k = 0; k < g.n; ++k) {
            MiniSeries row, col;
            row.color = ideal ? QColor(136, 153, 170, 110) : QColor("#0078D4");
            col.color = row.color;
            for (int i = 0; i < g.n; ++i) {
                const optics::DistortionNode &a = g.nodes[k * g.n + i];
                const optics::DistortionNode &b = g.nodes[i * g.n + k];
                row.pts.push_back(QPointF(ideal ? a.xIdeal_mm : a.xReal_mm,
                                          ideal ? a.yIdeal_mm : a.yReal_mm));
                col.pts.push_back(QPointF(ideal ? b.xIdeal_mm : b.xReal_mm,
                                          ideal ? b.yIdeal_mm : b.yReal_mm));
            }
            series.push_back(row);
            series.push_back(col);
        }
    }
    m_fanPlot->setSeries(series);
    m_fanPlot->setLabels(I18n::tr("lde_dg_x"), I18n::tr("lde_dg_y"));
    m_fanPlot->setVisible(true);
    m_spotView->setVisible(false);

    m_anInfo->setText(I18n::tr("lde_dg_res")
                          .arg(QString::number(field, 'f', 1),
                               QString::number(g.cornerPercent, 'f', 3),
                               QString::number(g.maxPercent, 'f', 3),
                               QString::number(pd.efl, 'f', 3))
                      + I18n::tr("lde_dg_note"));
}

// ── 包絡エネルギー (optics/EncircledEnergy) ────────────────────────────────
// MTF と同じ実光線追跡の交点を使う。中心は重心 (どこから測ったかを画面に
// 書く — 主光線基準とは値が変わるため)。
void LensEditorTab::runEncircled()
{
    const double epd = epdValue();
    const double field = fieldValue();
    if (m_waves.isEmpty()) m_waves = { 587.6 };
    double primary = m_waves.first();
    for (double w : m_waves)
        if (std::fabs(w - 587.6) < std::fabs(primary - 587.6)) primary = w;

    double imageDistance = -1.0;
    const std::vector<paraxial::Surface> surfs =
        collectSurfaces(&imageDistance, nullptr, primary * 1e-3);
    raytrace::System sys;
    sys.surfaces = raytrace::fromParaxial(surfs);
    sys.imageDistance = (imageDistance >= 0.0)
                            ? imageDistance
                            : (surfs.empty() ? 0.0 : surfs.back().thickness);
    const raytrace::SpotResult sp = raytrace::spotDiagram(sys, epd, field, 12);
    if (!sp.valid || sp.x.empty()) {
        m_fanPlot->setVisible(false);
        m_spotView->setVisible(false);
        m_anInfo->setText(I18n::tr("lde_an_fail"));
        return;
    }

    const optics::EeCurve ee =
        optics::encircledEnergy(sp.x, sp.y, sp.centroidX, sp.centroidY, 128);
    if (!ee.valid()) {
        m_fanPlot->setVisible(false);
        m_anInfo->setText(I18n::tr("lde_an_fail"));
        return;
    }

    MiniSeries s;
    s.color = QColor("#0078D4");
    s.label = "encircled energy";
    for (std::size_t i = 0; i < ee.radius_mm.size(); ++i)
        s.pts.push_back(QPointF(ee.radius_mm[i], ee.fraction[i]));
    m_fanPlot->setYRange(0.0, 1.05);
    m_fanPlot->setSeries({ s });
    m_fanPlot->setLabels(I18n::tr("lde_ee_x"), I18n::tr("lde_ee_y"));
    m_fanPlot->setVisible(true);
    m_spotView->setVisible(false);

    auto rad = [&](double f) {
        return optics::radiusForFraction(sp.x, sp.y, sp.centroidX,
                                         sp.centroidY, f);
    };
    m_anInfo->setText(I18n::tr("lde_ee_res")
                          .arg(ee.rays)
                          .arg(QString::number(ee.rmsRadius_mm, 'f', 4),
                               QString::number(rad(0.5), 'f', 4),
                               QString::number(rad(0.8), 'f', 4),
                               QString::number(rad(0.9), 'f', 4),
                               QString::number(ee.maxRadius_mm, 'f', 4))
                      + I18n::tr("lde_ee_note"));
}

// ── MTF (optics/Mtf) ───────────────────────────────────────────────────────
// 回折限界 (円形瞳の自己相関の閉形式) と、実光線追跡の像面交点から出した
// 幾何 MTF を**2 本並べて**描く。幾何は回折を含まないので高い周波数では
// 実際より良く出る — そのことを必ず画面に書く。
void LensEditorTab::runMtf()
{
    const double epd = epdValue();
    const double field = fieldValue();
    if (m_waves.isEmpty()) m_waves = { 587.6 };
    double primary = m_waves.first();
    for (double w : m_waves)
        if (std::fabs(w - 587.6) < std::fabs(primary - 587.6)) primary = w;

    // 近軸の F 値 (回折限界のカットオフを決める)
    double imageDistance = -1.0;
    const std::vector<paraxial::Surface> surfs =
        collectSurfaces(&imageDistance, nullptr, primary * 1e-3);
    const paraxial::SystemData d =
        paraxial::analyze(surfs, imageDistance, epd, field);

    // 実光線追跡で像面交点を得る (スポットダイアグラムと同じ経路)
    raytrace::System sys;
    sys.surfaces = raytrace::fromParaxial(surfs);
    sys.imageDistance = (imageDistance >= 0.0)
                            ? imageDistance
                            : (surfs.empty() ? 0.0 : surfs.back().thickness);
    const raytrace::SpotResult sp = raytrace::spotDiagram(sys, epd, field, 12);

    if (!d.valid || !(d.fnumber > 0.0) || !sp.valid || sp.x.empty()) {
        m_fanPlot->setVisible(false);
        m_spotView->setVisible(false);
        m_anInfo->setText(I18n::tr("lde_mtf_bad"));
        return;
    }

    // 断面は接線方向 (x) を採る。重心を引いておく (平行移動は MTF を
    // 変えないが、数値の桁を落とさないため)
    std::vector<double> xs;
    xs.reserve(sp.x.size());
    for (double v : sp.x) xs.push_back(v - sp.centroidX);

    const optics::MtfCurve mc =
        optics::mtfCurve(xs, primary, d.fnumber, 128);
    if (!mc.valid()) {
        m_fanPlot->setVisible(false);
        m_anInfo->setText(I18n::tr("lde_mtf_bad"));
        return;
    }

    MiniSeries diff;
    diff.color = QColor("#8899AA");
    diff.label = "diffraction limit";
    MiniSeries geo;
    geo.color = QColor("#0078D4");
    geo.label = "geometric";
    for (std::size_t i = 0; i < mc.nu.size(); ++i) {
        diff.pts.push_back(QPointF(mc.nu[i], mc.diffraction[i]));
        if (i < mc.geometric.size())
            geo.pts.push_back(QPointF(mc.nu[i], mc.geometric[i]));
    }
    m_fanPlot->setYRange(0.0, 1.05);
    m_fanPlot->setSeries({ diff, geo });
    m_fanPlot->setLabels(I18n::tr("lde_mtf_x"), I18n::tr("lde_mtf_y"));
    m_fanPlot->setVisible(true);
    m_spotView->setVisible(false);

    const double nc = optics::mtfCutoff_cyc_per_mm(primary, d.fnumber);
    const double f50d = optics::frequencyAtMtf(mc.nu, mc.diffraction, 0.5);
    const double f50g = optics::frequencyAtMtf(mc.nu, mc.geometric, 0.5);
    m_anInfo->setText(I18n::tr("lde_mtf_res")
                          .arg(QString::number(primary, 'f', 1),
                               QString::number(d.fnumber, 'f', 2),
                               QString::number(nc, 'f', 1),
                               QString::number(f50d, 'f', 1),
                               QString::number(f50g, 'f', 1))
                          .arg(int(sp.x.size()))
                          .arg(QString::number(sp.rmsRadius, 'f', 4))
                      + I18n::tr("lde_mtf_note"));
}

void LensEditorTab::runChromatic()
{
    if (m_waves.isEmpty()) return;
    const double epd = epdValue();
    const double field = fieldValue();

    // 主波長 = d 線に最も近いもの
    double primary = m_waves.first();
    for (double w : m_waves)
        if (std::fabs(w - 587.6) < std::fabs(primary - 587.6)) primary = w;

    auto bflAt = [&](double nm) {
        double imageDistance = -1.0;
        const std::vector<paraxial::Surface> s =
            collectSurfaces(&imageDistance, nullptr, nm * 1e-3);
        const paraxial::SystemData d =
            paraxial::analyze(s, imageDistance, epd, field);
        return d.valid ? d.bfl : std::numeric_limits<double>::quiet_NaN();
    };

    const double ref = bflAt(primary);
    if (!std::isfinite(ref)) {
        m_spotView->setVisible(false);
        m_fanPlot->setVisible(false);
        m_anInfo->setText(I18n::tr("lde_an_fail"));
        return;
    }

    // 連続曲線 (選んだ波長の範囲を少し広げて掃引) + 選んだ波長の点
    double lo = m_waves.first(), hi = m_waves.last();
    for (double w : m_waves) { lo = qMin(lo, w); hi = qMax(hi, w); }
    if (hi - lo < 1.0) { lo -= 50.0; hi += 50.0; }
    else { const double m = 0.08 * (hi - lo); lo -= m; hi += m; }
    if (lo < 200.0) lo = 200.0;

    MiniSeries curve;
    curve.color = QColor(0x00, 0x78, 0xD4);
    curve.label = I18n::tr("lde_an_focshift");
    const int n = 81;
    for (int i = 0; i < n; ++i) {
        const double nm = lo + (hi - lo) * double(i) / double(n - 1);
        const double v = bflAt(nm);
        if (std::isfinite(v)) curve.pts.push_back(QPointF(nm, (v - ref) * 1000.0));
    }
    MiniSeries marks;
    marks.color = QColor(0xB8, 0x32, 0x80);
    marks.markers = true;
    marks.label = I18n::tr("lde_waves");
    double worst = 0.0;
    for (double w : m_waves) {
        const double v = bflAt(w);
        if (!std::isfinite(v)) continue;
        marks.pts.push_back(QPointF(w, (v - ref) * 1000.0));
        worst = qMax(worst, std::fabs(v - ref));
    }
    if (curve.pts.isEmpty()) {
        m_anInfo->setText(I18n::tr("lde_an_fail"));
        return;
    }
    m_fanPlot->setSeries({ curve, marks });
    m_fanPlot->setLabels(I18n::tr("lde_an_xwave"), I18n::tr("lde_an_yshift"));
    m_fanPlot->setVisible(true);
    m_spotView->setVisible(false);

    // F 線と C 線が入っていれば軸上色収差 (LCA) をそのまま出す
    QString info = I18n::tr("lde_an_chrom")
                       .arg(QString::number(primary, 'f', 1),
                            QString::number(worst * 1000.0, 'f', 2));
    double bF = std::numeric_limits<double>::quiet_NaN();
    double bC = bF;
    for (double w : m_waves) {
        if (std::fabs(w - 486.1) < 0.2) bF = bflAt(w);
        if (std::fabs(w - 656.3) < 0.2) bC = bflAt(w);
    }
    if (std::isfinite(bF) && std::isfinite(bC))
        info += QStringLiteral("\n") + I18n::tr("lde_an_lca")
                                          .arg(QString::number((bC - bF) * 1000.0,
                                                               'f', 2));
    m_anInfo->setText(info);
}

// ── 面テーブルの永続化 (.ofdx "optical.lens.surfaces") ─────────────────────
// 空 = 既定の設計例。**既定のままなら書かない**ので、触っていない
// プロジェクトの .ofdx は 1 バイトも変わらない (CLAUDE.md 絶対規則 2)。
void LensEditorTab::loadRowsFromProject()
{
    const QVector<LensSurfaceRow> &src = m_p->optical().lensSurfaces;
    m_rows = src.isEmpty() ? defaultLensSurfaces() : src;
}

void LensEditorTab::pushRowsToProject()
{
    QVector<LensSurfaceRow> &dst = m_p->optical().lensSurfaces;
    if (dst == m_rows) return;
    if (dst.isEmpty() && m_rows == defaultLensSurfaces()) return;  // 既定のまま
    dst = m_rows;
    m_p->touch();
}
