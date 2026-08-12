// OutdoorNoiseTab.cpp
#include "OutdoorNoiseTab.h"
#include "TabHelpers.h"
#include "../core/Project.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"
#include "../acoustics/core/EnvironmentalNoise.h"

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QFont>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QVector>
#include <algorithm>
#include <cmath>

using namespace ofd;
namespace env = ofd::acoustics::outdoor;

// ── タブ固有語彙 (onz_) — file-local 登録 ───────────────────────────────────
namespace {
const bool s_i18n = [] {
    using ofd::I18n;
    // タブ名 (MainWindow 配線用)
    I18n::reg("t_outdoor", "🌳 屋外騒音", "🌳 Outdoor Noise");
    // 概要 / 音源種別
    I18n::reg("onz_main_section", "屋外騒音伝搬 (ISO 9613-2 / CNOSSOS-EU)",
              "Outdoor propagation (ISO 9613-2 / CNOSSOS-EU)");
    I18n::reg("onz_main_hint",
              "道路・鉄道・工場・風車の環境騒音を距離減衰+地面効果+障壁+気象で予測。\n"
              "SoundPLAN / CadnaA 相当の機能。\n"
              "(実装済みは幾何拡散 A_div と障壁回折 A_bar のみ。地面効果・空気吸収・"
              "気象補正と、交通量から音響パワーを求める発生源モデルは未実装)",
              "Predicts environmental noise from roads, railways, plants and wind "
              "turbines with divergence + ground + barrier + meteorology.\n"
              "Equivalent to SoundPLAN / CadnaA.\n"
              "(Only geometrical divergence A_div and barrier diffraction A_bar "
              "are implemented; ground effect, air absorption, meteorology and "
              "the traffic-based emission models are not.)");
    I18n::reg("onz_src_type", "騒音源種別", "Source type");
    I18n::reg("onz_sc_road", "道路交通", "Road traffic");
    I18n::reg("onz_sc_rail", "鉄道", "Railway");
    I18n::reg("onz_sc_industry", "工場・設備", "Industry / plant");
    I18n::reg("onz_sc_wind", "風力発電", "Wind turbine");
    I18n::reg("onz_sc_aircraft", "航空機", "Aircraft");
    // 音源モデル
    I18n::reg("onz_src_section", "音源モデル", "Source model");
    I18n::reg("onz_src_note",
              "▸ ここで計算に使われるのは「工場・設備」の音源PWL だけです "
              "(点音源の基準レベルへ換算)。交通量・列車本数・機種・便数から"
              "音響パワーを求める発生源モデル (ASJ RTN-Model / CNOSSOS-EU / "
              "FHWA TNM) は未実装で、これらの入力は計算に反映されません。",
              "▸ Of the inputs here only the plant PWL (Industry) enters the "
              "computation, converted into the point-source reference level. "
              "The emission models that derive sound power from traffic volume, "
              "train counts, aircraft type or movements (ASJ RTN-Model, "
              "CNOSSOS-EU, FHWA TNM) are not implemented, so those inputs do "
              "not affect the result.");
    // road
    I18n::reg("onz_traffic", "交通量", "Traffic volume");
    I18n::reg("onz_traffic_u", "台/日", "veh/day");
    I18n::reg("onz_heavy", "大型車混入率", "Heavy-vehicle ratio");
    I18n::reg("onz_speed", "速度", "Speed");
    I18n::reg("onz_pavement", "路面", "Pavement");
    I18n::reg("onz_pav_dense", "密粒", "Dense-graded");
    I18n::reg("onz_pav_drain", "排水性 (-3dB)", "Porous (-3 dB)");
    I18n::reg("onz_pav_cobble", "ブロック (+3dB)", "Block paving (+3 dB)");
    I18n::reg("onz_model", "計算モデル", "Prediction model");
    // rail
    I18n::reg("onz_train_type", "列車種別", "Train type");
    I18n::reg("onz_train_shinkansen", "新幹線 (270km/h)", "Shinkansen (270 km/h)");
    I18n::reg("onz_train_express", "在来線特急", "Conventional express");
    I18n::reg("onz_train_commuter", "通勤電車", "Commuter train");
    I18n::reg("onz_train_freight", "貨物", "Freight");
    I18n::reg("onz_train_count", "本数", "Trains");
    I18n::reg("onz_train_count_u", "本/日", "trains/day");
    I18n::reg("onz_rolling", "転動音", "Rolling noise");
    I18n::reg("onz_structure", "構造物音 (高架)", "Structure-borne (viaduct)");
    I18n::reg("onz_aero", "空力音 (>200km/h)", "Aerodynamic (>200 km/h)");
    // industry
    I18n::reg("onz_pwl", "音源PWL", "Source PWL");
    I18n::reg("onz_operation", "稼働", "Operation");
    I18n::reg("onz_op_day", "昼のみ", "Daytime only");
    I18n::reg("onz_op_24h", "24時間", "24 hours");
    I18n::reg("onz_building_ins", "建屋遮音を考慮",
              "Include building insulation");
    I18n::reg("onz_directivity", "指向性 (開口部)", "Directivity (openings)");
    // wind
    I18n::reg("onz_turbine", "機種", "Turbine");
    I18n::reg("onz_turb_3mw", "3MW級 (ハブ高100m)", "3 MW class (hub 100 m)");
    I18n::reg("onz_turb_2mw", "2MW級", "2 MW class");
    I18n::reg("onz_turb_off5", "洋上5MW級", "Offshore 5 MW class");
    I18n::reg("onz_turb_count", "基数", "Number of units");
    I18n::reg("onz_swish", "スウィッシュ音 (振幅変調)",
              "Swish (amplitude modulation)");
    I18n::reg("onz_lowfreq", "低周波音 (20-100Hz)", "Low frequency (20-100 Hz)");
    // aircraft
    I18n::reg("onz_ac_type", "機種", "Aircraft");
    I18n::reg("onz_ac_b787", "B787 (離陸)", "B787 (takeoff)");
    I18n::reg("onz_ac_a320", "A320 (着陸)", "A320 (landing)");
    I18n::reg("onz_ac_heli", "ヘリコプター", "Helicopter");
    I18n::reg("onz_ac_drone", "ドローン (UAM)", "Drone (UAM)");
    I18n::reg("onz_flights", "便数", "Flights");
    I18n::reg("onz_flights_u", "便/日", "flights/day");
    I18n::reg("onz_ac_metric", "評価量", "Rating quantity");
    // 伝搬経路
    I18n::reg("onz_prop_section", "伝搬経路 (ISO 9613-2)",
              "Propagation effects (ISO 9613-2)");
    I18n::reg("onz_a_div", "幾何拡散 A_div (20log r)",
              "Geometrical divergence A_div (20 log r)");
    I18n::reg("onz_a_atm", "空気吸収 A_atm (温度20℃, 湿度70%)",
              "Atmospheric absorption A_atm (20°C, 70% RH)");
    I18n::reg("onz_a_gr", "地面効果 A_gr (G=0.5 半硬質)",
              "Ground effect A_gr (G=0.5, semi-hard)");
    I18n::reg("onz_a_bar", "障壁回折 A_bar (防音壁)",
              "Barrier diffraction A_bar (noise wall)");
    I18n::reg("onz_a_misc", "植栽・住宅群 A_misc", "Foliage / housing A_misc");
    I18n::reg("onz_c_met", "気象補正 C_met (風向・逆転層)",
              "Meteorological correction C_met (wind, inversion)");
    I18n::reg("onz_recv_h", "受音点高さ", "Receiver height");
    I18n::reg("onz_recv_h_u", "m (1階) / 4.5m (2階)",
              "m (1st floor) / 4.5 m (2nd floor)");
    I18n::reg("onz_recv_d", "受音点距離", "Receiver distance");
    I18n::reg("onz_recv_d_u", "m (音源から)", "m (from the source)");
    I18n::reg("onz_prop_note",
              "▸ 計算に反映されるのは A_div (幾何拡散, ISO 9613-2 §7.1) と "
              "A_bar (前川の回折減衰) だけです。A_atm / A_gr / A_misc / C_met は"
              "未実装で、チェックしても計算は変わりません。受音点の距離と高さは"
              "下の「防音壁設計」「騒音予測」で使われます。",
              "▸ Only A_div (geometrical divergence, ISO 9613-2 §7.1) and A_bar "
              "(Maekawa diffraction) enter the computation. A_atm / A_gr / "
              "A_misc / C_met are not implemented — ticking them changes "
              "nothing. The receiver distance and height feed the barrier and "
              "prediction sections below.");
    // 防音壁
    I18n::reg("onz_bar_section", "防音壁設計 (前川チャート)",
              "Barrier design (Maekawa chart)");
    I18n::reg("onz_bar_h", "高さ", "Height");
    I18n::reg("onz_bar_pos", "位置", "Position");
    I18n::reg("onz_bar_pos_u", "m (音源から)", "m (from source)");
    I18n::reg("onz_bar_top", "頂部形状", "Top shape");
    I18n::reg("onz_top_straight", "直壁", "Straight");
    I18n::reg("onz_top_y", "Y型", "Y-shape");
    I18n::reg("onz_top_branch", "枝付き", "Branched");
    I18n::reg("onz_top_absorb", "吸音型", "Absorptive");
    I18n::reg("onz_src_h", "音源高さ", "Source height");
    I18n::reg("onz_bar_freq", "回折の評価周波数", "Frequency for diffraction");
    I18n::reg("onz_bar_result",
              "回折減衰 ΔL = %1 dB  (N = %2, 経路差 δ = %3 m, %4 Hz, "
              "λ = %5 m)",
              "Diffraction loss ΔL = %1 dB (N = %2, path difference δ = %3 m, "
              "%4 Hz, λ = %5 m)");
    I18n::reg("onz_bar_clamped",
              "回折減衰 ΔL = %1 dB (前川チャートの上限で頭打ち。N = %2, "
              "δ = %3 m, %4 Hz)",
              "Diffraction loss ΔL = %1 dB (capped at the Maekawa chart limit; "
              "N = %2, δ = %3 m, %4 Hz)");
    I18n::reg("onz_bar_los",
              "見通しあり — 壁が音源と受音点を遮っていないため回折減衰は"
              "計上しません (ΔL = 0 dB)",
              "Line of sight is open — the barrier does not block the source, "
              "so no diffraction loss is counted (ΔL = 0 dB)");
    I18n::reg("onz_bar_bad",
              "未計算 — 音源高さ・壁の高さ・壁の位置・受音点の距離と高さを"
              "正の数で入力してください (壁は音源と受音点の間にある必要が"
              "あります)",
              "Not computed — enter positive values for the source height, "
              "barrier height and position, and the receiver distance and "
              "height (the barrier must lie between source and receiver)");
    I18n::reg("onz_h_freq", "周波数 [Hz]", "Frequency [Hz]");
    I18n::reg("onz_h_n", "フレネル数 N", "Fresnel number N");
    I18n::reg("onz_h_dl", "ΔL [dB]", "ΔL [dB]");
    I18n::reg("onz_bar_src",
              "▸ 出典: Z. Maekawa, “Noise reduction by screens”, Applied "
              "Acoustics 1(3), 157-173 (1968)。ΔL = 10·log₁₀(3 + 20N)、"
              "N = 2δ/λ (δ = 経路差)。半無限薄板・点音源・平坦地面・無風の"
              "近似で、上限は 24 dB。壁体の透過音・地面反射・端部からの"
              "回り込みは含みません。",
              "▸ Source: Z. Maekawa, “Noise reduction by screens”, Applied "
              "Acoustics 1(3), 157-173 (1968). ΔL = 10·log₁₀(3 + 20N) with "
              "N = 2δ/λ (δ = path difference). Semi-infinite thin screen, point "
              "source, flat ground, no wind; capped at 24 dB. Transmission "
              "through the screen, ground reflection and flanking around its "
              "ends are not included.");
    I18n::reg("onz_bar_top_note",
              "▸ 頂部形状 (Y型・枝付き・吸音型) による付加効果は製品ごとの"
              "実測データが必要なため未計算です — 直壁として計算しています。",
              "▸ The extra benefit of a shaped top (Y, branched, absorptive) "
              "needs measured product data, so it is not computed — the wall is "
              "evaluated as a plain straight screen.");
    // 騒音予測 (断面) と基準適合判定
    I18n::reg("onz_pred_section", "騒音予測と環境基準の適合判定 (断面)",
              "Noise prediction and compliance check (cross-section)");
    I18n::reg("onz_pred_hint",
              "基準距離での既知レベルを出発点に、幾何拡散 (点音源 −6 dB/距離2倍, "
              "線音源 −3 dB/距離2倍) と上の回折減衰だけで受音点レベルを求め、"
              "環境基準値と比較します。",
              "Starting from a known level at a reference distance, the receiver "
              "level is obtained from geometrical divergence (point source "
              "−6 dB per distance doubling, line source −3 dB) and the "
              "diffraction loss above, then compared with the limit value.");
    I18n::reg("onz_ref_level", "基準レベル", "Reference level");
    I18n::reg("onz_ref_dist", "基準距離", "Reference distance");
    I18n::reg("onz_src_kind", "音源の扱い", "Source treated as");
    I18n::reg("onz_src_point", "点音源 (−6 dB / 距離2倍)",
              "Point source (−6 dB per doubling)");
    I18n::reg("onz_src_line", "線音源 (−3 dB / 距離2倍)",
              "Line source (−3 dB per doubling)");
    I18n::reg("onz_pwl_derived",
              "工場・設備の音源PWL %1 dB(A) から Lp(1 m) = PWL − 11 dB = "
              "%2 dB(A) を基準レベルに使用 (ISO 9613-2 §7.1, 全空間放射・無指向)",
              "Reference level taken from the plant PWL %1 dB(A): "
              "Lp(1 m) = PWL − 11 dB = %2 dB(A) (ISO 9613-2 §7.1, free-field "
              "omnidirectional radiation)");
    I18n::reg("onz_area", "地域類型", "Area category");
    I18n::reg("onz_area_aa", "AA (療養施設等が集合し特に静穏を要する)",
              "AA (needs special quiet: hospitals, welfare facilities)");
    I18n::reg("onz_area_a", "A (専ら住居)", "A (exclusively residential)");
    I18n::reg("onz_area_b", "B (主として住居)", "B (mainly residential)");
    I18n::reg("onz_area_c", "C (住居 + 商業・工業)",
              "C (residential mixed with commerce / industry)");
    I18n::reg("onz_area_road_a", "道路に面する A 地域 (2車線以上)",
              "Facing a road: area A with 2+ lanes");
    I18n::reg("onz_area_road_bc",
              "道路に面する B 地域 (2車線以上) / C 地域 (車線あり)",
              "Facing a road: area B with 2+ lanes / area C with lanes");
    I18n::reg("onz_area_road_trunk", "幹線交通を担う道路に近接する空間",
              "Adjacent to a trunk road");
    I18n::reg("onz_period", "時間帯", "Period");
    I18n::reg("onz_day", "昼間 (6:00-22:00)", "Daytime (06:00-22:00)");
    I18n::reg("onz_night", "夜間 (22:00-6:00)", "Night (22:00-06:00)");
    I18n::reg("onz_pred_result",
              "受音点 %1 m / 高さ %2 m: 予測 %3 dB  "
              "(A_div = %4 dB, A_bar = %5 dB)",
              "Receiver at %1 m, height %2 m: predicted %3 dB "
              "(A_div = %4 dB, A_bar = %5 dB)");
    I18n::reg("onz_pred_bad",
              "未計算 — 基準レベル・基準距離・受音点距離を正の数で"
              "入力してください",
              "Not computed — enter positive values for the reference level, "
              "reference distance and receiver distance");
    I18n::reg("onz_judge_ok", "基準値 %1 dB に適合 (%2 dB 下回る)",
              "Meets the %1 dB limit (%2 dB below)");
    I18n::reg("onz_judge_ng", "基準値 %1 dB を %2 dB 超過",
              "Exceeds the %1 dB limit by %2 dB");
    I18n::reg("onz_std_src",
              "基準値の出所: 騒音に係る環境基準 (平成10年9月30日 環境庁告示"
              "第64号) — %1 / %2。評価量は等価騒音レベル LAeq。",
              "Limit values: Japanese environmental quality standards for noise "
              "(Environment Agency Notification No. 64, 30 Sep 1998) — %1 / %2. "
              "The rating quantity is the equivalent level LAeq.");
    I18n::reg("onz_pred_note",
              "▸ 幾何拡散と回折減衰だけの断面計算です (空気吸収・地面効果・"
              "反射・気象は未考慮)。予測値の評価量は基準レベルに入力した量と"
              "同じで、LAeq を入力したときだけ環境基準と直接比較できます。"
              "交通量・車速から音響パワーを求める発生源モデル (ASJ RTN-Model / "
              "CNOSSOS-EU) は未実装のため、基準レベルは実測値・カタログ値から"
              "与えてください。",
              "▸ A cross-section calculation with divergence and diffraction "
              "only (no air absorption, ground effect, reflections or "
              "meteorology). The predicted quantity is whatever you entered as "
              "the reference level, so it can be compared with the limit "
              "directly only when that is an LAeq. Emission models that derive "
              "sound power from traffic volume and speed (ASJ RTN-Model, "
              "CNOSSOS-EU) are not implemented, so the reference level must come "
              "from measurements or product data.");
    I18n::reg("onz_pred_line_note",
              "▸ 線音源を選んだ場合も回折減衰は点音源の式で評価しています "
              "(ISO 9613-2 は線音源を等価点音源に分割して扱う) — 近似です。",
              "▸ Even for a line source the diffraction loss is evaluated with "
              "the point-source formula (ISO 9613-2 splits a line source into "
              "equivalent point sources) — this is an approximation.");
    // 断面図
    I18n::reg("onz_cv_src", "音源", "Source");
    I18n::reg("onz_cv_bar", "壁", "Wall");
    I18n::reg("onz_cv_recv", "受音点", "Receiver");
    I18n::reg("onz_cv_none", "未計算", "Not computed");
    I18n::reg("onz_cv_axis", "音源からの距離 [m]", "Distance from source [m]");
    I18n::reg("onz_assess_btn", "📄 環境アセス報告書",
              "📄 Environmental assessment report");
    return true;
}();

// ── 小物ヘルパー (mock の badge / muted / q-num 相当) ───────────────────────
const char kAcc[] = "#0078D4";      // badge acc
const char kOk[]  = "#2E8B57";      // badge ok
const char kNg[]  = "#C0392B";      // badge ng (基準超過)

// 音速 [m/s] : 20 ℃ の乾燥空気 (ISO 9613-1:1993 の c = 331.3·√(1+t/273.15))
const double kSoundSpeed = 343.2;

// 回折減衰を帯域別に出すオクターブ中心周波数
const int kNumOctaves = 7;
const double kOctaveHz[kNumOctaves] = { 63, 125, 250, 500, 1000, 2000, 4000 };

void setBadgeColor(QLabel *l, const char *color)
{
    if (!color) {
        l->setStyleSheet("color:palette(mid); font-weight:600;");
        return;
    }
    l->setStyleSheet(QString("color:%1; border:1px solid %1; border-radius:3px;"
                             " padding:1px 6px; font-weight:600;").arg(color));
}

QLabel *makeBadge(const QString &text, const char *color, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setWordWrap(true);
    setBadgeColor(l, color);
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

// <Seg> 相当 (少数選択肢の排他セグメント) — QComboBox で再現
QComboBox *makeSeg(const QStringList &items, int current, QWidget *parent)
{
    auto *c = new QComboBox(parent);
    c->addItems(items);
    c->setCurrentIndex(current);
    return c;
}

QHBoxLayout *unitRow(QWidget *w, const QString &unit, QWidget *parent)
{
    auto *h = new QHBoxLayout();
    h->addWidget(w);
    h->addWidget(new QLabel(unit, parent));
    h->addStretch(1);
    return h;
}

// mock の <Row> 相当: チェックボックスを横並びに
QHBoxLayout *checkRow(const QVector<QCheckBox *> &boxes)
{
    auto *h = new QHBoxLayout();
    h->setSpacing(8);
    for (auto *b : boxes)
        h->addWidget(b);
    h->addStretch(1);
    return h;
}

// 等レベル線に使う色 (高レベルほど赤)。値は下の kContourLevels と対応。
const char *const kContourColor[5] = {
    "#D32F2F", "#F57C00", "#FBC02D", "#7CB342", "#42A5F5"
};
const double kContourLevels[5] = { 70, 65, 60, 55, 50 };
} // namespace

// ── NoiseProfileView ────────────────────────────────────────────────────────
// モックの固定 SVG (等高線楕円 + 「予測 52 dB(A)」) を、計算結果から描く
// 鉛直断面図へ置き換えたもの。データが無いときは「未計算」と描く。
NoiseProfileView::NoiseProfileView(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(340, 160);
    setMaximumWidth(420);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void NoiseProfileView::setData(const NoiseProfileData &d)
{
    m_data = d;
    update();
}

void NoiseProfileView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // 論理座標 340×160 を widget に等倍フィット (モックと同じ縦横比)
    const double s = qMin(width() / 340.0, height() / 160.0);
    p.translate((width() - 340.0 * s) / 2.0, (height() - 160.0 * s) / 2.0);
    p.scale(s, s);

    p.fillRect(QRectF(0, 0, 340, 160), palette().base());
    p.setPen(QPen(palette().mid().color(), 1));
    p.drawRect(QRectF(0.5, 0.5, 339, 159));

    QFont f8 = font(); f8.setPixelSize(8);
    QFont f7 = font(); f7.setPixelSize(7);
    const QColor muted = palette().mid().color();

    if (!m_data.valid || m_data.maxDistM <= 0.0) {
        p.setPen(muted);
        p.setFont(f8);
        p.drawText(QRectF(0, 0, 340, 160), Qt::AlignCenter,
                   I18n::tr("onz_cv_none"));
        return;
    }

    // 座標変換: 距離 0..maxDist → x 26..320、高さ 0..maxH → y 128..30
    const double groundY = 128.0;
    const double topY    = 30.0;
    const double x0 = 26.0, x1 = 320.0;
    double maxH = qMax(m_data.srcHeightM, m_data.recvHeightM);
    if (m_data.barrier) maxH = qMax(maxH, m_data.barHeightM);
    if (maxH <= 0.0) maxH = 1.0;
    maxH *= 1.25;
    const auto X = [&](double d) {
        return x0 + (x1 - x0) * qBound(0.0, d / m_data.maxDistM, 1.0);
    };
    const auto Y = [&](double h) {
        return groundY - (groundY - topY) * qBound(0.0, h / maxH, 1.0);
    };

    // 地面
    p.setPen(QPen(muted, 1.2));
    p.drawLine(QPointF(x0 - 6, groundY), QPointF(x1 + 12, groundY));

    // 等レベル線 (計算で求めた距離に破線を立てる)
    p.setFont(f7);
    for (const NoiseContourMark &c : m_data.contours) {
        const double lx = X(c.distM);
        QPen pen(QColor(c.color));
        pen.setWidthF(1.2);
        pen.setStyle(Qt::CustomDashLine);
        pen.setDashPattern({ 3, 3 });
        p.setPen(pen);
        p.drawLine(QPointF(lx, topY - 12), QPointF(lx, groundY));
        p.setPen(QColor(c.color));
        p.drawText(QPointF(lx + 2, topY - 14),
                   QString::number(c.levelDb, 'f', 0) + " dB");
    }

    // 音源
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#666666"));
    p.drawEllipse(QPointF(X(0.0), Y(m_data.srcHeightM)), 3.5, 3.5);
    p.setPen(muted);
    p.setFont(f7);
    p.drawText(QPointF(X(0.0) - 12, Y(m_data.srcHeightM) - 6),
               I18n::tr("onz_cv_src"));

    // 防音壁
    if (m_data.barrier && m_data.barHeightM > 0.0
        && m_data.barDistM > 0.0 && m_data.barDistM < m_data.maxDistM) {
        const double bx = X(m_data.barDistM);
        QPen barPen{ QColor(kAcc) };
        barPen.setWidthF(3.0);
        p.setPen(barPen);
        p.drawLine(QPointF(bx, groundY), QPointF(bx, Y(m_data.barHeightM)));
        p.setPen(QColor(kAcc));
        p.drawText(QPointF(bx + 3, Y(m_data.barHeightM) - 3),
                   I18n::tr("onz_cv_bar") + " "
                       + QString::number(m_data.barHeightM, 'f', 1) + "m");
    }

    // 見通し線 / 回折経路 (音源 → 壁頂部 → 受音点)
    if (m_data.recvDistM > 0.0 && m_data.recvDistM <= m_data.maxDistM) {
        QPen ray(muted);
        ray.setWidthF(0.8);
        ray.setStyle(Qt::DotLine);
        p.setPen(ray);
        if (m_data.shadow && m_data.barrier) {
            p.drawLine(QPointF(X(0.0), Y(m_data.srcHeightM)),
                       QPointF(X(m_data.barDistM), Y(m_data.barHeightM)));
            p.drawLine(QPointF(X(m_data.barDistM), Y(m_data.barHeightM)),
                       QPointF(X(m_data.recvDistM), Y(m_data.recvHeightM)));
        } else {
            p.drawLine(QPointF(X(0.0), Y(m_data.srcHeightM)),
                       QPointF(X(m_data.recvDistM), Y(m_data.recvHeightM)));
        }

        // 受音点 (住宅)
        const double rx = X(m_data.recvDistM);
        const double ry = Y(m_data.recvHeightM);
        p.setPen(Qt::NoPen);
        p.setBrush(muted);
        p.drawRect(QRectF(rx - 7, groundY - 12, 14, 12));
        p.setBrush(m_data.limitValid
                       ? QColor(m_data.pass ? "#2E8B57" : "#C0392B")
                       : muted);
        p.drawEllipse(QPointF(rx, ry), 3.0, 3.0);
        p.setPen(muted);
        p.setFont(f8);
        p.drawText(QPointF(rx - 24, groundY + 12),
                   I18n::tr("onz_cv_recv") + " "
                       + QString::number(m_data.recvLevelDb, 'f', 1) + " dB");
    }

    // 距離軸
    p.setPen(muted);
    p.setFont(f7);
    p.drawText(QPointF(x0 - 4, groundY + 22), "0");
    p.drawText(QPointF(x1 - 14, groundY + 22),
               QString::number(m_data.maxDistM, 'f', 0));
    p.drawText(QPointF(120, 152), I18n::tr("onz_cv_axis"));
}

// ── OutdoorNoiseTab ─────────────────────────────────────────────────────────
OutdoorNoiseTab::OutdoorNoiseTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // ── 屋外騒音伝搬 (概要 + 音源種別) ──────────────────────────────────────
    auto *sm = new SectionBox(I18n::tr("onz_main_section"), body);
    sm->vbox()->addWidget(makeHint(I18n::tr("onz_main_hint"), sm));
    m_srcType = makeSeg({ I18n::tr("onz_sc_road"), I18n::tr("onz_sc_rail"),
                          I18n::tr("onz_sc_industry"), I18n::tr("onz_sc_wind"),
                          I18n::tr("onz_sc_aircraft") }, 0, sm);
    sm->form()->addRow(I18n::tr("onz_src_type"), m_srcType);
    v->addWidget(sm);

