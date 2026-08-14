// SoundproofTab.cpp
#include "SoundproofTab.h"
#include "../core/FlankingTransmission.h"
#include "../core/Project.h"
#include "../widgets/MiniPlot.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"
#include "TabHelpers.h"

#include "../core/RoomAcoustics.h"
#include "../acoustics/core/SoundInsulation.h"
#include "../io/DxfOutline.h"
#include "../MainWindow.h"   // automation() — 自動実行でモーダルを出さない

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QFileDialog>
#include <QInputDialog>
#include <QMenu>
#include <QFileInfo>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <vector>

using namespace ofd;
namespace ins = ofd::acoustics::insulation;

// ── タブ専用語彙 (file-local 登録, 接頭辞 sp_) ──────────────────────────────
namespace {
const bool s_i18n = [] {
    using ofd::I18n;
    // タブ名 (MainWindow 配線用)
    I18n::reg("t_soundproof", "🔇 防音設計", "🔇 Soundproofing");
    // シナリオ選択
    I18n::reg("sp_scenario_section", "解析シナリオ", "Scenario");
    I18n::reg("sp_scenario_hint",
              "評価したい防音シナリオを選択 — 下に表示される設定ページが切り替わります。",
              "Choose the soundproofing scenario — this switches the settings "
              "page shown below.");
    I18n::reg("sp_sc_partition", "間仕切壁 (Airborne)", "Partition (airborne)");
    I18n::reg("sp_sc_partition_d",
              "オフィス・住居の隣室間の遮音。STC / Rw を評価。",
              "Room-to-room insulation in offices and dwellings. Rates STC / Rw.");
    I18n::reg("sp_sc_facade", "外壁・窓 (Facade)", "Facade wall && window");
    I18n::reg("sp_sc_facade_d",
              "道路・鉄道騒音の屋内侵入。交通騒音スペクトル C_tr 加算。",
              "Road/rail noise intrusion. Adds traffic spectrum C_tr.");
    I18n::reg("sp_sc_floor", "床衝撃音 (Impact)", "Floor impact");
    I18n::reg("sp_sc_floor_d",
              "上階の足音・物落下。タッピングマシンで Ln,w / IIC を測定。",
              "Footsteps and drops from above. Tapping machine gives Ln,w / IIC.");
    I18n::reg("sp_sc_flank", "側路伝搬 (Flanking)", "Flanking transmission");
    I18n::reg("sp_sc_flank_d",
              "壁を回り込む音 (柱・床スラブ・天井裏)。実測値は直接透過のみより悪化。",
              "Sound bypassing the wall (columns, slabs, plenum). Field values "
              "are worse than direct-only.");
    I18n::reg("sp_sc_duct", "ダクト・配管音", "Duct && piping noise");
    I18n::reg("sp_sc_duct_d",
              "空調ダクト経由の音漏れ。減衰量・自己生成騒音を評価。",
              "Leakage via HVAC ducts. Rates attenuation and self-noise.");
    I18n::reg("sp_sc_machine", "設備機器囲い", "Machinery enclosure");
    I18n::reg("sp_sc_machine_d",
              "室外機・ポンプの遮音カバー設計。挿入損失 IL [dB]。",
              "Enclosure design for outdoor units and pumps. Insertion loss IL [dB].");
    I18n::reg("sp_sc_reverb", "室内残響対策", "Room reverberation");
    I18n::reg("sp_sc_reverb_d",
              "オフィス・体育館の吸音処理。RT60 を目標値に収める。",
              "Absorption treatment for offices and gyms. Brings RT60 to target.");
    I18n::reg("sp_sc_speech", "会話プライバシー", "Speech privacy");
    I18n::reg("sp_sc_speech_d",
              "執務空間の話し声漏れ。STI / Speech Privacy Class。",
              "Speech leakage in workspaces. STI / Speech Privacy Class.");
    // 共通
    I18n::reg("sp_vol_fmt", "体積 %1 m³", "volume %1 m³");
    // 間仕切壁
    I18n::reg("sp_rooms_section", "送信室・受信室", "Source && receiving rooms");
    I18n::reg("sp_src_room", "送信室", "Source room");
    I18n::reg("sp_src_room_def", "6.0 × 4.5 × 2.7 m (会議室)",
              "6.0 × 4.5 × 2.7 m (meeting room)");
    I18n::reg("sp_rcv_room", "受信室", "Receiving room");
    I18n::reg("sp_rcv_room_def", "5.5 × 4.5 × 2.7 m (隣接会議室)",
              "5.5 × 4.5 × 2.7 m (adjacent meeting room)");
    I18n::reg("sp_rcv_rt", "受信室 残響時間 T", "Receiving room RT60 T");
    I18n::reg("sp_wall_area", "仕切壁面積 S", "Partition area S");
    I18n::reg("sp_build_section", "壁構造", "Partition build-up");
    I18n::reg("sp_build_hint",
              "層構造を上から順に。厚さ・密度・ヤング率 E・内部損失 η が R(f) の"
              "計算に入ります。\nE 欄が「—」の層は空隙 (空気層・充填吸音材) として"
              "扱い、葉 (パネル) を分離します。",
              "Layers top to bottom. Thickness, density, Young's modulus E and "
              "internal loss η feed the R(f) calculation.\nA layer whose E cell "
              "is \"—\" is treated as a cavity (air gap / porous fill) and "
              "separates the leaves.");
    I18n::reg("sp_h_material", "材質", "Material");
    I18n::reg("sp_h_thick", "厚さ [mm]", "Thickness [mm]");
    I18n::reg("sp_h_density", "密度 [kg/m³]", "Density [kg/m³]");
    I18n::reg("sp_h_surfdens", "面密度 [kg/m²]", "Surface density [kg/m²]");
    I18n::reg("sp_h_young", "E [GPa]", "E [GPa]");
    I18n::reg("sp_h_eta", "η (内部損失)", "η (internal loss)");
    I18n::reg("sp_gypsum", "石膏ボード GB-R", "Gypsum board GB-R");
    I18n::reg("sp_glasswool", "グラスウール 32K", "Glass wool 32K");
    I18n::reg("sp_airgap", "空気層", "Air gap");
    I18n::reg("sp_total", "合計", "Total");
    I18n::reg("sp_add_layer", "＋ 層を追加…", "+ Add layer…");
    I18n::reg("sp_preset_btn", "📚 標準工法プリセット", "📚 Standard build presets");
    I18n::reg("sp_dxf_btn", "📁 .dxf 取込", "📁 Import .dxf");
    // .dxf 取込 — 閉じた LWPOLYLINE の囲む面積を仕切壁面積 S に入れる
    I18n::reg("sp_dxf_title", ".dxf から仕切壁面積を読む",
              "Read the partition area from a .dxf");
    I18n::reg("sp_dxf_filter", "AutoCAD DXF (*.dxf)", "AutoCAD DXF (*.dxf)");
    I18n::reg("sp_dxf_fail", "読めません: %1", "Cannot read: %1");
    I18n::reg("sp_dxf_noloop",
              "閉じた LWPOLYLINE がありません。読めたのは 線分 %1 本 / "
              "閉じていないポリライン %2 本 / 読まなかった実体 %3 個 です。"
              "面積は閉じたポリラインからしか求められません",
              "No closed LWPOLYLINE found. Read: %1 line segments, "
              "%2 open polylines, %3 entities not read. An area can only "
              "come from a closed polyline");
    I18n::reg("sp_dxf_unit_ask",
              "この図面には単位がありません ($INSUNITS なし)。"
              "図面の単位を選んでください",
              "This drawing carries no unit ($INSUNITS absent). "
              "Choose the unit of the drawing");
    I18n::reg("sp_dxf_pick",
              "使う輪郭を選んでください (仕切壁の外形)",
              "Choose the outline to use (the partition)");
    I18n::reg("sp_dxf_loop_item", "輪郭 %1: %2 m² (頂点 %3)",
              "Outline %1: %2 m2 (%3 vertices)");
    I18n::reg("sp_dxf_done",
              "仕切壁面積 S = %1 m² を入れました (%2 の図面 · 輪郭 %3/%4)",
              "Partition area S = %1 m2 applied (drawing in %2, "
              "outline %3 of %4)");
    I18n::reg("sp_dxf_arc",
              " · 円弧の頂点が %1 個あります。面積は円弧を直線で近似した値です",
              " · %1 vertices carry arcs; the area approximates them as "
              "straight segments");
    I18n::reg("sp_dxf_rest",
              " · 読まなかった実体 %1 個 / 線分 %2 本 (輪郭には使っていません)",
              " · %1 entities not read, %2 line segments (not used as outlines)");
    I18n::reg("sp_dxf_auto",
              "自動実行中は取込を行いません (単位と輪郭の選択に対話が要ります)",
              "Import is skipped in automated runs (choosing the unit and "
              "the outline needs a dialog)");
    I18n::reg("sp_dxf_zero",
              "選んだ輪郭の面積が 0 です (自己交差しているか、面積がありません)",
              "The chosen outline has zero area (it self-intersects or is "
              "degenerate)");
    I18n::reg("sp_rc", "コンクリート (RC)", "Concrete (RC)");
    I18n::reg("sp_alc", "ALC パネル", "ALC panel");
    I18n::reg("sp_steel", "鋼板", "Steel plate");
    I18n::reg("sp_ps_gb2gw",
              "乾式二重壁: 石膏ボード12.5×2 + GW50 + 石膏ボード12.5×2 (Rw≈50)",
              "Drywall double leaf: gypsum 12.5×2 + GW50 + gypsum 12.5×2 "
              "(Rw≈50)");
    I18n::reg("sp_ps_gb1",
              "軽鉄間仕切 (最小構成): 石膏ボード12.5 + 空気層65 + 石膏ボード12.5 "
              "(Rw≈33)",
              "Steel-stud partition (minimal): gypsum 12.5 + air 65 + "
              "gypsum 12.5 (Rw≈33)");
    I18n::reg("sp_ps_rc150", "RC 造壁 150mm (Rw≈53)", "RC wall 150 mm (Rw≈53)");
    I18n::reg("sp_ps_rc200", "RC 造壁 200mm (Rw≈56)", "RC wall 200 mm (Rw≈56)");
    I18n::reg("sp_ps_alc100", "ALC パネル 100mm 素板 (Rw≈40)",
              "Bare ALC panel 100 mm (Rw≈40)");
    I18n::reg("sp_rw_ref_fmt", "参考: 同種構造の公表値 Rw ≈ %1 dB",
              "Reference: published Rw ≈ %1 dB for this build-up");
    I18n::reg("sp_rw_ref_none", "参考値: — (プリセット未選択)",
              "Reference: — (no preset selected)");
    I18n::reg("sp_detail_section", "ディテール", "Construction details");
    I18n::reg("sp_det_double", "二重壁構造 (空気層で葉が分離 = 構造的結合なし)",
              "Double-leaf (leaves separated by the cavity, no rigid ties)");
    I18n::reg("sp_det_damp", "制振材塗布 (内側)", "Damping compound (inner side)");
    I18n::reg("sp_det_resil", "弾性支持 (resilient channel)",
              "Resilient channel mounting");
    I18n::reg("sp_det_seal", "気密処理 (シーリング・隙間ゼロ)",
              "Airtight sealing (zero gaps)");
    I18n::reg("sp_det_seal_note", "隙間 1% で R が 10 dB 悪化",
              "a 1% gap degrades R by 10 dB");
    I18n::reg("sp_det_outlet", "コンセント・開口部の遮音処理",
              "Treat outlets and openings");
    I18n::reg("sp_det_frame", "柱・梁(剛接合)", "Columns/beams (rigid joint)");
    I18n::reg("sp_det_yes", "あり", "Present");
    I18n::reg("sp_det_frame_note", "側路伝搬の主因", "main cause of flanking");
    I18n::reg("sp_det_wired_note",
              "R(f) の計算に反映されるのは「二重壁構造」のみです "
              "(OFF = 空隙をまたぐ構造的結合ありとみなし、全層を合計面密度の"
              "単一壁として扱う)。他の項目は計算に反映されません。",
              "Only \"Double-leaf\" feeds the R(f) calculation (OFF = rigid "
              "ties across the cavity, so the whole stack is treated as one "
              "leaf of the combined surface mass). The other items are not "
              "used in the calculation.");
    I18n::reg("sp_tl_section", "音響透過損失 R(f)",
              "Sound transmission loss R(f)");
    I18n::reg("sp_tl_none",
              "R(f) 未計算 — 層構成 (厚さ・密度) を入力してください。",
              "R(f) not computed — enter the layer build-up (thickness, "
              "density).");
    I18n::reg("sp_tl_model_single",
              "モデル: 単一壁 — 場入射質量則 + コインシデンス (Sharp 1973)。"
              "面密度 %1 kg/m²、限界周波数 fc = %2 Hz、η_tot(500Hz) = %3。",
              "Model: single leaf — field-incidence mass law + coincidence "
              "(Sharp 1973). Surface mass %1 kg/m², critical frequency "
              "fc = %2 Hz, η_tot(500 Hz) = %3.");
    I18n::reg("sp_tl_model_double",
              "モデル: 二重壁 (Sharp 1973)。葉 %1 / %2 kg/m²、空隙 %3 mm、"
              "質量-空気-質量共鳴 f0 = %4 Hz、限界周波数 fl = %5 Hz。",
              "Model: double leaf (Sharp 1973). Leaves %1 / %2 kg/m², cavity "
              "%3 mm, mass-air-mass resonance f0 = %4 Hz, limiting frequency "
              "fl = %5 Hz.");
    I18n::reg("sp_tl_fc_none", "限界周波数 fc: — (E 未入力 → 質量則のみ)",
              "Critical frequency fc: — (no E entered → mass law only)");
    I18n::reg("sp_tl_scope",
              "適用範囲: 拡散音場入射・無限大パネルの理想化による予測です。"
              "パネル寸法・端部条件・施工精度・側路伝搬は含みません。"
              "実測値や FDTD の結果を置き換えるものではありません。",
              "Scope: a prediction based on diffuse-field incidence and an "
              "infinite-panel idealisation. Panel size, edge conditions, "
              "workmanship and flanking are not included. It does not replace "
              "measurements or FDTD results.");
    I18n::reg("sp_tl_scope_double",
              "二重壁モデルは「空隙に吸音材が充填され、葉どうしが構造的に"
              "結合していない」理想状態 (上限値) を仮定します。"
              "スタッド等で結合した壁は中高域でこれより 5〜15 dB 低下します。",
              "The double-leaf model assumes the ideal case of an "
              "absorption-filled cavity with no structural ties between the "
              "leaves (an upper bound). Stud-connected walls fall 5-15 dB "
              "below this at mid/high frequencies.");
    I18n::reg("sp_tl_no_fill",
              "⚠ 空隙に吸音材がありません — モデルの前提 (充填吸音) を外れる"
              "ため、空洞共鳴により実際はこれより低下します。",
              "⚠ The cavity has no absorptive fill — this violates the model's "
              "assumption, and cavity resonances will lower the real value.");
    I18n::reg("sp_tl_reduced",
              "⚠ 葉が 3 枚以上あります — もっとも厚い空隙で 2 葉に集約して"
              "計算しています (近似)。",
              "⚠ More than two leaves — reduced to two leaves at the thickest "
              "cavity (approximation).");
    I18n::reg("sp_rating_section", "シングルナンバー評価", "Single-number rating");
    I18n::reg("sp_h_metric", "指標", "Metric");
    I18n::reg("sp_h_value", "値", "Value");
    I18n::reg("sp_h_meaning", "意味", "Meaning");
    I18n::reg("sp_r_c", "C (補正)", "C (adaptation)");
    I18n::reg("sp_r_ctr", "Ctr (交通騒音)", "Ctr (traffic)");
    I18n::reg("sp_m_rw", "遮音単一数値 (ISO 717-1)",
              "Weighted sound reduction (ISO 717-1)");
    I18n::reg("sp_m_stc", "米国規格 (ASTM E413)", "US rating (ASTM E413)");
    I18n::reg("sp_m_c", "ピンクノイズスペクトル補正",
              "Pink-noise spectrum adaptation");
    I18n::reg("sp_m_ctr", "道路・鉄道騒音適用時", "Applied for road/rail noise");
    I18n::reg("sp_m_rwctr", "交通騒音実効値", "Effective value for traffic noise");
    I18n::reg("sp_m_dntw", "標準化レベル差 = Rw + 10log10(0.32V/S)",
              "Standardized level difference = Rw + 10log10(0.32V/S)");
    I18n::reg("sp_rating_note",
              "Rw / C / Ctr は ISO 717-1、STC は ASTM E413 の基準曲線あてはめ"
              "手順を上の R(f) に適用して求めた計算値です。DnT,w は受信室体積 V "
              "と仕切壁面積 S から換算した現場相当値 (ISO 12354-1)。",
              "Rw / C / Ctr follow the ISO 717-1 contour-fitting procedure and "
              "STC the ASTM E413 procedure, both applied to the R(f) above. "
              "DnT,w is converted from the receiving-room volume V and the "
              "partition area S (ISO 12354-1).");
    I18n::reg("sp_use_hint", "用途別目安:", "Guideline by use:");
    I18n::reg("sp_use_hosp", "病室・録音 ≥55", "Hospital / studio ≥55");
    I18n::reg("sp_use_dwell", "住宅隣接 ≥50", "Adjacent dwellings ≥50");
    I18n::reg("sp_use_office", "オフィス 45-50", "Office 45-50");
    // 外壁・窓
    I18n::reg("sp_ext_section", "外部騒音源", "External noise");
    I18n::reg("sp_ext_type", "騒音源種別", "Noise source type");
    I18n::reg("sp_ext_road", "道路交通 (混合 — C_tr 適用)",
              "Road traffic (mixed — apply C_tr)");
    I18n::reg("sp_ext_rail", "鉄道 (高架)", "Railway (elevated)");
    I18n::reg("sp_ext_air", "航空機 (離着陸)", "Aircraft (takeoff/landing)");
    I18n::reg("sp_ext_constr", "建設機械", "Construction machinery");
    I18n::reg("sp_ext_custom", "カスタム スペクトル", "Custom spectrum");
    I18n::reg("sp_ext_level", "ファサード入射レベル Lp1",
              "Incident level at the facade Lp1");
    I18n::reg("sp_ext_level_u", "dB(A) (壁面入射)", "dB(A) (at the facade)");
    I18n::reg("sp_ext_adapt", "スペクトル適応項", "Spectrum adaptation term");
    I18n::reg("sp_ext_adapt_u", "dB (Rw に加算)", "dB (added to Rw)");
    I18n::reg("sp_ext_adapt_ctr",
              "ISO 717-1 では道路交通騒音に Ctr を適用します "
              "(製品の公表値を入力してください)。",
              "ISO 717-1 applies Ctr for road traffic noise (enter the "
              "published value for the product).");
    I18n::reg("sp_ext_adapt_c",
              "ISO 717-1 ではこの音源には C を適用します "
              "(製品の公表値を入力してください)。",
              "ISO 717-1 applies C for this source (enter the published value "
              "for the product).");
    I18n::reg("sp_ext_angle", "入射角", "Incidence angle");
    I18n::reg("sp_ext_angle_u", "° (壁面法線から)", "° (from facade normal)");
    I18n::reg("sp_ext_diffuse", "拡散入射 (diffuse field)",
              "Diffuse-field incidence");
    I18n::reg("sp_ext_unused",
              "入射角・「拡散入射」チェックは計算に使用しません — Rw は"
              "拡散音場入射で定義された量なので、常に拡散入射として扱います。",
              "The incidence angle and the \"diffuse incidence\" checkbox are "
              "not used — Rw is defined for diffuse-field incidence, which is "
              "always assumed here.");
    I18n::reg("sp_facade_section", "ファサード構成", "Facade build-up");
    I18n::reg("sp_fac_wall_area", "壁面積", "Wall area");
    I18n::reg("sp_fac_win_area", "窓面積", "Window area");
    I18n::reg("sp_fac_wall_rw", "壁 Rw", "Wall Rw");
    I18n::reg("sp_fac_win_type", "窓種別", "Glazing type");
    I18n::reg("sp_win1", "単板ガラス 5mm (Rw≈25)", "Single glazing 5 mm (Rw≈25)");
    I18n::reg("sp_win2", "複層ガラス 5+12A+5 (Rw≈30)",
              "Double glazing 5+12A+5 (Rw≈30)");
    I18n::reg("sp_win3", "遮音複層 5+12A+8 (Rw≈35)",
              "Acoustic double 5+12A+8 (Rw≈35)");
    I18n::reg("sp_win4", "合わせ遮音 8.8+12A+6.4 (Rw≈40)",
              "Laminated acoustic 8.8+12A+6.4 (Rw≈40)");
    I18n::reg("sp_fac_sash", "サッシ気密", "Sash airtightness");
    I18n::reg("sp_fac_sash_a4", "気密等級 A-4 (最高)",
              "Airtightness class A-4 (best)");
    I18n::reg("sp_fac_sash_note", "(気密等級は計算に反映されません)",
              "(the airtightness class is not used in the calculation)");
    I18n::reg("sp_fac_vent", "換気口・通気口を考慮 (τ = 1 の開口として合成)",
              "Include vents (combined as a τ = 1 opening)");
    I18n::reg("sp_fac_vent_area", "換気口 相当開口面積", "Equivalent vent area");
    I18n::reg("sp_fac_rcv", "受音室", "Receiving room");
    I18n::reg("sp_fac_rcv_v", "体積 V", "Volume V");
    I18n::reg("sp_fac_rcv_t", "残響時間 T", "RT60 T");
    I18n::reg("sp_fac_use", "用途 (基準値)", "Use (criterion)");
    I18n::reg("sp_fac_use_dwell", "住宅 ≤ 40 dB(A)", "Dwelling ≤ 40 dB(A)");
    I18n::reg("sp_fac_use_hosp", "病院 ≤ 35 dB(A)", "Hospital ≤ 35 dB(A)");
    I18n::reg("sp_fac_use_office", "オフィス ≤ 45 dB(A)", "Office ≤ 45 dB(A)");
    I18n::reg("sp_indoor_section", "室内騒音予測", "Predicted indoor SPL");
    I18n::reg("sp_indoor_lp_fmt", "室内 Lp = %1 dB(A)", "Indoor Lp = %1 dB(A)");
    I18n::reg("sp_indoor_rcomp_fmt",
              "複合 R = %1 dB (壁 %2 m² / 窓 %3 m² / 開口 %4 m² の面積加重 τ 平均, "
              "適応項 %5 dB 込み)、A = %6 m²",
              "Composite R = %1 dB (area-weighted τ average of wall %2 m² / "
              "glazing %3 m² / opening %4 m², adaptation %5 dB included), "
              "A = %6 m²");
    I18n::reg("sp_indoor_ref", "基準値", "Criteria");
    I18n::reg("sp_indoor_ref_note",
              "住宅 ≤ 40dB(A)、病院 ≤ 35dB(A)、オフィス ≤ 45dB(A) - WHO/建築学会",
              "Dwelling ≤ 40 dB(A), hospital ≤ 35 dB(A), office ≤ 45 dB(A) "
              "— WHO / AIJ");
    I18n::reg("sp_indoor_ok", "基準クリア", "Meets the criterion");
    I18n::reg("sp_indoor_ng", "基準未達", "Does not meet the criterion");
    I18n::reg("sp_indoor_note",
              "Lp2 = Lp1 − R + 10·log10(S/A) (ISO 12354-1 の R の定義)。"
              "A = 0.161·V/T (Sabine)。単一数値 Rw による概算で、周波数特性・"
              "側路伝搬・振動伝搬は含みません。",
              "Lp2 = Lp1 − R + 10·log10(S/A) (the definition of R in "
              "ISO 12354-1), with A = 0.161·V/T (Sabine). A single-number "
              "estimate: frequency dependence, flanking and structure-borne "
              "paths are not included.");
    I18n::reg("sp_indoor_need",
              "室内 Lp 未計算 — 入射レベル・面積・受音室 V / T を入力してください。",
              "Indoor Lp not computed — enter the incident level, the areas "
              "and the receiving-room V / T.");
    // 床衝撃音
    I18n::reg("sp_floor_section", "床構造", "Floor build-up");
    I18n::reg("sp_floor_finish", "床仕上げ", "Floor finish");
    I18n::reg("sp_fin_carpet", "カーペット ΔLw = -25 dB", "Carpet ΔLw = -25 dB");
    I18n::reg("sp_fin_cushion", "クッションフロア ΔLw = -18 dB",
              "Cushioned vinyl ΔLw = -18 dB");
    I18n::reg("sp_fin_iso", "遮音フローリング ΔLw = -15 dB",
              "Acoustic flooring ΔLw = -15 dB");
    I18n::reg("sp_fin_std", "標準フローリング ΔLw = 0 dB",
              "Standard flooring ΔLw = 0 dB");
    I18n::reg("sp_fin_tile", "タイル ΔLw = +5 dB (悪化)", "Tile ΔLw = +5 dB (worse)");
    I18n::reg("sp_floor_base", "床下地", "Structural floor");
    I18n::reg("sp_base_rc150", "RC スラブ 150mm", "RC slab 150 mm");
    I18n::reg("sp_base_rc200", "RC スラブ 200mm (推奨)",
              "RC slab 200 mm (recommended)");
    I18n::reg("sp_base_wood", "木造合板 28mm", "Wood panel 28 mm");
    I18n::reg("sp_floor_ceil", "天井", "Ceiling");
    I18n::reg("sp_ceil_direct", "直天井 (なし)", "Direct ceiling (none)");
    I18n::reg("sp_ceil_susp", "吊天井 (空気層あり)", "Suspended (air gap)");
    I18n::reg("sp_ceil_damp", "制振天井 (推奨)", "Damped ceiling (recommended)");
    I18n::reg("sp_floor_bare", "素床 (仕上げ無し) の Ln,w",
              "Bare floor Ln,w (before the covering)");
    I18n::reg("sp_floor_bare_u", "dB — 実測値 / 公表値 (空欄可)",
              "dB — measured or published value (may be left empty)");
    I18n::reg("sp_floor_unwired",
              "床下地・天井・衝撃源の選択は計算に反映されません "
              "(EN 12354-2 による素床 Ln,w の予測は未実装)。",
              "The structural floor, ceiling and impact-source selections are "
              "not used (prediction of the bare-floor Ln,w to EN 12354-2 is "
              "not implemented).");
    I18n::reg("sp_impact_section", "衝撃音発生源", "Impact source");
    I18n::reg("sp_impact_std", "標準源", "Standard source");
    I18n::reg("sp_imp_tap", "タッピングマシン", "Tapping machine");
    I18n::reg("sp_imp_ball", "ゴム球 (中量衝撃)", "Rubber ball (heavy-soft)");
    I18n::reg("sp_imp_tire", "自動車タイヤ", "Car tire");
    I18n::reg("sp_imp_step", "歩行音", "Footsteps");
    I18n::reg("sp_imp_drop", "物落下", "Object drop");
    I18n::reg("sp_floor_result", "結果", "Impact sound level");
    I18n::reg("sp_floor_lnw_fmt", "Ln,w = %1 dB", "Ln,w = %1 dB");
    I18n::reg("sp_floor_lnw_none", "Ln,w = — (未計算)", "Ln,w = — (not computed)");
    I18n::reg("sp_floor_calc_fmt",
              "Ln,w = 素床 %1 dB − ΔLw %2 dB (ISO 717-2 / ISO 12354-2 の "
              "床仕上げによる低減量)。",
              "Ln,w = bare floor %1 dB − ΔLw %2 dB (reduction of impact sound "
              "by the floor covering, ISO 717-2 / ISO 12354-2).");
    I18n::reg("sp_floor_need",
              "素床の Ln,w を入力すると、床仕上げの ΔLw を差し引いた Ln,w を"
              "計算します。素床 Ln,w 自体の予測 (EN 12354-2) は未実装です。",
              "Enter the bare-floor Ln,w and the covering's ΔLw is subtracted "
              "to give Ln,w. Predicting the bare-floor Ln,w itself "
              "(EN 12354-2) is not implemented.");
    I18n::reg("sp_floor_iic_none",
              "IIC (ASTM E989) / JIS A 1419-2 の等級は未計算 — いずれも 1/3 "
              "オクターブの衝撃音レベルスペクトルが必要で、単一数値からは"
              "決められません。",
              "IIC (ASTM E989) and the JIS A 1419-2 grade are not computed — "
              "both need the one-third-octave impact level spectrum and cannot "
              "be derived from a single number.");
    I18n::reg("sp_jis_grade", "JIS 等級:", "JIS grade:");
    // 側路伝搬
    I18n::reg("sp_flank_section", "伝達経路", "Transmission paths");
    I18n::reg("sp_flank_hint",
              "「直接透過 Dd」だけでなく「側路 Ff/Df/Fd」も含めて評価。\n"
              "実測 R' は実験室 R より 5〜10 dB 悪化することが多い。",
              "Includes flanking paths Ff/Df/Fd in addition to direct "
              "transmission Dd.\nField R' is often 5-10 dB worse than "
              "laboratory R.");
    I18n::reg("sp_h_path", "経路", "Path");
    I18n::reg("sp_h_desc", "説明", "Description");
    I18n::reg("sp_p_ff_floor", "Ff (床)", "Ff (floor)");
    I18n::reg("sp_p_df_floor", "Df (床)", "Df (floor)");
    I18n::reg("sp_p_fd_ceil", "Fd (天井)", "Fd (ceiling)");
    I18n::reg("sp_p_ff_col", "Ff (柱)", "Ff (column)");
    I18n::reg("sp_d_dd", "仕切壁直接透過", "Direct through partition");
    I18n::reg("sp_d_ff", "床→床 (側路)", "Floor→floor (flanking)");
    I18n::reg("sp_d_df", "壁→床→壁", "Wall→floor→wall");
    I18n::reg("sp_d_fd", "天井→天井", "Ceiling→ceiling");
    I18n::reg("sp_d_col", "柱経由", "Via columns");
    I18n::reg("sp_flank_total_fmt", "合成 R'w = %1 dB", "Combined R'w = %1 dB");
    I18n::reg("sp_flank_note_fmt", "(直接 %1 dB から %2 dB 悪化)",
              "(%2 dB worse than direct %1 dB)");
    I18n::reg("sp_flank_pred_note",
              "経路別 R [dB] は入力値 (チェック・編集可) — EN 12354-1 (Kij) に"
              "よる経路別 R の予測は未実装。合成 R'w のみ入力から計算します。",
              "Per-path R [dB] values are editable inputs — per-path "
              "prediction to EN 12354-1 (Kij) is not implemented; only the "
              "combined R'w is computed from the inputs.");
    I18n::reg("sp_improve_section", "改善案", "Improvements");
    I18n::reg("sp_impr_float", "床:浮き床 (vibration break) で Ff 改善 (+8 dB)",
              "Floor: floating floor (vibration break), Ff +8 dB");
    I18n::reg("sp_impr_hanger", "天井:防振吊金具で Fd 改善 (+6 dB)",
              "Ceiling: isolation hangers, Fd +6 dB");
    I18n::reg("sp_impr_tape", "柱:制振テープ巻き (+3 dB)",
              "Columns: damping tape (+3 dB)");
    I18n::reg("sp_impr_elastic", "梁:エラスティック分離 (+5 dB)",
              "Beams: elastic separation (+5 dB)");
    I18n::reg("sp_recalc_btn", "▶ 改善後の R' を再計算",
              "▶ Recompute R' with improvements");
    // ダクト
    I18n::reg("sp_duct_section", "ダクト経路", "Duct path");
    I18n::reg("sp_duct_shape", "ダクト形状", "Duct shape");
    I18n::reg("sp_duct_round", "円形", "Round");
    I18n::reg("sp_duct_rect", "角形", "Rectangular");
    I18n::reg("sp_duct_flex", "フレキ", "Flexible");
    I18n::reg("sp_duct_sect", "ダクト断面", "Duct cross-section");
    I18n::reg("sp_duct_sect_hint", "角形は「幅 × 高さ mm」、円形は「直径 mm」",
              "Rectangular: \"width × height mm\"; round: \"diameter mm\"");
    I18n::reg("sp_duct_len", "ダクト全長", "Total duct length");
    I18n::reg("sp_duct_elbow", "エルボ", "Elbows");
    I18n::reg("sp_duct_elbow_u", "箇所", "count");
    I18n::reg("sp_duct_branch", "分岐 (断面積比 A_分岐/A_主管)",
              "Branch (area ratio A_branch/A_main)");
    I18n::reg("sp_duct_branch_u", "(1.0 = 分岐なし)", "(1.0 = no branch)");
    I18n::reg("sp_duct_damper", "ダンパー / VAV", "Dampers / VAV");
    I18n::reg("sp_duct_damper_note", "(自己発生騒音は未計算)",
              "(self-generated noise is not computed)");
    I18n::reg("sp_atten_section", "減衰要素", "Attenuation elements");
    I18n::reg("sp_att_silencer", "消音器 (スプライサー) 挿入損失",
              "Silencer (splitter) insertion loss");
    I18n::reg("sp_att_lining", "内貼り吸音材 (Sabine の式で減衰を計算)",
              "Acoustic lining (attenuation from the Sabine equation)");
    I18n::reg("sp_att_alpha_note",
              "内貼りの吸音率 α は下の帯域表で帯域ごとに入力します "
              "(既定値 = 50mm グラスウール 32K の公表吸音率の代表値)。",
              "The lining's absorption coefficient α is entered per band in "
              "the table below (defaults = typical published values for 50 mm "
              "32 kg/m³ glass wool).");
    I18n::reg("sp_h_alpha", "内貼り α", "Lining α");
    I18n::reg("sp_att_isolator", "ファン振動アイソレータ",
              "Fan vibration isolators");
    I18n::reg("sp_att_isolator_note", "(固体伝搬経路は未計算)",
              "(the structure-borne path is not computed)");
    I18n::reg("sp_duct_room", "受音室", "Receiving room");
    I18n::reg("sp_duct_indoor", "室内到達音", "Indoor level");
    I18n::reg("sp_h_band", "帯域 [Hz]", "Band [Hz]");
    I18n::reg("sp_h_pwl", "ファン PWL [dB]", "Fan PWL [dB]");
    I18n::reg("sp_h_atten", "減衰 [dB]", "Attenuation [dB]");
    I18n::reg("sp_h_lp", "室内 Lp [dB]", "Indoor Lp [dB]");
    I18n::reg("sp_duct_lpa_fmt", "室内 SPL = %1 dB(A)",
              "Indoor SPL = %1 dB(A)");
    I18n::reg("sp_duct_nc_fmt", "NC-%1", "NC-%1");
    I18n::reg("sp_duct_need",
              "室内 SPL / NC 未計算 — ファン PWL のオクターブバンド値 (63〜4k Hz "
              "の 7 帯域すべて) と受音室 V / T を入力してください。",
              "Indoor SPL / NC not computed — enter the fan PWL for all seven "
              "octave bands (63-4k Hz) and the receiving-room V / T.");
    I18n::reg("sp_duct_note",
              "減衰は ASHRAE Handbook (HVAC Applications, Sound and Vibration "
              "Control) の式: 内貼り = Sabine の式 1.05·α^1.4·P/A [dB/m] "
              "(α≳0.2・1 kHz 以下で妥当、合計 40 dB で頭打ち)、エルボ = f·w 表、"
              "分岐 = 10log10(ΣA/A)、開口端反射 = 低周波放射効率 "
              "(Levine & Schwinger 1948)。室内は拡散音場 Lp = LW + 10log10(4/A)。"
              "⚠ Sabine の式は吸音率の高い内貼り (α ≳ 0.4) と 1 kHz 超で"
              "過大評価します — 設計値には製造者の実測挿入損失を使ってください。"
              "ファンの自己発生騒音・ダクト壁からの再放射は含みません。",
              "Attenuation follows the ASHRAE Handbook (HVAC Applications, "
              "Sound and Vibration Control): lining = Sabine's equation "
              "1.05·α^1.4·P/A [dB/m] (valid for α≳0.2 below 1 kHz, capped at "
              "40 dB), elbows = the f·w table, branch = 10log10(ΣA/A), end "
              "reflection = the low-frequency radiation efficiency "
              "(Levine & Schwinger 1948). The room is treated as a diffuse "
              "field, Lp = LW + 10log10(4/A). WARNING: the Sabine equation "
              "over-predicts for highly absorptive linings (α > ~0.4) and "
              "above ~1 kHz — use the manufacturer's measured insertion loss "
              "for design. Fan self-noise and breakout through the duct wall "
              "are not included.");
    // 設備機器囲い
    I18n::reg("sp_mach_section", "機器囲い", "Machinery enclosure");
    I18n::reg("sp_mach_dev", "機器", "Machine");
    I18n::reg("sp_dev_ac", "エアコン室外機", "A/C outdoor unit");
    I18n::reg("sp_dev_gen", "発電機", "Generator");
    I18n::reg("sp_dev_comp", "コンプレッサー", "Compressor");
    I18n::reg("sp_dev_hp", "ヒートポンプ", "Heat pump");
    I18n::reg("sp_dev_custom", "カスタム", "Custom");
    I18n::reg("sp_mach_level", "機器音源レベル", "Source level");
    I18n::reg("sp_mach_size", "囲い形状", "Enclosure size");
    I18n::reg("sp_mach_wall", "壁構造", "Wall build-up");
    I18n::reg("sp_mwall1", "鉄板 1.6mm + 制振材 + グラスウール50mm + 吸音材",
              "Steel 1.6 mm + damping + glass wool 50 mm + absorber");
    I18n::reg("sp_mwall2", "鉄板 2.3mm + ダンパー塗布",
              "Steel 2.3 mm + damping compound");
    I18n::reg("sp_mach_alpha", "内部吸音率 ᾱ", "Interior absorption ᾱ");
    I18n::reg("sp_mach_open", "開口部 (吸気・排気)", "Openings (intake/exhaust)");
    I18n::reg("sp_mach_open_u", "m² (要消音)", "m² (needs silencing)");
    I18n::reg("sp_il_section", "挿入損失 IL", "Insertion loss IL");
    I18n::reg("sp_il_badge_fmt", "IL(500 Hz) = %1 dB", "IL(500 Hz) = %1 dB");
    I18n::reg("sp_il_none", "IL = — (囲い寸法・壁構造を入力してください)",
              "IL = — (enter the enclosure size and wall build-up)");
    I18n::reg("sp_il_geom_fmt",
              "囲い表面積 S = %1 m²、開口 %2 m² (開口率 %3 %)、"
              "壁の面密度 %4 kg/m²",
              "Enclosure surface S = %1 m², openings %2 m² (%3 %), wall "
              "surface mass %4 kg/m²");
    I18n::reg("sp_il_note",
              "IL = R_eff − 10·log10(S/A_in) (ISO 11546-1 / Bies & Hansen)。"
              "R_eff は開口 (τ=1) を含む面積加重の τ 平均、A_in = S·ᾱ。"
              "内部が拡散音場で、構造伝搬・気密不良・開口の消音が無い理想状態"
              "の上限値です。開口を消音しない限り IL は開口率で頭打ちになります。",
              "IL = R_eff − 10·log10(S/A_in) (ISO 11546-1 / Bies & Hansen). "
              "R_eff is the area-weighted τ average including the openings "
              "(τ = 1) and A_in = S·ᾱ. An upper bound assuming a diffuse "
              "interior field with no structure-borne path, no leakage and no "
              "silencing of the openings — without silencers the IL saturates "
              "at the opening ratio.");
    I18n::reg("sp_il_srcnote_fmt",
              "機器音 %1 dB(A) @1m − IL(500 Hz) %2 dB = %3 dB(A) @1m "
              "(参考値。IL は帯域量、機器音は A 特性の総合値なので厳密には"
              "一致しません)。",
              "Source %1 dB(A) @1 m − IL(500 Hz) %2 dB = %3 dB(A) @1 m "
              "(indicative only: IL is a band quantity while the source level "
              "is an A-weighted overall level).");
    // 室内残響対策
    I18n::reg("sp_rev_section", "室内残響対策", "Room reverberation control");
    I18n::reg("sp_rev_room", "部屋", "Room");
    I18n::reg("sp_room_office", "オフィス (オープンプラン)", "Office (open plan)");
    I18n::reg("sp_room_gym", "体育館", "Gymnasium");
    I18n::reg("sp_room_cafe", "食堂", "Cafeteria");
    I18n::reg("sp_room_class", "教室", "Classroom");
    I18n::reg("sp_room_rest", "レストラン", "Restaurant");
    I18n::reg("sp_rev_size", "サイズ", "Size");
    I18n::reg("sp_rev_size_def", "20 × 15 × 3.5 m", "20 × 15 × 3.5 m");
    I18n::reg("sp_rev_vol_fmt", "(体積 %1 m³)", "(volume %1 m³)");
    I18n::reg("sp_rev_target", "目標 RT60", "Target RT60");
    I18n::reg("sp_rev_target_u", "s (オフィス推奨)", "s (office recommendation)");
    I18n::reg("sp_abs_section", "吸音材配置", "Absorber placement");
    I18n::reg("sp_h_surface", "面", "Surface");
    I18n::reg("sp_h_area", "面積", "Area");
    I18n::reg("sp_h_mat", "材料", "Material");
    I18n::reg("sp_s_ceiling", "天井", "Ceiling");
    I18n::reg("sp_s_wall_up", "壁 (上半)", "Wall (upper)");
    I18n::reg("sp_s_wall_low", "壁 (下半)", "Wall (lower)");
    I18n::reg("sp_s_floor", "床", "Floor");
    I18n::reg("sp_mat_t15", "音響パネル T15", "Acoustic panel T15");
    I18n::reg("sp_mat_abs", "吸音パネル", "Absorber panel");
    I18n::reg("sp_mat_wood", "木質パネル", "Wood panel");
    I18n::reg("sp_mat_carpet", "カーペット", "Carpet");
    I18n::reg("sp_rev_result", "評価", "Result");
    I18n::reg("sp_rev_ok", "目標達成", "Target met");
    I18n::reg("sp_rev_ng", "目標未達", "Target not met");
    I18n::reg("sp_rev_rt_fmt", "RT60 = %1 s @ 1kHz", "RT60 = %1 s @ 1kHz");
    I18n::reg("sp_rev_note_fmt",
              "Sabine: RT = 0.161 × V / A — V = %1 m³, A = %2 m²·Sabin "
              "(チェック ON 行の Σ 面積 × α@1kHz)",
              "Sabine: RT = 0.161 × V / A — V = %1 m³, A = %2 m²·Sabin "
              "(Σ area × α@1kHz over checked rows)");
    // 会話プライバシー
    I18n::reg("sp_speech_section", "会話プライバシー", "Speech privacy");
    I18n::reg("sp_speech_hint",
              "執務空間の会話漏れ評価。受聴点の S/N と残響時間から "
              "IEC 60268-16 の MTF 法で STI を求めます。",
              "Evaluates speech leakage in workspaces. STI is computed from "
              "the S/N at the listener and the reverberation time using the "
              "MTF method of IEC 60268-16.");
    I18n::reg("sp_speech_scenario", "シナリオ", "Scenario");
    I18n::reg("sp_spc_open", "オープンオフィス", "Open office");
    I18n::reg("sp_spc_closed", "個室間", "Between private rooms");
    I18n::reg("sp_spc_meeting", "会議室漏れ", "Meeting-room leakage");
    I18n::reg("sp_speech_scen_note",
              "シナリオの選択は計算に反映されません — 現在の計算は"
              "「同一室内の直接音 + 暗騒音」のみで、隔壁の遮音 (個室間・"
              "会議室漏れ) は含みません。",
              "The scenario selection is not used — the present calculation "
              "covers only the direct sound plus background noise within one "
              "room, and does not include a separating partition.");
    I18n::reg("sp_speech_level", "話者の音圧レベル @1m",
              "Talker level @1 m");
    I18n::reg("sp_speech_level_u", "dB(A) (通常発声 ≈ 60)",
              "dB(A) (normal vocal effort ≈ 60)");
    I18n::reg("sp_speech_dist", "話者-受聴者距離", "Talker-listener distance");
    I18n::reg("sp_speech_rt", "受聴室 残響時間 RT60", "Listening room RT60");
    I18n::reg("sp_speech_bg", "バックグラウンドノイズ (HVAC等)",
              "Background noise (HVAC etc.)");
    I18n::reg("sp_speech_bg_u", "dB(A) → マスキング有利", "dB(A) → aids masking");
    I18n::reg("sp_speech_mask", "サウンドマスキングシステム導入",
              "Install sound-masking system");
    I18n::reg("sp_speech_mask_u", "dB(A) (暗騒音とエネルギー合成)",
              "dB(A) (energy-summed with the background)");
    I18n::reg("sp_metrics_section", "評価指標", "Metrics");
    I18n::reg("sp_sti_fmt", "STI = %1", "STI = %1");
    I18n::reg("sp_sti_none", "STI = — (入力不足)", "STI = — (missing input)");
    I18n::reg("sp_sti_snr_fmt",
              "受聴点 S/N = %1 dB (話者 %2 dB(A) @1m を自由音場で %3 m 減衰、"
              "暗騒音 %4 dB(A))、RT60 = %5 s",
              "S/N at the listener = %1 dB (talker %2 dB(A) @1 m attenuated "
              "over %3 m in a free field, background %4 dB(A)), RT60 = %5 s");
    I18n::reg("sp_sti_note",
              "IEC 60268-16 の MTF 法 (男声の α/β 重み、変調周波数 0.63〜12.5 Hz "
              "の 14 点)。全帯域で同一の RT60・S/N を仮定した簡易版で、"
              "帯域別の暗騒音スペクトルとマスキング効果は含みません。"
              "直接音は自由音場 (−20log10 r) として扱っています。",
              "The MTF method of IEC 60268-16 (male α/β weights, 14 modulation "
              "frequencies from 0.63 to 12.5 Hz). A simplified form assuming "
              "the same RT60 and S/N in every band; band-wise background "
              "spectra and auditory masking are not included. The direct sound "
              "is treated as free-field (−20log10 r).");
    I18n::reg("sp_speech_ok", "プライバシー良好", "Good privacy");
    I18n::reg("sp_speech_ng", "プライバシー不十分", "Insufficient privacy");
    I18n::reg("sp_h_intel", "明瞭度", "Intelligibility");
    I18n::reg("sp_h_priv", "プライバシー", "Privacy");
    I18n::reg("sp_v_exc", "優", "Excellent");
    I18n::reg("sp_v_none", "なし", "None");
    I18n::reg("sp_v_normal", "普通", "Fair");
    I18n::reg("sp_v_insuf", "不十分", "Insufficient");
    I18n::reg("sp_v_good", "良好", "Good");
    I18n::reg("sp_v_bad", "不可", "Bad");
    I18n::reg("sp_v_conf", "機密保持", "Confidential");
    // 出力
    I18n::reg("sp_export_section", "出力", "Export");
    I18n::reg("sp_exp_report", "📄 遮音設計レポート (PDF)",
              "📄 Soundproofing report (PDF)");
    // シナリオによって出る量が違う (間仕切壁は R、機器囲いは IL) ので
    // 「R(f)」と言い切らない
    I18n::reg("sp_exp_csv", "📊 帯域スペクトル (CSV)", "📊 Band spectrum (CSV)");
    I18n::reg("sp_exp_csv_tip",
              "表示中のシナリオの帯域スペクトル (図と同じ値) を CSV で"
              "書き出します。間仕切壁は R(f)、設備機器囲いは IL(f)。",
              "Writes the band spectrum of the current scenario (the same "
              "values as the plot) as CSV. Partition gives R(f), machinery "
              "enclosure gives IL(f).");
    I18n::reg("sp_exp_csv_none",
              "このシナリオには帯域スペクトルがありません。R(f) は「間仕切壁」、"
              "IL(f) は「設備機器囲い」で計算されます。",
              "This scenario has no band spectrum. R(f) comes from “Partition” "
              "and IL(f) from “Machinery enclosure”.");
    I18n::reg("sp_exp_csv_empty",
              "まだ計算されていません。入力を埋めて曲線が出てから書き出して"
              "ください。",
              "Nothing has been computed yet. Fill in the inputs until the "
              "curve appears, then export.");
    I18n::reg("sp_exp_csv_title", "帯域スペクトルの書出", "Export band spectrum");
    I18n::reg("sp_exp_csv_ok",
              "書き出しました: %1\n%2 の %3(f) を %4 帯域 (%5 〜 %6 Hz)。"
              "図に出ている値そのものです。",
              "Written: %1\n%3(f) of %2 over %4 bands (%5 to %6 Hz). "
              "These are exactly the values shown in the plot.");
    I18n::reg("sp_exp_aural", "🎧 可聴化 (受音側で試聴)",
              "🎧 Auralization (listen at receiver)");
    I18n::reg("sp_exp_std", "📑 規格対応書式 (ISO/ASTM)",
              "📑 Standard forms (ISO/ASTM)");
    I18n::reg("sp_impr_after_fmt", "改善後 R'w = %1 dB  (+%2 dB)",
              "R'w with improvements = %1 dB  (+%2 dB)");
    I18n::reg("sp_impr_none", "改善案を選ぶと合成 R'w への効果が出ます",
              "Select an improvement to see its effect on the combined R'w");
    I18n::reg("sp_impr_weakest_fmt",
              "▸ 合成は最も弱い経路で頭打ちになります。改善後に最も弱いのは"
              "「%1」(%2 dB) — ここを直さない限り全体はこれ以上良くなりません。",
              "▸ The combination is capped by the weakest path. After the "
              "improvements the weakest is “%1” (%2 dB); nothing else helps "
              "until that one is fixed.");
    I18n::reg("sp_impr_note",
              "▸ 改善量は各項目に書いてある値を、対応する経路の R に足して"
              "合成し直したものです (表の入力値そのものは書き換えません)。"
              "梁の弾性分離は接合部の経路として Df (壁→床→壁) に計上します。",
              "▸ Each improvement adds the stated amount to its path's R and "
              "the combination is recomputed (the values in the table itself "
              "are left alone). Elastic separation of the beams is counted "
              "against Df (wall-floor-wall) as the junction path.");
    I18n::reg("sp_uw_flank_kij",
              "経路別 R の予測 (EN 12354-1 の振動低減指数 Kij)",
              "per-path prediction of R (the EN 12354-1 vibration reduction "
              "index Kij)");
    I18n::reg("sp_uw_flank_ok",
              "経路の R と改善案 — 合成 R'w = −10log10(Σ10^(−R/10)) の入力に"
              "なります",
              "the path R values and the improvements, which feed the "
              "combination R'w = -10log10(sum 10^(-R/10))");
    return true;
}();

// ── 小物ヘルパー (mock の badge / muted / q-num 相当) ───────────────────────
const char kAcc[]  = "#0078D4";   // badge acc
const char kOk[]   = "#2E8B57";   // badge ok
const char kWarn[] = "#B45309";   // badge warn
const char kAccAcoustic[] = "#2E8B57";   // var(--acc-acoustic)

// バッジの枠色スタイルを適用する (判定バッジの OK/警告 色替えにも使う)
void styleBadge(QLabel *l, const char *color, bool big = false)
{
    l->setStyleSheet(QString("color:%1; border:1px solid %1; border-radius:3px;"
                             " padding:%2; font-weight:600;%3")
                         .arg(color, big ? "3px 10px" : "1px 6px",
                              big ? " font-size:13px;" : ""));
}

QLabel *makeBadge(const QString &text, const char *color, QWidget *parent,
                  bool big = false)
{
    auto *l = new QLabel(text, parent);
    styleBadge(l, color, big);
    return l;
}

QLabel *makeHint(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setWordWrap(true);
    return l;
}

QCheckBox *makeCheck(const QString &text, bool on, QWidget *parent)
{
    auto *c = new QCheckBox(text, parent);
    c->setChecked(on);
    return c;
}

QLineEdit *numEdit(const QString &text, QWidget *parent, int w = 80)
{
    auto *e = new QLineEdit(text, parent);
    e->setMaximumWidth(w);
    return e;
}

QHBoxLayout *unitRow(QWidget *w, const QString &unit, QWidget *parent)
{
    auto *h = new QHBoxLayout();
    h->addWidget(w);
    h->addWidget(new QLabel(unit, parent));
    h->addStretch(1);
    return h;
}

QTableWidgetItem *textItem(const QString &s) { return new QTableWidgetItem(s); }

QTableWidgetItem *numItem(const QString &s)
{
    auto *it = new QTableWidgetItem(s);
    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return it;
}

QTableWidgetItem *checkItem(bool on)
{
    auto *it = new QTableWidgetItem;
    it->setCheckState(on ? Qt::Checked : Qt::Unchecked);
    it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
    return it;
}

QTableWidget *makeTable(const QStringList &headers, int rows, QWidget *parent,
                        int minH)
{
    auto *t = new QTableWidget(rows, headers.size(), parent);
    t->setHorizontalHeaderLabels(headers);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    t->horizontalHeader()->setStretchLastSection(true);
    t->verticalHeader()->setVisible(false);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setMinimumHeight(minH);
    return t;
}

// 入力テーブルの編集を有効化する (編集させたくないセルは lockItem で落とす)
void enableTableEdit(QTableWidget *t)
{
    t->setEditTriggers(QAbstractItemView::DoubleClicked |
                       QAbstractItemView::SelectedClicked |
                       QAbstractItemView::EditKeyPressed);
}

// セルを編集不可にする (textItem/numItem の既定フラグは編集可)
QTableWidgetItem *lockItem(QTableWidgetItem *it)
{
    it->setFlags(it->flags() & ~Qt::ItemIsEditable);
    return it;
}

// セルの数値を取り出す ("—" や空欄・未設定は ok=false)
double cellNum(const QTableWidget *t, int row, int col, bool *ok)
{
    const QTableWidgetItem *it = t->item(row, col);
    if (!it) { *ok = false; return 0; }
    return it->text().toDouble(ok);
}

// テキスト中の数値を先頭から最大 n 個取り出す (寸法入力の解釈に使う)
int parseNumbers(const QString &text, double *out, int n)
{
    static const QRegularExpression kNumRe(
        QStringLiteral("[0-9]+(?:[.][0-9]+)?"));
    int found = 0;
    auto mi = kNumRe.globalMatch(text);
    while (mi.hasNext() && found < n)
        out[found++] = mi.next().captured().toDouble();
    return found;
}

// 「L × W × H」形式から体積を返す (3 数値が揃わなければ 0)
double parseVolume(const QString &text)
{
    double d[3] = { 0, 0, 0 };
    if (parseNumbers(text, d, 3) != 3) return 0;
    if (d[0] <= 0 || d[1] <= 0 || d[2] <= 0) return 0;
    return d[0] * d[1] * d[2];
}

// ── 標準工法プリセット (間仕切壁の層構成) ───────────────────────────────────
// 層データの出所 (いずれも公表値):
//   石膏ボード GB-R : 12.5 mm の面密度 約 9.0 kg/m² (JIS A 6901) → 720 kg/m³、
//                     E ≈ 2.5 GPa、内部損失 η_int ≈ 0.015
//   グラスウール 32K: 32 kg/m³ (充填吸音材 = 空隙扱い)
//   普通コンクリート: 2400 kg/m³ (JASS 5)、E ≈ 30 GPa、η_int ≈ 0.006
//   ALC パネル      : 約 600 kg/m³ (JIS A 5416)、E ≈ 1.7 GPa、η_int ≈ 0.01
//   鋼板            : 7850 kg/m³、E = 210 GPa。制振材塗布時 η_int ≈ 0.05
// 内部損失 η_int の代表値は EN 12354-1:2000 附属書 C による。境界への振動流出
// (η_tot = η_int + m'/(485√f)) は SoundInsulation 側で加算する。
// メニュー名と「参考値」の Rw は日本建築学会「建築物の遮音性能基準と設計指針」
// 等で公表されている同種構造の代表値 — 計算値とは独立の参照値として表示する。
struct PresetLayer {
    const char *mat;
    double th;     // 厚さ [mm]
    double rho;    // 密度 [kg/m³] (<=0 → 空気層)
    double eGPa;   // ヤング率 [GPa] (<=0 → 空隙層 = 葉を分離する)
    double eta;    // 内部損失係数
};
struct WallPreset {
    const char *name;
    int  rw;           // 公表値 (参考表示のみ)
    bool decoupled;    // 葉が構造的に分離しているか (二重壁チェックの初期値)
    int  n;
    PresetLayer layers[6];
};

const WallPreset kWallPresets[] = {
    { "sp_ps_gb2gw", 50, true, 6,
      { { "sp_gypsum", 12.5, 720, 2.5, 0.015 },
        { "sp_gypsum", 12.5, 720, 2.5, 0.015 },
        { "sp_glasswool", 50, 32, 0, 0 },
        { "sp_airgap", 15, 0, 0, 0 },
        { "sp_gypsum", 12.5, 720, 2.5, 0.015 },
        { "sp_gypsum", 12.5, 720, 2.5, 0.015 } } },
    // 軽鉄間仕切はスタッドで両面が結合しているため decoupled = false
    { "sp_ps_gb1", 33, false, 3,
      { { "sp_gypsum", 12.5, 720, 2.5, 0.015 },
        { "sp_airgap", 65, 0, 0, 0 },
        { "sp_gypsum", 12.5, 720, 2.5, 0.015 } } },
    { "sp_ps_rc150", 53, true, 1, { { "sp_rc", 150, 2400, 30, 0.006 } } },
    { "sp_ps_rc200", 56, true, 1, { { "sp_rc", 200, 2400, 30, 0.006 } } },
    { "sp_ps_alc100", 40, true, 1, { { "sp_alc", 100, 600, 1.7, 0.01 } } },
};

// 既定の層構成 (mock soundproof.jsx の初期値と同一)
const PresetLayer kDefaultLayers[5] = {
    { "sp_gypsum", 12.5, 720, 2.5, 0.015 },
    { "sp_glasswool", 50, 32, 0, 0 },
    { "sp_airgap", 25, 0, 0, 0 },
    { "sp_glasswool", 50, 32, 0, 0 },
    { "sp_gypsum", 12.5, 720, 2.5, 0.015 },
};

// 層テーブルの列
enum LayerCol { LcCheck = 0, LcIndex, LcMat, LcThick, LcRho, LcSurf,
                LcYoung, LcEta, LcCount };

// 層テーブルの合計行 (下から 2 行目) を算術更新する。面密度列は
// 厚さ × 密度から再計算する (密度が数値でない層 = 空気層は質量 0)。
// 合計はチェック ON の層のみ。
void recomputeLayerTotals(QTableWidget *t)
{
    QSignalBlocker block(t);   // 計算セルの書込で itemChanged を再発火させない
    const int nLayers = t->rowCount() - 2;   // 末尾 2 行 = 合計・「＋層を追加…」
    double sumTh = 0, sumSd = 0;
    for (int r = 0; r < nLayers; ++r) {
        bool okTh = false, okRho = false;
        const double th  = cellNum(t, r, LcThick, &okTh);
        const double rho = cellNum(t, r, LcRho, &okRho);
        const bool hasSd = okTh && okRho;
        const double sd = hasSd ? th / 1000.0 * rho : 0;
        if (auto *it = t->item(r, LcSurf))
            it->setText(hasSd ? QString::number(sd, 'f', 1) : QString("—"));
        const QTableWidgetItem *chk = t->item(r, LcCheck);
        if (!chk || chk->checkState() != Qt::Checked) continue;
        if (okTh) sumTh += th;
        sumSd += sd;
    }
    if (auto *it = t->item(nLayers, LcThick))
        it->setText(QString::number(sumTh, 'g', 6));
    if (auto *it = t->item(nLayers, LcSurf))
        it->setText(QString::number(sumSd, 'f', 1));
}

// 層テーブル (チェック ON 行) から計算用の層構成を作る。
// E 欄が数値でない層は空隙 (空気層 / 充填吸音材) として扱う。
std::vector<ins::Layer> layersFromTable(const QTableWidget *t)
{
    std::vector<ins::Layer> out;
    const int nLayers = t->rowCount() - 2;
    for (int r = 0; r < nLayers; ++r) {
        const QTableWidgetItem *chk = t->item(r, LcCheck);
        if (!chk || chk->checkState() != Qt::Checked) continue;
        bool okTh = false, okRho = false, okE = false, okEta = false;
        const double th  = cellNum(t, r, LcThick, &okTh);
        const double rho = cellNum(t, r, LcRho, &okRho);
        const double e   = cellNum(t, r, LcYoung, &okE);
        const double eta = cellNum(t, r, LcEta, &okEta);
        if (!okTh || th <= 0) continue;
        ins::Layer L;
        L.thicknessM  = th / 1000.0;
        L.densityKgM3 = (okRho && rho > 0) ? rho : 0.0;
        L.cavity      = !(okE && e > 0 && L.densityKgM3 > 0);
        L.porousFill  = L.cavity && L.densityKgM3 > 0;
        L.youngsPa    = L.cavity ? 0.0 : e * 1e9;
        L.lossFactor  = (okEta && eta > 0) ? eta : 0.01;
        out.push_back(L);
    }
    return out;
}

// 層テーブルを層構成で埋め直し、末尾に合計行と「＋層を追加…」行を再構築する
void populateLayerTable(QTableWidget *t, const PresetLayer *layers, int n)
{
    QSignalBlocker block(t);
    t->clearSpans();
    t->clearContents();
    t->setRowCount(n + 2);
    for (int i = 0; i < n; ++i) {
        t->setItem(i, LcCheck, checkItem(true));
        t->setItem(i, LcIndex, lockItem(numItem(QString::number(i + 1))));
        t->setItem(i, LcMat, textItem(I18n::tr(layers[i].mat)));
        t->setItem(i, LcThick, numItem(QString::number(layers[i].th, 'g', 6)));
        t->setItem(i, LcRho, layers[i].rho > 0
                                 ? numItem(QString::number(layers[i].rho, 'g', 6))
                                 : numItem("—"));
        t->setItem(i, LcSurf, lockItem(numItem("")));  // 面密度 = 厚さ×密度
        t->setItem(i, LcYoung,
                   layers[i].eGPa > 0
                       ? numItem(QString::number(layers[i].eGPa, 'g', 6))
                       : numItem("—"));
        t->setItem(i, LcEta,
                   layers[i].eGPa > 0
                       ? numItem(QString::number(layers[i].eta, 'g', 6))
                       : numItem("—"));
    }
    // 合計行 (太字, recomputeLayerTotals が算術更新)
    t->setSpan(n, 0, 1, 3);
    auto *tot = lockItem(textItem(I18n::tr("sp_total")));
    QFont bf = tot->font();
    bf.setBold(true);
    tot->setFont(bf);
    t->setItem(n, LcCheck, tot);
    auto *tth = lockItem(numItem("")); tth->setFont(bf);
    t->setItem(n, LcThick, tth);
    t->setItem(n, LcRho, lockItem(textItem("—")));
    auto *tsd = lockItem(numItem("")); tsd->setFont(bf);
    t->setItem(n, LcSurf, tsd);
    t->setItem(n, LcYoung, lockItem(textItem("—")));
    t->setItem(n, LcEta, lockItem(textItem("—")));
    // ＋ 層を追加… (行追加は未実装のまま — 層構成の変更はプリセット/編集で)
    t->setItem(n + 1, LcCheck, checkItem(false));
    t->setSpan(n + 1, 1, 1, LcCount - 1);
    auto *add = lockItem(textItem(I18n::tr("sp_add_layer")));
    QFont itf = add->font();
    itf.setItalic(true);
    add->setFont(itf);
    t->setItem(n + 1, LcIndex, add);
    recomputeLayerTotals(t);
}

// ── ダクト用 ────────────────────────────────────────────────────────────────
// オクターブ帯域 63〜4000 Hz (NC 評価と同じ 7 帯域)
const double kOctHz[7] = { 63, 125, 250, 500, 1000, 2000, 4000 };
// A 特性の補正値 [dB] (IEC 61672-1)
const double kAWeight[7] = { -26.2, -16.1, -8.6, -3.2, 0.0, 1.2, 1.0 };
// ダクト内貼りの既定吸音率: 50 mm グラスウール 32K (剛壁裏打ち) の
// 公表ランダム入射吸音率の代表値。編集可の入力として表に流し込む。
const double kLiningAlpha[7] = { 0.10, 0.25, 0.65, 0.90, 0.98, 0.99, 0.99 };
} // namespace

