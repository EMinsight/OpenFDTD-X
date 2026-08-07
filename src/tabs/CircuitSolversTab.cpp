// CircuitSolversTab.cpp
#include "CircuitSolversTab.h"
#include "TabHelpers.h"
#include "../core/Project.h"
#include "../widgets/MiniPlot.h"
#include "../widgets/SectionBox.h"
#include "../em/LumpedRlc.h"
#include "../io/CircuitIO.h"
#include "../kernel/Runner.h"
#include <QProcess>
#include <QPlainTextEdit>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QRegularExpression>
#include "../I18n.h"
#include "../Theme.h"

#include <QCheckBox>
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
    // 抽出ソルバ用の端子座標 (PEEC はポートを節点 2 点で与える)
    I18n::reg("cir_col_p1", "端子A x,y,z [m]", "Terminal A x,y,z [m]");
    I18n::reg("cir_col_p2", "端子B x,y,z [m]", "Terminal B x,y,z [m]");
    I18n::reg("cir_col_z0", "Z0 [Ω]", "Z0 [ohm]");
    I18n::reg("cir_kind_lumped", "集中ポート", "Lumped port");
    I18n::reg("cir_kind_probe", "内部観測", "Internal probe");
    I18n::reg("cir_port_add", "＋ ポートを追加", "+ Add a port");
    I18n::reg("cir_port_del", "− 選択行を削除", "− Delete selected row");
    I18n::reg("cir_port_hint",
              "ポートは編集でき、プロジェクト (.ofdx) に保存されます。"
              "チェックを外した行は無効なポートとして保存されます。"
              "抽出エンジン (PEEC/FEM) の起動は未実装のため、この表は"
              "ポート定義の記録であり計算には渡されません。",
              "Ports are editable and saved with the project (.ofdx); unchecked "
              "rows are stored as disabled. Launching the extraction engine "
              "(PEEC/FEM) is not implemented, so this table only records the port "
              "definitions and is not passed to any computation.");

    // 抽出設定
    I18n::reg("cir_extract", "抽出設定", "Extraction");
    // 抽出実行 (OpenPEEC / OpenFEM)
    I18n::reg("cir_ex_idle",
              "「抽出実行」で OpenPEEC / OpenFEM を起動します。導体はジオメトリ"
              "タブの直方体 (材料が PEC か σ>0)、ポートは下のポート表の端子座標"
              "から作ります。",
              "Run extraction launches OpenPEEC / OpenFEM. Conductors come from "
              "the boxes on the Geometry tab (PEC or sigma>0 material), ports "
              "from the terminal coordinates in the port table below.");
    I18n::reg("cir_ex_noinput", "入力を作れませんでした: %1",
              "Could not build the input: %1");
    I18n::reg("cir_ex_nokernel",
              "カーネル %1 が見つかりません。環境変数 %2 かカーネルパス設定で"
              "場所を指定してください。",
              "The kernel %1 was not found. Set %2 or configure the kernel path.");
    I18n::reg("cir_ex_nowrite", "入力ファイルを書けませんでした: %1",
              "Could not write the input file: %1");
    I18n::reg("cir_ex_running", "実行中: %1 (導体 %2 本、ポート %3 個)",
              "Running: %1 (%2 conductors, %3 ports)");
    I18n::reg("cir_ex_failed", "抽出に失敗しました (終了コード %1)",
              "Extraction failed (exit code %1)");
    I18n::reg("cir_ex_nocsv",
              "zin.csv が読めませんでした (ログを確認してください)",
              "zin.csv could not be read (check the log)");
    I18n::reg("cir_ex_done", "完了: %1 行の入力インピーダンス (%2)",
              "Done: %1 input-impedance rows (%2)");
    I18n::reg("cir_ex_nolog",
              "ofe.log が読めませんでした (ログを確認してください)",
              "ofe.log could not be read (check the log)");
    I18n::reg("cir_ex_value", "値", "Value");
    I18n::reg("cir_ex_port", "ポート", "Port");
    I18n::reg("cir_ex_freq", "周波数", "Frequency");
    I18n::reg("cir_ex_r", "Rin [Ω]", "Rin [ohm]");
    I18n::reg("cir_ex_x", "Xin [Ω]", "Xin [ohm]");
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
    I18n::reg("cir_estimate", "推定 (目安の例 — 実測ではありません): %1",
              "Estimate (illustrative example, not measured): %1");
    I18n::reg("cir_est_peec", "~2分 (14k要素)", "~2 min (14k elements)");
    I18n::reg("cir_est_femq", "~30秒", "~30 s");
    I18n::reg("cir_est_femw", "~15分 (適応6パス)", "~15 min (6 adaptive passes)");
    I18n::reg("cir_handoff", "FDTD連成", "Hand-off to FDTD");
    I18n::reg("cir_handoff_sp",
              "抽出Sパラメータを FDTD ポートの周波数依存負荷として使用 (連携は未実装)",
              "Use the extracted S-parameters as a frequency-dependent load on the "
              "FDTD ports (hand-off not implemented)");
    I18n::reg("cir_handoff_rom",
              "PEEC等価回路を FDTD 集中定数素子へ縮約 (ROM) (連携は未実装)",
              "Reduce the PEEC equivalent circuit to FDTD lumped elements (ROM) "
              "(hand-off not implemented)");
    I18n::reg("cir_handoff_hint",
              "▸ 基板近傍=PEEC/FEM、筐体放射=FDTD の分担でマルチスケール解析 "
              "(連携は未実装)。",
              "▸ Multi-scale analysis: PEEC/FEM near the board, FDTD for enclosure "
              "radiation (hand-off not implemented).");

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
    I18n::reg("cir_results", "結果 / 集中定数モデルの |Z|",
              "Results / |Z| of the lumped model");
    I18n::reg("cir_col_item", "項目", "Item");
    I18n::reg("cir_res_note",
              "▸ PEEC/FEM による寄生抽出は未実装のため、抽出された R/L/C・"
              "S パラメータはまだありません (表示できる実測値・解析値なし)。"
              "ここに出るのは下で入力した集中定数 RLC の解析式 "
              "Z = R + jωL + 1/(jωC) (直列) / Y = 1/R + 1/(jωL) + jωC (並列) "
              "による値です。抽出を実行すると、この表と曲線は抽出結果で置き換わります。",
              "▸ Parasitic extraction by PEEC/FEM is not implemented, so no "
              "extracted R/L/C or S-parameters exist yet (there is no measured or "
              "computed value to show). What follows is evaluated from the lumped "
              "RLC entered below via Z = R + jωL + 1/(jωC) (series) / "
              "Y = 1/R + 1/(jωL) + jωC (parallel). Once extraction runs, this "
              "table and curve are replaced by the extracted values.");
    I18n::reg("cir_model", "集中定数モデル (入力)", "Lumped model (input)");
    I18n::reg("cir_topology", "構成", "Topology");
    I18n::reg("cir_topo_series", "直列 RLC", "Series RLC");
    I18n::reg("cir_topo_parallel", "並列 RLC", "Parallel RLC");
    I18n::reg("cir_row_r", "R [Ω]", "R [Ω]");
    I18n::reg("cir_row_xl", "ωL [Ω]", "ωL [Ω]");
    I18n::reg("cir_row_xc", "1/ωC [Ω]", "1/ωC [Ω]");
    I18n::reg("cir_row_z", "|Z| [Ω]", "|Z| [Ω]");
    I18n::reg("cir_res_f0", "LC 共振 f0 = %1", "LC resonance f0 = %1");
    I18n::reg("cir_res_f0_none", "LC 共振 f0: — (L と C の両方が必要)",
              "LC resonance f0: — (needs both L and C)");
    I18n::reg("cir_res_loads",
              "プロジェクトの load 行 (集中定数) から初期化しました: R=%1 個, "
              "L=%2 個, C=%3 個",
              "Initialized from the project's load lines (lumped elements): "
              "R=%1, L=%2, C=%3");
    I18n::reg("cir_res_noloads",
              "プロジェクトに load 行が無いため既定値を表示しています "
              "(値は自由に編集できます)。",
              "The project has no load lines, so default values are shown "
              "(they can be edited freely).");
    I18n::reg("cir_exp_snp", "📁 Touchstone .s3p 書出", "📁 Export Touchstone .s3p");
    I18n::reg("cir_exp_spice", "📁 SPICE サブサーキット書出", "📁 Export SPICE subcircuit");
    I18n::reg("cir_exp_h5", "💾 HDF5 保存", "💾 Save HDF5");
    I18n::reg("cir_exp_fdtd", "→ FDTDポートへ適用", "→ Apply to FDTD ports");
    return true;
}();

