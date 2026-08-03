// RoomAcousticsTab.cpp
#include "RoomAcousticsTab.h"
#include "TabHelpers.h"
#include "../core/OperaHalls.h"
#include "../core/Project.h"
#include "../widgets/MiniPlot.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFont>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPainter>
#include <QPushButton>
#include <QTableWidget>
#include <QTabWidget>
#include <QTextStream>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

using namespace ofd;
using namespace ofd::roomac;

// ── タブ固有の翻訳キー (rah_) — file-local 登録 (既存 ra_ は I18n.cpp) ──────
namespace {
const bool s_i18n = [] {
    using ofd::I18n;
    // ホールプリセット
    I18n::reg("rah_hall_section", "ホールモデル / Hall preset (実測データ)",
              "Hall preset (measured data)");
    I18n::reg("rah_cat_concert", "🎻 コンサートホール (世界)",
              "🎻 Concert halls (world)");
    I18n::reg("rah_cat_opera", "🎭 オペラ対応ホール (日本)",
              "🎭 Opera-ready halls (Japan)");
    I18n::reg("rah_import_3d", "📁 3Dモデル取込", "📁 Import 3D model");
    I18n::reg("rah_col_rt_occ", "RT60(満席)", "RT60 (occupied)");
    I18n::reg("rah_col_g", "G (強さ)", "G (strength)");
    I18n::reg("rah_col_source", "出典", "Source");
    I18n::reg("rah_measured", "実測", "measured");
    I18n::reg("rah_estimated", "推定", "estimated");
    I18n::reg("rah_closed", "休館中", "Closed");
    I18n::reg("rah_col_pit", "オケピット", "Orchestra pit");
    I18n::reg("rah_col_ensemble", "編成", "Ensemble");
    I18n::reg("rah_col_stage", "舞台機構", "Stage machinery");
    I18n::reg("rah_concert_info", "%1 · 体積 %2 m³ · %3 席",
              "%1 · volume %2 m³ · %3 seats");
    I18n::reg("rah_opera_info", "%1年開館 · %2席 · 体積 ~%3 m³",
              "opened %1 · %2 seats · volume ~%3 m³");
    I18n::reg("rah_scenario", "解析シナリオ:", "Scenarios:");
    I18n::reg("rah_scn_pit_balance", "ピット内→客席バランス",
              "Pit → audience balance");
    I18n::reg("rah_scn_singer_pit", "歌手↔ピット相互聴取",
              "Singer ↔ pit mutual hearing");
    I18n::reg("rah_scn_pit_lid", "ピット蓋開閉の影響",
              "Pit-lid open/close effect");
    I18n::reg("rah_run_hall", "▶ このホールでFDTD/Ray解析を実行",
              "▶ Run FDTD/Ray analysis for this hall");
    I18n::reg("rah_run_hint", "実測値 vs シミュレーション値の比較検証",
              "Validate simulation against measured data");
    I18n::reg("rah_cov_ref",
              "▸ 実測 (公表値): RT=%1s · C80=%2dB · G=+%3dB · ITDG=%4ms"
              " — シミュレーションとの検証目標は誤差 ±0.1s / ±0.5dB 以内",
              "▸ Measured (published): RT=%1s · C80=%2dB · G=+%3dB · ITDG=%4ms"
              " — validation target: within ±0.1 s / ±0.5 dB");
    // サブタブ
    I18n::reg("rah_tab_ir", "IR解析 (Schroeder)", "IR analysis (Schroeder)");
    I18n::reg("rah_tab_spatial", "空間印象 IACC/LF", "Spatial IACC/LF");
    I18n::reg("rah_tab_stage", "ステージ/可変音響", "Stage / variable");
    I18n::reg("rah_tab_materials", "吸音材/散乱体DB", "Material DB");
    I18n::reg("rah_tab_reinforce", "電気音響設計", "Reinforcement");
    // IR解析
    I18n::reg("rah_ir_section", "インパルス応答解析 (ISO 3382-1)",
              "IR analysis (ISO 3382-1)");
    I18n::reg("rah_ir_hint",
              "シミュレーションIRまたは実測インパルス応答 (WAV) から Schroeder "
              "逆積分で減衰曲線を算出。実測WAV取込に対応 → シミュレーションとの"
              "検証が可能。",
              "Computes the decay curve by Schroeder backward integration from a "
              "simulated IR or a measured impulse response (WAV). Measured-WAV "
              "import enables validation against the simulation.");
    I18n::reg("rah_ir_source", "IRソース", "IR source");
    I18n::reg("rah_ir_src_sim", "シミュレーション結果 (H5)",
              "Simulation result (H5)");
    I18n::reg("rah_ir_src_meas", "実測WAV (スイープ/バルーン)",
              "Measured WAV (sweep/balloon)");
    I18n::reg("rah_ir_file", "実測ファイル", "Measured file");
    I18n::reg("rah_browse", "参照…", "Browse…");
    I18n::reg("rah_inv_filter", "逆フィルタ", "Inverse filter");
    I18n::reg("rah_ess", "ESS (Exponential Sine Sweep) 逆畳み込み",
              "ESS (exponential sine sweep) deconvolution");
    I18n::reg("rah_harm_sep", "高調波歪分離", "Harmonic-distortion separation");
    I18n::reg("rah_schroeder_section", "Schroeder 減衰曲線",
              "Schroeder decay curve");
    I18n::reg("rah_col_metric", "指標", "Metric");
    I18n::reg("rah_t20t30_warn",
              "T20/T30 乖離警告 (非線形減衰 → カップリング疑い)",
              "Warn on T20/T30 divergence (non-linear decay → coupling)");
    I18n::reg("rah_inr_check", "INR (Impulse-to-Noise Ratio) 検査 ≥ 45dB",
              "INR (impulse-to-noise ratio) check ≥ 45 dB");
    I18n::reg("rah_validation_section", "実測 vs シミュレーション",
              "Measured vs simulation");
    I18n::reg("rah_col_metric1k", "指標 @1kHz", "Metric @1 kHz");
    I18n::reg("rah_col_meas", "実測", "Measured");
    I18n::reg("rah_col_sim", "シミュ", "Sim");
    I18n::reg("rah_col_diff", "差", "Diff");
    I18n::reg("rah_jnd_ok", "JND内", "within JND");
    // 空間印象
    I18n::reg("rah_spatial_section", "空間印象指標 (ISO 3382-1)",
              "Spatial impression (ISO 3382-1)");
    I18n::reg("rah_spatial_hint",
              "音の広がり感 (ASW) と包まれ感 (LEV) を定量評価。LF/LFCは8字マイク、"
              "IACCはバイノーラル受音点 (HRTF) から算出。",
              "Quantifies apparent source width (ASW) and listener envelopment "
              "(LEV). LF/LFC use a figure-8 mic; IACC uses a binaural receiver "
              "(HRTF).");
    I18n::reg("rah_rcv_model", "受音モデル", "Receiver model");
    I18n::reg("rah_fig8", "8字マイク (LF/LFC)", "Figure-8 mic (LF/LFC)");
    I18n::reg("rah_binaural", "バイノーラル (IACC)", "Binaural (IACC)");
    I18n::reg("rah_results_section", "計算結果", "Results");
    I18n::reg("rah_col_value", "値", "Value");
    I18n::reg("rah_col_range", "推奨範囲", "Recommended");
    I18n::reg("rah_col_meaning", "意味", "Meaning");
    I18n::reg("rah_ok", "良", "good");
    I18n::reg("rah_apt", "適", "OK");
    I18n::reg("rah_bqi_note",
              "※ BQI = 1-IACC(E3) (500/1k/2kHz平均)。Beranekのホールランキング"
              "と最も相関の高い指標。",
              "* BQI = 1-IACC(E3) (mean of 500/1k/2 kHz). The metric best "
              "correlated with Beranek's hall ranking.");
    I18n::reg("rah_seatmap_section", "座席別空間印象マップ",
              "Per-seat spatial map");
    I18n::reg("rah_seatmap_hint",
              "側壁に近い席ほどLF大。ヴィンヤード型はテラス背面が側方反射面と"
              "して働く。",
              "Seats near side walls get higher LF. In vineyard halls the "
              "terrace backs act as lateral reflectors.");
    I18n::reg("rah_lf_map_btn", "🗺 LFマップを客席カバレッジに表示",
              "🗺 Show LF map in coverage");
    I18n::reg("rah_bqi_map_btn", "🗺 BQIマップ", "🗺 BQI map");
    // ステージ/可変音響
    I18n::reg("rah_st_section", "ステージ音響支援 (ST)", "Stage support (ST)");
    I18n::reg("rah_st_hint",
              "演奏者が自分と他者の音をどれだけ聞けるか。ステージ上 1m 間隔の"
              "音源・受音点で測定 (ISO 3382-1)。",
              "How well performers hear themselves and each other. Measured with "
              "source/receiver pairs at 1 m spacing on stage (ISO 3382-1).");
    I18n::reg("rah_st_grid", "ステージ上に測定グリッド (1m間隔) を自動配置",
              "Auto-place 1 m measurement grid on stage");
    I18n::reg("rah_st_matrix", "セクション別 (弦/管/打) の相互聴取マトリクス",
              "Mutual-hearing matrix per section (strings/winds/perc)");
    I18n::reg("rah_va_section", "可変音響", "Variable acoustics");
    I18n::reg("rah_va_hint",
              "可動反射板・吸音バナー・電子残響で用途別に音響を切替 (hitaru / "
              "びわ湖ホールの機構転換相当)。",
              "Switch acoustics per use with movable reflectors, absorption "
              "banners and electronic reverberation (as in hitaru / Biwako "
              "Hall).");
    I18n::reg("rah_col_mech", "機構", "Mechanism");
    I18n::reg("rah_col_state", "状態", "State");
    I18n::reg("rah_col_rtchange", "RT変化", "ΔRT");
    I18n::reg("rah_va_now", "現在の構成: RT = %1 + 0.35 = %2 s (コンサート形式)",
              "Current config: RT = %1 + 0.35 = %2 s (concert)");
    I18n::reg("rah_va_batch", "▶ 各構成で一括解析 (4ケース)",
              "▶ Batch-run all configs (4 cases)");
    I18n::reg("rah_va_batch_hint",
              "コンサート/オペラ/講演/ポピュラーの構成比較表を出力",
              "Outputs a comparison table for concert/opera/speech/popular");
    I18n::reg("rah_cv_section", "カップルドボリューム", "Coupled volumes");
    I18n::reg("rah_cv_hint",
              "残響室を開閉して二段減衰を作る設計 (ルセルン・フィルハーモニー等)。",
              "Reverberation chambers opened/closed to create double-slope decay "
              "(e.g. Lucerne).");
    I18n::reg("rah_cv_aperture", "結合開口", "Coupling aperture");
    I18n::reg("rah_cv_aperture_unit", "m² (可変 0〜80)", "m² (variable 0–80)");
    I18n::reg("rah_cv_volume", "副室体積", "Chamber volume");
    I18n::reg("rah_cv_detect",
              "二段減衰検出 (T20/T30乖離 + Bayesian減衰分解)",
              "Double-slope detection (T20/T30 divergence + Bayesian decay "
              "decomposition)");
    // 吸音材/散乱体DB
    I18n::reg("rah_mat_section", "吸音材・散乱体データベース",
              "Absorber & scatterer DB");
    I18n::reg("rah_mat_hint",
              "表面材質ごとの吸音率α・散乱係数s をオクターブバンド別に管理。"
              "EASE/ODEON の材質ライブラリ相当。幾何音響では散乱係数が音場の質を"
              "左右する。",
              "Manages absorption α and scattering s per surface material by "
              "octave band. Equivalent to the EASE/ODEON material library; in "
              "geometrical acoustics the scattering coefficient governs sound-"
              "field quality.");
    I18n::reg("rah_mat_search", "🔎 材質を検索…", "🔎 Search materials…");
    I18n::reg("rah_mat_import", "📁 材質DB取込 (.csv/EASE .xhn)",
              "📁 Import DB (.csv/EASE .xhn)");
    I18n::reg("rah_alpha_section", "吸音率 α (オクターブバンド)",
              "Absorption α (octave bands)");
    I18n::reg("rah_col_material", "材質", "Material");
    I18n::reg("rah_add_material", "＋ 材質を追加…", "＋ Add material…");
    I18n::reg("rah_scatter_section", "散乱係数 s (Scattering coefficient)",
              "Scattering coefficient s");
    I18n::reg("rah_scatter_hint",
              "ISO 17497。0=完全鏡面反射、1=完全拡散。幾何音響の反射モデルに直結。",
              "ISO 17497. 0 = fully specular, 1 = fully diffuse. Feeds the "
              "geometrical-acoustics reflection model directly.");
    I18n::reg("rah_col_surface", "表面", "Surface");
    I18n::reg("rah_col_kind", "種別", "Kind");
    I18n::reg("rah_scatter_assign", "表面ごとに散乱係数を割当 (面選択)",
              "Assign scattering per surface (face pick)");
    I18n::reg("rah_scatter_freq", "周波数依存散乱を有効化",
              "Enable frequency-dependent scattering");
    I18n::reg("rah_assign_section", "面への割当", "Surface assignment");
    I18n::reg("rah_col_face", "面", "Face");
    I18n::reg("rah_col_absorber", "吸音材", "Absorber");
    I18n::reg("rah_col_scatterer", "散乱体", "Scatterer");
    // 電気音響設計
    I18n::reg("rah_sr_section", "電気音響設計 (EASE相当)",
              "Sound reinforcement (EASE-like)");
    I18n::reg("rah_sr_hint",
              "スピーカーの配置・向き(エイミング)を最適化し、客席全体で均一な"
              "SPL・高いSTIを実現。拡声系のハウリングマージン(GBF)も評価。",
              "Optimises loudspeaker placement and aiming for uniform SPL and "
              "high STI across the audience. Also evaluates the feedback margin "
              "(GBF) of the reinforcement chain.");
    I18n::reg("rah_ls_section", "スピーカーシステム", "Loudspeaker system");
    I18n::reg("rah_col_model", "機種 (GLL)", "Model (GLL)");
    I18n::reg("rah_col_pos", "位置", "Position");
    I18n::reg("rah_col_aim", "エイミング(方位/仰角)", "Aiming (az/el)");
    I18n::reg("rah_col_gain", "ゲイン", "Gain");
    I18n::reg("rah_add_speaker", "＋ スピーカーを追加…", "＋ Add loudspeaker…");
    I18n::reg("rah_auto_aim", "🎯 自動エイミング最適化",
              "🎯 Auto-aim optimisation");
    I18n::reg("rah_gll_lib", "GLLライブラリ", "GLL library");
    I18n::reg("rah_delay_section", "遅延・ディレイタワー", "Delay");
    I18n::reg("rah_delay_row", "ディレイ設定", "Delay settings");
    I18n::reg("rah_haas", "距離補正を自動適用 (Haas効果)",
              "Auto distance compensation (Haas effect)");
    I18n::reg("rah_delay_hint",
              "C: 12.4ms, F: 3.2ms, ディレイタワー: 68ms (34m地点)",
              "C: 12.4 ms, F: 3.2 ms, delay tower: 68 ms (at 34 m)");
    I18n::reg("rah_sti_section", "客席カバレッジ / STI マッピング",
              "Coverage / STI mapping");
    I18n::reg("rah_sti_caption", "STI分布 (緑=高明瞭度)",
              "STI map (green = high intelligibility)");
    I18n::reg("rah_sti_avg", "STI平均 0.68", "STI mean 0.68");
    I18n::reg("rah_sti_uniform", "±0.05 (均一性良)", "±0.05 (uniform)");
    I18n::reg("rah_gbf_section", "ハウリング余裕 (GBF)",
              "Feedback margin (GBF)");
    I18n::reg("rah_mic_pos", "マイク位置", "Mic position");
    I18n::reg("rah_gbf_hint", "(推奨 >6dB) — リンギング前の余裕",
              "(recommend >6 dB) — margin before ringing");
    I18n::reg("rah_notch", "notchフィルタ自動提案 (自動提案は未実装)",
              "Auto-suggest notch filters (auto-suggestion not implemented)");
    // 出力 (追加ボタン)
    I18n::reg("rah_export_aural", "🎧 各席の可聴化", "🎧 Per-seat auralization");
    I18n::reg("rah_export_ease", "📐 ODEON/EASE 形式エクスポート",
              "📐 Export ODEON/EASE format");
    // ── 表データ / 行ラベル (mock の静的テーブル) ──────────────────────────
    I18n::reg("rah_pit_split", "(分割可)", "(split)");
    I18n::reg("rah_time_s", "時間 [s]", "time [s]");
    I18n::reg("rah_col_recommend", "推奨", "Recommended");
    I18n::reg("rah_on", "ON", "ON");
    I18n::reg("rah_off", "OFF", "OFF");
    I18n::reg("rah_bqi_est",
              "※ オペラ対応ホールの BQI は公表値が無いため ITDG からの推定値 "
              "(表中 * 印, 要実測確認)。",
              "* BQI of the Japanese opera halls is estimated from ITDG "
              "(marked * in the table; needs measurement).");
    I18n::reg("rah_bqi_range", "≥0.55 (優良)", "≥0.55 (excellent)");
    // 空間印象の行
    I18n::reg("rah_lf", "LF (初期側方エネルギー比)",
              "LF (early lateral energy fraction)");
    I18n::reg("rah_lfc", "LFC", "LFC");
    I18n::reg("rah_bqi_row", "1-IACC_E3 (BQI)", "1-IACC_E3 (BQI)");
    I18n::reg("rah_iacc_l", "IACC_L (後期)", "IACC_L (late)");
    I18n::reg("rah_glate", "G_late", "G_late");
    I18n::reg("rah_mean_asw", "ASW (音の幅)", "ASW (source width)");
    I18n::reg("rah_mean_lfc", "同 (余弦重み)", "same (cosine weighted)");
    I18n::reg("rah_mean_bqi", "初期側方反射の質",
              "Quality of early lateral reflections");
    I18n::reg("rah_mean_lev", "LEV (包まれ感)", "LEV (envelopment)");
    I18n::reg("rah_mean_late", "残響エネルギー", "Reverberant energy");
    // ステージ支援 ST の行
    I18n::reg("rah_st_early", "ST_early (ST1)", "ST_early (ST1)");
    I18n::reg("rah_st_late", "ST_late", "ST_late");
    I18n::reg("rah_st_canopy", "上部反射板高さ", "Canopy height");
    I18n::reg("rah_st_m_ensemble", "アンサンブルのしやすさ", "Ease of ensemble");
    I18n::reg("rah_st_m_return", "響きの返り", "Reverberant return");
    I18n::reg("rah_st_m_delay", "初期反射の遅延", "Early-reflection delay");
    // 可変音響の機構
    I18n::reg("rah_va_shell", "音響反射板 (オーケストラシェル)",
              "Acoustic shell (orchestra shell)");
    I18n::reg("rah_va_banner", "吸音バナー (側壁上部)",
              "Absorptive banners (upper side walls)");
    I18n::reg("rah_va_pitlid", "床迫り・オケピット蓋",
              "Stage lift / orchestra-pit lid");
    I18n::reg("rah_va_electronic", "電子残響 (Constellation/ERES相当)",
              "Electronic reverberation (Constellation/ERES-like)");
    I18n::reg("rah_va_deployed", "設置", "Deployed");
    I18n::reg("rah_va_stored", "収納", "Stored");
    I18n::reg("rah_va_unrolled", "展開", "Unrolled");
    I18n::reg("rah_va_rolled", "巻取", "Rolled up");
    I18n::reg("rah_va_concert", "コンサート", "Concert");
    I18n::reg("rah_va_opera", "オペラ", "Opera");
    // 吸音材DB (材質名)
    I18n::reg("rah_m_concrete", "コンクリート打放し", "Bare concrete");
    I18n::reg("rah_m_gypsum", "石膏ボード t12.5", "Gypsum board 12.5 mm");
    I18n::reg("rah_m_wood_floor", "木質フローリング", "Wood flooring");
    I18n::reg("rah_m_carpet", "カーペット (厚手)", "Carpet (thick)");
    I18n::reg("rah_m_gw50", "グラスウール 50mm", "Glass wool 50 mm");
    I18n::reg("rah_m_perf_gw", "有孔ボード + GW", "Perforated board + GW");
    I18n::reg("rah_m_curtain", "音響カーテン", "Acoustic curtain");
    I18n::reg("rah_m_seats_full", "客席 (満席)", "Audience seats (occupied)");
    I18n::reg("rah_m_seats_empty", "客席 (空席)", "Audience seats (empty)");
    // 散乱体DB (表面名 / 種別)
    I18n::reg("rah_s_smooth", "平滑面 (ガラス・塗装)",
              "Smooth surface (glass/paint)");
    I18n::reg("rah_s_smooth_short", "平滑面", "Smooth");
    I18n::reg("rah_s_slight", "軽微な凹凸", "Slight relief");
    I18n::reg("rah_s_qrd", "QRD拡散体", "QRD diffuser");
    I18n::reg("rah_s_poly", "多面体拡散体", "Polyhedral diffuser");
    I18n::reg("rah_s_seats", "客席・不規則面", "Seating / irregular surface");
    I18n::reg("rah_k_specular", "鏡面", "Specular");
    I18n::reg("rah_k_semi", "準鏡面", "Semi-specular");
    I18n::reg("rah_k_diffuse", "拡散", "Diffuse");
    I18n::reg("rah_k_high", "高拡散", "Highly diffuse");
    // 面への割当
    I18n::reg("rah_f_ceiling", "天井", "Ceiling");
    I18n::reg("rah_f_sidewall", "側壁", "Side wall");
    I18n::reg("rah_f_rearwall", "後壁", "Rear wall");
    I18n::reg("rah_f_floor", "床", "Floor");
    I18n::reg("rah_a_panel_t15", "音響パネル T15", "Acoustic panel T15");
    I18n::reg("rah_a_wood_panel", "木質パネル", "Wood panel");
    // 電気音響設計
    I18n::reg("rah_sp_line8", "Line array 8box", "Line array 8box");
    I18n::reg("rah_sp_point", "Point source CD", "Point source CD");
    I18n::reg("rah_sp_front", "Front fill", "Front fill");
    I18n::reg("rah_spl_badge", "SPL 94±2.5 dB", "SPL 94±2.5 dB");
    I18n::reg("rah_gbf_badge", "GBF = 6.2 dB", "GBF = 6.2 dB");
    I18n::reg("rah_mic_default", "0, 1.2, 7.5 (演台)", "0, 1.2, 7.5 (lectern)");
    // 残響式 (Fitzroy)
    I18n::reg("rah_fitzroy", "Fitzroy (非均一)", "Fitzroy (non-uniform)");
    // 騒音源内訳
    I18n::reg("rah_ns_section", "騒音源内訳", "Noise source breakdown");
    I18n::reg("rah_ns_hint",
              "暗騒音を構成する騒音源ごとの寄与と対策。チェックを外した行は"
              "対策済み・対象外として扱う。",
              "Per-source contribution to the background noise and its "
              "countermeasure. Unchecked rows are treated as resolved or out "
              "of scope.");
    I18n::reg("rah_ns_col_source", "音源名", "Source");
    I18n::reg("rah_ns_col_level", "寄与 dB(A)", "Contribution dB(A)");
    I18n::reg("rah_ns_col_measure", "対策", "Countermeasure");
    I18n::reg("rah_ns_add", "＋ 行追加", "＋ Add row");
    I18n::reg("rah_ns_del", "− 行削除", "− Delete row");
    I18n::reg("rah_ns_new", "新規騒音源", "New noise source");
    // 音響障害診断: 改善後の再シミュレーション (試算)
    I18n::reg("rah_resim", "▶ 改善後を再シミュレーション",
              "▶ Re-simulate after improvements");
    I18n::reg("rah_resim_hint",
              "提案をすべて適用した場合の試算 (フラッター対象面 α≥0.30 / "
              "エコー対象面 α≥0.40 に引き上げ)。モデルは変更しません。",
              "Estimate with all proposals applied (raises flutter faces to "
              "α≥0.30 and echo faces to α≥0.40). The model is not modified.");
    I18n::reg("rah_resim_result",
              "提案をすべて適用した場合の試算: RT60(mid) %1 s → %2 s ・ "
              "A@1k %3 Sabin → %4 Sabin ・ 検出障害 %5 件 → %6 件",
              "Estimate with all proposals applied: RT60(mid) %1 s → %2 s · "
              "A@1k %3 Sabin → %4 Sabin · defects %5 → %6");
    return true;
}();

// 読取専用テーブル生成 (q-table 相当)
QTableWidget *makeStaticTable(QWidget *parent, const QStringList &headers,
                              int rows)
{
    auto *t = new QTableWidget(rows, headers.size(), parent);
    t->setHorizontalHeaderLabels(headers);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    t->verticalHeader()->setVisible(false);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setMinimumHeight(rows * 30 + 42);
    return t;
}

void setRowCells(QTableWidget *t, int row, const QStringList &cells)
{
    for (int c = 0; c < cells.size(); ++c)
        t->setItem(row, c, new QTableWidgetItem(cells[c]));
}

// バッジ相当のテーブルセル (前景色のみ最小限)
QTableWidgetItem *badgeItem(const QString &text, const char *color)
{
    auto *it = new QTableWidgetItem(text);
    it->setForeground(QColor(color));
    return it;
}

// バッジ相当の QLabel
QLabel *makeBadge(const QString &text, const char *color, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setStyleSheet(
        QStringLiteral("color:%1; font-weight:600;").arg(QLatin1String(color)));
    return l;
}

// 3桁区切り (mock の toLocaleString 相当)
QString groupNum(double v)
{
    return QLocale(QLocale::English).toString(qint64(qRound64(v)));
}

// mock の CSS クラス色 (badge acc / ok / warn / err)
const char kAcc[]  = "#0078D4";
const char kOk[]   = "#2E8B57";
const char kWarn[] = "#B45309";
const char kErr[]  = "#B91C1C";

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

// 数値セル (右寄せ)
QTableWidgetItem *numItem(const QString &s)
{
    auto *it = new QTableWidgetItem(s);
    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
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

// 「値 + 単位」の Row (mock の <input> + <span className="muted">)
QHBoxLayout *unitRow(QWidget *w, const QString &unit, QWidget *parent)
{
    auto *h = new QHBoxLayout();
    h->addWidget(w);
    h->addWidget(new QLabel(unit, parent));
    h->addStretch(1);
    return h;
}

QDoubleSpinBox *plainSpin(QWidget *parent, double lo, double hi, double value)
{
    auto *w = new QDoubleSpinBox(parent);
    w->setRange(lo, hi);
    w->setDecimals(0);
    w->setValue(value);
    w->setMaximumWidth(110);
    return w;
}

// ── ホールプリセットの正規化ビュー ─────────────────────────────────────────
// コンサートホール表 (ConcertHall) とオペラホール表 (OperaHall) は列が違うので、
// 派生表示 (IR解析 / 空間印象 / ステージ …) が共通に使える形へ畳む。
struct HallView {
    QString name, type, info, note;
    double  V = 0;
    double  RT = 0, EDT = 0, C80 = 0, G = 0;
    int     ITDG = 0;
    double  BQI = 0;
    bool    bqiEstimated = false;   // オペラ表は BQI 非公表 → ITDG から推定
    bool    measured = true;        // RT の出典 (実測 / 推定)
    bool    closed = false;
    QString closure, pit, ensemble, stage;
};

HallView currentHallView(bool opera, int concertIdx, int operaIdx)
{
    using ofd::I18n;
    HallView h;
    if (!opera) {
        const ofd::halls::ConcertHall &c = ofd::halls::kConcertHalls[
            qBound(0, concertIdx, ofd::halls::kConcertHallCount - 1)];
        h.name = QString::fromUtf8(c.name);
        h.type = QString::fromUtf8(c.type);
        h.note = QString::fromUtf8(c.note);
        h.info = I18n::tr("rah_concert_info")
                     .arg(QString::fromUtf8(c.dims), groupNum(c.V),
                          groupNum(c.N));
        h.V = c.V;
        h.RT = c.RT; h.EDT = c.EDT; h.C80 = c.C80; h.G = c.G;
        h.ITDG = c.ITDG; h.BQI = c.BQI;
        return h;
    }
    const ofd::halls::OperaHall &o = ofd::halls::kOperaHalls[
        qBound(0, operaIdx, ofd::halls::kOperaHallCount - 1)];
    h.name = QString::fromUtf8(o.name);
    h.type = QString::fromUtf8(o.type);
    h.note = QString::fromUtf8(o.note);
    h.info = I18n::tr("rah_opera_info")
                 .arg(QString::number(o.opened), groupNum(o.seats),
                      groupNum(o.volume_m3));
    h.V = o.volume_m3;
    h.RT = o.RT_occupied; h.EDT = o.EDT; h.C80 = o.C80; h.G = o.G;
    h.ITDG = o.ITDG;
    // BQI 非公表 → コンサート5ホールの傾向 (BQI ≈ 0.70 − 0.005·ITDG) で推定。
    h.BQI = qBound(0.40, 0.70 - 0.005 * o.ITDG, 0.75);
    h.bqiEstimated = true;
    h.measured = o.rtMeasured;
    h.closed = o.closed;
    h.closure = QString::fromUtf8(o.closure);
    h.pit = QString::fromUtf8(o.pitType)
            + (o.pitSplit ? " " + I18n::tr("rah_pit_split") : QString());
    h.ensemble = QString::fromUtf8(o.pitCapacity);
    h.stage = QString::fromUtf8(o.stage);
    return h;
}

// 電気音響設計の STI 分布ラスタ (mock の 12×16 グリッド SVG 相当)。
// mock の乱数項は再現性のため座標ハッシュで代用する。
class StiMapWidget : public QWidget {
public:
    explicit StiMapWidget(QWidget *parent = nullptr) : QWidget(parent)
    {
        setMinimumSize(300, 160);
        setMaximumWidth(360);
    }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.fillRect(rect(), palette().base());
        p.setPen(QPen(palette().mid().color(), 1));
        p.drawRect(rect().adjusted(0, 0, -1, -1));
        p.save();
        p.scale(width() / 300.0, height() / 160.0);
        for (int r = 0; r < 12; ++r) {
            for (int c = 0; c < 16; ++c) {
                double n = std::sin(c * 12.9898 + r * 78.233) * 43758.5453;
                n -= std::floor(n);                       // 0..1 疑似乱数
                const double sti = 0.72
                    - std::hypot(c - 8.0, r - 2.0) * 0.012 + (n - 0.5) * 0.02;
                const double g = qBound(0.0, (sti - 0.45) / 0.3, 1.0);
                p.fillRect(QRectF(10 + c * 17, 10 + r * 11, 16, 10),
                           QColor(int(220 - g * 180), int(60 + g * 150), 40));
            }
        }
        p.setPen(palette().text().color());
        QFont f = p.font();
        f.setPointSizeF(7.5);
        p.setFont(f);
        p.drawText(QRectF(0, 144, 300, 14), Qt::AlignCenter,
                   ofd::I18n::tr("rah_sti_caption"));
        p.restore();
    }
};

} // namespace

