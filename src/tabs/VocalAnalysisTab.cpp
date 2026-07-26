// VocalAnalysisTab.cpp
#include "VocalAnalysisTab.h"
#include "../core/Project.h"
#include "../acoustics/qt/QtAcousticAdapter.h"
#include "../acoustics/qt/AcousticResultModel.h"
#include "../widgets/MiniPlot.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"

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

using namespace ofd;
using namespace ofd::acoustics;

namespace {

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

} // namespace

// ── construction ────────────────────────────────────────────────────────────
VocalAnalysisTab::VocalAnalysisTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // 対象範囲の注記: 単一・無伴奏・モノフォニック歌唱のみ。
    // 声種から診断的結論 (声区判定・巧拙など) は導かない (ADR-0006)。
    auto *hint = new QLabel(I18n::tr("vocal_scope_note"), body);
    hint->setWordWrap(true);
    v->addWidget(hint);

    // ① 入力
    auto *sIn = new SectionBox(I18n::tr("vocal_input_section"), body);
    auto *fileRow = new QHBoxLayout();
    m_voicePath = new QLineEdit(sIn);
    m_voicePath->setReadOnly(true);
    m_voicePath->setPlaceholderText(I18n::tr("rir_file_placeholder"));
    auto *browse = new QPushButton(I18n::tr("rir_browse"), sIn);
    fileRow->addWidget(m_voicePath, 1);
    fileRow->addWidget(browse);
    sIn->form()->addRow(I18n::tr("vocal_file"), fileRow);

    m_voiceType = new QComboBox(sIn);
    m_voiceType->addItems({ I18n::tr("vocal_vt_soprano"),
                            I18n::tr("vocal_vt_mezzo"),
                            I18n::tr("vocal_vt_contralto"),
                            I18n::tr("vocal_vt_tenor"),
                            I18n::tr("vocal_vt_baritone"),
                            I18n::tr("vocal_vt_bass"),
                            I18n::tr("vocal_vt_unknown") });
    sIn->form()->addRow(I18n::tr("vocal_voice_type"), m_voiceType);

    m_f0Min = new QDoubleSpinBox(sIn);
    m_f0Min->setRange(0.0, 4000.0);
    m_f0Min->setDecimals(0);
    m_f0Min->setSuffix(QStringLiteral(" Hz"));
    m_f0Min->setSpecialValueText(I18n::tr("vocal_f0_auto"));
    sIn->form()->addRow(I18n::tr("vocal_f0_min"), m_f0Min);

    m_f0Max = new QDoubleSpinBox(sIn);
    m_f0Max->setRange(0.0, 4000.0);
    m_f0Max->setDecimals(0);
    m_f0Max->setSuffix(QStringLiteral(" Hz"));
    m_f0Max->setSpecialValueText(I18n::tr("vocal_f0_auto"));
    sIn->form()->addRow(I18n::tr("vocal_f0_max"), m_f0Max);

    // 校正状態は表示のみ (編集は実測RIR分析タブと共有の calibrationState)
    m_calibInfo = new QLabel(sIn);
    m_calibInfo->setWordWrap(true);
    sIn->form()->addRow(I18n::tr("vocal_calibration"), m_calibInfo);
    v->addWidget(sIn);

    // ② 実行
    auto *sRun = new SectionBox(I18n::tr("vocal_run_section"), body);
    auto *runRow = new QHBoxLayout();
    m_runBtn = new QPushButton(I18n::tr("vocal_run"), sRun);
    m_status = new QLabel(I18n::tr("vocal_status_idle"), sRun);
    m_status->setWordWrap(true);
    runRow->addWidget(m_runBtn);
    runRow->addWidget(m_status, 1);
    sRun->vbox()->addLayout(runRow);
    v->addWidget(sRun);

    // ③ 結果
    auto *sRes = new SectionBox(I18n::tr("vocal_result_section"), body);
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

    // ④ プロット (F0 軌跡 + LTAS)
    auto *sPlot = new SectionBox(I18n::tr("vocal_plot_section"), body);
    auto *f0Label = new QLabel(I18n::tr("vocal_f0_plot"), sPlot);
    sPlot->vbox()->addWidget(f0Label);
    m_f0Plot = new MiniPlot(sPlot);
    m_f0Plot->setLabels(I18n::tr("vocal_time_s"), I18n::tr("vocal_f0_hz"));
    m_f0Plot->setMinimumHeight(150);
    sPlot->vbox()->addWidget(m_f0Plot);
    auto *ltasLabel = new QLabel(I18n::tr("vocal_ltas_plot"), sPlot);
    sPlot->vbox()->addWidget(ltasLabel);
    m_ltasPlot = new MiniPlot(sPlot);
    m_ltasPlot->setLabels(I18n::tr("vocal_freq_hz"), I18n::tr("rir_level_db"));
    m_ltasPlot->setMinimumHeight(150);
    sPlot->vbox()->addWidget(m_ltasPlot);
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

    connect(browse, &QPushButton::clicked,
            this, &VocalAnalysisTab::browseVoice);
    connect(m_voiceType, &QComboBox::currentIndexChanged,
            this, &VocalAnalysisTab::apply);
    connect(m_f0Min, &QDoubleSpinBox::valueChanged,
            this, &VocalAnalysisTab::apply);
    connect(m_f0Max, &QDoubleSpinBox::valueChanged,
            this, &VocalAnalysisTab::apply);
    connect(m_runBtn, &QPushButton::clicked,
            this, &VocalAnalysisTab::runAnalysis);
    connect(m_csvBtn, &QPushButton::clicked,
            this, &VocalAnalysisTab::exportCsv);
    connect(m_jsonBtn, &QPushButton::clicked,
            this, &VocalAnalysisTab::exportJson);

    connect(project, &Project::loaded, this, &VocalAnalysisTab::refresh);
    // 校正状態は実測RIR分析タブで編集されるため、変更にも追従して表示する
    connect(project, &Project::changed, this, &VocalAnalysisTab::refresh);
    refresh();
}

