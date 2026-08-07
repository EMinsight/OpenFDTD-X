// AuralizationTab.cpp
#include "AuralizationTab.h"
#include "../core/Project.h"
#include "../core/RirAutoAssign.h"
#include "../acoustics/qt/QtAcousticAdapter.h"
#include "../widgets/MiniPlot.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"
#include "TabHelpers.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QThread>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

using namespace ofd;
using namespace ofd::acoustics;
// 波形包絡線などのタブ共有ヘルパーは src/tabs/TabHelpers.{h,cpp} に集約。
using namespace ofd::tabhelp;

namespace {

// ⑤ 複数受音点の一括可聴化 — タブ専用語彙 (接頭辞 aurb_) の file-local 登録
const bool s_i18nBatch = [] {
    ofd::I18n::reg("aurb_section", "複数受音点の一括可聴化",
                   "Batch auralization (all receivers)");
    ofd::I18n::reg("aurb_hint",
        "受音点リスト (音響タブと共有) の各受音点に、その位置の RIR WAV を"
        "割り当てて、①のドライ WAV を受音点ごとに一括で畳み込みます。"
        "受聴位置が違えば RIR は異なるため、単一 RIR の使い回しは行いません "
        "(RIR 未指定の行はスキップされます)。有効チェックが OFF の受音点は"
        "対象外です。fs が異なる RIR は自動でドライ側 fs へリサンプリングし、"
        "変換した旨を行の結果に明示します。",
        "Assigns each receiver in the receiver list (shared with the Acoustic "
        "tab) the RIR WAV measured/simulated at that position, then convolves "
        "the dry WAV from section 1 for every receiver in one run. A different "
        "listening position means a different RIR, so a single RIR is never "
        "reused across receivers (rows without an RIR are skipped). Receivers "
        "with their enabled checkbox off are excluded. RIRs at a different "
        "sample rate are resampled to the dry file's rate automatically, and "
        "the conversion is reported in the row result.");
    ofd::I18n::reg("aurb_col_receiver", "受音点", "Receiver");
    ofd::I18n::reg("aurb_col_rir", "RIR WAV", "RIR WAV");
    ofd::I18n::reg("aurb_col_status", "状態", "Status");
    ofd::I18n::reg("aurb_name_disabled", "(無効)", "(disabled)");
    ofd::I18n::reg("aurb_listen", "▶ 試聴", "▶ Play");
    ofd::I18n::reg("aurb_listen_tip",
                   "書き出した WAV を外部プレイヤーで開く (アプリ内再生は未対応)",
                   "Open the exported WAV in an external player "
                   "(in-app playback is not supported)");
    ofd::I18n::reg("aurb_out_dir", "出力先フォルダ", "Output folder");
    ofd::I18n::reg("aurb_out_dir_placeholder",
                   "既定: プロジェクトのフォルダ", "Default: project folder");
    ofd::I18n::reg("aurb_naming",
        "命名規則: <ドライ名>_<受音点名>.wav (空名は P行番号、使えない文字と"
        "空白は「_」、重複名は _2, _3… で一意化)",
        "Naming: <dry name>_<receiver name>.wav (empty names become P<row>, "
        "illegal characters and spaces become \"_\", duplicates are made "
        "unique with _2, _3…)");
    ofd::I18n::reg("aurb_run", "▶ 一括レンダリング", "▶ Render all");
    ofd::I18n::reg("aurb_cancel", "中断", "Cancel");
    ofd::I18n::reg("aurb_status_idle",
        "受音点ごとに RIR WAV を割り当てて一括レンダリングしてください。",
        "Assign an RIR WAV per receiver, then render all.");
    ofd::I18n::reg("aurb_status_nodry",
        "ドライ WAV が選択されていません (①で指定してください)。",
        "No dry WAV selected (choose one in section 1).");
    ofd::I18n::reg("aurb_status_nodir",
        "出力先フォルダを決められません (プロジェクト未保存かつドライ WAV "
        "未指定)。出力先フォルダを指定してください。",
        "Cannot resolve an output folder (project not saved and no dry WAV). "
        "Choose an output folder.");
    ofd::I18n::reg("aurb_status_nojobs",
        "レンダリング対象がありません (有効かつ RIR 指定済みの受音点が必要です)。",
        "Nothing to render (needs at least one enabled receiver with an RIR).");
    ofd::I18n::reg("aurb_status_running", "一括レンダリング中… (%1/%2)",
                   "Rendering… (%1/%2)");
    ofd::I18n::reg("aurb_status_cancelling", "中断します (実行中の行の完了後)…",
                   "Cancelling (after the current row finishes)…");
    ofd::I18n::reg("aurb_status_done",
                   "完了 — %1/%2 件を書き出しました。",
                   "Done — %1 of %2 files written.");
    ofd::I18n::reg("aurb_status_cancelled",
                   "中断しました — %1/%2 件を書き出し済み。",
                   "Cancelled — %1 of %2 files written.");
    ofd::I18n::reg("aurb_row_disabled", "対象外 (受音点が無効)",
                   "Excluded (receiver disabled)");
    ofd::I18n::reg("aurb_row_norir",
                   "スキップ: RIR 未指定 — ソルバ実行か実測 WAV の指定が必要",
                   "Skipped: no RIR — run a solver or set a measured WAV");
    ofd::I18n::reg("aurb_row_ready", "レンダリング可能", "Ready");
    ofd::I18n::reg("aurb_row_pending", "待機中", "Queued");
    ofd::I18n::reg("aurb_row_running", "レンダリング中…", "Rendering…");
    ofd::I18n::reg("aurb_row_done", "完了 — %1", "Done — %1");
    ofd::I18n::reg("aurb_row_resampled", "(RIR %1→%2 Hz 変換)",
                   "(RIR resampled %1→%2 Hz)");
    ofd::I18n::reg("aurb_row_clipped", "(クリップ %1 サンプル)",
                   "(%1 samples clipped)");
    ofd::I18n::reg("aurb_row_error", "失敗: %1", "Failed: %1");
    ofd::I18n::reg("aurb_row_cancelled", "中断 (未実行)", "Cancelled (not run)");
    // 📂 フォルダから自動割当 (core/RirAutoAssign)
    ofd::I18n::reg("aurb_auto_btn", "📂 フォルダから自動割当",
                   "📂 Auto-assign from folder");
    ofd::I18n::reg("aurb_auto_btn_tip",
        "選択したフォルダ直下の *.wav を、受音点名との対応規則で各行の RIR に"
        "割り当てます。規則: (1) 完全一致 <名前>.wav、(2) rir_<名前>.wav / "
        "<名前>_rir.wav、(3) rir.wav が 1 個だけで対象行が 1 行だけならその行。"
        "比較は拡張子・大文字小文字・記号を無視します。候補が複数の行は"
        "割り当てず、理由を状態欄に表示します。",
        "Assigns the *.wav files directly inside the chosen folder to the "
        "receivers by name. Rules: (1) exact match <name>.wav, "
        "(2) rir_<name>.wav / <name>_rir.wav, (3) a single rir.wav goes to a "
        "single eligible row. Matching ignores extension, case and symbols. "
        "Ambiguous rows are left unassigned with the reason in the status "
        "column.");
    // ① 🎵 音源リストからドライ音源を取り込む導線 (接頭辞 aurd_)
    ofd::I18n::reg("aurd_from_source", "🎵 音源リストから",
                   "🎵 From source list");
    ofd::I18n::reg("aurd_from_source_tip",
        "「🎤 音源/WAV/指向性」タブの音源一覧で信号 (WAV) を設定した有効な"
        "音源から 1 つ選び、ドライ音源に取り込みます (1 音源 = 1 ドライ音源)。",
        "Picks one of the enabled sources that have a signal (WAV) assigned in "
        "the Source/WAV/Directivity tab and uses it as the dry source "
        "(one source = one dry file).");
    ofd::I18n::reg("aurd_from_source_disabled_tip",
        "「🎤 音源/WAV/指向性」タブで音源に信号 (WAV) を設定してください "
        "(信号が設定された有効な音源がありません)。",
        "Assign a signal (WAV) to a source in the Source/WAV/Directivity tab "
        "first (no enabled source has a signal).");
    ofd::I18n::reg("aurd_title", "音源リストからドライ音源を選択",
                   "Choose the dry source from the source list");
    ofd::I18n::reg("aurd_prompt",
        "ドライ音源として使う音源を選んでください (1 音源 = 1 ドライ音源):",
        "Choose the source to use as the dry signal "
        "(one source = one dry file):");
    ofd::I18n::reg("aurd_missing_mark", "(ファイルが見つかりません)",
                   "(file not found)");
    ofd::I18n::reg("aurd_applied",
                   "ドライ音源に音源 %1 の信号を設定しました — 「▶ 実行」で"
                   "畳み込めます。",
                   "Dry source set from source %1 — press “▶ Run” to convolve.");
    ofd::I18n::reg("aurd_note",
        "▸ ドライ音源は 1 音源 = 1 ファイルです。複数音源の同時再生 "
        "(ミックス) は未実装 — 混ぜたい場合は「🎚 音響編集・解析」タブで"
        "合成してから、その WAV を選んでください。",
        "▸ The dry source is one file for one source. Simultaneous playback of "
        "several sources (mixing) is not implemented — mix them in the Audio "
        "Editor tab first and choose the resulting WAV here.");
    ofd::I18n::reg("aurb_auto_only_unset", "未設定の行のみ",
                   "Only rows without an RIR");
    ofd::I18n::reg("aurb_auto_only_unset_tip",
        "ON (既定): RIR が未設定の行だけに割り当て、既存の設定を守ります。"
        "OFF: 既存の割当も上書きします。",
        "On (default): assign only to rows without an RIR, keeping existing "
        "settings. Off: existing assignments are overwritten too.");
    ofd::I18n::reg("aurb_auto_dir_title", "RIR フォルダの選択",
                   "Choose RIR folder");
    ofd::I18n::reg("aurb_auto_summary",
                   "自動割当: %1 行割当 / %2 行未割当 (%3)",
                   "Auto-assign: %1 assigned, %2 not assigned (%3)");
    ofd::I18n::reg("aurb_auto_row_assigned", "自動割当 — %1",
                   "Auto-assigned — %1");
    ofd::I18n::reg("aurb_auto_row_ambiguous",
                   "自動割当なし: 候補が複数 (%1)",
                   "Not assigned: multiple candidates (%1)");
    ofd::I18n::reg("aurb_auto_row_nomatch",
                   "自動割当なし: 一致する WAV がありません",
                   "Not assigned: no matching WAV");
    ofd::I18n::reg("aurb_auto_nowav",
                   "自動割当: フォルダに WAV ファイルがありません (%1)",
                   "Auto-assign: no WAV files in the folder (%1)");
    ofd::I18n::reg("aurb_auto_rule_exact", "完全一致 <名前>.wav",
                   "Exact match <name>.wav");
    ofd::I18n::reg("aurb_auto_rule_affix",
                   "rir_<名前>.wav / <名前>_rir.wav",
                   "rir_<name>.wav / <name>_rir.wav");
    ofd::I18n::reg("aurb_auto_rule_single",
                   "唯一の rir.wav → 唯一の対象行",
                   "The only rir.wav → the only eligible row");
    return true;
}();

QVector<MiniSeries> waveformSeries(const std::vector<double> &x, double fs,
                                   const QColor &color)
{
    QVector<QPointF> top, bottom;
    // A/B 波形の時間軸は秒 (RIR 分析タブは ms)
    envelopeSeries(x, fs, 1200, TimeUnit::Seconds, top, bottom);
    MiniSeries hi;  hi.pts = top;     hi.color = color;
    MiniSeries lo;  lo.pts = bottom;  lo.color = color;
    return { hi, lo };
}

QHBoxLayout *pathRow(QLineEdit *&edit, QPushButton *&browse,
                     SectionBox *parent, const QString &placeholder)
{
    auto *row = new QHBoxLayout();
    edit = new QLineEdit(parent);
    edit->setReadOnly(true);
    edit->setPlaceholderText(placeholder);
    browse = new QPushButton(I18n::tr("rir_browse"), parent);
    row->addWidget(edit, 1);
    row->addWidget(browse);
    return row;
}

} // namespace

