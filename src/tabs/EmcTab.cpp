// EmcTab.cpp
#include "EmcTab.h"
#include "../core/Project.h"
#include "../widgets/MiniPlot.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFont>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QVector>

#include <cmath>

using namespace ofd;

// ── タブ固有語彙 (emc_) — file-local 登録 ───────────────────────────────────
namespace {
const bool s_i18n = [] {
    using ofd::I18n;
    // タブ名 (MainWindow 配線用)
    I18n::reg("t_emc", "📻 EMC/EMI", "📻 EMC/EMI");

    // 概要
    I18n::reg("emc_title", "EMC / EMI 解析 / Compliance analysis",
              "EMC / EMI compliance analysis");
    I18n::reg("emc_hint",
              "放射エミッション・イミュニティを規格の試験配置そのままで再現。"
              "試作前に規格逸脱を予測。",
              "Reproduces radiated emission and immunity tests in the standard's "
              "own setup, predicting non-compliance before the first prototype.");
    I18n::reg("emc_mode_emission", "放射エミッション", "Radiated emission");
    I18n::reg("emc_mode_conducted", "伝導エミッション", "Conducted emission");
    I18n::reg("emc_mode_immunity", "イミュニティ (RS/ESD)", "Immunity (RS/ESD)");
    I18n::reg("emc_standard", "適用規格", "Applicable standard");
    I18n::reg("emc_std_cispr32", "CISPR 32 / EN 55032 (マルチメディア機器)",
              "CISPR 32 / EN 55032 (multimedia equipment)");
    I18n::reg("emc_std_cispr25", "CISPR 25 (車載)", "CISPR 25 (automotive)");
    I18n::reg("emc_std_fcc15", "FCC Part 15 Class B", "FCC Part 15 Class B");
    I18n::reg("emc_std_iec4_3", "IEC 61000-4-3 (放射イミュニティ)",
              "IEC 61000-4-3 (radiated immunity)");
    I18n::reg("emc_std_iec4_2", "IEC 61000-4-2 (ESD)", "IEC 61000-4-2 (ESD)");
    I18n::reg("emc_std_do160", "RTCA DO-160 (航空)", "RTCA DO-160 (avionics)");
    I18n::reg("emc_std_mil461", "MIL-STD-461G (軍用)", "MIL-STD-461G (military)");

    // 試験配置
    I18n::reg("emc_setup_section", "試験配置 / Test setup", "Test setup");
    I18n::reg("emc_site", "サイト", "Site");
    I18n::reg("emc_site_oats", "OATS (オープンサイト)",
              "OATS (open area test site)");
    I18n::reg("emc_site_semi", "セミアネコイックチャンバ",
              "Semi-anechoic chamber");
    I18n::reg("emc_site_full", "フルアネコイック", "Fully anechoic");
    I18n::reg("emc_site_rev", "リバブレーションチャンバ",
              "Reverberation chamber");
    I18n::reg("emc_distance", "測定距離", "Measurement distance");
    I18n::reg("emc_distance_unit", "m (10m換算も併記)",
              "m (10 m equivalent also reported)");
    I18n::reg("emc_ant_h", "アンテナ高", "Antenna height");
    I18n::reg("emc_ant_h_unit", "〜4.0 m 走査", "scanned up to 4.0 m");
    I18n::reg("emc_eut", "EUT配置", "EUT placement");
    I18n::reg("emc_eut_turn", "ターンテーブル0〜360° (15°刻み)",
              "Turntable 0–360° (15° steps)");
    I18n::reg("emc_eut_pol", "水平/垂直偏波両方",
              "Both horizontal and vertical polarization");
    I18n::reg("emc_gnd", "グランドプレーン", "Ground plane");
    I18n::reg("emc_gnd_pec", "金属床 (PEC) を模擬",
              "Model the metal floor (PEC)");
    I18n::reg("emc_gnd_cable", "ケーブル配線を含む", "Include the cable routing");

    // 放射源
    I18n::reg("emc_src_section", "放射源 / Emission sources", "Emission sources");
    I18n::reg("emc_src_switching", "基板のスイッチングノイズ (PEEC抽出結果を使用)",
              "Board switching noise (uses the PEEC extraction result)");
    I18n::reg("emc_src_cm", "ケーブル・コモンモード電流",
              "Cable common-mode current");
    I18n::reg("emc_src_slit", "筐体スリット・開口",
              "Enclosure slits and apertures");
    I18n::reg("emc_clock", "クロック", "Clock");
    I18n::reg("emc_clock_unit", "MHz (高調波 40次まで)",
              "MHz (harmonics up to the 40th)");

    // 判定結果
    I18n::reg("emc_check_section", "判定結果 / Compliance check",
              "Compliance check");
    I18n::reg("emc_col_freq", "周波数", "Frequency");
    I18n::reg("emc_col_meas", "実測相当値", "Measured equivalent");
    I18n::reg("emc_col_limit", "規格限度", "Limit");
    I18n::reg("emc_col_margin", "マージン", "Margin");
    I18n::reg("emc_col_verdict", "判定", "Verdict");
    I18n::reg("emc_pass", "合格", "Pass");
    I18n::reg("emc_caution", "要注意", "Marginal");
    I18n::reg("emc_fail", "不合格", "Fail");
    I18n::reg("emc_btn_locate", "🔍 500MHz の放射源を特定 (寄与度)",
              "🔍 Locate the 500 MHz source (contributions)");
    I18n::reg("emc_btn_report", "📄 EMC事前評価レポート",
              "📄 EMC pre-compliance report");

    // 対策検討
    I18n::reg("emc_mit_section", "対策検討 / Mitigation", "Mitigation");
    I18n::reg("emc_col_mit", "対策", "Countermeasure");
    I18n::reg("emc_col_gain", "500MHz改善", "500 MHz improvement");
    I18n::reg("emc_col_cost", "コスト", "Cost");
    I18n::reg("emc_mit_ferrite", "ケーブルにフェライトコア",
              "Ferrite core on the cable");
    I18n::reg("emc_mit_slit", "筐体スリット幅を1/2に",
              "Halve the enclosure slit width");
    I18n::reg("emc_mit_via", "GNDビア追加 (スティッチング)",
              "Add GND vias (stitching)");
    I18n::reg("emc_mit_choke", "コモンモードチョーク", "Common-mode choke");
    I18n::reg("emc_cost_low", "低", "Low");
    I18n::reg("emc_cost_mid", "中", "Medium");
    I18n::reg("emc_mit_result",
              "選択対策後の予測: 41.2 dBμV/m (マージン -5.8dB)",
              "Prediction with the selected measures: 41.2 dBμV/m "
              "(margin -5.8 dB)");

    // 伝導エミッション
    I18n::reg("emc_cond_section", "伝導エミッション / Conducted emission",
              "Conducted emission");
    I18n::reg("emc_cond_setup", "測定系", "Measurement setup");
    I18n::reg("emc_cond_lisn", "LISN (AMN) 50Ω/50μH", "LISN (AMN) 50 Ω / 50 μH");
    I18n::reg("emc_cond_probe", "電流プローブ", "Current probe");
    I18n::reg("emc_cond_cdn", "CDN", "CDN");
    I18n::reg("emc_cond_range", "周波数範囲", "Frequency range");
    I18n::reg("emc_cond_range_unit", "〜30 MHz", "to 30 MHz");
    I18n::reg("emc_det_qp", "準尖頭値 (QP) 検波", "Quasi-peak (QP) detection");
    I18n::reg("emc_det_av", "平均値 (AV) 検波", "Average (AV) detection");
    I18n::reg("emc_cond_hint",
              "▸ PEEC抽出した基板寄生+電源フィルタ回路をSPICE共シミュレーション"
              "して算出。",
              "▸ Computed by SPICE co-simulation of the PEEC-extracted board "
              "parasitics plus the power-line filter.");

    // イミュニティ
    I18n::reg("emc_imm_section", "イミュニティ / Immunity", "Immunity");
    I18n::reg("emc_imm_test", "試験", "Test");
    I18n::reg("emc_imm_rs", "放射イミュニティ (RS)",
              "Radiated susceptibility (RS)");
    I18n::reg("emc_imm_esd", "ESD", "ESD");
    I18n::reg("emc_imm_eft", "ファストトランジェント",
              "Electrical fast transient");
    I18n::reg("emc_imm_surge", "サージ", "Surge");
    I18n::reg("emc_imm_level", "試験レベル", "Test level");
    I18n::reg("emc_imm_level_unit", "V/m (80MHz〜6GHz, 80%AM)",
              "V/m (80 MHz–6 GHz, 80% AM)");
    I18n::reg("emc_esd_v", "ESD電圧", "ESD voltage");
    I18n::reg("emc_esd_v_unit", "kV (接触) / 15kV (気中)",
              "kV (contact) / 15 kV (air discharge)");
    I18n::reg("emc_imm_field", "筐体内部の電界分布を可視化",
              "Visualize the E-field distribution inside the enclosure");
    I18n::reg("emc_imm_induced", "基板上の誘導電圧を算出",
              "Compute the voltage induced on the board");
    I18n::reg("emc_imm_badge", "クリティカル: IC#3 入力ピンで 42V 誘導 (耐圧 30V)",
              "Critical: 42 V induced on the IC#3 input pin (rated 30 V)");
    return true;
}();

// ── 小物ヘルパー (mock の badge / muted / q-table / Seg 相当) ───────────────
const char kAcc[]  = "#0078D4";     // badge acc / var(--acc)
const char kOk[]   = "#2E8B57";     // badge ok
const char kWarn[] = "#B45309";     // badge warn
const char kErr[]  = "#B91C1C";     // badge err

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

QTableWidgetItem *monoItem(const QString &s)
{
    auto *it = new QTableWidgetItem(s);
    QFont f = it->font();
    f.setFamily("monospace");
    it->setFont(f);
    return it;
}

// 判定セル (mock の <span className="badge ok|warn|err">)
QTableWidgetItem *badgeItem(const QString &s, const char *color)
{
    auto *it = new QTableWidgetItem(s);
    it->setForeground(QColor(color));
    QFont f = it->font();
    f.setBold(true);
    it->setFont(f);
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

// 判定結果表 (モックの <tbody> をそのまま)
struct CompRow { const char *freq, *meas, *limit, *margin, *verdictKey;
                 const char *color; };
const CompRow kComp[4] = {
    { "100 MHz", "32.1 dBμV/m", "40.0", "-7.9", "emc_pass",    kOk   },
    { "300 MHz", "44.8 dBμV/m", "47.0", "-2.2", "emc_caution", kWarn },
    { "500 MHz", "48.5 dBμV/m", "47.0", "+1.5", "emc_fail",    kErr  },
    { "700 MHz", "41.2 dBμV/m", "47.0", "-5.8", "emc_pass",    kOk   },
};

// 対策検討表 (モックの defaultChecked をそのまま)
struct MitRow { bool on; const char *nameKey, *gain, *costKey; };
const MitRow kMit[4] = {
    { true,  "emc_mit_ferrite", "-4.2 dB", "emc_cost_low" },
    { false, "emc_mit_slit",    "-2.8 dB", "emc_cost_mid" },
    { true,  "emc_mit_via",     "-3.1 dB", "emc_cost_low" },
    { false, "emc_mit_choke",   "-6.5 dB", "emc_cost_mid" },
};
} // namespace

// ── EmcTab ──────────────────────────────────────────────────────────────────
EmcTab::EmcTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // ── EMC / EMI 解析 (概要 + モード + 適用規格) ───────────────────────────
    auto *st = new SectionBox(I18n::tr("emc_title"), body);
    st->vbox()->addWidget(makeHint(I18n::tr("emc_hint"), st));
    st->vbox()->addWidget(segRow(st, &m_mode,
                                 { I18n::tr("emc_mode_emission"),
                                   I18n::tr("emc_mode_conducted"),
                                   I18n::tr("emc_mode_immunity") }, 0));
    m_standard = new QComboBox(st);
    m_standard->addItems({ I18n::tr("emc_std_cispr32"),
                           I18n::tr("emc_std_cispr25"),
                           I18n::tr("emc_std_fcc15"),
                           I18n::tr("emc_std_iec4_3"),
                           I18n::tr("emc_std_iec4_2"),
                           I18n::tr("emc_std_do160"),
                           I18n::tr("emc_std_mil461") });
    m_standard->setCurrentIndex(0);          // mock: defaultValue="cispr32"
    st->form()->addRow(I18n::tr("emc_standard"), m_standard);
    v->addWidget(st);

