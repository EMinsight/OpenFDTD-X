// OutdoorNoiseTab.cpp
#include "OutdoorNoiseTab.h"
#include "../core/Project.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QFont>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QVector>

using namespace ofd;

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
              "SoundPLAN / CadnaA 相当の機能。",
              "Predicts environmental noise from roads, railways, plants and wind "
              "turbines with divergence + ground + barrier + meteorology.\n"
              "Equivalent to SoundPLAN / CadnaA.");
    I18n::reg("onz_src_type", "騒音源種別", "Source type");
    I18n::reg("onz_sc_road", "道路交通", "Road traffic");
    I18n::reg("onz_sc_rail", "鉄道", "Railway");
    I18n::reg("onz_sc_industry", "工場・設備", "Industry / plant");
    I18n::reg("onz_sc_wind", "風力発電", "Wind turbine");
    I18n::reg("onz_sc_aircraft", "航空機", "Aircraft");
    // 音源モデル
    I18n::reg("onz_src_section", "音源モデル", "Source model");
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
    // 防音壁
    I18n::reg("onz_bar_section", "防音壁設計", "Barrier design");
    I18n::reg("onz_bar_h", "高さ", "Height");
    I18n::reg("onz_bar_pos", "位置", "Position");
    I18n::reg("onz_bar_pos_u", "m (音源から)", "m (from source)");
    I18n::reg("onz_bar_top", "頂部形状", "Top shape");
    I18n::reg("onz_top_straight", "直壁", "Straight");
    I18n::reg("onz_top_y", "Y型 (+2dB)", "Y-shape (+2 dB)");
    I18n::reg("onz_top_branch", "枝付き", "Branched");
    I18n::reg("onz_top_absorb", "吸音型", "Absorptive");
    I18n::reg("onz_bar_delta", "回折減衰 ΔL = 12.4 dB @ 500Hz",
              "Diffraction loss ΔL = 12.4 dB @ 500 Hz");
    I18n::reg("onz_bar_note", "(Maekawa チャート / Fresnel N=2.1)",
              "(Maekawa chart / Fresnel N=2.1)");
    // 等高線マップ
    I18n::reg("onz_map_section", "等高線マップ", "Noise contour");
    I18n::reg("onz_map_road", "道路", "Road");
    I18n::reg("onz_map_barrier", "防音壁 3m", "Barrier 3 m");
    I18n::reg("onz_map_houses", "住宅列 — 予測 52 dB(A) 昼間",
              "Housing row — predicted 52 dB(A), daytime");
    I18n::reg("onz_std_ok", "環境基準 (昼55/夜45) クリア",
              "Meets environmental limits (day 55 / night 45)");
    I18n::reg("onz_assess_btn", "📄 環境アセス報告書",
              "📄 Environmental assessment report");
    return true;
}();

// ── 小物ヘルパー (mock の badge / muted / q-num 相当) ───────────────────────
const char kAcc[] = "#0078D4";      // badge acc
const char kOk[]  = "#2E8B57";      // badge ok

QLabel *makeBadge(const QString &text, const char *color, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setStyleSheet(QString("color:%1; border:1px solid %1; border-radius:3px;"
                             " padding:1px 6px; font-weight:600;").arg(color));
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

// 等高線データ (モックの配列をそのまま): ry [viewBox px], 色, ラベル
struct Contour { int ry; const char *color; const char *label; };
const Contour kContours[5] = {
    {  22, "#D32F2F", "70" },
    {  44, "#F57C00", "65" },
    {  72, "#FBC02D", "60" },
    { 108, "#7CB342", "55" },
    { 150, "#42A5F5", "50" },
};
const int kHouseX[5] = { 40, 110, 180, 250, 310 };
} // namespace