    // ── 音源モデル (種別ごとに切替) ─────────────────────────────────────────
    auto *ss = new SectionBox(I18n::tr("onz_src_section"), body);
    m_srcStack = new QStackedWidget(ss);
    m_srcStack->addWidget(buildRoadPage());       // 0 road
    m_srcStack->addWidget(buildRailPage());       // 1 rail
    m_srcStack->addWidget(buildIndustryPage());   // 2 industry
    m_srcStack->addWidget(buildWindPage());       // 3 wind
    m_srcStack->addWidget(buildAircraftPage());   // 4 aircraft
    ss->vbox()->addWidget(m_srcStack);
    // 計算に使われるのは工場・設備の PWL だけ (発生源モデルは未実装 — 規則 5)
    ss->vbox()->addWidget(makeHint(I18n::tr("onz_src_note"), ss));
    v->addWidget(ss);

    // ── 伝搬経路 (ISO 9613-2) ───────────────────────────────────────────────
    auto *sp = new SectionBox(I18n::tr("onz_prop_section"), body);
    m_aDiv  = makeCheck(I18n::tr("onz_a_div"),  true,  sp);
    m_aAtm  = makeCheck(I18n::tr("onz_a_atm"),  true,  sp);
    m_aGr   = makeCheck(I18n::tr("onz_a_gr"),   true,  sp);
    m_aBar  = makeCheck(I18n::tr("onz_a_bar"),  true,  sp);
    m_aMisc = makeCheck(I18n::tr("onz_a_misc"), false, sp);
    m_cMet  = makeCheck(I18n::tr("onz_c_met"),  false, sp);
    sp->vbox()->addWidget(m_aDiv);
    sp->vbox()->addWidget(m_aAtm);
    sp->vbox()->addWidget(m_aGr);
    sp->vbox()->addWidget(m_aBar);
    sp->vbox()->addWidget(m_aMisc);
    sp->vbox()->addWidget(m_cMet);
    m_recvHeight = numEdit("1.2", sp);
    sp->form()->addRow(I18n::tr("onz_recv_h"),
                       unitRow(m_recvHeight, I18n::tr("onz_recv_h_u"), sp));
    m_recvDist = numEdit("25", sp);
    sp->form()->addRow(I18n::tr("onz_recv_d"),
                       unitRow(m_recvDist, I18n::tr("onz_recv_d_u"), sp));
    // A_div と A_bar は実計算、それ以外のチェックは未実装 (絶対規則 5)
    sp->vbox()->addWidget(makeHint(I18n::tr("onz_prop_note"), sp));
    v->addWidget(sp);

