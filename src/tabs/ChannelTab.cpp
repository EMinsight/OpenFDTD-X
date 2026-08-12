// ChannelTab.cpp
#include "ChannelTab.h"
#include "TabHelpers.h"
#include "../core/Project.h"
#include "../em/RadioPropagation.h"
#include "../widgets/FieldHeatmap.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"

#include <cmath>

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <QVBoxLayout>

using namespace ofd;

// ── タブ固有語彙 (chn_) — file-local 登録 ───────────────────────────────────
namespace {
const bool s_i18n = [] {
    using ofd::I18n;
    // タブ名 (MainWindow 配線用)
    I18n::reg("t_channel", "📡 電波伝搬", "📡 Propagation");

    // 概要
    I18n::reg("chn_title", "電波伝搬・チャネル解析 / Propagation & channel",
              "Propagation & channel");
    I18n::reg("chn_hint",
              "屋内・市街地の電波カバレッジとチャネル特性。FDTDは近傍・小規模、"
              "レイトレースは広域を担当。",
              "Indoor and urban radio coverage plus channel characteristics. FDTD "
              "handles the near field and small scales, ray tracing the wide area.");
    I18n::reg("chn_env_indoor", "屋内 (オフィス/工場)", "Indoor (office / factory)");
    I18n::reg("chn_env_urban", "市街地", "Urban");
    I18n::reg("chn_env_vehicle", "車内・車車間", "In-vehicle / V2V");
    I18n::reg("chn_env_tunnel", "トンネル/地下", "Tunnel / underground");
    I18n::reg("chn_band", "周波数帯", "Frequency band");
    I18n::reg("chn_band_sub1", "< 1GHz", "< 1 GHz");
    I18n::reg("chn_band_sub6", "Sub-6 (3.5G)", "Sub-6 (3.5 G)");
    I18n::reg("chn_band_mmw", "ミリ波 (28/39G)", "mmWave (28 / 39 G)");
    I18n::reg("chn_band_thz", "サブTHz (100G+, 6G)", "Sub-THz (100 G+, 6G)");
    I18n::reg("chn_method", "解析手法", "Method");
    I18n::reg("chn_method_rt", "レイトレース (広域)", "Ray tracing (wide area)");
    I18n::reg("chn_method_fdtd", "FDTD (詳細・回折)",
              "FDTD (detailed, diffraction)");
    I18n::reg("chn_method_hybrid", "ハイブリッド", "Hybrid");

    // 環境モデル
    I18n::reg("chn_envm_section", "環境モデル / Environment", "Environment");
    I18n::reg("chn_layout", "間取り/地形", "Floor plan / terrain");
    I18n::reg("chn_browse", "📁 参照…", "📁 Browse…");
    I18n::reg("chn_envm_hint",
              "▸ 対応: STL のみ (IFC/BIM・OpenStreetMap・DXF は未実装)",
              "▸ Supported: STL only (IFC/BIM, OpenStreetMap and DXF are not "
              "implemented yet)");
    I18n::reg("chn_stl_filter", "STL (*.stl);;すべてのファイル (*)",
              "STL (*.stl);;All files (*)");
    I18n::reg("chn_material", "材料", "Materials");
    I18n::reg("chn_mat_db", "コンクリート/石膏ボード/ガラスの透過損失DB",
              "Transmission-loss database for concrete / plasterboard / glass");
    I18n::reg("chn_mat_scatter", "家具・人体の散乱",
              "Scattering from furniture and people");

    // 送受信
    I18n::reg("chn_txrx_section", "送受信 / TX-RX", "TX-RX");
    I18n::reg("chn_ap", "基地局/AP", "Base stations / APs");
    I18n::reg("chn_ap_unit", "台 · ", "units · ");
    I18n::reg("chn_mimo", "MIMO 4×4", "MIMO 4×4");
    I18n::reg("chn_beamforming", "ビームフォーミング", "Beamforming");
    I18n::reg("chn_rx", "受信点", "Receive points");
    I18n::reg("chn_rx_grid", "格子 (カバレッジマップ)", "Grid (coverage map)");
    I18n::reg("chn_rx_route", "経路 (移動体)", "Route (mobile)");
    I18n::reg("chn_rx_points", "指定点", "Specified points");

    // ── リンク条件 (計算入力) ──
    I18n::reg("chn_link_section", "リンク条件 / Link budget inputs",
              "Link budget inputs");
    I18n::reg("chn_link_hint",
              "▸ 下のチャネル特性は、この条件を見通し内 (LOS) の伝搬モデルに"
              "入れて実計算します。周波数は選択した周波数帯の代表値を入れて"
              "ありますが、直接編集できます。",
              "▸ The channel metrics below are computed from these inputs with "
              "line-of-sight propagation models. The frequency is prefilled "
              "with a representative value for the selected band and can be "
              "edited directly.");
    I18n::reg("chn_freq", "中心周波数", "Centre frequency");
    I18n::reg("chn_dist", "送受信距離 d", "TX-RX distance d");
    I18n::reg("chn_htx", "送信アンテナ高", "TX antenna height");
    I18n::reg("chn_hrx", "受信アンテナ高", "RX antenna height");
    I18n::reg("chn_eirp", "送信 EIRP", "TX EIRP");
    I18n::reg("chn_grx", "受信アンテナ利得", "RX antenna gain");
    I18n::reg("chn_bw", "帯域幅", "Bandwidth");
    I18n::reg("chn_nf", "受信機雑音指数 NF", "RX noise figure NF");
    I18n::reg("chn_refl", "大地反射係数 |Γ|", "Ground reflection |Γ|");
    I18n::reg("chn_bad_input",
              "⚠ 入力に数値でない値、または範囲外の値があります — 計算できません",
              "⚠ Some inputs are not numbers or are out of range — cannot "
              "compute");

    // チャネル特性
    I18n::reg("chn_metrics_section", "チャネル特性 / Channel metrics",
              "Channel metrics");
    I18n::reg("chn_col_metric", "指標", "Metric");
    I18n::reg("chn_col_value", "値", "Value");
    I18n::reg("chn_col_note", "備考", "Notes");
    I18n::reg("chn_notcalc", "未計算", "not computed");
    I18n::reg("chn_m_fspl", "自由空間損失 (Friis)", "Free-space loss (Friis)");
    I18n::reg("chn_m_fspl_note", "L = 20log10(4πd/λ), λ = %1 · ITU-R P.525",
              "L = 20log10(4πd/λ), λ = %1 · ITU-R P.525");
    I18n::reg("chn_m_2ray", "経路損失 (2波モデル)",
              "Path loss (two-ray model)");
    I18n::reg("chn_m_2ray_note",
              "直接波 + 大地反射 (Γ = −%1) の干渉。建物透過・散乱は含まない",
              "interference of the direct and ground-reflected rays "
              "(Γ = −%1); no building penetration or scattering");
    I18n::reg("chn_m_env", "経路損失 (環境モデル)",
              "Path loss (environment model)");
    I18n::reg("chn_m_env_indoor",
              "ITU-R P.1238 屋内 (N = %1, 階層貫通損 0 dB)",
              "ITU-R P.1238 indoor (N = %1, no floor penetration)");
    I18n::reg("chn_m_env_hata", "奥村-秦 市街地 (Hata 1980)",
              "Okumura-Hata urban (Hata 1980)");
    I18n::reg("chn_m_env_hata_big", "奥村-秦 市街地・大都市補正 (Hata 1980)",
              "Okumura-Hata urban, large-city correction (Hata 1980)");
    I18n::reg("chn_m_env_cost", "COST-231 Hata 市街地 (%1 dB 都市補正)",
              "COST-231 Hata urban (%1 dB city correction)");
    I18n::reg("chn_m_env_2ray", "2 波モデル (見通し内)",
              "two-ray model (line of sight)");
    I18n::reg("chn_m_env_none", "—", "—");
    I18n::reg("chn_m_env_note_indoor",
              "L = 20log10(f[MHz]) + N·log10(d[m]) − 28。N は距離損失係数で、"
              "自由空間が 20、屋内オフィスの代表値が 30 (ITU-R P.1238-11)",
              "L = 20log10(f[MHz]) + N·log10(d[m]) − 28. N is the distance "
              "power-loss coefficient: 20 in free space, 30 for a typical "
              "office (ITU-R P.1238-11)");
    I18n::reg("chn_m_env_note_hata",
              "測定データの当てはめ (適用範囲: f 150–1500 MHz, 基地局高 "
              "30–200 m, 移動局高 1–10 m, 距離 1–20 km)",
              "an empirical fit to measurements (valid for f 150–1500 MHz, "
              "base 30–200 m, mobile 1–10 m, distance 1–20 km)");
    I18n::reg("chn_m_env_note_cost",
              "奥村-秦の 1.5–2 GHz 拡張 (適用範囲: 基地局高 30–200 m, "
              "移動局高 1–10 m, 距離 1–20 km)",
              "the 1.5–2 GHz extension of Okumura-Hata (base 30–200 m, "
              "mobile 1–10 m, distance 1–20 km)");
    I18n::reg("chn_m_env_note_2ray",
              "車内・車車間は見通しが基本なので、2 波モデルをそのまま使います",
              "in-vehicle and V2V links are line-of-sight, so the two-ray "
              "model is used as it stands");
    I18n::reg("chn_m_env_out",
              "適用範囲の外なので出しません (%1)。範囲外で経験式を外挿すると"
              "数字の形をした嘘になります",
              "outside the model's validity range, so no value is shown (%1). "
              "Extrapolating an empirical fit would be a lie in the shape of "
              "a number");
    I18n::reg("chn_m_env_tunnel",
              "トンネル・地下の経路損失は公表の経験式を持っていません "
              "(導波管モードの解析かレイトレースが要ります)",
              "there is no published empirical model for tunnels and "
              "underground here (a waveguide-mode analysis or ray tracing is "
              "needed)");
    I18n::reg("chn_m_bp", "ブレークポイント距離", "Breakpoint distance");
    I18n::reg("chn_m_bp_note", "d_bp = 4·h_t·h_r/λ — これより遠方は n ≈ 4",
              "d_bp = 4·h_t·h_r/λ — beyond this the exponent tends to 4");
    I18n::reg("chn_m_rx", "受信電力", "Received power");
    I18n::reg("chn_m_rx_note", "EIRP − 経路損失(2波) + 受信利得",
              "EIRP − two-ray path loss + RX gain");
    I18n::reg("chn_m_rx_env_note", "EIRP − 経路損失(%1) + 受信利得",
              "EIRP − path loss (%1) + RX gain");
    I18n::reg("chn_m_ds", "遅延スプレッド (RMS)", "Delay spread (RMS)");
    I18n::reg("chn_m_ds_note",
              "多重波の分布が要るためレイトレース / FDTD の実行が必要 "
              "(2波だけの遅延差は上の「遅延差 τ (2波)」の行)",
              "needs the multipath distribution, i.e. a ray-tracing or FDTD "
              "run (the two-ray-only delay difference is in the “excess delay "
              "τ” row above)");
    I18n::reg("chn_m_tau", "遅延差 τ (2波)", "Excess delay τ (two-ray)");
    I18n::reg("chn_m_tau_note", "τ = (d_ref − d_los)/c",
              "τ = (d_ref − d_los)/c");
    I18n::reg("chn_m_as", "角度スプレッド (方位)", "Angular spread (azimuth)");
    I18n::reg("chn_m_as_note",
              "到来角分布が要るためレイトレース / FDTD の実行が必要",
              "needs the angle-of-arrival distribution, i.e. a ray-tracing or "
              "FDTD run");
    I18n::reg("chn_m_k", "K-factor (2波モデル)", "K-factor (two-ray)");
    I18n::reg("chn_m_k_note", "直接波/反射波の電力比 = (d_ref/(d_los·|Γ|))²",
              "direct-to-reflected power ratio = (d_ref/(d_los·|Γ|))²");
    I18n::reg("chn_m_n", "経路損失指数 n", "Path-loss exponent n");
    I18n::reg("chn_m_n_note", "2波モデルの d〜2d 局所勾配 (自由空間 n = 2)",
              "local slope of the two-ray model between d and 2d "
              "(free space n = 2)");
    I18n::reg("chn_m_noise", "雑音電力 / SNR", "Noise power / SNR");
    I18n::reg("chn_m_noise_note", "N = kT0B + NF (T0 = 290 K), SNR = Prx − N",
              "N = kT0B + NF (T0 = 290 K), SNR = Prx − N");
    I18n::reg("chn_m_cap", "チャネル容量 (SISO Shannon)",
              "Channel capacity (SISO Shannon)");
    I18n::reg("chn_m_cap_note",
              "C = B·log2(1+SNR)。MIMO 4×4 の多重利得はチャネル行列が要るため"
              "未計算",
              "C = B·log2(1+SNR). The 4×4 MIMO multiplexing gain needs the "
              "channel matrix and is not computed");
    I18n::reg("chn_btn_heat", "🗺 カバレッジヒートマップ", "🗺 Coverage heat map");
    I18n::reg("chn_btn_pdp", "📊 電力遅延プロファイル",
              "📊 Power-delay profile");
    I18n::reg("chn_btn_h5", "💾 チャネル係数 (.h5) 書出",
              "💾 Export channel coefficients (.h5)");
    I18n::reg("chn_metrics_hint",
              "▸ 3GPP TR 38.901 形式のチャネルモデル係数の書出は未実装です。",
              "▸ Export as 3GPP TR 38.901 channel-model coefficients is not "
              "implemented yet.");
    I18n::reg("chn_model_note",
              "▸ 値は「平面大地・完全反射の 2 波モデル (見通し内)」と Friis の"
              "自由空間損失による実計算です (式と出典は src/em/RadioPropagation.h)。"
              "建物・壁の透過損失、家具や人体の散乱、多重反射は含みません — "
              "それらを含む指標 (RMS 遅延スプレッド・角度スプレッド) は"
              "レイトレース / FDTD の実行が必要で「未計算」と表示します。"
              "環境の選択は経路損失の経験式 (屋内 = ITU-R P.1238、市街地 = "
              "奥村-秦 / COST-231、車内・車車間 = 2 波モデル) を選び、受信電力・"
              "SNR・容量に効きます。解析手法 (レイトレース / FDTD / "
              "ハイブリッド) の選択は計算に反映されません。",
              "▸ The values are computed with the flat-earth perfectly-"
              "reflecting two-ray model (line of sight) and the Friis "
              "free-space loss (formulas and sources in "
              "src/em/RadioPropagation.h). Building or wall penetration, "
              "scattering from furniture and people and higher-order "
              "reflections are not included — metrics that need them (RMS "
              "delay spread, angular spread) require a ray-tracing or FDTD run "
              "and are shown as “not computed”. The environment selection "
              "picks the empirical path-loss model (indoor = ITU-R P.1238, "
              "urban = Okumura-Hata / COST-231, in-vehicle and V2V = the "
              "two-ray model) and drives the received power, SNR and "
              "capacity. The analysis-method selection (ray tracing / FDTD / "
              "hybrid) does not enter the calculation.");
    I18n::reg("chn_uw_env",
              "間取り / 地形 STL の読み込みと、材料の透過損失 DB・散乱のチェック",
              "loading the floor-plan / terrain STL and the material "
              "transmission-loss / scattering check boxes");
    I18n::reg("chn_uw_env_ok",
              "環境の選択と経路損失モデルのパラメータ (下の「経路損失 "
              "(環境モデル)」の行と、受信電力・SNR・容量に効きます)",
              "the environment selection and the path-loss model parameters "
              "(they drive the environment path-loss row below, and the "
              "received power, SNR and capacity)");
    I18n::reg("chn_model_row", "経路損失モデル", "Path-loss model");
    I18n::reg("chn_indoor_n", "距離損失係数 N", "Distance coefficient N");
    I18n::reg("chn_city", "都市規模", "City size");
    I18n::reg("chn_large_city", "大都市 (移動局高補正)",
              "Large city (mobile-height correction)");
    I18n::reg("chn_m_array", "アレイ利得 (ビームフォーミング)",
              "Array gain (beamforming)");
    I18n::reg("chn_m_array_note",
              "10log10(N) — N 素子を同相合成した上限。"
              "単一素子に対する増分なので、EIRP に既にアレイ分が入っていれば"
              "二重計上になる (上の受信電力には加えていません)",
              "10log10(N) - the upper bound for N elements combined in phase. "
              "It is the increment over a single element, so adding it on top "
              "of an EIRP that already includes the array double-counts "
              "(it is not added to the received power above)");
    I18n::reg("chn_m_array_off",
              "ビームフォーミングのチェックが外れています (単一素子)",
              "Beamforming is unchecked (single element)");
    I18n::reg("chn_m_mimo", "空間多重の容量上限 (MIMO)",
              "Spatial-multiplexing capacity (MIMO)");
    I18n::reg("chn_m_mimo_note",
              "min(Nt,Nr)·B·log2(1+SNR/Nt) — %1×%2、送信電力を Nt 本へ等分し"
              "等利得な固有モードが立つと仮定した上限。"
              "実チャネルの相関やランク落ちは含みません",
              "min(Nt,Nr)*B*log2(1+SNR/Nt) - %1x%2, an upper bound assuming the "
              "transmit power is split evenly over Nt and equal-gain "
              "eigenmodes exist. Channel correlation and rank loss are not "
              "included");
    I18n::reg("chn_m_mimo_off",
              "MIMO のチェックが外れています (1×1 = 上の Shannon 容量と同じ)",
              "MIMO is unchecked (1x1 - same as the Shannon capacity above)");
    I18n::reg("chn_cov_title", "カバレッジ (受信電力 [dBm])",
              "Coverage (received power [dBm])");
    I18n::reg("chn_cov_note",
              "半幅 %1 の水平面に受信点を並べ、2 波モデルで受信電力を求めた図。"
              "%2 … %3 dBm。送信点は「中央」で、アンテナ指向性と遮蔽物は"
              "含まないので円対称になる (見通し内の距離依存を見る図)",
              "Received power over a horizontal plane of half-span %1, from the "
              "two-ray model; %2 to %3 dBm. The transmitter sits at the centre; "
              "antenna patterns and obstacles are not included, so the map is "
              "circularly symmetric (it shows the line-of-sight distance "
              "dependence)");
    I18n::reg("chn_cov_off",
              "カバレッジ図は受信点が「格子」のときだけ描きます "
              "(経路・個別点は配置の入力が要るため未実装)",
              "The coverage map is drawn only when the receive points are a "
              "grid (routes and individual points need a layout input, which is "
              "not implemented)");
    I18n::reg("chn_cov_bad",
              "リンク条件の入力が不正なためカバレッジ図を描けません",
              "The coverage map needs valid link-budget inputs");
    I18n::reg("chn_uw_txrx",
              "AP の配置そのもの (間取りや地形に合わせて置くには配置の入力と"
              "レイトレーサが要ります — ここでは等間隔の円周配置に固定)",
              "the AP layout itself (placing APs against a floor plan or terrain "
              "needs a layout input and a ray tracer — here they are fixed to an "
              "evenly spaced ring)");
    I18n::reg("chn_uw_txrx_ok",
              "基地局/AP の台数と配置半径 — カバレッジ図が複数局になり、"
              "各点で最も強い局に繋いだときの受信電力・SINR・カバー率になります。"
              "MIMO 4×4 とビームフォーミングのチェックは下のチャネル特性表の"
              "「アレイ利得」「空間多重の容量上限」、受信点の種別は「格子」で"
              "カバレッジ図",
              "the AP count and ring radius — the coverage map becomes "
              "multi-AP, showing the received power, SINR and covered fraction "
              "when each point attaches to its strongest AP. The MIMO 4x4 and "
              "beamforming checkboxes drive the \"array gain\" and "
              "\"spatial-multiplexing capacity\" rows of the channel table, and "
              "the receive-point kind (\"grid\") draws the coverage map");
    // 複数 AP のカバレッジ
    I18n::reg("chn_ap_radius", "配置半径", "Ring radius");
    I18n::reg("chn_cov_quantity", "図に出す量", "Map quantity");
    I18n::reg("chn_cov_rsrp", "受信電力 (最良サーバ)", "Received power (best server)");
    I18n::reg("chn_cov_sinr", "SINR", "SINR");
    I18n::reg("chn_cov_thr", "カバー判定の閾値", "Coverage threshold");
    I18n::reg("chn_cov_title_sinr", "カバレッジ (SINR [dB])",
              "Coverage (SINR [dB])");
    I18n::reg("chn_cov_note_multi",
              "半幅 %1 の水平面に受信点を並べ、%2 局を半径 %3 の円周上に等間隔で"
              "置いて、各点で最も強い局に繋いだ図。%4 … %5 %6。"
              "残りの局は同一チャネル干渉として SINR = C/(I+N) に入れている "
              "(電力は真値で足す)。閾値 %7 dBm 以上が %8 %。"
              "最も強い局は最も近い局とは限らない (2 波モデルの干渉ヌルで"
              "近い局が弱くなる点がある)。アンテナ指向性と遮蔽物は含まない",
              "Receive points over a horizontal plane of half-span %1, with %2 "
              "APs evenly spaced on a ring of radius %3, each point attaching to "
              "its strongest AP; %4 to %5 %6. The remaining APs enter the "
              "SINR = C/(I+N) as co-channel interference (powers added in linear "
              "units). %8 % of the points are at or above the %7 dBm threshold. "
              "The strongest AP is not always the nearest one — two-ray "
              "interference nulls can weaken a near AP. Antenna patterns and "
              "obstacles are not included");
    return true;
}();

// ── 環境モデルごとの既定ファイル名 (モックの三項演算子をそのまま転記) ───────
const char kEnvFileIndoor[] = "office_floor3.ifc";
const char kEnvFileOther[]  = "city_shibuya.osm";

// ── 小物ヘルパー (mock の muted / q-table / Seg 相当) ───────────────────────
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

// 上の widget 版 (表示・非表示を切り替えたい行はこちらを使う — 返した
// QWidget ごと setVisible できる)
QWidget *formRowWidget(const QString &label, QWidget *field, QWidget *parent)
{
    auto *box = new QWidget(parent);
    auto *f = new QFormLayout(box);
    f->setContentsMargins(0, 0, 0, 0);
    f->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    f->setLabelAlignment(Qt::AlignLeft);
    f->setHorizontalSpacing(8);
    f->setVerticalSpacing(4);
    f->addRow(label, field);
    return box;
}

// mock の <Row label> 単発版。SectionBox::form() は 1 枚しか持てないため、
// hint を間に挟むセクションではこれで 1 行ずつ vbox に積む。
QFormLayout *formRow(const QString &label, QLayout *field)
{
    auto *f = new QFormLayout();
    f->setContentsMargins(0, 0, 0, 0);
    f->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    f->setLabelAlignment(Qt::AlignLeft);
    f->setHorizontalSpacing(8);
    f->setVerticalSpacing(4);
    f->addRow(label, field);
    return f;
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

QTableWidgetItem *textItem(const QString &s) { return new QTableWidgetItem(s); }

QTableWidgetItem *numItem(const QString &s)
{
    auto *it = new QTableWidgetItem(s);
    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
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
    t->verticalHeader()->setDefaultSectionSize(24);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setMinimumHeight(minH);
    return t;
}

// チャネル特性表の行 (値は fillMetricsTable が実計算で埋める)。
// note が計算値を含む行は書式引数を持つので、行ごとに fillMetricsTable で作る。
enum MetricRow {
    RowFspl = 0, RowTwoRay, RowEnv, RowBreak, RowRx, RowN, RowK, RowTau,
    RowDelaySpread, RowAngleSpread, RowNoise, RowCapacity,
    RowArrayGain, RowMimoCap, RowCount
};
const char *kMetricNameKey[RowCount] = {
    "chn_m_fspl", "chn_m_2ray", "chn_m_env", "chn_m_bp", "chn_m_rx",
    "chn_m_n", "chn_m_k", "chn_m_tau", "chn_m_ds", "chn_m_as", "chn_m_noise",
    "chn_m_cap", "chn_m_array", "chn_m_mimo",
};

// 環境の選択 (屋内 / 市街地 / 車内・車車間 / トンネル) から使う経験式を決める。
// **周波数と幾何が適用範囲の外なら使わない** — 経験式は測定データの当てはめ
// なので、範囲外の外挿は数字の形をした嘘になる。
struct EnvModel {
    bool    hasLoss = false;   // 経路損失を出せるか
    double  lossDb = 0.0;
    QString name;              // 使ったモデル名 (表に出す)
    QString note;              // 出所と適用範囲 / 出せない理由
};

// 選択した周波数帯の代表中心周波数 [GHz] (モックの帯域区分に対応)。
// 帯域を切り替えたときの既定値であって、利用者が直接編集できる。
double bandCenterGHz(int band)
{
    switch (band) {
    case 0:  return 0.9;      // < 1 GHz
    case 1:  return 3.5;      // Sub-6
    case 2:  return 28.0;     // ミリ波
    default: return 140.0;    // サブ THz
    }
}

// 距離の書式 (m / km)
QString fmtDistance(double m)
{
    if (!(m > 0.0) || !std::isfinite(m)) return QStringLiteral("—");
    if (m >= 1000.0) return QStringLiteral("%1 km").arg(m / 1000.0, 0, 'f', 2);
    return QStringLiteral("%1 m").arg(m, 0, 'f', 1);
}

// 波長の書式 (mm / m)
QString fmtWavelength(double m)
{
    if (!(m > 0.0) || !std::isfinite(m)) return QStringLiteral("—");
    if (m < 1.0) return QStringLiteral("%1 mm").arg(m * 1000.0, 0, 'f', 2);
    return QStringLiteral("%1 m").arg(m, 0, 'f', 3);
}
} // namespace

// ── ChannelTab ──────────────────────────────────────────────────────────────
ChannelTab::ChannelTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // ── 電波伝搬・チャネル解析 (概要 + 環境 + 周波数帯 + 手法) ──────────────
    auto *st = new SectionBox(I18n::tr("chn_title"), body);
    st->vbox()->addWidget(makeHint(I18n::tr("chn_hint"), st));
    st->vbox()->addWidget(segRow(st, &m_env, { I18n::tr("chn_env_indoor"),
                                               I18n::tr("chn_env_urban"),
                                               I18n::tr("chn_env_vehicle"),
                                               I18n::tr("chn_env_tunnel") }, 0));
    st->form()->addRow(I18n::tr("chn_band"),
                       segRow(st, &m_band, { I18n::tr("chn_band_sub1"),
                                             I18n::tr("chn_band_sub6"),
                                             I18n::tr("chn_band_mmw"),
                                             I18n::tr("chn_band_thz") }, 1));
    st->form()->addRow(I18n::tr("chn_method"),
                       segRow(st, &m_method, { I18n::tr("chn_method_rt"),
                                               I18n::tr("chn_method_fdtd"),
                                               I18n::tr("chn_method_hybrid") }, 2));
    v->addWidget(st);

