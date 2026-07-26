// AuralizationTab.cpp
#include "AuralizationTab.h"
#include "../core/Project.h"
#include "../acoustics/qt/QtAcousticAdapter.h"
#include "../widgets/MiniPlot.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"

#include <QComboBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <vector>

using namespace ofd;
using namespace ofd::acoustics;

namespace {

// 長い系列を maxBins 区間の min/max 包絡線 2 系列に間引く (波形表示用)
void envelopeSeries(const std::vector<double> &x, double fs, int maxBins,
                    QVector<QPointF> &top, QVector<QPointF> &bottom)
{
    top.clear(); bottom.clear();
    if (x.empty() || fs <= 0) return;
    const int n = int(x.size());
    const int bins = std::min(maxBins, n);
    const double perBin = double(n) / bins;
    top.reserve(bins); bottom.reserve(bins);
    for (int b = 0; b < bins; ++b) {
        const int i0 = int(b * perBin);
        const int i1 = std::min(n, std::max(i0 + 1, int((b + 1) * perBin)));
        double lo = x[i0], hi = x[i0];
        for (int i = i0 + 1; i < i1; ++i) {
            lo = std::min(lo, x[i]);
            hi = std::max(hi, x[i]);
        }
        const double tS = (i0 + (i1 - 1 - i0) * 0.5) / fs;
        top.push_back(QPointF(tS, hi));
        bottom.push_back(QPointF(tS, lo));
    }
}

QVector<MiniSeries> waveformSeries(const std::vector<double> &x, double fs,
                                   const QColor &color)
{
    QVector<QPointF> top, bottom;
    envelopeSeries(x, fs, 1200, top, bottom);
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

    // 概要: 畳み込み可聴化。自動正規化・リサンプリングは行わない。
    auto *hint = new QLabel(I18n::tr("aur_model_hint"), body);
    hint->setWordWrap(true);
    v->addWidget(hint);

    // ① 入力
    auto *sIn = new SectionBox(I18n::tr("aur_input_section"), body);
    QPushButton *dryBrowse = nullptr, *rirBrowse = nullptr, *outBrowse = nullptr;
    sIn->form()->addRow(I18n::tr("aur_dry_file"),
        pathRow(m_dryPath, dryBrowse, sIn, I18n::tr("rir_file_placeholder")));
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

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    connect(dryBrowse, &QPushButton::clicked,
            this, &AuralizationTab::browseDry);
    connect(rirBrowse, &QPushButton::clicked,
            this, &AuralizationTab::browseRir);
    connect(outBrowse, &QPushButton::clicked,
            this, &AuralizationTab::browseOutput);
    connect(m_gainMode, &QComboBox::currentIndexChanged,
            this, &AuralizationTab::apply);
    connect(m_runBtn, &QPushButton::clicked,
            this, &AuralizationTab::runConvolution);

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
    m_updating = false;
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
void AuralizationTab::runConvolution()
{
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

    std::vector<double> dry, wet;
    double fs = 0.0;
    const AcousticResult<ConvolutionInfo> res =
        QtAcousticAdapter::convolveFiles(
            s.auralizationDryFile, s.rirPath, m_outPath->text(),
            s.auralizationGainMode, &dry, &wet, &fs);
    if (!res.success()) {
        // fs 不一致 (UnsupportedSampleRate) はリサンプリングしない旨も明示
        QString msg = I18n::tr("aur_status_error")
                          .arg(QString::fromUtf8(
                                   acousticErrorCodeName(res.errorCode())),
                               QString::fromStdString(res.message()));
        if (res.errorCode() == kSampleRateMismatch)
            msg += QStringLiteral("\n") + I18n::tr("aur_no_resample_note");
        clearResult(msg);
        return;
    }

    // ③ 結果表示 (畳み込み値そのままの報告。自動正規化はしない)
    const ConvolutionInfo &info = res.value();
    m_peakLabel->setText(QStringLiteral("%1 (%2 dBFS)")
        .arg(QString::number(info.outputPeak, 'f', 4),
             QString::number(info.outputPeakDbfs, 'f', 1)));
    m_gainLabel->setText(QStringLiteral("%1 dB")
        .arg(QString::number(info.suggestedGainDb, 'f', 1)));
    m_clipLabel->setText(info.clipped
        ? I18n::tr("aur_clipped_yes")
              .arg(QString::number(qulonglong(info.clippedSampleCount)))
        : I18n::tr("aur_clipped_no"));

    QStringList warn;
    for (const std::string &w : info.warnings)
        warn << QStringLiteral("• ") + QString::fromStdString(w);
    m_warnings->setText(warn.join(QStringLiteral("\n")));
    m_warnings->setVisible(!warn.isEmpty());

    // ④ A/B 波形
    m_dryPlot->setSeries(waveformSeries(dry, fs, QColor("#0078D4")));
    m_wetPlot->setSeries(waveformSeries(wet, fs, QColor("#2E8B57")));

    m_status->setText(I18n::tr("aur_status_ok").arg(m_outPath->text()));
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