// 受音点の相対位置 (奥行き比, 幅比) — mock の P1..P4 に対応
static const struct { double dl, dw; const char *key; } kReceivers[4] = {
    { 0.30, 0.50, "ra_p1" },   // 中央前列
    { 0.60, 0.20, "ra_p2" },   // 左サイド
    { 0.60, 0.80, "ra_p3" },   // 右サイド
    { 0.85, 0.50, "ra_p4" },   // 後方中央
};

// ── CoverageMap ─────────────────────────────────────────────────────────────
CoverageMap::CoverageMap(Project *project, QWidget *parent)
    : QWidget(parent), m_p(project)
{
    setMinimumSize(360, 250);
    recompute();
}

double CoverageMap::cellValue(double r) const
{
    const AcousticOpts &a = m_p->acoustic();
    auto metricAt = [&](int band) {
        const double T = rt60(a, band);
        const SeatMetrics m = seatMetrics(r, T, a.volume);
        switch (m_metric) {
            case 0: return m.G;
            case 1: return m.C80;
            case 2: return m.STI;
            default: return m.RT;
        }
    };
    if (m_band >= 6) {   // 平均
        double s = 0;
        for (int b = 0; b < 6; ++b) s += metricAt(b);
        return s / 6.0;
    }
    return metricAt(m_band);
}