    // ── 防音壁設計 (前川チャートによる実計算) ──────────────────────────────
    // 固定サンプルだった「ΔL = 12.4 dB @ 500Hz / N = 2.1」を、幾何からの
    // 実計算 (acoustics/core/EnvironmentalNoise) へ置き換えたもの。
    auto *sb = new SectionBox(I18n::tr("onz_bar_section"), body);
    m_srcHeight = numEdit("0.5", sb);
    sb->form()->addRow(I18n::tr("onz_src_h"), unitRow(m_srcHeight, "m", sb));
    m_barHeight = numEdit("3.0", sb);
    sb->form()->addRow(I18n::tr("onz_bar_h"), unitRow(m_barHeight, "m", sb));
    m_barPos = numEdit("5.0", sb);
    sb->form()->addRow(I18n::tr("onz_bar_pos"),
                       unitRow(m_barPos, I18n::tr("onz_bar_pos_u"), sb));
    m_barFreq = numEdit("500", sb);
    sb->form()->addRow(I18n::tr("onz_bar_freq"),
                       unitRow(m_barFreq, "Hz", sb));
    m_barTop = makeSeg({ I18n::tr("onz_top_straight"), I18n::tr("onz_top_y"),
                         I18n::tr("onz_top_branch"), I18n::tr("onz_top_absorb") },
                       0, sb);
    sb->form()->addRow(I18n::tr("onz_bar_top"), m_barTop);
    m_barDelta = makeBadge(QString(), kAcc, sb);
    auto *hb = new QHBoxLayout();
    hb->addWidget(m_barDelta);
    hb->addStretch(1);
    sb->vbox()->addLayout(hb);
    // 帯域別の ΔL (λ に依存するので周波数ごとに変わる)
    m_barBands = new QTableWidget(0, 3, sb);
    m_barBands->setHorizontalHeaderLabels({ I18n::tr("onz_h_freq"),
                                            I18n::tr("onz_h_n"),
                                            I18n::tr("onz_h_dl") });
    m_barBands->horizontalHeader()
        ->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_barBands->horizontalHeader()->setStretchLastSection(true);
    m_barBands->verticalHeader()->setVisible(false);
    m_barBands->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_barBands->setMinimumHeight(180);
    sb->vbox()->addWidget(m_barBands);
    sb->vbox()->addWidget(makeHint(I18n::tr("onz_bar_top_note"), sb));
    sb->vbox()->addWidget(makeHint(I18n::tr("onz_bar_src"), sb));
    v->addWidget(sb);

