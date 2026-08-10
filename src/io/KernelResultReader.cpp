// KernelResultReader.cpp
#include "KernelResultReader.h"

#include <QFile>
#include <QRegularExpression>
#include <QTextStream>
#include <QStringList>

#include <utility>

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

// ── ofd_post の番号付き表 (ev2d を介さないポスト表示の素データ) ─────────────
//
// 形は 4 ファイル共通:
//
//   feed #1 (waveform)                        ← 見出し
//       No.    time[sec]      V[V]          I[A]   ← 列見出し
//         0   0.00000e+00   1.23456e-03   4.56789e-05
//
// 判定は「先頭トークンが非負整数の数値行かどうか」だけで行う。数値行が
// 始まった時点で、直前の非数値行を列見出し、その 1 つ前を見出しとみなす。
// 見出しが 1 行しか無いファイル (先頭ブロック) でも列見出しは必ずあるので、
// title が空になるだけで表としては成立する。
//
// 先頭の No. 列は捨て、次の数値列を x、残りを y にする。列名の数が
// データ列と合わないときは "col N" で埋める (カーネルが列を増やしても
// 落ちないようにする — 名前を勝手に解釈しないのが前提)。
namespace {

// 行を 1 本ずつ食わせる状態機械。**ファイル全体を QString に載せない**ため
// (大規模データ対策) パースとテキスト分割を分けてある。テキストから読む
// parsePostTables() とファイルから読む readPostTables() の両方がこれを使う。
class PostTableParser {
public:
    explicit PostTableParser(QString sourceFile) : m_src(std::move(sourceFile)) {}

    void feed(const QString &raw)
    {
        const QString line = raw.trimmed();
        if (line.isEmpty()) { flush(); return; }
        // 先頭が通し番号の数値行か (2 列以上ないと x/y に分けられない)
        if (numericRow(line, m_vals) && m_vals.size() >= 2) {
            if (!m_inBlock) startBlock();
            ++m_seen;
            // 上限を超えたら「半分に間引いてストライドを倍にする」を繰り返す。
            // 残る行は必ず 0, stride, 2*stride, … の等間隔なので、間引いても
            // 横軸の目盛が歪まない (先頭だけ密になる、といったことがない)。
            // 何行あったかは m_seen に残り、画面に「N 行中 M 行」と出す
            if (((m_seen - 1) % m_stride) != 0) return;
            m_rows.push_back(m_vals);
            if (m_rows.size() >= kMaxTableRows) {
                QVector<QVector<double>> half;
                half.reserve(m_rows.size() / 2 + 1);
                for (int i = 0; i < m_rows.size(); i += 2)
                    half.push_back(m_rows[i]);
                m_rows = half;
                m_stride *= 2;
            }
        } else {
            flush();
            m_prev2 = m_prev1;
            m_prev1 = line;
        }
    }

    QVector<PostTable> take() { flush(); return std::move(m_out); }

private:
    void startBlock()
    {
        m_cur = PostTable();
        m_cur.sourceFile = m_src;
        QStringList cols = m_prev1.split(QRegularExpression("\\s+"),
                                         Qt::SkipEmptyParts);
        if (!cols.isEmpty()) cols.removeFirst();   // 先頭は必ず "No."
        m_cur.xName  = cols.isEmpty() ? QString() : cols.first();
        m_cur.yNames = cols.mid(1);                // flush() で横軸を選び直す
        m_cur.title  = m_prev2;
        m_inBlock = true;
        m_rows.clear();
        m_seen = 0;
        m_stride = 1;
    }

