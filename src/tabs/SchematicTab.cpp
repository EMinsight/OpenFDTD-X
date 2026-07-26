// SchematicTab.cpp
#include "SchematicTab.h"
#include "../core/Project.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QFont>
#include <QFontDatabase>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QTableWidget>
#include <QVBoxLayout>

using namespace ofd;

// ── タブ専用の翻訳キー (接頭辞 sch_) ────────────────────────────────────────
namespace {
const bool s_i18n = [] {
    using ofd::I18n;
    I18n::reg("sch_sim_section", "フォトニック回路シミュレーション",
              "Photonic circuit simulation");
    I18n::reg("sch_sim_hint",
              "Ansys INTERCONNECT / Synopsys PhotonicCAD 風の回路エディタ。\n"
              "Sパラメータ・コンパクトモデルを連結して、チップ全体を秒単位で解析。",
              "Circuit editor in the style of Ansys INTERCONNECT / Synopsys PhotonicCAD.\n"
              "Chain S-parameters and compact models to analyze a whole chip in seconds.");
    I18n::reg("sch_mode",       "シミュレーションモード", "Simulation mode");
    I18n::reg("sch_mode_freq",  "周波数領域",             "Frequency domain");
    I18n::reg("sch_mode_time",  "時間領域",               "Time domain");
    I18n::reg("sch_mode_mixed", "混合 (光電子共存)",
              "Mixed (co-simulated opto-electronic)");

    I18n::reg("sch_lib_section", "要素ライブラリ / Element library", "Element library");
    I18n::reg("sch_lib_hint", "ドラッグして回路図に配置 (各要素は S パラメータ / コンパクトモデル)。",
              "Drag onto the schematic (each element is an S-parameter / compact model).");
    I18n::reg("sch_el_wg_s",   "Si リブ 450×220nm", "Si rib 450×220nm");
    I18n::reg("sch_el_eom_s",  "GHz級",             "GHz class");

    I18n::reg("sch_net_section", "ネットリスト / Connections", "Connections (netlist)");
    I18n::reg("sch_col_from",    "From",       "From");
    I18n::reg("sch_col_to",      "To",         "To");
    I18n::reg("sch_col_wl",      "波長依存",   "Wavelength dep.");

    I18n::reg("sch_noise_section", "ノイズ・温度効果", "Noise & temperature effects");
    I18n::reg("sch_shot",    "ショットノイズ",       "Shot noise");
    I18n::reg("sch_thermal", "熱雑音",               "Thermal noise");
    I18n::reg("sch_rin",     "RIN (相対強度雑音)",   "RIN (relative intensity noise)");
    I18n::reg("sch_phase",   "位相雑音",             "Phase noise");
    I18n::reg("sch_temp",    "温度",                 "Temperature");
    I18n::reg("sch_to_shift", "熱光学シフトを自動適用",
              "Apply thermo-optic shift automatically");
    return true;
}();

// muted text-sm 相当
QLabel *mutedLabel(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setWordWrap(true);
    l->setStyleSheet("color:#7A7A7A; font-size:11px;");
    return l;
}

// mock の要素ライブラリ (12 種) をそのまま転記。sub が翻訳キーのものは keyed=true。
struct ElemDef {
    const char *ic;
    const char *name;
    const char *sub;      // keyed のときは I18n キー
    bool        keyed;
};
const ElemDef kElements[12] = {
    { "⫴",  "Waveguide",          "sch_el_wg_s",     true  },
    { "⌑",  "Ring resonator",     "R=5μm, Q=15k",    false },
    { "≡",  "DBR Mirror",         "25 periods",      false },
    { "▷◁", "MZI",                "ΔL=50μm",         false },
    { "⊞",  "MMI 1×2",            "50:50 splitter",  false },
    { "⊟",  "MMI 2×2",            "3dB coupler",     false },
    { "◈",  "Grating coupler",    "-2.8dB @1550",    false },
    { "⊰",  "Edge coupler",       "SSC, -1.5dB",     false },
    { "⌬",  "Phase shifter (TO)", "Vπ=3.5V",         false },
    { "⚡", "PD detector",        "R=0.9 A/W",       false },
    { "☼",  "Laser source",       "DFB 1550nm",      false },
    { "◐",  "Modulator (EOM)",    "sch_el_eom_s",    true  },
};

// mock の netlist 行をそのまま転記
struct NetDef { const char *from, *to, *wl; };
const NetDef kNet[5] = {
    { "LASER1.out", "MZI1.in1",  "1530~1570nm" },
    { "LASER2.out", "MZI1.in2",  "1530~1570nm" },
    { "MZI1.out1",  "RING1.in",  "—" },
    { "RING1.drop", "PD1.in",    "—" },
    { "RING1.thru", "PD2.in",    "—" },
};
} // namespace