// ── NoiseContourView ────────────────────────────────────────────────────────
NoiseContourView::NoiseContourView(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(340, 160);
    setMaximumWidth(380);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void NoiseContourView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // viewBox "0 0 340 160" を widget に等倍フィット
    const double s = qMin(width() / 340.0, height() / 160.0);
    p.translate((width() - 340.0 * s) / 2.0, (height() - 160.0 * s) / 2.0);
    p.scale(s, s);

    // 背景 (bg-input) + 枠 (border-soft)
    p.fillRect(QRectF(0, 0, 340, 160), palette().base());
    p.setPen(QPen(palette().mid().color(), 1));
    p.drawRect(QRectF(0.5, 0.5, 339, 159));

    QFont f8 = font(); f8.setPixelSize(8);
    QFont f7 = font(); f7.setPixelSize(7);

    // 道路帯
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#666666"));
    p.drawRect(QRectF(0, 70, 340, 14));
    p.setPen(QColor("#FFFFFF"));
    p.setFont(f8);
    p.drawText(QPointF(6, 80), I18n::tr("onz_map_road"));

    // 等高線 (cx=170, cy=77, rx=170) — 破線楕円 + dB ラベル
    p.setBrush(Qt::NoBrush);
    p.setFont(f7);
    for (const auto &c : kContours) {
        QPen pen(QColor(c.color));
        pen.setWidthF(1.5);
        pen.setStyle(Qt::CustomDashLine);
        pen.setDashPattern({ 4, 2 });
        p.setPen(pen);
        p.drawEllipse(QRectF(0, 77.0 - c.ry, 340, 2.0 * c.ry));
        p.setPen(QColor(c.color));
        p.drawText(QPointF(172, 77 + c.ry - 3),
                   QString::fromUtf8(c.label) + "dB");
    }

    // 防音壁
    QPen barPen{ QColor(kAcc) };
    barPen.setWidthF(2.5);
    p.setPen(barPen);
    p.drawLine(QPointF(0, 95), QPointF(340, 95));
    p.setPen(QColor(kAcc));
    p.drawText(QPointF(290, 104), I18n::tr("onz_map_barrier"));

    // 住宅列
    const QColor muted = palette().mid().color();
    p.setPen(Qt::NoPen);
    p.setBrush(muted);
    for (int x : kHouseX)
        p.drawRect(QRectF(x, 120, 16, 12));
    p.setPen(muted);
    p.setFont(f8);
    p.drawText(QPointF(6, 152), I18n::tr("onz_map_houses"));
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
    v->addWidget(sp);

    // ── 防音壁設計 ──────────────────────────────────────────────────────────
    auto *sb = new SectionBox(I18n::tr("onz_bar_section"), body);
    m_barHeight = numEdit("3.0", sb);
    sb->form()->addRow(I18n::tr("onz_bar_h"), unitRow(m_barHeight, "m", sb));
    m_barPos = numEdit("5.0", sb);
    sb->form()->addRow(I18n::tr("onz_bar_pos"),
                       unitRow(m_barPos, I18n::tr("onz_bar_pos_u"), sb));
    m_barTop = makeSeg({ I18n::tr("onz_top_straight"), I18n::tr("onz_top_y"),
                         I18n::tr("onz_top_branch"), I18n::tr("onz_top_absorb") },
                       0, sb);
    sb->form()->addRow(I18n::tr("onz_bar_top"), m_barTop);
    auto *hb = new QHBoxLayout();
    m_barDelta = makeBadge(I18n::tr("onz_bar_delta"), kAcc, sb);
    hb->addWidget(m_barDelta);
    hb->addWidget(new QLabel(I18n::tr("onz_bar_note"), sb));
    hb->addStretch(1);
    sb->vbox()->addLayout(hb);
    v->addWidget(sb);

    // ── 等高線マップ ────────────────────────────────────────────────────────
    auto *sc = new SectionBox(I18n::tr("onz_map_section"), body);
    m_contour = new NoiseContourView(sc);
    sc->vbox()->addWidget(m_contour);
    auto *hm = new QHBoxLayout();
    hm->addWidget(makeBadge(I18n::tr("onz_std_ok"), kOk, sc));
    hm->addWidget(new QPushButton(I18n::tr("onz_assess_btn"), sc));
    hm->addStretch(1);
    sc->vbox()->addLayout(hm);
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
    });
    connect(project, &Project::loaded, this, &OutdoorNoiseTab::refresh);
    refresh();
}

void OutdoorNoiseTab::refresh()
{
    m_updating = true;
    m_srcType->setCurrentIndex(m_scenario);
    m_srcStack->setCurrentIndex(m_scenario);
    m_updating = false;
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
