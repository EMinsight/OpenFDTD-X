// ActivationCurve.cpp
#include "ActivationCurve.h"

#include <QFile>
#include <QRegularExpression>
#include <QStringList>
#include <QTextStream>

using namespace ofd;

bool ActivationCurve::parse(const QString &text, QVector<ActivationPoint> &pts,
                            QString *err)
{
    pts.clear();
    const QStringList lines = text.split('\n');
    for (const QString &raw : lines) {
        const QString line = raw.trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;
        const QStringList c = line.split(',');
        if (c.size() < 3) continue;
        bool ok0 = false, ok1 = false, ok2 = false;
        ActivationPoint p;
        p.pin  = c[0].trimmed().toDouble(&ok0);
        p.pout = c[1].trimmed().toDouble(&ok1);
        p.T    = c[2].trimmed().toDouble(&ok2);
        if (!ok0 || !ok1 || !ok2) continue;   // ヘッダ行など
        pts.push_back(p);
    }
    if (pts.isEmpty()) {
        if (err) *err = QStringLiteral("no data rows in activation curve CSV");
        return false;
    }
    return true;
}

bool ActivationCurve::load(const QString &path, QVector<ActivationPoint> &pts,
                           QString *err)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (err) *err = f.errorString();
        return false;
    }
    QTextStream in(&f);
    in.setEncoding(QStringConverter::Utf8);
    return parse(in.readAll(), pts, err);
}

double ActivationCurve::aeffFromLogLine(const QString &line)
{
    // "ONN: A_eff = 1.23e-13 [m^2]"
    static const QRegularExpression re(
        QStringLiteral("ONN:\\s*A_eff\\s*=\\s*([-+0-9.eE]+)"));
    const auto m = re.match(line);
    return m.hasMatch() ? m.captured(1).toDouble() : 0.0;
}
