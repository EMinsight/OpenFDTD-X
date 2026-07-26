// CircuitSolversTab.cpp
#include "CircuitSolversTab.h"
#include "../core/Project.h"
#include "../widgets/MiniPlot.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFont>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include <cmath>

using namespace ofd;

// ── タブ専用語彙 (file-local 登録, 接頭辞 cir_) ─────────────────────────────
namespace {
const bool s_i18n = [] {
    using ofd::I18n;
    I18n::reg("cir_title", "回路系電磁解析 (PEEC / FEM)",
              "Circuit-oriented EM (PEEC / FEM)");
    I18n::reg("cir_hint",
              "FDTD (主ソルバ) を補完する回路抽出用ソルバ。PCB・パッケージ・バスバー等から"
              "寄生RLGC・Sパラメータを抽出し、SPICE/FDTDポートへ受け渡します。",
              "Companion solvers for circuit extraction alongside FDTD (the primary "
              "solver). Extract parasitic RLGC and S-parameters from PCBs, packages "
              "and bus bars, then hand them to SPICE / FDTD ports.");
    I18n::reg("cir_solver", "ソルバ", "Solver");
    I18n::reg("cir_solver_peec", "PEEC (部分要素等価回路)",
              "PEEC (partial element equivalent circuit)");
    I18n::reg("cir_solver_femq", "FEM 準静的 (RLGC抽出)",
              "FEM quasi-static (RLGC extraction)");
    I18n::reg("cir_solver_femw", "FEM 波動 (Sパラメータ)",
              "FEM full-wave (S-parameters)");
    I18n::reg("cir_desc_peec",
              "導体を部分インダクタンス/容量の等価回路に分解。ノイズ経路・バスバー・"
              "グラウンド系に最適。SPICEと直接結合。",
              "Decomposes conductors into a partial-inductance/capacitance equivalent "
              "circuit. Ideal for noise paths, bus bars and ground systems; couples "
              "directly to SPICE.");
    I18n::reg("cir_desc_femq",
              "準静的FEMでR/L/G/C行列を抽出。伝送線路・ケーブル断面・IC パッケージ向け "
              "(λ≫寸法)。",
              "Quasi-static FEM extracting the R/L/G/C matrices. For transmission "
              "lines, cable cross-sections and IC packages (λ ≫ dimensions).");
    I18n::reg("cir_desc_femw",
              "フル波FEM。共振・高周波コネクタ・ビア遷移のSパラメータ抽出 (HFSS相当)。"
              "高Q構造はFDTDより高速。",
              "Full-wave FEM extracting S-parameters of resonators, RF connectors and "
              "via transitions (HFSS-class). Faster than FDTD on high-Q structures.");
    I18n::reg("cir_split", "FDTDとの役割分担", "Division of roles with FDTD");
    I18n::reg("cir_split_fdtd", "FDTD=広帯域・放射・大規模",
              "FDTD = wideband, radiation, large scale");
    I18n::reg("cir_split_circuit", "PEEC/FEM=回路抽出・高Q・準静的",
              "PEEC/FEM = circuit extraction, high-Q, quasi-static");

    // サブタブ
    I18n::reg("cir_tab_model", "モデル/ポート", "Model / ports");
    I18n::reg("cir_tab_extract", "抽出設定", "Extraction");
    I18n::reg("cir_tab_spice", "SPICE連成", "SPICE co-sim");
    I18n::reg("cir_tab_results", "結果", "Results");

    // モデル/ポート
    I18n::reg("cir_structure", "解析対象", "Structure");
    I18n::reg("cir_import", "取込", "Import");
    I18n::reg("cir_import_board", "PCB (ODB++/Gerber)", "PCB (ODB++/Gerber)");
    I18n::reg("cir_import_cad", "3D CAD (STEP)", "3D CAD (STEP)");
    I18n::reg("cir_import_geom", "本体ジオメトリ共有", "Share the main geometry");
    I18n::reg("cir_file", "ファイル", "File");
    I18n::reg("cir_browse", "📁 参照…", "📁 Browse…");
    I18n::reg("cir_cond_only", "導体層のみ抽出 (誘電体は層定義から)",
              "Extract conductor layers only (dielectrics from the stackup)");
    I18n::reg("cir_via_auto", "ビア/パッドを自動認識", "Auto-detect vias / pads");
    I18n::reg("cir_ports", "ポート定義", "Ports");
    I18n::reg("cir_col_name", "名前", "Name");
    I18n::reg("cir_col_kind", "種類", "Type");
    I18n::reg("cir_col_net", "接続ネット", "Net");
    I18n::reg("cir_col_ref", "基準", "Reference");
    I18n::reg("cir_kind_lumped", "集中ポート", "Lumped port");
    I18n::reg("cir_kind_probe", "内部観測", "Internal probe");
    I18n::reg("cir_port_add", "＋ ポートを追加…", "+ Add a port…");

    // 抽出設定
    I18n::reg("cir_extract", "抽出設定", "Extraction");
    I18n::reg("cir_peec_mesh", "メッシュ分割", "Mesh division");
    I18n::reg("cir_peec_mesh_unit", "mm (導体分割幅)", "mm (conductor cell width)");
    I18n::reg("cir_peec_lp", "部分インダクタンス Lp", "Partial inductance Lp");
    I18n::reg("cir_peec_cp", "部分容量 Cp", "Partial capacitance Cp");
    I18n::reg("cir_peec_r", "抵抗 (表皮効果)", "Resistance (skin effect)");
    I18n::reg("cir_peec_rpeec", "遅延PEEC (rPEEC — 放射考慮)",
              "Retarded PEEC (rPEEC — radiation included)");
    I18n::reg("cir_peec_quasi", "準静的PEEC (高速)", "Quasi-static PEEC (fast)");
    I18n::reg("cir_freq_range", "周波数範囲", "Frequency range");
    I18n::reg("cir_peec_freq_unit", "MHz (対数 40点)", "MHz (40 log points)");
    I18n::reg("cir_femq_mesh", "断面メッシュ", "Cross-section mesh");
    I18n::reg("cir_mesh_auto", "自動 (適応)", "Automatic (adaptive)");
    I18n::reg("cir_mesh_manual", "手動", "Manual");
    I18n::reg("cir_femq_rlgc", "RLGC 周波数依存 (表皮/近接効果)",
              "Frequency-dependent RLGC (skin / proximity effect)");
    I18n::reg("cir_femq_tand", "誘電損 tanδ", "Dielectric loss tanδ");
    I18n::reg("cir_femq_points", "周波数点", "Frequency points");
    I18n::reg("cir_femq_points_unit", "点 (対数)", "points (log)");
    I18n::reg("cir_femw_adapt", "適応メッシュ", "Adaptive mesh");
    I18n::reg("cir_femw_adapt_ck", "ΔS 収束 0.02 まで自動細分化",
              "Refine automatically until ΔS converges to 0.02");
    I18n::reg("cir_femw_sweep", "周波数掃引", "Frequency sweep");
    I18n::reg("cir_sweep_discrete", "離散", "Discrete");
    I18n::reg("cir_sweep_interp", "補間掃引 (高速)", "Interpolating sweep (fast)");
    I18n::reg("cir_sweep_eigen", "固有モード", "Eigenmode");
    I18n::reg("cir_range", "範囲", "Range");
    I18n::reg("cir_run_extract", "▶ 抽出実行", "▶ Run extraction");
    I18n::reg("cir_estimate", "推定: %1", "Estimate: %1");
    I18n::reg("cir_est_peec", "~2分 (14k要素)", "~2 min (14k elements)");
    I18n::reg("cir_est_femq", "~30秒", "~30 s");
    I18n::reg("cir_est_femw", "~15分 (適応6パス)", "~15 min (6 adaptive passes)");
    I18n::reg("cir_handoff", "FDTD連成", "Hand-off to FDTD");
    I18n::reg("cir_handoff_sp",
              "抽出Sパラメータを FDTD ポートの周波数依存負荷として使用",
              "Use the extracted S-parameters as a frequency-dependent load on the "
              "FDTD ports");
    I18n::reg("cir_handoff_rom",
              "PEEC等価回路を FDTD 集中定数素子へ縮約 (低次モデル ROM)",
              "Reduce the PEEC equivalent circuit to FDTD lumped elements (ROM)");
    I18n::reg("cir_handoff_hint",
              "▸ 基板近傍=PEEC/FEM、筐体放射=FDTD の分担でマルチスケール解析。",
              "▸ Multi-scale analysis: PEEC/FEM near the board, FDTD for enclosure "
              "radiation.");

    // SPICE
    I18n::reg("cir_spice", "SPICE 共シミュレーション", "SPICE co-simulation");
    I18n::reg("cir_netlist", "ネットリスト", "Netlist");
    I18n::reg("cir_engine", "エンジン", "Engine");
    I18n::reg("cir_engine_ng", "ngspice (内蔵)", "ngspice (built-in)");
    I18n::reg("cir_engine_xyce", "Xyce (並列)", "Xyce (parallel)");
    I18n::reg("cir_engine_ext", "外部 (LTspice等へ書出)",
              "External (export to LTspice etc.)");
    I18n::reg("cir_spice_insert", "抽出寄生 (.snp / 等価回路) を自動挿入",
              "Auto-insert the extracted parasitics (.snp / equivalent circuit)");
    I18n::reg("cir_spice_sw", "スイッチング素子モデル (MOSFET)",
              "Switching device model (MOSFET)");
    I18n::reg("cir_analysis", "解析", "Analysis");
    I18n::reg("cir_ana_tran", "過渡 (スイッチングノイズ)", "Transient (switching noise)");
    I18n::reg("cir_ana_ac", "AC (インピーダンス)", "AC (impedance)");
    I18n::reg("cir_ana_emi", "EMI予測 (放射へFDTD連携)",
              "EMI prediction (radiation via FDTD)");
    I18n::reg("cir_run_spice", "▶ 共シミュレーション実行", "▶ Run co-simulation");

    // 結果
    I18n::reg("cir_results", "抽出結果", "Extracted parameters");
    I18n::reg("cir_col_item", "項目", "Item");
    I18n::reg("cir_res_l", "L (VIN→VOUT ループ)", "L (VIN→VOUT loop)");
    I18n::reg("cir_res_r", "R (表皮効果込み)", "R (skin effect included)");
    I18n::reg("cir_res_c", "C (VBUS-GND)", "C (VBUS-GND)");
    I18n::reg("cir_res_z", "Z (PDNインピーダンス)", "Z (PDN impedance)");
    I18n::reg("cir_exp_snp", "📁 Touchstone .s3p 書出", "📁 Export Touchstone .s3p");
    I18n::reg("cir_exp_spice", "📁 SPICE サブサーキット書出", "📁 Export SPICE subcircuit");
    I18n::reg("cir_exp_h5", "💾 HDF5 保存", "💾 Save HDF5");
    I18n::reg("cir_exp_fdtd", "→ FDTDポートへ適用", "→ Apply to FDTD ports");
    return true;
}();

// ソルバごとの説明文 / 推定時間 (モックの三項演算子をそのまま転記)
const char *kDescKeys[3] = { "cir_desc_peec", "cir_desc_femq", "cir_desc_femw" };
const char *kEstKeys[3]  = { "cir_est_peec", "cir_est_femq", "cir_est_femw" };

// ポート定義表 (モックの <tbody> をそのまま)
struct PortRow { const char *num, *name, *kind, *net, *ref; bool on; };
const PortRow kPorts[3] = {
    { "1", "VIN",     "cir_kind_lumped", "NET_VBUS", "GND", true },
    { "2", "VOUT",    "cir_kind_lumped", "NET_VOUT", "GND", true },
    { "3", "SW_NODE", "cir_kind_probe",  "NET_SW",   "GND", true },
};

// 抽出結果表 (モックの数値をそのまま)
struct ResRow { const char *itemKey, *f1, *f10, *f100; };
const ResRow kResults[4] = {
    { "cir_res_l", "48.2 nH", "45.1 nH", "43.8 nH" },
    { "cir_res_r", "3.2 mΩ",  "8.5 mΩ",  "26.4 mΩ" },
    { "cir_res_c", "182 pF",  "180 pF",  "178 pF"  },
    { "cir_res_z", "0.31 Ω",  "2.9 Ω",   "1.1 Ω"   },
};

QLabel *hintLabel(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setWordWrap(true);
    return l;
}

QLineEdit *numEdit(const char *value, int width, QWidget *parent)
{
    auto *e = new QLineEdit(QString::fromUtf8(value), parent);
    if (width > 0) e->setMaximumWidth(width);
    return e;
}

QCheckBox *check(const QString &text, bool on, QWidget *parent)
{
    auto *ck = new QCheckBox(text, parent);
    ck->setChecked(on);
    return ck;
}

QLabel *makeBadge(const QString &text, bool accent, QWidget *parent)
{
    auto *b = new QLabel(text, parent);
    QString css = "border-radius:3px; padding:1px 6px; font-size:11px;";
    css += accent ? "background:#DEECF9; color:#0078D4;"
                  : "background:palette(midlight);";
    b->setStyleSheet(css);
    return b;
}
} // namespace