// ── model ⇄ widgets ─────────────────────────────────────────────────────────
void VocalAnalysisTab::refresh()
{
    m_updating = true;
    const OperaAcousticSettings &s = m_p->operaAcoustic();
    m_voicePath->setText(s.voicePath);
    m_voiceType->setCurrentIndex(qBound(0, s.voiceType, 6));
    m_f0Min->setValue(std::max(0.0, s.vocalF0MinHz));
    m_f0Max->setValue(std::max(0.0, s.vocalF0MaxHz));

    // 校正状態の表示 (SPL 系は Absolute 校正時のみ算出可能)
    const QString state =
        s.calibrationState == 0 ? I18n::tr("rir_calib_absolute")
        : s.calibrationState == 1 ? I18n::tr("rir_calib_relative")
                                  : I18n::tr("rir_calib_uncalibrated");
    m_calibInfo->setText(QStringLiteral("%1 — %2").arg(
        state, s.calibrationState == 0 ? I18n::tr("vocal_calib_spl_ok")
                                       : I18n::tr("vocal_calib_spl_na")));
    m_updating = false;
}

void VocalAnalysisTab::apply()
{
    if (m_updating) return;
    OperaAcousticSettings &s = m_p->operaAcoustic();
    s.voicePath = m_voicePath->text();
    s.voiceType = m_voiceType->currentIndex();
    s.vocalF0MinHz = m_f0Min->value();
    s.vocalF0MaxHz = m_f0Max->value();
    m_p->touch();
}

void VocalAnalysisTab::browseVoice()
{
    const QString path = QFileDialog::getOpenFileName(
        this, I18n::tr("vocal_file"), m_voicePath->text(),
        I18n::tr("rir_wav_filter"));
    if (path.isEmpty()) return;
    m_voicePath->setText(path);
    m_p->operaAcoustic().enabled = true;
    apply();
}