void CoverageMap::recompute()
{
    const AcousticOpts &a = m_p->acoustic();
    m_values.clear();
    double sum = 0, sum2 = 0;
    int n = 0;
    for (int row = 0; row <= 10; ++row) {
        const double t = row / 10.0;
        for (int col = 0; col <= 10; ++col) {
            const double halfW = (0.15 + 0.35 * t) * a.roomW;
            const double x = (col - 5) / 5.0 * halfW;
            if (std::fabs(x) > halfW + 1e-9) { m_values.push_back(NAN); continue; }
            const double y = 2.0 + t * (a.roomL - 4.0);   // 舞台前 2m から
            const double r = std::sqrt(x * x + y * y);
            const double v = cellValue(r);
            m_values.push_back(v);
            sum += v; sum2 += v * v; ++n;
        }
    }
    m_mean = n ? sum / n : 0;
    m_std = n ? std::sqrt(std::max(0.0, sum2 / n - m_mean * m_mean)) : 0;
    update();
}

void CoverageMap::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), palette().base());
    p.setPen(QPen(palette().mid().color(), 1));
    p.drawRect(rect().adjusted(0, 0, -1, -1));

    const double W = width(), H = height();
    // 扇形ホール外形 (mock と同じ構図)
    QPolygonF hall;
    hall << QPointF(W/2, 18) << QPointF(W*0.17, H-30) << QPointF(W*0.83, H-30);
    p.setPen(QPen(palette().text().color(), 1.2));
    p.setBrush(Qt::NoBrush);
    p.drawPolygon(hall);
    // 舞台
    p.setBrush(QColor(146, 64, 14, 150));
    p.setPen(Qt::NoPen);
    p.drawRect(QRectF(W/2 - 30, 12, 60, 14));
    p.setPen(palette().text().color());
    QFont f = p.font(); f.setPointSizeF(7.5); p.setFont(f);
    p.drawText(QRectF(W/2 - 30, 0, 60, 12), Qt::AlignCenter, "STAGE");

    // 値レンジ (色スケール正規化)
    double lo = 1e300, hi = -1e300;
    for (double v : m_values)
        if (!std::isnan(v)) { lo = std::min(lo, v); hi = std::max(hi, v); }
    if (lo >= hi) { lo -= 1; hi += 1; }

    // セル
    int idx = 0;
    for (int row = 0; row <= 10; ++row) {
        const double t = row / 10.0;
        for (int col = 0; col <= 10; ++col, ++idx) {
            const double v = m_values.value(idx, NAN);
            if (std::isnan(v)) continue;
            const double halfWpx = (0.13 + 0.33 * t) * W;
            const double cx = W/2 + (col - 5) / 5.0 * halfWpx;
            const double cy = 34 + t * (H - 70);
            if (std::fabs(cx - W/2) > halfWpx) continue;
            double norm = (v - lo) / (hi - lo);
            if (m_metric == 3) norm = 1.0 - norm;   // RT は短いほど「良」= 緑
            const QColor c = QColor::fromHsl(int(norm * 120), 178, 128);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(c.red(), c.green(), c.blue(), 200));
            p.drawRoundedRect(QRectF(cx - 9, cy - 7, 18, 15), 2, 2);
        }
    }

    // 受音点
    for (const auto &r : kReceivers) {
        const double t = r.dl;
        const double halfWpx = (0.13 + 0.33 * t) * W;
        const double cx = W/2 + (r.dw - 0.5) * 2.0 * halfWpx;
        const double cy = 34 + t * (H - 70);
        p.setPen(QPen(Qt::black, 1));
        p.setBrush(Qt::white);
        p.drawEllipse(QPointF(cx, cy), 3.5, 3.5);
    }

    // カラースケール
    QLinearGradient grad(W - 130, 0, W - 10, 0);
    grad.setColorAt(0, QColor::fromHsl(0, 178, 128));
    grad.setColorAt(0.5, QColor::fromHsl(60, 178, 128));
    grad.setColorAt(1, QColor::fromHsl(120, 178, 128));
    p.setPen(Qt::NoPen);
    p.setBrush(grad);
    p.drawRect(QRectF(W - 130, H - 22, 120, 10));
    p.setPen(palette().text().color());
    const bool inv = (m_metric == 3);
    p.drawText(QPointF(W - 130, H - 26), QString::number(inv ? hi : lo, 'g', 3));
    p.drawText(QPointF(W - 40, H - 26), QString::number(inv ? lo : hi, 'g', 3));
}

// ── RoomAcousticsTab ────────────────────────────────────────────────────────
RoomAcousticsTab::RoomAcousticsTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    auto *hint = new QLabel(I18n::tr("ra_model_hint"), body);
    hint->setWordWrap(true);
    v->addWidget(hint);

    // ホールモデル / Hall preset (サブタブ全体の前提 → タブの外に置く)
    v->addWidget(buildHallPresetSection());

    m_tabs = new QTabWidget(body);
    m_tabs->setDocumentMode(true);
    m_tabs->addTab(buildCoveragePage(),  I18n::tr("ra_tab_coverage"));
    m_tabs->addTab(buildEchogramPage(),  I18n::tr("ra_tab_echogram"));
    m_tabs->addTab(buildIRPage(),        I18n::tr("rah_tab_ir"));
    m_tabs->addTab(buildReverbPage(),    I18n::tr("ra_tab_reverb"));
    m_tabs->addTab(buildSpatialPage(),   I18n::tr("rah_tab_spatial"));
    m_tabs->addTab(buildStagePage(),     I18n::tr("rah_tab_stage"));
    m_tabs->addTab(buildMaterialsPage(), I18n::tr("rah_tab_materials"));
    m_tabs->addTab(buildReinforcePage(), I18n::tr("rah_tab_reinforce"));
    m_tabs->addTab(buildNoisePage(),     I18n::tr("ra_tab_noise"));
    m_tabs->addTab(buildDefectsPage(),   I18n::tr("ra_tab_defects"));
    v->addWidget(m_tabs);

    // export
    auto *sExp = new SectionBox(I18n::tr("ra_export"), body);
    auto *row = new QHBoxLayout();
    auto *repBtn = new QPushButton(I18n::tr("ra_export_report"), sExp);
    auto *pngBtn = new QPushButton(I18n::tr("ra_export_png"), sExp);
    row->addWidget(repBtn);
    row->addWidget(pngBtn);
    // 可聴化 / ODEON・EASE エクスポートは未配線 (絶対規則 5)
    auto *auralBtn = new QPushButton(I18n::tr("rah_export_aural"), sExp);
    auto *easeBtn  = new QPushButton(I18n::tr("rah_export_ease"), sExp);
    tabhelp::markNotImplemented(auralBtn);
    tabhelp::markNotImplemented(easeBtn);
    row->addWidget(auralBtn);
    row->addWidget(easeBtn);
    row->addStretch(1);
    sExp->vbox()->addLayout(row);
    v->addWidget(sExp);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    connect(repBtn, &QPushButton::clicked, this, &RoomAcousticsTab::exportReport);
    connect(pngBtn, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getSaveFileName(
            this, I18n::tr("ra_export_png"), "coverage.png", "PNG (*.png)");
        if (!path.isEmpty()) m_map->grab().save(path);
    });

    connect(project, &Project::loaded, this, &RoomAcousticsTab::refresh);
    refresh();
}

void RoomAcousticsTab::sourcePos(double out[3]) const
{
    const AcousticOpts &a = m_p->acoustic();
    out[0] = 0.05 * a.roomL;
    out[1] = 0.50 * a.roomW;
    out[2] = 1.5;
}

void RoomAcousticsTab::receiverPos(int index, double out[3]) const
{
    const AcousticOpts &a = m_p->acoustic();
    index = qBound(0, index, 3);
    out[0] = kReceivers[index].dl * a.roomL;
    out[1] = kReceivers[index].dw * a.roomW;
    out[2] = 1.2;
}

