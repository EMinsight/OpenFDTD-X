// Touchstone.cpp
#include "Touchstone.h"

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>

#include <cmath>

using namespace ofd;

namespace {

constexpr double kPi = 3.14159265358979323846;

void writeHeaderLine(QTextStream &out, double z0)
{
    out << "! OpenFDTD-X export\n";
    out << "# Hz S RI R " << QString::number(z0, 'g', 10) << '\n';
}

// 数値形式 (MA/DB/RI) の 2 数を複素数へ
std::complex<double> pairToComplex(double a, double b, const QString &fmt)
{
    if (fmt == QLatin1String("RI")) return std::complex<double>(a, b);
    const double mag = (fmt == QLatin1String("DB")) ? std::pow(10.0, a / 20.0)
                                                    : a;
    const double th = b * kPi / 180.0;
    return std::complex<double>(mag * std::cos(th), mag * std::sin(th));
}

double freqMultiplier(const QString &unit)
{
    if (unit == QLatin1String("KHZ")) return 1.0e3;
    if (unit == QLatin1String("MHZ")) return 1.0e6;
    if (unit == QLatin1String("GHZ")) return 1.0e9;
    return 1.0;   // HZ
}

// 拡張子 .sNp からポート数を得る (.snp のように N が数字でなければ 0)
int portsFromSuffix(const QString &path)
{
    const QString name = QFileInfo(path).fileName();
    static const QRegularExpression re(QStringLiteral("\\.s(\\d+)p$"),
                                       QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch m = re.match(name);
    if (!m.hasMatch()) return 0;
    const int n = m.captured(1).toInt();
    return (n >= 1 && n <= 99) ? n : 0;
}

// 整数の平方根 (完全平方でなければ 0)
int isqrtExact(int v)
{
    if (v < 1) return 0;
    int r = static_cast<int>(std::lround(std::sqrt(static_cast<double>(v))));
    for (int c = r - 1; c <= r + 1; ++c)
        if (c > 0 && c * c == v) return c;
    return 0;
}

// 「1 行 = 1 周波数」の書き方が続く行数を返す (0 = その書き方ではない)。
// 先頭から数えて個数が同じで、先頭の値 (周波数) が増加している行の連なり。
// 2 ポートの雑音パラメータブロックは個数が変わるのでここで切れる。
int linePerRecordRun(const QVector<QVector<double>> &lines)
{
    if (lines.isEmpty()) return 0;
    const int k = lines[0].size();
    int run = 1;
    while (run < lines.size() && lines[run].size() == k
           && lines[run][0] > lines[run - 1][0])
        ++run;
    // 2 行目で崩れる = N≥3 の準拠ファイル (1 レコードが複数行)
    return (run >= 2 || lines.size() == 1) ? run : 0;
}

} // namespace

std::complex<double> TouchstoneData::at(int freqIndex, int row, int col) const
{
    if (freqIndex < 0 || freqIndex >= s.size()) return {};
    if (row < 1 || col < 1 || row > ports || col > ports) return {};
    const QVector<std::complex<double>> &m = s[freqIndex];
    const int idx = (row - 1) * ports + (col - 1);
    return (idx >= 0 && idx < m.size()) ? m[idx] : std::complex<double>();
}

bool TouchstoneData::isKnown(int row, int col) const
{
    if (row < 1 || col < 1 || row > ports || col > ports) return false;
    return !column1Only || col == 1;
}

QVector<std::complex<double>> TouchstoneData::series(int row, int col) const
{
    QVector<std::complex<double>> v;
    if (!isKnown(row, col)) return v;
    v.reserve(s.size());
    for (int i = 0; i < s.size(); ++i) v.push_back(at(i, row, col));
    return v;
}

bool Touchstone::writeS1p(const QString &path,
                          const QVector<double> &freqHz,
                          const QVector<std::complex<double>> &s11,
                          QString *err)
{
    if (freqHz.size() != s11.size()) {
        if (err) *err = "frequency/data size mismatch";
        return false;
    }
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (err) *err = f.errorString();
        return false;
    }
    QTextStream out(&f);
    writeHeaderLine(out, 50.0);
    for (int i = 0; i < freqHz.size(); ++i)
        out << QString::number(freqHz[i], 'e', 9) << ' '
            << QString::number(s11[i].real(), 'e', 9) << ' '
            << QString::number(s11[i].imag(), 'e', 9) << '\n';
    return true;
}