    // ── 騒音予測と環境基準の適合判定 ────────────────────────────────────────
    // 固定サンプルだった等高線 SVG・「予測 52 dB(A)」・「環境基準クリア」を、
    // 実計算 + 告示の基準値との比較へ置き換えたもの。
    auto *sc = new SectionBox(I18n::tr("onz_pred_section"), body);
    sc->vbox()->addWidget(makeHint(I18n::tr("onz_pred_hint"), sc));
    m_srcKind = new QLabel(sc);
    sc->form()->addRow(I18n::tr("onz_src_kind"), m_srcKind);
    m_refLevel = numEdit("75", sc);
    sc->form()->addRow(I18n::tr("onz_ref_level"),
                       unitRow(m_refLevel, "dB(A)", sc));
    m_refDist = numEdit("1.0", sc);
    sc->form()->addRow(I18n::tr("onz_ref_dist"), unitRow(m_refDist, "m", sc));
    m_pwlNote = makeHint(QString(), sc);
    sc->vbox()->addWidget(m_pwlNote);
    m_areaType = makeSeg({ I18n::tr("onz_area_aa"), I18n::tr("onz_area_a"),
                           I18n::tr("onz_area_b"), I18n::tr("onz_area_c"),
                           I18n::tr("onz_area_road_a"),
                           I18n::tr("onz_area_road_bc"),
                           I18n::tr("onz_area_road_trunk") }, 1, sc);
    sc->form()->addRow(I18n::tr("onz_area"), m_areaType);
    m_period = makeSeg({ I18n::tr("onz_day"), I18n::tr("onz_night") }, 0, sc);
    sc->form()->addRow(I18n::tr("onz_period"), m_period);
    m_profile = new NoiseProfileView(sc);
    sc->vbox()->addWidget(m_profile);
    m_predResult = makeHint(QString(), sc);
    sc->vbox()->addWidget(m_predResult);
    auto *hm = new QHBoxLayout();
    m_judge = makeBadge(QString(), kOk, sc);
    hm->addWidget(m_judge);
    auto *assessBtn = new QPushButton(I18n::tr("onz_assess_btn"), sc);
    tabhelp::markNotImplemented(assessBtn, I18n::tr(tabhelp::notimpl::kReport));   // 報告書出力は未配線
    hm->addWidget(assessBtn);
    hm->addStretch(1);
    sc->vbox()->addLayout(hm);
    m_stdSource = makeHint(QString(), sc);
    sc->vbox()->addWidget(m_stdSource);
    sc->vbox()->addWidget(makeHint(I18n::tr("onz_pred_note"), sc));
    sc->vbox()->addWidget(makeHint(I18n::tr("onz_pred_line_note"), sc));
    v->addWidget(sc);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    connect(m_srcType, &QComboBox::currentIndexChanged, this, [this](int i) {
        // ローカル state のみ (Project に対応フィールド無し → 永続化しない)
        if (m_updating) return;
        m_scenario = i;
        m_srcStack->setCurrentIndex(i);
        recompute();
    });

