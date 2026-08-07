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

// ── 2 次元の場マップ ────────────────────────────────────────────────────────
namespace {

// 行頭が 2 つの整数で始まり、その後に数が並ぶデータ行か
bool dataRow(const QStringList &t, int minCols)
{
    if (t.size() < minCols) return false;
    bool a = false, b = false;
    t[0].toInt(&a);
    t[1].toInt(&b);
    return a && b;
}

// 収集した (i, j, 面内座標 2 つ, 値) から FieldMap を組む。
// 番号は 0 始まりで詰まっている前提 (カーネルの出力がそうなっている)。
FieldMap buildMap(const QVector<int> &ii, const QVector<int> &jj,
                  const QVector<double> &aa, const QVector<double> &bb,
                  const QVector<double> &vv)
{
    FieldMap m;
    if (ii.isEmpty()) return m;
    int imax = 0, jmax = 0;
    for (int k = 0; k < ii.size(); ++k) {
        imax = qMax(imax, ii[k]);
        jmax = qMax(jmax, jj[k]);
    }
    m.rows = imax + 1;
    m.cols = jmax + 1;
    if (qsizetype(m.rows) * m.cols != ii.size()) {   // 欠けがある = 格子でない
        m.rows = m.cols = 0;
        return m;
    }
    m.values.resize(qsizetype(m.rows) * m.cols);
    m.rowMin = m.rowMax = aa.isEmpty() ? 0.0 : aa[0];
    m.colMin = m.colMax = bb.isEmpty() ? 0.0 : bb[0];
    for (int k = 0; k < ii.size(); ++k) {
        m.values[qsizetype(ii[k]) * m.cols + jj[k]] = vv[k];
        m.rowMin = qMin(m.rowMin, aa[k]); m.rowMax = qMax(m.rowMax, aa[k]);
        m.colMin = qMin(m.colMin, bb[k]); m.colMax = qMax(m.colMax, bb[k]);
    }
    return m;
}

} // namespace

QVector<FieldMap> parseFar2d(const QString &text)
{
    QVector<FieldMap> out;
    QVector<int> ii, jj;
    QVector<double> th, ph, val;
    double freq = 0.0;
    QString label;

    const auto flush = [&] {
        if (ii.isEmpty()) return;
        FieldMap m = buildMap(ii, jj, th, ph, val);
        if (m.rows > 0) {
            m.freqHz = freq;
            m.label = label;
            m.valueName = QStringLiteral("E-abs[dB]");
            m.rowAxis = QStringLiteral("theta[deg]");
            m.colAxis = QStringLiteral("phi[deg]");
            out.push_back(m);
        }
        ii.clear(); jj.clear(); th.clear(); ph.clear(); val.clear();
    };

    for (const QString &raw : text.split(QLatin1Char('\n'))) {
        const QString line = raw.trimmed();
        if (line.isEmpty()) continue;
        if (line.contains(QLatin1String("frequency"))) {
            flush();
            label = line;
            const int eq = line.lastIndexOf(QLatin1Char('='));
            if (eq >= 0) freq = line.mid(eq + 1).trimmed().toDouble();
            continue;
        }
        const QStringList t =
            line.split(QRegularExpression(QStringLiteral("\\s+")),
                       Qt::SkipEmptyParts);
        if (!dataRow(t, 5)) continue;    // 列見出しなどは飛ばす
        ii.push_back(t[0].toInt());
        jj.push_back(t[1].toInt());
        th.push_back(t[2].toDouble());
        ph.push_back(t[3].toDouble());
        val.push_back(t[4].toDouble());   // E-abs[dB]
    }
    flush();
    return out;
}

QVector<FieldMap> parseNear2d(const QString &text)
{
    QVector<FieldMap> out;
    QVector<int> ii, jj;
    QVector<double> xs, ys, zs, val;
    double freq = 0.0;
    QString label;

    const auto flush = [&] {
        if (ii.isEmpty()) return;
        // 3 座標のうち変化しない軸が断面の法線。残り 2 軸を面内座標にする。
        const auto spread = [](const QVector<double> &v) {
            if (v.isEmpty()) return 0.0;
            double lo = v[0], hi = v[0];
            for (const double x : v) { lo = qMin(lo, x); hi = qMax(hi, x); }
            return hi - lo;
        };
        const double sx = spread(xs), sy = spread(ys), sz = spread(zs);
        const QVector<double> *a = &ys, *b = &zs;
        QString an = QStringLiteral("Y[m]"), bn = QStringLiteral("Z[m]");
        if (sx <= sy && sx <= sz)      { a = &ys; b = &zs; an = "Y[m]"; bn = "Z[m]"; }
        else if (sy <= sx && sy <= sz) { a = &xs; b = &zs; an = "X[m]"; bn = "Z[m]"; }
        else                           { a = &xs; b = &ys; an = "X[m]"; bn = "Y[m]"; }

        FieldMap m = buildMap(ii, jj, *a, *b, val);
        if (m.rows > 0) {
            m.freqHz = freq;
            m.label = label;
            m.valueName = QStringLiteral("E[V/m]");
            m.rowAxis = an;
            m.colAxis = bn;
            out.push_back(m);
        }
        ii.clear(); jj.clear(); xs.clear(); ys.clear(); zs.clear(); val.clear();
    };

    for (const QString &raw : text.split(QLatin1Char('\n'))) {
        const QString line = raw.trimmed();
        if (line.isEmpty()) continue;
        if (line.contains(QLatin1String("frequency"))) {
            flush();
            label = line;
            const int eq = line.lastIndexOf(QLatin1Char('='));
            if (eq >= 0) freq = line.mid(eq + 1).trimmed().toDouble();
            continue;
        }
        const QStringList t =
            line.split(QRegularExpression(QStringLiteral("\\s+")),
                       Qt::SkipEmptyParts);
        if (!dataRow(t, 6)) continue;
        ii.push_back(t[0].toInt());
        jj.push_back(t[1].toInt());
        xs.push_back(t[2].toDouble());
        ys.push_back(t[3].toDouble());
        zs.push_back(t[4].toDouble());
        val.push_back(t[5].toDouble());   // E[V/m]
    }
    flush();
    return out;
}

QVector<FieldMap> readFar2d(const QString &path)
{
    const QString t = readAll(path);
    return t.isEmpty() ? QVector<FieldMap>() : parseFar2d(t);
}
QVector<FieldMap> readNear2d(const QString &path)
{
    const QString t = readAll(path);
    return t.isEmpty() ? QVector<FieldMap>() : parseNear2d(t);
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