    // ── モード別セクション群 (show/hide で切替) ─────────────────────────────
    m_emissionPage  = buildEmissionPage();
    m_conductedPage = buildConductedPage();
    m_immunityPage  = buildImmunityPage();
    v->addWidget(m_emissionPage);
    v->addWidget(m_conductedPage);
    v->addWidget(m_immunityPage);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    connect(m_mode, &QButtonGroup::idClicked, this, [this](int id) {
        // ローカル state のみ (Project に対応フィールド無し → 永続化しない)
        if (m_updating) return;
        m_modeIdx = id;
        onModeChanged();
    });
    connect(project, &Project::loaded, this, &EmcTab::refresh);
    refresh();
}

void EmcTab::refresh()
{
    m_updating = true;
    if (auto *b = m_mode->button(m_modeIdx)) b->setChecked(true);
    m_updating = false;
    onModeChanged();
}

void EmcTab::onModeChanged()
{
    m_emissionPage->setVisible(m_modeIdx == 0);
    m_conductedPage->setVisible(m_modeIdx == 1);
    m_immunityPage->setVisible(m_modeIdx == 2);
}

// ── 放射エミッション: 試験配置 / 放射源 / 判定結果 / 対策検討 ───────────────
QWidget *EmcTab::buildEmissionPage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(8);

    // 試験配置 / Test setup
    auto *ss = new SectionBox(I18n::tr("emc_setup_section"), page);
    ss->form()->addRow(I18n::tr("emc_site"),
                       segRow(ss, &m_site, { I18n::tr("emc_site_oats"),
                                             I18n::tr("emc_site_semi"),
                                             I18n::tr("emc_site_full"),
                                             I18n::tr("emc_site_rev") }, 0));
    m_distance = numEdit("3", ss);
    ss->form()->addRow(I18n::tr("emc_distance"),
                       unitRow(m_distance, I18n::tr("emc_distance_unit"), ss));
    m_antHeight = numEdit("1.0", ss);
    ss->form()->addRow(I18n::tr("emc_ant_h"),
                       unitRow(m_antHeight, I18n::tr("emc_ant_h_unit"), ss));
    m_turnTable = makeCheck(I18n::tr("emc_eut_turn"), true, ss);
    m_bothPol   = makeCheck(I18n::tr("emc_eut_pol"), true, ss);
    ss->form()->addRow(I18n::tr("emc_eut"),
                       checkRow({ m_turnTable, m_bothPol }));
    m_gndPec   = makeCheck(I18n::tr("emc_gnd_pec"), true, ss);
    m_gndCable = makeCheck(I18n::tr("emc_gnd_cable"), true, ss);
    ss->form()->addRow(I18n::tr("emc_gnd"),
                       checkRow({ m_gndPec, m_gndCable }));
    v->addWidget(ss);

    // 放射源 / Emission sources
    auto *se = new SectionBox(I18n::tr("emc_src_section"), page);
    m_srcSwitching  = makeCheck(I18n::tr("emc_src_switching"), true, se);
    m_srcCommonMode = makeCheck(I18n::tr("emc_src_cm"), true, se);
    m_srcSlit       = makeCheck(I18n::tr("emc_src_slit"), true, se);
    se->vbox()->addLayout(checkRow({ m_srcSwitching }));
    se->vbox()->addLayout(checkRow({ m_srcCommonMode, m_srcSlit }));
    m_clock = numEdit("100", se);
    se->form()->addRow(I18n::tr("emc_clock"),
                       unitRow(m_clock, I18n::tr("emc_clock_unit"), se));
    v->addWidget(se);

    // 判定結果 / Compliance check
    auto *sc = new SectionBox(I18n::tr("emc_check_section"), page);
    m_compTable = makeTable({ I18n::tr("emc_col_freq"), I18n::tr("emc_col_meas"),
                              I18n::tr("emc_col_limit"),
                              I18n::tr("emc_col_margin"),
                              I18n::tr("emc_col_verdict") }, 4, sc, 140);
    sc->vbox()->addWidget(m_compTable);
    fillComplianceTable();

    m_spectrum = new MiniPlot(sc);
    m_spectrum->setLabels("f [MHz]", "dBμV/m @3m");
    m_spectrum->setMinimumSize(360, 130);        // mock: width=360 height=130
    sc->vbox()->addWidget(m_spectrum);
    updateSpectrumPlot();

    auto *cb = new QHBoxLayout();
    cb->addWidget(new QPushButton(I18n::tr("emc_btn_locate"), sc));
    cb->addWidget(new QPushButton(I18n::tr("emc_btn_report"), sc));
    cb->addStretch(1);
    sc->vbox()->addLayout(cb);
    v->addWidget(sc);

    // 対策検討 / Mitigation
    auto *sm = new SectionBox(I18n::tr("emc_mit_section"), page);
    m_mitTable = makeTable({ QString(), I18n::tr("emc_col_mit"),
                             I18n::tr("emc_col_gain"),
                             I18n::tr("emc_col_cost") }, 4, sm, 140);
    sm->vbox()->addWidget(m_mitTable);
    fillMitigationTable();
    m_mitBadge = makeBadge(I18n::tr("emc_mit_result"), kOk, sm);
    auto *mb = new QHBoxLayout();
    mb->addWidget(m_mitBadge);
    mb->addStretch(1);
    sm->vbox()->addLayout(mb);
    v->addWidget(sm);

    return page;
}