// ソルバごとの説明文 / 推定時間 (モックの三項演算子をそのまま転記)
const char *kDescKeys[3] = { "cir_desc_peec", "cir_desc_femq", "cir_desc_femw" };
const char *kEstKeys[3]  = { "cir_est_peec", "cir_est_femq", "cir_est_femw" };

// 結果表を評価する周波数 (列見出し @1MHz / @10MHz / @100MHz と対応)
const double kEvalFreqHz[3] = { 1.0e6, 1.0e7, 1.0e8 };

// |Z(f)| 曲線の描画範囲 (log10 f[MHz] = -2 … 2 → 0.01 MHz 〜 100 MHz)
const double kZPlotLogMin = -2.0, kZPlotLogMax = 2.0;
const int    kZPlotPoints = 121;

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
    connect(project, &Project::loaded, this, &CircuitSolversTab::refresh);
    refresh();
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
    auto *fileEdit = numEdit("power_board_v3.odb", 0, sStr);
    fileRow->addWidget(fileEdit, 1);
    // 「📁 参照…」のみ実配線 (選択パスを欄へ反映する。取込処理自体は未実装)
    auto *browseBtn = new QPushButton(I18n::tr("cir_browse"), sStr);
    connect(browseBtn, &QPushButton::clicked, this, [this, fileEdit] {
        const QString path = QFileDialog::getOpenFileName(
            this, I18n::tr("cir_file"), fileEdit->text());
        if (!path.isEmpty()) fileEdit->setText(path);
    });
    fileRow->addWidget(browseBtn);
    sStr->form()->addRow(I18n::tr("cir_file"), fileRow);

    auto *optRow = new QHBoxLayout();
    optRow->addWidget(check(I18n::tr("cir_cond_only"), true, sStr));
    optRow->addWidget(check(I18n::tr("cir_via_auto"), true, sStr));
    optRow->addStretch(1);
    sStr->form()->addRow(optRow);
    v->addWidget(sStr);

    // ポート定義 / Ports — Project::circuitPorts() のビュー。
    // 編集は即座にモデルへ書き戻し .ofdx ("circuit.ports") へ保存される。
    auto *sPort = new SectionBox(I18n::tr("cir_ports"), page);
    m_portTable = new QTableWidget(0, 9, sPort);
    m_portTable->setHorizontalHeaderLabels({ QString(), "#", I18n::tr("cir_col_name"),
                                             I18n::tr("cir_col_kind"),
                                             I18n::tr("cir_col_net"),
                                             I18n::tr("cir_col_ref"),
                                             I18n::tr("cir_col_p1"),
                                             I18n::tr("cir_col_p2"),
                                             I18n::tr("cir_col_z0") });
    m_portTable->verticalHeader()->setVisible(false);
    m_portTable->verticalHeader()->setDefaultSectionSize(24);
    m_portTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_portTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_portTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_portTable->setMaximumHeight(160);

    m_mono.setStyleHint(QFont::Monospace);
    // 実在するファミリのみ指定 (Theme が環境ごとに解決済み)
    if (const QString mf = Theme::monoFontFamily(); !mf.isEmpty())
        m_mono.setFamily(mf);

    sPort->vbox()->addWidget(m_portTable);
    sPort->vbox()->addWidget(hintLabel(I18n::tr("cir_port_hint"), sPort));

    auto *portBtns = new QHBoxLayout();
    auto *addBtn = new QPushButton(I18n::tr("cir_port_add"), sPort);
    auto *delBtn = new QPushButton(I18n::tr("cir_port_del"), sPort);
    portBtns->addWidget(addBtn);
    portBtns->addWidget(delBtn);
    portBtns->addStretch(1);
    sPort->vbox()->addLayout(portBtns);
    v->addWidget(sPort);

    connect(addBtn, &QPushButton::clicked, this, [this] {
        QVector<CircuitPortRow> &ports = m_p->circuitPorts();
        CircuitPortRow r;
        r.name = QStringLiteral("PORT%1").arg(ports.size() + 1);
        r.ref = QStringLiteral("GND");
        ports.push_back(r);
        m_p->touch();
        refreshPorts();
        m_portTable->setCurrentCell(m_portTable->rowCount() - 1, 2);
    });
    connect(delBtn, &QPushButton::clicked, this, [this] {
        const int row = m_portTable->currentRow();
        QVector<CircuitPortRow> &ports = m_p->circuitPorts();
        if (row < 0 || row >= ports.size()) return;
        ports.remove(row);
        m_p->touch();
        refreshPorts();
    });
    connect(m_portTable, &QTableWidget::itemChanged,
            this, &CircuitSolversTab::onPortItemChanged);

    v->addStretch(1);
    return page;
}