// ── page builders ───────────────────────────────────────────────────────────
// ホールモデル / Hall preset — 実在ホールの実測データ (core/OperaHalls.h)。
// カテゴリ (世界のコンサートホール / 日本のオペラ対応ホール) を排他ボタンで
// 切替え、選択されたホールの公表値を表示する。V だけ AcousticOpts に反映。
QWidget *RoomAcousticsTab::buildHallPresetSection()
{
    auto *s = new SectionBox(I18n::tr("rah_hall_section"));

    // カテゴリ (mock の <Seg>) — 排他 checkable ボタン行
    auto *cat = new QHBoxLayout();
    m_catConcert = new QPushButton(I18n::tr("rah_cat_concert"), s);
    m_catOpera   = new QPushButton(I18n::tr("rah_cat_opera"), s);
    for (auto *b : { m_catConcert, m_catOpera }) {
        b->setCheckable(true);
        b->setStyleSheet("padding:2px 10px;");
        cat->addWidget(b);
    }
    m_catConcert->setChecked(true);
    cat->addStretch(1);
    s->vbox()->addLayout(cat);

    // ── 世界のコンサートホール ──
    m_concertPane = new QWidget(s);
    auto *cv = new QVBoxLayout(m_concertPane);
    cv->setContentsMargins(0, 0, 0, 0);
    cv->setSpacing(4);
    auto *crow = new QHBoxLayout();
    m_hallBox = new QComboBox(m_concertPane);
    for (int i = 0; i < halls::kConcertHallCount; ++i)
        m_hallBox->addItem(QString::fromUtf8(halls::kConcertHalls[i].name));
    crow->addWidget(m_hallBox, 1);
    auto *imp3dConcert = new QPushButton(I18n::tr("rah_import_3d"),
                                         m_concertPane);
    tabhelp::markNotImplemented(imp3dConcert);   // 3D モデル取込は未配線
    crow->addWidget(imp3dConcert);
    cv->addLayout(crow);

    auto *cinfo = new QHBoxLayout();
    m_hallType = makeBadge(QString(), kAcc, m_concertPane);
    m_hallInfo = makeHint(QString(), m_concertPane);
    cinfo->addWidget(m_hallType);
    cinfo->addWidget(m_hallInfo, 1);
    cv->addLayout(cinfo);

    m_hallMetrics = makeStaticTable(m_concertPane,
        { I18n::tr("rah_col_rt_occ"), "EDT", "C80", I18n::tr("rah_col_g"),
          "ITDG", "BQI" }, 1);
    cv->addWidget(m_hallMetrics);
    m_hallNote = makeHint(QString(), m_concertPane);
    cv->addWidget(m_hallNote);
    s->vbox()->addWidget(m_concertPane);

    // ── 日本のオペラ対応ホール ──
    m_operaPane = new QWidget(s);
    auto *ov = new QVBoxLayout(m_operaPane);
    ov->setContentsMargins(0, 0, 0, 0);
    ov->setSpacing(4);
    auto *orow = new QHBoxLayout();
    m_operaBox = new QComboBox(m_operaPane);
    for (int i = 0; i < halls::kOperaHallCount; ++i) {
        const halls::OperaHall &o = halls::kOperaHalls[i];
        m_operaBox->addItem(QString::fromUtf8(o.closed ? "⚠ " : "")
            + QStringLiteral("[%1] %2").arg(QString::fromUtf8(o.region),
                                            QString::fromUtf8(o.name)));
    }
    orow->addWidget(m_operaBox, 1);
    auto *imp3dOpera = new QPushButton(I18n::tr("rah_import_3d"), m_operaPane);
    tabhelp::markNotImplemented(imp3dOpera);     // 3D モデル取込は未配線
    orow->addWidget(imp3dOpera);
    ov->addLayout(orow);

    auto *oinfo = new QHBoxLayout();
    m_operaType   = makeBadge(QString(), kAcc, m_operaPane);
    m_operaInfo   = makeHint(QString(), m_operaPane);
    m_operaClosed = makeBadge(QString(), kErr, m_operaPane);
    oinfo->addWidget(m_operaType);
    oinfo->addWidget(m_operaInfo, 1);
    oinfo->addWidget(m_operaClosed);
    ov->addLayout(oinfo);

    m_operaMetrics = makeStaticTable(m_operaPane,
        { I18n::tr("rah_col_rt_occ"), "EDT", "C80", "G", "ITDG",
          I18n::tr("rah_col_source") }, 1);
    ov->addWidget(m_operaMetrics);
    m_operaPit = makeStaticTable(m_operaPane,
        { I18n::tr("rah_col_pit"), I18n::tr("rah_col_ensemble"),
          I18n::tr("rah_col_stage") }, 1);
    ov->addWidget(m_operaPit);
    m_operaNote = makeHint(QString(), m_operaPane);
    ov->addWidget(m_operaNote);

    auto *scn = new QHBoxLayout();
    scn->addWidget(new QLabel(I18n::tr("rah_scenario"), m_operaPane));
    scn->addWidget(makeCheck(I18n::tr("rah_scn_pit_balance"), true, m_operaPane));
    scn->addWidget(makeCheck(I18n::tr("rah_scn_singer_pit"), true, m_operaPane));
    scn->addWidget(makeCheck(I18n::tr("rah_scn_pit_lid"), false, m_operaPane));
    scn->addStretch(1);
    ov->addLayout(scn);
    s->vbox()->addWidget(m_operaPane);
    m_operaPane->setVisible(false);

    // 共通: 実測 vs シミュレーションの検証実行
    auto *run = new QHBoxLayout();
    auto *runBtn = new QPushButton(I18n::tr("rah_run_hall"), s);
    runBtn->setStyleSheet("font-weight:600;");
    tabhelp::markNotImplemented(runBtn);   // FDTD/Ray 実行は未配線
    run->addWidget(runBtn);
    run->addWidget(makeHint(I18n::tr("rah_run_hint"), s), 1);
    s->vbox()->addLayout(run);

    // 選択変更 → プリセットの V を AcousticOpts へ
    connect(m_hallBox, &QComboBox::currentIndexChanged, this,
            &RoomAcousticsTab::applyHallPreset);
    connect(m_operaBox, &QComboBox::currentIndexChanged, this,
            &RoomAcousticsTab::applyHallPreset);
    auto pickCat = [this](bool opera) {
        m_catConcert->setChecked(!opera);
        m_catOpera->setChecked(opera);
        m_concertPane->setVisible(!opera);
        m_operaPane->setVisible(opera);
        applyHallPreset();
    };
    connect(m_catConcert, &QPushButton::clicked, this,
            [pickCat] { pickCat(false); });
    connect(m_catOpera, &QPushButton::clicked, this,
            [pickCat] { pickCat(true); });
    return s;
}

QWidget *RoomAcousticsTab::buildCoveragePage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setSpacing(8);

    auto *s = new SectionBox(I18n::tr("ra_coverage_section"), page);
    auto *hint = new QLabel(I18n::tr("ra_coverage_hint"), s);
    hint->setWordWrap(true);
    s->vbox()->addWidget(hint);

    m_metricBox = new QComboBox(s);
    m_metricBox->addItems({ "G (SPL) [dB]", "C80 [dB]", "STI", "RT60 [s]" });
    m_bandBox = new QComboBox(s);
    m_bandBox->addItems({ "125Hz", "250Hz", "500Hz", "1kHz", "2kHz", "4kHz",
                          I18n::tr("ra_band_avg") });
    m_bandBox->setCurrentIndex(3);
    s->form()->addRow(I18n::tr("ra_metric"), m_metricBox);
    s->form()->addRow(I18n::tr("ra_band"), m_bandBox);
    v->addWidget(s);

    auto *sm = new SectionBox(I18n::tr("ra_map_section"), page);
    auto *h = new QHBoxLayout();
    m_map = new CoverageMap(m_p, sm);
    h->addWidget(m_map, 1);
    m_covStats = new QLabel(sm);
    m_covStats->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    h->addWidget(m_covStats);
    sm->vbox()->addLayout(h);
    v->addWidget(sm);

    auto *st = new SectionBox(I18n::tr("ra_seat_section"), page);
    m_seatTable = new QTableWidget(4, 7, st);
    m_seatTable->setHorizontalHeaderLabels({
        I18n::tr("ra_receiver"), "G [dB]", "C80 [dB]", "D50", "STI",
        "RT60 [s]", I18n::tr("ra_verdict") });
    m_seatTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_seatTable->verticalHeader()->setVisible(false);
    m_seatTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_seatTable->setMinimumHeight(140);
    st->vbox()->addWidget(m_seatTable);
    // ▸ 実測 (公表値) — ホールプリセット由来の検証目標 (refreshHallDerived)
    m_covRefNote = makeHint(QString(), st);
    st->vbox()->addWidget(m_covRefNote);
    v->addWidget(st);
    v->addStretch(1);

    connect(m_metricBox, &QComboBox::currentIndexChanged, this, [this](int i) {
        m_map->setMetric(i);
        recomputeAll();
    });
    connect(m_bandBox, &QComboBox::currentIndexChanged, this, [this](int i) {
        m_map->setBand(i);
        recomputeAll();
    });
    return page;
}

QWidget *RoomAcousticsTab::buildEchogramPage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setSpacing(8);

    auto *s = new SectionBox(I18n::tr("ra_echo_section"), page);
    auto *hint = new QLabel(I18n::tr("ra_echo_hint"), s);
    hint->setWordWrap(true);
    s->vbox()->addWidget(hint);
    m_rcvBox = new QComboBox(s);
    for (const auto &r : kReceivers)
        m_rcvBox->addItem(I18n::tr(r.key));
    s->form()->addRow(I18n::tr("ra_receiver"), m_rcvBox);
    v->addWidget(s);

    auto *sp = new SectionBox(I18n::tr("ra_reflectogram"), page);
    m_echoPlot = new MiniPlot(sp);
    m_echoPlot->setImpulseMode(true);
    m_echoPlot->setLabels(I18n::tr("ra_time_ms"), I18n::tr("ra_level_db"));
    m_echoPlot->setMinimumHeight(150);
    sp->vbox()->addWidget(m_echoPlot);
    m_itdgLabel = new QLabel(sp);
    sp->vbox()->addWidget(m_itdgLabel);
    v->addWidget(sp);

    auto *st = new SectionBox(I18n::tr("ra_refl_section"), page);
    m_reflTable = new QTableWidget(0, 5, st);
    m_reflTable->setHorizontalHeaderLabels({
        I18n::tr("ra_reflection"), I18n::tr("ra_delay_ms"),
        I18n::tr("ra_level_db"), I18n::tr("ra_refl_surface"),
        I18n::tr("ra_verdict") });
    m_reflTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_reflTable->verticalHeader()->setVisible(false);
    m_reflTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_reflTable->setMinimumHeight(170);
    st->vbox()->addWidget(m_reflTable);
    v->addWidget(st);
    v->addStretch(1);

    connect(m_rcvBox, &QComboBox::currentIndexChanged, this,
            &RoomAcousticsTab::recomputeAll);
    return page;
}

// IR解析 — Schroeder 逆積分の減衰曲線 + 帯域別指標 + 実測 vs シミュレーション。
// 値はホールプリセットの公表値からの派生 (refreshHallDerived が更新)。
QWidget *RoomAcousticsTab::buildIRPage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setSpacing(8);

    auto *s = new SectionBox(I18n::tr("rah_ir_section"), page);
    s->vbox()->addWidget(makeHint(I18n::tr("rah_ir_hint"), s));
    auto *srcBox = new QComboBox(s);
    srcBox->addItems({ I18n::tr("rah_ir_src_sim"), I18n::tr("rah_ir_src_meas") });
    s->form()->addRow(I18n::tr("rah_ir_source"), srcBox);

    auto *fileRow = new QHBoxLayout();
    auto *fileEdit = new QLineEdit("measured_IR_P1_sweep.wav", s);
    auto *browse = new QPushButton(I18n::tr("rah_browse"), s);
    fileRow->addWidget(fileEdit, 1);
    fileRow->addWidget(browse);
    s->form()->addRow(I18n::tr("rah_ir_file"), fileRow);

    auto *invRow = new QHBoxLayout();
    invRow->addWidget(makeCheck(I18n::tr("rah_ess"), true, s));
    invRow->addWidget(makeCheck(I18n::tr("rah_harm_sep"), false, s));
    invRow->addStretch(1);
    s->form()->addRow(I18n::tr("rah_inv_filter"), invRow);
    v->addWidget(s);

    auto *sd = new SectionBox(I18n::tr("rah_schroeder_section"), page);
    m_schroederPlot = new MiniPlot(sd);
    m_schroederPlot->setLabels(I18n::tr("rah_time_s"), I18n::tr("ra_level_db"));
    m_schroederPlot->setYRange(-80, 0);
    m_schroederPlot->setMinimumHeight(150);
    sd->vbox()->addWidget(m_schroederPlot);
    m_irBandTable = makeStaticTable(sd,
        { I18n::tr("rah_col_metric"), "125", "250", "500", "1k", "2k",
          "4k [Hz]" }, 6);
    sd->vbox()->addWidget(m_irBandTable);
    // 帯域別指標はプリセット公表値+固定係数の派生 (IR 解析は未実装)
    sd->vbox()->addWidget(tabhelp::sampleNote(sd));
    sd->vbox()->addWidget(makeCheck(I18n::tr("rah_t20t30_warn"), true, sd));
    auto *inrCheck = makeCheck(I18n::tr("rah_inr_check"), true, sd);
    tabhelp::markNotImplemented(inrCheck);   // INR 検査は未実装・未使用
    sd->vbox()->addWidget(inrCheck);
    v->addWidget(sd);

    auto *sv = new SectionBox(I18n::tr("rah_validation_section"), page);
    m_irValTable = makeStaticTable(sv,
        { I18n::tr("rah_col_metric1k"), I18n::tr("rah_col_meas"),
          I18n::tr("rah_col_sim"), I18n::tr("rah_col_diff"), "JND",
          I18n::tr("ra_verdict") }, 3);
    sv->vbox()->addWidget(m_irValTable);
    // 「シミュ」列は実行結果ではなく固定係数による見本 (絶対規則 5)
    sv->vbox()->addWidget(tabhelp::sampleNote(sv));
    v->addWidget(sv);
    v->addStretch(1);

    connect(browse, &QPushButton::clicked, this, [this, fileEdit] {
        const QString p = QFileDialog::getOpenFileName(
            this, I18n::tr("rah_ir_file"), QString(), "WAV (*.wav)");
        if (!p.isEmpty()) fileEdit->setText(p);
    });
    return page;
}