// ── SoundproofTab ───────────────────────────────────────────────────────────
SoundproofTab::SoundproofTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // 解析シナリオ (カード式セレクタ)
    auto *ss = new SectionBox(I18n::tr("sp_scenario_section"), body);
    ss->vbox()->addWidget(makeHint(I18n::tr("sp_scenario_hint"), ss));
    auto *grid = new QGridLayout();
    grid->setSpacing(6);
    m_scenarioGroup = new QButtonGroup(this);
    m_scenarioGroup->setExclusive(true);
    struct Sc { const char *icon; const char *name; const char *desc;
                const char *std; };
    static const Sc kSc[8] = {
        { "▥", "sp_sc_partition", "sp_sc_partition_d", "ASTM E90 / ISO 10140-2" },
        { "▤", "sp_sc_facade",    "sp_sc_facade_d",  "ISO 717-1 (C_tr) / EN 12354-3" },
        { "▭", "sp_sc_floor",     "sp_sc_floor_d",     "ISO 10140-3 / ASTM E492" },
        { "⌐", "sp_sc_flank",     "sp_sc_flank_d",     "EN 12354-1" },
        { "⫼", "sp_sc_duct",      "sp_sc_duct_d",      "VDI 2081 / ASHRAE" },
        { "⚙", "sp_sc_machine",   "sp_sc_machine_d",   "ISO 11546" },
        { "♻", "sp_sc_reverb",    "sp_sc_reverb_d",    "ISO 3382-2 / JIS A 1417" },
        { "🗣", "sp_sc_speech",    "sp_sc_speech_d",    "ASTM E1130 / ANSI S3.5" },
    };
    for (int i = 0; i < 8; ++i) {
        auto *b = new QPushButton(
            QString::fromUtf8(kSc[i].icon) + " " + I18n::tr(kSc[i].name) + "\n"
                + I18n::tr(kSc[i].desc) + "\n" + QString::fromUtf8(kSc[i].std),
            ss);
        b->setCheckable(true);
        b->setStyleSheet("text-align:left; padding:6px 10px;");
        m_scenarioGroup->addButton(b, i);
        grid->addWidget(b, i / 2, i % 2);
    }
    ss->vbox()->addLayout(grid);
    v->addWidget(ss);

    // シナリオ別ページ
    m_stack = new QStackedWidget(body);
    // シナリオ毎の控え (ページ数と同じ長さ。ページを足したらここも足す)
    m_spectra.resize(8);
    m_stack->addWidget(buildPartitionPage());   // 0 partition
    m_stack->addWidget(buildFacadePage());      // 1 facade
    m_stack->addWidget(buildFloorPage());       // 2 floor
    m_stack->addWidget(buildFlankingPage());    // 3 flanking
    m_stack->addWidget(buildDuctPage());        // 4 duct
    m_stack->addWidget(buildMachinePage());     // 5 machine
    m_stack->addWidget(buildReverbPage());      // 6 reverb
    m_stack->addWidget(buildSpeechPage());      // 7 speech
    v->addWidget(m_stack);

    // 出力 (全シナリオ共通)。**押せない理由は 3 つとも別**なので、まとめて
    // 同じ理由にしない (「同梱データが無い」で括ると、実際には帯域スペクトル
    // という出せるものがあることが見えなくなる)
    auto *se = new SectionBox(I18n::tr("sp_export_section"), body);
    auto *he = new QHBoxLayout();
    auto *repBtn   = new QPushButton(I18n::tr("sp_exp_report"), se);
    auto *csvBtn   = new QPushButton(I18n::tr("sp_exp_csv"), se);
    auto *auralBtn = new QPushButton(I18n::tr("sp_exp_aural"), se);
    auto *stdBtn   = new QPushButton(I18n::tr("sp_exp_std"), se);
    // PDF の作図には Qt PrintSupport が要る (依存を増やさない方針) うえ、
    // 設計レポートの様式そのものも決まっていない
    tabhelp::markNotImplemented(repBtn, I18n::tr(tabhelp::notimpl::kReport));
    // 試聴は音声出力を持たない方針 (QtMultimedia 不使用)
    tabhelp::markNotImplemented(auralBtn, I18n::tr(tabhelp::notimpl::kAudio));
    // ISO/ASTM の報告書式は有償規格の書式そのもので、参照できる公開仕様が無い
    tabhelp::markNotImplemented(stdBtn, I18n::tr(tabhelp::notimpl::kFormat));
    csvBtn->setToolTip(I18n::tr("sp_exp_csv_tip"));
    connect(csvBtn, &QPushButton::clicked, this,
            &SoundproofTab::exportSpectrumCsv);
    for (QPushButton *b : { repBtn, csvBtn, auralBtn, stdBtn })
        he->addWidget(b);
    he->addStretch(1);
    se->vbox()->addLayout(he);
    v->addWidget(se);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    connect(m_scenarioGroup, &QButtonGroup::idClicked, this, [this](int id) {
        // ローカル state のみ (Project に対応フィールド無し → 永続化しない)
        m_scenario = id;
        m_stack->setCurrentIndex(id);
    });

    connect(project, &Project::loaded, this, &SoundproofTab::refresh);
    refresh();
}

