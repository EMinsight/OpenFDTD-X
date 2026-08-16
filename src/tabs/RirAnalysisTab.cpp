// RirAnalysisTab.cpp
#include "RirAnalysisTab.h"
#include "../core/Project.h"
#include "../acoustics/qt/QtAcousticAdapter.h"
#include "../acoustics/qt/AcousticResultModel.h"
#include "../widgets/MiniPlot.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"
#include "TabHelpers.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

using namespace ofd;
using namespace ofd::acoustics;
// 品質バッジ / 読み取り専用セル / 波形包絡線 / テキスト保存は
// src/tabs/TabHelpers.{h,cpp} に集約 (3 タブで共有)。
using namespace ofd::tabhelp;

namespace {

// 実測 STI の等級 (IEC 60268-16 の区分 0.30 / 0.45 / 0.60 / 0.75)
QString stiGradeText(double sti)
{
    if (sti < 0.30) return I18n::tr("rir_sti_bad");
    if (sti < 0.45) return I18n::tr("rir_sti_poor");
    if (sti < 0.60) return I18n::tr("rir_sti_fair");
    if (sti < 0.75) return I18n::tr("rir_sti_good");
    return I18n::tr("rir_sti_excellent");
}

} // namespace

// ── construction ────────────────────────────────────────────────────────────
RirAnalysisTab::RirAnalysisTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // 統計推定 (ホール解析) / 実測RIR分析 (本タブ) / シミュレーションRIR分析 の区別
    auto *hint = new QLabel(I18n::tr("rir_model_hint"), body);
    hint->setWordWrap(true);
    v->addWidget(hint);

    // ① 入力
    auto *sIn = new SectionBox(I18n::tr("rir_input_section"), body);
    auto *fileRow = new QHBoxLayout();
    m_rirPath = new QLineEdit(sIn);
    m_rirPath->setReadOnly(true);
    m_rirPath->setPlaceholderText(I18n::tr("rir_file_placeholder"));
    auto *browse = new QPushButton(I18n::tr("rir_browse"), sIn);
    fileRow->addWidget(m_rirPath, 1);
    fileRow->addWidget(browse);
    sIn->form()->addRow(I18n::tr("rir_file"), fileRow);
    // 音響ソルバ連携の実行結果を設定したときだけ出すヒント (既定は非表示)。
    // 「次に ▶ 分析 を押せばよい」ことが分かる文言にする。
    m_solverHint = new QLabel(sIn);
    m_solverHint->setObjectName(QStringLiteral("rirSolverHint"));
    m_solverHint->setWordWrap(true);
    m_solverHint->setVisible(false);
    m_solverHint->setStyleSheet(QStringLiteral("color:#2E8B57;"));
    sIn->form()->addRow(QString(), m_solverHint);

    m_channel = new QComboBox(sIn);
    m_channel->addItems({ I18n::tr("rir_ch_left"), I18n::tr("rir_ch_right"),
                          I18n::tr("rir_ch_mono") });
    sIn->form()->addRow(I18n::tr("rir_channel"), m_channel);

    m_calibration = new QComboBox(sIn);
    m_calibration->addItems({ I18n::tr("rir_calib_absolute"),
                              I18n::tr("rir_calib_relative"),
                              I18n::tr("rir_calib_uncalibrated") });
    sIn->form()->addRow(I18n::tr("rir_calibration"), m_calibration);

    // 校正オフセット (dBFS → dB SPL)。Absolute のときだけ有効
    // (未校正なのにオフセットが効いていると誤解されないようグレーアウト)。
    m_calibOffset = new QDoubleSpinBox(sIn);
    m_calibOffset->setRange(-200.0, 200.0);
    m_calibOffset->setDecimals(1);
    m_calibOffset->setSuffix(QStringLiteral(" dB"));
    m_calibOffset->setToolTip(I18n::tr("rir_calib_offset_tip"));
    m_calibOffsetLabel = new QLabel(I18n::tr("rir_calib_offset"), sIn);
    m_calibOffsetLabel->setToolTip(I18n::tr("rir_calib_offset_tip"));
    sIn->form()->addRow(m_calibOffsetLabel, m_calibOffset);

    m_directMethod = new QComboBox(sIn);
    m_directMethod->addItems({ I18n::tr("rir_dm_peak"),
                               I18n::tr("rir_dm_envelope"),
                               I18n::tr("rir_dm_movingrms") });
    sIn->form()->addRow(I18n::tr("rir_direct_method"), m_directMethod);

    m_bandMode = new QComboBox(sIn);
    m_bandMode->addItems({ I18n::tr("rir_bm_compat6"), I18n::tr("rir_bm_oct"),
                           I18n::tr("rir_bm_thirdoct"),
                           I18n::tr("rir_bm_formant") });
    sIn->form()->addRow(I18n::tr("rir_band_mode"), m_bandMode);

    m_noiseCorr = new QCheckBox(I18n::tr("rir_noise_correction"), sIn);
    sIn->form()->addRow(QString(), m_noiseCorr);

    m_minDr = new QDoubleSpinBox(sIn);
    m_minDr->setRange(10.0, 80.0);
    m_minDr->setDecimals(1);
    m_minDr->setSuffix(QStringLiteral(" dB"));
    sIn->form()->addRow(I18n::tr("rir_min_dr"), m_minDr);

    // ── G (音の強さ) の基準 ──
    // 基準録音 (自由音場 10 m) が無ければ G は出さない。絶対 SPL 校正とは
    // 独立で、要るのは「同じ利得系で録られていること」だけ。
    m_gRefMode = new QComboBox(sIn);
    m_gRefMode->addItems({ I18n::tr("rir_g_mode_none"),
                           I18n::tr("rir_g_mode_file"),
                           I18n::tr("rir_g_mode_level") });
    sIn->form()->addRow(I18n::tr("rir_g_ref_mode"), m_gRefMode);

    auto *gFileRow = new QHBoxLayout();
    m_gRefFile = new QLineEdit(sIn);
    m_gRefFile->setReadOnly(true);
    m_gRefFile->setPlaceholderText(I18n::tr("rir_g_ref_file_placeholder"));
    m_gRefBrowse = new QPushButton(I18n::tr("rir_browse"), sIn);
    gFileRow->addWidget(m_gRefFile, 1);
    gFileRow->addWidget(m_gRefBrowse);
    m_gRefFileLabel = new QLabel(I18n::tr("rir_g_ref_file"), sIn);
    sIn->form()->addRow(m_gRefFileLabel, gFileRow);

    m_gRefLevel = new QDoubleSpinBox(sIn);
    m_gRefLevel->setRange(-200.0, 200.0);
    m_gRefLevel->setDecimals(2);
    m_gRefLevel->setSuffix(QStringLiteral(" dB"));
    m_gRefLevel->setToolTip(I18n::tr("rir_g_ref_level_tip"));
    m_gRefLevelLabel = new QLabel(I18n::tr("rir_g_ref_level"), sIn);
    m_gRefLevelLabel->setToolTip(I18n::tr("rir_g_ref_level_tip"));
    sIn->form()->addRow(m_gRefLevelLabel, m_gRefLevel);

    m_gRefDistance = new QDoubleSpinBox(sIn);
    m_gRefDistance->setRange(0.1, 1000.0);
    m_gRefDistance->setDecimals(2);
    m_gRefDistance->setSuffix(QStringLiteral(" m"));
    m_gRefDistance->setToolTip(I18n::tr("rir_g_ref_distance_tip"));
    m_gRefDistanceLabel = new QLabel(I18n::tr("rir_g_ref_distance"), sIn);
    m_gRefDistanceLabel->setToolTip(I18n::tr("rir_g_ref_distance_tip"));
    sIn->form()->addRow(m_gRefDistanceLabel, m_gRefDistance);
    v->addWidget(sIn);

    // ② 実行
    auto *sRun = new SectionBox(I18n::tr("rir_run_section"), body);
    auto *runRow = new QHBoxLayout();
    m_runBtn = new QPushButton(I18n::tr("rir_run"), sRun);
    m_status = new QLabel(I18n::tr("rir_status_idle"), sRun);
    m_status->setWordWrap(true);
    runRow->addWidget(m_runBtn);
    runRow->addWidget(m_status, 1);
    sRun->vbox()->addLayout(runRow);
    v->addWidget(sRun);

    // ③ 結果
    auto *sRes = new SectionBox(I18n::tr("rir_result_section"), body);
    m_metricTable = new QTableWidget(0, 6, sRes);
    m_metricTable->setHorizontalHeaderLabels({
        I18n::tr("rir_metric"), I18n::tr("rir_band"), I18n::tr("rir_value"),
        I18n::tr("rir_unit"), I18n::tr("rir_quality"), I18n::tr("rir_note") });
    m_metricTable->horizontalHeader()->setSectionResizeMode(
        QHeaderView::ResizeToContents);
    m_metricTable->horizontalHeader()->setStretchLastSection(true);
    m_metricTable->verticalHeader()->setVisible(false);
    m_metricTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_metricTable->setMinimumHeight(220);
    sRes->vbox()->addWidget(m_metricTable);
    // 舞台支援 (ST) の測定前提の注記。値そのものは物理量として正しいが、
    // ISO 3382-1 Annex C の指標としては舞台上 1 m 測定の RIR が前提で、
    // それは計算側では確認できない (絶対規則 5/6 の趣旨で明示する)
    auto *stNote = new QLabel(I18n::tr("rir_st_note"), sRes);
    stNote->setWordWrap(true);
    stNote->setStyleSheet("color:#888; font-size:10px;");
    sRes->vbox()->addWidget(stNote);
    // 実測 STI (IEC 60268-16 間接法) — 表と別枠。等級は規格の区分。
    m_stiLabel = new QLabel(sRes);
    m_stiLabel->setWordWrap(true);
    m_stiLabel->setVisible(false);
    sRes->vbox()->addWidget(m_stiLabel);
    auto *stiNote = new QLabel(I18n::tr("rir_sti_note"), sRes);
    stiNote->setWordWrap(true);
    stiNote->setStyleSheet("color:#888; font-size:10px;");
    sRes->vbox()->addWidget(stiNote);
    // G (音の強さ, ISO 3382-1) — 基準録音がある時だけ値が出る
    m_strengthLabel = new QLabel(sRes);
    m_strengthLabel->setWordWrap(true);
    m_strengthLabel->setVisible(false);
    sRes->vbox()->addWidget(m_strengthLabel);
    auto *gNote = new QLabel(I18n::tr("rir_g_note"), sRes);
    gNote->setWordWrap(true);
    gNote->setStyleSheet("color:#888; font-size:10px;");
    sRes->vbox()->addWidget(gNote);
    m_warnings = new QLabel(sRes);
    m_warnings->setWordWrap(true);
    m_warnings->setVisible(false);
    sRes->vbox()->addWidget(m_warnings);
    v->addWidget(sRes);

    // ④ プロット + 初期反射一覧
    auto *sPlot = new SectionBox(I18n::tr("rir_plot_section"), body);
    auto *waveLabel = new QLabel(I18n::tr("rir_waveform"), sPlot);
    sPlot->vbox()->addWidget(waveLabel);
    m_wavePlot = new MiniPlot(sPlot);
    m_wavePlot->setLabels(I18n::tr("rir_time_ms"), I18n::tr("rir_amplitude"));
    m_wavePlot->setMinimumHeight(130);
    sPlot->vbox()->addWidget(m_wavePlot);
    auto *decayLabel = new QLabel(I18n::tr("rir_decay"), sPlot);
    sPlot->vbox()->addWidget(decayLabel);
    m_decayPlot = new MiniPlot(sPlot);
    m_decayPlot->setLabels(I18n::tr("rir_time_ms"), I18n::tr("rir_level_db"));
    m_decayPlot->setMinimumHeight(150);
    sPlot->vbox()->addWidget(m_decayPlot);
    // MiniPlot はマーカー注釈を持たないため、初期反射は表で示す
    auto *reflLabel = new QLabel(I18n::tr("rir_refl_section"), sPlot);
    sPlot->vbox()->addWidget(reflLabel);
    m_reflTable = new QTableWidget(0, 5, sPlot);
    m_reflTable->setHorizontalHeaderLabels({
        I18n::tr("rir_refl_no"), I18n::tr("rir_refl_time"),
        I18n::tr("rir_refl_delay"), I18n::tr("rir_refl_level"),
        I18n::tr("rir_refl_bin") });
    m_reflTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_reflTable->verticalHeader()->setVisible(false);
    m_reflTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_reflTable->setMinimumHeight(140);
    sPlot->vbox()->addWidget(m_reflTable);
    v->addWidget(sPlot);

    // ⑤ 出力
    auto *sExp = new SectionBox(I18n::tr("rir_export_section"), body);
    auto *expRow = new QHBoxLayout();
    m_csvBtn = new QPushButton(I18n::tr("rir_export_csv"), sExp);
    m_jsonBtn = new QPushButton(I18n::tr("rir_export_json"), sExp);
    m_csvBtn->setEnabled(false);
    m_jsonBtn->setEnabled(false);
    expRow->addWidget(m_csvBtn);
    expRow->addWidget(m_jsonBtn);
    expRow->addStretch(1);
    sExp->vbox()->addLayout(expRow);
    v->addWidget(sExp);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    connect(browse, &QPushButton::clicked, this, &RirAnalysisTab::browseRir);
    connect(m_channel, &QComboBox::currentIndexChanged,
            this, &RirAnalysisTab::apply);
    connect(m_calibration, &QComboBox::currentIndexChanged,
            this, &RirAnalysisTab::apply);
    connect(m_calibOffset, &QDoubleSpinBox::valueChanged,
            this, &RirAnalysisTab::apply);
    connect(m_directMethod, &QComboBox::currentIndexChanged,
            this, &RirAnalysisTab::apply);
    connect(m_bandMode, &QComboBox::currentIndexChanged,
            this, &RirAnalysisTab::apply);
    connect(m_noiseCorr, &QCheckBox::toggled, this, &RirAnalysisTab::apply);
    connect(m_minDr, &QDoubleSpinBox::valueChanged,
            this, &RirAnalysisTab::apply);
    connect(m_gRefMode, &QComboBox::currentIndexChanged,
            this, &RirAnalysisTab::apply);
    connect(m_gRefBrowse, &QPushButton::clicked,
            this, &RirAnalysisTab::browseStrengthRef);
    connect(m_gRefLevel, &QDoubleSpinBox::valueChanged,
            this, &RirAnalysisTab::apply);
    connect(m_gRefDistance, &QDoubleSpinBox::valueChanged,
            this, &RirAnalysisTab::apply);
    connect(m_runBtn, &QPushButton::clicked,
            this, &RirAnalysisTab::runAnalysis);
    connect(m_csvBtn, &QPushButton::clicked, this, &RirAnalysisTab::exportCsv);
    connect(m_jsonBtn, &QPushButton::clicked,
            this, &RirAnalysisTab::exportJson);

    connect(project, &Project::loaded, this, &RirAnalysisTab::refresh);
    refresh();
}