// ── CircuitSolversTab ───────────────────────────────────────────────────────
CircuitSolversTab::CircuitSolversTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // ── 回路系電磁解析 (ソルバ選択) ────────────────────────────────────────
    auto *sTop = new SectionBox(I18n::tr("cir_title"), body);
    sTop->vbox()->addWidget(hintLabel(I18n::tr("cir_hint"), sTop));

    m_solver = new QComboBox(sTop);
    m_solver->addItem(I18n::tr("cir_solver_peec"));
    m_solver->addItem(I18n::tr("cir_solver_femq"));
    m_solver->addItem(I18n::tr("cir_solver_femw"));
    sTop->form()->addRow(I18n::tr("cir_solver"), m_solver);

    m_solverDesc = hintLabel(QString(), sTop);
    sTop->form()->addRow(m_solverDesc);

    auto *splitRow = new QHBoxLayout();
    splitRow->addWidget(makeBadge(I18n::tr("cir_split_fdtd"), true, sTop));
    splitRow->addWidget(makeBadge(I18n::tr("cir_split_circuit"), false, sTop));
    splitRow->addStretch(1);
    sTop->form()->addRow(I18n::tr("cir_split"), splitRow);
    v->addWidget(sTop);

    // ── サブタブ ──────────────────────────────────────────────────────────
    m_tabs = new QTabWidget(body);
    m_tabs->setDocumentMode(true);
    m_tabs->addTab(buildModelPage(),   I18n::tr("cir_tab_model"));
    m_tabs->addTab(buildExtractPage(), I18n::tr("cir_tab_extract"));
    m_tabs->addTab(buildSpicePage(),   I18n::tr("cir_tab_spice"));
    m_tabs->addTab(buildResultsPage(), I18n::tr("cir_tab_results"));
    v->addWidget(m_tabs);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    connect(m_solver, &QComboBox::currentIndexChanged,
            this, &CircuitSolversTab::solverChanged);
    solverChanged(0);                    // 既定 "peec"
    updateZPlot();
}