// ── analysis ────────────────────────────────────────────────────────────────
void VocalAnalysisTab::runAnalysis()
{
    apply();
    const OperaAcousticSettings &s = m_p->operaAcoustic();
    if (s.voicePath.trimmed().isEmpty()) {
        clearResult(I18n::tr("vocal_status_nofile"));
        return;
    }

    const AcousticResult<VocalAnalysisResult> res =
        QtAcousticAdapter::analyzeVocalFile(s.voicePath, s);
    if (!res.success()) {
        clearResult(I18n::tr("vocal_status_error")
                        .arg(QString::fromUtf8(
                                 acousticErrorCodeName(res.errorCode())),
                             QString::fromStdString(res.message())));
        return;
    }
    showResult(res.value());
}

void VocalAnalysisTab::clearResult(const QString &statusText)
{
    m_hasResult = false;
    m_result = VocalAnalysisResult();
    m_metricTable->setRowCount(0);
    m_warnings->clear();
    m_warnings->setVisible(false);
    m_f0Plot->setSeries({});
    m_ltasPlot->setSeries({});
    m_csvBtn->setEnabled(false);
    m_jsonBtn->setEnabled(false);
    m_status->setText(statusText);
}

void VocalAnalysisTab::showResult(const VocalAnalysisResult &result)
{
    m_result = result;
    m_hasResult = true;

    // ③ 指標表 (無効値は「算出不可 (理由)」。SPL は未校正時必ず算出不可)
    const QVector<AcousticResultRow> rows =
        AcousticResultModel::vocalRows(result);
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

    // ④ F0 軌跡 — 無声区間は線を切る (有声の連続区間ごとに系列を分ける)
    {
        QVector<MiniSeries> series;
        MiniSeries seg;
        seg.color = QColor("#0078D4");
        for (const F0Frame &f : result.f0Track) {
            if (f.voiced) {
                seg.pts.push_back(QPointF(f.timeSeconds, f.f0Hz));
            } else if (!seg.pts.isEmpty()) {
                series.push_back(seg);
                seg.pts.clear();
            }
        }
        if (!seg.pts.isEmpty()) series.push_back(seg);
        m_f0Plot->setSeries(series);
    }

    // ④ LTAS (dB vs Hz)
    {
        QVector<MiniSeries> series;
        const LtasResult &ltas = result.ltas;
        if (ltas.valid && !ltas.frequenciesHz.empty() &&
            ltas.frequenciesHz.size() == ltas.levelsDb.size()) {
            const int n = int(ltas.frequenciesHz.size());
            const int stride = std::max(1, n / 1500);
            MiniSeries l;
            l.color = QColor("#2E8B57");
            for (int i = 0; i < n; i += stride)
                l.pts.push_back(QPointF(ltas.frequenciesHz[std::size_t(i)],
                                        ltas.levelsDb[std::size_t(i)]));
            series.push_back(l);
        }
        m_ltasPlot->setSeries(series);
    }

    // ⑤ 出力ボタン有効化 + ステータス
    m_csvBtn->setEnabled(true);
    m_jsonBtn->setEnabled(true);
    m_status->setText(I18n::tr("vocal_status_ok")
        .arg(qualityBadge(AcousticResultModel::qualityToken(
                 result.overallQuality)),
             QString::number(result.voicedRatio * 100.0, 'f', 0),
             result.f0MedianHz.valid
                 ? QString::number(result.f0MedianHz.value, 'f', 1) +
                       QStringLiteral(" Hz")
                 : I18n::tr("rir_not_computable")));
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

void VocalAnalysisTab::exportCsv()
{
    if (!m_hasResult) return;
    saveTextFile(this, I18n::tr("rir_export_csv"),
                 QStringLiteral("vocal_analysis.csv"), "CSV (*.csv)",
                 AcousticResultModel::toCsv(m_result));
}

void VocalAnalysisTab::exportJson()
{
    if (!m_hasResult) return;
    saveTextFile(this, I18n::tr("rir_export_json"),
                 QStringLiteral("vocal_analysis.json"), "JSON (*.json)",
                 AcousticResultModel::toJson(m_result));
}