void EmcTab::fillComplianceTable()
{
    for (int r = 0; r < 4; ++r) {
        const CompRow &row = kComp[r];
        m_compTable->setItem(r, 0, monoItem(QString::fromUtf8(row.freq)));
        m_compTable->setItem(r, 1, numItem(QString::fromUtf8(row.meas)));
        m_compTable->setItem(r, 2, numItem(QString::fromUtf8(row.limit)));
        m_compTable->setItem(r, 3, numItem(QString::fromUtf8(row.margin)));
        m_compTable->setItem(r, 4, badgeItem(I18n::tr(row.verdictKey), row.color));
    }
}

void EmcTab::fillMitigationTable()
{
    for (int r = 0; r < 4; ++r) {
        const MitRow &row = kMit[r];
        m_mitTable->setItem(r, 0, checkItem(row.on));
        m_mitTable->setItem(r, 1, textItem(I18n::tr(row.nameKey)));
        m_mitTable->setItem(r, 2, numItem(QString::fromUtf8(row.gain)));
        m_mitTable->setItem(r, 3, textItem(I18n::tr(row.costKey)));
    }
}

// モック: f = 30 + i*16, y = 28 + |sin(i*0.55)|*22 + (i>28 && i<34 ? 8 : 0)
void EmcTab::updateSpectrumPlot()
{
    MiniSeries s;
    s.color = QColor(kAcc);                    // mock: color="var(--acc)"
    for (int i = 0; i < 60; ++i) {
        const double f = 30.0 + i * 16.0;
        const double y = 28.0 + std::fabs(std::sin(i * 0.55)) * 22.0
                       + ((i > 28 && i < 34) ? 8.0 : 0.0);
        s.pts.push_back({ f, y });
    }
    m_spectrum->setSeries({ s });
}

