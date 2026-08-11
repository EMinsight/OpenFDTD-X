// SpiceNetlist.cpp — SPICE ネットリストの読み取り (仕様は SpiceNetlist.h)
#include "SpiceNetlist.h"

#include <QFile>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>

using namespace ofd;

int SpiceNetlist::count(QChar type) const
{
    int n = 0;
    for (const SpiceElement &e : elements)
        if (e.type == type) ++n;
    return n;
}

QString SpiceIO::fileDialogFilter()
{
    return QStringLiteral("SPICE netlist (*.cir *.sp *.net *.ckt *.spi);;"
                          "All files (*)");
}

// ── 数値 ────────────────────────────────────────────────────────────────────
// 先頭の数値部分を読み、続く英字を接尾辞として解釈する。
// **MEG / MIL を M より先に判定する** (M 単独はミリ)。
bool SpiceIO::parseValue(const QString &token, double *out)
{
    const QString t = token.trimmed();
    if (t.isEmpty()) return false;

    // 数値部分の終わりを探す (符号・数字・小数点・指数)
    int i = 0;
    if (i < t.size() && (t[i] == '+' || t[i] == '-')) ++i;
    bool digits = false;
    while (i < t.size() && t[i].isDigit()) { ++i; digits = true; }
    if (i < t.size() && t[i] == '.') {
        ++i;
        while (i < t.size() && t[i].isDigit()) { ++i; digits = true; }
    }
    if (!digits) return false;
    // 指数部 (e/E の直後が符号か数字のときだけ — "1e" や "4.7Farad" と区別)
    if (i < t.size() && (t[i] == 'e' || t[i] == 'E')) {
        int j = i + 1;
        if (j < t.size() && (t[j] == '+' || t[j] == '-')) ++j;
        int k = j;
        while (k < t.size() && t[k].isDigit()) ++k;
        if (k > j) i = k;
    }
    bool ok = false;
    double v = t.left(i).toDouble(&ok);
    if (!ok) return false;

    const QString suffix = t.mid(i).toUpper();
    double scale = 1.0;
    if      (suffix.startsWith(QLatin1String("MEG"))) scale = 1e6;
    else if (suffix.startsWith(QLatin1String("MIL"))) scale = 25.4e-6;
    else if (!suffix.isEmpty()) {
        switch (suffix.at(0).unicode()) {
        case 'T': scale = 1e12;  break;
        case 'G': scale = 1e9;   break;
        case 'K': scale = 1e3;   break;
        case 'M': scale = 1e-3;  break;   // MEG/MIL は上で処理済み
        case 'U': scale = 1e-6;  break;
        case 'N': scale = 1e-9;  break;
        case 'P': scale = 1e-12; break;
        case 'F': scale = 1e-15; break;
        default:  scale = 1.0;   break;   // 単位表記 (Ohm, H, V …) は無視
        }
    }
    if (out) *out = v * scale;
    return true;
}