void SoundproofTab::refresh()
{
    m_updating = true;
    if (auto *b = m_scenarioGroup->button(m_scenario))
        b->setChecked(true);
    m_stack->setCurrentIndex(m_scenario);
    m_updating = false;
}

// ── 間仕切壁 (Airborne) ─────────────────────────────────────────────────────


// ── .dxf から仕切壁面積 S を読む ──────────────────────────────────────────
// 読み取りは io/DxfOutline (ENTITIES の LINE / LWPOLYLINE だけ)。ここは
// 単位と輪郭の選択を利用者に決めてもらい、結果と**読まなかったもの**を
// 画面に出すだけにする。図面を全部理解したように見せない。
// 表示中のシナリオの帯域スペクトルを CSV で書き出す。
// **図を描いたときに控えた値をそのまま書く** (書出のために計算し直さないので、
// 図と CSV が食い違うことがない)。
void SoundproofTab::exportSpectrumCsv()
{
    const int page = m_stack ? m_stack->currentIndex() : -1;
    const bool hasSpectrum = (page == 0 || page == 5);   // 間仕切壁 / 機器囲い
    if (!hasSpectrum) {
        // どのシナリオなら出せるのかまで書く (押せない理由を残さない)
        QMessageBox::information(this, I18n::tr("sp_exp_csv_title"),
                                 I18n::tr("sp_exp_csv_none"));
        return;
    }
    const io::BandSpectrum spec =
        (page >= 0 && page < m_spectra.size()) ? m_spectra[page]
                                               : io::BandSpectrum();
    const QString csv = io::buildBandSpectrumCsv(spec);
    if (csv.isEmpty()) {
        // 曲線が出ていない = 入力が足りない。空のファイルを作らない
        QMessageBox::information(this, I18n::tr("sp_exp_csv_title"),
                                 I18n::tr("sp_exp_csv_empty"));
        return;
    }
    const QString path = tabhelp::saveTextFile(
        this, I18n::tr("sp_exp_csv_title"), QStringLiteral("spectrum.csv"),
        QStringLiteral("CSV (*.csv);;All files (*)"), csv);
    if (path.isEmpty()) return;      // 取り消し / 失敗 — 成功を名乗らない
    QMessageBox::information(
        this, I18n::tr("sp_exp_csv_title"),
        I18n::tr("sp_exp_csv_ok")
            .arg(QFileInfo(path).fileName(), spec.scenario, spec.quantity,
                 QString::number(spec.freqHz.size()),
                 QString::number(spec.freqHz.first(), 'g', 4),
                 QString::number(spec.freqHz.last(), 'g', 4)));
}

