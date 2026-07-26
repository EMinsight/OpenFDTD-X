// LayoutGDSTab.cpp
#include "LayoutGDSTab.h"
#include "../core/Project.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QFont>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

using namespace ofd;

// ── タブ専用の翻訳キー (接頭辞 gds_) ────────────────────────────────────────
namespace {
const bool s_i18n = [] {
    using ofd::I18n;
    I18n::reg("gds_layout_section", "GDSII レイアウト / Photonic IC layout",
              "Photonic IC layout (GDSII)");
    I18n::reg("gds_layout_hint",
              "KLayout / SiEPIC PDK / RSoft CAD 互換のフォトニックICレイアウトワークスペース。\n"
              "PCell (パラメトリックセル) を配置し、自動配線でフォトニック回路を構築。",
              "Photonic IC layout workspace compatible with KLayout / SiEPIC PDK / RSoft CAD.\n"
              "Place PCells (parametric cells) and build photonic circuits with automatic routing.");
    I18n::reg("gds_pdk",        "プロセス (PDK)", "Process (PDK)");
    I18n::reg("gds_pdk_custom", "カスタム PDK",   "Custom PDK");
    I18n::reg("gds_chip",       "チップサイズ",   "Chip size");
    I18n::reg("gds_grid",       "グリッド",       "Grid");
    I18n::reg("gds_layers_section", "レイヤー / Layers", "Layers");
    I18n::reg("gds_col_layer",  "レイヤー",   "Layer");
    I18n::reg("gds_col_gdsnum", "GDS番号",    "GDS number");
    I18n::reg("gds_col_use",    "用途",       "Purpose");
    I18n::reg("gds_col_color",  "色",         "Color");
    I18n::reg("gds_use_si",     "Si導波路コア",       "Si waveguide core");
    I18n::reg("gds_use_etch1",  "浅エッチ (220→90nm)", "Shallow etch (220→90nm)");
    I18n::reg("gds_use_etch2",  "深エッチ",           "Deep etch");
    I18n::reg("gds_use_metal",  "金属配線",           "Metal wiring");
    I18n::reg("gds_use_via",    "ビア",               "Via");
    I18n::reg("gds_use_heater", "熱光学ヒーター",     "Thermo-optic heater");
    I18n::reg("gds_use_pad",    "パッド",             "Pad");
    I18n::reg("gds_use_text",   "ラベル",             "Label");
    I18n::reg("gds_cells_section", "配置済みセル / Placed cells (PCells)",
              "Placed cells (PCells)");
    I18n::reg("gds_col_name",     "名前",       "Name");
    I18n::reg("gds_col_celltype", "セルタイプ", "Cell type");
    I18n::reg("gds_col_params",   "パラメータ", "Parameters");
    I18n::reg("gds_col_pos",      "位置",       "Position");
    I18n::reg("gds_add_pcell",    "＋ PCellを追加…", "＋ Add PCell…");
    I18n::reg("gds_drc_section", "DRC (デザインルールチェック)",
              "DRC (design rule check)");
    I18n::reg("gds_drc_hint",
              "製造可能性を自動検証。Foundry PDK のルールに準拠。",
              "Automatic manufacturability verification against the foundry PDK rules.");
    I18n::reg("gds_col_rule",       "ルール", "Rule");
    I18n::reg("gds_col_violations", "違反数", "Violations");
    I18n::reg("gds_rule_width",   "最小線幅 Si ≥ 80nm",   "Min. line width Si ≥ 80nm");
    I18n::reg("gds_rule_space",   "最小間隔 Si ≥ 100nm",  "Min. spacing Si ≥ 100nm");
    I18n::reg("gds_rule_bend",    "最小曲率半径 ≥ 5μm",   "Min. bend radius ≥ 5μm");
    I18n::reg("gds_rule_pad",     "パッド間隔 ≥ 50μm",    "Pad spacing ≥ 50μm");
    I18n::reg("gds_rule_density", "密度 0.3 ≤ ρ_Si ≤ 0.7", "Density 0.3 ≤ ρ_Si ≤ 0.7");
    I18n::reg("gds_run_drc", "▶ DRC 実行",        "▶ Run DRC");
    I18n::reg("gds_export",  "📤 GDS エクスポート", "📤 GDS export");
    I18n::reg("gds_import",  "📥 GDS インポート",   "📥 GDS import");
    I18n::reg("gds_fdtd_section", "FDTD-IC 連携 / FDTD ↔ IC layout",
              "FDTD ↔ IC layout");
    I18n::reg("gds_fdtd_hint",
              "選択した領域だけFDTDで詳細解析。残りはSパラメータライブラリで高速回路シミュレーション。",
              "Only the selected region is analyzed rigorously with FDTD; the rest runs as a fast circuit simulation from the S-parameter library.");
    I18n::reg("gds_fdtd_sel", "選択セルをFDTD解析",
              "FDTD-analyze selected cells");
    I18n::reg("gds_fdtd_lib", "他セルはSパラメータライブラリ参照",
              "Other cells use the S-parameter library");
    I18n::reg("gds_fdtd_rerun", "🔍 選択範囲をFDTDで再解析",
              "🔍 Re-analyze selection with FDTD");
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

// チェック列 (mock の <input type="checkbox">) 相当のアイテム
QTableWidgetItem *checkItem(bool checked)
{
    auto *it = new QTableWidgetItem;
    it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
    it->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    return it;
}
} // namespace

LayoutGDSTab::LayoutGDSTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    const QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);

    // GDSII レイアウト / Photonic IC layout
    auto *sTop = new SectionBox(I18n::tr("gds_layout_section"), body);
    sTop->vbox()->addWidget(mutedLabel(I18n::tr("gds_layout_hint"), sTop));

    m_pdk = new QComboBox(sTop);
    m_pdk->addItem("AMF Silicon Photonics 220nm");
    m_pdk->addItem("IMEC iSiPP200");
    m_pdk->addItem("GlobalFoundries 45SPCLO");
    m_pdk->addItem("LiNbO3 on Insulator (LNOI)");
    m_pdk->addItem("SiN (Ligentec/CSEM)");
    m_pdk->addItem(I18n::tr("gds_pdk_custom"));
    sTop->form()->addRow(I18n::tr("gds_pdk"), m_pdk);

    m_chipW = new QLineEdit("5000", sTop);
    m_chipW->setMaximumWidth(100);
    m_chipH = new QLineEdit("5000", sTop);
    m_chipH->setMaximumWidth(100);
    auto *chipRow = new QHBoxLayout();
    chipRow->addWidget(m_chipW);
    chipRow->addWidget(new QLabel(QString::fromUtf8("×"), sTop));
    chipRow->addWidget(m_chipH);
    chipRow->addWidget(new QLabel(QString::fromUtf8("μm"), sTop));
    chipRow->addStretch(1);
    sTop->form()->addRow(I18n::tr("gds_chip"), chipRow);

    m_grid = new QComboBox(sTop);
    m_grid->addItems({ "Manhattan", "DBU (1nm)", "Hexagonal" });
    m_grid->setCurrentIndex(1);        // mock: value="db"
    sTop->form()->addRow(I18n::tr("gds_grid"), m_grid);
    v->addWidget(sTop);

    // レイヤー / Layers
    auto *sLay = new SectionBox(I18n::tr("gds_layers_section"), body);
    m_layers = new QTableWidget(8, 5, sLay);
    m_layers->setHorizontalHeaderLabels({
        "", I18n::tr("gds_col_layer"), I18n::tr("gds_col_gdsnum"),
        I18n::tr("gds_col_use"), I18n::tr("gds_col_color") });
    m_layers->verticalHeader()->setVisible(false);
    m_layers->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_layers->setColumnWidth(0, 26);
    m_layers->setColumnWidth(4, 40);
    m_layers->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_layers->setMinimumHeight(250);
    const struct { const char *name, *gds, *useKey, *color; } layers[8] = {
        { "Si",       "1/0",  "gds_use_si",     "#3B82F6" },
        { "Si_etch1", "2/0",  "gds_use_etch1",  "#1E40AF" },
        { "Si_etch2", "3/0",  "gds_use_etch2",  "#1E3A8A" },
        { "Metal1",   "11/0", "gds_use_metal",  "#FBBF24" },
        { "Via1",     "12/0", "gds_use_via",    "#F59E0B" },
        { "Heater",   "21/0", "gds_use_heater", "#EF4444" },
        { "Pad",      "31/0", "gds_use_pad",    "#10B981" },
        { "Text",     "99/0", "gds_use_text",   "#6B7280" },
    };
    for (int i = 0; i < 8; ++i) {
        m_layers->setItem(i, 0, checkItem(true));
        m_layers->setItem(i, 1, new QTableWidgetItem(
            QString::fromUtf8(layers[i].name)));
        auto *gd = new QTableWidgetItem(QString::fromUtf8(layers[i].gds));
        gd->setFont(mono);
        m_layers->setItem(i, 2, gd);
        m_layers->setItem(i, 3, new QTableWidgetItem(
            I18n::tr(layers[i].useKey)));
        auto *col = new QTableWidgetItem;
        col->setBackground(QColor(layers[i].color));
        col->setFlags(Qt::ItemIsEnabled);
        m_layers->setItem(i, 4, col);
    }
    sLay->vbox()->addWidget(m_layers);
    v->addWidget(sLay);

    // 配置済みセル / Placed cells (PCells)
    auto *sCells = new SectionBox(I18n::tr("gds_cells_section"), body);
    m_cells = new QTableWidget(6, 5, sCells);
    m_cells->setHorizontalHeaderLabels({
        "", I18n::tr("gds_col_name"), I18n::tr("gds_col_celltype"),
        I18n::tr("gds_col_params"), I18n::tr("gds_col_pos") });
    m_cells->verticalHeader()->setVisible(false);
    m_cells->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_cells->setColumnWidth(0, 26);
    m_cells->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_cells->setMinimumHeight(200);
    const struct { const char *name, *type, *params, *pos; } cells[5] = {
        { "RING_1550",  "SiEPIC.RingResonator",  "R=5μm, gap=200nm",    "(100, 200)" },
        { "GC_IN",      "SiEPIC.GratingCoupler", "period=630nm, ff=0.5", "(0, 200)" },
        { "GC_THRU",    "SiEPIC.GratingCoupler", "period=630nm",         "(250, 200)" },
        { "GC_DROP",    "SiEPIC.GratingCoupler", "period=630nm",         "(150, 350)" },
        { "WG_routing", "auto.Route",            "3 ports, R_min=10μm",  "—" },
    };
    for (int i = 0; i < 5; ++i) {
        m_cells->setItem(i, 0, checkItem(true));
        m_cells->setItem(i, 1, new QTableWidgetItem(
            QString::fromUtf8(cells[i].name)));
        m_cells->setItem(i, 2, new QTableWidgetItem(
            QString::fromUtf8(cells[i].type)));
        auto *par = new QTableWidgetItem(QString::fromUtf8(cells[i].params));
        par->setFont(mono);
        m_cells->setItem(i, 3, par);
        auto *pos = new QTableWidgetItem(QString::fromUtf8(cells[i].pos));
        pos->setFont(mono);
        m_cells->setItem(i, 4, pos);
    }
    // 追加行 (mock の「＋ PCellを追加…」、列1〜4を結合)
    m_cells->setItem(5, 0, checkItem(false));
    m_cells->setSpan(5, 1, 1, 4);
    auto *add = new QTableWidgetItem(I18n::tr("gds_add_pcell"));
    QFont italic = add->font();
    italic.setItalic(true);
    add->setFont(italic);
    add->setForeground(QColor("#7A7A7A"));
    m_cells->setItem(5, 1, add);
    sCells->vbox()->addWidget(m_cells);
    v->addWidget(sCells);

    // DRC (デザインルールチェック)
    auto *sDrc = new SectionBox(I18n::tr("gds_drc_section"), body);
    sDrc->vbox()->addWidget(mutedLabel(I18n::tr("gds_drc_hint"), sDrc));
    m_drc = new QTableWidget(5, 4, sDrc);
    m_drc->setHorizontalHeaderLabels({
        "", I18n::tr("gds_col_rule"), I18n::tr("gds_col_violations"),
        I18n::tr("gds_col_pos") });
    m_drc->verticalHeader()->setVisible(false);
    m_drc->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_drc->setColumnWidth(0, 40);
    m_drc->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_drc->setMinimumHeight(170);
    const struct { bool ok; const char *ruleKey; int count; const char *pos; }
    drc[5] = {
        { true,  "gds_rule_width",   0, "—" },
        { true,  "gds_rule_space",   0, "—" },
        { false, "gds_rule_bend",    2, "cell:WG_route (3,4)" },
        { true,  "gds_rule_pad",     0, "—" },
        { true,  "gds_rule_density", 0, "—" },
    };
    for (int i = 0; i < 5; ++i) {
        auto *st = new QTableWidgetItem(drc[i].ok ? QStringLiteral("OK")
                                                  : QStringLiteral("!"));
        st->setTextAlignment(Qt::AlignCenter);
        st->setForeground(drc[i].ok ? QColor("#2E7D32") : QColor("#B06B0F"));
        QFont bold = st->font();
        bold.setBold(true);
        st->setFont(bold);
        m_drc->setItem(i, 0, st);
        m_drc->setItem(i, 1, new QTableWidgetItem(I18n::tr(drc[i].ruleKey)));
        auto *cnt = new QTableWidgetItem(QString::number(drc[i].count));
        cnt->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_drc->setItem(i, 2, cnt);
        auto *pos = new QTableWidgetItem(QString::fromUtf8(drc[i].pos));
        if (!drc[i].ok) pos->setFont(mono);
        m_drc->setItem(i, 3, pos);
    }
    sDrc->vbox()->addWidget(m_drc);
    auto *drcRow = new QHBoxLayout();
    drcRow->addWidget(new QPushButton(I18n::tr("gds_run_drc"), sDrc));
    drcRow->addWidget(new QPushButton(I18n::tr("gds_export"), sDrc));
    drcRow->addWidget(new QPushButton(I18n::tr("gds_import"), sDrc));
    drcRow->addStretch(1);
    sDrc->vbox()->addLayout(drcRow);
    v->addWidget(sDrc);

    // FDTD-IC 連携 / FDTD ↔ IC layout
    auto *sFdtd = new SectionBox(I18n::tr("gds_fdtd_section"), body);
    sFdtd->vbox()->addWidget(mutedLabel(I18n::tr("gds_fdtd_hint"), sFdtd));
    auto *chkRow = new QHBoxLayout();
    auto *chkSel = new QCheckBox(I18n::tr("gds_fdtd_sel"), sFdtd);
    chkSel->setChecked(true);
    auto *chkLib = new QCheckBox(I18n::tr("gds_fdtd_lib"), sFdtd);
    chkLib->setChecked(true);
    chkRow->addWidget(chkSel);
    chkRow->addWidget(chkLib);
    chkRow->addStretch(1);
    sFdtd->vbox()->addLayout(chkRow);
    auto *rerunRow = new QHBoxLayout();
    auto *rerun = new QPushButton(I18n::tr("gds_fdtd_rerun"), sFdtd);
    rerun->setProperty("primary", true);
    rerunRow->addWidget(rerun);
    rerunRow->addStretch(1);
    sFdtd->vbox()->addLayout(rerunRow);
    v->addWidget(sFdtd);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
}
