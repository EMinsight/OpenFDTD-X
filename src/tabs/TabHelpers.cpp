// TabHelpers.cpp
#include "TabHelpers.h"
#include "../I18n.h"

#include <QAbstractButton>
#include <QFile>
#include <QFileDialog>
#include <QLabel>
#include <QMessageBox>
#include <QTableWidgetItem>
#include <QTextStream>
#include <algorithm>

namespace {
const bool s_i18n = [] {
    ofd::I18n::reg("th_notimpl", "未実装", "Not implemented");
    ofd::I18n::reg("th_sample",
        "⚠ サンプル表示 — 実行結果ではありません (機能未実装)",
        "⚠ Sample display — not a computed result (feature not implemented)");
    ofd::I18n::reg("th_unwired",
        "▸ この設定は現在計算へ反映されません (未実装)",
        "▸ These settings are not applied to any computation yet "
        "(not implemented)");
    return true;
}();
} // namespace

namespace ofd {
namespace tabhelp {

// RIR のナイキストがこの値未満なら「高域が無い」と警告する。
// 16 kHz は可聴帯域上端 (20 kHz) より下だが、これを下回ると音色が
// はっきり曇るので実用上の境目として採る。
double rirBandWarnThresholdHz() { return 16000.0; }

QStringList rirSampleRateNotes(double rirFsHz, double outFsHz)
{
    QStringList notes;
    if (!(rirFsHz > 0.0) || !(outFsHz > 0.0)) return notes;
    if (rirFsHz != outFsHz)
        notes << I18n::tr("aur_resampled_note")
                     .arg(QString::number(qRound64(rirFsHz)),
                          QString::number(qRound64(outFsHz)));
    const double band = 0.5 * rirFsHz;
    if (band < rirBandWarnThresholdHz())
        notes << I18n::tr("aur_rir_band_note")
                     .arg(QString::number(qRound64(band)),
                          QString::number(qRound64(rirFsHz)));
    return notes;
}

void markNotImplemented(QAbstractButton *b)
{
    b->setEnabled(false);
    b->setToolTip(I18n::tr("th_notimpl"));
}

QLabel *sampleNote(QWidget *parent)
{
    auto *l = new QLabel(I18n::tr("th_sample"), parent);
    l->setWordWrap(true);
    l->setStyleSheet("font-size:11px; color:#B8860B;");
    return l;
}

QLabel *unwiredNote(QWidget *parent)
{
    auto *l = new QLabel(I18n::tr("th_unwired"), parent);
    l->setWordWrap(true);
    l->setStyleSheet("font-size:11px; color:palette(mid);");
    return l;
}

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

void envelopeSeries(const std::vector<double> &x, double fs, int maxBins,
                    TimeUnit unit, QVector<QPointF> &top,
                    QVector<QPointF> &bottom)
{
    top.clear(); bottom.clear();
    if (x.empty() || fs <= 0 || maxBins <= 0) return;
    const double tScale = (unit == TimeUnit::Milliseconds) ? 1000.0 : 1.0;
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
        const double t = (i0 + (i1 - 1 - i0) * 0.5) / fs * tScale;
        top.push_back(QPointF(t, hi));
        bottom.push_back(QPointF(t, lo));
    }
}

void saveTextFile(QWidget *parent, const QString &caption,
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

} // namespace tabhelp
} // namespace ofd