void CircuitSolversTab::solverChanged(int index)
{
    index = qBound(0, index, 2);
    m_solverDesc->setText(I18n::tr(kDescKeys[index]));
    m_extractStack->setCurrentIndex(index);
    m_estimate->setText(I18n::tr("cir_estimate").arg(I18n::tr(kEstKeys[index])));
}

// ── モデル/ポート ───────────────────────────────────────────────────────────
QWidget *CircuitSolversTab::buildModelPage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setSpacing(8);

    // 解析対象 / Structure
    auto *sStr = new SectionBox(I18n::tr("cir_structure"), page);
    auto *imp = new QComboBox(sStr);
    imp->addItem(I18n::tr("cir_import_board"));
    imp->addItem(I18n::tr("cir_import_cad"));
    imp->addItem(I18n::tr("cir_import_geom"));
    sStr->form()->addRow(I18n::tr("cir_import"), imp);

    auto *fileRow = new QHBoxLayout();
    fileRow->addWidget(numEdit("power_board_v3.odb", 0, sStr), 1);
    fileRow->addWidget(new QPushButton(I18n::tr("cir_browse"), sStr));
    sStr->form()->addRow(I18n::tr("cir_file"), fileRow);

    auto *optRow = new QHBoxLayout();
    optRow->addWidget(check(I18n::tr("cir_cond_only"), true, sStr));
    optRow->addWidget(check(I18n::tr("cir_via_auto"), true, sStr));
    optRow->addStretch(1);
    sStr->form()->addRow(optRow);
    v->addWidget(sStr);

    // ポート定義 / Ports
    auto *sPort = new SectionBox(I18n::tr("cir_ports"), page);
    m_portTable = new QTableWidget(4, 6, sPort);
    m_portTable->setHorizontalHeaderLabels({ QString(), "#", I18n::tr("cir_col_name"),
                                             I18n::tr("cir_col_kind"),
                                             I18n::tr("cir_col_net"),
                                             I18n::tr("cir_col_ref") });
    m_portTable->verticalHeader()->setVisible(false);
    m_portTable->verticalHeader()->setDefaultSectionSize(24);
    m_portTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_portTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_portTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_portTable->setMaximumHeight(140);

    QFont mono("Menlo");
    mono.setStyleHint(QFont::Monospace);

    for (int r = 0; r < 3; ++r) {
        const PortRow &p = kPorts[r];
        auto *sel = new QTableWidgetItem();
        sel->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        sel->setCheckState(p.on ? Qt::Checked : Qt::Unchecked);
        m_portTable->setItem(r, 0, sel);

        auto *num = new QTableWidgetItem(QString::fromUtf8(p.num));
        num->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        num->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        m_portTable->setItem(r, 1, num);

        // 名前だけ編集可 (モックの <input className="cell-input">)
        m_portTable->setItem(r, 2, new QTableWidgetItem(QString::fromUtf8(p.name)));

        auto *kind = new QTableWidgetItem(I18n::tr(p.kind));
        kind->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        m_portTable->setItem(r, 3, kind);

        for (int c = 0; c < 2; ++c) {
            auto *it = new QTableWidgetItem(
                QString::fromUtf8(c == 0 ? p.net : p.ref));
            it->setFont(mono);
            it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            m_portTable->setItem(r, 4 + c, it);
        }
    }
    // 追加行 (チェック無し + 5列結合のイタリック行)
    auto *addSel = new QTableWidgetItem();
    addSel->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
    addSel->setCheckState(Qt::Unchecked);
    m_portTable->setItem(3, 0, addSel);
    auto *addIt = new QTableWidgetItem(I18n::tr("cir_port_add"));
    QFont italic = addIt->font();
    italic.setItalic(true);
    addIt->setFont(italic);
    addIt->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    m_portTable->setItem(3, 1, addIt);
    m_portTable->setSpan(3, 1, 1, 5);

    sPort->vbox()->addWidget(m_portTable);
    v->addWidget(sPort);

    v->addStretch(1);
    return page;
}