// モデル → ポート表 (行数が変わるので毎回作り直す)
void CircuitSolversTab::refreshPorts()
{
    m_updating = true;
    const QVector<CircuitPortRow> &ports = m_p->circuitPorts();
    m_portTable->setRowCount(ports.size());
    for (int r = 0; r < ports.size(); ++r) {
        const CircuitPortRow &p = ports[r];

        auto *sel = new QTableWidgetItem();
        sel->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        sel->setCheckState(p.enabled ? Qt::Checked : Qt::Unchecked);
        m_portTable->setItem(r, 0, sel);

        auto *num = new QTableWidgetItem(QString::number(r + 1));
        num->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        num->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        m_portTable->setItem(r, 1, num);

        m_portTable->setItem(r, 2, new QTableWidgetItem(p.name));
        // 端子座標は "x, y, z" のテキストで編集する (3 列に割らない —
        // 表が横に伸びすぎるため)。未設定 (両端が同じ) は空欄で出す。
        auto xyz = [](double x, double y, double z) {
            return QStringLiteral("%1, %2, %3")
                .arg(x, 0, 'g', 8).arg(y, 0, 'g', 8).arg(z, 0, 'g', 8);
        };
        const bool set = p.hasEndpoints();
        m_portTable->setItem(r, 6, new QTableWidgetItem(
            set ? xyz(p.x1_m, p.y1_m, p.z1_m) : QString()));
        m_portTable->setItem(r, 7, new QTableWidgetItem(
            set ? xyz(p.x2_m, p.y2_m, p.z2_m) : QString()));
        m_portTable->setItem(r, 8, new QTableWidgetItem(
            QString::number(p.z0_ohm, 'g', 6)));

        // 種類は 2 択なのでセル内コンボ (行を作り直すたびに張り替える)
        auto *kind = new QComboBox(m_portTable);
        kind->addItem(I18n::tr("cir_kind_lumped"));
        kind->addItem(I18n::tr("cir_kind_probe"));
        kind->setCurrentIndex(qBound(0, p.kind, 1));
        connect(kind, &QComboBox::currentIndexChanged, this, [this, r](int idx) {
            if (m_updating) return;
            QVector<CircuitPortRow> &v = m_p->circuitPorts();
            if (r < 0 || r >= v.size()) return;
            v[r].kind = idx;
            m_p->touch();
        });
        m_portTable->setCellWidget(r, 3, kind);

        auto *net = new QTableWidgetItem(p.net);
        net->setFont(m_mono);
        m_portTable->setItem(r, 4, net);
        auto *ref = new QTableWidgetItem(p.ref);
        ref->setFont(m_mono);
        m_portTable->setItem(r, 5, ref);
    }
    m_updating = false;
}