// ── model ⇄ widgets ─────────────────────────────────────────────────────────
void RirAnalysisTab::refresh()
{
    m_updating = true;
    const OperaAcousticSettings &s = m_p->operaAcoustic();
    m_rirPath->setText(s.rirPath);
    m_channel->setCurrentIndex(qBound(0, s.channelMode, 2));
    m_calibration->setCurrentIndex(qBound(0, s.calibrationState, 2));
    m_calibOffset->setValue(s.calibrationOffsetDb);
    updateCalibOffsetEnabled();
    m_directMethod->setCurrentIndex(qBound(0, s.directSoundMethod, 2));
    m_bandMode->setCurrentIndex(qBound(0, s.bandMode, 3));
    m_noiseCorr->setChecked(s.noiseCorrection);
    m_minDr->setValue(s.minimumDynamicRangeDb);
    m_gRefMode->setCurrentIndex(qBound(0, s.strengthRefMode, 2));
    m_gRefFile->setText(s.strengthRefFile);
    m_gRefLevel->setValue(s.strengthRefLevelDb);
    m_gRefDistance->setValue(s.strengthRefDistanceM);
    updateStrengthRefEnabled();
    m_updating = false;
}

void RirAnalysisTab::apply()
{
    if (m_updating) return;
    OperaAcousticSettings &s = m_p->operaAcoustic();
    s.rirPath = m_rirPath->text();
    s.channelMode = m_channel->currentIndex();
    s.calibrationState = m_calibration->currentIndex();
    // モデルには常に入力値を保存する (校正状態を戻したときに値が消えない)。
    // Absolute 以外で分析へ渡さないゲートは QtAcousticAdapter 側が持つ。
    s.calibrationOffsetDb = m_calibOffset->value();
    s.directSoundMethod = m_directMethod->currentIndex();
    s.bandMode = m_bandMode->currentIndex();
    s.noiseCorrection = m_noiseCorr->isChecked();
    s.minimumDynamicRangeDb = m_minDr->value();
    s.strengthRefMode = m_gRefMode->currentIndex();
    s.strengthRefFile = m_gRefFile->text();
    s.strengthRefLevelDb = m_gRefLevel->value();
    s.strengthRefDistanceM = m_gRefDistance->value();
    updateCalibOffsetEnabled();
    updateStrengthRefEnabled();
    m_p->touch();
}