bool Touchstone::writeS2p(const QString &path,
                          const QVector<double> &freqHz,
                          const QVector<std::complex<double>> &s11,
                          const QVector<std::complex<double>> &s21,
                          const QVector<std::complex<double>> &s12,
                          const QVector<std::complex<double>> &s22,
                          QString *err)
{
    const int n = freqHz.size();
    if (s11.size() != n || s21.size() != n || s12.size() != n || s22.size() != n) {
        if (err) *err = "frequency/data size mismatch";
        return false;
    }
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (err) *err = f.errorString();
        return false;
    }
    QTextStream out(&f);
    writeHeaderLine(out, 50.0);
    for (int i = 0; i < n; ++i) {
        out << QString::number(freqHz[i], 'e', 9);
        for (const auto *s : { &s11, &s21, &s12, &s22 })
            out << ' ' << QString::number((*s)[i].real(), 'e', 9)
                << ' ' << QString::number((*s)[i].imag(), 'e', 9);
        out << '\n';
    }
    return true;
}

bool Touchstone::writeSnp(const QString &path, const TouchstoneData &d,
                          QString *err)
{
    const int n = d.ports;
    if (n < 1) {
        if (err) *err = "no ports";
        return false;
    }
    if (d.s.size() != d.freqHz.size()) {
        if (err) *err = "frequency/data size mismatch";
        return false;
    }
    for (const QVector<std::complex<double>> &m : d.s)
        if (m.size() != n * n) {
            if (err) *err = "matrix size mismatch";
            return false;
        }
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (err) *err = f.errorString();
        return false;
    }
    QTextStream out(&f);
    writeHeaderLine(out, d.z0);
    for (int i = 0; i < d.freqHz.size(); ++i) {
        const QVector<std::complex<double>> &m = d.s[i];
        out << QString::number(d.freqHz[i], 'e', 9);
        if (n == 2) {
            // 2 ポートだけ列順が S11 S21 S12 S22 (転置) — Touchstone の仕様
            for (int k : { 0, 2, 1, 3 })
                out << ' ' << QString::number(m[k].real(), 'e', 9)
                    << ' ' << QString::number(m[k].imag(), 'e', 9);
            out << '\n';
        } else {
            for (int r = 0; r < n; ++r) {
                if (r > 0) out << '\n';           // N≥3 は 1 行 = 1 行分
                for (int c = 0; c < n; ++c) {
                    const std::complex<double> &v = m[r * n + c];
                    out << ' ' << QString::number(v.real(), 'e', 9)
                        << ' ' << QString::number(v.imag(), 'e', 9);
                }
            }
            out << '\n';
        }
    }
    return true;
}