// ポート表 → モデル (1 セル分)
void CircuitSolversTab::onPortItemChanged(QTableWidgetItem *item)
{
    if (m_updating || !item) return;
    QVector<CircuitPortRow> &ports = m_p->circuitPorts();
    const int row = item->row();
    if (row < 0 || row >= ports.size()) return;
    switch (item->column()) {
    case 0: ports[row].enabled = (item->checkState() == Qt::Checked); break;
    case 2: ports[row].name = item->text(); break;
    case 4: ports[row].net = item->text(); break;
    case 5: ports[row].ref = item->text(); break;
    case 6: case 7: {
        // "x, y, z" を読む。3 個そろわない入力は **モデルへ書かず**
        // 表示を元へ戻す (UI と保存内容が食い違わないように — gui.md)
        static const QRegularExpression sep(QStringLiteral("[,\\s]+"));
        const QStringList t =
            item->text().trimmed().split(sep, Qt::SkipEmptyParts);
        double v[3] = {};
        bool ok = (t.size() == 3);
        for (int i = 0; ok && i < 3; ++i) {
            bool good = false;
            v[i] = t[i].toDouble(&good);
            ok = ok && good;
        }
        if (!ok) { refreshPorts(); return; }
        if (item->column() == 6) {
            ports[row].x1_m = v[0]; ports[row].y1_m = v[1]; ports[row].z1_m = v[2];
        } else {
            ports[row].x2_m = v[0]; ports[row].y2_m = v[1]; ports[row].z2_m = v[2];
        }
        break;
    }
    case 8: {
        bool ok = false;
        const double z0 = item->text().toDouble(&ok);
        if (!ok || z0 <= 0.0) { refreshPorts(); return; }
        ports[row].z0_ohm = z0;
        break;
    }
    default: return;
    }
    m_p->touch();
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
    // 抽出設定フォームはどこにも読まれていない (未実装)
    sExt->vbox()->addWidget(tabhelp::unwiredNote(sExt));

    auto *runRow = new QHBoxLayout();
    m_runExtract = new QPushButton(I18n::tr("cir_run_extract"), sExt);
    connect(m_runExtract, &QPushButton::clicked,
            this, &CircuitSolversTab::runExtraction);
    runRow->addWidget(m_runExtract);
    m_estimate = new QLabel(sExt);
    runRow->addWidget(m_estimate);
    runRow->addStretch(1);
    sExt->vbox()->addLayout(runRow);
    m_extractStatus = new QLabel(I18n::tr("cir_ex_idle"), sExt);
    m_extractStatus->setWordWrap(true);
    sExt->vbox()->addWidget(m_extractStatus);
    m_zinTable = new QTableWidget(0, 4, sExt);
    m_zinTable->setHorizontalHeaderLabels(
        { I18n::tr("cir_ex_port"), I18n::tr("cir_ex_freq"),
          I18n::tr("cir_ex_r"), I18n::tr("cir_ex_x") });
    m_zinTable->horizontalHeader()->setStretchLastSection(true);
    m_zinTable->verticalHeader()->setVisible(false);
    m_zinTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_zinTable->setMinimumHeight(150);
    m_zinTable->setVisible(false);
    sExt->vbox()->addWidget(m_zinTable);
    m_extractLog = new QPlainTextEdit(sExt);
    m_extractLog->setReadOnly(true);
    m_extractLog->setMaximumHeight(120);
    m_extractLog->setVisible(false);
    sExt->vbox()->addWidget(m_extractLog);
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
    auto *netEdit = numEdit("buck_converter.cir", 0, s);
    netRow->addWidget(netEdit, 1);
    // 「📁 参照…」のみ実配線 (選択パスを欄へ反映する。共シミュレーションは未実装)
    auto *netBrowse = new QPushButton(I18n::tr("cir_browse"), s);
    connect(netBrowse, &QPushButton::clicked, this, [this, netEdit] {
        const QString path = QFileDialog::getOpenFileName(
            this, I18n::tr("cir_netlist"), netEdit->text());
        if (!path.isEmpty()) netEdit->setText(path);
    });
    netRow->addWidget(netBrowse);
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

    // 設定フォームはどこにも読まれていない (未実装)
    s->vbox()->addWidget(tabhelp::unwiredNote(s));

    auto *runRow = new QHBoxLayout();
    auto *runBtn = new QPushButton(I18n::tr("cir_run_spice"), s);
    tabhelp::markNotImplemented(runBtn);   // 共シミュレーション実行は未配線 (絶対規則 5)
    runRow->addWidget(runBtn);
    runRow->addStretch(1);
    s->vbox()->addLayout(runRow);
    v->addWidget(s);

    v->addStretch(1);
    return page;
}