void SoundproofTab::importDxfArea(QLineEdit *areaEdit, QLabel *status)
{
    if (!areaEdit || !status) return;
    // 自動実行 (--screenshot) では**モーダルを一切出さない**。押す人が居ない
    // ので、開けば待ち続けて CI が診断なしにタイムアウトする (過去に
    // 読み込み失敗の QMessageBox で 6 分 40 秒ハングした前例がある)。
    // 取込は単位と輪郭の選択に対話が要るので、自動実行では行わない
    if (MainWindow::automation()) {
        status->setVisible(true);
        status->setText(I18n::tr("sp_dxf_auto"));
        return;
    }
    const QString path = QFileDialog::getOpenFileName(
        this, I18n::tr("sp_dxf_title"), QString(), I18n::tr("sp_dxf_filter"));
    if (path.isEmpty()) return;

    ofd::DxfOutline dxf;
    QString err;
    if (!ofd::loadDxfOutline(path, &dxf, &err)) {
        status->setVisible(true);
        status->setText(I18n::tr("sp_dxf_fail").arg(err));
        return;
    }
    if (dxf.loops.isEmpty()) {
        status->setVisible(true);
        status->setText(I18n::tr("sp_dxf_noloop")
                            .arg(dxf.lineSegments)
                            .arg(dxf.openPolylines)
                            .arg(dxf.skippedEntities));
        return;
    }

    // 単位。$INSUNITS があればそれを使い、無ければ選ばせる
    // (推測で m とみなすと面積が 10^6 倍ずれる)
    ofd::DxfUnit unit = dxf.unit;
    if (unit == ofd::DxfUnit::Unknown) {
        const QStringList names{ "mm", "cm", "m", "inch", "ft" };
        bool ok = false;
        const QString pick = QInputDialog::getItem(
            this, I18n::tr("sp_dxf_title"), I18n::tr("sp_dxf_unit_ask"),
            names, 0, false, &ok);
        if (!ok) return;
        const int idx = names.indexOf(pick);
        static const ofd::DxfUnit kUnits[5] = {
            ofd::DxfUnit::Millimeter, ofd::DxfUnit::Centimeter,
            ofd::DxfUnit::Meter, ofd::DxfUnit::Inch, ofd::DxfUnit::Foot };
        unit = kUnits[qBound(0, idx, 4)];
    }
    const double k = ofd::dxfUnitToMeter(unit);
    if (k <= 0.0) return;

    // 輪郭が複数あるならどれが仕切壁かは図面からは決まらないので選ばせる
    int sel = 0;
    if (dxf.loops.size() > 1) {
        QStringList items;
        for (int i = 0; i < dxf.loops.size(); ++i)
            items << I18n::tr("sp_dxf_loop_item")
                         .arg(i + 1)
                         .arg(QString::number(dxf.loops[i].area * k * k,
                                              'f', 3))
                         .arg(dxf.loops[i].x.size());
        bool ok = false;
        const QString pick = QInputDialog::getItem(
            this, I18n::tr("sp_dxf_title"), I18n::tr("sp_dxf_pick"),
            items, 0, false, &ok);
        if (!ok) return;
        sel = qMax(0, items.indexOf(pick));
    }

    const ofd::DxfLoop &lp = dxf.loops[sel];
    const double area = lp.area * k * k;      // 図面単位² → m²
    status->setVisible(true);
    if (!(area > 0.0)) {
        status->setText(I18n::tr("sp_dxf_zero"));
        return;
    }
    areaEdit->setText(QString::number(area, 'f', 3));

    QString msg = I18n::tr("sp_dxf_done")
                      .arg(QString::number(area, 'f', 3),
                           QLatin1String(ofd::dxfUnitName(unit)))
                      .arg(sel + 1)
                      .arg(dxf.loops.size());
    if (lp.arcVertices > 0)
        msg += I18n::tr("sp_dxf_arc").arg(lp.arcVertices);
    if (dxf.skippedEntities > 0 || dxf.lineSegments > 0)
        msg += I18n::tr("sp_dxf_rest")
                   .arg(dxf.skippedEntities).arg(dxf.lineSegments);
    status->setText(msg);
    emit areaEdit->editingFinished();     // 評価結果を再計算させる
}