    // 計算に効く入力はすべて再計算へ配線する (閉形式なので同期実行でよい)
    for (QLineEdit *e : { m_recvHeight, m_recvDist, m_srcHeight, m_barHeight,
                          m_barPos, m_barFreq, m_refLevel, m_refDist,
                          m_plantPwl })
        connect(e, &QLineEdit::textChanged, this, &OutdoorNoiseTab::recompute);
    for (QCheckBox *c : { m_aDiv, m_aBar })
        connect(c, &QCheckBox::toggled, this, &OutdoorNoiseTab::recompute);
    for (QComboBox *c : { m_areaType, m_period })
        connect(c, &QComboBox::currentIndexChanged, this,
                &OutdoorNoiseTab::recompute);

    connect(project, &Project::loaded, this, &OutdoorNoiseTab::refresh);
    refresh();
}

void OutdoorNoiseTab::refresh()
{
    m_updating = true;
    m_srcType->setCurrentIndex(m_scenario);
    m_srcStack->setCurrentIndex(m_scenario);
    m_updating = false;
    recompute();
}

// ── 実計算 (幾何拡散 + 前川の回折減衰 + 環境基準の比較) ─────────────────────
// 式はすべて acoustics/core/EnvironmentalNoise 側にある (GUI に式を書かない)。
void OutdoorNoiseTab::recompute()
{
    // 道路・鉄道は線音源、工場・風車・航空機は点音源として扱う
    const bool lineSource = (m_scenario == 0 || m_scenario == 1);
    const bool industry   = (m_scenario == 2);
    m_srcKind->setText(I18n::tr(lineSource ? "onz_src_line"
                                           : "onz_src_point"));

    bool okRh = false, okRd = false, okSh = false, okBh = false, okBp = false,
         okBf = false, okRl = false, okRr = false;
    const double recvH = m_recvHeight->text().toDouble(&okRh);
    const double recvD = m_recvDist->text().toDouble(&okRd);
    const double srcH  = m_srcHeight->text().toDouble(&okSh);
    const double barH  = m_barHeight->text().toDouble(&okBh);
    const double barD  = m_barPos->text().toDouble(&okBp);
    const double barF  = m_barFreq->text().toDouble(&okBf);
    double refL        = m_refLevel->text().toDouble(&okRl);
    const double refD  = m_refDist->text().toDouble(&okRr);

    // 工場・設備は音源PWL から基準レベルを導ける (ISO 9613-2 §7.1)
    bool okPwl = false;
    const double pwl = m_plantPwl->text().toDouble(&okPwl);
    m_refLevel->setEnabled(!industry);
    m_refDist->setEnabled(!industry);
    if (industry && okPwl) {
        refL = env::pointSourceLevelAt1m(pwl);
        okRl = true;
        m_pwlNote->setText(I18n::tr("onz_pwl_derived")
                               .arg(QString::number(pwl, 'f', 1))
                               .arg(QString::number(refL, 'f', 1)));
        m_pwlNote->setVisible(true);
        QSignalBlocker b1(m_refLevel), b2(m_refDist);
        m_refLevel->setText(QString::number(refL, 'f', 1));
        m_refDist->setText("1.0");
    } else {
        m_pwlNote->setVisible(false);
    }
    const double refDist = industry ? 1.0 : refD;
    const bool okRefDist = industry ? true : (okRr && refD > 0);

    // ── 防音壁 (前川チャート) ───────────────────────────────────────────
    env::BarrierGeometry g;
    g.srcHeightM  = srcH;
    g.barDistM    = barD;
    g.barHeightM  = barH;
    g.recvDistM   = recvD;
    g.recvHeightM = recvH;
    const bool geomOk = okRh && okRd && okSh && okBh && okBp && okBf
                        && recvH > 0 && recvD > 0 && srcH > 0 && barH > 0
                        && barD > 0 && barD < recvD && barF > 0;
    env::BarrierResult bar;
    if (geomOk)
        bar = env::barrierDiffraction(g, barF, kSoundSpeed);

    m_barBands->setRowCount(0);
    if (!geomOk || !bar.valid) {
        m_barDelta->setText(I18n::tr("onz_bar_bad"));
    } else if (!bar.shadow) {
        m_barDelta->setText(I18n::tr("onz_bar_los"));
    } else {
        m_barDelta->setText(
            I18n::tr(bar.clamped ? "onz_bar_clamped" : "onz_bar_result")
                .arg(QString::number(bar.attenDb, 'f', 1))
                .arg(QString::number(bar.fresnelN, 'f', 2))
                .arg(QString::number(bar.pathDiffM, 'f', 3))
                .arg(QString::number(barF, 'f', 0))
                .arg(QString::number(bar.wavelengthM, 'f', 3)));
        // オクターブ帯域ごとの ΔL (経路差は同じ、λ だけが変わる)
        for (int i = 0; i < kNumOctaves; ++i) {
            const env::BarrierResult r =
                env::barrierDiffraction(g, kOctaveHz[i], kSoundSpeed);
            m_barBands->insertRow(i);
            m_barBands->setItem(i, 0,
                tabhelp::roItem(QString::number(kOctaveHz[i], 'f', 0)));
            m_barBands->setItem(i, 1,
                tabhelp::roItem(QString::number(r.fresnelN, 'f', 2)));
            m_barBands->setItem(i, 2,
                tabhelp::roItem(QString::number(r.attenDb, 'f', 1)));
        }
    }

    // ── 断面予測 ────────────────────────────────────────────────────────
    env::SiteModel model;
    model.refLevelDb        = refL;
    model.refDistM          = refDist;
    model.lineSource        = lineSource;
    model.srcHeightM        = srcH;
    model.divergenceEnabled = m_aDiv->isChecked();
    model.barrierEnabled    = m_aBar->isChecked() && geomOk;
    model.barDistM       = barD;
    model.barHeightM     = barH;
    model.evalFreqHz     = barF;
    model.soundSpeedMs   = kSoundSpeed;

    NoiseProfileData data;
    const bool inputOk = okRl && okRefDist && okRd && okRh
                         && recvD > 0 && recvH > 0;
    const env::PredictionResult pred =
        inputOk ? env::predictLevel(model, recvD, recvH)
                : env::PredictionResult();

    // 環境基準 (告示第64号) と比較する
    const env::EnvStandard std_ =
        env::environmentalStandardJp(m_areaType->currentIndex());
    const bool night = (m_period->currentIndex() == 1);
    const double limit = night ? std_.nightDb : std_.dayDb;
    m_stdSource->setText(
        I18n::tr("onz_std_src")
            .arg(m_areaType->currentText())
            .arg(I18n::tr(night ? "onz_night" : "onz_day")));

    if (!pred.valid) {
        m_predResult->setText(I18n::tr("onz_pred_bad"));
        m_judge->setText(I18n::tr("onz_cv_none"));
        setBadgeColor(m_judge, nullptr);
        m_profile->setData(data);
        return;
    }

    m_predResult->setText(I18n::tr("onz_pred_result")
                              .arg(QString::number(recvD, 'f', 1))
                              .arg(QString::number(recvH, 'f', 2))
                              .arg(QString::number(pred.levelDb, 'f', 1))
                              .arg(QString::number(pred.aDivDb, 'f', 1))
                              .arg(QString::number(pred.aBarDb, 'f', 1)));

    const bool pass = std_.valid && (pred.levelDb <= limit);
    if (std_.valid) {
        m_judge->setText(
            I18n::tr(pass ? "onz_judge_ok" : "onz_judge_ng")
                .arg(QString::number(limit, 'f', 0))
                .arg(QString::number(std::fabs(limit - pred.levelDb), 'f', 1)));
        setBadgeColor(m_judge, pass ? kOk : kNg);
    } else {
        m_judge->setText(I18n::tr("onz_cv_none"));
        setBadgeColor(m_judge, nullptr);
    }

    // ── 断面図のデータ ──────────────────────────────────────────────────
    data.valid       = true;
    data.maxDistM    = model.barrierEnabled
                           ? std::max(recvD * 1.3, barD * 2.0)
                           : recvD * 1.3;
    data.srcHeightM  = srcH;
    data.barrier     = model.barrierEnabled;
    data.barDistM    = barD;
    data.barHeightM  = barH;
    data.shadow      = pred.barrier.shadow;
    data.recvDistM   = recvD;
    data.recvHeightM = recvH;
    data.recvLevelDb = pred.levelDb;
    data.limitValid  = std_.valid;
    data.limitDb     = limit;
    data.pass        = pass;
    for (int i = 0; i < 5; ++i) {
        // その等レベル線が断面内のどの距離に来るかを二分探索で求める
        const double d = env::distanceForLevel(model, kContourLevels[i], recvH,
                                               0.5, data.maxDistM);
        if (d <= 0.0) continue;
        NoiseContourMark mk;
        mk.distM   = d;
        mk.levelDb = kContourLevels[i];
        mk.color   = QString::fromUtf8(kContourColor[i]);
        data.contours.push_back(mk);
    }
    m_profile->setData(data);
}