// ── construction ────────────────────────────────────────────────────────────
AuralizationTab::AuralizationTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // 概要: 畳み込み可聴化。自動正規化は行わない。fs 不一致は RIR を
    // ドライ側 fs へリサンプリングして続行する (変換した旨を結果に明示)。
    auto *hint = new QLabel(I18n::tr("aur_model_hint"), body);
    hint->setWordWrap(true);
    v->addWidget(hint);

    // ① 入力
    auto *sIn = new SectionBox(I18n::tr("aur_input_section"), body);
    QPushButton *dryBrowse = nullptr, *rirBrowse = nullptr, *outBrowse = nullptr;
    // ドライ WAV: ファイル選択に加えて、音源リスト (音源/WAV/指向性タブ) で
    // 信号を設定済みの音源から取り込む導線を並べる
    sIn->form()->addRow(I18n::tr("aur_dry_file"),
        pathRow(m_dryPath, dryBrowse, sIn, I18n::tr("rir_file_placeholder")));
    // 取り込みボタンは別の行に置く (パス欄と並べると左ペインの幅では
    // ファイル名が読めなくなるため)
    m_dryFromSrcBtn = new QPushButton(I18n::tr("aurd_from_source"), sIn);
    m_dryFromSrcBtn->setObjectName(QStringLiteral("aurDryFromSourceBtn"));
    auto *dryBtnRow = new QHBoxLayout();
    dryBtnRow->addWidget(m_dryFromSrcBtn);
    dryBtnRow->addStretch(1);
    sIn->form()->addRow(QString(), dryBtnRow);
    // 1 音源 = 1 ドライ音源 (ミックスは未実装) を UI に明記する
    auto *dryNote = new QLabel(I18n::tr("aurd_note"), sIn);
    dryNote->setWordWrap(true);
    dryNote->setStyleSheet(QStringLiteral("color:#888888;"));
    sIn->form()->addRow(QString(), dryNote);
    sIn->form()->addRow(I18n::tr("aur_rir_file"),
        pathRow(m_rirPath, rirBrowse, sIn, I18n::tr("rir_file_placeholder")));
    sIn->form()->addRow(I18n::tr("aur_output_file"),
        pathRow(m_outPath, outBrowse, sIn, I18n::tr("aur_output_placeholder")));

    m_gainMode = new QComboBox(sIn);
    m_gainMode->addItems({ I18n::tr("aur_gain_asis"),
                           I18n::tr("aur_gain_suggested") });
    sIn->form()->addRow(I18n::tr("aur_gain_mode"), m_gainMode);
    v->addWidget(sIn);

    // ② 実行
    auto *sRun = new SectionBox(I18n::tr("aur_run_section"), body);
    auto *runRow = new QHBoxLayout();
    m_runBtn = new QPushButton(I18n::tr("aur_run"), sRun);
    m_status = new QLabel(I18n::tr("aur_status_idle"), sRun);
    m_status->setWordWrap(true);
    runRow->addWidget(m_runBtn);
    runRow->addWidget(m_status, 1);
    sRun->vbox()->addLayout(runRow);
    v->addWidget(sRun);

    // ③ 結果
    auto *sRes = new SectionBox(I18n::tr("aur_result_section"), body);
    m_peakLabel = new QLabel(QStringLiteral("-"), sRes);
    m_gainLabel = new QLabel(QStringLiteral("-"), sRes);
    m_clipLabel = new QLabel(QStringLiteral("-"), sRes);
    sRes->form()->addRow(I18n::tr("aur_output_peak"), m_peakLabel);
    sRes->form()->addRow(I18n::tr("aur_suggested_gain"), m_gainLabel);
    sRes->form()->addRow(I18n::tr("aur_clipped_samples"), m_clipLabel);
    m_warnings = new QLabel(sRes);
    m_warnings->setWordWrap(true);
    m_warnings->setVisible(false);
    sRes->vbox()->addWidget(m_warnings);
    v->addWidget(sRes);

    // ④ A/B 波形 (ドライ / ウェット並置)。アプリ内再生は未対応。
    auto *sAb = new SectionBox(I18n::tr("aur_ab_section"), body);
    auto *abRow = new QHBoxLayout();
    auto *dryCol = new QVBoxLayout();
    dryCol->addWidget(new QLabel(I18n::tr("aur_dry_wave"), sAb));
    m_dryPlot = new MiniPlot(sAb);
    m_dryPlot->setLabels(I18n::tr("vocal_time_s"), I18n::tr("rir_amplitude"));
    m_dryPlot->setMinimumHeight(130);
    dryCol->addWidget(m_dryPlot);
    auto *wetCol = new QVBoxLayout();
    wetCol->addWidget(new QLabel(I18n::tr("aur_wet_wave"), sAb));
    m_wetPlot = new MiniPlot(sAb);
    m_wetPlot->setLabels(I18n::tr("vocal_time_s"), I18n::tr("rir_amplitude"));
    m_wetPlot->setMinimumHeight(130);
    wetCol->addWidget(m_wetPlot);
    abRow->addLayout(dryCol, 1);
    abRow->addLayout(wetCol, 1);
    sAb->vbox()->addLayout(abRow);
    // 未実装機能を動作済みと誤解させないための明示注記
    auto *playbackNote = new QLabel(I18n::tr("aur_playback_note"), sAb);
    playbackNote->setWordWrap(true);
    sAb->vbox()->addWidget(playbackNote);
    v->addWidget(sAb);

    // ⑤ 複数受音点の一括可聴化 — 受音点リスト (AcousticTab と共有) の各行に
    //    受音点ごとの RIR を割り当てて順に畳み込む。単一 RIR の使い回しは
    //    物理的に無意味 (受聴位置が違えば RIR は違う) なので導線を置かない。
    auto *sBatch = new SectionBox(I18n::tr("aurb_section"), body);
    auto *batchHint = new QLabel(I18n::tr("aurb_hint"), sBatch);
    batchHint->setWordWrap(true);
    sBatch->vbox()->addWidget(batchHint);

    m_batchTable = new QTableWidget(0, 5, sBatch);
    m_batchTable->setObjectName(QStringLiteral("aurBatchTable"));
    m_batchTable->setHorizontalHeaderLabels(
        { I18n::tr("aurb_col_receiver"), I18n::tr("aurb_col_rir"),
          QString(), I18n::tr("aurb_col_status"), QString() });
    m_batchTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_batchTable->horizontalHeader()->setSectionResizeMode(
        2, QHeaderView::ResizeToContents);
    m_batchTable->horizontalHeader()->setSectionResizeMode(
        4, QHeaderView::ResizeToContents);
    m_batchTable->verticalHeader()->setVisible(false);
    m_batchTable->setMinimumHeight(4 * 26 + 40);
    sBatch->vbox()->addWidget(m_batchTable);

    // 📂 フォルダから自動割当 — ソルバ実行が生成した RIR 群を行ごとに手動で
    //    選ばなくて済む導線。対応規則は core/RirAutoAssign (説明はツールチップ)
    auto *autoRow = new QHBoxLayout();
    m_autoAssignBtn = new QPushButton(I18n::tr("aurb_auto_btn"), sBatch);
    m_autoAssignBtn->setObjectName(QStringLiteral("aurAutoAssignBtn"));
    m_autoAssignBtn->setToolTip(I18n::tr("aurb_auto_btn_tip"));
    m_autoOnlyUnset = new QCheckBox(I18n::tr("aurb_auto_only_unset"), sBatch);
    m_autoOnlyUnset->setToolTip(I18n::tr("aurb_auto_only_unset_tip"));
    m_autoOnlyUnset->setChecked(true);   // 既定 ON = 既存設定を守る
    autoRow->addWidget(m_autoAssignBtn);
    autoRow->addWidget(m_autoOnlyUnset);
    autoRow->addStretch(1);
    sBatch->vbox()->addLayout(autoRow);

    QPushButton *outDirBrowse = nullptr;
    sBatch->form()->addRow(I18n::tr("aurb_out_dir"),
        pathRow(m_batchOutDir, outDirBrowse, sBatch,
                I18n::tr("aurb_out_dir_placeholder")));
    m_batchOutDir->setObjectName(QStringLiteral("aurBatchOutDir"));
    auto *nameRule = new QLabel(I18n::tr("aurb_naming"), sBatch);
    nameRule->setWordWrap(true);
    nameRule->setStyleSheet(QStringLiteral("color:#888888;"));
    sBatch->vbox()->addWidget(nameRule);

    auto *batchRow = new QHBoxLayout();
    m_batchRunBtn = new QPushButton(I18n::tr("aurb_run"), sBatch);
    m_batchCancelBtn = new QPushButton(I18n::tr("aurb_cancel"), sBatch);
    m_batchCancelBtn->setEnabled(false);
    m_batchStatus = new QLabel(I18n::tr("aurb_status_idle"), sBatch);
    m_batchStatus->setObjectName(QStringLiteral("aurBatchStatus"));
    m_batchStatus->setWordWrap(true);
    batchRow->addWidget(m_batchRunBtn);
    batchRow->addWidget(m_batchCancelBtn);
    batchRow->addWidget(m_batchStatus, 1);
    sBatch->vbox()->addLayout(batchRow);
    v->addWidget(sBatch);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    connect(dryBrowse, &QPushButton::clicked,
            this, &AuralizationTab::browseDry);
    connect(m_dryFromSrcBtn, &QPushButton::clicked,
            this, &AuralizationTab::chooseDryFromSource);
    connect(rirBrowse, &QPushButton::clicked,
            this, &AuralizationTab::browseRir);
    connect(outBrowse, &QPushButton::clicked,
            this, &AuralizationTab::browseOutput);
    connect(m_gainMode, &QComboBox::currentIndexChanged,
            this, &AuralizationTab::apply);
    connect(m_runBtn, &QPushButton::clicked,
            this, &AuralizationTab::runConvolution);
    connect(outDirBrowse, &QPushButton::clicked,
            this, &AuralizationTab::browseBatchOutDir);
    connect(m_autoAssignBtn, &QPushButton::clicked,
            this, &AuralizationTab::autoAssignRirs);
    connect(m_batchRunBtn, &QPushButton::clicked,
            this, &AuralizationTab::runBatch);
    connect(m_batchCancelBtn, &QPushButton::clicked,
            this, &AuralizationTab::cancelBatch);
    // RIR 列 (列1) の編集を ReceiverRow::rirFile へ反映 (.ofdx に永続化)
    connect(m_batchTable, &QTableWidget::cellChanged,
            this, [this](int row, int col) {
        if (m_updating || col != 1) return;
        AcousticOpts &a = m_p->acoustic();
        if (row < 0 || row >= a.receivers.size()) return;
        const QTableWidgetItem *it = m_batchTable->item(row, 1);
        if (!it) return;
        a.receivers[row].rirFile = it->text().trimmed();
        m_p->touch();   // → changed → refresh (rebuild は m_updating ガード)
    });

    connect(project, &Project::loaded, this, &AuralizationTab::refresh);
    // rirPath は実測RIR分析タブでも編集されるため、変更にも追従する
    connect(project, &Project::changed, this, &AuralizationTab::refresh);
    refresh();
}