QWidget *SoundproofTab::buildPartitionPage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(8);

    // 送信室・受信室 (体積は寸法入力から計算する)
    auto *sr = new SectionBox(I18n::tr("sp_rooms_section"), page);
    auto *srcEdit = new QLineEdit(I18n::tr("sp_src_room_def"), sr);
    auto *srcVol = new QLabel(sr);
    auto *h1 = new QHBoxLayout();
    h1->addWidget(srcEdit, 1);
    h1->addWidget(srcVol);
    sr->form()->addRow(I18n::tr("sp_src_room"), h1);
    auto *rcvEdit = new QLineEdit(I18n::tr("sp_rcv_room_def"), sr);
    auto *rcvVol = new QLabel(sr);
    auto *h2 = new QHBoxLayout();
    h2->addWidget(rcvEdit, 1);
    h2->addWidget(rcvVol);
    sr->form()->addRow(I18n::tr("sp_rcv_room"), h2);
    auto *areaEdit = numEdit("12.15", sr);
    sr->form()->addRow(I18n::tr("sp_wall_area"), unitRow(areaEdit, "m²", sr));
    v->addWidget(sr);

    // 壁構造 (層テーブル)
    auto *sb = new SectionBox(I18n::tr("sp_build_section"), page);
    sb->vbox()->addWidget(makeHint(I18n::tr("sp_build_hint"), sb));
    auto *t = makeTable({ "", "#", I18n::tr("sp_h_material"),
                          I18n::tr("sp_h_thick"), I18n::tr("sp_h_density"),
                          I18n::tr("sp_h_surfdens"), I18n::tr("sp_h_young"),
                          I18n::tr("sp_h_eta") }, 7, sb, 230);
    enableTableEdit(t);   // 材質/厚さ/密度/E/η は編集可 (面密度・合計は計算列)
    populateLayerTable(t, kDefaultLayers, 5);
    sb->vbox()->addWidget(t);
    auto *hb = new QHBoxLayout();
    auto *presetBtn = new QPushButton(I18n::tr("sp_preset_btn"), sb);
    hb->addWidget(presetBtn);
    auto *dxfBtn = new QPushButton(I18n::tr("sp_dxf_btn"), sb);
    hb->addWidget(dxfBtn);
    hb->addStretch(1);
    // 参考値 (同種構造の公表 Rw)。計算値は評価結果セクションに出す。
    auto *rwRefLabel = new QLabel(I18n::tr("sp_rw_ref_none"), sb);
    hb->addWidget(rwRefLabel);
    sb->vbox()->addLayout(hb);
    // .dxf 取込の結果 (何をどう読んだか)。読めなかったものも必ず出す
    auto *dxfStatus = new QLabel(sb);
    dxfStatus->setWordWrap(true);
    dxfStatus->setStyleSheet("font-size:11px; color:palette(mid);");
    dxfStatus->setVisible(false);
    sb->vbox()->addWidget(dxfStatus);
    connect(dxfBtn, &QPushButton::clicked, this,
            [this, areaEdit, dxfStatus] { importDxfArea(areaEdit, dxfStatus); });
    v->addWidget(sb);

    // ディテール
    auto *sd = new SectionBox(I18n::tr("sp_detail_section"), page);
    auto *doubleChk = makeCheck(I18n::tr("sp_det_double"), true, sd);
    sd->vbox()->addWidget(doubleChk);
    sd->vbox()->addWidget(makeCheck(I18n::tr("sp_det_damp"), false, sd));
    sd->vbox()->addWidget(makeCheck(I18n::tr("sp_det_resil"), false, sd));
    auto *hSeal = new QHBoxLayout();
    hSeal->addWidget(makeCheck(I18n::tr("sp_det_seal"), true, sd));
    hSeal->addWidget(new QLabel(I18n::tr("sp_det_seal_note"), sd));
    hSeal->addStretch(1);
    sd->vbox()->addLayout(hSeal);
    sd->vbox()->addWidget(makeCheck(I18n::tr("sp_det_outlet"), false, sd));
    auto *hFrame = new QHBoxLayout();
    hFrame->addWidget(makeCheck(I18n::tr("sp_det_yes"), false, sd));
    hFrame->addWidget(new QLabel(I18n::tr("sp_det_frame_note"), sd));
    hFrame->addStretch(1);
    sd->form()->addRow(I18n::tr("sp_det_frame"), hFrame);
    // 計算に効くのは「二重壁構造」だけ — その旨を明示する (絶対規則 5)
    sd->vbox()->addWidget(makeHint(I18n::tr("sp_det_wired_note"), sd));
    v->addWidget(sd);

    // 音響透過損失 R(f) — 層構成からの計算結果
    auto *st = new SectionBox(I18n::tr("sp_tl_section"), page);
    auto *plot = new MiniPlot(st);
    plot->setLabels("f [Hz] (log)", "R [dB]");
    plot->setXTickPow10(true);
    plot->setYRange(0, 90);
    plot->setMinimumHeight(120);
    st->vbox()->addWidget(plot);
    auto *modelLbl = makeHint(QString(), st);
    st->vbox()->addWidget(modelLbl);
    auto *warnLbl = makeHint(QString(), st);
    warnLbl->setStyleSheet(QString("color:%1;").arg(kWarn));
    st->vbox()->addWidget(warnLbl);
    st->vbox()->addWidget(makeHint(I18n::tr("sp_tl_scope"), st));
    v->addWidget(st);

    // シングルナンバー評価
    auto *sg = new SectionBox(I18n::tr("sp_rating_section"), page);
    auto *hBadge = new QHBoxLayout();
    auto *rwBadge = makeBadge(QString(), kAcc, sg, true);
    auto *stcBadge = makeBadge(QString(), kAcc, sg, true);
    hBadge->addWidget(rwBadge);
    hBadge->addWidget(stcBadge);
    hBadge->addStretch(1);
    sg->vbox()->addLayout(hBadge);
    auto *rt = makeTable({ I18n::tr("sp_h_metric"), I18n::tr("sp_h_value"),
                           I18n::tr("sp_h_meaning") }, 6, sg, 200);
    struct Rating { const char *metric; bool trMetric; const char *meaning; };
    static const Rating kRatings[6] = {
        { "Rw",       false, "sp_m_rw"    },
        { "STC",      false, "sp_m_stc"   },
        { "sp_r_c",   true,  "sp_m_c"     },
        { "sp_r_ctr", true,  "sp_m_ctr"   },
        { "Rw+Ctr",   false, "sp_m_rwctr" },
        { "DnT,w",    false, "sp_m_dntw"  },
    };
    for (int i = 0; i < 6; ++i) {
        rt->setItem(i, 0, textItem(kRatings[i].trMetric
                                       ? I18n::tr(kRatings[i].metric)
                                       : QString::fromUtf8(kRatings[i].metric)));
        rt->setItem(i, 1, numItem("—"));
        rt->setItem(i, 2, textItem(I18n::tr(kRatings[i].meaning)));
    }
    sg->vbox()->addWidget(rt);
    sg->vbox()->addWidget(makeHint(I18n::tr("sp_rating_note"), sg));
    auto *hu = new QHBoxLayout();
    hu->addWidget(new QLabel(I18n::tr("sp_use_hint"), sg));
    hu->addWidget(makeBadge(I18n::tr("sp_use_hosp"), kOk, sg));
    hu->addWidget(makeBadge(I18n::tr("sp_use_dwell"), kOk, sg));
    hu->addWidget(makeBadge(I18n::tr("sp_use_office"), kWarn, sg));
    hu->addStretch(1);
    sg->vbox()->addLayout(hu);
    v->addWidget(sg);

    // ── 再計算 (層構成 → R(f) → Rw/STC/C/Ctr/DnT,w) ──────────────────────
    auto recompute = [this, t, doubleChk, plot, modelLbl, warnLbl, rwBadge, stcBadge,
                      rt, srcEdit, srcVol, rcvEdit, rcvVol, areaEdit]() {
        // 室容積 (寸法テキストの先頭 3 数値を L×W×H [m] とみなす)
        const double vSrc = parseVolume(srcEdit->text());
        const double vRcv = parseVolume(rcvEdit->text());
        srcVol->setText(vSrc > 0 ? I18n::tr("sp_vol_fmt")
                                       .arg(QString::number(vSrc, 'f', 1))
                                 : QString("—"));
        rcvVol->setText(vRcv > 0 ? I18n::tr("sp_vol_fmt")
                                       .arg(QString::number(vRcv, 'f', 1))
                                 : QString("—"));

        const std::vector<ins::Layer> layers = layersFromTable(t);
        const ins::TlResult tl =
            ins::transmissionLoss(layers, doubleChk->isChecked());

        auto clearAll = [&]() {
            plot->setSeries({});
            if (m_spectra.size() > 0) m_spectra[0] = io::BandSpectrum();
            modelLbl->setText(I18n::tr("sp_tl_none"));
            warnLbl->clear();
            rwBadge->setText("Rw = —");
            stcBadge->setText("STC —");
            for (int i = 0; i < 6; ++i)
                if (auto *it = rt->item(i, 1)) it->setText("—");
        };
        if (!tl.valid) { clearAll(); return; }

        MiniSeries s;
        s.color = QColor(kAccAcoustic);
        io::BandSpectrum spec;
        spec.scenario = I18n::tr("sp_sc_partition");
        spec.quantity = QStringLiteral("R");
        for (int i = 0; i < ins::kNumBands; ++i) {
            s.pts.push_back({ std::log10(ins::kThirdOctaveHz[i]), tl.R[i] });
            // 図と同じ値をそのまま控える (書出のために計算し直さない)
            spec.freqHz.push_back(ins::kThirdOctaveHz[i]);
            spec.value.push_back(tl.R[i]);
        }
        plot->setSeries({ s });
        if (m_spectra.size() > 0) m_spectra[0] = spec;

        if (tl.model == ins::ModelDoubleLeaf) {
            modelLbl->setText(
                I18n::tr("sp_tl_model_double")
                    .arg(QString::number(tl.leafMass[0], 'f', 1),
                         QString::number(tl.leafMass[1], 'f', 1),
                         QString::number(tl.cavityDepthM * 1000.0, 'f', 0),
                         QString::number(tl.massAirMassHz, 'f', 0),
                         QString::number(tl.limitingHz, 'f', 0)));
        } else {
            modelLbl->setText(
                tl.leafCriticalHz[0] > 0
                    ? I18n::tr("sp_tl_model_single")
                          .arg(QString::number(tl.surfaceMass, 'f', 1),
                               QString::number(tl.leafCriticalHz[0], 'f', 0),
                               QString::number(tl.lossFactor500, 'f', 3))
                    : I18n::tr("sp_tl_fc_none"));
        }
        QStringList warn;
        if (tl.model == ins::ModelDoubleLeaf) {
            warn << I18n::tr("sp_tl_scope_double");
            if (!tl.cavityAbsorbed) warn << I18n::tr("sp_tl_no_fill");
        }
        if (tl.reducedToTwoLeaves) warn << I18n::tr("sp_tl_reduced");
        warnLbl->setText(warn.join("\n"));

        const ins::RatingResult rw  = ins::weightedReduction(tl.R);
        const ins::RatingResult stc = ins::soundTransmissionClass(tl.R);
        rwBadge->setText(rw.valid ? QString("Rw = %1 dB").arg(rw.value)
                                  : QString("Rw = —"));
        stcBadge->setText(stc.valid ? QString("STC %1").arg(stc.value)
                                    : QString("STC —"));
        bool okC = false, okCtr = false;
        const int C   = rw.valid ? ins::spectrumAdaptation(tl.R, ins::SpectrumPink,
                                                           rw.value, &okC) : 0;
        const int Ctr = rw.valid ? ins::spectrumAdaptation(tl.R,
                                                           ins::SpectrumTraffic,
                                                           rw.value, &okCtr) : 0;
        bool okArea = false;
        const double S = areaEdit->text().toDouble(&okArea);
        const bool okDnT = rw.valid && okArea && S > 0 && vRcv > 0;
        const double dnt = okDnT
            ? ins::standardizedLevelDifference(rw.value, vRcv, S) : 0;

        const QString vals[6] = {
            rw.valid  ? QString("%1 dB").arg(rw.value)  : QString("—"),
            stc.valid ? QString::number(stc.value)      : QString("—"),
            okC       ? QString::number(C)              : QString("—"),
            okCtr     ? QString::number(Ctr)            : QString("—"),
            (rw.valid && okCtr) ? QString("%1 dB").arg(rw.value + Ctr)
                                : QString("—"),
            okDnT ? QString("%1 dB").arg(QString::number(dnt, 'f', 1))
                  : QString("—"),
        };
        for (int i = 0; i < 6; ++i)
            if (auto *it = rt->item(i, 1)) it->setText(vals[i]);
    };

    // 標準工法プリセット: 層構成を書込み、二重壁チェックと参考値も切替える
    auto *presetMenu = new QMenu(presetBtn);
    for (const WallPreset &preset : kWallPresets) {
        const WallPreset *p = &preset;   // 静的配列要素 → ポインタで値キャプチャ
        presetMenu->addAction(I18n::tr(p->name), this,
                              [t, rwRefLabel, doubleChk, recompute, p]() {
            populateLayerTable(t, p->layers, p->n);
            doubleChk->setChecked(p->decoupled);
            rwRefLabel->setText(I18n::tr("sp_rw_ref_fmt").arg(p->rw));
            recompute();
        });
    }
    presetBtn->setMenu(presetMenu);
    connect(t, &QTableWidget::itemChanged, this,
            [t, rwRefLabel, recompute](QTableWidgetItem *) {
                recomputeLayerTotals(t);
                // 手動編集後はプリセットの公表値と一致しなくなる
                rwRefLabel->setText(I18n::tr("sp_rw_ref_none"));
                recompute();
            });
    connect(doubleChk, &QCheckBox::toggled, this,
            [recompute](bool) { recompute(); });
    for (QLineEdit *e : { srcEdit, rcvEdit, areaEdit })
        connect(e, &QLineEdit::textChanged, this,
                [recompute](const QString &) { recompute(); });
    recompute();

    v->addStretch(1);
    return page;
}