// 校正オフセットは Absolute のときだけ編集可能 (誤解防止)。
void RirAnalysisTab::updateCalibOffsetEnabled()
{
    const bool absolute = (m_calibration->currentIndex() == 0);
    m_calibOffset->setEnabled(absolute);
    m_calibOffsetLabel->setEnabled(absolute);
}

// G の基準はモードに応じて必要な欄だけ有効化する
// (使われない欄が編集できると、設定したのに効かないように見える)。
void RirAnalysisTab::updateStrengthRefEnabled()
{
    const int mode = m_gRefMode->currentIndex();
    const bool useFile  = (mode == 1);
    const bool useLevel = (mode == 2);
    m_gRefFile->setEnabled(useFile);
    m_gRefBrowse->setEnabled(useFile);
    m_gRefFileLabel->setEnabled(useFile);
    m_gRefLevel->setEnabled(useLevel);
    m_gRefLevelLabel->setEnabled(useLevel);
    // 距離補正はどちらのモードでも効く (基準が 10 m でない場合の補正)
    m_gRefDistance->setEnabled(mode != 0);
    m_gRefDistanceLabel->setEnabled(mode != 0);
}

void RirAnalysisTab::browseStrengthRef()
{
    const QString path = QFileDialog::getOpenFileName(
        this, I18n::tr("rir_g_ref_file"), m_gRefFile->text(),
        I18n::tr("rir_wav_filter"));
    if (path.isEmpty()) return;
    m_gRefFile->setText(path);
    apply();
}