// ── 結果 (集中定数モデルの |Z| — 抽出は未実装) ──────────────────────────────
QWidget *CircuitSolversTab::buildResultsPage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setSpacing(8);

    auto *s = new SectionBox(I18n::tr("cir_results"), page);
    // 抽出は未実行 (未実装) — 何が無くて何を表示しているのかを明示する
    s->vbox()->addWidget(hintLabel(I18n::tr("cir_res_note"), s));

    // 集中定数モデルの入力 (プロジェクトの load 行があればそこから初期化)
    auto *modelForm = new QFormLayout();
    modelForm->setContentsMargins(0, 0, 0, 0);
    m_rlcTopology = new QComboBox(s);
    m_rlcTopology->addItem(I18n::tr("cir_topo_series"));
    m_rlcTopology->addItem(I18n::tr("cir_topo_parallel"));
    modelForm->addRow(I18n::tr("cir_topology"), m_rlcTopology);

    auto *rlcRow = new QHBoxLayout();
    m_rlcR = numEdit("0.01", 80, s);
    m_rlcL = numEdit("50", 80, s);
    m_rlcC = numEdit("200", 80, s);
    rlcRow->addWidget(new QLabel("R [Ω]", s));
    rlcRow->addWidget(m_rlcR);
    rlcRow->addWidget(new QLabel("L [nH]", s));
    rlcRow->addWidget(m_rlcL);
    rlcRow->addWidget(new QLabel("C [pF]", s));
    rlcRow->addWidget(m_rlcC);
    rlcRow->addStretch(1);
    modelForm->addRow(I18n::tr("cir_model"), rlcRow);
    s->vbox()->addLayout(modelForm);

    m_rlcSource = hintLabel(QString(), s);
    s->vbox()->addWidget(m_rlcSource);

    m_resultTable = new QTableWidget(4, 4, s);
    m_resultTable->setHorizontalHeaderLabels({ I18n::tr("cir_col_item"), "@1MHz",
                                               "@10MHz", "@100MHz" });
    m_resultTable->verticalHeader()->setVisible(false);
    m_resultTable->verticalHeader()->setDefaultSectionSize(24);
    m_resultTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_resultTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultTable->setMaximumHeight(140);
    const char *rowKeys[4] = { "cir_row_r", "cir_row_xl", "cir_row_xc",
                               "cir_row_z" };
    for (int r = 0; r < 4; ++r) {
        m_resultTable->setItem(r, 0, new QTableWidgetItem(I18n::tr(rowKeys[r])));
        for (int c = 0; c < 3; ++c) {
            auto *it = new QTableWidgetItem(QStringLiteral("—"));
            it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            m_resultTable->setItem(r, c + 1, it);
        }
    }
    s->vbox()->addWidget(m_resultTable);

    m_resonance = hintLabel(QString(), s);
    s->vbox()->addWidget(m_resonance);

    // |Z(f)| — 集中定数モデルの解析式 (log10 f[MHz] 軸、縦軸 dBΩ)
    m_zPlot = new MiniPlot(s);
    m_zPlot->setLabels("f [MHz]", "20log10|Z| [dBΩ]");
    m_zPlot->setXTickPow10(true);        // x は log10 値 → 目盛りは実周波数
    m_zPlot->setMinimumSize(340, 120);
    s->vbox()->addWidget(m_zPlot);

    for (QLineEdit *e : { m_rlcR, m_rlcL, m_rlcC })
        connect(e, &QLineEdit::textChanged, this,
                &CircuitSolversTab::updateResults);
    connect(m_rlcTopology, &QComboBox::currentIndexChanged,
            this, &CircuitSolversTab::updateResults);

    // 書出/適用ボタンはいずれも未配線 (絶対規則 5)
    auto *btnRow = new QHBoxLayout();
    auto *expSnp   = new QPushButton(I18n::tr("cir_exp_snp"), s);
    auto *expSpice = new QPushButton(I18n::tr("cir_exp_spice"), s);
    auto *expH5    = new QPushButton(I18n::tr("cir_exp_h5"), s);
    auto *expFdtd  = new QPushButton(I18n::tr("cir_exp_fdtd"), s);
    for (QPushButton *b : { expSnp, expSpice, expH5, expFdtd }) {
        tabhelp::markNotImplemented(b);
        btnRow->addWidget(b);
    }
    btnRow->addStretch(1);
    s->vbox()->addLayout(btnRow);
    v->addWidget(s);

    v->addStretch(1);
    return page;
}