// ── model ⇄ widgets ─────────────────────────────────────────────────────────
void AuralizationTab::refresh()
{
    m_updating = true;
    const OperaAcousticSettings &s = m_p->operaAcoustic();
    m_dryPath->setText(s.auralizationDryFile);
    m_rirPath->setText(s.rirPath);
    m_outPath->setText(s.auralizationOutputFile);
    m_gainMode->setCurrentIndex(qBound(0, s.auralizationGainMode, 1));
    // 🎵 音源リストから: 信号が設定された有効な音源が 1 つも無ければ無効化し、
    // 理由をツールチップで示す (押しても何も起きないボタンにしない)
    const bool hasSrcSignal =
        !drySourceCandidates(m_p->acoustic().sources).isEmpty();
    m_dryFromSrcBtn->setEnabled(hasSrcSignal);
    m_dryFromSrcBtn->setToolTip(I18n::tr(hasSrcSignal
                                             ? "aurd_from_source_tip"
                                             : "aurd_from_source_disabled_tip"));
    m_updating = false;
    // ⑤ 受音点表は AcousticTab の編集にも追従する。一括実行中は実行中の
    //    状態表示を上書きしないよう作り直しを保留する (モデル編集は反映済み)
    if (!m_batchBusy) rebuildBatchTable();
}