// 音響ソルバ連携タブが rir.wav を設定した — WAV 欄を最新化してヒントを出す。
// (このタブは Project::changed に繋いでいない: 編集中の入力欄を他タブの
//  touch() で上書きしないため。実行完了という単発イベントだけを受け取る)
void RirAnalysisTab::applySolverRir(const QString &path)
{
    refresh();   // rirPath はモデル側で更新済み
    if (m_solverHint) {
        m_solverHint->setText(
            I18n::tr("rir_solver_assigned").arg(QFileInfo(path).fileName()));
        m_solverHint->setToolTip(path);
        m_solverHint->setVisible(true);
    }
}

void RirAnalysisTab::browseRir()
{
    const QString path = QFileDialog::getOpenFileName(
        this, I18n::tr("rir_file"), m_rirPath->text(),
        I18n::tr("rir_wav_filter"));
    if (path.isEmpty()) return;
    m_rirPath->setText(path);
    // 利用者が別のファイルを選んだ — ソルバ結果のヒントは合わなくなる
    if (m_solverHint) m_solverHint->setVisible(false);
    m_p->operaAcoustic().enabled = true;   // 実測RIR分析を使う意思表示
    apply();
}

// ── analysis ────────────────────────────────────────────────────────────────
void RirAnalysisTab::runAnalysis()
{
    apply();
    const OperaAcousticSettings &s = m_p->operaAcoustic();
    if (s.rirPath.trimmed().isEmpty()) {
        clearResult(I18n::tr("rir_status_nofile"));
        return;
    }

    std::vector<double> samples;
    double fs = 0.0;
    const AcousticResult<RirAnalysisResult> res =
        QtAcousticAdapter::analyzeFile(s, &samples, &fs);
    if (!res.success()) {
        clearResult(I18n::tr("rir_status_error")
                        .arg(QString::fromUtf8(
                                 acousticErrorCodeName(res.errorCode())),
                             QString::fromStdString(res.message())));
        return;
    }
    showResult(res.value(), samples, fs);
}

