// AcousticSolverTab.cpp
#include "AcousticSolverTab.h"
#include "../core/Project.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"
#include "TabHelpers.h"

#include <QComboBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

using namespace ofd;
using namespace ofd::tabhelp;

// ── タブ固有語彙 (acs_) — file-local 登録 ───────────────────────────────────
namespace {
const bool s_i18n = [] {
    I18n::reg("acs_sec_backend",
        "外部音響ソルバー連携 (実装: AcousticRunner — QProcess 疎結合)",
        "External acoustic solver (AcousticRunner — loosely-coupled QProcess)");
    I18n::reg("acs_backend", "RIRの取得元", "RIR source");
    I18n::reg("acs_b_none", "None — RIR取得なし (統計推定のみ)",
              "None — no RIR (statistical estimate only)");
    I18n::reg("acs_b_measured", "MeasuredRir — 実測RIR (RIR分析タブの従来経路)",
              "MeasuredRir — measured RIR (RIR analysis tab)");
    I18n::reg("acs_b_stat", "Statistical — 統計モデルからの合成RIR",
              "Statistical — synthetic RIR from statistical model");
    I18n::reg("acs_b_fdtd", "ExternalFDTD — 外部音響FDTDソルバー",
              "ExternalFDTD — external acoustic FDTD solver");
    I18n::reg("acs_b_geo", "ExternalGeometric — 外部幾何音響 (レイトレース系)",
              "ExternalGeometric — external geometric acoustics (ray tracing)");
    I18n::reg("acs_backend_note",
        "▸ 外部プロセスを起動するのは ExternalFDTD / ExternalGeometric のみ。"
        "`.ofdx` に opera_analysis.solver.backend (int) で永続化。",
        "▸ Only ExternalFDTD / ExternalGeometric launch an external process. "
        "Persisted to .ofdx as opera_analysis.solver.backend (int).");
    I18n::reg("acs_launch", "起動形式", "Invocation");
    I18n::reg("acs_binary", "バイナリ", "Binary");
    I18n::reg("acs_binary_ph", "(空 = 自動解決)", "(empty = auto-resolve)");
    I18n::reg("acs_browse", "📁 参照…", "📁 Browse…");
    I18n::reg("acs_parallel", "並列", "Parallelism");
    I18n::reg("acs_threads", "threads", "threads");
    I18n::reg("acs_procs", "processes", "processes");
    I18n::reg("acs_procs_note", "(>1 で mpiexec -n)", "(>1 uses mpiexec -n)");
    I18n::reg("acs_run", "▶ 実行", "▶ Run");
    I18n::reg("acs_stop", "■ 停止", "■ Stop");
    I18n::reg("acs_progress_note", "進捗は stdout の \"progress a/b\" 行を解析",
              "Progress parsed from stdout \"progress a/b\" lines");
    I18n::reg("acs_resolved", "解決結果", "Resolved binary");
    I18n::reg("acs_resolved_none",
        "⚠ ソルバーが見つかりません — 見つからなければ起動せず finished(false)",
        "⚠ Solver not found — start is refused with finished(false)");
    I18n::reg("acs_sec_resolve",
        "バイナリ探索順 / Solver resolution (ADR-0007 Decision 3)",
        "Solver resolution order (ADR-0007 Decision 3)");
    I18n::reg("acs_col_order", "#", "#");
    I18n::reg("acs_col_where", "探索場所", "Location");
    I18n::reg("acs_col_use", "用途", "Purpose");
    I18n::reg("acs_sec_contract", "出力契約 / Output contract (docs/adr/0007)",
              "Output contract (docs/adr/0007)");
    I18n::reg("acs_col_file", "ファイル", "File");
    I18n::reg("acs_col_required", "必須", "Required");
    I18n::reg("acs_col_content", "内容", "Content");
    I18n::reg("acs_req_yes", "必須", "Required");
    I18n::reg("acs_req_no", "任意", "Optional");
    I18n::reg("acs_contract_note",
        "▸ 契約検証後に rirReady(path) → RIR分析タブの実測RIRに設定される。"
        "FDTD推定は AcousticFdtdEstimator (格子/時間/メモリ見積) を使用。",
        "▸ After contract validation, rirReady(path) sets the measured RIR of "
        "the RIR analysis tab. FDTD sizing uses AcousticFdtdEstimator.");
    I18n::reg("acs_dev_note",
        "実音響ソルバーは別リポジトリで開発中 — CI はモックソルバーで出力契約を"
        "検証している (docs/opera-acoustics-development-status.md §3)。",
        "The real acoustic solver is developed in a separate repository — CI "
        "validates the output contract with a mock solver.");
    I18n::reg("acs_status_idle", "待機中", "Idle");
    I18n::reg("acs_status_running", "実行中…", "Running…");
    I18n::reg("acs_status_done_ok", "✓ 正常終了 — RIR を受領し RIR分析タブに設定しました",
              "✓ Finished — RIR received and set on the RIR analysis tab");
    I18n::reg("acs_status_done_ng", "✗ 失敗 (ログを確認)", "✗ Failed (see log)");
    I18n::reg("acs_log", "実行ログ", "Run log");
    return true;
}();

QTableWidget *makeTable(QWidget *parent, const QStringList &headers)
{
    auto *t = new QTableWidget(0, headers.size(), parent);
    t->setHorizontalHeaderLabels(headers);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    t->horizontalHeader()->setStretchLastSection(true);
    t->verticalHeader()->setVisible(false);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionMode(QAbstractItemView::NoSelection);
    return t;
}

void fitTable(QTableWidget *t)
{
    t->resizeRowsToContents();
    int h = t->horizontalHeader()->height() + 2;
    for (int r = 0; r < t->rowCount(); ++r) h += t->rowHeight(r);
    t->setFixedHeight(h + 4);
}

// 現在の設定から AcousticRunConfig を作る
AcousticRunConfig configFrom(const OperaAcousticSettings &s)
{
    AcousticRunConfig cfg;
    cfg.backend = static_cast<AcousticBackend>(s.solverBackend);
    cfg.executable = s.solverExecutable;
    cfg.threads = s.solverThreads;
    cfg.processes = s.solverProcesses;
    return cfg;
}

} // namespace