    // ── 環境モデル / Environment ────────────────────────────────────────────
    // モックは Row(間取り) → hint → Row(材料) の順なので、
    // SectionBox::form() を使わずに QFormLayout を 2 枚に分けて順序を保つ。
    auto *se = new SectionBox(I18n::tr("chn_envm_section"), body);
    m_envFile = new QLineEdit(QString::fromUtf8(kEnvFileIndoor), se);
    auto *fr = new QHBoxLayout();
    fr->addWidget(m_envFile, 1);
    // 「📁 参照…」のみ実配線 (選択パスを欄へ反映する。読込・解析は未実装)
    auto *browseBtn = new QPushButton(I18n::tr("chn_browse"), se);
    connect(browseBtn, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, I18n::tr("chn_layout"), m_envFile->text(),
            I18n::tr("chn_stl_filter"));
        if (!path.isEmpty()) m_envFile->setText(path);
    });
    fr->addWidget(browseBtn);
    se->vbox()->addLayout(formRow(I18n::tr("chn_layout"), fr));

    se->vbox()->addWidget(makeHint(I18n::tr("chn_envm_hint"), se));

    m_matDb      = makeCheck(I18n::tr("chn_mat_db"), true, se);
    m_matScatter = makeCheck(I18n::tr("chn_mat_scatter"), false, se);
    auto *mr = new QHBoxLayout();
    mr->addWidget(m_matDb);
    mr->addWidget(m_matScatter);
    mr->addStretch(1);
    se->vbox()->addLayout(formRow(I18n::tr("chn_material"), mr));
    // 経路損失モデル — 上の環境の選択から自動で決まる。屋内の距離損失係数
    // だけは値が場所で大きく変わるので入力にする (既定は P.1238 の office)。
    m_modelName = new QLabel(se);
    m_modelName->setWordWrap(true);
    se->vbox()->addWidget(
        formRowWidget(I18n::tr("chn_model_row"), m_modelName, se));
    m_indoorN = new QDoubleSpinBox(se);
    m_indoorN->setRange(10.0, 50.0);
    m_indoorN->setDecimals(1);
    m_indoorN->setSingleStep(1.0);
    m_indoorN->setValue(30.0);
    m_indoorN->setMaximumWidth(90);
    m_indoorNRow = formRowWidget(I18n::tr("chn_indoor_n"), m_indoorN, se);
    se->vbox()->addWidget(m_indoorNRow);
    m_largeCity = makeCheck(I18n::tr("chn_large_city"), false, se);
    m_largeCityRow = formRowWidget(I18n::tr("chn_city"), m_largeCity, se);
    se->vbox()->addWidget(m_largeCityRow);
    connect(m_indoorN, &QDoubleSpinBox::valueChanged,
            this, &ChannelTab::recompute);
    connect(m_largeCity, &QCheckBox::toggled, this, &ChannelTab::recompute);
    // 間取り STL と材料 DB のチェックはどこにも読まれていない (未実装)。
    // 経路損失モデルの選択とパラメータは効く。
    se->vbox()->addWidget(tabhelp::unwiredNote(se, I18n::tr("chn_uw_env"),
                                               I18n::tr("chn_uw_env_ok")));
    v->addWidget(se);

    // ── 送受信 / TX-RX ──────────────────────────────────────────────────────
    auto *sx = new SectionBox(I18n::tr("chn_txrx_section"), body);
    m_apCount     = numEdit("4", sx);
    m_mimo        = makeCheck(I18n::tr("chn_mimo"), true, sx);
    m_beamforming = makeCheck(I18n::tr("chn_beamforming"), true, sx);
    auto *ar = new QHBoxLayout();
    ar->addWidget(m_apCount);
    ar->addWidget(new QLabel(I18n::tr("chn_ap_unit"), sx));
    ar->addWidget(m_mimo);
    ar->addWidget(m_beamforming);
    ar->addStretch(1);
    sx->form()->addRow(I18n::tr("chn_ap"), ar);

    // 複数局の配置半径・図に出す量・カバー判定の閾値 (カバレッジ図へ効く)
    m_apRadius = numEdit("50", sx);
    auto *rr = new QHBoxLayout();
    rr->addWidget(m_apRadius);
    rr->addWidget(new QLabel(QStringLiteral("m"), sx));
    rr->addStretch(1);
    sx->form()->addRow(I18n::tr("chn_ap_radius"), rr);
    m_covQuantity = new QComboBox(sx);
    m_covQuantity->addItem(I18n::tr("chn_cov_rsrp"));
    m_covQuantity->addItem(I18n::tr("chn_cov_sinr"));
    sx->form()->addRow(I18n::tr("chn_cov_quantity"), m_covQuantity);
    m_covThreshold = numEdit("-90", sx);
    auto *tr2 = new QHBoxLayout();
    tr2->addWidget(m_covThreshold);
    tr2->addWidget(new QLabel(QStringLiteral("dBm"), sx));
    tr2->addStretch(1);
    sx->form()->addRow(I18n::tr("chn_cov_thr"), tr2);
    connect(m_covQuantity, &QComboBox::currentIndexChanged,
            this, &ChannelTab::recompute);
    for (QLineEdit *e : { m_apCount, m_apRadius, m_covThreshold })
        connect(e, &QLineEdit::editingFinished, this, &ChannelTab::recompute);

    sx->form()->addRow(I18n::tr("chn_rx"),
                       segRow(sx, &m_rxKind, { I18n::tr("chn_rx_grid"),
                                               I18n::tr("chn_rx_route"),
                                               I18n::tr("chn_rx_points") }, 0));
    // 受信点=格子 のときだけカバレッジ図を描く
    m_coverage = new FieldHeatmap(sx);
    m_coverage->setMinimumHeight(220);
    m_coverage->setTitle(I18n::tr("chn_cov_title"));
    sx->vbox()->addWidget(m_coverage);
    m_coverageNote = new QLabel(sx);
    m_coverageNote->setWordWrap(true);
    m_coverageNote->setStyleSheet("color:#666; font-size:11px;");
    sx->vbox()->addWidget(m_coverageNote);
    connect(m_rxKind, &QButtonGroup::idClicked, this, &ChannelTab::recompute);

    // AP 台数はどこにも読まれていない。MIMO / ビームフォーミングはチャネル
    // 特性表の 2 行に、受信点の種別はカバレッジ図に効くので併記する
    sx->vbox()->addWidget(tabhelp::unwiredNote(sx, I18n::tr("chn_uw_txrx"),
                                               I18n::tr("chn_uw_txrx_ok")));
    v->addWidget(sx);

    // ── リンク条件 / Link budget inputs (チャネル特性の計算入力) ────────────
    auto *sl = new SectionBox(I18n::tr("chn_link_section"), body);
    sl->vbox()->addWidget(makeHint(I18n::tr("chn_link_hint"), sl));
    m_freqGHz = numEdit(QString::number(bandCenterGHz(1)), sl);
    m_dist    = numEdit(QStringLiteral("100"), sl);
    m_hTx     = numEdit(QStringLiteral("10"), sl);
    m_hRx     = numEdit(QStringLiteral("1.5"), sl);
    m_eirp    = numEdit(QStringLiteral("30"), sl);
    m_gRx     = numEdit(QStringLiteral("0"), sl);
    m_bw      = numEdit(QStringLiteral("100"), sl);
    m_nf      = numEdit(QStringLiteral("7"), sl);
    m_refl    = numEdit(QStringLiteral("1.0"), sl);
    const struct { const char *key; QLineEdit *edit; const char *unit; }
    kLinkRows[] = {
        { "chn_freq", m_freqGHz, "GHz"  },
        { "chn_dist", m_dist,    "m"    },
        { "chn_htx",  m_hTx,     "m"    },
        { "chn_hrx",  m_hRx,     "m"    },
        { "chn_eirp", m_eirp,    "dBm"  },
        { "chn_grx",  m_gRx,     "dBi"  },
        { "chn_bw",   m_bw,      "MHz"  },
        { "chn_nf",   m_nf,      "dB"   },
        { "chn_refl", m_refl,    ""     },
    };
    for (const auto &r : kLinkRows) {
        auto *row = new QHBoxLayout();
        row->addWidget(r.edit);
        if (*r.unit) row->addWidget(new QLabel(QString::fromUtf8(r.unit), sl));
        row->addStretch(1);
        sl->form()->addRow(I18n::tr(r.key), row);
        connect(r.edit, &QLineEdit::textChanged,
                this, &ChannelTab::recompute);
    }
    // MIMO / ビームフォーミングもチャネル特性表に効くので再計算させる
    connect(m_mimo, &QCheckBox::toggled, this, &ChannelTab::recompute);
    connect(m_beamforming, &QCheckBox::toggled, this, &ChannelTab::recompute);
    m_inputError = new QLabel(sl);
    m_inputError->setWordWrap(true);
    m_inputError->setStyleSheet("color:#C0392B; font-size:11px;");
    m_inputError->setVisible(false);
    sl->vbox()->addWidget(m_inputError);
    v->addWidget(sl);

    // ── チャネル特性 / Channel metrics — 上のリンク条件からの実計算 ─────────
    auto *sm = new SectionBox(I18n::tr("chn_metrics_section"), body);
    m_metrics = makeTable({ I18n::tr("chn_col_metric"), I18n::tr("chn_col_value"),
                            I18n::tr("chn_col_note") }, RowCount, sm,
                          26 * RowCount + 30);   // 全行を折り返さず見せる
    sm->vbox()->addWidget(m_metrics);
    sm->vbox()->addWidget(makeHint(I18n::tr("chn_model_note"), sm));

    // ヒートマップ / PDP / 書出のボタンはいずれも未配線 (絶対規則 5)
    auto *bb = new QHBoxLayout();
    auto *heatBtn = new QPushButton(I18n::tr("chn_btn_heat"), sm);
    auto *pdpBtn  = new QPushButton(I18n::tr("chn_btn_pdp"), sm);
    auto *h5Btn   = new QPushButton(I18n::tr("chn_btn_h5"), sm);
    for (QPushButton *b : { heatBtn, pdpBtn, h5Btn }) {
        tabhelp::markNotImplemented(b, I18n::tr(tabhelp::notimpl::kEngine));
        bb->addWidget(b);
    }
    bb->addStretch(1);
    sm->vbox()->addLayout(bb);
    sm->vbox()->addWidget(makeHint(I18n::tr("chn_metrics_hint"), sm));
    v->addWidget(sm);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    connect(m_env, &QButtonGroup::idClicked, this, [this](int id) {
        // ローカル state のみ (Project に対応フィールド無し → 永続化しない)
        if (m_updating) return;
        m_envIdx = id;
        onEnvChanged();
    });
    // 周波数帯を切り替えたら中心周波数の既定値を入れ直す (編集は自由)
    connect(m_band, &QButtonGroup::idClicked, this, [this](int id) {
        if (m_updating) return;
        m_freqGHz->setText(QString::number(bandCenterGHz(id)));
    });
    connect(project, &Project::loaded, this, &ChannelTab::refresh);
    refresh();
}