// ── 抽出設定 (ソルバ別) + FDTD連成 ─────────────────────────────────────────
QWidget *CircuitSolversTab::buildPeecPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->setContentsMargins(0, 0, 0, 0);

    auto *meshRow = new QHBoxLayout();
    meshRow->addWidget(numEdit("0.5", 70, page));
    meshRow->addWidget(new QLabel(I18n::tr("cir_peec_mesh_unit"), page));
    meshRow->addStretch(1);
    form->addRow(I18n::tr("cir_peec_mesh"), meshRow);

    auto *row1 = new QHBoxLayout();
    row1->addWidget(check(I18n::tr("cir_peec_lp"), true, page));
    row1->addWidget(check(I18n::tr("cir_peec_cp"), true, page));
    row1->addWidget(check(I18n::tr("cir_peec_r"), true, page));
    row1->addStretch(1);
    form->addRow(row1);

    auto *row2 = new QHBoxLayout();
    row2->addWidget(check(I18n::tr("cir_peec_rpeec"), false, page));
    row2->addWidget(check(I18n::tr("cir_peec_quasi"), true, page));
    row2->addStretch(1);
    form->addRow(row2);

    auto *freqRow = new QHBoxLayout();
    freqRow->addWidget(numEdit("0.01", 70, page));
    freqRow->addWidget(new QLabel("〜", page));
    freqRow->addWidget(numEdit("100", 70, page));
    freqRow->addWidget(new QLabel(I18n::tr("cir_peec_freq_unit"), page));
    freqRow->addStretch(1);
    form->addRow(I18n::tr("cir_freq_range"), freqRow);
    return page;
}