// ── 伝導エミッション ────────────────────────────────────────────────────────
QWidget *EmcTab::buildConductedPage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(8);

    auto *s = new SectionBox(I18n::tr("emc_cond_section"), page);
    s->form()->addRow(I18n::tr("emc_cond_setup"),
                      segRow(s, &m_condSetup, { I18n::tr("emc_cond_lisn"),
                                                I18n::tr("emc_cond_probe"),
                                                I18n::tr("emc_cond_cdn") }, 0));
    m_condFreq = numEdit("0.15", s);
    s->form()->addRow(I18n::tr("emc_cond_range"),
                      unitRow(m_condFreq, I18n::tr("emc_cond_range_unit"), s));
    m_detQp = makeCheck(I18n::tr("emc_det_qp"), true, s);
    m_detAv = makeCheck(I18n::tr("emc_det_av"), true, s);
    s->vbox()->addLayout(checkRow({ m_detQp, m_detAv }));
    s->vbox()->addWidget(makeHint(I18n::tr("emc_cond_hint"), s));
    v->addWidget(s);

    return page;
}

// ── イミュニティ ────────────────────────────────────────────────────────────
QWidget *EmcTab::buildImmunityPage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(8);

    auto *s = new SectionBox(I18n::tr("emc_imm_section"), page);
    s->form()->addRow(I18n::tr("emc_imm_test"),
                      segRow(s, &m_immTest, { I18n::tr("emc_imm_rs"),
                                              I18n::tr("emc_imm_esd"),
                                              I18n::tr("emc_imm_eft"),
                                              I18n::tr("emc_imm_surge") }, 0));
    m_immLevel = numEdit("10", s);
    s->form()->addRow(I18n::tr("emc_imm_level"),
                      unitRow(m_immLevel, I18n::tr("emc_imm_level_unit"), s));
    m_esdVolt = numEdit("8", s);
    s->form()->addRow(I18n::tr("emc_esd_v"),
                      unitRow(m_esdVolt, I18n::tr("emc_esd_v_unit"), s));
    m_immField   = makeCheck(I18n::tr("emc_imm_field"), true, s);
    m_immInduced = makeCheck(I18n::tr("emc_imm_induced"), true, s);
    s->vbox()->addLayout(checkRow({ m_immField, m_immInduced }));
    m_immBadge = makeBadge(I18n::tr("emc_imm_badge"), kWarn, s);
    auto *bb = new QHBoxLayout();
    bb->addWidget(m_immBadge);
    bb->addStretch(1);
    s->vbox()->addLayout(bb);
    v->addWidget(s);

    return page;
}