void ChannelTab::refresh()
{
    m_updating = true;
    if (auto *b = m_env->button(m_envIdx)) b->setChecked(true);
    m_updating = false;
    onEnvChanged();     // 内部で recompute() まで行う
}

// mock: defaultValue={env==="indoor" ? "office_floor3.ifc" : "city_shibuya.osm"}
void ChannelTab::onEnvChanged()
{
    m_envFile->setText(QString::fromUtf8(m_envIdx == 0 ? kEnvFileIndoor
                                                       : kEnvFileOther));
    recompute();      // 環境は経路損失モデルを決めるので表も出し直す
}

// リンク条件 → チャネル特性 (見通し内の伝搬モデルによる実計算)。
// 計算できない指標 (多重波が要るもの・入力不正) は「未計算」+ 理由を出す。
void ChannelTab::recompute()
{
    namespace prop = ofd::em::propagation;
    const QString nc = I18n::tr("chn_notcalc");

    auto value = [](QLineEdit *e, bool &ok) {
        const double v = e->text().trimmed().toDouble(&ok);
        if (!std::isfinite(v)) ok = false;
        return v;
    };
    bool ok = true, all = true;
    const double fGHz = value(m_freqGHz, ok); all = all && ok;
    const double d    = value(m_dist, ok);    all = all && ok;
    const double ht   = value(m_hTx, ok);     all = all && ok;
    const double hr   = value(m_hRx, ok);     all = all && ok;
    const double eirp = value(m_eirp, ok);    all = all && ok;
    const double grx  = value(m_gRx, ok);     all = all && ok;
    const double bwM  = value(m_bw, ok);      all = all && ok;
    const double nf   = value(m_nf, ok);      all = all && ok;
    const double refl = value(m_refl, ok);    all = all && ok;
    // 物理的に意味のある範囲か (負の距離・負の高さ・|Γ| > 1 は受け付けない)
    const bool valid = all && fGHz > 0.0 && d > 0.0 && ht >= 0.0 && hr >= 0.0
                       && bwM > 0.0 && refl >= 0.0 && refl <= 1.0;
    m_inputError->setText(I18n::tr("chn_bad_input"));
    m_inputError->setVisible(!valid);

    // 行の見出しは常に埋める (値だけ差し替える)
    for (int r = 0; r < RowCount; ++r)
        m_metrics->setItem(r, 0, textItem(I18n::tr(kMetricNameKey[r])));

    auto setRow = [this](int r, const QString &val, const QString &note) {
        m_metrics->setItem(r, 1, numItem(val));
        m_metrics->setItem(r, 2, textItem(note));
    };

    if (!valid) {
        for (int r = 0; r < RowCount; ++r)
            setRow(r, nc, I18n::tr("chn_bad_input"));
        if (m_coverage) {
            m_coverage->clearData();
            m_coverageNote->setText(I18n::tr("chn_cov_bad"));
        }
        return;
    }

    const double f = fGHz * 1e9;
    const double lam = prop::wavelength(f);
    const double fspl = prop::freeSpacePathLossDb(d, f);
    const double l2ray = prop::twoRayPathLossDb(d, ht, hr, f, refl);
    const double dbp = prop::breakpointDistance(ht, hr, f);

    // ── 環境の選択 → 経路損失の経験式 ──
    // 適用範囲の外では値を出さない (外挿しない)。出せたときは受信電力・
    // SNR・容量もこの損失で計算する。
    EnvModel env;
    switch (m_envIdx) {
    case 0: {          // 屋内 — ITU-R P.1238
        const double N = m_indoorN ? m_indoorN->value() : 30.0;
        env.hasLoss = true;
        env.lossDb = prop::indoorP1238PathLossDb(d, f, N);
        env.name = I18n::tr("chn_m_env_indoor").arg(N, 0, 'f', 1);
        env.note = I18n::tr("chn_m_env_note_indoor");
        break;
    }
    case 1: {          // 市街地 — 奥村-秦 / COST-231
        const bool big = m_largeCity && m_largeCity->isChecked();
        if (prop::hataApplicable(d, f, ht, hr)) {
            env.hasLoss = true;
            env.lossDb = prop::hataUrbanPathLossDb(d, f, ht, hr, big);
            env.name = I18n::tr(big ? "chn_m_env_hata_big" : "chn_m_env_hata");
            env.note = I18n::tr("chn_m_env_note_hata");
        } else if (prop::cost231Applicable(d, f, ht, hr)) {
            const double c = big ? 3.0 : 0.0;
            env.hasLoss = true;
            env.lossDb = prop::cost231HataPathLossDb(d, f, ht, hr, c, big);
            env.name = I18n::tr("chn_m_env_cost").arg(c, 0, 'f', 0);
            env.note = I18n::tr("chn_m_env_note_cost");
        } else {
            env.name = I18n::tr("chn_m_env_none");
            env.note = I18n::tr("chn_m_env_out")
                           .arg(I18n::tr("chn_m_env_note_hata"));
        }
        break;
    }
    case 2:            // 車内・車車間 — 見通しなので 2 波モデルそのもの
        env.hasLoss = true;
        env.lossDb = l2ray;
        env.name = I18n::tr("chn_m_env_2ray");
        env.note = I18n::tr("chn_m_env_note_2ray");
        break;
    default:           // トンネル・地下 — 公表の経験式を持っていない
        env.name = I18n::tr("chn_m_env_none");
        env.note = I18n::tr("chn_m_env_tunnel");
        break;
    }
    if (m_modelName) m_modelName->setText(env.name);
    if (m_indoorNRow) m_indoorNRow->setVisible(m_envIdx == 0);
    if (m_largeCityRow) m_largeCityRow->setVisible(m_envIdx == 1);

    // 受信電力は「使えるモデル」で計算する (環境モデルが出せないときだけ
    // 2 波モデルに戻し、その旨を備考に書く)
    const double lossUsed = env.hasLoss ? env.lossDb : l2ray;
    const double prx = prop::receivedPowerDbm(eirp, lossUsed, grx);
    const double n = prop::pathLossExponent(
        l2ray, d, prop::twoRayPathLossDb(2.0 * d, ht, hr, f, refl), 2.0 * d);
    const double kdb = prop::twoRayKFactorDb(d, ht, hr, refl);
    const double tau = prop::twoRayExcessDelay(d, ht, hr);
    const double bw = bwM * 1e6;
    const double noise = prop::thermalNoiseDbm(bw, nf);
    const double snr = prx - noise;
    const double cap = prop::shannonCapacity(bw, snr);

    setRow(RowFspl, QStringLiteral("%1 dB").arg(fspl, 0, 'f', 2),
           I18n::tr("chn_m_fspl_note").arg(fmtWavelength(lam)));
    setRow(RowTwoRay, QStringLiteral("%1 dB").arg(l2ray, 0, 'f', 2),
           I18n::tr("chn_m_2ray_note").arg(refl, 0, 'f', 2));
    setRow(RowEnv,
           env.hasLoss ? QStringLiteral("%1 dB").arg(env.lossDb, 0, 'f', 2)
                       : nc,
           env.name + QStringLiteral(" — ") + env.note);
    setRow(RowBreak, fmtDistance(dbp), I18n::tr("chn_m_bp_note"));
    setRow(RowRx, QStringLiteral("%1 dBm").arg(prx, 0, 'f', 2),
           env.hasLoss ? I18n::tr("chn_m_rx_env_note").arg(env.name)
                       : I18n::tr("chn_m_rx_note"));
    setRow(RowN, QString::number(n, 'f', 2), I18n::tr("chn_m_n_note"));
    setRow(RowK, QStringLiteral("%1 dB").arg(kdb, 0, 'f', 2),
           I18n::tr("chn_m_k_note"));
    setRow(RowTau, QStringLiteral("%1 ns").arg(tau * 1e9, 0, 'f', 2),
           I18n::tr("chn_m_tau_note"));
    // 多重波の統計が要る指標は実行しないと出せない (偽の値を出さない)
    setRow(RowDelaySpread, nc, I18n::tr("chn_m_ds_note"));
    setRow(RowAngleSpread, nc, I18n::tr("chn_m_as_note"));
    setRow(RowNoise, QStringLiteral("%1 dBm / SNR %2 dB")
                         .arg(noise, 0, 'f', 2).arg(snr, 0, 'f', 2),
           I18n::tr("chn_m_noise_note"));
    setRow(RowCapacity, QStringLiteral("%1 Mbps").arg(cap / 1e6, 0, 'f', 1),
           I18n::tr("chn_m_cap_note"));

    // ── 送受信セクションのチェックが効く 2 行 ──────────────────────────────
    // 「MIMO 4×4」はラベルどおり 4 送信 × 4 受信。外せば 1×1 (SISO)。
    // ビームフォーミングのアレイ素子数は MIMO の送信本数に合わせる。
    const int nT = m_mimo->isChecked() ? 4 : 1;
    const int nR = nT;
    const double ag = prop::arrayGainDb(m_beamforming->isChecked() ? nT : 1);
    setRow(RowArrayGain, QStringLiteral("%1 dB").arg(ag, 0, 'f', 2),
           m_beamforming->isChecked() ? I18n::tr("chn_m_array_note")
                                      : I18n::tr("chn_m_array_off"));
    const double mcap = prop::mimoCapacity(bw, snr, nT, nR);
    setRow(RowMimoCap, QStringLiteral("%1 Mbps").arg(mcap / 1e6, 0, 'f', 1),
           m_mimo->isChecked()
               ? I18n::tr("chn_m_mimo_note").arg(nT).arg(nR)
               : I18n::tr("chn_m_mimo_off"));

    // ── カバレッジ図 (受信点 = 格子 のときだけ) ────────────────────────────
    // 半幅はリンク条件の距離に合わせる (見たい距離が図に入るように)。
    // 近傍は 2 波モデルの前提を外れるので、下限を λ で切る。
    updateCoverage(d, ht, hr, f, eirp, grx, refl, lam, bwM * 1e6, nf);
}