// ── 道路交通 / road ─────────────────────────────────────────────────────────
QWidget *OutdoorNoiseTab::buildRoadPage()
{
    auto *page = new QWidget;
    auto *fl = new QFormLayout(page);
    fl->setContentsMargins(0, 0, 0, 0);

    m_traffic = numEdit("24000", page, 100);
    fl->addRow(I18n::tr("onz_traffic"),
               unitRow(m_traffic, I18n::tr("onz_traffic_u"), page));
    m_heavyRatio = numEdit("15", page);
    fl->addRow(I18n::tr("onz_heavy"), unitRow(m_heavyRatio, "%", page));
    m_speed = numEdit("60", page);
    fl->addRow(I18n::tr("onz_speed"), unitRow(m_speed, "km/h", page));
    m_pavement = makeSeg({ I18n::tr("onz_pav_dense"), I18n::tr("onz_pav_drain"),
                           I18n::tr("onz_pav_cobble") }, 0, page);
    fl->addRow(I18n::tr("onz_pavement"), m_pavement);
    m_roadModel = makeSeg({ "ASJ RTN-Model 2018", "CNOSSOS-EU", "FHWA TNM" },
                          0, page);
    fl->addRow(I18n::tr("onz_model"), m_roadModel);
    return page;
}

// ── 鉄道 / rail ─────────────────────────────────────────────────────────────
QWidget *OutdoorNoiseTab::buildRailPage()
{
    auto *page = new QWidget;
    auto *fl = new QFormLayout(page);
    fl->setContentsMargins(0, 0, 0, 0);

    m_trainType = makeSeg({ I18n::tr("onz_train_shinkansen"),
                            I18n::tr("onz_train_express"),
                            I18n::tr("onz_train_commuter"),
                            I18n::tr("onz_train_freight") }, 0, page);
    fl->addRow(I18n::tr("onz_train_type"), m_trainType);
    m_trainCount = numEdit("220", page);
    fl->addRow(I18n::tr("onz_train_count"),
               unitRow(m_trainCount, I18n::tr("onz_train_count_u"), page));
    m_rolling   = makeCheck(I18n::tr("onz_rolling"),   true, page);
    m_structure = makeCheck(I18n::tr("onz_structure"), true, page);
    m_aero      = makeCheck(I18n::tr("onz_aero"),      true, page);
    fl->addRow(checkRow({ m_rolling, m_structure, m_aero }));
    return page;
}