QWidget *CircuitSolversTab::buildFemqPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->setContentsMargins(0, 0, 0, 0);

    auto *mesh = new QComboBox(page);
    mesh->addItem(I18n::tr("cir_mesh_auto"));
    mesh->addItem(I18n::tr("cir_mesh_manual"));
    form->addRow(I18n::tr("cir_femq_mesh"), mesh);

    auto *row = new QHBoxLayout();
    row->addWidget(check(I18n::tr("cir_femq_rlgc"), true, page));
    row->addWidget(check(I18n::tr("cir_femq_tand"), true, page));
    row->addStretch(1);
    form->addRow(row);

    auto *ptRow = new QHBoxLayout();
    ptRow->addWidget(numEdit("20", 70, page));
    ptRow->addWidget(new QLabel(I18n::tr("cir_femq_points_unit"), page));
    ptRow->addStretch(1);
    form->addRow(I18n::tr("cir_femq_points"), ptRow);
    return page;
}

QWidget *CircuitSolversTab::buildFemwPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->setContentsMargins(0, 0, 0, 0);

    form->addRow(I18n::tr("cir_femw_adapt"),
                 check(I18n::tr("cir_femw_adapt_ck"), true, page));

    auto *sweep = new QComboBox(page);
    sweep->addItem(I18n::tr("cir_sweep_discrete"));
    sweep->addItem(I18n::tr("cir_sweep_interp"));
    sweep->addItem(I18n::tr("cir_sweep_eigen"));
    sweep->setCurrentIndex(1);                   // 既定 "interp"
    form->addRow(I18n::tr("cir_femw_sweep"), sweep);

    auto *rangeRow = new QHBoxLayout();
    rangeRow->addWidget(numEdit("0.1", 70, page));
    rangeRow->addWidget(new QLabel("〜", page));
    rangeRow->addWidget(numEdit("20", 70, page));
    rangeRow->addWidget(new QLabel("GHz", page));
    rangeRow->addStretch(1);
    form->addRow(I18n::tr("cir_range"), rangeRow);
    return page;
}