void ChannelTab::updateCoverage(double dist, double ht, double hr, double f,
                                double eirp, double grx, double refl,
                                double lam, double noiseBw_hz,
                                double noiseFigureDb)
{
    namespace prop = ofd::em::propagation;
    if (!m_coverage) return;
    if (!m_rxKind || m_rxKind->checkedId() != 0) {   // 0 = 格子
        m_coverage->clearData();
        m_coverageNote->setText(I18n::tr("chn_cov_off"));
        return;
    }
    const int n = 121;                   // 表示用の解像度 (奇数 = 中心を含む)
    const double minD = (lam > 0.0) ? lam : 1.0;

    // 台数 1 なら中心 1 局 (従来の図と厳密に一致する)。2 局以上は円周配置。
    const int aps = std::max(1, m_apCount ? m_apCount->text().toInt() : 1);
    const double radius = m_apRadius ? m_apRadius->text().toDouble() : 0.0;
    const double thr = m_covThreshold ? m_covThreshold->text().toDouble() : -90.0;
    const bool showSinr = (m_covQuantity && m_covQuantity->currentIndex() == 1);
    const double noise = prop::thermalNoiseDbm(noiseBw_hz, noiseFigureDb);

    const prop::MultiCoverage g =
        prop::coverageMapMulti(prop::apRing(aps, radius, ht, eirp), dist, n, hr,
                               f, grx, noise, thr, refl, minD);
    if (!g.valid()) {
        m_coverage->clearData();
        m_coverageNote->setText(I18n::tr("chn_cov_bad"));
        return;
    }

    const std::vector<double> &src = showSinr ? g.sinrDb : g.bestDbm;
    double lo = src.empty() ? 0.0 : src[0], hi = lo;
    for (double v : src) { lo = std::min(lo, v); hi = std::max(hi, v); }
    // 値域を 0..1 へ正規化 (最小 = 0, 最大 = 1)。実スケールは注記に数値で出す
    QVector<double> cells;
    cells.reserve(int(src.size()));
    const double span = (hi > lo) ? (hi - lo) : 1.0;
    for (double v : src) cells.push_back((v - lo) / span);
    m_coverage->setTitle(I18n::tr(showSinr ? "chn_cov_title_sinr"
                                           : "chn_cov_title"));
    m_coverage->setData(cells, g.n, g.n);
    m_coverageNote->setText(I18n::tr("chn_cov_note_multi")
        .arg(fmtDistance(g.halfSpan_m))
        .arg(aps)
        .arg(fmtDistance(aps > 1 ? radius : 0.0))
        .arg(lo, 0, 'f', 1)
        .arg(hi, 0, 'f', 1)
        .arg(showSinr ? QStringLiteral("dB") : QStringLiteral("dBm"))
        .arg(thr, 0, 'f', 1)
        .arg(100.0 * g.coveredFraction, 0, 'f', 1));
}