SchematicTab::SchematicTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    const QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);

    // ── フォトニック回路シミュレーション ───────────────────────────────────
    auto *sSim = new SectionBox(I18n::tr("sch_sim_section"), body);
    sSim->vbox()->addWidget(mutedLabel(I18n::tr("sch_sim_hint"), sSim));
    m_mode = new QComboBox(sSim);
    m_mode->addItem(I18n::tr("sch_mode_freq"));
    m_mode->addItem(I18n::tr("sch_mode_time"));
    m_mode->addItem(I18n::tr("sch_mode_mixed"));
    m_mode->setCurrentIndex(0);              // mock: value="freq"
    sSim->form()->addRow(I18n::tr("sch_mode"), m_mode);
    v->addWidget(sSim);

    // ── 要素ライブラリ / Element library (3列グリッドのカード) ─────────────
    auto *sLib = new SectionBox(I18n::tr("sch_lib_section"), body);
    sLib->vbox()->addWidget(mutedLabel(I18n::tr("sch_lib_hint"), sLib));
    auto *grid = new QGridLayout();
    grid->setSpacing(6);
    for (int i = 0; i < 12; ++i) {
        const ElemDef &e = kElements[i];
        auto *card = new QFrame(sLib);
        card->setObjectName("elemCard");
        card->setCursor(Qt::OpenHandCursor);   // mock: cursor:"grab"
        card->setStyleSheet(
            "#elemCard { border:1px solid palette(mid); border-radius:3px;"
            " background:palette(alternate-base); }");
        auto *cv = new QVBoxLayout(card);
        cv->setContentsMargins(7, 5, 7, 5);
        cv->setSpacing(1);

        auto *head = new QHBoxLayout();
        head->setSpacing(4);
        auto *ic = new QLabel(QString::fromUtf8(e.ic), card);
        ic->setStyleSheet("font-size:11px; font-weight:600; color:#0078D4;");
        head->addWidget(ic);
        auto *nameL = new QLabel(QString::fromUtf8(e.name), card);
        nameL->setStyleSheet("font-size:11px; font-weight:600;");
        head->addWidget(nameL);
        head->addStretch(1);
        cv->addLayout(head);

        auto *subL = new QLabel(e.keyed ? I18n::tr(e.sub)
                                        : QString::fromUtf8(e.sub), card);
        subL->setStyleSheet("font-size:10px; color:#7A7A7A;");
        subL->setWordWrap(true);
        cv->addWidget(subL);

        grid->addWidget(card, i / 3, i % 3);
    }
    for (int c = 0; c < 3; ++c) grid->setColumnStretch(c, 1);
    sLib->vbox()->addLayout(grid);
    v->addWidget(sLib);

    // ── ネットリスト / Connections ─────────────────────────────────────────
    auto *sNet = new SectionBox(I18n::tr("sch_net_section"), body);
    m_net = new QTableWidget(5, 4, sNet);
    m_net->setHorizontalHeaderLabels({ "#", I18n::tr("sch_col_from"),
                                       I18n::tr("sch_col_to"),
                                       I18n::tr("sch_col_wl") });
    m_net->verticalHeader()->setVisible(false);
    m_net->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_net->setColumnWidth(0, 32);
    m_net->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_net->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_net->setMinimumHeight(170);
    for (int i = 0; i < 5; ++i) {
        auto *no = new QTableWidgetItem(QString::number(i + 1));
        no->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_net->setItem(i, 0, no);
        m_net->setItem(i, 1, new QTableWidgetItem(QString::fromUtf8(kNet[i].from)));
        m_net->setItem(i, 2, new QTableWidgetItem(QString::fromUtf8(kNet[i].to)));
        auto *wl = new QTableWidgetItem(QString::fromUtf8(kNet[i].wl));
        wl->setFont(mono);
        m_net->setItem(i, 3, wl);
    }
    sNet->vbox()->addWidget(m_net);
    v->addWidget(sNet);

    // ── ノイズ・温度効果 ───────────────────────────────────────────────────
    auto *sNo = new SectionBox(I18n::tr("sch_noise_section"), body);
    m_shot = new QCheckBox(I18n::tr("sch_shot"), sNo);
    m_shot->setChecked(true);
    m_thermal = new QCheckBox(I18n::tr("sch_thermal"), sNo);
    m_thermal->setChecked(true);
    auto *n1 = new QHBoxLayout();
    n1->addWidget(m_shot);
    n1->addWidget(m_thermal);
    n1->addStretch(1);
    sNo->vbox()->addLayout(n1);

    m_rin   = new QCheckBox(I18n::tr("sch_rin"), sNo);
    m_phase = new QCheckBox(I18n::tr("sch_phase"), sNo);
    auto *n2 = new QHBoxLayout();
    n2->addWidget(m_rin);
    n2->addWidget(m_phase);
    n2->addStretch(1);
    sNo->vbox()->addLayout(n2);

    m_temp = new QLineEdit("25", sNo);
    m_temp->setMaximumWidth(60);
    auto *tRow = new QHBoxLayout();
    tRow->addWidget(m_temp);
    tRow->addWidget(new QLabel(QString::fromUtf8("°C"), sNo));
    tRow->addStretch(1);
    sNo->form()->addRow(I18n::tr("sch_temp"), tRow);

    m_toShift = new QCheckBox(I18n::tr("sch_to_shift"), sNo);
    m_toShift->setChecked(true);
    sNo->form()->addRow(m_toShift);
    v->addWidget(sNo);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
}
