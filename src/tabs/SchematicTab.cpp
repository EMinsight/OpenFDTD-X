// SchematicTab.cpp
#include "SchematicTab.h"
#include "../core/Project.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"
#include "TabHelpers.h"

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
#include <QPushButton>
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
              "(回路シミュレーションは未実装 — この画面は設計モックです)",
              "Circuit editor in the style of Ansys INTERCONNECT / Synopsys PhotonicCAD.\n"
              "(Circuit simulation is not implemented — this page is a design mock.)");
    I18n::reg("sch_mode",       "シミュレーションモード", "Simulation mode");
    I18n::reg("sch_mode_freq",  "周波数領域",             "Frequency domain");
    I18n::reg("sch_mode_time",  "時間領域",               "Time domain");
    I18n::reg("sch_mode_mixed", "混合 (光電子共存)",
              "Mixed (co-simulated opto-electronic)");

    I18n::reg("sch_lib_section", "要素ライブラリ / Element library", "Element library");
    I18n::reg("sch_lib_hint",
              "各要素は S パラメータ / コンパクトモデル "
              "(回路図キャンバス・ドラッグ配置は未実装)。",
              "Each element is an S-parameter / compact model "
              "(schematic canvas and drag placement are not implemented).");
    I18n::reg("sch_el_wg_s",   "Si リブ 450×220nm", "Si rib 450×220nm");
    I18n::reg("sch_el_eom_s",  "GHz級",             "GHz class");

    I18n::reg("sch_net_section", "ネットリスト / Connections", "Connections (netlist)");
    I18n::reg("sch_col_from",    "From",       "From");
    I18n::reg("sch_col_to",      "To",         "To");
    I18n::reg("sch_col_wl",      "波長依存",   "Wavelength dep.");
    I18n::reg("sch_net_hint",
              "接続は編集でき、プロジェクト (.ofdx) に保存されます。"
              "チェックを外した行は無効な接続として保存されます。"
              "回路図キャンバスからの自動生成と回路シミュレーションは未実装のため、"
              "この表は接続の記録であり計算には渡されません。",
              "The connections are editable and saved with the project (.ofdx); "
              "unchecked rows are stored as disabled. Generating the list from a "
              "schematic canvas and the circuit simulation itself are not "
              "implemented, so this table records connectivity only and is not "
              "passed to any computation.");
    I18n::reg("sch_net_add",   "＋ 接続を追加", "+ Add connection");
    I18n::reg("sch_net_del",   "− 選択行を削除", "− Delete selected row");

    I18n::reg("sch_noise_section", "ノイズ・温度効果", "Noise & temperature effects");
    I18n::reg("sch_shot",    "ショットノイズ",       "Shot noise");
    I18n::reg("sch_thermal", "熱雑音",               "Thermal noise");
    I18n::reg("sch_rin",     "RIN (相対強度雑音)",   "RIN (relative intensity noise)");
    I18n::reg("sch_phase",   "位相雑音",             "Phase noise");
    I18n::reg("sch_temp",    "温度",                 "Temperature");
    I18n::reg("sch_to_shift", "熱光学シフトを自動適用 (未実装)",
              "Apply thermo-optic shift automatically (not implemented)");
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
    sSim->form()->addRow(tabhelp::unwiredNote(sSim));
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
        // キャンバスが無くドラッグできないため、掴めるカーソルにしない
        // (mock は cursor:"grab" だが未実装機能を動作済みに見せない — 絶対規則 5)
        card->setCursor(Qt::ArrowCursor);
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
    // 表は Project::photonicNetlist() のビュー。編集は即座にモデルへ書き戻し、
    // .ofdx ("schematic.netlist") へ保存される (固定サンプルではない)。
    auto *sNet = new SectionBox(I18n::tr("sch_net_section"), body);
    m_netFont = mono;
    m_net = new QTableWidget(0, 4, sNet);
    m_net->setHorizontalHeaderLabels({ "#", I18n::tr("sch_col_from"),
                                       I18n::tr("sch_col_to"),
                                       I18n::tr("sch_col_wl") });
    m_net->verticalHeader()->setVisible(false);
    m_net->setColumnWidth(0, 40);
    m_net->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_net->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_net->setMinimumHeight(170);
    sNet->vbox()->addWidget(m_net);
    sNet->vbox()->addWidget(mutedLabel(I18n::tr("sch_net_hint"), sNet));

    auto *netBtns = new QHBoxLayout();
    auto *addBtn = new QPushButton(I18n::tr("sch_net_add"), sNet);
    auto *delBtn = new QPushButton(I18n::tr("sch_net_del"), sNet);
    netBtns->addWidget(addBtn);
    netBtns->addWidget(delBtn);
    netBtns->addStretch(1);
    sNet->vbox()->addLayout(netBtns);
    v->addWidget(sNet);

    connect(addBtn, &QPushButton::clicked, this, [this] {
        PhotonicNetRow r;
        r.wavelength = QString::fromUtf8("—");
        m_p->photonicNetlist().push_back(r);
        m_p->touch();
        refreshNetlist();
        m_net->setCurrentCell(m_net->rowCount() - 1, 1);
    });
    connect(delBtn, &QPushButton::clicked, this, [this] {
        const int row = m_net->currentRow();
        QVector<PhotonicNetRow> &net = m_p->photonicNetlist();
        if (row < 0 || row >= net.size()) return;
        net.remove(row);
        m_p->touch();
        refreshNetlist();
    });
    connect(m_net, &QTableWidget::itemChanged, this, &SchematicTab::onNetItemChanged);

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
    sNo->form()->addRow(tabhelp::unwiredNote(sNo));
    v->addWidget(sNo);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    connect(project, &Project::loaded, this, &SchematicTab::refreshNetlist);
    refreshNetlist();
}

// モデル → 表 (m_updating ガード付き。行数が変わるので毎回作り直す)
void SchematicTab::refreshNetlist()
{
    m_updating = true;
    const QVector<PhotonicNetRow> &net = m_p->photonicNetlist();
    m_net->setRowCount(net.size());
    for (int i = 0; i < net.size(); ++i) {
        const PhotonicNetRow &r = net[i];
        auto *no = new QTableWidgetItem(QString::number(i + 1));
        no->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        no->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable |
                     Qt::ItemIsUserCheckable);
        no->setCheckState(r.enabled ? Qt::Checked : Qt::Unchecked);
        m_net->setItem(i, 0, no);
        m_net->setItem(i, 1, new QTableWidgetItem(r.from));
        m_net->setItem(i, 2, new QTableWidgetItem(r.to));
        auto *wl = new QTableWidgetItem(r.wavelength);
        wl->setFont(m_netFont);
        m_net->setItem(i, 3, wl);
    }
    m_updating = false;
}

// 表 → モデル (1 セル分)
void SchematicTab::onNetItemChanged(QTableWidgetItem *item)
{
    if (m_updating || !item) return;
    const int row = item->row();
    QVector<PhotonicNetRow> &net = m_p->photonicNetlist();
    if (row < 0 || row >= net.size()) return;
    switch (item->column()) {
    case 0: net[row].enabled = (item->checkState() == Qt::Checked); break;
    case 1: net[row].from = item->text(); break;
    case 2: net[row].to = item->text(); break;
    case 3: net[row].wavelength = item->text(); break;
    default: return;
    }
    m_p->touch();
}