QWidget *CircuitSolversTab::buildExtractPage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setSpacing(8);

    auto *sExt = new SectionBox(I18n::tr("cir_extract"), page);
    m_extractStack = new QStackedWidget(sExt);
    m_extractStack->addWidget(buildPeecPage());   // [0] peec
    m_extractStack->addWidget(buildFemqPage());   // [1] femq
    m_extractStack->addWidget(buildFemwPage());   // [2] femw
    sExt->vbox()->addWidget(m_extractStack);

    auto *runRow = new QHBoxLayout();
    auto *runBtn = new QPushButton(I18n::tr("cir_run_extract"), sExt);
    runBtn->setDefault(true);                     // q-btn primary
    runRow->addWidget(runBtn);
    m_estimate = new QLabel(sExt);
    runRow->addWidget(m_estimate);
    runRow->addStretch(1);
    sExt->vbox()->addLayout(runRow);
    v->addWidget(sExt);

    // FDTD連成 / Hand-off to FDTD
    auto *sHand = new SectionBox(I18n::tr("cir_handoff"), page);
    sHand->vbox()->addWidget(check(I18n::tr("cir_handoff_sp"), true, sHand));
    sHand->vbox()->addWidget(check(I18n::tr("cir_handoff_rom"), false, sHand));
    sHand->vbox()->addWidget(hintLabel(I18n::tr("cir_handoff_hint"), sHand));
    v->addWidget(sHand);

    v->addStretch(1);
    return page;
}