QWidget *RoomAcousticsTab::buildReverbPage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setSpacing(8);

    auto *s = new SectionBox(I18n::tr("ra_reverb_section"), page);
    auto *hint = new QLabel(I18n::tr("ra_reverb_hint"), s);
    hint->setWordWrap(true);
    s->vbox()->addWidget(hint);

    auto dsb = [&s](double lo, double hi, const char *suffix) {
        auto *w = new QDoubleSpinBox(s);
        w->setRange(lo, hi);
        w->setDecimals(1);
        w->setSuffix(QString::fromUtf8(suffix));
        return w;
    };
    m_roomL = dsb(2, 500, " m");
    m_roomW = dsb(2, 500, " m");
    m_roomH = dsb(2, 100, " m");
    m_volume = dsb(10, 1e6, " m³");
    m_surface = dsb(10, 1e6, " m²");
    auto *dims = new QHBoxLayout();
    dims->addWidget(new QLabel("L", s)); dims->addWidget(m_roomL);
    dims->addWidget(new QLabel("W", s)); dims->addWidget(m_roomW);
    dims->addWidget(new QLabel("H", s)); dims->addWidget(m_roomH);
    auto *fromDims = new QPushButton(I18n::tr("ra_from_dims"), s);
    dims->addWidget(fromDims);
    dims->addStretch(1);
    s->form()->addRow(I18n::tr("ra_room_dims"), dims);
    s->form()->addRow(I18n::tr("ra_volume"), m_volume);
    s->form()->addRow(I18n::tr("ra_surface"), m_surface);

    m_occupancy = new QComboBox(s);
    m_occupancy->addItems({ I18n::tr("ra_occ_empty"), I18n::tr("ra_occ_half"),
                            I18n::tr("ra_occ_full") });
    m_formula = new QComboBox(s);
    m_formula->addItems({ "Sabine", I18n::tr("ra_eyring"),
                          I18n::tr("rah_fitzroy") });
    s->form()->addRow(I18n::tr("ra_occupancy"), m_occupancy);
    s->form()->addRow(I18n::tr("ra_formula"), m_formula);
    v->addWidget(s);

    auto *sb = new SectionBox(I18n::tr("ra_budget_section"), page);
    m_budget = new QTableWidget(0, 8, sb);
    m_budget->setHorizontalHeaderLabels({
        "", I18n::tr("ra_element"), I18n::tr("ra_area"),
        "α125", "α500", "α1k", "α4k", "A@1k" });
    m_budget->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_budget->horizontalHeader()->setStretchLastSection(true);
    m_budget->verticalHeader()->setVisible(false);
    m_budget->setMinimumHeight(200);
    sb->vbox()->addWidget(m_budget);
    v->addWidget(sb);

    auto *sr = new SectionBox(I18n::tr("ra_rt_section"), page);
    m_rtPlot = new MiniPlot(sr);
    m_rtPlot->setLabels("f [Hz]", "RT60 [s]");
    m_rtPlot->setXTickPow10(true);
    m_rtPlot->setMinimumHeight(140);
    sr->vbox()->addWidget(m_rtPlot);
    m_rtBadge = new QLabel(sr);
    m_rtBadge->setStyleSheet("font-weight:600; font-size:13px;");
    sr->vbox()->addWidget(m_rtBadge);
    auto *targets = new QLabel(I18n::tr("ra_rt_targets"), sr);
    targets->setWordWrap(true);
    sr->vbox()->addWidget(targets);
    v->addWidget(sr);
    v->addStretch(1);

    auto applyScalar = [this] {
        if (m_updating) return;
        AcousticOpts &a = m_p->acoustic();
        a.roomL = m_roomL->value();
        a.roomW = m_roomW->value();
        a.roomH = m_roomH->value();
        a.volume = m_volume->value();
        a.surface = m_surface->value();
        a.occupancy = m_occupancy->currentIndex();
        a.rtFormula = m_formula->currentIndex();
        refreshBudgetDerived();
        recomputeAll();
        m_p->touch();
    };
    for (auto *w : { m_roomL, m_roomW, m_roomH, m_volume, m_surface })
        connect(w, &QDoubleSpinBox::valueChanged, this, applyScalar);
    connect(m_occupancy, &QComboBox::currentIndexChanged, this, applyScalar);
    connect(m_formula, &QComboBox::currentIndexChanged, this, applyScalar);
    connect(fromDims, &QPushButton::clicked, this, [this] {
        AcousticOpts &a = m_p->acoustic();
        const double L = m_roomL->value(), W = m_roomW->value(),
                     H = m_roomH->value();
        a.volume = L * W * H;
        a.surface = 2.0 * (L * W + L * H + W * H);
        refresh();
        recomputeAll();
        m_p->touch();
    });
    connect(m_budget, &QTableWidget::cellChanged, this, [this] {
        if (m_updating) return;
        applyBudgetTable();
        refreshBudgetDerived();
        recomputeAll();
        m_p->touch();
    });
    return page;
}

// 空間印象 — LF / LFC / BQI (1-IACC_E3) / IACC_late / G_late (ISO 3382-1)。
QWidget *RoomAcousticsTab::buildSpatialPage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setSpacing(8);

    auto *s = new SectionBox(I18n::tr("rah_spatial_section"), page);
    s->vbox()->addWidget(makeHint(I18n::tr("rah_spatial_hint"), s));
    auto *rm = new QHBoxLayout();
    rm->addWidget(makeCheck(I18n::tr("rah_fig8"), true, s));
    rm->addWidget(makeCheck(I18n::tr("rah_binaural"), true, s));
    rm->addStretch(1);
    s->form()->addRow(I18n::tr("rah_rcv_model"), rm);
    v->addWidget(s);

    auto *sr = new SectionBox(I18n::tr("rah_results_section"), page);
    m_spatialTable = makeStaticTable(sr,
        { I18n::tr("rah_col_metric"), I18n::tr("rah_col_value"),
          I18n::tr("rah_col_range"), I18n::tr("rah_col_meaning"),
          I18n::tr("ra_verdict") }, 5);
    sr->vbox()->addWidget(m_spatialTable);
    // LF/LFC/IACC は回帰推定・固定値の見本 (IACC 計算は未実装 — 絶対規則 5)
    sr->vbox()->addWidget(tabhelp::sampleNote(sr));
    sr->vbox()->addWidget(makeHint(I18n::tr("rah_bqi_note"), sr));
    sr->vbox()->addWidget(makeHint(I18n::tr("rah_bqi_est"), sr));
    v->addWidget(sr);

    auto *sm = new SectionBox(I18n::tr("rah_seatmap_section"), page);
    sm->vbox()->addWidget(makeHint(I18n::tr("rah_seatmap_hint"), sm));
    auto *maps = new QHBoxLayout();
    // LF/BQI マップ表示は未配線 (絶対規則 5)
    auto *lfMapBtn  = new QPushButton(I18n::tr("rah_lf_map_btn"), sm);
    auto *bqiMapBtn = new QPushButton(I18n::tr("rah_bqi_map_btn"), sm);
    tabhelp::markNotImplemented(lfMapBtn);
    tabhelp::markNotImplemented(bqiMapBtn);
    maps->addWidget(lfMapBtn);
    maps->addWidget(bqiMapBtn);
    maps->addStretch(1);
    sm->vbox()->addLayout(maps);
    v->addWidget(sm);
    v->addStretch(1);
    return page;
}

// ステージ支援 ST + 可変音響 (可動反射板/バナー/電子残響) + カップルドボリューム。
QWidget *RoomAcousticsTab::buildStagePage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setSpacing(8);

    // ── ステージ音響支援 (ST)
    auto *s = new SectionBox(I18n::tr("rah_st_section"), page);
    s->vbox()->addWidget(makeHint(I18n::tr("rah_st_hint"), s));
    auto *st = makeStaticTable(s,
        { I18n::tr("rah_col_metric"), I18n::tr("rah_col_value"),
          I18n::tr("rah_col_recommend"), I18n::tr("rah_col_meaning"),
          I18n::tr("ra_verdict") }, 3);
    static const struct { const char *key, *val, *rec, *mean, *verdict; }
    kSt[3] = {
        { "rah_st_early",  "-12.8 dB", "-14 〜 -12", "rah_st_m_ensemble",
          "rah_ok" },
        { "rah_st_late",   "-14.5 dB", "-16 〜 -13", "rah_st_m_return",
          "rah_ok" },
        { "rah_st_canopy", "9.5 m",    "8〜12 m",    "rah_st_m_delay",
          "rah_apt" },
    };
    for (int r = 0; r < 3; ++r) {
        st->setItem(r, 0, new QTableWidgetItem(I18n::tr(kSt[r].key)));
        st->setItem(r, 1, numItem(QString::fromUtf8(kSt[r].val)));
        st->setItem(r, 2, numItem(QString::fromUtf8(kSt[r].rec)));
        st->setItem(r, 3, new QTableWidgetItem(I18n::tr(kSt[r].mean)));
        st->setItem(r, 4, badgeItem(I18n::tr(kSt[r].verdict), kOk));
    }
    s->vbox()->addWidget(st);
    s->vbox()->addWidget(makeCheck(I18n::tr("rah_st_grid"), true, s));
    s->vbox()->addWidget(makeCheck(I18n::tr("rah_st_matrix"), false, s));
    v->addWidget(s);

    // ── 可変音響
    auto *sv = new SectionBox(I18n::tr("rah_va_section"), page);
    sv->vbox()->addWidget(makeHint(I18n::tr("rah_va_hint"), sv));
    auto *va = makeStaticTable(sv,
        { "", I18n::tr("rah_col_mech"), I18n::tr("rah_col_state"),
          I18n::tr("rah_col_rtchange") }, 4);
    static const struct { const char *mech, *s0, *s1, *drt; bool on; int sel; }
    kVa[4] = {
        { "rah_va_shell",      "rah_va_deployed", "rah_va_stored",
          "+0.35 s",      true,  0 },
        { "rah_va_banner",     "rah_va_unrolled", "rah_va_rolled",
          "-0.3 s",       false, 1 },
        { "rah_va_pitlid",     "rah_va_concert",  "rah_va_opera",
          "±0.1 s",       false, 0 },
        { "rah_va_electronic", "rah_off",         "rah_on",
          "+0.2〜1.5 s",  false, 0 },
    };
    for (int r = 0; r < 4; ++r) {
        va->setItem(r, 0, checkItem(kVa[r].on));
        va->setItem(r, 1, new QTableWidgetItem(I18n::tr(kVa[r].mech)));
        auto *seg = new QComboBox(va);
        seg->addItems({ I18n::tr(kVa[r].s0), I18n::tr(kVa[r].s1) });
        seg->setCurrentIndex(kVa[r].sel);
        va->setCellWidget(r, 2, seg);
        va->setItem(r, 3, numItem(QString::fromUtf8(kVa[r].drt)));
    }
    sv->vbox()->addWidget(va);
    m_stageRtBadge = makeBadge(QString(), kAcc, sv);
    sv->vbox()->addWidget(m_stageRtBadge);
    auto *batch = new QHBoxLayout();
    auto *batchBtn = new QPushButton(I18n::tr("rah_va_batch"), sv);
    batchBtn->setStyleSheet("font-weight:600;");
    tabhelp::markNotImplemented(batchBtn);   // 一括解析は未配線
    batch->addWidget(batchBtn);
    batch->addWidget(makeHint(I18n::tr("rah_va_batch_hint"), sv), 1);
    sv->vbox()->addLayout(batch);
    v->addWidget(sv);

    // ── カップルドボリューム
    auto *sc = new SectionBox(I18n::tr("rah_cv_section"), page);
    sc->vbox()->addWidget(makeHint(I18n::tr("rah_cv_hint"), sc));
    sc->form()->addRow(I18n::tr("rah_cv_aperture"),
        unitRow(plainSpin(sc, 0, 80, 45), I18n::tr("rah_cv_aperture_unit"), sc));
    sc->form()->addRow(I18n::tr("rah_cv_volume"),
        unitRow(plainSpin(sc, 0, 100000, 3200), QString::fromUtf8("m³"), sc));
    sc->vbox()->addWidget(makeCheck(I18n::tr("rah_cv_detect"), true, sc));
    v->addWidget(sc);
    v->addStretch(1);
    return page;
}

// 吸音材・散乱体DB — α / s のオクターブバンド表と面への割当 (EASE/ODEON 相当)。
QWidget *RoomAcousticsTab::buildMaterialsPage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setSpacing(8);

    auto *s = new SectionBox(I18n::tr("rah_mat_section"), page);
    s->vbox()->addWidget(makeHint(I18n::tr("rah_mat_hint"), s));
    auto *tools = new QHBoxLayout();
    auto *search = new QLineEdit(s);
    search->setPlaceholderText(I18n::tr("rah_mat_search"));
    tools->addWidget(search, 1);
    auto *matImport = new QPushButton(I18n::tr("rah_mat_import"), s);
    tabhelp::markNotImplemented(matImport);   // 材質 DB 取込は未配線
    tools->addWidget(matImport);
    s->vbox()->addLayout(tools);
    v->addWidget(s);

    // ── 吸音率 α
    auto *sa = new SectionBox(I18n::tr("rah_alpha_section"), page);
    static const struct { const char *key; double a[6]; double nrc; } kMat[9] = {
        { "rah_m_concrete",    { 0.01, 0.01, 0.02, 0.02, 0.02, 0.03 }, 0.02 },
        { "rah_m_gypsum",      { 0.29, 0.10, 0.05, 0.04, 0.07, 0.09 }, 0.05 },
        { "rah_m_wood_floor",  { 0.15, 0.11, 0.10, 0.07, 0.06, 0.07 }, 0.10 },
        { "rah_m_carpet",      { 0.08, 0.24, 0.57, 0.69, 0.71, 0.73 }, 0.55 },
        { "rah_m_gw50",        { 0.22, 0.60, 0.90, 0.95, 0.90, 0.85 }, 0.85 },
        { "rah_m_perf_gw",     { 0.40, 0.75, 0.85, 0.60, 0.45, 0.30 }, 0.65 },
        { "rah_m_curtain",     { 0.07, 0.31, 0.49, 0.75, 0.70, 0.60 }, 0.55 },
        { "rah_m_seats_full",  { 0.39, 0.57, 0.80, 0.94, 0.92, 0.87 }, 0.80 },
        { "rah_m_seats_empty", { 0.19, 0.37, 0.56, 0.67, 0.61, 0.59 }, 0.55 },
    };
    auto *alpha = makeStaticTable(sa,
        { I18n::tr("rah_col_material"), "125", "250", "500", "1k", "2k", "4k",
          "NRC" }, 10);
    for (int r = 0; r < 9; ++r) {
        alpha->setItem(r, 0, new QTableWidgetItem(I18n::tr(kMat[r].key)));
        for (int b = 0; b < 6; ++b)
            alpha->setItem(r, b + 1,
                           numItem(QString::number(kMat[r].a[b], 'f', 2)));
        auto *nrc = numItem(QString::number(kMat[r].nrc, 'f', 2));
        QFont bold = nrc->font();
        bold.setBold(true);
        nrc->setFont(bold);
        alpha->setItem(r, 7, nrc);
    }
    auto *addMat = new QTableWidgetItem(I18n::tr("rah_add_material"));
    QFont ital = addMat->font();
    ital.setItalic(true);
    addMat->setFont(ital);
    alpha->setItem(9, 0, addMat);
    alpha->setSpan(9, 0, 1, 8);
    sa->vbox()->addWidget(alpha);
    v->addWidget(sa);

    // ── 散乱係数 s
    auto *ss = new SectionBox(I18n::tr("rah_scatter_section"), page);
    ss->vbox()->addWidget(makeHint(I18n::tr("rah_scatter_hint"), ss));
    static const struct { const char *key; double s[4]; const char *kind; }
    kScat[5] = {
        { "rah_s_smooth", { 0.05, 0.05, 0.05, 0.10 }, "rah_k_specular" },
        { "rah_s_slight", { 0.05, 0.10, 0.15, 0.25 }, "rah_k_semi" },
        { "rah_s_qrd",    { 0.10, 0.45, 0.70, 0.85 }, "rah_k_diffuse" },
        { "rah_s_poly",   { 0.15, 0.50, 0.75, 0.90 }, "rah_k_diffuse" },
        { "rah_s_seats",  { 0.30, 0.60, 0.70, 0.80 }, "rah_k_high" },
    };
    auto *scat = makeStaticTable(ss,
        { I18n::tr("rah_col_surface"), "125", "500", "1k", "4k",
          I18n::tr("rah_col_kind") }, 5);
    for (int r = 0; r < 5; ++r) {
        scat->setItem(r, 0, new QTableWidgetItem(I18n::tr(kScat[r].key)));
        for (int b = 0; b < 4; ++b)
            scat->setItem(r, b + 1,
                          numItem(QString::number(kScat[r].s[b], 'f', 2)));
        scat->setItem(r, 5, new QTableWidgetItem(I18n::tr(kScat[r].kind)));
    }
    ss->vbox()->addWidget(scat);
    ss->vbox()->addWidget(makeCheck(I18n::tr("rah_scatter_assign"), true, ss));
    ss->vbox()->addWidget(makeCheck(I18n::tr("rah_scatter_freq"), true, ss));
    v->addWidget(ss);

    // ── 面への割当
    auto *sf = new SectionBox(I18n::tr("rah_assign_section"), page);
    static const struct { const char *face, *absorber, *scat, *area; }
    kAssign[4] = {
        { "rah_f_ceiling",  "rah_a_panel_t15",  "rah_s_poly",         "300" },
        { "rah_f_sidewall", "rah_a_wood_panel", "rah_s_qrd",          "180" },
        { "rah_f_rearwall", "rah_m_gw50",       "rah_s_smooth_short", "60"  },
        { "rah_f_floor",    "rah_m_wood_floor", "rah_s_smooth_short", "300" },
    };
    auto *assign = makeStaticTable(sf,
        { I18n::tr("rah_col_face"), I18n::tr("rah_col_absorber"),
          I18n::tr("rah_col_scatterer"), I18n::tr("ra_area") }, 4);
    for (int r = 0; r < 4; ++r) {
        assign->setItem(r, 0, new QTableWidgetItem(I18n::tr(kAssign[r].face)));
        assign->setItem(r, 1,
                        new QTableWidgetItem(I18n::tr(kAssign[r].absorber)));
        assign->setItem(r, 2, new QTableWidgetItem(I18n::tr(kAssign[r].scat)));
        assign->setItem(r, 3, numItem(QString::fromUtf8(kAssign[r].area)));
    }
    sf->vbox()->addWidget(assign);
    v->addWidget(sf);
    v->addStretch(1);

    // 材質名でのインクリメンタル絞り込み (末尾の「材質を追加」行は常に表示)
    connect(search, &QLineEdit::textChanged, this, [alpha](const QString &q) {
        for (int r = 0; r < 9; ++r) {
            const QTableWidgetItem *it = alpha->item(r, 0);
            alpha->setRowHidden(r, !q.isEmpty() && it
                                && !it->text().contains(q, Qt::CaseInsensitive));
        }
    });
    return page;
}