// ── construction ────────────────────────────────────────────────────────────
AcousticSolverTab::AcousticSolverTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    m_runner = new AcousticRunner(this);

    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // ── バックエンド選択 + 実行設定 ─────────────────────────────────────────
    auto *s1 = new SectionBox(I18n::tr("acs_sec_backend"), body);
    m_backend = new QComboBox(s1);
    // AcousticBackend 5 値・並び固定 (kernel/AcousticRunner.h)
    m_backend->addItem(I18n::tr("acs_b_none"));
    m_backend->addItem(I18n::tr("acs_b_measured"));
    m_backend->addItem(I18n::tr("acs_b_stat"));
    m_backend->addItem(I18n::tr("acs_b_fdtd"));
    m_backend->addItem(I18n::tr("acs_b_geo"));
    s1->form()->addRow(I18n::tr("acs_backend"), m_backend);
    auto *bnote = new QLabel(I18n::tr("acs_backend_note"), s1);
    bnote->setWordWrap(true);
    s1->vbox()->addWidget(bnote);

    // 外部プロセス設定 (ExternalFDTD / ExternalGeometric のみ表示)
    m_extGroup = new QWidget(s1);
    auto *ev = new QVBoxLayout(m_extGroup);
    ev->setContentsMargins(0, 0, 0, 0);
    auto *launch = new QLabel(
        QStringLiteral("solver <working_dir> [<input_file>]"), m_extGroup);
    launch->setStyleSheet("font-family:monospace;");
    auto *lr = new QHBoxLayout();
    lr->addWidget(new QLabel(I18n::tr("acs_launch"), m_extGroup));
    lr->addWidget(launch);
    lr->addStretch(1);
    ev->addLayout(lr);

    auto *br = new QHBoxLayout();
    br->addWidget(new QLabel(I18n::tr("acs_binary"), m_extGroup));
    m_execPath = new QLineEdit(m_extGroup);
    m_execPath->setPlaceholderText(I18n::tr("acs_binary_ph"));
    br->addWidget(m_execPath, 1);
    auto *btnBrowse = new QPushButton(I18n::tr("acs_browse"), m_extGroup);
    br->addWidget(btnBrowse);
    ev->addLayout(br);

    auto *pr = new QHBoxLayout();
    pr->addWidget(new QLabel(I18n::tr("acs_parallel"), m_extGroup));
    pr->addWidget(new QLabel(I18n::tr("acs_threads"), m_extGroup));
    m_threads = new QSpinBox(m_extGroup);
    m_threads->setRange(1, 256);
    m_threads->setValue(4);
    pr->addWidget(m_threads);
    pr->addWidget(new QLabel(QStringLiteral("(OMP_NUM_THREADS) ·"), m_extGroup));
    pr->addWidget(new QLabel(I18n::tr("acs_procs"), m_extGroup));
    m_processes = new QSpinBox(m_extGroup);
    m_processes->setRange(1, 256);
    m_processes->setValue(1);
    pr->addWidget(m_processes);
    pr->addWidget(new QLabel(I18n::tr("acs_procs_note"), m_extGroup));
    pr->addStretch(1);
    ev->addLayout(pr);

    // 解決結果のライブ表示 (実環境の探索結果 — サンプル値ではない)
    auto *rr = new QHBoxLayout();
    rr->addWidget(new QLabel(I18n::tr("acs_resolved"), m_extGroup));
    m_resolved = new QLabel(m_extGroup);
    m_resolved->setWordWrap(true);
    m_resolved->setStyleSheet("font-family:monospace;");
    rr->addWidget(m_resolved, 1);
    ev->addLayout(rr);

    auto *runRow = new QHBoxLayout();
    m_btnRun = new QPushButton(I18n::tr("acs_run"), m_extGroup);
    m_btnStop = new QPushButton(I18n::tr("acs_stop"), m_extGroup);
    m_btnStop->setEnabled(false);
    runRow->addWidget(m_btnRun);
    runRow->addWidget(m_btnStop);
    runRow->addWidget(new QLabel(I18n::tr("acs_progress_note"), m_extGroup));
    m_progress = new QProgressBar(m_extGroup);
    m_progress->setRange(0, 100);
    m_progress->setVisible(false);
    runRow->addWidget(m_progress);
    runRow->addStretch(1);
    m_status = new QLabel(I18n::tr("acs_status_idle"), m_extGroup);
    runRow->addWidget(m_status);
    ev->addLayout(runRow);

    m_log = new QPlainTextEdit(m_extGroup);
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(2000);
    m_log->setFixedHeight(120);
    m_log->setPlaceholderText(I18n::tr("acs_log"));
    ev->addWidget(m_log);
    s1->vbox()->addWidget(m_extGroup);
    v->addWidget(s1);

    // ── 探索順 (静的事実 — ADR-0007 Decision 3) ─────────────────────────────
    auto *s2 = new SectionBox(I18n::tr("acs_sec_resolve"), body);
    auto *resTable = makeTable(s2, { I18n::tr("acs_col_order"),
        I18n::tr("acs_col_where"), I18n::tr("acs_col_use") });
    const char *kOrder[][3] = {
        { "1", "cfg.executable (明示指定)", "最優先" },
        { "2", "$OFDX_ACOUSTIC_SOLVER", "絶対パス直接指定 (CI/開発オーバーライド)" },
        { "3", "$OPENFDTD_ACOUSTICS_HOME 配下", "導入先指定" },
        { "4", "アプリ実行ディレクトリ kernel/", "同梱配置" },
        { "5", "PATH", "最後" },
    };
    for (const auto &row : kOrder) {
        const int r = resTable->rowCount();
        resTable->insertRow(r);
        for (int c = 0; c < 3; ++c)
            resTable->setItem(r, c, roItem(QString::fromUtf8(row[c])));
    }
    fitTable(resTable);
    s2->vbox()->addWidget(resTable);
    auto *devNote = new QLabel(I18n::tr("acs_dev_note"), s2);
    devNote->setWordWrap(true);
    s2->vbox()->addWidget(devNote);
    v->addWidget(s2);

    // ── 出力契約 (静的事実 — ADR-0007) ──────────────────────────────────────
    auto *s3 = new SectionBox(I18n::tr("acs_sec_contract"), body);
    auto *conTable = makeTable(s3, { I18n::tr("acs_col_file"),
        I18n::tr("acs_col_required"), I18n::tr("acs_col_content") });
    const struct { const char *file; bool required; const char *what; }
    kContract[] = {
        { "metadata.json", true,  "ソルバー情報・格子・実行条件" },
        { "rir.wav",       true,  "算出RIR (名前は outputRirFile で変更可)" },
        { "metrics.json",  false, "ソルバー側算出の指標" },
        { "solver.log",    true,  "実行ログ" },
    };
    for (const auto &row : kContract) {
        const int r = conTable->rowCount();
        conTable->insertRow(r);
        conTable->setItem(r, 0, roItem(QString::fromUtf8(row.file)));
        conTable->setItem(r, 1, roItem(I18n::tr(
            row.required ? "acs_req_yes" : "acs_req_no")));
        conTable->setItem(r, 2, roItem(QString::fromUtf8(row.what)));
    }
    fitTable(conTable);
    s3->vbox()->addWidget(conTable);
    auto *conNote = new QLabel(I18n::tr("acs_contract_note"), s3);
    conNote->setWordWrap(true);
    s3->vbox()->addWidget(conNote);
    v->addWidget(s3);
    v->addStretch(1);

    // ── 接続 ────────────────────────────────────────────────────────────────
    connect(m_backend, &QComboBox::currentIndexChanged, this,
            [this](int) { apply(); });
    connect(m_execPath, &QLineEdit::editingFinished, this,
            &AcousticSolverTab::apply);
    connect(m_threads, &QSpinBox::valueChanged, this,
            [this](int) { apply(); });
    connect(m_processes, &QSpinBox::valueChanged, this,
            [this](int) { apply(); });
    connect(btnBrowse, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, I18n::tr("acs_binary"));
        if (path.isEmpty()) return;
        m_execPath->setText(path);
        apply();
    });
    connect(m_btnRun, &QPushButton::clicked, this,
            &AcousticSolverTab::startSolver);
    connect(m_btnStop, &QPushButton::clicked, this,
            &AcousticSolverTab::stopSolver);

    // ランナーからの通知
    connect(m_runner, &AcousticRunner::logLine, this, [this](const QString &l) {
        m_log->appendPlainText(l);
    });
    connect(m_runner, &AcousticRunner::progress, this,
            [this](int step, int total) {
        m_progress->setVisible(true);
        m_progress->setValue(total > 0 ? step * 100 / total : 0);
    });
    connect(m_runner, &AcousticRunner::rirReady, this, [this](const QString &p) {
        // 契約検証済み RIR を実測 RIR 分析の入力へ (単一ソース原則)
        m_p->operaAcoustic().rirPath = p;
        m_p->touch();
    });
    connect(m_runner, &AcousticRunner::finished, this, [this](bool ok) {
        m_btnStop->setEnabled(false);
        m_progress->setVisible(false);
        m_status->setText(I18n::tr(ok ? "acs_status_done_ok"
                                      : "acs_status_done_ng"));
        updateResolution();   // Run の有効化は解決結果に従う
    });

    connect(m_p, &Project::changed, this, &AcousticSolverTab::refresh);
    connect(m_p, &Project::loaded, this, &AcousticSolverTab::refresh);

    refresh();
    setWidget(body);
    setWidgetResizable(true);
}

