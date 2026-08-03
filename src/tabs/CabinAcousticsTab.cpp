// CabinAcousticsTab.cpp
#include "CabinAcousticsTab.h"
#include "../core/Project.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"
#include "TabHelpers.h"

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QFileDialog>
#include <QFont>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QVector>

using namespace ofd;

// ── タブ固有語彙 (cab_) — file-local 登録 ───────────────────────────────────
namespace {
const bool s_i18n = [] {
    using ofd::I18n;
    // タブ名 (MainWindow 配線用)
    I18n::reg("t_cabin", "🚗 車内音響", "🚗 Cabin NVH");
    // 概要
    I18n::reg("cab_main_section", "車内・機内音響 (NVH)", "Cabin NVH");
    I18n::reg("cab_main_hint",
              "自動車・航空機・鉄道の車内騒音を、構造振動+音響のFEM/FDTD/SEA連成で"
              "予測する構想 (連成解析は未実装 — この画面は設計モック)。",
              "Concept for predicting interior noise of cars, aircraft and "
              "trains with coupled structural-acoustic FEM / FDTD / SEA "
              "(coupled analysis not implemented — this page is a design mock).");
    I18n::reg("cab_target", "対象", "Target");
    I18n::reg("cab_veh_car", "乗用車", "Passenger car");
    I18n::reg("cab_veh_ev", "EV", "EV");
    I18n::reg("cab_veh_train", "鉄道", "Train");
    I18n::reg("cab_veh_aircraft", "航空機", "Aircraft");
    // 騒音源
    I18n::reg("cab_src_section", "騒音源", "Noise sources");
    I18n::reg("cab_car_engine", "エンジン透過音 (100-500Hz)",
              "Engine airborne noise (100-500 Hz)");
    I18n::reg("cab_car_road", "ロードノイズ (30-300Hz)",
              "Road noise (30-300 Hz)");
    I18n::reg("cab_car_wind", "風切り音 (>1kHz)", "Wind noise (>1 kHz)");
    I18n::reg("cab_car_boom", "こもり音 (ブーミング 40-80Hz)",
              "Booming (40-80 Hz)");
    I18n::reg("cab_ev_motor", "モーター高周波音 (2-8kHz)",
              "Motor high-frequency tone (2-8 kHz)");
    I18n::reg("cab_ev_inverter", "インバータスイッチング音",
              "Inverter switching noise");
    I18n::reg("cab_ev_road", "ロードノイズ (相対的に顕在化)",
              "Road noise (relatively dominant)");
    I18n::reg("cab_ev_gear", "ギヤワイン", "Gear whine");
    I18n::reg("cab_ev_note",
              "▸ EVはエンジンマスキングが無いため高周波トーンの対策が主戦場",
              "▸ Without engine masking, EVs fight high-frequency tones");
    I18n::reg("cab_tr_rolling", "転動音", "Rolling noise");
    I18n::reg("cab_tr_tunnel", "トンネル内圧力変動",
              "Tunnel pressure fluctuation");
    I18n::reg("cab_tr_hvac", "空調", "HVAC");
    I18n::reg("cab_ac_tbl", "境界層騒音 (TBL)",
              "Turbulent boundary layer (TBL) noise");
    I18n::reg("cab_ac_engine", "エンジン (ファン/ジェット)",
              "Engine (fan / jet)");
    I18n::reg("cab_ac_press", "与圧系統", "Pressurization system");
    // 解析手法
    I18n::reg("cab_method_section", "解析手法 (帯域別)", "Method by frequency");
    I18n::reg("cab_h_band", "帯域", "Band");
    I18n::reg("cab_h_method", "手法", "Method");
    I18n::reg("cab_h_use", "用途", "Use");
    I18n::reg("cab_band_low", "〜200 Hz", "up to 200 Hz");
    I18n::reg("cab_band_mid", "200-1000 Hz", "200-1000 Hz");
    I18n::reg("cab_band_high", "1 kHz〜", "1 kHz and above");
    I18n::reg("cab_m_fem", "FEM/FDTD", "FEM / FDTD");
    I18n::reg("cab_m_fesea", "ハイブリッド FE-SEA", "Hybrid FE-SEA");
    I18n::reg("cab_m_sea", "SEA (統計的)", "SEA (statistical)");
    I18n::reg("cab_u_low", "こもり音・パネル共振・音響モード",
              "Booming, panel resonance, acoustic modes");
    I18n::reg("cab_u_mid", "中間周波数帯", "Mid-frequency range");
    I18n::reg("cab_u_high", "風切り音・高周波透過",
              "Wind noise, high-frequency transmission");
    // 車室モデル
    I18n::reg("cab_model_section", "車室モデル", "Cabin model");
    I18n::reg("cab_cad", "3Dモデル", "3D model");
    I18n::reg("cab_browse", "📁 参照…", "📁 Browse…");
    I18n::reg("cab_modal_row", "音響モード解析", "Acoustic modal analysis");
    I18n::reg("cab_modal", "〜200Hzの固有モード抽出",
              "Extract eigenmodes up to 200 Hz");
    I18n::reg("cab_modal_note",
              "1次: 42Hz (前後) / 2次: 68Hz (左右) / 3次: 87Hz (上下) — ブーミング対策周波数",
              "1st: 42 Hz (fore-aft) / 2nd: 68 Hz (lateral) / 3rd: 87 Hz "
              "(vertical) — booming countermeasure frequencies");
    I18n::reg("cab_absorb", "吸音内装", "Interior absorption");
    I18n::reg("cab_abs_roof", "ルーフライナー", "Roof liner");
    I18n::reg("cab_abs_carpet", "カーペット", "Carpet");
    I18n::reg("cab_abs_door", "ドアトリム", "Door trim");
    I18n::reg("cab_abs_seat", "シート", "Seats");
    // 評価
    I18n::reg("cab_metrics_section", "評価", "Metrics");
    I18n::reg("cab_m_earspl", "運転席耳位置 SPL (dB(A))",
              "Driver ear-position SPL (dB(A))");
    I18n::reg("cab_m_ai", "AI (会話明瞭度指数)",
              "AI (articulation index)");
    I18n::reg("cab_m_loud", "Loudness (ISO 532-1)", "Loudness (ISO 532-1)");
    I18n::reg("cab_m_sharp", "Sharpness / Roughness (音質評価)",
              "Sharpness / Roughness (sound quality)");
    I18n::reg("cab_result", "60km/h 巡航: 62.4 dB(A)",
              "60 km/h cruise: 62.4 dB(A)");
    I18n::reg("cab_seg_avg", "セグメント平均 -1.8dB",
              "-1.8 dB vs segment average");
    I18n::reg("cab_aural_btn", "🎧 車内音の可聴化", "🎧 Auralize cabin sound");
    I18n::reg("cab_tpa_btn", "📊 寄与度分析 (TPA)",
              "📊 Contribution analysis (TPA)");
    // 対策検討
    I18n::reg("cab_meas_section", "対策検討", "Countermeasures");
    I18n::reg("cab_h_meas", "対策", "Countermeasure");
    I18n::reg("cab_h_effect", "効果", "Effect");
    I18n::reg("cab_h_weight", "重量", "Weight");
    I18n::reg("cab_h_cost", "コスト", "Cost");
    I18n::reg("cab_meas_dash", "ダッシュインシュレータ増厚",
              "Thicker dash insulator");
    I18n::reg("cab_meas_damp", "制振材 (フロア)", "Damping material (floor)");
    I18n::reg("cab_meas_glass", "遮音ガラス (合わせ)",
              "Acoustic laminated glass");
    I18n::reg("cab_meas_anc", "ANC (アクティブ制御)", "ANC (active control)");
    I18n::reg("cab_eff_glass", "-1.8 dB (風切)", "-1.8 dB (wind)");
    I18n::reg("cab_eff_anc", "-8 dB (@40-200Hz)", "-8 dB (@40-200 Hz)");
    I18n::reg("cab_cost_low", "低", "Low");
    I18n::reg("cab_cost_mid", "中", "Medium");
    I18n::reg("cab_cost_high", "高", "High");
    return true;
}();

// ── 小物ヘルパー (mock の badge / muted / q-table 相当) ─────────────────────
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

// <Seg> 相当 (少数選択肢の排他セグメント) — QComboBox で再現
QComboBox *makeSeg(const QStringList &items, int current, QWidget *parent)
{
    auto *c = new QComboBox(parent);
    c->addItems(items);
    c->setCurrentIndex(current);
    return c;
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

QTableWidgetItem *textItem(const QString &s) { return new QTableWidgetItem(s); }

QTableWidgetItem *numItem(const QString &s)
{
    auto *it = new QTableWidgetItem(s);
    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return it;
}

QTableWidgetItem *badgeItem(const QString &s, const char *color)
{
    auto *it = new QTableWidgetItem(s);
    if (color) it->setForeground(QColor(color));
    QFont f = it->font();
    f.setBold(true);
    it->setFont(f);
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

// ── CabinAcousticsTab ───────────────────────────────────────────────────────
CabinAcousticsTab::CabinAcousticsTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // ── 車内・機内音響 (概要 + 対象) ────────────────────────────────────────
    auto *sm = new SectionBox(I18n::tr("cab_main_section"), body);
    sm->vbox()->addWidget(makeHint(I18n::tr("cab_main_hint"), sm));
    m_vehicle = makeSeg({ I18n::tr("cab_veh_car"), I18n::tr("cab_veh_ev"),
                          I18n::tr("cab_veh_train"),
                          I18n::tr("cab_veh_aircraft") }, 0, sm);
    sm->form()->addRow(I18n::tr("cab_target"), m_vehicle);
    v->addWidget(sm);

    // ── 騒音源 (対象ごとに切替) ─────────────────────────────────────────────
    auto *ss = new SectionBox(I18n::tr("cab_src_section"), body);
    m_srcStack = new QStackedWidget(ss);
    m_srcStack->addWidget(buildCarSources());       // 0 car
    m_srcStack->addWidget(buildEvSources());        // 1 ev
    m_srcStack->addWidget(buildTrainSources());     // 2 train
    m_srcStack->addWidget(buildAircraftSources());  // 3 aircraft
    ss->vbox()->addWidget(m_srcStack);
    // 騒音源チェックはローカル状態のみ (どこにも読まれない)
    ss->vbox()->addWidget(tabhelp::unwiredNote(ss));
    v->addWidget(ss);

    // ── 解析手法 (帯域別) ───────────────────────────────────────────────────
    auto *sf = new SectionBox(I18n::tr("cab_method_section"), body);
    m_methodTable = makeTable({ I18n::tr("cab_h_band"), I18n::tr("cab_h_method"),
                                I18n::tr("cab_h_use") }, 3, sf, 120);
    struct Meth { const char *band; const char *method; const char *use;
                  const char *color; };
    static const Meth kMeth[3] = {
        { "cab_band_low",  "cab_m_fem",   "cab_u_low",  kAcc    },
        { "cab_band_mid",  "cab_m_fesea", "cab_u_mid",  nullptr },
        { "cab_band_high", "cab_m_sea",   "cab_u_high", nullptr },
    };
    for (int i = 0; i < 3; ++i) {
        m_methodTable->setItem(i, 0, textItem(I18n::tr(kMeth[i].band)));
        m_methodTable->setItem(i, 1, badgeItem(I18n::tr(kMeth[i].method),
                                               kMeth[i].color));
        m_methodTable->setItem(i, 2, textItem(I18n::tr(kMeth[i].use)));
    }
    sf->vbox()->addWidget(m_methodTable);
    v->addWidget(sf);

    // ── 車室モデル ──────────────────────────────────────────────────────────
    auto *sc = new SectionBox(I18n::tr("cab_model_section"), body);
    auto *hCad = new QHBoxLayout();
    m_cadFile = new QLineEdit("cabin_interior.step", sc);
    hCad->addWidget(m_cadFile, 1);
    auto *cadBrowse = new QPushButton(I18n::tr("cab_browse"), sc);
    hCad->addWidget(cadBrowse);
    sc->form()->addRow(I18n::tr("cab_cad"), hCad);
    m_modal = makeCheck(I18n::tr("cab_modal"), true, sc);
    sc->form()->addRow(I18n::tr("cab_modal_row"), m_modal);
    sc->vbox()->addWidget(makeHint(I18n::tr("cab_modal_note"), sc));
    // モード周波数は固定サンプル (解析結果ではない)
    sc->vbox()->addWidget(tabhelp::sampleNote(sc));
    m_absRoof   = makeCheck(I18n::tr("cab_abs_roof"),   true, sc);
    m_absCarpet = makeCheck(I18n::tr("cab_abs_carpet"), true, sc);
    m_absDoor   = makeCheck(I18n::tr("cab_abs_door"),   true, sc);
    m_absSeat   = makeCheck(I18n::tr("cab_abs_seat"),   true, sc);
    sc->form()->addRow(I18n::tr("cab_absorb"),
                       checkRow({ m_absRoof, m_absCarpet, m_absDoor,
                                  m_absSeat }));
    // 車室モデル設定はまだどこにも読まれない
    sc->vbox()->addWidget(tabhelp::unwiredNote(sc));
    v->addWidget(sc);

    // ── 評価 ────────────────────────────────────────────────────────────────
    auto *se = new SectionBox(I18n::tr("cab_metrics_section"), body);
    m_mSpl       = makeCheck(I18n::tr("cab_m_earspl"), true,  se);
    m_mAi        = makeCheck(I18n::tr("cab_m_ai"),     true,  se);
    m_mLoudness  = makeCheck(I18n::tr("cab_m_loud"),   false, se);
    m_mSharpness = makeCheck(I18n::tr("cab_m_sharp"),  false, se);
    se->vbox()->addLayout(checkRow({ m_mSpl, m_mAi }));
    se->vbox()->addLayout(checkRow({ m_mLoudness, m_mSharpness }));
    // 評価指標チェックはローカル状態のみ (どこにも読まれない)
    se->vbox()->addWidget(tabhelp::unwiredNote(se));
    auto *hBadge = new QHBoxLayout();
    hBadge->addWidget(makeBadge(I18n::tr("cab_result"), kAcc, se));
    hBadge->addWidget(makeBadge(I18n::tr("cab_seg_avg"), kOk, se));
    hBadge->addStretch(1);
    se->vbox()->addLayout(hBadge);
    // 評価バッジは固定サンプル — 校正なし絶対 SPL を実測と誤認させない
    // (絶対規則 5・6)
    se->vbox()->addWidget(tabhelp::sampleNote(se));
    auto *hBtn = new QHBoxLayout();
    auto *auralBtn = new QPushButton(I18n::tr("cab_aural_btn"), se);
    auto *tpaBtn   = new QPushButton(I18n::tr("cab_tpa_btn"), se);
    tabhelp::markNotImplemented(auralBtn);
    tabhelp::markNotImplemented(tpaBtn);
    hBtn->addWidget(auralBtn);
    hBtn->addWidget(tpaBtn);
    hBtn->addStretch(1);
    se->vbox()->addLayout(hBtn);
    v->addWidget(se);

    // ── 対策検討 ────────────────────────────────────────────────────────────
    auto *sw = new SectionBox(I18n::tr("cab_meas_section"), body);
    m_measureTable = makeTable({ "", I18n::tr("cab_h_meas"),
                                 I18n::tr("cab_h_effect"),
                                 I18n::tr("cab_h_weight"),
                                 I18n::tr("cab_h_cost") }, 4, sw, 150);
    struct Meas { bool on; const char *name; const char *effect;
                  const char *weight; const char *cost; bool effKey; };
    static const Meas kMeas[4] = {
        { true,  "cab_meas_dash",  "-2.1 dB",      "+1.2 kg", "cab_cost_mid",  false },
        { false, "cab_meas_damp",  "-1.4 dB",      "+2.8 kg", "cab_cost_low",  false },
        { true,  "cab_meas_glass", "cab_eff_glass","+3.5 kg", "cab_cost_high", true  },
        { false, "cab_meas_anc",   "cab_eff_anc",  "+0.5 kg", "cab_cost_high", true  },
    };
    for (int i = 0; i < 4; ++i) {
        m_measureTable->setItem(i, 0, checkItem(kMeas[i].on));
        m_measureTable->setItem(i, 1, textItem(I18n::tr(kMeas[i].name)));
        m_measureTable->setItem(i, 2,
            numItem(kMeas[i].effKey ? I18n::tr(kMeas[i].effect)
                                    : QString::fromUtf8(kMeas[i].effect)));
        m_measureTable->setItem(i, 3,
            numItem(QString::fromUtf8(kMeas[i].weight)));
        m_measureTable->setItem(i, 4, textItem(I18n::tr(kMeas[i].cost)));
    }
    sw->vbox()->addWidget(m_measureTable);
    // 対策効果・重量・コストは固定サンプル (解析結果ではない)
    sw->vbox()->addWidget(tabhelp::sampleNote(sw));
    v->addWidget(sw);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    // 3D モデルの参照ボタンのみ実配線 (隣の QLineEdit にパスを反映)
    connect(cadBrowse, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, I18n::tr("cab_cad"), QString(),
            "3D model (*.step *.stp *.iges *.igs *.stl *.obj);;All files (*)");
        if (!path.isEmpty()) m_cadFile->setText(path);
    });
    connect(m_vehicle, &QComboBox::currentIndexChanged, this, [this](int i) {
        // ローカル state のみ (Project に対応フィールド無し → 永続化しない)
        if (m_updating) return;
        m_vehicleIdx = i;
        m_srcStack->setCurrentIndex(i);
    });
    connect(project, &Project::loaded, this, &CabinAcousticsTab::refresh);
    refresh();
}