bool Touchstone::read(const QString &path, TouchstoneData *out, QString *err,
                      int portsHint)
{
    if (!out) {
        if (err) *err = "null output";
        return false;
    }
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (err) *err = f.errorString();
        return false;
    }
    QTextStream in(&f);

    // 既定値は Touchstone 1.x の規定 (GHz / S / MA / R 50)
    QString unit = QStringLiteral("GHZ");
    QString type = QStringLiteral("S");
    QString fmt  = QStringLiteral("MA");
    double  z0   = 50.0;
    bool    sawOption = false;

    QVector<QVector<double>> lines;
    static const QRegularExpression ws(QStringLiteral("[\\s,]+"));
    while (!in.atEnd()) {
        QString line = in.readLine();
        const int bang = line.indexOf('!');
        if (bang >= 0) line = line.left(bang);
        line = line.trimmed();
        if (line.isEmpty()) continue;
        if (line.startsWith('[')) {
            if (err) *err = "Touchstone 2.0 ([Version] 記法) は未対応";
            return false;
        }
        if (line.startsWith('#')) {
            if (sawOption) continue;             // 2 個目以降のオプション行は無視
            sawOption = true;
            const QStringList t =
                line.mid(1).split(ws, Qt::SkipEmptyParts);
            for (int i = 0; i < t.size(); ++i) {
                const QString u = t[i].toUpper();
                if (u == "HZ" || u == "KHZ" || u == "MHZ" || u == "GHZ")
                    unit = u;
                else if (u == "S" || u == "Y" || u == "Z" || u == "G" || u == "H")
                    type = u;
                else if (u == "MA" || u == "DB" || u == "RI")
                    fmt = u;
                else if (u == "R" && i + 1 < t.size())
                    z0 = t[++i].toDouble();
            }
            continue;
        }
        const QStringList t = line.split(ws, Qt::SkipEmptyParts);
        QVector<double> v;
        v.reserve(t.size());
        for (const QString &tok : t) {
            bool ok = false;
            const double d = tok.toDouble(&ok);
            if (!ok) {
                if (err) *err = QStringLiteral("数値として読めない行: %1").arg(line);
                return false;
            }
            v.push_back(d);
        }
        if (!v.isEmpty()) lines.push_back(v);
    }
    if (type != QLatin1String("S")) {
        if (err) *err = QStringLiteral("S パラメータ以外 (%1) は未対応").arg(type);
        return false;
    }
    if (lines.isEmpty()) {
        if (err) *err = "データ行がありません";
        return false;
    }

    // ── ポート数とレコード構造の判別 ────────────────────────────────────
    // 準拠ファイル (1 レコード = 1 + 2·N² 個) と、カーネルの test.snp
    // (1 行 = 1 + 2·N 個 = 第 1 列だけ) の両方を受ける。
    const int k = lines[0].size();
    const int hintSuffix = portsFromSuffix(path);
    const int run = linePerRecordRun(lines);
    const bool linePerRecord = (run > 0);
    // 1 行 = 1 周波数の並びが途中で崩れたら、そこから先は別ブロック
    // (2 ポートの雑音パラメータ) — S データとして読まない。
    if (linePerRecord && run < lines.size()) lines.resize(run);

    int  n = 0;
    bool colOnly = false;
    if ((k - 1) % 2 != 0) {
        if (err) *err = "1 行の数値の個数が S パラメータの並びになっていません";
        return false;
    }
    const int pairs = (k - 1) / 2;            // 1 行目の複素数の個数
    const int nFull = isqrtExact(pairs);      // 全行列が 1 行に収まる場合
    // 曖昧さ (pairs が完全平方かつ第 1 列解釈もありうる) は
    // 拡張子 → 呼び出し側のヒント → 準拠解釈 (全行列) の順で解く。
    const int hint = (hintSuffix > 0) ? hintSuffix : portsHint;
    if (linePerRecord) {
        if (hint > 0 && nFull == hint)              { n = hint;  colOnly = false; }
        else if (hint > 0 && pairs == hint)         { n = hint;  colOnly = true;  }
        else if (nFull > 0)                         { n = nFull; colOnly = false; }
        else if (pairs >= 2)                        { n = pairs; colOnly = true;  }
    } else {
        // 複数行で 1 レコード = N≥3 の準拠ファイル (1 行目 = freq + N 複素数)
        if (pairs >= 2) n = pairs;
    }
    if (n <= 0) {
        if (err) *err = "ポート数を判別できません";
        return false;
    }

    QVector<double> flat;
    for (const QVector<double> &v : lines) flat += v;

    const int rec = colOnly ? (1 + 2 * n) : (1 + 2 * n * n);
    if (flat.size() < rec) {
        if (err) *err = QStringLiteral("%1 ポートのデータが 1 点分に足りません")
                            .arg(n);
        return false;
    }

    out->ports = n;
    out->z0 = z0;
    out->column1Only = colOnly;
    out->freqHz.clear();
    out->s.clear();
    const double mult = freqMultiplier(unit);
    for (int i = 0; i + rec <= flat.size(); i += rec) {
        const double freq = flat[i] * mult;
        // 周波数が増加しなくなったらそこで打ち切る。2 ポートの雑音パラメータ
        // ブロックは S データの後ろに低い周波数から続くので、ここで切れる。
        if (!out->freqHz.isEmpty() && freq <= out->freqHz.back()) break;
        QVector<std::complex<double>> m(n * n, std::complex<double>(0.0, 0.0));
        const int count = colOnly ? n : (n * n);
        for (int c = 0; c < count; ++c) {
            const double a = flat[i + 1 + 2 * c];
            const double b = flat[i + 2 + 2 * c];
            const std::complex<double> v = pairToComplex(a, b, fmt);
            if (colOnly) {
                m[c * n] = v;                 // S(c+1, 1) — 第 1 列
            } else if (n == 2) {
                // 読み側も転置を戻す (S11 S21 S12 S22 → 行優先)
                static const int map[4] = { 0, 2, 1, 3 };
                m[map[c]] = v;
            } else {
                m[c] = v;
            }
        }
        out->freqHz.push_back(freq);
        out->s.push_back(m);
    }
    if (out->freqHz.isEmpty()) {
        if (err) *err = "有効な周波数点がありません";
        return false;
    }
    return true;
}

QVector<double> Touchstone::unwrapPhaseRad(
    const QVector<std::complex<double>> &s)
{
    QVector<double> phi;
    phi.reserve(s.size());
    double offset = 0.0;
    double prev = 0.0;
    for (int i = 0; i < s.size(); ++i) {
        const double raw = std::atan2(s[i].imag(), s[i].real());
        if (i > 0) {
            double d = raw - prev;
            while (d >  kPi) { offset -= 2.0 * kPi; d -= 2.0 * kPi; }
            while (d < -kPi) { offset += 2.0 * kPi; d += 2.0 * kPi; }
        }
        phi.push_back(raw + offset);
        prev = raw;
    }
    return phi;
}