// ── 工場・設備 / industry ───────────────────────────────────────────────────
QWidget *OutdoorNoiseTab::buildIndustryPage()
{
    auto *page = new QWidget;
    auto *fl = new QFormLayout(page);
    fl->setContentsMargins(0, 0, 0, 0);

    m_plantPwl = numEdit("105", page);
    fl->addRow(I18n::tr("onz_pwl"), unitRow(m_plantPwl, "dB(A)", page));
    m_operation = makeSeg({ I18n::tr("onz_op_day"), I18n::tr("onz_op_24h") },
                          0, page);
    fl->addRow(I18n::tr("onz_operation"), m_operation);
    m_buildingIns = makeCheck(I18n::tr("onz_building_ins"), true,  page);
    m_directivity = makeCheck(I18n::tr("onz_directivity"), false, page);
    fl->addRow(checkRow({ m_buildingIns, m_directivity }));
    return page;
}

// ── 風力発電 / wind ─────────────────────────────────────────────────────────
QWidget *OutdoorNoiseTab::buildWindPage()
{
    auto *page = new QWidget;
    auto *fl = new QFormLayout(page);
    fl->setContentsMargins(0, 0, 0, 0);

    m_turbine = makeSeg({ I18n::tr("onz_turb_3mw"), I18n::tr("onz_turb_2mw"),
                          I18n::tr("onz_turb_off5") }, 0, page);
    fl->addRow(I18n::tr("onz_turbine"), m_turbine);
    m_turbineCount = numEdit("8", page);
    fl->addRow(I18n::tr("onz_turb_count"), m_turbineCount);
    m_swish    = makeCheck(I18n::tr("onz_swish"),   true, page);
    m_lowFreq  = makeCheck(I18n::tr("onz_lowfreq"), true, page);
    fl->addRow(checkRow({ m_swish, m_lowFreq }));
    return page;
}

// ── 航空機 / aircraft ───────────────────────────────────────────────────────
QWidget *OutdoorNoiseTab::buildAircraftPage()
{
    auto *page = new QWidget;
    auto *fl = new QFormLayout(page);
    fl->setContentsMargins(0, 0, 0, 0);

    m_acType = makeSeg({ I18n::tr("onz_ac_b787"), I18n::tr("onz_ac_a320"),
                         I18n::tr("onz_ac_heli"), I18n::tr("onz_ac_drone") },
                       0, page);
    fl->addRow(I18n::tr("onz_ac_type"), m_acType);
    m_flights = numEdit("180", page);
    fl->addRow(I18n::tr("onz_flights"),
               unitRow(m_flights, I18n::tr("onz_flights_u"), page));
    m_acMetric = makeSeg({ "Lden", "WECPNL" }, 0, page);
    fl->addRow(I18n::tr("onz_ac_metric"), m_acMetric);
    return page;
}