// ── 本体 ────────────────────────────────────────────────────────────────────
SpiceNetlist SpiceIO::parse(const QString &text)
{
    SpiceNetlist out;

    // 1) 行の整形: コメント除去 + 継続行 (`+`) の連結
    QStringList logical;
    const QStringList raw = text.split(QRegularExpression("\r\n|\n|\r"));
    for (int i = 0; i < raw.size(); ++i) {
        QString line = raw[i];
        // 先頭行はタイトル (SPICE の慣習)
        if (i == 0) { out.title = line.trimmed(); continue; }
        if (line.trimmed().startsWith('*')) continue;      // 行コメント
        const int semi = line.indexOf(';');                // 行中コメント
        if (semi >= 0) line = line.left(semi);
        line = line.trimmed();
        if (line.isEmpty()) continue;
        if (line.startsWith('+') && !logical.isEmpty())
            logical.last() += QLatin1Char(' ') + line.mid(1).trimmed();
        else
            logical << line;
    }

    // 2) 解釈
    QSet<QString> seenNodes;
    int otherKinds[26] = {0};
    bool inSubckt = false;
    for (const QString &line : logical) {
        const QStringList tok = line.split(QRegularExpression("[ \t]+"),
                                           Qt::SkipEmptyParts);
        if (tok.isEmpty()) continue;
        const QString head = tok[0];

        if (head.startsWith('.')) {
            const QString d = head.toLower();
            if (d == QLatin1String(".subckt")) {
                inSubckt = true;
                ++out.skippedSubckts;
            } else if (d == QLatin1String(".ends")
                       || d == QLatin1String(".eom")) {
                inSubckt = false;
            }
            continue;                       // ディレクティブは解釈しない
        }
        if (inSubckt) continue;             // サブサーキットの中身は展開しない

        const QChar type = head.at(0).toUpper();
        if (type == 'R' || type == 'L' || type == 'C') {
            if (tok.size() < 4) {
                out.warnings << QStringLiteral("%1: 値が読めません (%2)")
                                    .arg(head, line);
                continue;
            }
            double v = 0;
            if (!parseValue(tok[3], &v)) {
                out.warnings << QStringLiteral("%1: 値 '%2' を解釈できません")
                                    .arg(head, tok[3]);
                continue;
            }
            SpiceElement e;
            e.type = type;
            e.name = head;
            e.node1 = tok[1];
            e.node2 = tok[2];
            e.value = v;
            out.elements << e;
            for (const QString &n : { e.node1, e.node2 })
                if (!seenNodes.contains(n)) { seenNodes.insert(n); out.nodes << n; }
        } else {
            ++out.skippedElements;
            const int k = type.unicode() - 'A';
            if (k >= 0 && k < 26) ++otherKinds[k];
        }
    }

    // 3) 読み飛ばしたものを言う (黙って落とさない)
    if (out.skippedSubckts > 0)
        out.warnings << QStringLiteral(".subckt を %1 個読み飛ばしました "
                                       "(展開は未対応 — 中の素子は取り込まれません)")
                            .arg(out.skippedSubckts);
    if (out.skippedElements > 0) {
        QStringList kinds;
        for (int k = 0; k < 26; ++k)
            if (otherKinds[k] > 0)
                kinds << QStringLiteral("%1×%2")
                             .arg(QChar('A' + k)).arg(otherKinds[k]);
        out.warnings << QStringLiteral("R/L/C 以外の素子を %1 個読み飛ばしました "
                                       "(%2)")
                            .arg(out.skippedElements)
                            .arg(kinds.join(QStringLiteral(", ")));
    }
    return out;
}

bool SpiceIO::read(const QString &path, SpiceNetlist &out, QString *err)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (err) *err = f.errorString();
        return false;
    }
    out = parse(QString::fromUtf8(f.readAll()));
    if (!out.isValid()) {
        if (err) *err = QStringLiteral("R/L/C の素子行が 1 つもありません");
        return false;
    }
    return true;
}

// ── 書き出し ────────────────────────────────────────────────────────────────
QString SpiceIO::buildSubckt(const SpiceSubckt &sub)
{
    if (!sub.hasAny()) return QString();

    // 値は指数表記で書く。SPICE の接尾辞 (M / MEG …) は読み手によって
    // 解釈が割れるので使わない — 取り込み側の落とし穴そのものを避ける。
    auto num = [](double v) { return QString::number(v, 'g', 6); };

    QString out;
    if (!sub.comment.isEmpty())
        out += QStringLiteral("* ") + sub.comment + QLatin1Char('\n');
    out += QStringLiteral("* generated by OpenFDTD-X\n");
    out += QStringLiteral(".subckt %1 1 2\n").arg(sub.name);

    struct Item { QChar type; double value; };
    QVector<Item> items;
    if (sub.r_ohm > 0.0) items.push_back({ QLatin1Char('R'), sub.r_ohm });
    if (sub.l_h   > 0.0) items.push_back({ QLatin1Char('L'), sub.l_h });
    if (sub.c_f   > 0.0) items.push_back({ QLatin1Char('C'), sub.c_f });

    if (sub.series) {
        // 1 → n1 → n2 → … → 2 と数珠つなぎ
        QString from = QStringLiteral("1");
        for (int i = 0; i < items.size(); ++i) {
            const bool last = (i + 1 == items.size());
            const QString to = last ? QStringLiteral("2")
                                    : QStringLiteral("n%1").arg(i + 1);
            out += QStringLiteral("%1%2 %3 %4 %5\n")
                       .arg(items[i].type).arg(i + 1)
                       .arg(from, to, num(items[i].value));
            from = to;
        }
    } else {
        for (int i = 0; i < items.size(); ++i)
            out += QStringLiteral("%1%2 1 2 %3\n")
                       .arg(items[i].type).arg(i + 1)
                       .arg(num(items[i].value));
    }
    out += QStringLiteral(".ends %1\n").arg(sub.name);
    return out;
}

bool SpiceIO::writeSubckt(const QString &path, const SpiceSubckt &sub,
                          QString *err)
{
    const QString text = buildSubckt(sub);
    if (text.isEmpty()) {
        if (err) *err = QStringLiteral("書き出す素子がありません "
                                       "(R / L / C がすべて 0 以下)");
        return false;
    }
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (err) *err = f.errorString();
        return false;
    }
    f.write(text.toUtf8());
    return true;
}