void RirAnalysisTab::clearResult(const QString &statusText)
{
    m_hasResult = false;
    m_result = RirAnalysisResult();
    m_metricTable->setRowCount(0);
    m_reflTable->setRowCount(0);
    m_warnings->clear();
    m_warnings->setVisible(false);
    m_stiLabel->clear();
    m_stiLabel->setVisible(false);
    m_strengthLabel->clear();
    m_strengthLabel->setVisible(false);
    m_wavePlot->setSeries({});
    m_decayPlot->setSeries({});
    m_csvBtn->setEnabled(false);
    m_jsonBtn->setEnabled(false);
    m_status->setText(statusText);
}

void RirAnalysisTab::showResult(const RirAnalysisResult &result,
                                const std::vector<double> &samples,
                                double sampleRateHz)
{
    m_result = result;
    m_hasResult = true;

    // ③ 指標表
    const QVector<AcousticResultRow> rows =
        AcousticResultModel::metricRows(result);
    m_metricTable->setRowCount(rows.size());
    for (int r = 0; r < rows.size(); ++r) {
        const AcousticResultRow &row = rows[r];
        m_metricTable->setItem(r, 0, roItem(row.metric));
        m_metricTable->setItem(r, 1, roItem(row.band));
        QString valueText;
        if (row.valid) {
            valueText = row.valueText;
        } else {
            valueText = row.warning.isEmpty()
                ? I18n::tr("rir_not_computable")
                : QStringLiteral("%1 (%2)")
                      .arg(I18n::tr("rir_not_computable"), row.warning);
        }
        m_metricTable->setItem(r, 2, roItem(valueText));
        m_metricTable->setItem(r, 3, roItem(row.valid ? row.unit : QString()));
        auto *badge = roItem(qualityBadge(row.quality));
        badge->setForeground(qualityColor(row.quality));
        m_metricTable->setItem(r, 4, badge);
        m_metricTable->setItem(r, 5,
                               roItem(row.valid ? row.warning : QString()));
    }

    // 実測 STI (IEC 60268-16 間接法)
    if (result.sti.sti.valid) {
        m_stiLabel->setText(I18n::tr("rir_sti_value")
                                .arg(QString::number(result.sti.sti.value, 'f', 2),
                                     stiGradeText(result.sti.sti.value)));
    } else {
        m_stiLabel->setText(I18n::tr("rir_sti_invalid")
                                .arg(result.sti.warning.empty()
                                         ? I18n::tr("rir_not_computable")
                                         : QString::fromStdString(
                                               result.sti.warning)));
    }
    m_stiLabel->setVisible(true);

    // G (音の強さ)。基準録音が無いときは「基準未設定」と明示して値は出さない。
    {
        const SoundStrengthResult &g = result.strength;
        QString text;
        if (g.g.valid) {
            QStringList parts;
            parts << I18n::tr("rir_g_value")
                         .arg(QString::number(g.g.value, 'f', 1));
            if (g.gEarly.valid && g.gLate.valid) {
                parts << I18n::tr("rir_g_early_late")
                             .arg(QString::number(g.gEarly.value, 'f', 1),
                                  QString::number(g.gLate.value, 'f', 1));
            }
            if (std::fabs(g.distanceCorrectionDb) > 1e-9) {
                parts << I18n::tr("rir_g_distance_corr")
                             .arg(QString::number(g.distanceCorrectionDb,
                                                  'f', 1));
            }
            text = parts.join(QStringLiteral(" / "));
        } else {
            text = I18n::tr("rir_g_invalid")
                       .arg(g.warning.empty()
                                ? I18n::tr("rir_not_computable")
                                : QString::fromStdString(g.warning));
        }
        m_strengthLabel->setText(text);
        m_strengthLabel->setVisible(true);
    }

    // 警告リスト
    QStringList warn;
    for (const std::string &w : result.warnings)
        warn << QStringLiteral("• ") + QString::fromStdString(w);
    m_warnings->setText(warn.join(QStringLiteral("\n")));
    m_warnings->setVisible(!warn.isEmpty());

    // ④ 波形 + 減衰カーブ
    const OperaAcousticSettings &s = m_p->operaAcoustic();
    {
        QVector<QPointF> top, bottom;
        envelopeSeries(samples, sampleRateHz, 1200, TimeUnit::Milliseconds,
                       top, bottom);
        MiniSeries hi;  hi.pts = top;     hi.color = QColor("#2E8B57");
        MiniSeries lo;  lo.pts = bottom;  lo.color = QColor("#2E8B57");
        m_wavePlot->setSeries({ hi, lo });
    }
    {
        const SchroederResult decay =
            QtAcousticAdapter::decayCurve(samples, sampleRateHz, s);
        QVector<MiniSeries> series;
        if (decay.valid && !decay.decayDb.empty()) {
            const int n = int(decay.decayDb.size());
            const int stride = std::max(1, n / 1500);
            MiniSeries d;
            d.color = QColor("#0078D4");
            d.label = I18n::tr("rir_decay_label");
            for (int i = 0; i < n; i += stride)
                d.pts.push_back(QPointF(i / sampleRateHz * 1000.0,
                                        decay.decayDb[i]));
            series.push_back(d);
            // ノイズフロアの目安線 (破線)
            MiniSeries nf;
            nf.color = QColor("#C0392B");
            nf.dashed = true;
            nf.label = I18n::tr("rir_noise_floor");
            nf.pts.push_back(QPointF(0.0, decay.noiseFloorDb));
            nf.pts.push_back(QPointF((n - 1) / sampleRateHz * 1000.0,
                                     decay.noiseFloorDb));
            series.push_back(nf);
        }
        m_decayPlot->setSeries(series);
    }

    // ④ 初期反射一覧 (0-20 / 20-80 / 80-200 / 200+ ms)
    m_reflTable->setRowCount(int(result.reflections.size()));
    for (int i = 0; i < int(result.reflections.size()); ++i) {
        const ReflectionEvent &e = result.reflections[std::size_t(i)];
        m_reflTable->setItem(i, 0, roItem(QString::number(i + 1)));
        m_reflTable->setItem(i, 1,
            roItem(QString::number(e.arrivalTime * 1000.0, 'f', 2)));
        m_reflTable->setItem(i, 2,
            roItem(QString::number(e.delayFromDirect * 1000.0, 'f', 2)));
        m_reflTable->setItem(i, 3,
            roItem(QString::number(e.relativeLevelDb, 'f', 1)));
        m_reflTable->setItem(i, 4,
            roItem(AcousticResultModel::reflectionBinLabel(e.delayFromDirect)));
    }

    // ⑤ 出力ボタン有効化 + ステータス
    m_csvBtn->setEnabled(true);
    m_jsonBtn->setEnabled(true);
    m_status->setText(I18n::tr("rir_status_ok")
        .arg(qualityBadge(AcousticResultModel::qualityToken(
                 result.overallQuality)),
             QString::number(result.preprocess.dynamicRangeDb, 'f', 1),
             QString::number(result.directSound.timeSeconds * 1000.0, 'f', 2)));
}

// ── export ──────────────────────────────────────────────────────────────────
void RirAnalysisTab::exportCsv()
{
    if (!m_hasResult) return;
    saveTextFile(this, I18n::tr("rir_export_csv"),
                 QStringLiteral("rir_analysis.csv"), "CSV (*.csv)",
                 AcousticResultModel::toCsv(m_result));
}

void RirAnalysisTab::exportJson()
{
    if (!m_hasResult) return;
    saveTextFile(this, I18n::tr("rir_export_json"),
                 QStringLiteral("rir_analysis.json"), "JSON (*.json)",
                 AcousticResultModel::toJson(m_result));
}