// 電気音響設計 — スピーカー配置/エイミング, ディレイ, STIマップ, GBF (EASE相当)。
QWidget *RoomAcousticsTab::buildReinforcePage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setSpacing(8);

    auto *s = new SectionBox(I18n::tr("rah_sr_section"), page);
    s->vbox()->addWidget(makeHint(I18n::tr("rah_sr_hint"), s));
    v->addWidget(s);

    // ── スピーカーシステム
    auto *sl = new SectionBox(I18n::tr("rah_ls_section"), page);
    static const struct { const char *sp, *model, *pos, *aim, *gain; bool on; }
    kSp[4] = {
        { "L", "rah_sp_line8", "-4, 8, 6",  "-20° / -8°",  "0 dB",  true  },
        { "R", "rah_sp_line8", "4, 8, 6",   "+20° / -8°",  "0 dB",  true  },
        { "C", "rah_sp_point", "0, 8.5, 6", "0° / -12°",   "-3 dB", true  },
        { "F", "rah_sp_front", "0, 1.2, 3", "0° / -30°",   "-9 dB", false },
    };
    auto *ls = makeStaticTable(sl,
        { "", "SP", I18n::tr("rah_col_model"), I18n::tr("rah_col_pos"),
          I18n::tr("rah_col_aim"), I18n::tr("rah_col_gain") }, 5);
    for (int r = 0; r < 4; ++r) {
        ls->setItem(r, 0, checkItem(kSp[r].on));
        ls->setItem(r, 1, new QTableWidgetItem(QString::fromUtf8(kSp[r].sp)));
        ls->setItem(r, 2, new QTableWidgetItem(I18n::tr(kSp[r].model)));
        ls->setItem(r, 3, new QTableWidgetItem(QString::fromUtf8(kSp[r].pos)));
        ls->setItem(r, 4, new QTableWidgetItem(QString::fromUtf8(kSp[r].aim)));
        ls->setItem(r, 5, numItem(QString::fromUtf8(kSp[r].gain)));
    }
    auto *addSp = new QTableWidgetItem(I18n::tr("rah_add_speaker"));
    QFont ital = addSp->font();
    ital.setItalic(true);
    addSp->setFont(ital);
    ls->setItem(4, 0, addSp);
    ls->setSpan(4, 0, 1, 6);
    sl->vbox()->addWidget(ls);
    auto *lsBtns = new QHBoxLayout();
    // 自動エイミング / GLL ライブラリは未配線 (絶対規則 5)
    auto *aimBtn = new QPushButton(I18n::tr("rah_auto_aim"), sl);
    auto *gllBtn = new QPushButton(I18n::tr("rah_gll_lib"), sl);
    tabhelp::markNotImplemented(aimBtn);
    tabhelp::markNotImplemented(gllBtn);
    lsBtns->addWidget(aimBtn);
    lsBtns->addWidget(gllBtn);
    lsBtns->addStretch(1);
    sl->vbox()->addLayout(lsBtns);
    v->addWidget(sl);

    // ── 遅延・ディレイタワー
    auto *sd = new SectionBox(I18n::tr("rah_delay_section"), page);
    sd->form()->addRow(I18n::tr("rah_delay_row"),
                       makeCheck(I18n::tr("rah_haas"), true, sd));
    sd->vbox()->addWidget(makeHint(I18n::tr("rah_delay_hint"), sd));
    // ディレイ値は固定のサンプル (距離補正の計算は未実装 — 絶対規則 5)
    sd->vbox()->addWidget(tabhelp::sampleNote(sd));
    v->addWidget(sd);

    // ── STI マッピング
    auto *sm = new SectionBox(I18n::tr("rah_sti_section"), page);
    sm->vbox()->addWidget(new StiMapWidget(sm));
    auto *badges = new QHBoxLayout();
    badges->addWidget(makeBadge(I18n::tr("rah_sti_avg"), kOk, sm));
    badges->addWidget(makeBadge(I18n::tr("rah_sti_uniform"), kOk, sm));
    badges->addWidget(makeBadge(I18n::tr("rah_spl_badge"), kAcc, sm));
    badges->addStretch(1);
    sm->vbox()->addLayout(badges);
    // STI マップ・STI 平均・SPL バッジは固定のサンプル表示。
    // 校正なしの絶対 SPL を実行結果として見せない (絶対規則 5・6)
    sm->vbox()->addWidget(tabhelp::sampleNote(sm));
    v->addWidget(sm);

    // ── ハウリング余裕 (GBF)
    auto *sg = new SectionBox(I18n::tr("rah_gbf_section"), page);
    sg->form()->addRow(I18n::tr("rah_mic_pos"),
                       new QLineEdit(I18n::tr("rah_mic_default"), sg));
    auto *gbf = new QHBoxLayout();
    gbf->addWidget(makeBadge(I18n::tr("rah_gbf_badge"), kOk, sg));
    gbf->addWidget(makeHint(I18n::tr("rah_gbf_hint"), sg), 1);
    sg->vbox()->addLayout(gbf);
    // GBF 値は固定のサンプル (ハウリング解析は未実装 — 絶対規則 5)
    sg->vbox()->addWidget(tabhelp::sampleNote(sg));
    sg->vbox()->addWidget(makeCheck(I18n::tr("rah_notch"), false, sg));
    v->addWidget(sg);
    v->addStretch(1);
    return page;
}

QWidget *RoomAcousticsTab::buildNoisePage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setSpacing(8);

    auto *s = new SectionBox(I18n::tr("ra_noise_section"), page);
    auto *hint = new QLabel(I18n::tr("ra_noise_hint"), s);
    hint->setWordWrap(true);
    s->vbox()->addWidget(hint);

    m_noise = new QTableWidget(1, 7, s);
    m_noise->setHorizontalHeaderLabels(
        { "63", "125", "250", "500", "1k", "2k", "4k" });
    m_noise->setVerticalHeaderLabels({ I18n::tr("ra_noise_row") });
    m_noise->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_noise->setMaximumHeight(70);
    s->vbox()->addWidget(m_noise);
    v->addWidget(s);

    auto *sp = new SectionBox(I18n::tr("ra_nc_section"), page);
    m_ncPlot = new MiniPlot(sp);
    m_ncPlot->setLabels(I18n::tr("ra_octave_hz"), "SPL [dB]");
    m_ncPlot->setXTickPow10(true);
    m_ncPlot->setMinimumHeight(160);
    sp->vbox()->addWidget(m_ncPlot);
    m_ncBadge = new QLabel(sp);
    m_ncBadge->setStyleSheet("font-weight:600; font-size:13px;");
    sp->vbox()->addWidget(m_ncBadge);
    auto *guide = new QLabel(I18n::tr("ra_nc_guide"), sp);
    guide->setWordWrap(true);
    sp->vbox()->addWidget(guide);
    v->addWidget(sp);

    // ── 騒音源内訳 (mock room-acoustics.jsx:697-709) — 編集可テーブル ──
    auto *sn = new SectionBox(I18n::tr("rah_ns_section"), page);
    sn->vbox()->addWidget(makeHint(I18n::tr("rah_ns_hint"), sn));
    m_noiseSrc = new QTableWidget(0, 4, sn);
    m_noiseSrc->setHorizontalHeaderLabels({
        "", I18n::tr("rah_ns_col_source"), I18n::tr("rah_ns_col_level"),
        I18n::tr("rah_ns_col_measure") });
    m_noiseSrc->horizontalHeader()->setSectionResizeMode(
        QHeaderView::ResizeToContents);
    m_noiseSrc->horizontalHeader()->setStretchLastSection(true);
    m_noiseSrc->verticalHeader()->setVisible(false);
    m_noiseSrc->setMinimumHeight(150);
    sn->vbox()->addWidget(m_noiseSrc);
    auto *nsBtns = new QHBoxLayout();
    auto *nsAdd = new QPushButton(I18n::tr("rah_ns_add"), sn);
    auto *nsDel = new QPushButton(I18n::tr("rah_ns_del"), sn);
    nsBtns->addWidget(nsAdd);
    nsBtns->addWidget(nsDel);
    nsBtns->addStretch(1);
    sn->vbox()->addLayout(nsBtns);
    v->addWidget(sn);
    v->addStretch(1);

    connect(m_noise, &QTableWidget::cellChanged, this, [this] {
        if (m_updating) return;
        AcousticOpts &a = m_p->acoustic();
        for (int b = 0; b < 7; ++b)
            if (auto *it = m_noise->item(0, b))
                a.noiseLevels[b] = it->text().toDouble();
        recomputeAll();
        m_p->touch();
    });
    connect(m_noiseSrc, &QTableWidget::cellChanged, this, [this] {
        if (m_updating) return;
        applyNoiseSources();
        m_p->touch();
    });
    connect(nsAdd, &QPushButton::clicked, this, [this] {
        NoiseSourceRow r;
        r.name = I18n::tr("rah_ns_new");
        r.measure = QString::fromUtf8("—");
        m_p->acoustic().noiseSources.push_back(r);
        refreshNoiseSources();
        m_p->touch();
    });
    connect(nsDel, &QPushButton::clicked, this, [this] {
        const int r = m_noiseSrc->currentRow();
        auto &rows = m_p->acoustic().noiseSources;
        if (r >= 0 && r < rows.size()) {
            rows.removeAt(r);
            refreshNoiseSources();
            m_p->touch();
        }
    });
    return page;
}

QWidget *RoomAcousticsTab::buildDefectsPage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setSpacing(8);

    auto *s = new SectionBox(I18n::tr("ra_defect_section"), page);
    auto *hint = new QLabel(I18n::tr("ra_defect_hint"), s);
    hint->setWordWrap(true);
    s->vbox()->addWidget(hint);
    m_defects = new QTableWidget(0, 4, s);
    m_defects->setHorizontalHeaderLabels({
        I18n::tr("ra_defect"), I18n::tr("ra_place"),
        I18n::tr("ra_cause"), I18n::tr("ra_severity") });
    m_defects->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_defects->verticalHeader()->setVisible(false);
    m_defects->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_defects->setMinimumHeight(140);
    s->vbox()->addWidget(m_defects);
    v->addWidget(s);

    auto *sr = new SectionBox(I18n::tr("ra_recommend_section"), page);
    m_recommend = new QLabel(sr);
    m_recommend->setWordWrap(true);
    m_recommend->setTextFormat(Qt::RichText);
    sr->vbox()->addWidget(m_recommend);

    // ── 改善後の再シミュレーション (mock room-acoustics.jsx:735) ──
    // 検出障害の対象面 α をフラッター ≥0.30 / エコー ≥0.40 に引き上げた
    // コピーで試算する。モデル (AcousticOpts) は書き換えない。
    auto *resimRow = new QHBoxLayout();
    auto *resimBtn = new QPushButton(I18n::tr("rah_resim"), sr);
    resimBtn->setStyleSheet("font-weight:600;");
    resimRow->addWidget(resimBtn);
    resimRow->addWidget(makeHint(I18n::tr("rah_resim_hint"), sr), 1);
    sr->vbox()->addLayout(resimRow);
    m_resimResult = new QLabel(sr);
    m_resimResult->setWordWrap(true);
    m_resimResult->setStyleSheet("font-weight:600;");
    m_resimResult->setVisible(false);
    sr->vbox()->addWidget(m_resimResult);
    v->addWidget(sr);
    v->addStretch(1);

    connect(resimBtn, &QPushButton::clicked, this,
            &RoomAcousticsTab::resimulateImproved);
    return page;
}