void AuralizationTab::apply()
{
    if (m_updating) return;
    OperaAcousticSettings &s = m_p->operaAcoustic();
    s.auralizationDryFile = m_dryPath->text();
    s.rirPath = m_rirPath->text();
    s.auralizationOutputFile = m_outPath->text();
    s.auralizationGainMode = m_gainMode->currentIndex();
    m_p->touch();
}

void AuralizationTab::browseDry()
{
    const QString path = QFileDialog::getOpenFileName(
        this, I18n::tr("aur_dry_file"), m_dryPath->text(),
        I18n::tr("rir_wav_filter"));
    if (path.isEmpty()) return;
    m_dryPath->setText(path);
    m_p->operaAcoustic().enabled = true;
    apply();
}

// ① 音源リスト (.ofdx acoustic.sources) の信号をドライ音源として取り込む。
// 音源が「スピーカーから流れる音」を持っていても可聴化に効かなければ
// 意味が無いため、ここで 1 音源を選んで auralizationDryFile に入れる。
// ミックスは行わない (1 音源 = 1 ドライ音源 — 規則 5: 未実装を実装済みに
// 見せない)。合成が要る場合は音響編集・解析タブで作った WAV を選ぶ。
void AuralizationTab::chooseDryFromSource()
{
    const QVector<AcousticSourceRow> &src = m_p->acoustic().sources;
    const QVector<int> cand = drySourceCandidates(src);
    if (cand.isEmpty()) {   // ボタンは無効化済み — 念のための保険
        QMessageBox::information(this, I18n::tr("aurd_title"),
                                 I18n::tr("aurd_from_source_disabled_tip"));
        return;
    }
    // 表示は「#行番号 名前 — ファイル名」。行番号を付けて一意にしてある
    // (同名の音源でも選択結果を取り違えない)。
    QStringList items;
    for (int i : cand) {
        const QString sig = src[i].signal.trimmed();
        const QFileInfo fi(sig);
        const QString shown = fi.fileName().isEmpty() ? sig : fi.fileName();
        QString label = QStringLiteral("#%1 %2 — %3")
                            .arg(i + 1)
                            .arg(src[i].name.isEmpty() ? QStringLiteral("-")
                                                       : src[i].name,
                                 shown);
        // 実在しないファイルは印を付ける (選べるが嘘の表示はしない)
        if (!fi.exists() || !fi.isFile())
            label += QLatin1Char(' ') + I18n::tr("aurd_missing_mark");
        items << label;
    }
    bool ok = false;
    const QString chosen = QInputDialog::getItem(
        this, I18n::tr("aurd_title"), I18n::tr("aurd_prompt"), items, 0,
        /*editable=*/false, &ok);
    if (!ok) return;
    const int sel = items.indexOf(chosen);
    if (sel < 0 || sel >= cand.size()) return;
    if (!setDryFromSource(*m_p, cand[sel])) return;
    m_p->touch();   // → changed → refresh() がドライ欄を更新する
    m_status->setText(I18n::tr("aurd_applied").arg(cand[sel] + 1));
}