// ── SPICE 共シミュレーション ────────────────────────────────────────────────
QWidget *CircuitSolversTab::buildSpicePage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setSpacing(8);

    auto *s = new SectionBox(I18n::tr("cir_spice"), page);
    auto *netRow = new QHBoxLayout();
    netRow->addWidget(numEdit("buck_converter.cir", 0, s), 1);
    netRow->addWidget(new QPushButton(I18n::tr("cir_browse"), s));
    s->form()->addRow(I18n::tr("cir_netlist"), netRow);

    auto *engine = new QComboBox(s);
    engine->addItem(I18n::tr("cir_engine_ng"));
    engine->addItem(I18n::tr("cir_engine_xyce"));
    engine->addItem(I18n::tr("cir_engine_ext"));
    s->form()->addRow(I18n::tr("cir_engine"), engine);

    auto *optRow = new QHBoxLayout();
    optRow->addWidget(check(I18n::tr("cir_spice_insert"), true, s));
    optRow->addWidget(check(I18n::tr("cir_spice_sw"), true, s));
    optRow->addStretch(1);
    s->form()->addRow(optRow);

    auto *anaRow = new QHBoxLayout();
    anaRow->addWidget(check(I18n::tr("cir_ana_tran"), true, s));
    anaRow->addWidget(check(I18n::tr("cir_ana_ac"), true, s));
    anaRow->addWidget(check(I18n::tr("cir_ana_emi"), false, s));
    anaRow->addStretch(1);
    s->form()->addRow(I18n::tr("cir_analysis"), anaRow);

    auto *runRow = new QHBoxLayout();
    auto *runBtn = new QPushButton(I18n::tr("cir_run_spice"), s);
    runBtn->setDefault(true);
    runRow->addWidget(runBtn);
    runRow->addStretch(1);
    s->vbox()->addLayout(runRow);
    v->addWidget(s);

    v->addStretch(1);
    return page;
}

// ── 結果 (抽出パラメータ表 + |Z| プロット) ──────────────────────────────────
QWidget *CircuitSolversTab::buildResultsPage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setSpacing(8);

    auto *s = new SectionBox(I18n::tr("cir_results"), page);

    m_resultTable = new QTableWidget(4, 4, s);
    m_resultTable->setHorizontalHeaderLabels({ I18n::tr("cir_col_item"), "@1MHz",
                                               "@10MHz", "@100MHz" });
    m_resultTable->verticalHeader()->setVisible(false);
    m_resultTable->verticalHeader()->setDefaultSectionSize(24);
    m_resultTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_resultTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultTable->setMaximumHeight(140);
    for (int r = 0; r < 4; ++r) {
        const ResRow &row = kResults[r];
        m_resultTable->setItem(r, 0, new QTableWidgetItem(I18n::tr(row.itemKey)));
        const char *vals[3] = { row.f1, row.f10, row.f100 };
        for (int c = 0; c < 3; ++c) {
            auto *it = new QTableWidgetItem(QString::fromUtf8(vals[c]));
            it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            m_resultTable->setItem(r, c + 1, it);
        }
    }
    s->vbox()->addWidget(m_resultTable);

    // |Z| (PDN インピーダンス) — モックの MiniPlot data と同じ数式
    m_zPlot = new MiniPlot(s);
    m_zPlot->setLabels("log f [MHz]", "|Z| [Ω]");
    m_zPlot->setMinimumSize(340, 120);
    s->vbox()->addWidget(m_zPlot);

    auto *btnRow = new QHBoxLayout();
    btnRow->addWidget(new QPushButton(I18n::tr("cir_exp_snp"), s));
    btnRow->addWidget(new QPushButton(I18n::tr("cir_exp_spice"), s));
    btnRow->addWidget(new QPushButton(I18n::tr("cir_exp_h5"), s));
    btnRow->addWidget(new QPushButton(I18n::tr("cir_exp_fdtd"), s));
    btnRow->addStretch(1);
    s->vbox()->addLayout(btnRow);
    v->addWidget(s);

    v->addStretch(1);
    return page;
}

// モック: f = i/39*4 (log10 f: 0.01..100MHz → 0..4),
//         y = |sin(f*2.2) + 0.3| * 2 + f*0.4
void CircuitSolversTab::updateZPlot()
{
    MiniSeries z;
    z.color = QColor("#0078D4");
    for (int i = 0; i < 40; ++i) {
        const double f = i / 39.0 * 4.0;
        const double y = std::fabs(std::sin(f * 2.2) + 0.3) * 2.0 + f * 0.4;
        z.pts.push_back({ f, y });
    }
    m_zPlot->setSeries({ z });
}