// 騒音源内訳: widgets → model (先頭列チェック / 名前 / dB(A) / 対策)
void RoomAcousticsTab::applyNoiseSources()
{
    auto &rows = m_p->acoustic().noiseSources;
    for (int r = 0; r < m_noiseSrc->rowCount() && r < rows.size(); ++r) {
        NoiseSourceRow &row = rows[r];
        if (auto *en = m_noiseSrc->item(r, 0))
            row.enabled = en->checkState() == Qt::Checked;
        if (auto *nm = m_noiseSrc->item(r, 1))
            row.name = nm->text();
        if (auto *lv = m_noiseSrc->item(r, 2))
            row.level_dBA = lv->text().toDouble();
        if (auto *ms = m_noiseSrc->item(r, 3))
            row.measure = ms->text();
    }
}

// 騒音源内訳: model → widgets (行追加/削除・refresh 共用)
void RoomAcousticsTab::refreshNoiseSources()
{
    m_updating = true;
    const auto &rows = m_p->acoustic().noiseSources;
    m_noiseSrc->setRowCount(rows.size());
    for (int r = 0; r < rows.size(); ++r) {
        const NoiseSourceRow &row = rows[r];
        m_noiseSrc->setItem(r, 0, checkItem(row.enabled));
        m_noiseSrc->setItem(r, 1, new QTableWidgetItem(row.name));
        m_noiseSrc->setItem(r, 2, numItem(
            QString::number(row.level_dBA, 'f', 0)));
        m_noiseSrc->setItem(r, 3, new QTableWidgetItem(row.measure));
    }
    m_updating = false;
}

// 改善後の再シミュレーション — 検出障害の対象面 α を引き上げた試算。
// フラッターエコー: 対向面ペアの両面を α≥0.30 へ、
// ロングディレイエコー: 反射面を α≥0.40 へ (全帯域)。モデルは不変。
void RoomAcousticsTab::resimulateImproved()
{
    const AcousticOpts &a0 = m_p->acoustic();
    double src[3], rcv[3];
    sourcePos(src);
    receiverPos(m_rcvBox->currentIndex(), rcv);
    const QVector<Defect> before = detectDefects(a0, src, rcv);

    AcousticOpts trial = a0;   // コピーで試算 (モデルは書き換えない)
    auto raiseRole = [&trial](int role, double target) {
        for (AbsorptionRow &r : trial.absorption)
            if (r.enabled && r.role == role)
                for (double &al : r.alpha)
                    al = std::max(al, target);
    };
    const QString kFloor = QString::fromUtf8("床");
    const QString kCeil  = QString::fromUtf8("天井");
    const QString kSide  = QString::fromUtf8("側壁");
    const QString kRear  = QString::fromUtf8("後壁");
    for (const Defect &d : before) {
        if (d.name.contains(QString::fromUtf8("フラッター"))) {
            // place: 側壁L-R間 / 床-天井間 / 舞台-後壁間 (detectDefects)
            if (d.place.contains(kSide)) {
                raiseRole(AbsorptionRow::SideWall, 0.30);
            } else if (d.place.contains(kFloor)) {
                raiseRole(AbsorptionRow::Floor, 0.30);
                raiseRole(AbsorptionRow::Ceiling, 0.30);
            } else {
                raiseRole(AbsorptionRow::SideWall, 0.30);
                raiseRole(AbsorptionRow::RearWall, 0.30);
            }
        } else {
            // place: 反射面名 (床/天井/側壁L/側壁R/舞台側/後壁)
            if (d.place.contains(kFloor))     raiseRole(AbsorptionRow::Floor, 0.40);
            else if (d.place.contains(kCeil)) raiseRole(AbsorptionRow::Ceiling, 0.40);
            else if (d.place.contains(kRear)) raiseRole(AbsorptionRow::RearWall, 0.40);
            else                              raiseRole(AbsorptionRow::SideWall, 0.40);
        }
    }
    const QVector<Defect> after = detectDefects(trial, src, rcv);

    auto rtMid = [](const AcousticOpts &x) {
        return (rt60(x, 2) + rt60(x, 3)) / 2.0;
    };
    m_resimResult->setText(I18n::tr("rah_resim_result")
        .arg(QString::number(rtMid(a0), 'f', 2),
             QString::number(rtMid(trial), 'f', 2),
             QString::number(totalAbsorption(a0, 3), 'f', 0),
             QString::number(totalAbsorption(trial, 3), 'f', 0),
             QString::number(before.size()),
             QString::number(after.size())));
    m_resimResult->setVisible(true);
}

// ── model → widgets ─────────────────────────────────────────────────────────
void RoomAcousticsTab::refresh()
{
    m_updating = true;
    const AcousticOpts &a = m_p->acoustic();

    m_roomL->setValue(a.roomL);
    m_roomW->setValue(a.roomW);
    m_roomH->setValue(a.roomH);
    m_volume->setValue(a.volume);
    m_surface->setValue(a.surface);
    m_occupancy->setCurrentIndex(qBound(0, a.occupancy, 2));
    m_formula->setCurrentIndex(qBound(0, a.rtFormula, 2));

    // 吸音バジェット表
    m_budget->setRowCount(a.absorption.size());
    for (int r = 0; r < a.absorption.size(); ++r) {
        const AbsorptionRow &row = a.absorption[r];
        auto *en = new QTableWidgetItem;
        en->setCheckState(row.enabled ? Qt::Checked : Qt::Unchecked);
        en->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        m_budget->setItem(r, 0, en);
        auto *nm = new QTableWidgetItem(row.name);
        m_budget->setItem(r, 1, nm);
        const bool air = row.role == AbsorptionRow::Air;
        auto num = [air](double v, bool editable) {
            auto *it = new QTableWidgetItem(
                air && !editable ? QStringLiteral("—")
                                 : QString::number(v, 'g', 4));
            if (air && !editable)
                it->setFlags(it->flags() & ~Qt::ItemIsEditable);
            return it;
        };
        m_budget->setItem(r, 2, num(row.area, false));
        m_budget->setItem(r, 3, num(row.alpha[0], false));
        m_budget->setItem(r, 4, num(row.alpha[2], false));
        m_budget->setItem(r, 5, num(row.alpha[3], false));
        m_budget->setItem(r, 6, num(row.alpha[5], false));
        // A@1k (Air は airA を直接編集)
        const double A1k = air
            ? row.airA
            : row.alpha[3] * row.area
              * (row.role == AbsorptionRow::Audience
                     ? occupancyFactor(a.occupancy) : 1.0);
        auto *aItem = new QTableWidgetItem(QString::number(A1k, 'f', 0));
        if (!air) aItem->setFlags(aItem->flags() & ~Qt::ItemIsEditable);
        m_budget->setItem(r, 7, aItem);
    }

    // 騒音レベル
    for (int b = 0; b < 7; ++b)
        m_noise->setItem(0, b, new QTableWidgetItem(
            QString::number(a.noiseLevels[b], 'f', 0)));

    m_updating = false;
    refreshNoiseSources();
    refreshHallDerived();
    recomputeAll();
}

// ── ホールプリセット ────────────────────────────────────────────────────────
// 選択されたホールの室容積 V を AcousticOpts へ (唯一の永続化フック)。
// 残響計算・カバレッジは V に依存するので refresh() 経由で全ページを更新する。
void RoomAcousticsTab::applyHallPreset()
{
    if (m_updating) return;
    const HallView H = currentHallView(m_catOpera->isChecked(),
                                       m_hallBox->currentIndex(),
                                       m_operaBox->currentIndex());
    AcousticOpts &a = m_p->acoustic();
    a.volume = H.V;
    refresh();
    m_p->touch();
}

// プリセット由来の表示 (公表値テーブル / IR解析 / 空間印象 / ステージ) を更新。
void RoomAcousticsTab::refreshHallDerived()
{
    const HallView C = currentHallView(false, m_hallBox->currentIndex(), 0);
    const HallView O = currentHallView(true, 0, m_operaBox->currentIndex());
    const HallView &H = m_catOpera->isChecked() ? O : C;

    auto sec = [](double v) { return QString::number(v, 'f', 2) + " s"; };
    auto db  = [](double v) {
        return (v > 0 ? QStringLiteral("+") : QString())
               + QString::number(v, 'f', 1) + " dB";
    };
    auto ms  = [](int v) { return QString::number(v) + " ms"; };
    const QString arrow = QString::fromUtf8("▸ ");

    // ── 世界のコンサートホール pane
    m_hallType->setText(C.type);
    m_hallInfo->setText(C.info);
    m_hallNote->setText(arrow + C.note);
    setRowCells(m_hallMetrics, 0, { sec(C.RT), sec(C.EDT), db(C.C80),
                                    "+" + QString::number(C.G, 'f', 1) + " dB",
                                    ms(C.ITDG),
                                    QString::number(C.BQI, 'f', 2) });

    // ── 日本のオペラ対応ホール pane
    m_operaType->setText(O.type);
    m_operaInfo->setText(O.info);
    m_operaClosed->setText(I18n::tr("rah_closed") + ": " + O.closure);
    m_operaClosed->setVisible(O.closed);
    m_operaNote->setText(arrow + O.note);
    m_operaNote->setVisible(!O.note.isEmpty());
    setRowCells(m_operaMetrics, 0, { sec(O.RT), sec(O.EDT), db(O.C80),
                                     "+" + QString::number(O.G, 'f', 1) + " dB",
                                     ms(O.ITDG) });
    m_operaMetrics->setItem(0, 5, badgeItem(
        I18n::tr(O.measured ? "rah_measured" : "rah_estimated"),
        O.measured ? kOk : kWarn));
    setRowCells(m_operaPit, 0, { O.pit, O.ensemble, O.stage });

    // ── 客席カバレッジ: 実測 (公表値) の検証目標
    m_covRefNote->setText(I18n::tr("rah_cov_ref")
        .arg(QString::number(H.RT, 'f', 2), QString::number(H.C80, 'f', 1),
             QString::number(H.G, 'f', 1), QString::number(H.ITDG)));

    // ── IR解析: Schroeder 減衰曲線 (初期減衰を急にした二段近似)
    MiniSeries decay;
    decay.color = QColor(kAcc);
    const double RT = H.RT > 0.05 ? H.RT : 1.0;
    for (int i = 0; i < 100; ++i) {
        const double tt = i * 0.025;
        const double lin = -60.0 * tt / RT;
        decay.pts.push_back(
            { tt, std::max(lin + (tt < 0.1 ? -tt * 18.0 : 0.0), -75.0) });
    }
    m_schroederPlot->setSeries({ decay });

    // 帯域別指標 (mock の係数をそのまま)
    static const double kEdtK[6] = { 1.10, 1.05, 1.02, 1.00, 0.92, 0.78 };
    static const double kT20K[6] = { 1.12, 1.06, 1.02, 1.00, 0.93, 0.80 };
    static const double kT30K[6] = { 1.13, 1.07, 1.03, 1.01, 0.94, 0.81 };
    static const double kC80D[6] = { -1.2, -0.6, -0.2, 0.0, 0.5, 1.3 };
    static const char *kD50[6] = { "0.38", "0.42", "0.45", "0.48", "0.52",
                                   "0.57" };
    static const char *kTs[6]  = { "142", "131", "124", "118", "108", "92" };
    static const char *kIrRow[6] = { "EDT [s]", "T20 [s]", "T30 [s]",
                                     "C80 [dB]", "D50 [-]", "Ts [ms]" };
    for (int r = 0; r < 6; ++r)
        m_irBandTable->setItem(r, 0, new QTableWidgetItem(
            QString::fromUtf8(kIrRow[r])));
    for (int b = 0; b < 6; ++b) {
        m_irBandTable->setItem(0, b + 1,
            numItem(QString::number(H.EDT * kEdtK[b], 'f', 2)));
        m_irBandTable->setItem(1, b + 1,
            numItem(QString::number(H.RT * kT20K[b], 'f', 2)));
        m_irBandTable->setItem(2, b + 1,
            numItem(QString::number(H.RT * kT30K[b], 'f', 2)));
        m_irBandTable->setItem(3, b + 1,
            numItem(QString::number(H.C80 + kC80D[b], 'f', 1)));
        m_irBandTable->setItem(4, b + 1, numItem(QString::fromUtf8(kD50[b])));
        m_irBandTable->setItem(5, b + 1, numItem(QString::fromUtf8(kTs[b])));
    }

    // 実測 vs シミュレーション (@1kHz)
    setRowCells(m_irValTable, 0, { "T30",
        QString::number(H.RT, 'f', 2) + "s",
        QString::number(H.RT * 1.03, 'f', 2) + "s",
        "+" + QString::number(H.RT * 0.03, 'f', 2) + "s", "5%" });
    m_irValTable->setItem(0, 5, badgeItem(I18n::tr("rah_jnd_ok"), kOk));
    setRowCells(m_irValTable, 1, { "EDT",
        QString::number(H.EDT, 'f', 2) + "s",
        QString::number(H.EDT * 0.94, 'f', 2) + "s",
        "-" + QString::number(H.EDT * 0.06, 'f', 2) + "s", "5%" });
    m_irValTable->setItem(1, 5, badgeItem(I18n::tr("ra_check"), kWarn));
    setRowCells(m_irValTable, 2, { "C80",
        QString::number(H.C80, 'f', 1) + "dB",
        QString::number(H.C80 + 0.4, 'f', 1) + "dB", "+0.4dB", "1dB" });
    m_irValTable->setItem(2, 5, badgeItem(I18n::tr("rah_jnd_ok"), kOk));

    // ── 空間印象 (LF/LFC は BQI からの回帰推定, mock の式)
    const bool bqiOk = H.BQI >= 0.55;
    setRowCells(m_spatialTable, 0, { I18n::tr("rah_lf"),
        QString::number(0.15 + H.BQI * 0.2, 'f', 2),
        QString::fromUtf8("0.10–0.35"), I18n::tr("rah_mean_asw") });
    setRowCells(m_spatialTable, 1, { I18n::tr("rah_lfc"),
        QString::number(0.18 + H.BQI * 0.2, 'f', 2),
        QString::fromUtf8("—"), I18n::tr("rah_mean_lfc") });
    setRowCells(m_spatialTable, 2, { I18n::tr("rah_bqi_row"),
        QString::number(H.BQI, 'f', 2) + (H.bqiEstimated ? " *" : ""),
        I18n::tr("rah_bqi_range"), I18n::tr("rah_mean_bqi") });
    setRowCells(m_spatialTable, 3, { I18n::tr("rah_iacc_l"), "0.13",
        QString::fromUtf8("≤0.28"), I18n::tr("rah_mean_lev") });
    setRowCells(m_spatialTable, 4, { I18n::tr("rah_glate"),
        QString::number(H.G - 2.4, 'f', 1) + " dB",
        QString::fromUtf8("≥ 0 dB"), I18n::tr("rah_mean_late") });
    for (int r = 0; r < 5; ++r) {
        if (auto *it = m_spatialTable->item(r, 1))
            it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_spatialTable->setItem(r, 4, r == 2
            ? badgeItem(I18n::tr(bqiOk ? "ra_excellent" : "ra_fair"),
                        bqiOk ? kOk : kWarn)
            : badgeItem(I18n::tr("rah_ok"), kOk));
    }

    // ── ステージ: 反射板設置時の RT (mock の +0.35 s)
    m_stageRtBadge->setText(I18n::tr("rah_va_now")
        .arg(QString::number(H.RT, 'f', 2),
             QString::number(H.RT + 0.35, 'f', 2)));
}