// ── 外壁・窓 (Facade) ───────────────────────────────────────────────────────
QWidget *SoundproofTab::buildFacadePage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(8);

    auto *se = new SectionBox(I18n::tr("sp_ext_section"), page);
    auto *type = new QComboBox(se);
    type->addItems({ I18n::tr("sp_ext_road"), I18n::tr("sp_ext_rail"),
                     I18n::tr("sp_ext_air"), I18n::tr("sp_ext_constr"),
                     I18n::tr("sp_ext_custom") });
    se->form()->addRow(I18n::tr("sp_ext_type"), type);
    auto *lpEdit = numEdit("75", se);
    se->form()->addRow(I18n::tr("sp_ext_level"),
                       unitRow(lpEdit, I18n::tr("sp_ext_level_u"), se));
    auto *adaptEdit = numEdit("0", se);
    se->form()->addRow(I18n::tr("sp_ext_adapt"),
                       unitRow(adaptEdit, I18n::tr("sp_ext_adapt_u"), se));
    auto *adaptHint = makeHint(QString(), se);
    se->vbox()->addWidget(adaptHint);
    se->form()->addRow(I18n::tr("sp_ext_angle"),
                       unitRow(numEdit("45", se), I18n::tr("sp_ext_angle_u"), se));
    se->vbox()->addWidget(makeCheck(I18n::tr("sp_ext_diffuse"), true, se));
    se->vbox()->addWidget(makeHint(I18n::tr("sp_ext_unused"), se));
    v->addWidget(se);

    auto *sf = new SectionBox(I18n::tr("sp_facade_section"), page);
    auto *wallArea = numEdit("20", sf);
    sf->form()->addRow(I18n::tr("sp_fac_wall_area"), unitRow(wallArea, "m²", sf));
    auto *winArea = numEdit("8", sf);
    sf->form()->addRow(I18n::tr("sp_fac_win_area"), unitRow(winArea, "m²", sf));
    auto *wallRw = numEdit("55", sf);
    sf->form()->addRow(I18n::tr("sp_fac_wall_rw"), unitRow(wallRw, "dB", sf));
    auto *win = new QComboBox(sf);
    win->addItems({ I18n::tr("sp_win1"), I18n::tr("sp_win2"),
                    I18n::tr("sp_win3"), I18n::tr("sp_win4") });
    sf->form()->addRow(I18n::tr("sp_fac_win_type"), win);
    // 窓の Rw はコンボの表示値そのもの (製品の公表代表値)
    static const double kWinRw[4] = { 25, 30, 35, 40 };
    auto *hSash = new QHBoxLayout();
    hSash->addWidget(makeCheck(I18n::tr("sp_fac_sash_a4"), true, sf));
    hSash->addWidget(new QLabel(I18n::tr("sp_fac_sash_note"), sf));
    hSash->addStretch(1);
    sf->form()->addRow(I18n::tr("sp_fac_sash"), hSash);
    auto *ventChk = makeCheck(I18n::tr("sp_fac_vent"), false, sf);
    sf->vbox()->addWidget(ventChk);
    auto *ventArea = numEdit("0.01", sf);
    ventArea->setEnabled(false);
    sf->form()->addRow(I18n::tr("sp_fac_vent_area"), unitRow(ventArea, "m²", sf));
    v->addWidget(sf);

    auto *si = new SectionBox(I18n::tr("sp_indoor_section"), page);
    auto *rcvV = numEdit("60", si);
    si->form()->addRow(I18n::tr("sp_fac_rcv_v"), unitRow(rcvV, "m³", si));
    auto *rcvT = numEdit("0.5", si);
    si->form()->addRow(I18n::tr("sp_fac_rcv_t"), unitRow(rcvT, "s", si));
    auto *use = new QComboBox(si);
    use->addItems({ I18n::tr("sp_fac_use_dwell"), I18n::tr("sp_fac_use_hosp"),
                    I18n::tr("sp_fac_use_office") });
    si->form()->addRow(I18n::tr("sp_fac_use"), use);
    static const double kUseLimit[3] = { 40, 35, 45 };
    auto *hb = new QHBoxLayout();
    auto *lpBadge = makeBadge(QString(), kAcc, si, true);
    hb->addWidget(lpBadge);
    auto *okBadge = makeBadge(QString(), kOk, si);
    hb->addWidget(okBadge);
    hb->addStretch(1);
    si->vbox()->addLayout(hb);
    auto *detailLbl = makeHint(QString(), si);
    si->vbox()->addWidget(detailLbl);
    si->form()->addRow(I18n::tr("sp_indoor_ref"),
                       makeHint(I18n::tr("sp_indoor_ref_note"), si));
    si->vbox()->addWidget(makeHint(I18n::tr("sp_indoor_note"), si));
    v->addWidget(si);

    auto recompute = [type, lpEdit, adaptEdit, adaptHint, wallArea, winArea,
                      wallRw, win, ventChk, ventArea, rcvV, rcvT, use,
                      lpBadge, okBadge, detailLbl]() {
        // ISO 717-1: 道路交通騒音には Ctr、それ以外には C を適用する
        adaptHint->setText(I18n::tr(type->currentIndex() == 0
                                        ? "sp_ext_adapt_ctr"
                                        : "sp_ext_adapt_c"));
        ventArea->setEnabled(ventChk->isChecked());

        bool okLp = false, okAd = false, okSw = false, okSg = false,
             okRw = false, okV = false, okT = false, okVent = false;
        const double Lp1  = lpEdit->text().toDouble(&okLp);
        const double adapt = adaptEdit->text().toDouble(&okAd);
        const double Sw   = wallArea->text().toDouble(&okSw);
        const double Sg   = winArea->text().toDouble(&okSg);
        const double Rw   = wallRw->text().toDouble(&okRw);
        const double V    = rcvV->text().toDouble(&okV);
        const double T    = rcvT->text().toDouble(&okT);
        double Sv = ventArea->text().toDouble(&okVent);
        if (!ventChk->isChecked() || !okVent || Sv < 0) Sv = 0;

        const double Rg = kWinRw[win->currentIndex()];
        const double A = (okV && okT) ? ins::sabineAbsorption(V, T) : 0;
        const bool ready = okLp && okSw && okSg && okRw && Sw > 0 && Sg >= 0
                        && A > 0 && okAd;
        if (!ready) {
            lpBadge->setText(I18n::tr("sp_indoor_lp_fmt").arg("—"));
            okBadge->setVisible(false);
            detailLbl->setText(I18n::tr("sp_indoor_need"));
            return;
        }
        // 面積加重の τ 平均。適応項は壁・窓の Rw に加算してから合成する
        // (開口 = τ 1 は R をきわめて小さい値として与える)
        const double areas[3] = { Sw, Sg, Sv };
        const double Rs[3]    = { Rw + adapt, Rg + adapt, 0.0 };
        const double Rcomp = ins::compositeReduction(areas, Rs, 3);
        const double S = Sw + Sg + Sv;
        const double Lp2 = ins::receivingLevel(Lp1, Rcomp, S, A);
        lpBadge->setText(I18n::tr("sp_indoor_lp_fmt")
                             .arg(QString::number(Lp2, 'f', 1)));
        const double limit = kUseLimit[use->currentIndex()];
        const bool met = Lp2 <= limit;
        okBadge->setVisible(true);
        okBadge->setText(I18n::tr(met ? "sp_indoor_ok" : "sp_indoor_ng"));
        styleBadge(okBadge, met ? kOk : kWarn);
        detailLbl->setText(I18n::tr("sp_indoor_rcomp_fmt")
                               .arg(QString::number(Rcomp, 'f', 1),
                                    QString::number(Sw, 'f', 1),
                                    QString::number(Sg, 'f', 1),
                                    QString::number(Sv, 'f', 3),
                                    QString::number(adapt, 'f', 0),
                                    QString::number(A, 'f', 1)));
    };
    for (QLineEdit *e : { lpEdit, adaptEdit, wallArea, winArea, wallRw,
                          ventArea, rcvV, rcvT })
        connect(e, &QLineEdit::textChanged, this,
                [recompute](const QString &) { recompute(); });
    for (QComboBox *c : { type, win, use })
        connect(c, &QComboBox::currentIndexChanged, this,
                [recompute](int) { recompute(); });
    connect(ventChk, &QCheckBox::toggled, this,
            [recompute](bool) { recompute(); });
    recompute();

    v->addStretch(1);
    return page;
}

// ── 床衝撃音 (Impact) ───────────────────────────────────────────────────────
QWidget *SoundproofTab::buildFloorPage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(8);

    auto *sf = new SectionBox(I18n::tr("sp_floor_section"), page);
    auto *fin = new QComboBox(sf);
    fin->addItems({ I18n::tr("sp_fin_carpet"), I18n::tr("sp_fin_cushion"),
                    I18n::tr("sp_fin_iso"), I18n::tr("sp_fin_std"),
                    I18n::tr("sp_fin_tile") });
    sf->form()->addRow(I18n::tr("sp_floor_finish"), fin);
    // コンボ表示と同じ ΔLw (床仕上げによる衝撃音レベル低減量, ISO 717-2)
    static const double kDeltaLw[5] = { 25, 18, 15, 0, -5 };
    auto *base = new QComboBox(sf);
    base->addItems({ I18n::tr("sp_base_rc150"), I18n::tr("sp_base_rc200"),
                     I18n::tr("sp_base_wood") });
    sf->form()->addRow(I18n::tr("sp_floor_base"), base);
    auto *ceil = new QComboBox(sf);
    ceil->addItems({ I18n::tr("sp_ceil_direct"), I18n::tr("sp_ceil_susp"),
                     I18n::tr("sp_ceil_damp") });
    sf->form()->addRow(I18n::tr("sp_floor_ceil"), ceil);
    auto *bareEdit = numEdit("", sf, 100);
    sf->form()->addRow(I18n::tr("sp_floor_bare"),
                       unitRow(bareEdit, I18n::tr("sp_floor_bare_u"), sf));
    sf->vbox()->addWidget(makeHint(I18n::tr("sp_floor_unwired"), sf));
    v->addWidget(sf);

    auto *si = new SectionBox(I18n::tr("sp_impact_section"), page);
    auto *src = new QComboBox(si);
    src->addItems({ I18n::tr("sp_imp_tap"), I18n::tr("sp_imp_ball"),
                    I18n::tr("sp_imp_tire"), I18n::tr("sp_imp_step"),
                    I18n::tr("sp_imp_drop") });
    si->form()->addRow(I18n::tr("sp_impact_std"), src);
    v->addWidget(si);

    auto *sr = new SectionBox(I18n::tr("sp_floor_result"), page);
    auto *hb = new QHBoxLayout();
    auto *lnwBadge = makeBadge(QString(), kAcc, sr, true);
    hb->addWidget(lnwBadge);
    hb->addWidget(makeBadge("IIC —", kWarn, sr, true));
    hb->addStretch(1);
    sr->vbox()->addLayout(hb);
    auto *calcLbl = makeHint(QString(), sr);
    sr->vbox()->addWidget(calcLbl);
    auto *hj = new QHBoxLayout();
    hj->addWidget(new QLabel(I18n::tr("sp_jis_grade"), sr));
    hj->addWidget(makeBadge("—", kWarn, sr));
    hj->addStretch(1);
    sr->vbox()->addLayout(hj);
    // IIC / JIS 等級はスペクトルが要るため未計算 — 何が必要かを明示する
    sr->vbox()->addWidget(makeHint(I18n::tr("sp_floor_iic_none"), sr));
    v->addWidget(sr);

    auto recompute = [fin, bareEdit, lnwBadge, calcLbl]() {
        bool okBare = false;
        const double bare = bareEdit->text().toDouble(&okBare);
        const double dLw = kDeltaLw[fin->currentIndex()];
        if (!okBare || bare <= 0) {
            lnwBadge->setText(I18n::tr("sp_floor_lnw_none"));
            calcLbl->setText(I18n::tr("sp_floor_need"));
            return;
        }
        const double lnw = bare - dLw;
        lnwBadge->setText(I18n::tr("sp_floor_lnw_fmt")
                              .arg(QString::number(lnw, 'f', 1)));
        calcLbl->setText(I18n::tr("sp_floor_calc_fmt")
                             .arg(QString::number(bare, 'f', 1),
                                  QString::number(dLw, 'f', 0)));
    };
    connect(bareEdit, &QLineEdit::textChanged, this,
            [recompute](const QString &) { recompute(); });
    connect(fin, &QComboBox::currentIndexChanged, this,
            [recompute](int) { recompute(); });
    recompute();

    v->addStretch(1);
    return page;
}