void AuralizationTab::browseRir()
{
    const QString path = QFileDialog::getOpenFileName(
        this, I18n::tr("aur_rir_file"), m_rirPath->text(),
        I18n::tr("rir_wav_filter"));
    if (path.isEmpty()) return;
    m_rirPath->setText(path);
    m_p->operaAcoustic().enabled = true;
    apply();
}

void AuralizationTab::browseOutput()
{
    const QString path = QFileDialog::getSaveFileName(
        this, I18n::tr("aur_output_file"),
        m_outPath->text().isEmpty() ? QStringLiteral("auralized.wav")
                                    : m_outPath->text(),
        I18n::tr("rir_wav_filter"));
    if (path.isEmpty()) return;
    m_outPath->setText(path);
    apply();
}

// ── convolution ─────────────────────────────────────────────────────────────
// 畳み込み + WAV 書き出しは QThread::create + busy ガードで非同期に実行する
// (gui.md: 秒単位の処理を GUI スレッドで同期実行しない。リサンプリングが
// 加わり所要時間が伸び得るため、AcousticSourceTab::loadWavPreview と同じ
// パターンに揃えた)。
void AuralizationTab::runConvolution()
{
    if (m_runBusy || m_batchBusy) return;   // 一括実行とは排他
    apply();
    const OperaAcousticSettings &s = m_p->operaAcoustic();
    if (s.auralizationDryFile.trimmed().isEmpty() ||
        s.rirPath.trimmed().isEmpty()) {
        clearResult(I18n::tr("aur_status_nofile"));
        return;
    }
    // 出力先が未指定なら、実行時に保存先を選択させる
    if (m_outPath->text().trimmed().isEmpty()) {
        browseOutput();
        if (m_outPath->text().trimmed().isEmpty()) {
            clearResult(I18n::tr("aur_status_nooutput"));
            return;
        }
    }

    struct RunData {
        AcousticResult<ConvolutionInfo> res;
        QtAcousticAdapter::RirResampleNote note;
        std::vector<double> dry, wet;
        double fs = 0.0;
        bool prepped = false;   // 音源の前処理を適用したか (注記用)
    };
    auto d = std::make_shared<RunData>();
    const QString dryPath = s.auralizationDryFile;
    const QString rirPath = s.rirPath;
    const QString outPath = m_outPath->text();
    const int gainMode = s.auralizationGainMode;

    m_runBusy = true;
    updateBusyUi();
    m_status->setText(I18n::tr("aur_status_running"));

    // ドライ音源には音源モデリングタブの前処理 (トリム / HPF / ゲイン) を
    // 掛けてから畳み込む。既定 (何もしない) なら読み込んだままのバッファを
    // 使うので、従来と結果はビット一致する。
    const audioedit::SourcePrep prep = tabhelp::sourcePrep(m_p->acoustic());
    QThread *th = QThread::create([d, dryPath, rirPath, outPath, gainMode,
                                   prep] {
        d->res = tabhelp::convolveWithPrep(dryPath, rirPath, outPath, gainMode,
                                           prep, &d->prepped, &d->dry, &d->wet,
                                           &d->fs, &d->note);
    });
    connect(th, &QThread::finished, this, [this, th, d, outPath, gainMode] {
        th->deleteLater();
        m_runBusy = false;
        updateBusyUi();

        const AcousticResult<ConvolutionInfo> &res = d->res;
        const QtAcousticAdapter::RirResampleNote &note = d->note;
        const std::vector<double> &dry = d->dry;
        const std::vector<double> &wet = d->wet;
        const double fs = d->fs;

        if (!res.success()) {
            // UnsupportedSampleRate は「fs が不正で自動変換もできない」場合
            // のみ (単なる不一致は RIR のリサンプリングで続行される —
            // 負債 #12 解消)
            QString msg = I18n::tr("aur_status_error")
                              .arg(QString::fromUtf8(
                                       acousticErrorCodeName(res.errorCode())),
                                   QString::fromStdString(res.message()));
            if (res.errorCode() == kSampleRateMismatch)
                msg += QStringLiteral("\n") + I18n::tr("aur_no_resample_note");
            clearResult(msg);
            return;
        }

        // ③ 結果表示。ピーク / クリップ数は書き出した WAV のサンプルで
        // 測った値 (ゲイン適用時はアダプター側で適用後に測り直している)。
        const bool gainApplied = (gainMode == 1);
        const ConvolutionInfo &info = res.value();
        m_peakLabel->setText(QStringLiteral("%1 (%2 dBFS)")
            .arg(QString::number(info.outputPeak, 'f', 4),
                 QString::number(info.outputPeakDbfs, 'f', 1)));
        m_gainLabel->setText(QStringLiteral("%1 dB")
                .arg(QString::number(info.suggestedGainDb, 'f', 1))
            + (gainApplied ? QStringLiteral(" ") + I18n::tr("aur_gain_applied")
                           : QString()));
        m_clipLabel->setText(info.clipped
            ? I18n::tr("aur_clipped_yes")
                  .arg(QString::number(qulonglong(info.clippedSampleCount)))
            : I18n::tr("aur_clipped_no"));

        QStringList warn;
        // RIR の fs の注記 (変換した事実 + 帯域が足りない場合の警告)。
        // 3 箇所で同じ文言を出すため tabhelp に集約している。
        // 有効帯域はソルバーの metadata.json (source.fmax_hz) を優先する
        // — FDTD はナイキストではなく格子分解能で帯域が決まるため
        const double validBand =
            QtAcousticAdapter::metadataForRir(m_p->operaAcoustic().rirPath)
                .sourceFmaxHz;
        for (const QString &n : tabhelp::rirSampleRateNotes(note.fromHz,
                                                            note.toHz,
                                                            validBand))
            warn << QStringLiteral("• ") + n;
        if (d->prepped)   // 黙って音源を加工しない (適用した事実を必ず出す)
            warn << QStringLiteral("• ") + I18n::tr("aur_src_prep_note");
        if (gainApplied)
            warn << QStringLiteral("• ") + I18n::tr("aur_post_gain_note");
        for (const std::string &w : info.warnings)
            warn << QStringLiteral("• ") + QString::fromStdString(w);
        m_warnings->setText(warn.join(QStringLiteral("\n")));
        m_warnings->setVisible(!warn.isEmpty());

        // ④ A/B 波形
        m_dryPlot->setSeries(waveformSeries(dry, fs, QColor("#0078D4")));
        m_wetPlot->setSeries(waveformSeries(wet, fs, QColor("#2E8B57")));

        m_status->setText(I18n::tr("aur_status_ok").arg(outPath));
    });
    th->start();
}

