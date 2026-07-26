// SoundproofTab.cpp
#include "SoundproofTab.h"
#include "../core/Project.h"
#include "../widgets/MiniPlot.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <cmath>

using namespace ofd;

// ── タブ専用語彙 (file-local 登録, 接頭辞 sp_) ──────────────────────────────
namespace {
const bool s_i18n = [] {
    using ofd::I18n;
    // タブ名 (MainWindow 配線用)
    I18n::reg("t_soundproof", "🔇 防音設計", "🔇 Soundproofing");
    // シナリオ選択
    I18n::reg("sp_scenario_section", "解析シナリオ", "Scenario");
    I18n::reg("sp_scenario_hint",
              "評価したい防音シナリオを選択 — 構造・測定法・規格表記が自動切替されます。",
              "Choose the soundproofing scenario — structure, test method and "
              "rating notation switch automatically.");
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
    // 間仕切壁
    I18n::reg("sp_rooms_section", "送信室・受信室", "Source && receiving rooms");
    I18n::reg("sp_src_room", "送信室", "Source room");
    I18n::reg("sp_src_room_def", "6.0 × 4.5 × 2.7 m (会議室)",
              "6.0 × 4.5 × 2.7 m (meeting room)");
    I18n::reg("sp_src_room_v", "体積 73 m³", "volume 73 m³");
    I18n::reg("sp_rcv_room", "受信室", "Receiving room");
    I18n::reg("sp_rcv_room_def", "5.5 × 4.5 × 2.7 m (隣接会議室)",
              "5.5 × 4.5 × 2.7 m (adjacent meeting room)");
    I18n::reg("sp_rcv_room_v", "体積 67 m³", "volume 67 m³");
    I18n::reg("sp_wall_area", "仕切壁面積 S", "Partition area S");
    I18n::reg("sp_build_section", "壁構造", "Partition build-up");
    I18n::reg("sp_build_hint",
              "層構造を上から順に。各層が遮音性能 (質量+剛性+空気層) を決定。",
              "Layers top to bottom. Each layer sets the insulation "
              "(mass + stiffness + air gap).");
    I18n::reg("sp_h_material", "材質", "Material");
    I18n::reg("sp_h_thick", "厚さ [mm]", "Thickness [mm]");
    I18n::reg("sp_h_density", "密度 [kg/m³]", "Density [kg/m³]");
    I18n::reg("sp_h_surfdens", "面密度 [kg/m²]", "Surface density [kg/m²]");
    I18n::reg("sp_gypsum", "石膏ボード GB-R", "Gypsum board GB-R");
    I18n::reg("sp_glasswool", "グラスウール 32K", "Glass wool 32K");
    I18n::reg("sp_airgap", "空気層", "Air gap");
    I18n::reg("sp_total", "合計", "Total");
    I18n::reg("sp_add_layer", "＋ 層を追加…", "+ Add layer…");
    I18n::reg("sp_preset_btn", "📚 標準工法プリセット", "📚 Standard build presets");
    I18n::reg("sp_dxf_btn", "📁 .dxf 取込", "📁 Import .dxf");
    I18n::reg("sp_rw_est", "推定 Rw ~ 50 dB", "Estimated Rw ~ 50 dB");
    I18n::reg("sp_detail_section", "ディテール", "Construction details");
    I18n::reg("sp_det_double", "二重壁構造 (空気層分離)",
              "Double-leaf wall (separated air gap)");
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
    I18n::reg("sp_tl_section", "評価結果", "Sound transmission loss R(f)");
    I18n::reg("sp_tl_dip_note",
              "▸ 1250 Hz 付近のディップ = コインシデンス周波数 (限界周波数)",
              "▸ Dip near 1250 Hz = coincidence (critical) frequency");
    I18n::reg("sp_rating_section", "シングルナンバー評価", "Single-number rating");
    I18n::reg("sp_h_metric", "指標", "Metric");
    I18n::reg("sp_h_value", "値", "Value");
    I18n::reg("sp_h_meaning", "意味", "Meaning");
    I18n::reg("sp_r_c", "C (補正)", "C (adaptation)");
    I18n::reg("sp_r_ctr", "Ctr (交通騒音)", "Ctr (traffic)");
    I18n::reg("sp_m_rw", "遮音単一数値 (ISO 717-1)",
              "Weighted sound reduction (ISO 717-1)");
    I18n::reg("sp_m_stc", "米国規格 (ASTM E90)", "US rating (ASTM E90)");
    I18n::reg("sp_m_c", "ピンクノイズスペクトル補正",
              "Pink-noise spectrum adaptation");
    I18n::reg("sp_m_ctr", "道路・鉄道騒音適用時", "Applied for road/rail noise");
    I18n::reg("sp_m_rwctr", "交通騒音実効値", "Effective value for traffic noise");
    I18n::reg("sp_m_dntw", "標準化レベル差 (現場)",
              "Standardized level difference (field)");
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
    I18n::reg("sp_ext_level", "騒音レベル", "Noise level");
    I18n::reg("sp_ext_angle", "入射角", "Incidence angle");
    I18n::reg("sp_ext_angle_u", "° (壁面法線から)", "° (from facade normal)");
    I18n::reg("sp_ext_diffuse", "拡散入射 (diffuse field)",
              "Diffuse-field incidence");
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
    I18n::reg("sp_fac_vent", "換気口・通気口を考慮", "Include vents and air inlets");
    I18n::reg("sp_indoor_section", "室内騒音予測", "Predicted indoor SPL");
    I18n::reg("sp_indoor_lp", "室内 Lp = 38 dB(A)", "Indoor Lp = 38 dB(A)");
    I18n::reg("sp_indoor_ref", "基準値", "Criteria");
    I18n::reg("sp_indoor_ref_note",
              "住宅 ≤ 40dB(A)、病院 ≤ 35dB(A)、オフィス ≤ 45dB(A) - WHO/建築学会",
              "Dwelling ≤ 40 dB(A), hospital ≤ 35 dB(A), office ≤ 45 dB(A) "
              "— WHO / AIJ");
    I18n::reg("sp_indoor_ok", "住宅基準クリア", "Meets dwelling criterion");
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
    I18n::reg("sp_impact_section", "衝撃音発生源", "Impact source");
    I18n::reg("sp_impact_std", "標準源", "Standard source");
    I18n::reg("sp_imp_tap", "タッピングマシン", "Tapping machine");
    I18n::reg("sp_imp_ball", "ゴム球 (中量衝撃)", "Rubber ball (heavy-soft)");
    I18n::reg("sp_imp_tire", "自動車タイヤ", "Car tire");
    I18n::reg("sp_imp_step", "歩行音", "Footsteps");
    I18n::reg("sp_imp_drop", "物落下", "Object drop");
    I18n::reg("sp_floor_result", "結果", "Impact sound level");
    I18n::reg("sp_jis_grade", "JIS 等級:", "JIS grade:");
    I18n::reg("sp_jis_l50", "L-50 (集合住宅推奨)",
              "L-50 (recommended for apartments)");
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
    I18n::reg("sp_flank_total", "合成 R'w = 47 dB", "Combined R'w = 47 dB");
    I18n::reg("sp_flank_note", "(直接 52dB から 5dB 悪化)",
              "(5 dB worse than direct 52 dB)");
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
    I18n::reg("sp_duct_len", "ダクト全長", "Total duct length");
    I18n::reg("sp_duct_elbow", "エルボ", "Elbows");
    I18n::reg("sp_duct_elbow_u", "箇所", "count");
    I18n::reg("sp_duct_damper", "ダンパー / VAV", "Dampers / VAV");
    I18n::reg("sp_atten_section", "減衰要素", "Attenuation elements");
    I18n::reg("sp_att_silencer", "消音器 (スプライサー)", "Silencer (splitter)");
    I18n::reg("sp_att_lining", "内貼り吸音材 50mm", "Acoustic lining 50 mm");
    I18n::reg("sp_att_isolator", "ファン振動アイソレータ",
              "Fan vibration isolators");
    I18n::reg("sp_duct_indoor", "室内到達音", "Indoor level");
    I18n::reg("sp_fan_pwl", "ファン PWL", "Fan PWL");
    I18n::reg("sp_indoor_spl", "室内 SPL 予測", "Predicted indoor SPL");
    I18n::reg("sp_nc35", "NC-35 (オフィス推奨)", "NC-35 (recommended for offices)");
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
    I18n::reg("sp_mach_open", "開口部 (吸気・排気)", "Openings (intake/exhaust)");
    I18n::reg("sp_mach_open_u", "m² (要消音)", "m² (needs silencing)");
    I18n::reg("sp_il_section", "挿入損失 IL", "Insertion loss IL");
    I18n::reg("sp_il_note",
              "機器音 85 - IL 22 = 63 dB(A) @ 1m → 敷地境界で住宅基準クリア",
              "Source 85 - IL 22 = 63 dB(A) @ 1 m → meets residential limit "
              "at the site boundary");
    // 室内残響対策
    I18n::reg("sp_rev_section", "室内残響対策", "Room reverberation control");
    I18n::reg("sp_rev_room", "部屋", "Room");
    I18n::reg("sp_room_office", "オフィス (オープンプラン)", "Office (open plan)");
    I18n::reg("sp_room_gym", "体育館", "Gymnasium");
    I18n::reg("sp_room_cafe", "食堂", "Cafeteria");
    I18n::reg("sp_room_class", "教室", "Classroom");
    I18n::reg("sp_room_rest", "レストラン", "Restaurant");
    I18n::reg("sp_rev_size", "サイズ", "Size");
    I18n::reg("sp_rev_size_def", "20 × 15 × 3.5 m (体積 1050 m³)",
              "20 × 15 × 3.5 m (volume 1050 m³)");
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
    I18n::reg("sp_rev_note",
              "Sabine: RT = 0.161 × V / A_total — A_total = 168 m²·Sabin (吸音面積)",
              "Sabine: RT = 0.161 × V / A_total — A_total = 168 m²·Sabin "
              "(absorption area)");
    // 会話プライバシー
    I18n::reg("sp_speech_section", "会話プライバシー", "Speech privacy");
    I18n::reg("sp_speech_hint",
              "執務空間の会話漏れ評価。STI または Speech Privacy Class (SPC) で評価。",
              "Evaluates speech leakage in workspaces via STI or "
              "Speech Privacy Class (SPC).");
    I18n::reg("sp_speech_scenario", "シナリオ", "Scenario");
    I18n::reg("sp_spc_open", "オープンオフィス", "Open office");
    I18n::reg("sp_spc_closed", "個室間", "Between private rooms");
    I18n::reg("sp_spc_meeting", "会議室漏れ", "Meeting-room leakage");
    I18n::reg("sp_speech_dist", "話者-受聴者距離", "Talker-listener distance");
    I18n::reg("sp_speech_bg", "バックグラウンドノイズ (HVAC等)",
              "Background noise (HVAC etc.)");
    I18n::reg("sp_speech_bg_u", "dB(A) → マスキング有利", "dB(A) → aids masking");
    I18n::reg("sp_speech_mask", "サウンドマスキングシステム導入",
              "Install sound-masking system");
    I18n::reg("sp_metrics_section", "評価指標", "Metrics");
    I18n::reg("sp_speech_ok", "プライバシー良好", "Good privacy");
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
    I18n::reg("sp_exp_csv", "📊 R(f) スペクトル (CSV)", "📊 R(f) spectrum (CSV)");
    I18n::reg("sp_exp_aural", "🎧 可聴化 (受音側で試聴)",
              "🎧 Auralization (listen at receiver)");
    I18n::reg("sp_exp_std", "📑 規格対応書式 (ISO/ASTM)",
              "📑 Standard forms (ISO/ASTM)");
    return true;
}();

// ── 小物ヘルパー (mock の badge / muted / q-num 相当) ───────────────────────
const char kAcc[]  = "#0078D4";   // badge acc
const char kOk[]   = "#2E8B57";   // badge ok
const char kWarn[] = "#B45309";   // badge warn
const char kAccAcoustic[] = "#2E8B57";   // var(--acc-acoustic)

QLabel *makeBadge(const QString &text, const char *color, QWidget *parent,
                  bool big = false)
{
    auto *l = new QLabel(text, parent);
    l->setStyleSheet(QString("color:%1; border:1px solid %1; border-radius:3px;"
                             " padding:%2; font-weight:600;%3")
                         .arg(color, big ? "3px 10px" : "1px 6px",
                              big ? " font-size:13px;" : ""));
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
    m_stack->addWidget(buildPartitionPage());   // 0 partition
    m_stack->addWidget(buildFacadePage());      // 1 facade
    m_stack->addWidget(buildFloorPage());       // 2 floor
    m_stack->addWidget(buildFlankingPage());    // 3 flanking
    m_stack->addWidget(buildDuctPage());        // 4 duct
    m_stack->addWidget(buildMachinePage());     // 5 machine
    m_stack->addWidget(buildReverbPage());      // 6 reverb
    m_stack->addWidget(buildSpeechPage());      // 7 speech
    v->addWidget(m_stack);

    // 出力 (全シナリオ共通)
    auto *se = new SectionBox(I18n::tr("sp_export_section"), body);
    auto *he = new QHBoxLayout();
    he->addWidget(new QPushButton(I18n::tr("sp_exp_report"), se));
    he->addWidget(new QPushButton(I18n::tr("sp_exp_csv"), se));
    he->addWidget(new QPushButton(I18n::tr("sp_exp_aural"), se));
    he->addWidget(new QPushButton(I18n::tr("sp_exp_std"), se));
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
QWidget *SoundproofTab::buildPartitionPage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(8);

    // 送信室・受信室
    auto *sr = new SectionBox(I18n::tr("sp_rooms_section"), page);
    auto *srcEdit = new QLineEdit(I18n::tr("sp_src_room_def"), sr);
    auto *h1 = new QHBoxLayout();
    h1->addWidget(srcEdit, 1);
    h1->addWidget(new QLabel(I18n::tr("sp_src_room_v"), sr));
    sr->form()->addRow(I18n::tr("sp_src_room"), h1);
    auto *rcvEdit = new QLineEdit(I18n::tr("sp_rcv_room_def"), sr);
    auto *h2 = new QHBoxLayout();
    h2->addWidget(rcvEdit, 1);
    h2->addWidget(new QLabel(I18n::tr("sp_rcv_room_v"), sr));
    sr->form()->addRow(I18n::tr("sp_rcv_room"), h2);
    sr->form()->addRow(I18n::tr("sp_wall_area"),
                       unitRow(numEdit("12.15", sr), "m²", sr));
    v->addWidget(sr);

    // 壁構造 (層テーブル)
    auto *sb = new SectionBox(I18n::tr("sp_build_section"), page);
    sb->vbox()->addWidget(makeHint(I18n::tr("sp_build_hint"), sb));
    auto *t = makeTable({ "", "#", I18n::tr("sp_h_material"),
                          I18n::tr("sp_h_thick"), I18n::tr("sp_h_density"),
                          I18n::tr("sp_h_surfdens") }, 7, sb, 230);
    struct Layer { const char *mat; const char *th; const char *rho;
                   const char *sd; };
    static const Layer kLayers[5] = {
        { "sp_gypsum",    "12.5", "720", "9.0" },
        { "sp_glasswool", "50",   "32",  "1.6" },
        { "sp_airgap",    "25",   "—",   "—"   },
        { "sp_glasswool", "50",   "32",  "1.6" },
        { "sp_gypsum",    "12.5", "720", "9.0" },
    };
    for (int i = 0; i < 5; ++i) {
        t->setItem(i, 0, checkItem(true));
        t->setItem(i, 1, numItem(QString::number(i + 1)));
        t->setItem(i, 2, textItem(I18n::tr(kLayers[i].mat)));
        t->setItem(i, 3, numItem(kLayers[i].th));
        t->setItem(i, 4, numItem(kLayers[i].rho));
        t->setItem(i, 5, numItem(kLayers[i].sd));
    }
    // 合計行 (太字)
    t->setSpan(5, 0, 1, 3);
    auto *tot = textItem(I18n::tr("sp_total"));
    QFont bf = tot->font();
    bf.setBold(true);
    tot->setFont(bf);
    t->setItem(5, 0, tot);
    auto *tth = numItem("150"); tth->setFont(bf); t->setItem(5, 3, tth);
    t->setItem(5, 4, textItem("—"));
    auto *tsd = numItem("21.2"); tsd->setFont(bf); t->setItem(5, 5, tsd);
    // ＋ 層を追加…
    t->setItem(6, 0, checkItem(false));
    t->setSpan(6, 1, 1, 5);
    auto *add = textItem(I18n::tr("sp_add_layer"));
    QFont itf = add->font();
    itf.setItalic(true);
    add->setFont(itf);
    t->setItem(6, 1, add);
    sb->vbox()->addWidget(t);
    auto *hb = new QHBoxLayout();
    hb->addWidget(new QPushButton(I18n::tr("sp_preset_btn"), sb));
    hb->addWidget(new QPushButton(I18n::tr("sp_dxf_btn"), sb));
    hb->addStretch(1);
    hb->addWidget(new QLabel(I18n::tr("sp_rw_est"), sb));
    sb->vbox()->addLayout(hb);
    v->addWidget(sb);

    // ディテール
    auto *sd = new SectionBox(I18n::tr("sp_detail_section"), page);
    sd->vbox()->addWidget(makeCheck(I18n::tr("sp_det_double"), true, sd));
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
    v->addWidget(sd);

    // 評価結果 R(f) — mock のデータ点をそのまま転記 (x は log10(f))
    auto *st = new SectionBox(I18n::tr("sp_tl_section"), page);
    auto *plot = new MiniPlot(st);
    plot->setLabels("f [Hz] (log)", "R [dB]");
    plot->setXTickPow10(true);
    plot->setYRange(10, 80);
    plot->setMinimumHeight(120);
    static const double kTL[21][2] = {
        { 50, 18 },   { 63, 22 },   { 80, 25 },
        { 100, 28 },  { 125, 30 },  { 160, 33 },
        { 200, 36 },  { 250, 40 },  { 315, 43 },
        { 400, 46 },  { 500, 48 },  { 630, 51 },
        { 800, 53 },  { 1000, 56 }, { 1250, 55 },
        { 1600, 48 }, { 2000, 52 }, { 2500, 58 },
        { 3150, 62 }, { 4000, 65 }, { 5000, 68 },
    };
    MiniSeries tl;
    tl.color = QColor(kAccAcoustic);
    for (const auto &d : kTL)
        tl.pts.push_back({ std::log10(d[0]), d[1] });
    plot->setSeries({ tl });
    st->vbox()->addWidget(plot);
    st->vbox()->addWidget(makeHint(I18n::tr("sp_tl_dip_note"), st));
    v->addWidget(st);

    // シングルナンバー評価
    auto *sg = new SectionBox(I18n::tr("sp_rating_section"), page);
    auto *hBadge = new QHBoxLayout();
    hBadge->addWidget(makeBadge("Rw = 52 dB", kAcc, sg, true));
    hBadge->addWidget(makeBadge("STC 51", kAcc, sg, true));
    hBadge->addStretch(1);
    sg->vbox()->addLayout(hBadge);
    auto *rt = makeTable({ I18n::tr("sp_h_metric"), I18n::tr("sp_h_value"),
                           I18n::tr("sp_h_meaning") }, 6, sg, 200);
    struct Rating { const char *metric; bool trMetric; const char *value;
                    const char *meaning; };
    static const Rating kRatings[6] = {
        { "Rw",       false, "52 dB", "sp_m_rw"   },
        { "STC",      false, "51",    "sp_m_stc"  },
        { "sp_r_c",   true,  "-1",    "sp_m_c"    },
        { "sp_r_ctr", true,  "-6",    "sp_m_ctr"  },
        { "Rw+Ctr",   false, "46 dB", "sp_m_rwctr" },
        { "DnT,w",    false, "53 dB", "sp_m_dntw" },
    };
    for (int i = 0; i < 6; ++i) {
        rt->setItem(i, 0, textItem(kRatings[i].trMetric
                                       ? I18n::tr(kRatings[i].metric)
                                       : QString::fromUtf8(kRatings[i].metric)));
        rt->setItem(i, 1, numItem(kRatings[i].value));
        rt->setItem(i, 2, textItem(I18n::tr(kRatings[i].meaning)));
    }
    sg->vbox()->addWidget(rt);
    auto *hu = new QHBoxLayout();
    hu->addWidget(new QLabel(I18n::tr("sp_use_hint"), sg));
    hu->addWidget(makeBadge(I18n::tr("sp_use_hosp"), kOk, sg));
    hu->addWidget(makeBadge(I18n::tr("sp_use_dwell"), kOk, sg));
    hu->addWidget(makeBadge(I18n::tr("sp_use_office"), kWarn, sg));
    hu->addStretch(1);
    sg->vbox()->addLayout(hu);
    v->addWidget(sg);

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
    se->form()->addRow(I18n::tr("sp_ext_level"),
                       unitRow(numEdit("75", se), "dB(A) @ 25m", se));
    se->form()->addRow(I18n::tr("sp_ext_angle"),
                       unitRow(numEdit("45", se), I18n::tr("sp_ext_angle_u"), se));
    se->vbox()->addWidget(makeCheck(I18n::tr("sp_ext_diffuse"), true, se));
    v->addWidget(se);

    auto *sf = new SectionBox(I18n::tr("sp_facade_section"), page);
    sf->form()->addRow(I18n::tr("sp_fac_wall_area"),
                       unitRow(numEdit("20", sf), "m²", sf));
    sf->form()->addRow(I18n::tr("sp_fac_win_area"),
                       unitRow(numEdit("8", sf), "m² (40%)", sf));
    sf->form()->addRow(I18n::tr("sp_fac_wall_rw"),
                       unitRow(numEdit("55", sf), "dB", sf));
    auto *win = new QComboBox(sf);
    win->addItems({ I18n::tr("sp_win1"), I18n::tr("sp_win2"),
                    I18n::tr("sp_win3"), I18n::tr("sp_win4") });
    sf->form()->addRow(I18n::tr("sp_fac_win_type"), win);
    auto *hSash = new QHBoxLayout();
    hSash->addWidget(makeCheck(I18n::tr("sp_fac_sash_a4"), true, sf));
    hSash->addStretch(1);
    sf->form()->addRow(I18n::tr("sp_fac_sash"), hSash);
    sf->vbox()->addWidget(makeCheck(I18n::tr("sp_fac_vent"), false, sf));
    v->addWidget(sf);

    auto *si = new SectionBox(I18n::tr("sp_indoor_section"), page);
    auto *hb = new QHBoxLayout();
    hb->addWidget(makeBadge(I18n::tr("sp_indoor_lp"), kAcc, si, true));
    hb->addStretch(1);
    si->vbox()->addLayout(hb);
    si->form()->addRow(I18n::tr("sp_indoor_ref"),
                       makeHint(I18n::tr("sp_indoor_ref_note"), si));
    auto *hOk = new QHBoxLayout();
    hOk->addWidget(makeBadge(I18n::tr("sp_indoor_ok"), kOk, si));
    hOk->addStretch(1);
    si->vbox()->addLayout(hOk);
    v->addWidget(si);

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
    auto *base = new QComboBox(sf);
    base->addItems({ I18n::tr("sp_base_rc150"), I18n::tr("sp_base_rc200"),
                     I18n::tr("sp_base_wood") });
    sf->form()->addRow(I18n::tr("sp_floor_base"), base);
    auto *ceil = new QComboBox(sf);
    ceil->addItems({ I18n::tr("sp_ceil_direct"), I18n::tr("sp_ceil_susp"),
                     I18n::tr("sp_ceil_damp") });
    sf->form()->addRow(I18n::tr("sp_floor_ceil"), ceil);
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
    hb->addWidget(makeBadge("Ln,w = 48 dB", kAcc, sr, true));
    hb->addWidget(makeBadge("IIC 52", kAcc, sr, true));
    hb->addStretch(1);
    sr->vbox()->addLayout(hb);
    auto *hj = new QHBoxLayout();
    hj->addWidget(new QLabel(I18n::tr("sp_jis_grade"), sr));
    hj->addWidget(makeBadge(I18n::tr("sp_jis_l50"), kOk, sr));
    hj->addStretch(1);
    sr->vbox()->addLayout(hj);
    v->addWidget(sr);

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
    for (int i = 0; i < 5; ++i) {
        t->setItem(i, 0, checkItem(true));
        t->setItem(i, 1, textItem(kPaths[i].trPath
                                      ? I18n::tr(kPaths[i].path)
                                      : QString::fromUtf8(kPaths[i].path)));
        t->setItem(i, 2, textItem(I18n::tr(kPaths[i].desc)));
        t->setItem(i, 3, numItem(kPaths[i].r));
    }
    sp->vbox()->addWidget(t);
    auto *hb = new QHBoxLayout();
    hb->addWidget(makeBadge(I18n::tr("sp_flank_total"), kAcc, sp, true));
    hb->addWidget(new QLabel(I18n::tr("sp_flank_note"), sp));
    hb->addStretch(1);
    sp->vbox()->addLayout(hb);
    v->addWidget(sp);

    auto *si = new SectionBox(I18n::tr("sp_improve_section"), page);
    si->vbox()->addWidget(makeCheck(I18n::tr("sp_impr_float"), false, si));
    si->vbox()->addWidget(makeCheck(I18n::tr("sp_impr_hanger"), false, si));
    si->vbox()->addWidget(makeCheck(I18n::tr("sp_impr_tape"), false, si));
    si->vbox()->addWidget(makeCheck(I18n::tr("sp_impr_elastic"), false, si));
    auto *hr = new QHBoxLayout();
    hr->addWidget(new QPushButton(I18n::tr("sp_recalc_btn"), si));
    hr->addStretch(1);
    si->vbox()->addLayout(hr);
    v->addWidget(si);

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
    sd->form()->addRow(I18n::tr("sp_duct_sect"),
                       new QLineEdit("400 × 250 mm", sd));
    sd->form()->addRow(I18n::tr("sp_duct_len"),
                       unitRow(numEdit("12", sd, 100), "m", sd));
    sd->form()->addRow(I18n::tr("sp_duct_elbow"),
                       unitRow(numEdit("3", sd), I18n::tr("sp_duct_elbow_u"), sd));
    auto *hd = new QHBoxLayout();
    hd->addWidget(numEdit("2", sd));
    hd->addStretch(1);
    sd->form()->addRow(I18n::tr("sp_duct_damper"), hd);
    v->addWidget(sd);

    auto *sa = new SectionBox(I18n::tr("sp_atten_section"), page);
    auto *h1 = new QHBoxLayout();
    h1->addWidget(makeCheck(I18n::tr("sp_att_silencer"), true, sa));
    h1->addWidget(new QLabel("~15 dB", sa));
    h1->addStretch(1);
    sa->vbox()->addLayout(h1);
    auto *h2 = new QHBoxLayout();
    h2->addWidget(makeCheck(I18n::tr("sp_att_lining"), true, sa));
    h2->addWidget(new QLabel("~3 dB/m", sa));
    h2->addStretch(1);
    sa->vbox()->addLayout(h2);
    sa->vbox()->addWidget(makeCheck(I18n::tr("sp_att_isolator"), true, sa));
    v->addWidget(sa);

    auto *si = new SectionBox(I18n::tr("sp_duct_indoor"), page);
    si->form()->addRow(I18n::tr("sp_fan_pwl"),
                       unitRow(numEdit("80", si), "dB", si));
    auto *hs = new QHBoxLayout();
    hs->addWidget(makeBadge("35 dB(A)", kAcc, si));
    hs->addStretch(1);
    si->form()->addRow(I18n::tr("sp_indoor_spl"), hs);
    auto *hn = new QHBoxLayout();
    hn->addWidget(makeBadge(I18n::tr("sp_nc35"), kOk, si));
    hn->addStretch(1);
    si->vbox()->addLayout(hn);
    v->addWidget(si);

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
    sm->form()->addRow(I18n::tr("sp_mach_level"),
                       unitRow(numEdit("85", sm), "dB(A) @1m", sm));
    sm->form()->addRow(I18n::tr("sp_mach_size"),
                       new QLineEdit("1.5 × 1.5 × 1.2 m", sm));
    auto *wall = new QComboBox(sm);
    wall->addItems({ I18n::tr("sp_mwall1"), I18n::tr("sp_mwall2") });
    sm->form()->addRow(I18n::tr("sp_mach_wall"), wall);
    auto *ho = new QHBoxLayout();
    ho->addWidget(numEdit("2", sm));
    ho->addWidget(new QLabel("×", sm));
    ho->addWidget(numEdit("0.04", sm));
    ho->addWidget(new QLabel(I18n::tr("sp_mach_open_u"), sm));
    ho->addStretch(1);
    sm->form()->addRow(I18n::tr("sp_mach_open"), ho);
    v->addWidget(sm);

    auto *si = new SectionBox(I18n::tr("sp_il_section"), page);
    auto *hb = new QHBoxLayout();
    hb->addWidget(makeBadge("IL = 22 dB(A)", kAcc, si, true));
    hb->addStretch(1);
    si->vbox()->addLayout(hb);
    si->vbox()->addWidget(makeHint(I18n::tr("sp_il_note"), si));
    v->addWidget(si);

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
    sr->form()->addRow(I18n::tr("sp_rev_size"),
                       new QLineEdit(I18n::tr("sp_rev_size_def"), sr));
    sr->form()->addRow(I18n::tr("sp_rev_target"),
                       unitRow(numEdit("0.8", sr), I18n::tr("sp_rev_target_u"),
                               sr));
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
    for (int i = 0; i < 4; ++i) {
        t->setItem(i, 0, checkItem(kAbs[i].on));
        t->setItem(i, 1, textItem(I18n::tr(kAbs[i].surf)));
        t->setItem(i, 2, numItem(kAbs[i].area));
        t->setItem(i, 3, textItem(I18n::tr(kAbs[i].mat)));
        t->setItem(i, 4, numItem(kAbs[i].alpha));
        t->setItem(i, 5, numItem(kAbs[i].nrc));
    }
    sa->vbox()->addWidget(t);
    v->addWidget(sa);

    auto *se = new SectionBox(I18n::tr("sp_rev_result"), page);
    auto *hb = new QHBoxLayout();
    hb->addWidget(makeBadge("RT60 = 0.74 s @ 1kHz", kAcc, se, true));
    hb->addWidget(makeBadge(I18n::tr("sp_rev_ok"), kOk, se));
    hb->addStretch(1);
    se->vbox()->addLayout(hb);
    se->vbox()->addWidget(makeHint(I18n::tr("sp_rev_note"), se));
    v->addWidget(se);

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
    ss->form()->addRow(I18n::tr("sp_speech_dist"),
                       unitRow(numEdit("5", ss), "m", ss));
    ss->form()->addRow(I18n::tr("sp_speech_bg"),
                       unitRow(numEdit("42", ss), I18n::tr("sp_speech_bg_u"),
                               ss));
    ss->vbox()->addWidget(makeCheck(I18n::tr("sp_speech_mask"), false, ss));
    v->addWidget(ss);

    auto *sm = new SectionBox(I18n::tr("sp_metrics_section"), page);
    auto *hb = new QHBoxLayout();
    hb->addWidget(makeBadge("STI = 0.32", kAcc, sm, true));
    hb->addWidget(makeBadge(I18n::tr("sp_speech_ok"), kOk, sm));
    hb->addStretch(1);
    sm->vbox()->addLayout(hb);
    auto *t = makeTable({ "STI", I18n::tr("sp_h_intel"), I18n::tr("sp_h_priv") },
                        4, sm, 160);
    struct Sti { const char *range; const char *intel; const char *priv;
                 const char *color; };
    static const Sti kSti[4] = {
        { "≥0.75",     "sp_v_exc",    "sp_v_none", nullptr },
        { "0.45-0.60", "sp_v_normal", "sp_v_insuf", kWarn  },
        { "0.20-0.40", "sp_v_insuf",  "sp_v_good",  kOk    },
        { "<0.20",     "sp_v_bad",    "sp_v_conf",  kOk    },
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
    v->addWidget(sm);

    v->addStretch(1);
    return page;
}