// ── 側路伝搬 (Flanking) ─────────────────────────────────────────────────────
QWidget *SoundproofTab::buildFlankingPage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(8);

    auto *sp = new SectionBox(I18n::tr("sp_flank_section"), page);
    sp->vbox()->addWidget(makeHint(I18n::tr("sp_flank_hint"), sp));
    auto *t = makeTable({ "", I18n::tr("sp_h_path"), I18n::tr("sp_h_desc"),
                          "R [dB]" }, 5, sp, 180);
    struct Path { const char *path; bool trPath; const char *desc;
                  const char *r; };
    static const Path kPaths[5] = {
        { "Dd",           false, "sp_d_dd",  "52" },
        { "sp_p_ff_floor", true, "sp_d_ff",  "58" },
        { "sp_p_df_floor", true, "sp_d_df",  "62" },
        { "sp_p_fd_ceil",  true, "sp_d_fd",  "60" },
        { "sp_p_ff_col",   true, "sp_d_col", "65" },
    };
    enableTableEdit(t);   // R [dB] 列のみ編集可の入力テーブル
    for (int i = 0; i < 5; ++i) {
        t->setItem(i, 0, checkItem(true));
        t->setItem(i, 1, lockItem(textItem(kPaths[i].trPath
                                      ? I18n::tr(kPaths[i].path)
                                      : QString::fromUtf8(kPaths[i].path))));
        t->setItem(i, 2, lockItem(textItem(I18n::tr(kPaths[i].desc))));
        t->setItem(i, 3, numItem(kPaths[i].r));
    }
    sp->vbox()->addWidget(t);
    auto *hb = new QHBoxLayout();
    auto *totalBadge = makeBadge(QString(), kAcc, sp, true);
    hb->addWidget(totalBadge);
    auto *noteLbl = new QLabel(sp);
    hb->addWidget(noteLbl);
    hb->addStretch(1);
    sp->vbox()->addLayout(hb);
    // 改善案 — 各項目の改善量と、それが効く経路 (表の行番号) はモックの
    // 文言そのまま。梁の弾性分離だけは表に「梁」の行が無いので、接合部の
    // 経路である Df (壁→床→壁) に計上する (注記で明示する)。
    struct Improvement { const char *key; int row; double delta_dB; };
    static const Improvement kImprovements[4] = {
        { "sp_impr_float",   1, 8.0 },   // 床: 浮き床 → Ff (床)
        { "sp_impr_hanger",  3, 6.0 },   // 天井: 防振吊金具 → Fd (天井)
        { "sp_impr_tape",    4, 3.0 },   // 柱: 制振テープ → Ff (柱)
        { "sp_impr_elastic", 2, 5.0 },   // 梁: 弾性分離 → Df (壁→床→壁)
    };
    auto *si = new SectionBox(I18n::tr("sp_improve_section"), page);
    QVector<QCheckBox*> imprBoxes;
    for (const Improvement &im : kImprovements) {
        auto *c = makeCheck(I18n::tr(im.key), false, si);
        imprBoxes.push_back(c);
        si->vbox()->addWidget(c);
    }
    auto *afterLbl = new QLabel(si);
    afterLbl->setWordWrap(true);
    auto *weakLbl = new QLabel(si);
    weakLbl->setWordWrap(true);

    // 合成 R'w = −10·log10(Σ 10^(−R_i/10)) を、チェック ON の経路の入力 R
    // からエネルギー合成で計算する (core/FlankingTransmission)。
    // 経路別 R そのものの予測 (Kij) は未実装 — 下の注記で明示する。
    auto recompute = [t, totalBadge, noteLbl, afterLbl, weakLbl, imprBoxes]() {
        std::vector<flanking::Path> paths;
        paths.reserve(t->rowCount());
        double direct = 0;
        bool haveDirect = false;
        for (int r = 0; r < t->rowCount(); ++r) {
            flanking::Path pth;
            const QTableWidgetItem *chk = t->item(r, 0);
            bool ok = false;
            const double R = cellNum(t, r, 3, &ok);
            // 数値でない R の行・チェック OFF の行は合成に含めない
            pth.enabled = ok && chk && chk->checkState() == Qt::Checked;
            pth.R_dB = ok ? R : 0.0;
            paths.push_back(pth);
            if (r == 0 && pth.enabled) { direct = R; haveDirect = true; }
        }
        // 改善量を対応する経路へ (表の入力値そのものは書き換えない)
        for (int i = 0; i < imprBoxes.size(); ++i) {
            if (!imprBoxes[i]->isChecked()) continue;
            const int row = kImprovements[i].row;
            if (row >= 0 && row < int(paths.size()))
                paths[size_t(row)].deltaR_dB += kImprovements[i].delta_dB;
        }

        const flanking::Combined c = flanking::combine(paths);
        if (!c.valid) {
            totalBadge->setText(I18n::tr("sp_flank_total_fmt").arg("—"));
            noteLbl->clear();
            afterLbl->clear();
            weakLbl->clear();
            return;
        }
        totalBadge->setText(I18n::tr("sp_flank_total_fmt")
                                .arg(QString::number(c.base_dB, 'f', 1)));
        // 直接透過のみとの比較 (Dd がチェック ON で側路もあるときのみ)
        noteLbl->setText(haveDirect && c.paths > 1
                             ? I18n::tr("sp_flank_note_fmt")
                                   .arg(QString::number(direct, 'g', 4),
                                        QString::number(direct - c.base_dB, 'f', 1))
                             : QString());
        if (c.gain_dB > 0.0) {
            afterLbl->setText(I18n::tr("sp_impr_after_fmt")
                                  .arg(QString::number(c.rw_dB, 'f', 1),
                                       QString::number(c.gain_dB, 'f', 1)));
            // 合成は最も弱い経路で頭打ちになる — どこが効いていないかを言う
            const int w = c.weakestIndex;
            const QTableWidgetItem *nameIt = (w >= 0) ? t->item(w, 1) : nullptr;
            weakLbl->setText(nameIt
                ? I18n::tr("sp_impr_weakest_fmt")
                      .arg(nameIt->text(),
                           QString::number(paths[size_t(w)].R_dB
                                               + paths[size_t(w)].deltaR_dB,
                                           'f', 1))
                : QString());
        } else {
            afterLbl->setText(I18n::tr("sp_impr_none"));
            weakLbl->clear();
        }
    };
    connect(t, &QTableWidget::itemChanged, this,
            [recompute](QTableWidgetItem *) { recompute(); });
    for (QCheckBox *c : imprBoxes)
        connect(c, &QCheckBox::toggled, this, [recompute](bool) { recompute(); });
    sp->vbox()->addWidget(makeHint(I18n::tr("sp_flank_pred_note"), sp));
    v->addWidget(sp);

    si->vbox()->addWidget(afterLbl);
    si->vbox()->addWidget(weakLbl);
    si->vbox()->addWidget(makeHint(I18n::tr("sp_impr_note"), si));
    // 改善案と経路の R は合成へ入る。残る未実装は経路別 R の予測 (Kij)。
    si->vbox()->addWidget(tabhelp::unwiredNote(si, I18n::tr("sp_uw_flank_kij"),
                                               I18n::tr("sp_uw_flank_ok")));
    auto *hr = new QHBoxLayout();
    auto *recalcBtn = new QPushButton(I18n::tr("sp_recalc_btn"), si);
    connect(recalcBtn, &QPushButton::clicked, this,
            [recompute] { recompute(); });
    hr->addWidget(recalcBtn);
    hr->addStretch(1);
    si->vbox()->addLayout(hr);
    v->addWidget(si);
    recompute();

    v->addStretch(1);
    return page;
}

// ── ダクト・配管音 ──────────────────────────────────────────────────────────
QWidget *SoundproofTab::buildDuctPage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(8);

    auto *sd = new SectionBox(I18n::tr("sp_duct_section"), page);
    auto *shape = new QComboBox(sd);
    shape->addItems({ I18n::tr("sp_duct_round"), I18n::tr("sp_duct_rect"),
                      I18n::tr("sp_duct_flex") });
    shape->setCurrentIndex(1);   // mock: value="rect"
    sd->form()->addRow(I18n::tr("sp_duct_shape"), shape);
    auto *sect = new QLineEdit("400 × 250 mm", sd);
    sd->form()->addRow(I18n::tr("sp_duct_sect"), sect);
    sd->vbox()->addWidget(makeHint(I18n::tr("sp_duct_sect_hint"), sd));
    auto *lenEdit = numEdit("12", sd, 100);
    sd->form()->addRow(I18n::tr("sp_duct_len"), unitRow(lenEdit, "m", sd));
    auto *elbowEdit = numEdit("3", sd);
    sd->form()->addRow(I18n::tr("sp_duct_elbow"),
                       unitRow(elbowEdit, I18n::tr("sp_duct_elbow_u"), sd));
    auto *branchEdit = numEdit("1.0", sd);
    sd->form()->addRow(I18n::tr("sp_duct_branch"),
                       unitRow(branchEdit, I18n::tr("sp_duct_branch_u"), sd));
    auto *hd = new QHBoxLayout();
    hd->addWidget(numEdit("2", sd));
    hd->addWidget(new QLabel(I18n::tr("sp_duct_damper_note"), sd));
    hd->addStretch(1);
    sd->form()->addRow(I18n::tr("sp_duct_damper"), hd);
    v->addWidget(sd);

    auto *sa = new SectionBox(I18n::tr("sp_atten_section"), page);
    auto *silChk = makeCheck(I18n::tr("sp_att_silencer"), true, sa);
    auto *silEdit = numEdit("15", sa);
    auto *h1 = new QHBoxLayout();
    h1->addWidget(silChk);
    h1->addWidget(silEdit);
    h1->addWidget(new QLabel("dB", sa));
    h1->addStretch(1);
    sa->vbox()->addLayout(h1);
    auto *linChk = makeCheck(I18n::tr("sp_att_lining"), true, sa);
    sa->vbox()->addWidget(linChk);
    sa->vbox()->addWidget(makeHint(I18n::tr("sp_att_alpha_note"), sa));
    auto *h3 = new QHBoxLayout();
    h3->addWidget(makeCheck(I18n::tr("sp_att_isolator"), true, sa));
    h3->addWidget(new QLabel(I18n::tr("sp_att_isolator_note"), sa));
    h3->addStretch(1);
    sa->vbox()->addLayout(h3);
    v->addWidget(sa);

    auto *si = new SectionBox(I18n::tr("sp_duct_indoor"), page);
    auto *roomV = numEdit("150", si);
    si->form()->addRow(I18n::tr("sp_fac_rcv_v"), unitRow(roomV, "m³", si));
    auto *roomT = numEdit("0.6", si);
    si->form()->addRow(I18n::tr("sp_fac_rcv_t"), unitRow(roomT, "s", si));
    auto *bt = makeTable({ I18n::tr("sp_h_band"), I18n::tr("sp_h_pwl"),
                           I18n::tr("sp_h_alpha"), I18n::tr("sp_h_atten"),
                           I18n::tr("sp_h_lp") },
                         7, si, 200);
    enableTableEdit(bt);   // PWL 列と α 列が入力 (減衰・Lp は計算列)
    for (int i = 0; i < 7; ++i) {
        bt->setItem(i, 0, lockItem(numItem(QString::number(kOctHz[i], 'g', 5))));
        bt->setItem(i, 1, numItem(""));       // 既定は空 = 未入力
        bt->setItem(i, 2, numItem(QString::number(kLiningAlpha[i], 'g', 3)));
        bt->setItem(i, 3, lockItem(numItem("—")));
        bt->setItem(i, 4, lockItem(numItem("—")));
    }
    si->vbox()->addWidget(bt);
    auto *hs = new QHBoxLayout();
    auto *splBadge = makeBadge(QString(), kAcc, si, true);
    hs->addWidget(splBadge);
    auto *ncBadge = makeBadge(QString(), kOk, si);
    hs->addWidget(ncBadge);
    hs->addStretch(1);
    si->vbox()->addLayout(hs);
    auto *statusLbl = makeHint(QString(), si);
    si->vbox()->addWidget(statusLbl);
    si->vbox()->addWidget(makeHint(I18n::tr("sp_duct_note"), si));
    v->addWidget(si);

    auto recompute = [shape, sect, lenEdit, elbowEdit, branchEdit, silChk,
                      silEdit, linChk, roomV, roomT, bt, splBadge,
                      ncBadge, statusLbl]() {
        QSignalBlocker blk(bt);   // 計算列の書込で itemChanged を再発火させない
        // 断面 (角形は 幅×高さ mm、円形/フレキは 直径 mm)
        double dims[2] = { 0, 0 };
        const int nd = parseNumbers(sect->text(), dims, 2);
        const bool rect = (shape->currentIndex() == 1);
        double area = 0, perim = 0, width = 0;
        if (rect && nd >= 2 && dims[0] > 0 && dims[1] > 0) {
            const double w = dims[0] / 1000.0, h = dims[1] / 1000.0;
            area = w * h;
            perim = 2.0 * (w + h);
            width = w;
        } else if (!rect && nd >= 1 && dims[0] > 0) {
            const double d = dims[0] / 1000.0;
            area = 3.14159265358979323846 * d * d / 4.0;
            perim = 3.14159265358979323846 * d;
            width = d;
        }
        bool okLen = false, okElb = false, okBr = false, okSil = false,
             okV = false, okT = false;
        const double len = lenEdit->text().toDouble(&okLen);
        const double nElb = elbowEdit->text().toDouble(&okElb);
        const double brRatio = branchEdit->text().toDouble(&okBr);
        const double silIl = silEdit->text().toDouble(&okSil);
        const double V = roomV->text().toDouble(&okV);
        const double T = roomT->text().toDouble(&okT);
        const double A = (okV && okT) ? ins::sabineAbsorption(V, T) : 0;

        // 帯域ごとの減衰 (ASHRAE)
        double att[7] = { 0, 0, 0, 0, 0, 0, 0 };
        const bool geomOk = area > 0 && perim > 0;
        for (int i = 0; i < 7; ++i) {
            double a = 0;
            bool okAl = false;
            const double alpha = cellNum(bt, i, 2, &okAl);
            if (geomOk && linChk->isChecked() && okAl && alpha > 0 && okLen
                && len > 0) {
                // Sabine の式。ASHRAE の実務にならい合計 40 dB で頭打ち
                a += std::min(40.0,
                              ins::linedDuctAttenuation(alpha, perim, area) * len);
            }
            if (geomOk && okElb && nElb > 0)
                a += nElb * ins::elbowAttenuation(kOctHz[i], width,
                                                  linChk->isChecked());
            if (okBr && brRatio > 0 && brRatio < 1.0)
                a += ins::branchAttenuation(brRatio, 1.0);
            if (geomOk)
                a += ins::endReflectionLoss(kOctHz[i], area, true);
            if (silChk->isChecked() && okSil && silIl > 0)
                a += silIl;
            att[i] = a;
        }

        // PWL 入力 → 帯域 Lp → dB(A) と NC
        double lp[7];
        bool allPwl = true;
        for (int i = 0; i < 7; ++i) {
            bool ok = false;
            const double pwl = cellNum(bt, i, 1, &ok);
            if (auto *it = bt->item(i, 3))
                it->setText(geomOk ? QString::number(att[i], 'f', 1)
                                   : QString("—"));
            if (!ok || A <= 0 || !geomOk) {
                allPwl = false;
                lp[i] = 0;
                if (auto *it = bt->item(i, 4)) it->setText("—");
                continue;
            }
            lp[i] = ins::reverberantLevel(pwl - att[i], A);
            if (auto *it = bt->item(i, 4))
                it->setText(QString::number(lp[i], 'f', 1));
        }
        if (!allPwl) {
            splBadge->setText(I18n::tr("sp_duct_lpa_fmt").arg("—"));
            ncBadge->setVisible(false);
            statusLbl->setText(I18n::tr("sp_duct_need"));
            return;
        }
        double sumA = 0;
        for (int i = 0; i < 7; ++i)
            sumA += std::pow(10.0, (lp[i] + kAWeight[i]) / 10.0);
        const double lpa = 10.0 * std::log10(std::max(1e-12, sumA));
        splBadge->setText(I18n::tr("sp_duct_lpa_fmt")
                              .arg(QString::number(lpa, 'f', 1)));
        const int nc = roomac::ncRating(lp);
        ncBadge->setVisible(true);
        ncBadge->setText(I18n::tr("sp_duct_nc_fmt").arg(nc));
        styleBadge(ncBadge, nc <= 35 ? kOk : kWarn);
        statusLbl->clear();
    };
    connect(bt, &QTableWidget::itemChanged, this,
            [recompute](QTableWidgetItem *) { recompute(); });
    for (QLineEdit *e : { sect, lenEdit, elbowEdit, branchEdit, silEdit,
                          roomV, roomT })
        connect(e, &QLineEdit::textChanged, this,
                [recompute](const QString &) { recompute(); });
    connect(shape, &QComboBox::currentIndexChanged, this,
            [recompute](int) { recompute(); });
    for (QCheckBox *c : { silChk, linChk })
        connect(c, &QCheckBox::toggled, this, [recompute](bool) { recompute(); });
    recompute();

    v->addStretch(1);
    return page;
}