QVector<double> Touchstone::groupDelaySec(
    const QVector<double> &freqHz, const QVector<std::complex<double>> &s)
{
    const int n = freqHz.size();
    if (s.size() != n || n < 2) return QVector<double>(n, 0.0);
    const QVector<double> phi = unwrapPhaseRad(s);
    QVector<double> tau(n, 0.0);
    for (int i = 0; i < n; ++i) {
        const int a = (i == 0) ? 0 : i - 1;
        const int b = (i == n - 1) ? n - 1 : i + 1;
        const double df = freqHz[b] - freqHz[a];
        if (df == 0.0) continue;
        tau[i] = -(phi[b] - phi[a]) / df / (2.0 * kPi);
    }
    return tau;
}

TouchstoneData Touchstone::subset(const TouchstoneData &d,
                                  const QVector<int> &ports1based)
{
    TouchstoneData r;
    const int m = ports1based.size();
    if (m < 1 || d.ports < 1) return r;
    for (int p : ports1based)
        if (p < 1 || p > d.ports) return r;
    for (int rr = 0; rr < m; ++rr)
        for (int cc = 0; cc < m; ++cc)
            if (!d.isKnown(ports1based[rr], ports1based[cc])) return r;
    r.ports = m;
    r.z0 = d.z0;
    r.freqHz = d.freqHz;
    r.s.reserve(d.s.size());
    for (int i = 0; i < d.s.size(); ++i) {
        QVector<std::complex<double>> mat(m * m);
        for (int rr = 0; rr < m; ++rr)
            for (int cc = 0; cc < m; ++cc)
                mat[rr * m + cc] = d.at(i, ports1based[rr], ports1based[cc]);
        r.s.push_back(mat);
    }
    return r;
}

std::complex<double> Touchstone::zToS(std::complex<double> z, double z0)
{
    return (z - z0) / (z + z0);
}

QString Touchstone::toCsv(const TouchstoneData &d, const QString &sourceName,
                          QString *err)
{
    if (err) err->clear();
    if (d.isEmpty() || d.s.size() != d.freqHz.size()) {
        if (err) *err = QStringLiteral("S パラメータが空です");
        return QString();
    }

    // 列にする要素 (1 始まりの行・列)。**既知のものだけ**
    QVector<QPair<int, int>> comps;
    for (int r = 1; r <= d.ports; ++r)
        for (int c = 1; c <= d.ports; ++c)
            if (d.isKnown(r, c)) comps.push_back(qMakePair(r, c));
    if (comps.isEmpty()) {
        if (err) *err = QStringLiteral("計算された要素がありません");
        return QString();
    }

    // 厳密に 0 の要素があると dB に落とせない。床値も -inf も値を変えるので
    // 変換しない (.h 参照)
    for (int i = 0; i < d.freqHz.size(); ++i)
        for (const QPair<int, int> &p : comps)
            if (std::abs(d.at(i, p.first, p.second)) == 0.0) {
                if (err)
                    *err = QStringLiteral(
                        "S%1%2 が %3 Hz で厳密に 0 です "
                        "(dB に落とせないため変換しません)")
                        .arg(p.first).arg(p.second)
                        .arg(QString::number(d.freqHz[i], 'g', 10));
                return QString();
            }

    const double kRad2Deg = 180.0 / 3.14159265358979323846;
    QString out;
    out += QStringLiteral("# OpenFDTD-X Touchstone import\n");
    if (!sourceName.isEmpty()) {
        QString src = sourceName;
        src.replace('\r', ' ');
        src.replace('\n', ' ');
        out += QStringLiteral("# source: %1\n").arg(src.trimmed());
    }
    out += QStringLiteral("# ports: %1, reference: %2 ohm, points: %3\n")
               .arg(d.ports).arg(QString::number(d.z0, 'g', 10))
               .arg(d.freqHz.size());
    if (d.column1Only)
        out += QStringLiteral("# only the first column (S_n1) was computed; "
                              "the other elements are not in this file\n");

    QString head = QStringLiteral("freq_Hz");
    for (const QPair<int, int> &p : comps)
        head += QStringLiteral(",S%1%2_dB,S%1%2_deg").arg(p.first).arg(p.second);
    out += head + QLatin1Char('\n');

    for (int i = 0; i < d.freqHz.size(); ++i) {
        QString line = QString::number(d.freqHz[i], 'g', 10);
        for (const QPair<int, int> &p : comps) {
            const std::complex<double> v = d.at(i, p.first, p.second);
            line += QStringLiteral(",%1,%2")
                        .arg(QString::number(20.0 * std::log10(std::abs(v)),
                                             'g', 10),
                             QString::number(std::arg(v) * kRad2Deg, 'g', 10));
        }
        out += line + QLatin1Char('\n');
    }
    return out;
}