void AuralizationTab::clearResult(const QString &statusText)
{
    m_peakLabel->setText(QStringLiteral("-"));
    m_gainLabel->setText(QStringLiteral("-"));
    m_clipLabel->setText(QStringLiteral("-"));
    m_warnings->clear();
    m_warnings->setVisible(false);
    m_dryPlot->setSeries({});
    m_wetPlot->setSeries({});
    m_status->setText(statusText);
}

// ── ⑤ 複数受音点の一括可聴化 ────────────────────────────────────────────────
// 命名規則 <ドライ名>_<受音点名>.wav — 決定的な純関数 (ヘッドレス検証用に public)
QStringList AuralizationTab::batchOutputNames(const QString &dryPath,
                                              const QStringList &receiverNames)
{
    QString base = QFileInfo(dryPath).completeBaseName();
    if (base.isEmpty()) base = QStringLiteral("auralized");
    // ファイル名に使えない文字 (Windows 予約文字) と空白を '_' へ
    static const QRegularExpression kBad(
        QStringLiteral("[\\\\/:*?\"<>|\\s]+"));
    QStringList out;
    QSet<QString> used;
    for (int i = 0; i < receiverNames.size(); ++i) {
        QString n = receiverNames.at(i).trimmed();
        n.replace(kBad, QStringLiteral("_"));
        while (n.startsWith(QLatin1Char('_'))) n.remove(0, 1);
        while (n.endsWith(QLatin1Char('_'))) n.chop(1);
        if (n.isEmpty()) n = QStringLiteral("P%1").arg(i + 1);
        const QString candidate = base + QLatin1Char('_') + n;
        // 大文字小文字を区別しないファイルシステム (Windows/macOS) でも
        // 衝突しないよう小文字化した名前で一意性を判定する
        QString uniq = candidate;
        int k = 2;
        while (used.contains(uniq.toLower()))
            uniq = candidate + QLatin1Char('_') + QString::number(k++);
        used.insert(uniq.toLower());
        out << uniq + QStringLiteral(".wav");
    }
    return out;
}

// 出力先フォルダ: 指定があればそれ、無ければプロジェクトのフォルダ、
// 未保存プロジェクトはドライ WAV のフォルダ。どれも無ければ空 (エラー)。
QString AuralizationTab::batchOutputDir() const
{
    const QString dir = m_batchOutDir->text().trimmed();
    if (!dir.isEmpty()) return dir;
    if (!m_p->filePath().isEmpty())
        return QFileInfo(m_p->filePath()).absolutePath();
    const QString dry = m_p->operaAcoustic().auralizationDryFile.trimmed();
    if (!dry.isEmpty()) return QFileInfo(dry).absolutePath();
    return QString();
}

void AuralizationTab::browseBatchOutDir()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this, I18n::tr("aurb_out_dir"), batchOutputDir());
    if (dir.isEmpty()) return;
    m_batchOutDir->setText(dir);
}

// ── ⑤ RIR の自動割当 (フォルダスキャン) ─────────────────────────────────────
// ダイアログの初期フォルダ: 直近のソルバ実行が分かる場合はその出力フォルダ
// (AcousticSolverTab が契約検証済み rir.wav の絶対パスを rirPath へ書く —
// 実測 WAV を指定していた場合もその置き場で妥当)。無ければプロジェクトの
// フォルダ (→ ドライ WAV のフォルダ) = batchOutputDir と同じ解決。
QString AuralizationTab::autoAssignDefaultDir() const
{
    const QString rir = m_p->operaAcoustic().rirPath.trimmed();
    if (!rir.isEmpty()) {
        const QFileInfo fi(rir);
        if (fi.absoluteDir().exists()) return fi.absolutePath();
    }
    return batchOutputDir();
}

void AuralizationTab::autoAssignRirs()
{
    if (m_runBusy || m_batchBusy) return;
    const QString dir = QFileDialog::getExistingDirectory(
        this, I18n::tr("aurb_auto_dir_title"), autoAssignDefaultDir());
    if (dir.isEmpty()) return;
    autoAssignFromDir(dir);
}

