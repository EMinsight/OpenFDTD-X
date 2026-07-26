// RirAnalysisTab.cpp
#include "RirAnalysisTab.h"
#include "../core/Project.h"
#include "../acoustics/qt/QtAcousticAdapter.h"
#include "../acoustics/qt/AcousticResultModel.h"
#include "../widgets/MiniPlot.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTextStream>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

using namespace ofd;
using namespace ofd::acoustics;

namespace {

// 品質トークン → バッジ文字列 + 色
QString qualityBadge(const QString &token)
{
    if (token == QLatin1String("valid"))   return I18n::tr("rir_q_valid");
    if (token == QLatin1String("warning")) return I18n::tr("rir_q_warning");
    return I18n::tr("rir_q_invalid");
}

QColor qualityColor(const QString &token)
{
    if (token == QLatin1String("valid"))   return QColor(0x2E, 0x8B, 0x57);
    if (token == QLatin1String("warning")) return QColor(0xB8, 0x86, 0x0B);
    return QColor(0xC0, 0x39, 0x2B);
}

QTableWidgetItem *roItem(const QString &text)
{
    auto *it = new QTableWidgetItem(text);
    it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    return it;
}

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
        const double tMs = (i0 + (i1 - 1 - i0) * 0.5) / fs * 1000.0;
        top.push_back(QPointF(tMs, hi));
        bottom.push_back(QPointF(tMs, lo));
    }
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

    m_channel = new QComboBox(sIn);
    m_channel->addItems({ I18n::tr("rir_ch_left"), I18n::tr("rir_ch_right"),
                          I18n::tr("rir_ch_mono") });
    sIn->form()->addRow(I18n::tr("rir_channel"), m_channel);

    m_calibration = new QComboBox(sIn);
    m_calibration->addItems({ I18n::tr("rir_calib_absolute"),
                              I18n::tr("rir_calib_relative"),
                              I18n::tr("rir_calib_uncalibrated") });
    sIn->form()->addRow(I18n::tr("rir_calibration"), m_calibration);

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
    connect(m_directMethod, &QComboBox::currentIndexChanged,
            this, &RirAnalysisTab::apply);
    connect(m_bandMode, &QComboBox::currentIndexChanged,
            this, &RirAnalysisTab::apply);
    connect(m_noiseCorr, &QCheckBox::toggled, this, &RirAnalysisTab::apply);
    connect(m_minDr, &QDoubleSpinBox::valueChanged,
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
    m_directMethod->setCurrentIndex(qBound(0, s.directSoundMethod, 2));
    m_bandMode->setCurrentIndex(qBound(0, s.bandMode, 3));
    m_noiseCorr->setChecked(s.noiseCorrection);
    m_minDr->setValue(s.minimumDynamicRangeDb);
    m_updating = false;
}

void RirAnalysisTab::apply()
{
    if (m_updating) return;
    OperaAcousticSettings &s = m_p->operaAcoustic();
    s.rirPath = m_rirPath->text();
    s.channelMode = m_channel->currentIndex();
    s.calibrationState = m_calibration->currentIndex();
    s.directSoundMethod = m_directMethod->currentIndex();
    s.bandMode = m_bandMode->currentIndex();
    s.noiseCorrection = m_noiseCorr->isChecked();
    s.minimumDynamicRangeDb = m_minDr->value();
    m_p->touch();
}

void RirAnalysisTab::browseRir()
{
    const QString path = QFileDialog::getOpenFileName(
        this, I18n::tr("rir_file"), m_rirPath->text(),
        I18n::tr("rir_wav_filter"));
    if (path.isEmpty()) return;
    m_rirPath->setText(path);
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
        envelopeSeries(samples, sampleRateHz, 1200, top, bottom);
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
static void saveTextFile(QWidget *parent, const QString &caption,
                         const QString &suggested, const QString &filter,
                         const QString &content)
{
    const QString path = QFileDialog::getSaveFileName(parent, caption,
                                                      suggested, filter);
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(parent, caption, f.errorString());
        return;
    }
    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    out << content;
}

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