// ── モデル → 集中定数フォーム (プロジェクトの load 行から初期化) ────────────
// .ofd の `load = dir x y z R|L|C value` は集中定数素子そのものなので、
// 定義されていれば最初の R/L/C をモデルの初期値に使う (無ければ既定値)。
void CircuitSolversTab::refresh()
{
    m_updating = true;
    refreshPorts();

    int nR = 0, nL = 0, nC = 0;
    double r = -1, l = -1, c = -1;
    for (const Load &ld : m_p->loads()) {
        const QChar k = ld.kind.toUpper();
        if (k == QLatin1Char('R')) { if (nR++ == 0) r = ld.value; }
        else if (k == QLatin1Char('L')) { if (nL++ == 0) l = ld.value; }
        else if (k == QLatin1Char('C')) { if (nC++ == 0) c = ld.value; }
    }
    if (nR + nL + nC > 0) {
        // load が 1 つでもあれば、その値で埋める (欠けている種類は 0 = 素子なし)
        m_rlcR->setText(QString::number(r >= 0 ? r : 0.0, 'g', 6));
        m_rlcL->setText(QString::number(l >= 0 ? l * 1e9 : 0.0, 'g', 6));
        m_rlcC->setText(QString::number(c >= 0 ? c * 1e12 : 0.0, 'g', 6));
        m_rlcSource->setText(I18n::tr("cir_res_loads")
                                 .arg(nR).arg(nL).arg(nC));
    } else {
        m_rlcSource->setText(I18n::tr("cir_res_noloads"));
    }
    m_updating = false;
    updateResults();
}

// ── 集中定数モデル → 表 + |Z(f)| 曲線 (解析式、em/LumpedRlc) ────────────────
void CircuitSolversTab::updateResults()
{
    em::RlcModel m;
    m.r_ohm = m_rlcR->text().toDouble();          // [Ω]
    m.l_H   = m_rlcL->text().toDouble() * 1e-9;   // nH → H
    m.c_F   = m_rlcC->text().toDouble() * 1e-12;  // pF → F
    m.topology = (m_rlcTopology->currentIndex() == 1) ? em::RlcTopology::Parallel
                                                      : em::RlcTopology::Series;

    auto cell = [](double v) {
        return (v > 0) ? QString::number(v, 'g', 4) : QStringLiteral("—");
    };
    for (int c = 0; c < 3; ++c) {
        const em::RlcImpedance z = em::rlcImpedance(m, kEvalFreqHz[c]);
        const QString vals[4] = {
            (m.r_ohm > 0) ? QString::number(m.r_ohm, 'g', 4) : QStringLiteral("—"),
            z.valid ? cell(z.xL_ohm) : QStringLiteral("—"),
            z.valid ? cell(z.xC_ohm) : QStringLiteral("—"),
            z.valid ? QString::number(z.magnitude_ohm, 'g', 4)
                    : QStringLiteral("—"),
        };
        for (int r = 0; r < 4; ++r)
            if (auto *it = m_resultTable->item(r, c + 1)) it->setText(vals[r]);
    }

    const double f0 = em::rlcResonanceHz(m.l_H, m.c_F);
    m_resonance->setText(f0 > 0
        ? I18n::tr("cir_res_f0")
              .arg(QString::number(f0 * 1e-6, 'f', 3) + QStringLiteral(" MHz"))
        : I18n::tr("cir_res_f0_none"));

    // |Z(f)| を dBΩ で描く (0.01 MHz 〜 100 MHz の対数軸)。
    // |Z| = 0 (完全短絡) の点は dB に落とせないので打たない。
    MiniSeries z;
    z.color = QColor("#0078D4");
    for (int i = 0; i < kZPlotPoints; ++i) {
        const double lg = kZPlotLogMin
                        + (kZPlotLogMax - kZPlotLogMin) * i / (kZPlotPoints - 1);
        const double f_Hz = std::pow(10.0, lg) * 1e6;
        const em::RlcImpedance zi = em::rlcImpedance(m, f_Hz);
        if (!zi.valid || zi.magnitude_ohm <= 0) continue;
        // x は log10(f[MHz])。MiniPlot::setXTickPow10 が 10^x = MHz で目盛る
        z.pts.push_back({ lg, 20.0 * std::log10(zi.magnitude_ohm) });
    }
    m_zPlot->setSeries({ z });
}