// フォルダ直下の *.wav を受音点名との対応規則 (core/RirAutoAssign) で
// 各行へ割り当てる。割当はモデル (ReceiverRow::rirFile) へ書いて
// Project::touch() (.ofdx acoustic.receivers[].rir_file に永続化)。
// 曖昧・不一致の行は割り当てず、理由を行の状態欄に表示する。
int AuralizationTab::autoAssignFromDir(const QString &dirPath)
{
    if (m_runBusy || m_batchBusy) return 0;
    const QStringList wavs = rirauto::listWavFiles(dirPath);
    if (wavs.isEmpty()) {
        m_batchStatus->setText(
            I18n::tr("aurb_auto_nowav").arg(QDir::toNativeSeparators(dirPath)));
        return 0;
    }

    AcousticOpts &a = m_p->acoustic();
    const bool onlyUnset = m_autoOnlyUnset->isChecked();
    QStringList names;
    QVector<bool> eligible;
    for (const ReceiverRow &r : a.receivers) {
        names << r.name;
        // 無効行は常に対象外。既定 (未設定の行のみ ON) では設定済みも守る
        eligible << (r.enabled &&
                     (!onlyUnset || r.rirFile.trimmed().isEmpty()));
    }
    const QVector<rirauto::Assignment> res =
        rirauto::assign(wavs, names, eligible);

    const QDir dir(dirPath);
    int assigned = 0, unassigned = 0;
    for (int i = 0; i < res.size(); ++i) {
        if (!eligible.at(i)) continue;   // 対象外の行は表示も変えない
        const rirauto::Assignment &as = res.at(i);
        if (as.fileIndex >= 0) {
            a.receivers[i].rirFile =
                dir.absoluteFilePath(wavs.at(as.fileIndex));
            ++assigned;
            // 状態欄: 割り当てたファイル名 + 根拠 (どの規則で一致したか)
            const char *ruleKey =
                (as.rule == rirauto::Rule::Exact) ? "aurb_auto_rule_exact"
                : (as.rule == rirauto::Rule::Affix) ? "aurb_auto_rule_affix"
                                                    : "aurb_auto_rule_single";
            setBatchRowStatus(i,
                I18n::tr("aurb_auto_row_assigned").arg(wavs.at(as.fileIndex)),
                I18n::tr(ruleKey) + QStringLiteral("\n")
                    + a.receivers.at(i).rirFile);
        } else {
            ++unassigned;
            if (as.rule == rirauto::Rule::Ambiguous)
                setBatchRowStatus(i,
                    I18n::tr("aurb_auto_row_ambiguous")
                        .arg(as.candidates.join(QStringLiteral(", "))),
                    as.candidates.join(QStringLiteral("\n")));
            else
                setBatchRowStatus(i, I18n::tr("aurb_auto_row_nomatch"));
        }
    }

    m_batchStatus->setText(I18n::tr("aurb_auto_summary")
                               .arg(assigned).arg(unassigned)
                               .arg(QDir::toNativeSeparators(dirPath)));
    if (assigned > 0) m_p->touch();   // → changed → refresh → rebuild (表示反映)
    else              rebuildBatchTable();
    return assigned;
}

// 単発実行 / 一括実行の排他とボタン状態を 1 か所で更新する
void AuralizationTab::updateBusyUi()
{
    const bool idle = !m_runBusy && !m_batchBusy;
    m_runBtn->setEnabled(idle);
    m_batchRunBtn->setEnabled(idle);
    // 一括実行中は状態表示・モデルを書き換えないよう自動割当も止める
    m_autoAssignBtn->setEnabled(idle);
    m_batchCancelBtn->setEnabled(m_batchBusy && !m_batchCancel);
}

// model → 受音点表。直近の一括実行の結果 (m_batchRow*) は行番号で引き継ぐ
void AuralizationTab::rebuildBatchTable()
{
    const bool wasUpdating = m_updating;
    m_updating = true;
    const QVector<ReceiverRow> &rows = m_p->acoustic().receivers;
    m_batchTable->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const ReceiverRow &r = rows.at(i);
        // 受音点名 (空名は既定名) — 無効の受音点は対象外と明示
        QString name = r.name.trimmed().isEmpty()
                           ? QStringLiteral("P%1").arg(i + 1)
                           : r.name;
        if (!r.enabled)
            name += QStringLiteral(" ") + I18n::tr("aurb_name_disabled");
        m_batchTable->setItem(i, 0, roItem(name));

        // RIR ファイル (編集可) + 参照ボタン
        m_batchTable->setItem(i, 1, new QTableWidgetItem(r.rirFile));
        auto *browse = new QPushButton(QStringLiteral("…"), m_batchTable);
        connect(browse, &QPushButton::clicked, this, [this, i] {
            AcousticOpts &a = m_p->acoustic();
            if (i < 0 || i >= a.receivers.size()) return;
            const QString path = QFileDialog::getOpenFileName(
                this, I18n::tr("aurb_col_rir"), a.receivers.at(i).rirFile,
                I18n::tr("rir_wav_filter"));
            if (path.isEmpty()) return;
            a.receivers[i].rirFile = path;
            m_p->touch();   // → refresh → rebuildBatchTable
        });
        m_batchTable->setCellWidget(i, 2, browse);

        // 状態: 直近の結果 > 対象外 (無効) > RIR 未指定 (スキップ理由) > 可能
        QString status, tip;
        if (m_batchRowText.contains(i)) {
            status = m_batchRowText.value(i);
            tip = m_batchRowTip.value(i);
        } else if (!r.enabled) {
            status = I18n::tr("aurb_row_disabled");
        } else if (r.rirFile.trimmed().isEmpty()) {
            status = I18n::tr("aurb_row_norir");
        } else {
            status = I18n::tr("aurb_row_ready");
        }
        QTableWidgetItem *st = roItem(status);
        st->setToolTip(tip);
        m_batchTable->setItem(i, 3, st);

        // 試聴 (外部プレイヤー) — この一括実行で書き出した行のみ有効
        auto *listen = new QPushButton(I18n::tr("aurb_listen"), m_batchTable);
        listen->setToolTip(I18n::tr("aurb_listen_tip"));
        listen->setEnabled(m_batchOutFiles.contains(i));
        connect(listen, &QPushButton::clicked, this, [this, i] {
            const QString path = m_batchOutFiles.value(i);
            if (!path.isEmpty())
                QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        });
        m_batchTable->setCellWidget(i, 4, listen);
    }
    m_updating = wasUpdating;
}

void AuralizationTab::setBatchRowStatus(int row, const QString &text,
                                        const QString &tooltip)
{
    m_batchRowText.insert(row, text);
    if (tooltip.isEmpty()) m_batchRowTip.remove(row);
    else                   m_batchRowTip.insert(row, tooltip);
    if (row < 0 || row >= m_batchTable->rowCount()) return;
    QTableWidgetItem *it = m_batchTable->item(row, 3);
    if (!it) return;
    const bool wasUpdating = m_updating;
    m_updating = true;
    it->setText(text);
    it->setToolTip(tooltip);
    m_updating = wasUpdating;
}

