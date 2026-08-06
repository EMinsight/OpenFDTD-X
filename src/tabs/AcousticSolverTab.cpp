// AcousticSolverTab.cpp
#include "AcousticSolverTab.h"
#include "../core/Project.h"
#include "../acoustics/core/HybridRir.h"
#include "../acoustics/io/WavWriter.h"
#include "../acoustics/qt/QtAcousticAdapter.h"
#include "../io/OfdIO.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"
#include "TabHelpers.h"

#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTableWidget>
#include <QVBoxLayout>

using namespace ofd;
using namespace ofd::tabhelp;

// ── タブ固有語彙 (acs_) — file-local 登録 ───────────────────────────────────
namespace {
const bool s_i18n = [] {
    // ── ハイブリッド RIR 合成 ──────────────────────────────────────────────
    I18n::reg("acs_sec_hybrid",
        "ハイブリッド RIR 合成 (低域 = FDTD + 高域 = 幾何音響)",
        "Hybrid RIR (low band = FDTD + high band = geometrical acoustics)");
    I18n::reg("acs_hy_note",
        "音響 FDTD の有効帯域は格子分解能で決まります (fmax = c/(10·dx) — "
        "dx = 0.5 m なら 69 Hz)。歌声の可聴化に要る 4 kHz は FDTD 単独では"
        "セル数が現実的でないため、低域を FDTD、高域を幾何音響が担う合成を"
        "行います。「▶▶ ハイブリッド実行」は音響 FDTD (ofdx_acoustic_fdtd) と"
        "幾何音響 (ofdx_acoustic_ga) を順に起動し、合成して可聴化の RIR まで"
        "設定します (2 つのバイナリは GUI に同梱していないので、"
        "$OPENFDTD_ACOUSTICS_HOME / PATH かカーネルパスの設定で位置を教えて"
        "ください)。合成では FDTD 側の音源パルス (ガウシアン微分) の逆フィルタ"
        "と遅延 t0 の除去、fs 合わせ、クロスオーバー帯でのレベル整合を"
        "自動で行います。クロスオーバーは両ソルバーの申告帯域 "
        "(FDTD の fmax と幾何音響の Schroeder 周波数) の重なりから決めます。"
        "片側だけ作り直したいときは「① 低域だけ実行」「② 高域だけ実行」を"
        "使ってください (その欄だけ更新し、合成は行いません)。",
        "An acoustic FDTD's usable band is set by the grid "
        "(fmax = c/(10·dx) — only 69 Hz at dx = 0.5 m). Reaching the 4 kHz a "
        "singing-voice auralization needs is not feasible with FDTD alone, so "
        "the low band comes from FDTD and the high band from geometrical "
        "acoustics. This panel implements the **combining** side: supply the "
        "high band from geometrical acoustics. \"Run hybrid\" launches the "
        "acoustic FDTD (ofdx_acoustic_fdtd) and the geometric solver "
        "(ofdx_acoustic_ga) in sequence, combines them and sets the "
        "auralization RIR (neither binary ships with the GUI — point at them "
        "with $OPENFDTD_ACOUSTICS_HOME / PATH or Tools -> Kernel paths). "
        "Combining deconvolves the FDTD source pulse (Gaussian derivative), "
        "removes its t0 delay, matches the sample rates and levels, and picks "
        "the crossover from the bands both solvers declare (the FDTD fmax and "
        "the geometric Schroeder frequency). To redo just one side, use "
        "\"(1) Run the low band only\" / \"(2) Run the high band only\" — they "
        "fill that field and stop, without combining.");
    I18n::reg("acs_hy_low", "低域 RIR (FDTD)", "Low-band RIR (FDTD)");
    I18n::reg("acs_hy_low_ph",
              "ソルバー実行結果の rir.wav (metadata.json が隣にあると精度が上がる)",
              "rir.wav from a solver run (a sibling metadata.json improves it)");
    I18n::reg("acs_hy_high", "高域 RIR (幾何音響)",
              "High-band RIR (geometrical)");
    I18n::reg("acs_hy_high_ph", "幾何音響ソルバーが出力した RIR WAV",
              "RIR WAV produced by a geometric acoustics solver");
    I18n::reg("acs_hy_out", "出力 RIR", "Output RIR");
    I18n::reg("acs_hy_out_ph", "合成した RIR の保存先 WAV",
              "Where to write the combined RIR");
    I18n::reg("acs_hy_filter", "WAV ファイル (*.wav)", "WAV files (*.wav)");
    I18n::reg("acs_hy_cross", "クロスオーバー [Hz]", "Crossover [Hz]");
    I18n::reg("acs_hy_cross_ph", "自動", "auto");
    I18n::reg("acs_hy_cross_note",
        "空欄なら metadata.json の source.fmax_hz (FDTD の有効帯域上限) を使う",
        "empty = use source.fmax_hz from metadata.json (the FDTD valid band)");
    I18n::reg("acs_hy_runlow", "① 低域だけ実行 (FDTD)",
              "(1) Run the low band only (FDTD)");
    I18n::reg("acs_hy_runhigh", "② 高域だけ実行 (幾何音響)",
              "(2) Run the high band only (geometric)");
    I18n::reg("acs_hy_runlow_tip",
        "音響 FDTD だけを実行して「低域 RIR」欄に入れます "
        "(合成は行いません)。\n%1",
        "Runs only the acoustic FDTD and fills the low-band field "
        "(does not combine).\n%1");
    I18n::reg("acs_hy_runhigh_tip",
        "幾何音響ソルバーだけを実行して「高域 RIR」欄に入れます "
        "(合成は行いません)。\n%1",
        "Runs only the geometric solver and fills the high-band field "
        "(does not combine).\n%1");
    I18n::reg("acs_hy_stage_done", "%1 の実行が完了しました — %2",
              "%1 finished — %2");
    I18n::reg("acs_hy_runall",
              "▶▶ ハイブリッド実行 (FDTD → 幾何音響 → 合成)",
              "▶▶ Run hybrid (FDTD → geometric → combine)");
    I18n::reg("acs_hy_runall_tip",
        "2 つのソルバーを順に起動し、結果を合成して可聴化の RIR に設定します。\n"
        "低域: %1\n高域: %2",
        "Runs both solvers in sequence, combines the results and sets the "
        "auralization RIR.\nLow: %1\nHigh: %2");
    I18n::reg("acs_hy_missing_solver",
        "%1 が見つかりません。ツール → カーネルパスの設定 か "
        "$OPENFDTD_ACOUSTICS_HOME / PATH に置いてください "
        "(幾何音響は ofdx_acoustic_ga)。",
        "%1 was not found. Put it on $OPENFDTD_ACOUSTICS_HOME / PATH or set "
        "it in Tools → Kernel paths (the geometric solver is "
        "ofdx_acoustic_ga).");
    I18n::reg("acs_hy_solver_fdtd", "音響 FDTD ソルバー (ofdx_acoustic_fdtd)",
              "the acoustic FDTD solver (ofdx_acoustic_fdtd)");
    I18n::reg("acs_hy_solver_ga", "幾何音響ソルバー (ofdx_acoustic_ga)",
              "the geometric solver (ofdx_acoustic_ga)");
    I18n::reg("acs_hy_phase_fdtd", "① 低域 (音響 FDTD) を実行: %1",
              "(1) Low band — running the acoustic FDTD solver: %1");
    I18n::reg("acs_hy_phase_ga", "② 高域 (幾何音響) を実行: %1",
              "(2) High band — running the geometric solver: %1");
    I18n::reg("acs_hy_phase_fdtd_ng", "① 低域 (音響 FDTD) が失敗しました",
              "(1) the acoustic FDTD run failed");
    I18n::reg("acs_hy_phase_ga_ng", "② 高域 (幾何音響) が失敗しました",
              "(2) the geometric run failed");
    I18n::reg("acs_hy_no_rir",
        "ソルバーは終了しましたが RIR が受け取れませんでした",
        "the solvers finished but no RIR was received");
    I18n::reg("acs_hy_run", "▶ 合成する", "▶ Combine");
    I18n::reg("acs_hy_assign", "合成結果を可聴化の RIR に設定",
              "Use the result as the auralization RIR");
    I18n::reg("acs_hy_need_low", "低域 RIR (FDTD) のファイルを指定してください",
              "Choose the low-band (FDTD) RIR file");
    I18n::reg("acs_hy_need_high",
        "高域 RIR (幾何音響) がありません。「② 高域だけ実行 (幾何音響)」を"
        "押すか、📁 参照… で ofdx_acoustic_ga の出力を選んでください "
        "(「▶▶ ハイブリッド実行」なら両方まとめて作ります)",
        "No high-band (geometrical) RIR. Press \"(2) Run the high band only\", "
        "or browse to an ofdx_acoustic_ga output (\"Run hybrid\" makes both "
        "at once)");
    I18n::reg("acs_hy_no_cross",
        "クロスオーバーが決まりません: 数値を入力するか、低域 RIR の隣に "
        "metadata.json (source.fmax_hz を含む) を置いてください。",
        "Cannot determine the crossover: enter a value, or place a "
        "metadata.json (with source.fmax_hz) next to the low-band RIR.");
    I18n::reg("acs_hy_done", "完了 — 出力: %1 (クロスオーバー %2 Hz, fs %3 Hz)",
              "Done — output: %1 (crossover %2 Hz, fs %3 Hz)");
    I18n::reg("acs_hy_detail",
        "低域に適用したレベル整合 %1 dB / 音源パルスの逆フィルタ %2 / "
        "クロスオーバー FIR %3 タップ (線形位相・相補対なので時間原点は不変)",
        "Level match applied to the low band %1 dB / source deconvolution %2 "
        "/ crossover FIR %3 taps (linear-phase complementary pair — the time "
        "origin is preserved)");
    I18n::reg("acs_hy_deconv_yes", "実施", "applied");
    I18n::reg("acs_hy_deconv_no", "なし (metadata なし)",
              "not applied (no metadata)");
    I18n::reg("acs_hy_failed", "合成に失敗しました: %1",
              "Combining failed: %1");

    I18n::reg("acs_sec_backend",
        "外部音響ソルバー連携 (実装: AcousticRunner — QProcess 疎結合)",
        "External acoustic solver (AcousticRunner — loosely-coupled QProcess)");
    I18n::reg("acs_backend", "RIRの取得元", "RIR source");
    I18n::reg("acs_b_none", "None — RIR取得なし (統計推定のみ)",
              "None — no RIR (statistical estimate only)");
    I18n::reg("acs_b_measured", "MeasuredRir — 実測RIR (実測RIR分析タブの従来経路)",
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
    // ExternalFDTD の正体の明確化 (ADR-0004): ofd (電磁 FDTD) の流用ではなく
    // 音響専用の外部ソルバー。計算ボタン (ofd) とは別物であることを明示する
    I18n::reg("acs_fdtd_note",
        "▸ ExternalFDTD は OpenFDTD (ofd, 電磁 FDTD) ではありません — "
        "ADR-0004 により音響 FDTD は音響専用の外部ソルバー "
        "(OpenAcoustics の ofdx_acoustic_fdtd。GUI には同梱していない) が"
        "担います。ExternalGeometric は同リポジトリの ofdx_acoustic_ga です。"
        "ツールバーの計算ボタン (ofd) は波動アナロジー表示用で、"
        "定量的な RIR はここからは得られません。",
        "▸ ExternalFDTD is NOT OpenFDTD (ofd, the electromagnetic FDTD) — "
        "per ADR-0004, acoustic FDTD is handled by a dedicated external "
        "acoustic solver (OpenAcoustics ofdx_acoustic_fdtd; not bundled with "
        "the GUI). ExternalGeometric is ofdx_acoustic_ga from the same "
        "repository. "
        "The toolbar Run button (ofd) is a wave-analogy display and yields "
        "no quantitative RIR.");
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
        "⚠ ソルバーが見つかりません — 契約 (ADR-0007: metadata.json + rir.wav "
        "→ metrics.json) を満たすソルバーを用意し、ツール → カーネルパス設定 "
        "か $OFDX_ACOUSTIC_SOLVER で指定してください。それまで RIR は実測 WAV "
        "の指定 (MeasuredRir) で分析・可聴化できます。",
        "⚠ Solver not found — provide a solver satisfying the contract "
        "(ADR-0007: metadata.json + rir.wav → metrics.json) and point to it "
        "via Tools → Kernel paths or $OFDX_ACOUSTIC_SOLVER. Until then, a "
        "measured WAV (MeasuredRir) still enables RIR analysis and "
        "auralization.");
    I18n::reg("acs_sec_resolve",
        "バイナリ探索順 / Solver resolution (ADR-0007 Decision 3)",
        "Solver resolution order (ADR-0007 Decision 3)");
    I18n::reg("acs_col_order", "#", "#");
    I18n::reg("acs_col_where", "探索場所", "Location");
    I18n::reg("acs_col_use", "用途", "Purpose");
    I18n::reg("acs_o1", "cfg.executable (明示指定)",
              "cfg.executable (explicit path)");
    I18n::reg("acs_o1_use", "最優先 (このプロジェクト限り)",
              "Highest priority (this project only)");
    I18n::reg("acs_o1b", "ツール → カーネルパスの設定 (室内音響)",
              "Tools → Kernel paths (room acoustics)");
    I18n::reg("acs_o1b_use", "全プロジェクト共通の既定パス",
              "Default path shared by every project");
    I18n::reg("acs_o2_use", "絶対パス直接指定 (CI/開発オーバーライド)",
              "Direct absolute path (CI / dev override)");
    I18n::reg("acs_o3", "$OPENFDTD_ACOUSTICS_HOME 配下",
              "Under $OPENFDTD_ACOUSTICS_HOME");
    I18n::reg("acs_o3_use", "導入先指定", "Install location");
    I18n::reg("acs_o4", "アプリ実行ディレクトリ kernel/",
              "kernel/ next to the app executable");
    I18n::reg("acs_o4_use", "同梱配置", "Bundled layout");
    I18n::reg("acs_o5_use", "最後", "Last resort");
    I18n::reg("acs_c1", "ソルバー情報・格子・実行条件",
              "Solver info, grid and run conditions");
    I18n::reg("acs_c2", "算出RIR (名前は outputRirFile で変更可)",
              "Computed RIR (name configurable via outputRirFile)");
    I18n::reg("acs_c3", "ソルバー側算出の指標", "Solver-side metrics");
    I18n::reg("acs_c4", "実行ログ", "Run log");
    I18n::reg("acs_sec_contract", "出力契約 / Output contract (docs/adr/0007)",
              "Output contract (docs/adr/0007)");
    I18n::reg("acs_col_file", "ファイル", "File");
    I18n::reg("acs_col_required", "必須", "Required");
    I18n::reg("acs_col_content", "内容", "Content");
    I18n::reg("acs_req_yes", "必須", "Required");
    I18n::reg("acs_req_no", "任意", "Optional");
    I18n::reg("acs_contract_note",
        "▸ 契約検証後に rirReady(path) → 実測RIR分析タブの実測RIRに設定される。"
        "FDTD推定は AcousticFdtdEstimator (格子/時間/メモリ見積) を使用。",
        "▸ After contract validation, rirReady(path) sets the measured RIR of "
        "the RIR analysis tab. FDTD sizing uses AcousticFdtdEstimator.");
    I18n::reg("acs_dev_note",
        "実音響ソルバーは別リポジトリ (OpenAcoustics) にある — "
        "ExternalFDTD = ofdx_acoustic_fdtd、ExternalGeometric = "
        "ofdx_acoustic_ga。GUI には同梱していないので、2 本を同じ場所に置いて "
        "$OPENFDTD_ACOUSTICS_HOME かカーネルパスの設定でどちらか片方を"
        "指定すれば両方解決する。CI は契約の番人としてモックソルバーで"
        "出力契約を検証している (docs/opera-acoustics-development-status.md §3)。",
        "The real acoustic solvers live in a separate repository "
        "(OpenAcoustics): ExternalFDTD = ofdx_acoustic_fdtd, "
        "ExternalGeometric = ofdx_acoustic_ga. Neither ships with the GUI — "
        "put both in one directory and point $OPENFDTD_ACOUSTICS_HOME (or the "
        "kernel-path setting) at either one; the other is found next to it. "
        "CI keeps validating the output contract with a mock solver.");
    I18n::reg("acs_status_idle", "待機中", "Idle");
    I18n::reg("acs_status_running", "実行中…", "Running…");
    // 受領した RIR の行き先はナビの「音響ドメイン → 🎤 実測RIR分析」
    // (I18n の t_riranalysis)。旧文言の「RIR分析タブ」はナビに無い名前で
    // 「どこ？」となるため、実際のナビ表記で案内する
    I18n::reg("acs_status_done_ok",
              "✓ 正常終了 — RIR を受領しました "
              "(ナビの 音響ドメイン → 🎤 実測RIR分析 で開けます)",
              "✓ Finished — RIR received "
              "(open it from Acoustic domain → 🎤 Measured RIR in the nav)");
    I18n::reg("acs_status_done_ng", "✗ 失敗 (ログを確認)", "✗ Failed (see log)");
    // 実行前の入力準備 (現在のプロジェクトを .ofd + .ofdx で書き出す)。
    // これを渡さないとソルバーは「入力が無い」で失敗する
    I18n::reg("acs_prep_wrote",
        "入力を書き出しました: %1 (作業ディレクトリ %2)",
        "Wrote solver input: %1 (working dir %2)");
    I18n::reg("acs_prep_fail",
        "入力の書き出しに失敗しました: %1",
        "Failed to write the solver input: %1");
    I18n::reg("acs_prep_mkdir",
        "作業ディレクトリを作成できません: %1",
        "Cannot create the working directory: %1");
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
    // ExternalFDTD ≠ OpenFDTD (ofd) の明示 (ADR-0004 — 監査 2026-08-05)
    auto *fdtdNote = new QLabel(I18n::tr("acs_fdtd_note"), s1);
    fdtdNote->setWordWrap(true);
    s1->vbox()->addWidget(fdtdNote);

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
    // 探索場所は I18n キー or そのまま表示するリテラル (環境変数名/PATH)
    const struct { const char *n; const char *where; bool whereIsKey;
                   const char *useKey; } kOrder[] = {
        { "1", "acs_o1", true, "acs_o1_use" },
        { "2", "acs_o1b", true, "acs_o1b_use" },
        { "3", "$OFDX_ACOUSTIC_SOLVER", false, "acs_o2_use" },
        { "4", "acs_o3", true, "acs_o3_use" },
        { "5", "acs_o4", true, "acs_o4_use" },
        { "6", "PATH", false, "acs_o5_use" },
    };
    for (const auto &row : kOrder) {
        const int r = resTable->rowCount();
        resTable->insertRow(r);
        resTable->setItem(r, 0, roItem(QString::fromUtf8(row.n)));
        resTable->setItem(r, 1, roItem(row.whereIsKey
            ? I18n::tr(row.where) : QString::fromUtf8(row.where)));
        resTable->setItem(r, 2, roItem(I18n::tr(row.useKey)));
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
    const struct { const char *file; bool required; const char *whatKey; }
    kContract[] = {
        { "metadata.json", true,  "acs_c1" },
        { "rir.wav",       true,  "acs_c2" },
        { "metrics.json",  false, "acs_c3" },
        { "solver.log",    true,  "acs_c4" },
    };
    for (const auto &row : kContract) {
        const int r = conTable->rowCount();
        conTable->insertRow(r);
        conTable->setItem(r, 0, roItem(QString::fromUtf8(row.file)));
        conTable->setItem(r, 1, roItem(I18n::tr(
            row.required ? "acs_req_yes" : "acs_req_no")));
        conTable->setItem(r, 2, roItem(I18n::tr(row.whatKey)));
    }
    fitTable(conTable);
    s3->vbox()->addWidget(conTable);
    auto *conNote = new QLabel(I18n::tr("acs_contract_note"), s3);
    conNote->setWordWrap(true);
    s3->vbox()->addWidget(conNote);
    v->addWidget(s3);

    // ── ハイブリッド RIR 合成 (低域 FDTD + 高域 幾何音響) ───────────────────
    // FDTD の有効帯域は fmax = c/(10·dx) しかないので、歌声の可聴化には
    // 高域を幾何音響で補う必要がある。ここは「足す」側だけを実装している
    // (幾何音響ソルバー自体は未提供 — 下の注記で明示する)。
    auto *s4 = new SectionBox(I18n::tr("acs_sec_hybrid"), body);
    auto *hyNote = new QLabel(I18n::tr("acs_hy_note"), s4);
    hyNote->setWordWrap(true);
    s4->vbox()->addWidget(hyNote);

    const auto fileRow = [this, s4](const char *labelKey, QLineEdit **edit,
                                    const char *phKey, bool save) {
        auto *row = new QHBoxLayout();
        row->addWidget(new QLabel(I18n::tr(labelKey), s4));
        *edit = new QLineEdit(s4);
        (*edit)->setPlaceholderText(I18n::tr(phKey));
        row->addWidget(*edit, 1);
        auto *b = new QPushButton(I18n::tr("acs_browse"), s4);
        QLineEdit *e = *edit;
        connect(b, &QPushButton::clicked, this, [this, e, save] {
            const QString p = save
                ? QFileDialog::getSaveFileName(this, I18n::tr("acs_sec_hybrid"),
                                               e->text(),
                                               I18n::tr("acs_hy_filter"))
                : QFileDialog::getOpenFileName(this, I18n::tr("acs_sec_hybrid"),
                                               e->text(),
                                               I18n::tr("acs_hy_filter"));
            if (!p.isEmpty()) e->setText(p);
            updateHybridUi();
        });
        row->addWidget(b);
        s4->vbox()->addLayout(row);
    };
    fileRow("acs_hy_low", &m_hyLow, "acs_hy_low_ph", false);
    fileRow("acs_hy_high", &m_hyHigh, "acs_hy_high_ph", false);
    fileRow("acs_hy_out", &m_hyOut, "acs_hy_out_ph", true);

    auto *cr = new QHBoxLayout();
    cr->addWidget(new QLabel(I18n::tr("acs_hy_cross"), s4));
    m_hyCross = new QLineEdit(s4);
    m_hyCross->setPlaceholderText(I18n::tr("acs_hy_cross_ph"));
    m_hyCross->setMaximumWidth(120);
    cr->addWidget(m_hyCross);
    cr->addWidget(new QLabel(I18n::tr("acs_hy_cross_note"), s4), 1);
    s4->vbox()->addLayout(cr);

    // 片側だけ作り直したいとき用 (高域 RIR を GUI だけで用意できるように)
    auto *sr = new QHBoxLayout();
    m_hyRunLow = new QPushButton(I18n::tr("acs_hy_runlow"), s4);
    m_hyRunHigh = new QPushButton(I18n::tr("acs_hy_runhigh"), s4);
    sr->addWidget(m_hyRunLow);
    sr->addWidget(m_hyRunHigh);
    sr->addStretch(1);
    s4->vbox()->addLayout(sr);

    auto *hr = new QHBoxLayout();
    m_hyRunAll = new QPushButton(I18n::tr("acs_hy_runall"), s4);
    m_hyRun = new QPushButton(I18n::tr("acs_hy_run"), s4);
    m_hyAssign = new QPushButton(I18n::tr("acs_hy_assign"), s4);
    m_hyAssign->setEnabled(false);
    hr->addWidget(m_hyRunAll);
    hr->addWidget(m_hyRun);
    hr->addWidget(m_hyAssign);
    hr->addStretch(1);
    s4->vbox()->addLayout(hr);

    m_hyResult = new QLabel(s4);
    m_hyResult->setWordWrap(true);
    m_hyResult->setVisible(false);
    s4->vbox()->addWidget(m_hyResult);
    v->addWidget(s4);
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

    // ハイブリッド合成
    for (QLineEdit *e : { m_hyLow, m_hyHigh, m_hyOut })
        connect(e, &QLineEdit::textChanged, this,
                [this](const QString &) { updateHybridUi(); });
    connect(m_hyRun, &QPushButton::clicked, this,
            &AcousticSolverTab::buildHybrid);
    connect(m_hyRunAll, &QPushButton::clicked, this,
            &AcousticSolverTab::startHybridRun);
    connect(m_hyRunLow, &QPushButton::clicked, this,
            [this] { m_log->clear(); startHybridStage(3); });
    connect(m_hyRunHigh, &QPushButton::clicked, this,
            [this] { m_log->clear(); startHybridStage(4); });
    connect(m_hyAssign, &QPushButton::clicked, this, [this] {
        if (m_hyLastOut.isEmpty()) return;
        m_p->operaAcoustic().rirPath = m_hyLastOut;
        m_p->touch();
        emit rirAssigned(m_hyLastOut);   // 実測RIR分析タブへも即反映
    });

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
        // ハイブリッド実行中の中間結果は可聴化へ流さない (最終的に設定するのは
        // 合成結果だけ)。段に応じて低域/高域の欄へ入れる。
        if (m_hybridPhase == 1 || m_hybridPhase == 3) {
            m_hyLow->setText(p);
            return;
        }
        if (m_hybridPhase == 2 || m_hybridPhase == 4) {
            m_hyHigh->setText(p);
            return;
        }
        // 契約検証済み RIR を実測 RIR 分析の入力へ (単一ソース原則)
        m_p->operaAcoustic().rirPath = p;
        m_p->touch();
        // 実測RIR分析タブの WAV 欄へ即座に反映させる (touch() だけでは
        // あちらの refresh は走らない — MainWindow が中継する)
        emit rirAssigned(p);
    });
    connect(m_runner, &AcousticRunner::finished, this, [this](bool ok) {
        m_btnStop->setEnabled(false);
        m_progress->setVisible(false);
        m_status->setText(I18n::tr(ok ? "acs_status_done_ok"
                                      : "acs_status_done_ng"));
        if (m_hybridPhase != 0) {
            advanceHybridRun(ok);   // 次の段 or 合成へ
            return;
        }
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
    // 低域 RIR の既定 (直近のソルバー結果) と実行可否を更新する。
    // m_updating を解いた後に呼ぶ (中でウィジェットを触るため)。
    updateHybridUi();
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
// ソルバーは `solver <working_dir> [<input_file>]` で起動する契約なので、
// 現在のプロジェクトを .ofd (+ .ofdx サイドカー) として書き出して渡す。
// 保存済みプロジェクトは <プロジェクトフォルダ>/acoustic_run/ を作業
// ディレクトリにする (元の .ofd を上書きしない / 出力 RIR が
// プロジェクトの近くに残り、一括可聴化の自動割当から辿れる)。
// 未保存なら一時ディレクトリを使う (保存を強制しない)。
QString AcousticSolverTab::prepareRunInput(QString *workingDir, QString *err,
                                           const QString &subDir)
{
    const QString projPath = m_p->filePath();
    const QString base = projPath.isEmpty()
        ? QStringLiteral("untitled")
        : QFileInfo(projPath).completeBaseName();
    QString dir = projPath.isEmpty()
        ? QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
              .absoluteFilePath(QStringLiteral("openfdtd-x-acoustics"))
        : QDir(QFileInfo(projPath).path())
              .absoluteFilePath(QStringLiteral("acoustic_run"));
    // ハイブリッド実行は 2 つのソルバーが同じ名前の契約ファイル
    // (rir.wav / metadata.json) を出すので、段ごとに別ディレクトリへ分ける
    if (!subDir.isEmpty()) dir = QDir(dir).absoluteFilePath(subDir);

    if (!QDir().mkpath(dir)) {
        if (err) *err = I18n::tr("acs_prep_mkdir").arg(dir);
        return QString();
    }
    // 前回実行の契約ファイルを消す (失敗した実行で古い RIR を拾わないため)
    QDir d(dir);
    const QStringList stale = d.entryList(
        QStringList{ "rir*.wav", "metadata.json", "metrics.json",
                     "solver.log" }, QDir::Files);
    for (const QString &f : stale) d.remove(f);

    const QString ofd = d.absoluteFilePath(base + ".ofd");
    QString e;
    if (!OfdIO::save(ofd, *m_p, &e)) {
        if (err) *err = I18n::tr("acs_prep_fail").arg(e);
        return QString();
    }
    // 吸音率などの音響設定は .ofdx サイドカー側にある (ソルバーが読む)
    if (!OfdxIO::save(d.absoluteFilePath(base + ".ofdx"), *m_p, &e)) {
        if (err) *err = I18n::tr("acs_prep_fail").arg(e);
        return QString();
    }
    if (workingDir) *workingDir = dir;
    return ofd;
}

void AcousticSolverTab::startSolver()
{
    if (m_runner->isRunning()) return;
    m_log->clear();

    QString dir, err;
    const QString input = prepareRunInput(&dir, &err);
    if (input.isEmpty()) {
        m_log->appendPlainText(err);
        m_status->setText(I18n::tr("acs_status_done_ng"));
        return;
    }
    m_log->appendPlainText(I18n::tr("acs_prep_wrote").arg(input, dir));

    m_status->setText(I18n::tr("acs_status_running"));
    m_btnRun->setEnabled(false);
    m_btnStop->setEnabled(true);
    AcousticRunConfig cfg = configFrom(m_p->operaAcoustic());
    cfg.workingDir = dir;
    cfg.inputFile = input;
    m_runner->start(cfg);
}

void AcousticSolverTab::stopSolver()
{
    m_runner->stop();
}

// ── ハイブリッド実行 (FDTD → 幾何音響 → 合成) ──────────────────────────────
void AcousticSolverTab::startHybridRun()
{
    // 一括 (①→②→合成)。片方だけ走らせる場合は startHybridStage を直接呼ぶ。
    if (m_runner->isRunning() || m_hybridPhase != 0) return;
    // 先に両方のバイナリを解決して、片方しか無い状態で走り出さないようにする
    const QString fdtdBin =
        AcousticRunner::resolveSolverForHybrid(AcousticBackend::ExternalFDTD);
    const QString gaBin = AcousticRunner::resolveSolverForHybrid(
        AcousticBackend::ExternalGeometric);
    if (fdtdBin.isEmpty() || gaBin.isEmpty()) {
        m_hyResult->setText(I18n::tr("acs_hy_missing_solver")
                                .arg(fdtdBin.isEmpty()
                                         ? I18n::tr("acs_hy_solver_fdtd")
                                         : I18n::tr("acs_hy_solver_ga")));
        m_hyResult->setVisible(true);
        return;
    }
    m_log->clear();
    startHybridStage(1);
}

// phase: 1 = 低域 (一括の第 1 段) / 2 = 高域 (一括の第 2 段)
//        3 = 低域のみ / 4 = 高域のみ (それぞれ欄を埋めて終わる)
void AcousticSolverTab::startHybridStage(int phase)
{
    if (m_runner->isRunning() || (m_hybridPhase != 0 && phase != 2)) return;
    const bool low = (phase == 1 || phase == 3);
    const AcousticBackend backend = low ? AcousticBackend::ExternalFDTD
                                        : AcousticBackend::ExternalGeometric;
    const QString bin = AcousticRunner::resolveSolverForHybrid(backend);
    if (bin.isEmpty()) {
        m_hyResult->setText(I18n::tr("acs_hy_missing_solver")
                                .arg(I18n::tr(low ? "acs_hy_solver_fdtd"
                                                  : "acs_hy_solver_ga")));
        m_hyResult->setVisible(true);
        return;
    }

    QString dir, err;
    const QString input = prepareRunInput(
        &dir, &err,
        low ? QStringLiteral("hybrid_fdtd") : QStringLiteral("hybrid_ga"));
    if (input.isEmpty()) {
        m_hyResult->setText(I18n::tr("acs_hy_failed").arg(err));
        m_hyResult->setVisible(true);
        return;
    }
    m_log->appendPlainText(
        I18n::tr(low ? "acs_hy_phase_fdtd" : "acs_hy_phase_ga").arg(bin));
    m_log->appendPlainText(I18n::tr("acs_prep_wrote").arg(input, dir));

    m_hybridPhase = phase;
    // 一括の第 1 段では両方の欄を作り直す。片側のみの実行はもう片方を残す。
    if (phase == 1) { m_hyLow->clear(); m_hyHigh->clear(); }
    else if (phase == 3) m_hyLow->clear();
    else if (phase == 4) m_hyHigh->clear();
    m_btnRun->setEnabled(false);
    m_btnStop->setEnabled(true);
    m_status->setText(I18n::tr("acs_status_running"));
    updateHybridUi();          // 実行中はハイブリッド系のボタンを全て無効化

    AcousticRunConfig cfg;
    cfg.backend = backend;
    cfg.executable = bin;
    cfg.threads = m_p->operaAcoustic().solverThreads;
    // 幾何音響は MPI を使わない
    cfg.processes = low ? m_p->operaAcoustic().solverProcesses : 1;
    cfg.workingDir = dir;
    cfg.inputFile = input;
    m_runner->start(cfg);
}

void AcousticSolverTab::advanceHybridRun(bool ok)
{
    if (m_hybridPhase == 0) return;
    const int phase = m_hybridPhase;

    const auto abort = [this](const QString &msg) {
        m_hybridPhase = 0;
        m_hyResult->setText(I18n::tr("acs_hy_failed").arg(msg));
        m_hyResult->setVisible(true);
        updateHybridUi();
        updateResolution();
    };
    if (!ok)
        return abort(I18n::tr(phase == 1 ? "acs_hy_phase_fdtd_ng"
                                         : "acs_hy_phase_ga_ng"));

    // 片側のみの実行はここで終わり (欄は rirReady が埋めている)
    if (phase == 3 || phase == 4) {
        m_hybridPhase = 0;
        m_hyResult->setText(I18n::tr("acs_hy_stage_done")
                                .arg(I18n::tr(phase == 3 ? "acs_hy_solver_fdtd"
                                                         : "acs_hy_solver_ga"),
                                     (phase == 3 ? m_hyLow : m_hyHigh)->text()));
        m_hyResult->setVisible(true);
        updateHybridUi();
        updateResolution();
        return;
    }

    if (phase == 1) {
        m_hybridPhase = 0;      // startHybridStage の再入ガードを通すため
        startHybridStage(2);    // FDTD 完了 → 幾何音響へ
        return;
    }

    // 幾何音響も完了 → 合成 (両方の RIR は rirReady で欄に入っている)
    m_hybridPhase = 0;
    if (m_hyLow->text().trimmed().isEmpty() ||
        m_hyHigh->text().trimmed().isEmpty())
        return abort(I18n::tr("acs_hy_no_rir"));
    m_hyOut->clear();          // 既定 (低域 RIR の隣) を採り直す
    updateHybridUi();
    buildHybrid();
    if (!m_hyLastOut.isEmpty()) {
        // 合成できたら可聴化の RIR に設定する (一括実行の目的地)
        m_p->operaAcoustic().rirPath = m_hyLastOut;
        m_p->touch();
        emit rirAssigned(m_hyLastOut);
    }
    updateResolution();
}

// ── ハイブリッド RIR 合成 ───────────────────────────────────────────────────
void AcousticSolverTab::updateHybridUi()
{
    if (!m_hyRun) return;
    // 低域が空なら直近のソルバー結果を既定にする (毎回選ばせない)
    if (m_hyLow->text().trimmed().isEmpty() &&
        !m_p->operaAcoustic().rirPath.trimmed().isEmpty())
        m_hyLow->setText(m_p->operaAcoustic().rirPath);

    const QString low = m_hyLow->text().trimmed();
    const QString high = m_hyHigh->text().trimmed();
    const bool busy = (m_hybridPhase != 0) || m_runner->isRunning();
    QString why;
    if (busy)
        why = I18n::tr("acs_status_running");
    else if (low.isEmpty() || !QFileInfo::exists(low))
        why = I18n::tr("acs_hy_need_low");
    else if (high.isEmpty() || !QFileInfo::exists(high))
        why = I18n::tr("acs_hy_need_high");
    m_hyRun->setEnabled(why.isEmpty());
    m_hyRun->setToolTip(why);

    // 一括実行は 2 つのソルバーが両方解決できるときだけ (片方だけで
    // 走り出して途中で止まる、という体験にしない)
    if (m_hyRunAll) {
        const QString fdtdBin = AcousticRunner::resolveSolverForHybrid(
            AcousticBackend::ExternalFDTD);
        const QString gaBin = AcousticRunner::resolveSolverForHybrid(
            AcousticBackend::ExternalGeometric);
        QString whyAll;
        if (busy) whyAll = I18n::tr("acs_status_running");
        else if (fdtdBin.isEmpty())
            whyAll = I18n::tr("acs_hy_missing_solver")
                         .arg(I18n::tr("acs_hy_solver_fdtd"));
        else if (gaBin.isEmpty())
            whyAll = I18n::tr("acs_hy_missing_solver")
                         .arg(I18n::tr("acs_hy_solver_ga"));
        m_hyRunAll->setEnabled(whyAll.isEmpty());
        m_hyRunAll->setToolTip(whyAll.isEmpty()
                                   ? I18n::tr("acs_hy_runall_tip")
                                         .arg(fdtdBin, gaBin)
                                   : whyAll);
        // 片側実行はそのソルバーだけ解決できればよい
        const struct { QPushButton *b; const QString &bin; const char *nameKey;
                       const char *tipKey; }
        kStage[] = {
            { m_hyRunLow,  fdtdBin, "acs_hy_solver_fdtd", "acs_hy_runlow_tip" },
            { m_hyRunHigh, gaBin,   "acs_hy_solver_ga",   "acs_hy_runhigh_tip" },
        };
        for (const auto &st : kStage) {
            if (!st.b) continue;
            QString whyOne;
            if (busy) whyOne = I18n::tr("acs_status_running");
            else if (st.bin.isEmpty())
                whyOne = I18n::tr("acs_hy_missing_solver")
                             .arg(I18n::tr(st.nameKey));
            st.b->setEnabled(whyOne.isEmpty());
            st.b->setToolTip(whyOne.isEmpty()
                                 ? I18n::tr(st.tipKey).arg(st.bin)
                                 : whyOne);
        }
    }

    // 出力先の既定は低域 RIR と同じ場所の hybrid_rir.wav
    if (m_hyOut->text().trimmed().isEmpty() && !low.isEmpty()) {
        const QFileInfo fi(low);
        m_hyOut->setText(
            QDir(fi.absolutePath()).absoluteFilePath(
                QStringLiteral("hybrid_rir.wav")));
    }
}

void AcousticSolverTab::buildHybrid()
{
    using namespace ofd::acoustics;
    const QString lowPath = m_hyLow->text().trimmed();
    const QString highPath = m_hyHigh->text().trimmed();
    QString outPath = m_hyOut->text().trimmed();
    if (outPath.isEmpty()) {
        const QFileInfo fi(lowPath);
        outPath = QDir(fi.absolutePath()).absoluteFilePath(
            QStringLiteral("hybrid_rir.wav"));
        m_hyOut->setText(outPath);
    }

    const auto fail = [this](const QString &msg) {
        m_hyResult->setText(I18n::tr("acs_hy_failed").arg(msg));
        m_hyResult->setVisible(true);
        m_hyAssign->setEnabled(false);
    };

    const AcousticResult<AudioBuffer> low = QtAcousticAdapter::readWav(lowPath);
    if (!low.success())
        return fail(QStringLiteral("low: ") +
                    QString::fromStdString(low.message()));
    const AcousticResult<AudioBuffer> high =
        QtAcousticAdapter::readWav(highPath);
    if (!high.success())
        return fail(QStringLiteral("high: ") +
                    QString::fromStdString(high.message()));

    // FDTD 側の metadata.json から有効帯域と音源パルスを取る (無ければ
    // 逆フィルタを行わず、その旨を warning として出す — 黙って進めない)
    const QtAcousticAdapter::SolverMetadata meta =
        QtAcousticAdapter::metadataForRir(lowPath);
    // 幾何音響側は valid_band_hz[0] (Schroeder 周波数) を申告する。両方
    // 分かるとクロスオーバーを重なり区間の幾何平均に置ける。
    const QtAcousticAdapter::SolverMetadata gaMeta =
        QtAcousticAdapter::metadataForRir(highPath);
    HybridRirConfig cfg;
    cfg.fdtdFmaxHz = meta.sourceFmaxHz;
    cfg.gaValidLoHz = gaMeta.validBandLoHz;
    cfg.sourceSigmaS = meta.sourceSigmaS;
    cfg.sourceT0S = meta.sourceT0S;
    bool okCross = false;
    const double cross = m_hyCross->text().trimmed().toDouble(&okCross);
    if (okCross && cross > 0.0) cfg.crossoverHz = cross;
    if (!(cfg.crossoverHz > 0.0) && !(cfg.fdtdFmaxHz > 0.0))
        return fail(I18n::tr("acs_hy_no_cross"));

    HybridRirInfo info;
    const AcousticResult<AudioBuffer> mix =
        buildHybridRir(low.value(), high.value(), cfg, &info);
    if (!mix.success())
        return fail(QString::fromStdString(mix.message()));

    const AcousticResult<bool> w =
        writeWavFile(outPath.toStdString(), mix.value());
    if (!w.success())
        return fail(QString::fromStdString(w.message()));

    QStringList lines;
    lines << I18n::tr("acs_hy_done")
                 .arg(outPath,
                      QString::number(info.crossoverHz, 'f', 1),
                      QString::number(info.outputRateHz, 'f', 0));
    lines << I18n::tr("acs_hy_detail")
                 .arg(QString::number(info.fdtdGainDb, 'f', 2),
                      I18n::tr(info.deconvolved ? "acs_hy_deconv_yes"
                                                : "acs_hy_deconv_no"),
                      QString::number(qulonglong(info.filterLength)));
    for (const std::string &s : info.warnings)
        lines << QStringLiteral("• ") + QString::fromStdString(s);
    m_hyResult->setText(lines.join(QStringLiteral("\n")));
    m_hyResult->setVisible(true);
    m_hyLastOut = outPath;
    m_hyAssign->setEnabled(true);
}
