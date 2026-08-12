#include "RcwaEfficiency.h"

#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

#include <cmath>

namespace ofd {
namespace rcwa {

double Efficiency::worstEnergyError() const
{
    double worst = 0.0;
    for (const EfficiencyPoint &p : points) {
        worst = std::max(worst, std::fabs(p.rTE + p.tTE - 1.0));
        worst = std::max(worst, std::fabs(p.rTM + p.tTM - 1.0));
    }
    return worst;
}

double Efficiency::worstEnergyFreqHz() const
{
    double worst = -1.0, at = 0.0;
    for (const EfficiencyPoint &p : points) {
        const double e = std::max(std::fabs(p.rTE + p.tTE - 1.0),
                                  std::fabs(p.rTM + p.tTM - 1.0));
        if (e > worst) { worst = e; at = p.freqHz; }
    }
    return (worst >= 0.0) ? at : 0.0;
}

Efficiency parse(const QString &text)
{
    Efficiency out;
    static const QRegularExpression sep(QStringLiteral("[,;\\s]+"));
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString &raw : lines) {
        const QString line = raw.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) continue;
        const QStringList parts = line.split(sep, Qt::SkipEmptyParts);
        if (parts.size() < 6) continue;               // 見出し行もここで落ちる
        double v[6] = { 0, 0, 0, 0, 0, 0 };
        bool all = true;
        for (int k = 0; k < 6 && all; ++k) {
            bool ok = false;
            v[k] = parts[k].toDouble(&ok);
            if (!ok) all = false;
        }
        if (!all) continue;                           // 数字でない行は捨てる
        EfficiencyPoint p;
        p.freqHz = v[0];
        p.lambda_m = v[1];
        p.rTE = v[2];
        p.tTE = v[3];
        p.rTM = v[4];
        p.tTM = v[5];
        out.points.push_back(p);
    }
    return out;
}

Efficiency read(const QString &csvPath)
{
    QFile f(csvPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return Efficiency();
    QTextStream ts(&f);
    return parse(ts.readAll());
}

} // namespace rcwa
} // namespace ofd
