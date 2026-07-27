// TabHelpers.cpp
#include "TabHelpers.h"
#include "../I18n.h"

#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QTableWidgetItem>
#include <QTextStream>
#include <algorithm>

namespace ofd {
namespace tabhelp {

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
