// KernelResultReader.cpp
#include "KernelResultReader.h"

#include <QFile>
#include <QRegularExpression>
#include <QStringList>

namespace ofd {
namespace KernelResultReader {

namespace {

QString readAll(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return QString();
    return QString::fromUtf8(f.readAll());
}

// 空白区切りの数値行をパース。全トークンが数値なら true
bool numericRow(const QString &line, QVector<double> &vals)
{
    vals.clear();
    const QStringList toks =
        line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (toks.isEmpty()) return false;
    for (const QString &t : toks) {
        bool ok = false;
        vals.push_back(t.toDouble(&ok));
        if (!ok) return false;
    }
    return true;
}

} // namespace

QVector<FeedSweep> parseFeedSweeps(const QString &text)
{
    QVector<FeedSweep> out;
    // 例: "feed #1 (Z0[ohm] = 50.00)"
    static const QRegularExpression head(
        QStringLiteral("^feed\\s+#(\\d+)\\s*\\(Z0\\[ohm\\]\\s*=\\s*"
                       "([-+0-9.eE]+)\\s*\\)"));
    const QStringList lines = text.split(QLatin1Char('\n'));
    int i = 0;
    while (i < lines.size()) {
        const QRegularExpressionMatch m = head.match(lines[i].trimmed());
        if (!m.hasMatch()) { ++i; continue; }
        FeedSweep sweep;
        sweep.feedIndex = m.captured(1).toInt();
        sweep.z0 = m.captured(2).toDouble();
        ++i;
        // ヘッダ行 ("frequency[Hz] Rin[ohm] ...") を読み飛ばし、数値行を収集
        QVector<double> v;
        while (i < lines.size()) {
            const QString line = lines[i].trimmed();
            if (numericRow(line, v)) {
                // freq Rin Xin Gin Bin Ref VSWR (7 列)
                if (v.size() >= 7) {
                    FeedSweepPoint pt;
                    pt.freqHz = v[0];
                    pt.rin = v[1];
                    pt.xin = v[2];
                    pt.refDb = v[5];
                    pt.vswr = v[6];
                    sweep.points.push_back(pt);
                }
                ++i;
            } else if (line.startsWith(QStringLiteral("frequency[Hz]"))) {
                ++i;    // 列ヘッダ
            } else {
                break;  // 表の終わり (空行や次のセクション)
            }
        }
        if (!sweep.points.isEmpty()) out.push_back(sweep);
    }
    return out;
}

QVector<FeedSweep> readFeedSweeps(const QString &logPath)
{
    const QString text = readAll(logPath);
    return text.isEmpty() ? QVector<FeedSweep>() : parseFeedSweeps(text);
}

QVector<FarPattern> parseFar1d(const QString &text)
{
    QVector<FarPattern> out;
    // 例: "#1 : X-plane, frequency[Hz] = 3.00000e+09"
    static const QRegularExpression head(
        QStringLiteral("^#(\\d+)\\s*:\\s*([^,]+),\\s*frequency\\[Hz\\]\\s*=\\s*"
                       "([-+0-9.eE]+)"));
    const QStringList lines = text.split(QLatin1Char('\n'));
    int i = 0;
    while (i < lines.size()) {
        const QRegularExpressionMatch m = head.match(lines[i].trimmed());
        if (!m.hasMatch()) { ++i; continue; }
        FarPattern pat;
        pat.plane = m.captured(2).trimmed();
        pat.freqHz = m.captured(3).toDouble();
        ++i;
        QVector<double> v;
        while (i < lines.size()) {
            const QString line = lines[i].trimmed();
            if (numericRow(line, v)) {
                // No. deg E-abs[dB] ... (3 列以上)
                if (v.size() >= 3) {
                    pat.deg.push_back(v[1]);
                    pat.eAbsDb.push_back(v[2]);
                }
                ++i;
            } else if (line.startsWith(QStringLiteral("No."))) {
                ++i;    // 列ヘッダ
            } else {
                break;
            }
        }
        if (!pat.deg.isEmpty()) out.push_back(pat);
    }
    return out;
}

QVector<FarPattern> readFar1d(const QString &path)
{
    const QString text = readAll(path);
    return text.isEmpty() ? QVector<FarPattern>() : parseFar1d(text);
}

// ── 熱解析レイヤの診断 ──────────────────────────────────────────────────────
// 書式はカーネル (sol/solve.c) の sprintf そのもの:
//   Thermal: dissipated[%d] = %.6e (f=%.6e Hz)
// 読めない行は黙って読み飛ばす (他のログ行に混ざって出るため)。
QVector<ThermalPoint> parseThermal(const QString &text)
{
    QVector<ThermalPoint> out;
    static const QRegularExpression re(
        QStringLiteral("^\\s*Thermal:\\s*dissipated\\[(\\d+)\\]\\s*=\\s*"
                       "([-+0-9.eE]+)\\s*\\(f\\s*=\\s*([-+0-9.eE]+)\\s*Hz\\)"));
    for (const QString &line : text.split(QLatin1Char('\n'))) {
        const QRegularExpressionMatch m = re.match(line);
        if (!m.hasMatch()) continue;
        bool okI = false, okV = false, okF = false;
        ThermalPoint p;
        p.index = m.captured(1).toInt(&okI);
        p.dissipated = m.captured(2).toDouble(&okV);
        p.freqHz = m.captured(3).toDouble(&okF);
        if (okI && okV && okF) out.push_back(p);
    }
    return out;
}

QVector<ThermalPoint> readThermal(const QString &path)
{
    const QString text = readAll(path);
    return text.isEmpty() ? QVector<ThermalPoint>() : parseThermal(text);
}

} // namespace KernelResultReader
} // namespace ofd