// ── model ⇄ widgets ─────────────────────────────────────────────────────────
void AcousticSolverTab::apply()
{
    if (m_updating) return;
    OperaAcousticSettings &s = m_p->operaAcoustic();
    s.solverBackend = m_backend->currentIndex();
    s.solverExecutable = m_execPath->text();
    s.solverThreads = m_threads->value();
    s.solverProcesses = m_processes->value();
    m_p->touch();
}

void AcousticSolverTab::refresh()
{
    m_updating = true;
    const OperaAcousticSettings &s = m_p->operaAcoustic();
    m_backend->setCurrentIndex(
        qBound(0, s.solverBackend, m_backend->count() - 1));
    m_execPath->setText(s.solverExecutable);
    m_threads->setValue(s.solverThreads);
    m_processes->setValue(s.solverProcesses);
    // 外部プロセス設定は ExternalFDTD / ExternalGeometric のみ
    const bool ext = s.solverBackend == 3 || s.solverBackend == 4;
    m_extGroup->setVisible(ext);
    if (ext) updateResolution();
    m_updating = false;
}

void AcousticSolverTab::updateResolution()
{
    const QString path =
        AcousticRunner::resolveSolver(configFrom(m_p->operaAcoustic()));
    if (path.isEmpty()) {
        m_resolved->setText(I18n::tr("acs_resolved_none"));
        m_btnRun->setEnabled(false);
    } else {
        m_resolved->setText(path);
        m_btnRun->setEnabled(!m_runner->isRunning());
    }
}

// ── 実行 ────────────────────────────────────────────────────────────────────
void AcousticSolverTab::startSolver()
{
    if (m_runner->isRunning()) return;
    m_log->clear();
    m_status->setText(I18n::tr("acs_status_running"));
    m_btnRun->setEnabled(false);
    m_btnStop->setEnabled(true);
    m_runner->start(configFrom(m_p->operaAcoustic()));
}

void AcousticSolverTab::stopSolver()
{
    m_runner->stop();
}