void AuralizationTab::runBatch()
{
    if (m_runBusy || m_batchBusy) return;   // 単発実行とは排他
    apply();
    const QString dryPath = m_p->operaAcoustic().auralizationDryFile.trimmed();
    if (dryPath.isEmpty()) {
        m_batchStatus->setText(I18n::tr("aurb_status_nodry"));
        return;
    }
    const QString outDir = batchOutputDir();
    if (outDir.isEmpty()) {
        m_batchStatus->setText(I18n::tr("aurb_status_nodir"));
        return;
    }

    // 前回の結果表示を消し、有効かつ RIR 指定済みの行だけをジョブにする。
    // 対象外の行にはスキップ理由を先に表示する。
    m_batchRowText.clear();
    m_batchRowTip.clear();
    m_batchOutFiles.clear();
    m_batchJobs.clear();
    m_batchDone = 0;
    m_batchCancel = false;

    const QVector<ReceiverRow> &rows = m_p->acoustic().receivers;
    QStringList names;
    for (const ReceiverRow &r : rows) names << r.name;
    const QStringList files = batchOutputNames(dryPath, names);
    for (int i = 0; i < rows.size(); ++i) {
        if (!rows.at(i).enabled) {
            m_batchRowText.insert(i, I18n::tr("aurb_row_disabled"));
            continue;
        }
        if (rows.at(i).rirFile.trimmed().isEmpty()) {
            m_batchRowText.insert(i, I18n::tr("aurb_row_norir"));
            continue;
        }
        BatchJob job;
        job.row = i;
        job.rirPath = rows.at(i).rirFile;
        job.outPath = QDir(outDir).filePath(files.at(i));
        m_batchJobs.push_back(job);
        m_batchRowText.insert(i, I18n::tr("aurb_row_pending"));
        m_batchRowTip.insert(i, job.outPath);
    }
    rebuildBatchTable();
    if (m_batchJobs.isEmpty()) {
        m_batchStatus->setText(I18n::tr("aurb_status_nojobs"));
        return;
    }
    m_batchBusy = true;
    updateBusyUi();
    startBatchJob(0);
}

// m_batchJobs[jobIdx] を QThread で実行し、完了時に次の行へ進む
// (行間で中断可能。実行中の行は完了まで走る)
void AuralizationTab::startBatchJob(int jobIdx)
{
    if (m_batchCancel || jobIdx >= m_batchJobs.size()) {
        finishBatch(jobIdx);
        return;
    }
    const BatchJob job = m_batchJobs.at(jobIdx);
    m_batchStatus->setText(I18n::tr("aurb_status_running")
                               .arg(jobIdx + 1).arg(m_batchJobs.size()));
    setBatchRowStatus(job.row, I18n::tr("aurb_row_running"), job.outPath);

    struct RunData {
        AcousticResult<ConvolutionInfo> res;
        QtAcousticAdapter::RirResampleNote note;
        bool prepped = false;
    };
    auto d = std::make_shared<RunData>();
    const QString dryPath = m_p->operaAcoustic().auralizationDryFile;
    const int gainMode = qBound(0, m_p->operaAcoustic().auralizationGainMode, 1);
    // 単発実行と同じ前処理を通す (一括だけ違う音になるのを防ぐ)
    const audioedit::SourcePrep prep = tabhelp::sourcePrep(m_p->acoustic());

    QThread *th = QThread::create([d, dryPath, job, gainMode, prep] {
        d->res = tabhelp::convolveWithPrep(dryPath, job.rirPath, job.outPath,
                                           gainMode, prep, &d->prepped,
                                           nullptr, nullptr, nullptr, &d->note);
    });
    connect(th, &QThread::finished, this, [this, th, d, job, jobIdx] {
        th->deleteLater();
        if (d->res.success()) {
            const ConvolutionInfo &info = d->res.value();
            // 行ごとの結果ログ: 出力名 + リサンプリングの明示 + クリップ警告
            QString txt = I18n::tr("aurb_row_done")
                              .arg(QFileInfo(job.outPath).fileName());
            QStringList tip;
            tip << job.outPath;
            if (d->note.resampled) {
                const QString rs = I18n::tr("aurb_row_resampled")
                    .arg(QString::number(qRound64(d->note.fromHz)),
                         QString::number(qRound64(d->note.toHz)));
                txt += QStringLiteral(" ") + rs;
            }
            // 行の文字列は短く保ち、詳しい注記 (fs 変換・帯域制限) は
            // ツールチップへ (単発実行と同じ文言)
            tip << tabhelp::rirSampleRateNotes(
                d->note.fromHz, d->note.toHz,
                QtAcousticAdapter::metadataForRir(job.rirPath).sourceFmaxHz);
            if (info.clipped) {
                const QString cl = I18n::tr("aurb_row_clipped")
                    .arg(QString::number(qulonglong(info.clippedSampleCount)));
                txt += QStringLiteral(" ") + cl;
                tip << cl;
            }
            if (d->prepped) tip << I18n::tr("aur_src_prep_note");
            for (const std::string &w : info.warnings)
                tip << QString::fromStdString(w);
            setBatchRowStatus(job.row, txt, tip.join(QStringLiteral("\n")));
            m_batchOutFiles.insert(job.row, job.outPath);
            if (QWidget *w = m_batchTable->cellWidget(job.row, 4))
                w->setEnabled(true);   // 試聴ボタン
            ++m_batchDone;
        } else {
            setBatchRowStatus(job.row,
                I18n::tr("aurb_row_error")
                    .arg(QString::fromUtf8(
                        acousticErrorCodeName(d->res.errorCode()))),
                QString::fromStdString(d->res.message()));
        }
        startBatchJob(jobIdx + 1);
    });
    th->start();
}

void AuralizationTab::finishBatch(int nextIdx)
{
    // 中断時: 未実行のまま残った行へ明示する
    for (int k = nextIdx; k < m_batchJobs.size(); ++k)
        setBatchRowStatus(m_batchJobs.at(k).row,
                          I18n::tr("aurb_row_cancelled"));
    const int total = m_batchJobs.size();
    const bool cancelled = (nextIdx < total);
    m_batchBusy = false;
    m_batchCancel = false;
    updateBusyUi();
    m_batchStatus->setText(
        (cancelled ? I18n::tr("aurb_status_cancelled")
                   : I18n::tr("aurb_status_done"))
            .arg(m_batchDone).arg(total));
}

void AuralizationTab::cancelBatch()
{
    if (!m_batchBusy || m_batchCancel) return;
    m_batchCancel = true;   // 実行中の行の完了後、次の行へ進まず終了する
    updateBusyUi();
    m_batchStatus->setText(I18n::tr("aurb_status_cancelling"));
}