    void flush()
    {
        if (!m_inBlock) { m_rows.clear(); return; }
        m_inBlock = false;
        if (m_rows.isEmpty()) return;

        // 行ごとに列数が違う行は捨てる (先頭行の列数を基準にする)
        const int ncol = m_rows.first().size();
        QVector<QVector<double>> col(ncol - 1);   // 列 1..ncol-1 (0 は通し番号)
        for (const QVector<double> &r : m_rows) {
            if (r.size() != ncol) continue;
            for (int c = 1; c < ncol; ++c) col[c - 1].push_back(r[c]);
        }
        m_rows.clear();
        if (col.isEmpty() || col.first().isEmpty()) return;

        // 列名を実データの本数に合わせてから、値が変化する列を横軸に採る
        QStringList names = m_cur.yNames;
        names.prepend(m_cur.xName);               // 一旦 1 本の列名列にする
        while (names.size() > col.size()) names.removeLast();
        for (int i = names.size(); i < col.size(); ++i)
            names << QStringLiteral("col %1").arg(i + 2);

        auto varies = [](const QVector<double> &v) {
            for (int i = 1; i < v.size(); ++i)
                if (v[i] != v[0]) return true;
            return false;
        };
        int xi = -1;
        for (int i = 0; i < col.size(); ++i)
            if (varies(col[i])) { xi = i; break; }
        if (xi < 0) xi = 0;                       // 全部一定なら先頭を横軸に

        // 一定列を注記へ回すのは 2 行以上あるときだけ。1 行しかない表
        // (周波数 1 点の far0d.log など) では全列が「一定」に見えるので、
        // 退避すると系列が 1 本も残らず表ごと消える
        const bool manyRows = (col.first().size() >= 2);

        m_cur.xName = names[xi];
        m_cur.x     = col[xi];
        m_cur.yNames.clear();
        m_cur.y.clear();
        QStringList fixedParts;
        for (int i = 0; i < col.size(); ++i) {
            if (i == xi) continue;
            if (manyRows && !varies(col[i])) {   // 一定列は系列にせず注記へ回す
                fixedParts << QStringLiteral("%1=%2")
                                  .arg(names[i])
                                  .arg(col[i].first(), 0, 'g', 4);
                continue;
            }
            m_cur.yNames << names[i];
            m_cur.y << col[i];
        }
        m_cur.fixed = fixedParts.join(QStringLiteral(", "));
        m_cur.totalRows = m_seen;
        if (m_cur.isValid()) m_out.push_back(m_cur);
    }

    QString m_src;
    QString m_prev1, m_prev2;      // 直近の非数値行 (m_prev1 が直前)
    PostTable m_cur;
    bool m_inBlock = false;
    QVector<QVector<double>> m_rows;
    QVector<double> m_vals;
    QVector<PostTable> m_out;
    int m_seen = 0;      // このブロックで見た行数 (間引き前)
    int m_stride = 1;    // 現在の間引き間隔
};

} // namespace

QVector<PostTable> parsePostTables(const QString &text,
                                   const QString &sourceFile)
{
    PostTableParser parser(sourceFile);
    for (const QString &line : text.split(QLatin1Char('\n'))) parser.feed(line);
    return parser.take();
}

// ファイルは **1 行ずつ** 読む。ポスト出力は問題規模と反復回数に比例して
// 大きくなる (実測: 30x30x31 セルの dipole でも near2d.log が 117 KB) ので、
// 全体を QString に載せると規模を上げたときに素直に破綻する。
QVector<PostTable> readPostTables(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return QVector<PostTable>();
    const int slash = qMax(path.lastIndexOf(QLatin1Char('/')),
                           path.lastIndexOf(QLatin1Char('\\')));
    PostTableParser parser(path.mid(slash + 1));
    QTextStream in(&f);
    while (!in.atEnd()) parser.feed(in.readLine());
    return parser.take();
}

// ── 散乱断面積 (RCS) ────────────────────────────────────────────────────────
// "=== cross section ===" の見出しの後、見出し行を 1 行挟んで数値行が続く。
// 数値行は 3 列 (周波数 / 後方 / 前方) で、次の "===" 見出しまでを読む。
QVector<CrossSectionPoint> parseCrossSection(const QString &text)
{
    QVector<CrossSectionPoint> out;
    const QStringList lines = text.split(QLatin1Char('\n'));
    int i = 0;
    while (i < lines.size()
           && !lines[i].contains(QLatin1String("=== cross section ===")))
        ++i;
    if (i >= lines.size()) return out;      // 平面波入射の問題ではない
    ++i;
    for (; i < lines.size(); ++i) {
        const QString line = lines[i].trimmed();
        if (line.isEmpty()) continue;
        if (line.startsWith(QLatin1String("==="))) break;   // 次の節
        QVector<double> v;
        if (!numericRow(line, v)) continue;                 // 見出し行を飛ばす
        if (v.size() < 3) continue;
        CrossSectionPoint p;
        p.freqHz      = v[0];
        p.backward_m2 = v[1];
        p.forward_m2  = v[2];
        out.push_back(p);
    }
    return out;
}

QVector<CrossSectionPoint> readCrossSection(const QString &path)
{
    const QString text = readAll(path);
    return text.isEmpty() ? QVector<CrossSectionPoint>()
                          : parseCrossSection(text);
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