// ── 抽出実行 (OpenPEEC / OpenFEM) ───────────────────────────────────────────
// 入力を作業ディレクトリへ書き、QProcess で起動して zin.csv を読む。
// 入力が作れない場合は **起動せず理由を出す** (絶対規則 5 / gui.md)。
void CircuitSolversTab::runExtraction()
{
    if (m_proc && m_proc->state() != QProcess::NotRunning) return;

    const int kind = m_solver ? m_solver->currentIndex() : 0;
    const Kernel kernel = (kind == 0) ? Kernel::PEEC : Kernel::FEM;
    const CircuitInput in = (kind == 0) ? CircuitIO::peecText(*m_p)
                                        : CircuitIO::femText(*m_p);
    m_zinTable->setVisible(false);
    m_extractLog->setVisible(false);
    if (!in.isValid()) {
        m_extractStatus->setText(I18n::tr("cir_ex_noinput").arg(in.reason));
        return;
    }
    RunConfig cfg;
    cfg.kernel = kernel;
    const QString bin = Runner::resolvedSolverPath(cfg);
    if (bin.isEmpty()) {
        m_extractStatus->setText(
            I18n::tr("cir_ex_nokernel")
                .arg(kind == 0 ? QStringLiteral("peec") : QStringLiteral("ofe"))
                .arg(QLatin1String(Runner::homeVarFor(kernel))));
        return;
    }

    // 作業ディレクトリ: プロジェクトの隣 (無題ならテンポラリ)
    m_runDir = m_p->filePath().isEmpty()
                   ? QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                         + QStringLiteral("/openfdtd-x/circuit")
                   : QFileInfo(m_p->filePath()).path() + QStringLiteral("/circuit_run");
    QDir().mkpath(m_runDir);
    const QString base = CircuitIO::caseName(*m_p);
    const QString inPath = m_runDir + QLatin1Char('/') + base
                           + (kind == 0 ? QStringLiteral(".peec")
                                        : QStringLiteral(".ofe"));
    // 前回の結果を消す (残骸を今回の結果として拾わない)
    QFile::remove(m_runDir + QStringLiteral("/zin.csv"));
    QFile f(inPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_extractStatus->setText(I18n::tr("cir_ex_nowrite").arg(f.errorString()));
        return;
    }
    f.write(in.text.toUtf8());
    f.close();

    QString head = I18n::tr("cir_ex_running")
                       .arg(QFileInfo(inPath).fileName())
                       .arg(in.conductors).arg(in.ports);
    for (const QString &w : in.warnings)
        head += QStringLiteral("\n• ") + w;
    m_extractStatus->setText(head);

    if (!m_proc) {
        m_proc = new QProcess(this);
        m_proc->setProcessChannelMode(QProcess::MergedChannels);
        connect(m_proc, &QProcess::readyReadStandardOutput, this, [this] {
            m_extractLog->setVisible(true);
            m_extractLog->appendPlainText(
                QString::fromLocal8Bit(m_proc->readAllStandardOutput()).trimmed());
        });
        connect(m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this](int code, QProcess::ExitStatus) {
                    onExtractionFinished(code);
                });
    }
    m_runExtract->setEnabled(false);
    m_proc->setWorkingDirectory(m_runDir);
    m_proc->start(bin, { QFileInfo(inPath).fileName() });
}