void CabinAcousticsTab::refresh()
{
    m_updating = true;
    m_vehicle->setCurrentIndex(m_vehicleIdx);
    m_srcStack->setCurrentIndex(m_vehicleIdx);
    m_updating = false;
}

// ── 乗用車 / car ────────────────────────────────────────────────────────────
QWidget *CabinAcousticsTab::buildCarSources()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0);
    m_carEngine = makeCheck(I18n::tr("cab_car_engine"), true, page);
    m_carRoad   = makeCheck(I18n::tr("cab_car_road"),   true, page);
    m_carWind   = makeCheck(I18n::tr("cab_car_wind"),   true, page);
    m_carBoom   = makeCheck(I18n::tr("cab_car_boom"),   true, page);
    v->addLayout(checkRow({ m_carEngine, m_carRoad }));
    v->addLayout(checkRow({ m_carWind, m_carBoom }));
    return page;
}

// ── EV ──────────────────────────────────────────────────────────────────────
QWidget *CabinAcousticsTab::buildEvSources()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0);
    m_evMotor    = makeCheck(I18n::tr("cab_ev_motor"),    true, page);
    m_evInverter = makeCheck(I18n::tr("cab_ev_inverter"), true, page);
    m_evRoad     = makeCheck(I18n::tr("cab_ev_road"),     true, page);
    m_evGear     = makeCheck(I18n::tr("cab_ev_gear"),     true, page);
    v->addLayout(checkRow({ m_evMotor, m_evInverter }));
    v->addLayout(checkRow({ m_evRoad, m_evGear }));
    v->addWidget(makeHint(I18n::tr("cab_ev_note"), page));
    return page;
}

// ── 鉄道 / train ────────────────────────────────────────────────────────────
QWidget *CabinAcousticsTab::buildTrainSources()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0);
    m_trRolling = makeCheck(I18n::tr("cab_tr_rolling"), true, page);
    m_trTunnel  = makeCheck(I18n::tr("cab_tr_tunnel"),  true, page);
    m_trHvac    = makeCheck(I18n::tr("cab_tr_hvac"),    true, page);
    v->addLayout(checkRow({ m_trRolling, m_trTunnel, m_trHvac }));
    return page;
}

// ── 航空機 / aircraft ───────────────────────────────────────────────────────
QWidget *CabinAcousticsTab::buildAircraftSources()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0);
    m_acTbl      = makeCheck(I18n::tr("cab_ac_tbl"),    true,  page);
    m_acEngine   = makeCheck(I18n::tr("cab_ac_engine"), true,  page);
    m_acPressure = makeCheck(I18n::tr("cab_ac_press"),  false, page);
    v->addLayout(checkRow({ m_acTbl, m_acEngine, m_acPressure }));
    return page;
}