// ── 設備機器囲い ────────────────────────────────────────────────────────────
QWidget *SoundproofTab::buildMachinePage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(8);

    auto *sm = new SectionBox(I18n::tr("sp_mach_section"), page);
    auto *dev = new QComboBox(sm);
    dev->addItems({ I18n::tr("sp_dev_ac"), I18n::tr("sp_dev_gen"),
                    I18n::tr("sp_dev_comp"), I18n::tr("sp_dev_hp"),
                    I18n::tr("sp_dev_custom") });
    sm->form()->addRow(I18n::tr("sp_mach_dev"), dev);
    auto *srcEdit = numEdit("85", sm);
    sm->form()->addRow(I18n::tr("sp_mach_level"),
                       unitRow(srcEdit, "dB(A) @1m", sm));
    auto *sizeEdit = new QLineEdit("1.5 × 1.5 × 1.2 m", sm);
    sm->form()->addRow(I18n::tr("sp_mach_size"), sizeEdit);
    auto *wall = new QComboBox(sm);
    wall->addItems({ I18n::tr("sp_mwall1"), I18n::tr("sp_mwall2") });
    sm->form()->addRow(I18n::tr("sp_mach_wall"), wall);
    // 囲いの壁 = 制振鋼板の単板 (内側の吸音材は内部吸音 ᾱ として扱う)
    // 鋼板: ρ = 7850 kg/m³, E = 210 GPa。制振材塗布で η_int ≈ 0.05。
    struct MachWall { double thMm; double eta; };
    static const MachWall kMachWalls[2] = { { 1.6, 0.05 }, { 2.3, 0.05 } };
    auto *alphaEdit = numEdit("0.5", sm);
    sm->form()->addRow(I18n::tr("sp_mach_alpha"), unitRow(alphaEdit, "", sm));
    auto *nOpen = numEdit("2", sm);
    auto *aOpen = numEdit("0.04", sm);
    auto *ho = new QHBoxLayout();
    ho->addWidget(nOpen);
    ho->addWidget(new QLabel("×", sm));
    ho->addWidget(aOpen);
    ho->addWidget(new QLabel(I18n::tr("sp_mach_open_u"), sm));
    ho->addStretch(1);
    sm->form()->addRow(I18n::tr("sp_mach_open"), ho);
    v->addWidget(sm);

    auto *si = new SectionBox(I18n::tr("sp_il_section"), page);
    auto *plot = new MiniPlot(si);
    plot->setLabels("f [Hz] (log)", "IL [dB]");
    plot->setXTickPow10(true);
    plot->setYRange(0, 60);
    plot->setMinimumHeight(110);
    si->vbox()->addWidget(plot);
    auto *hb = new QHBoxLayout();
    auto *ilBadge = makeBadge(QString(), kAcc, si, true);
    hb->addWidget(ilBadge);
    hb->addStretch(1);
    si->vbox()->addLayout(hb);
    auto *geomLbl = makeHint(QString(), si);
    si->vbox()->addWidget(geomLbl);
    auto *srcLbl = makeHint(QString(), si);
    si->vbox()->addWidget(srcLbl);
    si->vbox()->addWidget(makeHint(I18n::tr("sp_il_note"), si));
    v->addWidget(si);

    auto recompute = [this, sizeEdit, wall, alphaEdit, nOpen, aOpen, srcEdit,
                      plot, ilBadge, geomLbl, srcLbl]() {
        double d[3] = { 0, 0, 0 };
        const bool okDim = parseNumbers(sizeEdit->text(), d, 3) == 3
                        && d[0] > 0 && d[1] > 0 && d[2] > 0;
        // 直方体の表面積 (設置面も遮音面とみなす)
        const double S = okDim ? 2.0 * (d[0] * d[1] + d[0] * d[2] + d[1] * d[2])
                               : 0;
        bool okN = false, okA = false, okAl = false;
        const double n = nOpen->text().toDouble(&okN);
        const double a = aOpen->text().toDouble(&okA);
        const double alpha = alphaEdit->text().toDouble(&okAl);
        const double Sopen = (okN && okA && n > 0 && a > 0) ? n * a : 0;
        const double Swall = S - Sopen;

        const MachWall &mw = kMachWalls[wall->currentIndex()];
        std::vector<ins::Layer> layers;
        ins::Layer steel;
        steel.thicknessM  = mw.thMm / 1000.0;
        steel.densityKgM3 = 7850.0;
        steel.youngsPa    = 210e9;
        steel.poisson     = 0.3;
        steel.lossFactor  = mw.eta;
        layers.push_back(steel);
        const ins::TlResult tl = ins::transmissionLoss(layers, false);

        if (!okDim || Swall <= 0 || !okAl || alpha <= 0 || !tl.valid) {
            plot->setSeries({});
            if (m_spectra.size() > 5) m_spectra[5] = io::BandSpectrum();
            ilBadge->setText(I18n::tr("sp_il_none"));
            geomLbl->clear();
            srcLbl->clear();
            return;
        }
        MiniSeries s;
        s.color = QColor(kAccAcoustic);
        io::BandSpectrum spec;
        spec.scenario = I18n::tr("sp_sc_machine");
        spec.quantity = QStringLiteral("IL");
        double il500 = 0;
        for (int i = 0; i < ins::kNumBands; ++i) {
            const double il = ins::enclosureInsertionLoss(tl.R[i], Swall,
                                                          Sopen, alpha);
            s.pts.push_back({ std::log10(ins::kThirdOctaveHz[i]), il });
            if (ins::kThirdOctaveHz[i] == 500) il500 = il;
            spec.freqHz.push_back(ins::kThirdOctaveHz[i]);
            spec.value.push_back(il);
        }
        plot->setSeries({ s });
        if (m_spectra.size() > 5) m_spectra[5] = spec;
        ilBadge->setText(I18n::tr("sp_il_badge_fmt")
                             .arg(QString::number(il500, 'f', 1)));
        geomLbl->setText(I18n::tr("sp_il_geom_fmt")
                             .arg(QString::number(S, 'f', 2),
                                  QString::number(Sopen, 'f', 3),
                                  QString::number(100.0 * Sopen / S, 'f', 2),
                                  QString::number(tl.surfaceMass, 'f', 1)));
        bool okSrc = false;
        const double lw = srcEdit->text().toDouble(&okSrc);
        srcLbl->setText(okSrc ? I18n::tr("sp_il_srcnote_fmt")
                                    .arg(QString::number(lw, 'f', 1),
                                         QString::number(il500, 'f', 1),
                                         QString::number(lw - il500, 'f', 1))
                              : QString());
    };
    for (QLineEdit *e : { sizeEdit, alphaEdit, nOpen, aOpen, srcEdit })
        connect(e, &QLineEdit::textChanged, this,
                [recompute](const QString &) { recompute(); });
    connect(wall, &QComboBox::currentIndexChanged, this,
            [recompute](int) { recompute(); });
    recompute();

    v->addStretch(1);
    return page;
}

// ── 室内残響対策 ────────────────────────────────────────────────────────────
QWidget *SoundproofTab::buildReverbPage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(8);

    auto *sr = new SectionBox(I18n::tr("sp_rev_section"), page);
    auto *room = new QComboBox(sr);
    room->addItems({ I18n::tr("sp_room_office"), I18n::tr("sp_room_gym"),
                     I18n::tr("sp_room_cafe"), I18n::tr("sp_room_class"),
                     I18n::tr("sp_room_rest") });
    sr->form()->addRow(I18n::tr("sp_rev_room"), room);
    auto *sizeEdit = new QLineEdit(I18n::tr("sp_rev_size_def"), sr);
    auto *volLabel = new QLabel(sr);   // 寸法から計算した体積を表示
    auto *hSize = new QHBoxLayout();
    hSize->addWidget(sizeEdit, 1);
    hSize->addWidget(volLabel);
    sr->form()->addRow(I18n::tr("sp_rev_size"), hSize);
    auto *targetEdit = numEdit("0.8", sr);
    sr->form()->addRow(I18n::tr("sp_rev_target"),
                       unitRow(targetEdit, I18n::tr("sp_rev_target_u"), sr));
    v->addWidget(sr);

    auto *sa = new SectionBox(I18n::tr("sp_abs_section"), page);
    auto *t = makeTable({ "", I18n::tr("sp_h_surface"), I18n::tr("sp_h_area"),
                          I18n::tr("sp_h_mat"), "α @1kHz", "NRC" }, 4, sa, 160);
    struct Abs { bool on; const char *surf; const char *area; const char *mat;
                 const char *alpha; const char *nrc; };
    static const Abs kAbs[4] = {
        { true,  "sp_s_ceiling",  "300", "sp_mat_t15",    "0.85", "0.85" },
        { true,  "sp_s_wall_up",  "122", "sp_mat_abs",    "0.75", "0.75" },
        { false, "sp_s_wall_low", "122", "sp_mat_wood",   "0.20", "0.15" },
        { true,  "sp_s_floor",    "300", "sp_mat_carpet", "0.35", "0.30" },
    };
    enableTableEdit(t);   // 面積・α@1kHz は編集可の入力 (RT60 計算に使う)
    for (int i = 0; i < 4; ++i) {
        t->setItem(i, 0, checkItem(kAbs[i].on));
        t->setItem(i, 1, lockItem(textItem(I18n::tr(kAbs[i].surf))));
        t->setItem(i, 2, numItem(kAbs[i].area));
        t->setItem(i, 3, lockItem(textItem(I18n::tr(kAbs[i].mat))));
        t->setItem(i, 4, numItem(kAbs[i].alpha));
        t->setItem(i, 5, lockItem(numItem(kAbs[i].nrc)));
    }
    sa->vbox()->addWidget(t);
    v->addWidget(sa);

    auto *se = new SectionBox(I18n::tr("sp_rev_result"), page);
    auto *hb = new QHBoxLayout();
    auto *rtBadge = makeBadge(QString(), kAcc, se, true);
    hb->addWidget(rtBadge);
    auto *okBadge = makeBadge(QString(), kOk, se);
    hb->addWidget(okBadge);
    hb->addStretch(1);
    se->vbox()->addLayout(hb);
    auto *noteLbl = makeHint(QString(), se);
    se->vbox()->addWidget(noteLbl);
    v->addWidget(se);

    // RT60 を core/RoomAcoustics の Sabine 式 (rt60, formula=0) で実計算する。
    // 吸音力 A はテーブルのチェック ON 行の 面積 × α@1kHz (帯域 1kHz)、
    // 体積 V は寸法入力 (L × W × H) から取る。
    auto recompute = [sizeEdit, volLabel, targetEdit, t, rtBadge, okBadge,
                      noteLbl]() {
        // 寸法テキスト中の最初の 3 つの数値を L × W × H [m] とみなす
        double dim[3] = { 0, 0, 0 };
        const int nd = parseNumbers(sizeEdit->text(), dim, 3);
        const bool dimsOk = (nd == 3) && dim[0] > 0 && dim[1] > 0 && dim[2] > 0;
        const double V = dimsOk ? dim[0] * dim[1] * dim[2] : 0;
        const double S = dimsOk ? 2 * (dim[0] * dim[1] + dim[0] * dim[2]
                                       + dim[1] * dim[2]) : 0;
        volLabel->setText(dimsOk ? I18n::tr("sp_rev_vol_fmt")
                                       .arg(QString::number(V, 'g', 6))
                                 : QString("—"));

        // 吸音バジェット → AcousticOpts (role=Other, α は 1kHz 値)
        AcousticOpts opts;
        opts.volume = V;
        opts.surface = S;
        opts.absorption.clear();
        for (int r = 0; r < t->rowCount(); ++r) {
            const QTableWidgetItem *chk = t->item(r, 0);
            if (!chk || chk->checkState() != Qt::Checked) continue;
            bool okA = false, okAl = false;
            const double area  = cellNum(t, r, 2, &okA);
            const double alpha = cellNum(t, r, 4, &okAl);
            if (!okA || !okAl || area <= 0) continue;
            AbsorptionRow row;
            row.role = AbsorptionRow::Other;
            row.area = area;
            for (double &al : row.alpha) al = alpha;   // 帯域は 1kHz のみ使用
            opts.absorption.push_back(row);
        }
        const double A = roomac::totalAbsorption(opts, 3);   // band 3 = 1kHz
        const double T = dimsOk ? roomac::rt60(opts, 3, 0) : 0;   // 0 = Sabine
        if (!dimsOk || T <= 0) {
            // 寸法が読めない / 吸音力ゼロ → 値と判定を出さない
            rtBadge->setText(I18n::tr("sp_rev_rt_fmt").arg("—"));
            okBadge->setVisible(false);
            noteLbl->setText(I18n::tr("sp_rev_note_fmt").arg("—", "—"));
            return;
        }
        rtBadge->setText(I18n::tr("sp_rev_rt_fmt")
                             .arg(QString::number(T, 'f', 2)));
        bool okTgt = false;
        const double target = targetEdit->text().toDouble(&okTgt);
        okBadge->setVisible(okTgt && target > 0);
        if (okTgt && target > 0) {
            const bool met = T <= target;
            okBadge->setText(I18n::tr(met ? "sp_rev_ok" : "sp_rev_ng"));
            styleBadge(okBadge, met ? kOk : kWarn);
        }
        noteLbl->setText(I18n::tr("sp_rev_note_fmt")
                             .arg(QString::number(V, 'g', 6),
                                  QString::number(A, 'f', 1)));
    };
    connect(t, &QTableWidget::itemChanged, this,
            [recompute](QTableWidgetItem *) { recompute(); });
    connect(sizeEdit, &QLineEdit::textChanged, this,
            [recompute](const QString &) { recompute(); });
    connect(targetEdit, &QLineEdit::textChanged, this,
            [recompute](const QString &) { recompute(); });
    recompute();

    v->addStretch(1);
    return page;
}

// ── 会話プライバシー ────────────────────────────────────────────────────────
QWidget *SoundproofTab::buildSpeechPage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(8);

    auto *ss = new SectionBox(I18n::tr("sp_speech_section"), page);
    ss->vbox()->addWidget(makeHint(I18n::tr("sp_speech_hint"), ss));
    auto *scen = new QComboBox(ss);
    scen->addItems({ I18n::tr("sp_spc_open"), I18n::tr("sp_spc_closed"),
                     I18n::tr("sp_spc_meeting") });
    ss->form()->addRow(I18n::tr("sp_speech_scenario"), scen);
    ss->vbox()->addWidget(makeHint(I18n::tr("sp_speech_scen_note"), ss));
    // 通常発声の音圧レベル 60 dB(A) @1m は ANSI S3.5 (SII) の代表値
    auto *talkEdit = numEdit("60", ss);
    ss->form()->addRow(I18n::tr("sp_speech_level"),
                       unitRow(talkEdit, I18n::tr("sp_speech_level_u"), ss));
    auto *distEdit = numEdit("5", ss);
    ss->form()->addRow(I18n::tr("sp_speech_dist"), unitRow(distEdit, "m", ss));
    auto *rtEdit = numEdit("0.6", ss);
    ss->form()->addRow(I18n::tr("sp_speech_rt"), unitRow(rtEdit, "s", ss));
    auto *bgEdit = numEdit("42", ss);
    ss->form()->addRow(I18n::tr("sp_speech_bg"),
                       unitRow(bgEdit, I18n::tr("sp_speech_bg_u"), ss));
    auto *maskChk = makeCheck(I18n::tr("sp_speech_mask"), false, ss);
    auto *maskEdit = numEdit("45", ss);
    maskEdit->setEnabled(false);
    auto *hm = new QHBoxLayout();
    hm->addWidget(maskChk);
    hm->addWidget(maskEdit);
    hm->addWidget(new QLabel(I18n::tr("sp_speech_mask_u"), ss));
    hm->addStretch(1);
    ss->vbox()->addLayout(hm);
    v->addWidget(ss);

    auto *sm = new SectionBox(I18n::tr("sp_metrics_section"), page);
    auto *hb = new QHBoxLayout();
    auto *stiBadge = makeBadge(QString(), kAcc, sm, true);
    hb->addWidget(stiBadge);
    auto *privBadge = makeBadge(QString(), kOk, sm);
    hb->addWidget(privBadge);
    hb->addStretch(1);
    sm->vbox()->addLayout(hb);
    auto *snrLbl = makeHint(QString(), sm);
    sm->vbox()->addWidget(snrLbl);
    // STI の区分表 (IEC 60268-16 の明瞭度カテゴリに対応する一般的な目安)
    auto *t = makeTable({ "STI", I18n::tr("sp_h_intel"), I18n::tr("sp_h_priv") },
                        4, sm, 160);
    struct Sti { const char *range; double lo; double hi; const char *intel;
                 const char *priv; const char *color; };
    static const Sti kSti[4] = {
        { "≥0.75",      0.75, 1.01, "sp_v_exc",    "sp_v_none",  nullptr },
        { "0.45–0.75",  0.45, 0.75, "sp_v_normal", "sp_v_insuf", kWarn  },
        { "0.20–0.45",  0.20, 0.45, "sp_v_insuf",  "sp_v_good",  kOk    },
        { "<0.20",     -0.01, 0.20, "sp_v_bad",    "sp_v_conf",  kOk    },
    };
    for (int i = 0; i < 4; ++i) {
        t->setItem(i, 0, numItem(QString::fromUtf8(kSti[i].range)));
        t->setItem(i, 1, textItem(I18n::tr(kSti[i].intel)));
        auto *pv = textItem(I18n::tr(kSti[i].priv));
        if (kSti[i].color)
            pv->setForeground(QColor(kSti[i].color));
        t->setItem(i, 2, pv);
    }
    sm->vbox()->addWidget(t);
    sm->vbox()->addWidget(makeHint(I18n::tr("sp_sti_note"), sm));
    v->addWidget(sm);

    auto recompute = [talkEdit, distEdit, rtEdit, bgEdit, maskChk, maskEdit,
                      stiBadge, privBadge, snrLbl, t]() {
        maskEdit->setEnabled(maskChk->isChecked());
        bool okL = false, okR = false, okT = false, okB = false, okM = false;
        const double talk = talkEdit->text().toDouble(&okL);
        const double r    = distEdit->text().toDouble(&okR);
        const double rt   = rtEdit->text().toDouble(&okT);
        const double bg   = bgEdit->text().toDouble(&okB);
        const double mask = maskEdit->text().toDouble(&okM);
        // 行のハイライトを一旦解除
        for (int i = 0; i < t->rowCount(); ++i)
            for (int c = 0; c < t->columnCount(); ++c)
                if (auto *it = t->item(i, c)) it->setBackground(QBrush());
        if (!okL || !okR || r <= 0 || !okT || rt < 0 || !okB) {
            stiBadge->setText(I18n::tr("sp_sti_none"));
            privBadge->setVisible(false);
            snrLbl->clear();
            return;
        }
        // 暗騒音 (必要ならマスキング音とエネルギー合成)
        double noise = bg;
        if (maskChk->isChecked() && okM)
            noise = 10.0 * std::log10(std::pow(10.0, bg / 10.0)
                                      + std::pow(10.0, mask / 10.0));
        // 直接音は自由音場 (−20log10 r)
        const double signal = talk - 20.0 * std::log10(r);
        const double snr = signal - noise;
        const double stiVal = ins::sti(rt, snr);
        stiBadge->setText(I18n::tr("sp_sti_fmt")
                              .arg(QString::number(stiVal, 'f', 2)));
        snrLbl->setText(I18n::tr("sp_sti_snr_fmt")
                            .arg(QString::number(snr, 'f', 1),
                                 QString::number(talk, 'f', 1),
                                 QString::number(r, 'f', 1),
                                 QString::number(noise, 'f', 1),
                                 QString::number(rt, 'f', 2)));
        // 会話プライバシーは STI が低いほど良い (0.20 未満で機密保持相当)
        const bool good = stiVal < 0.45;
        privBadge->setVisible(true);
        privBadge->setText(I18n::tr(good ? "sp_speech_ok" : "sp_speech_ng"));
        styleBadge(privBadge, good ? kOk : kWarn);
        for (int i = 0; i < 4; ++i) {
            if (stiVal < kSti[i].lo || stiVal >= kSti[i].hi) continue;
            for (int c = 0; c < t->columnCount(); ++c)
                if (auto *it = t->item(i, c))
                    it->setBackground(QColor(0, 120, 212, 40));
        }
    };
    for (QLineEdit *e : { talkEdit, distEdit, rtEdit, bgEdit, maskEdit })
        connect(e, &QLineEdit::textChanged, this,
                [recompute](const QString &) { recompute(); });
    connect(maskChk, &QCheckBox::toggled, this,
            [recompute](bool) { recompute(); });
    recompute();

    v->addStretch(1);
    return page;
}