void CircuitSolversTab::onExtractionFinished(int exitCode)
{
    m_runExtract->setEnabled(true);
    if (exitCode != 0) {
        m_extractStatus->setText(I18n::tr("cir_ex_failed").arg(exitCode));
        m_extractLog->setVisible(true);
        return;
    }
    // PEEC は zin.csv、FEM は ofe.log に結果が出る
    if (m_solver && m_solver->currentIndex() != 0) {
        showFemLog(m_runDir + QStringLiteral("/ofe.log"));
        return;
    }
    showZinCsv(m_runDir + QStringLiteral("/zin.csv"));
}

// zin.csv: port, frequency[Hz], Rin[ohm], Xin[ohm], Gin[mS], Bin[mS], Zabs[ohm]
void CircuitSolversTab::showZinCsv(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_extractStatus->setText(I18n::tr("cir_ex_nocsv"));
        return;
    }
    const QStringList lines = QString::fromUtf8(f.readAll()).split(QLatin1Char('\n'));
    m_zinTable->setRowCount(0);
    m_zinTable->setColumnCount(4);
    m_zinTable->setHorizontalHeaderLabels(
        { I18n::tr("cir_ex_port"), I18n::tr("cir_ex_freq"),
          I18n::tr("cir_ex_r"), I18n::tr("cir_ex_x") });
    int rows = 0;
    for (const QString &line : lines) {
        const QStringList t = line.split(QLatin1Char(','));
        if (t.size() < 4) continue;
        bool okF = false, okR = false, okX = false;
        const double fr = t[1].toDouble(&okF);
        const double re = t[2].toDouble(&okR);
        const double im = t[3].toDouble(&okX);
        if (!okF || !okR || !okX) continue;   // ヘッダ行
        const int r = m_zinTable->rowCount();
        m_zinTable->insertRow(r);
        m_zinTable->setItem(r, 0, new QTableWidgetItem(t[0].trimmed()));
        m_zinTable->setItem(r, 1, new QTableWidgetItem(
            QString::number(fr / 1e6, 'g', 6) + QStringLiteral(" MHz")));
        m_zinTable->setItem(r, 2, new QTableWidgetItem(QString::number(re, 'g', 6)));
        // Xin から等価 L / C も出す (X>0 は誘導性、X<0 は容量性)
        const double L = (fr > 0 && im > 0) ? im / (2 * M_PI * fr) : 0.0;
        const double C = (fr > 0 && im < 0) ? -1.0 / (2 * M_PI * fr * im) : 0.0;
        QString x = QString::number(im, 'g', 6);
        if (L > 0) x += QStringLiteral("  (L = %1 nH)").arg(L * 1e9, 0, 'g', 5);
        if (C > 0) x += QStringLiteral("  (C = %1 pF)").arg(C * 1e12, 0, 'g', 5);
        m_zinTable->setItem(r, 3, new QTableWidgetItem(x));
        ++rows;
    }
    m_zinTable->setVisible(rows > 0);
    m_extractStatus->setText(rows > 0
        ? I18n::tr("cir_ex_done").arg(rows).arg(QDir::toNativeSeparators(m_runDir))
        : I18n::tr("cir_ex_nocsv"));
}

// OpenFEM の結果は ofe.log の表 (C / L / R / Z0 / eps_eff) に出る。
// 「= 数値 [単位]」の行だけを拾って項目表にする。
void CircuitSolversTab::showFemLog(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_extractStatus->setText(I18n::tr("cir_ex_nolog"));
        return;
    }
    static const QRegularExpression re(
        QStringLiteral("^\\s*([A-Za-z_][\\w\\[\\]/,]*)\\s*=\\s*"
                       "([-+0-9.eE]+)\\s*(\\[[^\\]]*\\])?\\s*$"));
    m_zinTable->setRowCount(0);
    m_zinTable->setColumnCount(2);
    m_zinTable->setHorizontalHeaderLabels(
        { I18n::tr("cir_col_item"), I18n::tr("cir_ex_value") });
    int rows = 0;
    for (const QString &line :
         QString::fromUtf8(f.readAll()).split(QLatin1Char('\n'))) {
        const auto m = re.match(line);
        if (!m.hasMatch()) continue;
        const int r = m_zinTable->rowCount();
        m_zinTable->insertRow(r);
        m_zinTable->setItem(r, 0, new QTableWidgetItem(m.captured(1)));
        m_zinTable->setItem(r, 1, new QTableWidgetItem(
            (m.captured(2) + QLatin1Char(' ') + m.captured(3)).trimmed()));
        ++rows;
    }
    m_zinTable->setVisible(rows > 0);
    m_extractStatus->setText(rows > 0
        ? I18n::tr("cir_ex_done").arg(rows).arg(QDir::toNativeSeparators(m_runDir))
        : I18n::tr("cir_ex_nolog"));
}