// A=αS@1k 派生列のみ再表示 (cellChanged の再入は m_updating で防ぐ)
void RoomAcousticsTab::refreshBudgetDerived()
{
    const AcousticOpts &a = m_p->acoustic();
    m_updating = true;
    for (int r = 0; r < m_budget->rowCount() && r < a.absorption.size(); ++r) {
        const AbsorptionRow &row = a.absorption[r];
        if (row.role == AbsorptionRow::Air) continue;   // airA は編集セル
        const double A1k = row.alpha[3] * row.area
            * (row.role == AbsorptionRow::Audience
                   ? occupancyFactor(a.occupancy) : 1.0);
        if (auto *it = m_budget->item(r, 7))
            it->setText(QString::number(A1k, 'f', 0));
    }
    m_updating = false;
}

void RoomAcousticsTab::applyBudgetTable()
{
    AcousticOpts &a = m_p->acoustic();
    for (int r = 0; r < m_budget->rowCount() && r < a.absorption.size(); ++r) {
        AbsorptionRow &row = a.absorption[r];
        if (auto *en = m_budget->item(r, 0))
            row.enabled = en->checkState() == Qt::Checked;
        if (auto *nm = m_budget->item(r, 1))
            row.name = nm->text();
        auto cell = [this, r](int c) {
            auto *it = m_budget->item(r, c);
            return it ? it->text() : QString();
        };
        if (row.role == AbsorptionRow::Air) {
            row.airA = cell(7).toDouble();
        } else {
            row.area = cell(2).toDouble();
            row.alpha[0] = cell(3).toDouble();
            row.alpha[2] = cell(4).toDouble();
            row.alpha[3] = cell(5).toDouble();
            row.alpha[5] = cell(6).toDouble();
            // 未表示帯域 (250Hz, 2kHz) は隣接帯域の平均で補間
            row.alpha[1] = (row.alpha[0] + row.alpha[2]) / 2.0;
            row.alpha[4] = (row.alpha[3] + row.alpha[5]) / 2.0;
        }
    }
}

// ── 派生値の再計算 ──────────────────────────────────────────────────────────
void RoomAcousticsTab::recomputeAll()
{
    const AcousticOpts &a = m_p->acoustic();

    // カバレッジ
    m_map->recompute();
    static const char *metricName[4] = { "G", "C80", "STI", "RT60" };
    const double sd = m_map->stddev();
    const bool uniform = (m_metricBox->currentIndex() == 2)
        ? sd < 0.08 : sd < 3.0;
    m_covStats->setText(QStringLiteral(
        "<b>%1</b><br>%2: %3<br>σ: ±%4<br>%5: %6<br><br>○ = %7 (4)")
        .arg(QString::fromUtf8(metricName[m_metricBox->currentIndex()]),
             I18n::tr("ra_mean"), QString::number(m_map->mean(), 'f', 2),
             QString::number(sd, 'f', 2),
             I18n::tr("ra_uniformity"),
             uniform ? I18n::tr("ra_good") : I18n::tr("ra_check"),
             I18n::tr("ra_receiver")));

    // 席別表
    double src[3];
    sourcePos(src);
    const int band = qMin(5, m_bandBox->currentIndex());
    const double T = rt60(a, band);
    for (int i = 0; i < 4; ++i) {
        double rcv[3];
        receiverPos(i, rcv);
        const double dx = rcv[0]-src[0], dy = rcv[1]-src[1], dz = rcv[2]-src[2];
        const double r = std::sqrt(dx*dx + dy*dy + dz*dz);
        const SeatMetrics m = seatMetrics(r, T, a.volume);
        m_seatTable->setItem(i, 0, new QTableWidgetItem(
            I18n::tr(kReceivers[i].key)));
        m_seatTable->setItem(i, 1, new QTableWidgetItem(QString::number(m.G, 'f', 1)));
        m_seatTable->setItem(i, 2, new QTableWidgetItem(QString::number(m.C80, 'f', 1)));
        m_seatTable->setItem(i, 3, new QTableWidgetItem(QString::number(m.D50, 'f', 2)));
        m_seatTable->setItem(i, 4, new QTableWidgetItem(QString::number(m.STI, 'f', 2)));
        m_seatTable->setItem(i, 5, new QTableWidgetItem(QString::number(m.RT, 'f', 2)));
        const QString verdict = m.STI >= 0.60 ? I18n::tr("ra_excellent")
                              : m.STI >= 0.45 ? I18n::tr("ra_good")
                                              : I18n::tr("ra_fair");
        m_seatTable->setItem(i, 6, new QTableWidgetItem(verdict));
    }

    // エコーグラム
    double rcv[3];
    receiverPos(m_rcvBox->currentIndex(), rcv);
    const QVector<Reflection> refl = echogram(a, src, rcv);
    MiniSeries direct, earlyS, lateS;
    direct.color = QColor("#DC2626");
    earlyS.color = QColor("#2563EB");
    lateS.color = QColor("#9CA3AF");
    direct.label = I18n::tr("ra_direct");
    earlyS.label = I18n::tr("ra_early");
    lateS.label = I18n::tr("ra_late");
    for (const Reflection &r : refl) {
        if (r.surface.isEmpty()) direct.pts.push_back({ r.timeMs, 0.0 });
        else if (r.early) earlyS.pts.push_back({ r.timeMs, r.levelDb });
        else lateS.pts.push_back({ r.timeMs, r.levelDb });
    }
    m_echoPlot->setYRange(-30, 2);
    m_echoPlot->setSeries({ lateS, earlyS, direct });
    m_itdgLabel->setText(QStringLiteral("ITDG = %1 ms  (%2)")
        .arg(QString::number(itdgMs(refl), 'f', 1),
             itdgMs(refl) < 25 ? I18n::tr("ra_itdg_good")
                               : I18n::tr("ra_itdg_far")));

    m_reflTable->setRowCount(refl.size());
    for (int i = 0; i < refl.size(); ++i) {
        const Reflection &r = refl[i];
        m_reflTable->setItem(i, 0, new QTableWidgetItem(
            r.surface.isEmpty() ? I18n::tr("ra_direct")
                                : QStringLiteral("R%1").arg(i)));
        m_reflTable->setItem(i, 1, new QTableWidgetItem(
            QString::number(r.timeMs, 'f', 1)));
        m_reflTable->setItem(i, 2, new QTableWidgetItem(
            QString::number(r.levelDb, 'f', 1)));
        m_reflTable->setItem(i, 3, new QTableWidgetItem(
            r.surface.isEmpty() ? QStringLiteral("—") : r.surface));
        QString verdict = QStringLiteral("—");
        if (!r.surface.isEmpty()) {
            if (r.timeMs > 50 && r.levelDb > -10)
                verdict = I18n::tr("ra_echo_risk");
            else if (r.timeMs <= 50)
                verdict = I18n::tr("ra_beneficial");
        }
        m_reflTable->setItem(i, 4, new QTableWidgetItem(verdict));
    }

    // RT60 帯域プロット
    MiniSeries rt;
    rt.color = QColor("#2E8B57");
    rt.markers = true;
    for (int b = 0; b < 6; ++b)
        rt.pts.push_back({ std::log10(kBandHz[b]), rt60(a, b) });
    m_rtPlot->setYRange(0, 2.6);
    m_rtPlot->setSeries({ rt });
    const double tMid = (rt60(a, 2) + rt60(a, 3)) / 2.0;
    m_rtBadge->setText(QStringLiteral("RT60(mid) = %1 s   (A@1k = %2 Sabin)")
        .arg(QString::number(tMid, 'f', 2))
        .arg(QString::number(totalAbsorption(a, 3), 'f', 0)));

    // NC
    const int nc = ncRating(a.noiseLevels);
    MiniSeries meas, ref;
    meas.color = QColor("#2E8B57");
    meas.markers = true;
    meas.label = I18n::tr("ra_measured");
    const int refNc = qBound(15, ((nc + 4) / 5) * 5, 70);
    ref.color = QColor("#DC2626");
    ref.dashed = true;
    ref.label = QStringLiteral("NC-%1").arg(refNc);
    const QVector<double> curve = ncCurve(refNc);
    static const double octHz[7] = { 63, 125, 250, 500, 1000, 2000, 4000 };
    for (int b = 0; b < 7; ++b) {
        meas.pts.push_back({ std::log10(octHz[b]), a.noiseLevels[b] });
        if (b < curve.size())
            ref.pts.push_back({ std::log10(octHz[b]), curve[b] });
    }
    m_ncPlot->setSeries({ ref, meas });
    m_ncBadge->setText(QStringLiteral("NC-%1   (%2)")
        .arg(nc)
        .arg(nc <= 25 ? I18n::tr("ra_nc_hall_ok") : I18n::tr("ra_nc_high")));

    // 障害検出
    const QVector<Defect> defects = detectDefects(a, src, rcv);
    m_defects->setRowCount(defects.size());
    static const char *sev[3] = { "低", "中", "高" };
    for (int i = 0; i < defects.size(); ++i) {
        m_defects->setItem(i, 0, new QTableWidgetItem(defects[i].name));
        m_defects->setItem(i, 1, new QTableWidgetItem(defects[i].place));
        m_defects->setItem(i, 2, new QTableWidgetItem(defects[i].cause));
        m_defects->setItem(i, 3, new QTableWidgetItem(
            QString::fromUtf8(sev[qBound(0, defects[i].severity, 2)])));
    }
    QString rec;
    bool flutter = false, echo = false;
    for (const Defect &d : defects) {
        if (d.name.contains(QString::fromUtf8("フラッター"))) flutter = true;
        else echo = true;
    }
    if (flutter) rec += "• " + I18n::tr("ra_rec_flutter") + "<br>";
    if (echo)    rec += "• " + I18n::tr("ra_rec_echo") + "<br>";
    if (rec.isEmpty()) rec = I18n::tr("ra_rec_none");
    rec += "<br><i>" + I18n::tr("ra_defect_note") + "</i>";
    m_recommend->setText(rec);
}

// ── レポート出力 ────────────────────────────────────────────────────────────
void RoomAcousticsTab::exportReport()
{
    const QString path = QFileDialog::getSaveFileName(
        this, I18n::tr("ra_export_report"), "room_acoustics_report.md",
        "Markdown (*.md);;Text (*.txt)");
    if (path.isEmpty()) return;

    const AcousticOpts &a = m_p->acoustic();
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);

    out << "# " << I18n::tr("ra_report_title") << "\n\n";
    out << "## " << I18n::tr("ra_reverb_section") << "\n";
    out << QStringLiteral("- V = %1 m³, S = %2 m², %3\n")
        .arg(a.volume).arg(a.surface)
        .arg(a.rtFormula ? "Eyring" : "Sabine");
    out << "\n| f [Hz] | A [Sabin] | RT60 [s] |\n|---|---|---|\n";
    for (int b = 0; b < 6; ++b)
        out << QStringLiteral("| %1 | %2 | %3 |\n")
            .arg(kBandHz[b])
            .arg(totalAbsorption(a, b), 0, 'f', 0)
            .arg(rt60(a, b), 0, 'f', 2);

    double src[3], rcv[3];
    sourcePos(src);
    out << "\n## " << I18n::tr("ra_seat_section") << "\n";
    out << "| " << I18n::tr("ra_receiver")
        << " | G | C80 | D50 | STI | RT60 |\n|---|---|---|---|---|---|\n";
    const double T = rt60(a, 3);
    for (int i = 0; i < 4; ++i) {
        receiverPos(i, rcv);
        const double dx = rcv[0]-src[0], dy = rcv[1]-src[1], dz = rcv[2]-src[2];
        const SeatMetrics m = seatMetrics(std::sqrt(dx*dx+dy*dy+dz*dz), T, a.volume);
        out << QStringLiteral("| %1 | %2 | %3 | %4 | %5 | %6 |\n")
            .arg(I18n::tr(kReceivers[i].key))
            .arg(m.G, 0, 'f', 1).arg(m.C80, 0, 'f', 1)
            .arg(m.D50, 0, 'f', 2).arg(m.STI, 0, 'f', 2)
            .arg(m.RT, 0, 'f', 2);
    }

    out << "\n## NC\n- " << QStringLiteral("NC-%1\n").arg(ncRating(a.noiseLevels));

    receiverPos(m_rcvBox->currentIndex(), rcv);
    const QVector<Defect> defects = detectDefects(a, src, rcv);
    out << "\n## " << I18n::tr("ra_defect_section") << "\n";
    if (defects.isEmpty()) out << "- " << I18n::tr("ra_rec_none") << "\n";
    for (const Defect &d : defects)
        out << QStringLiteral("- %1 (%2): %3\n")
            .arg(d.name, d.place, d.cause);
    out << "\n> " << I18n::tr("ra_model_hint") << "\n";
}
