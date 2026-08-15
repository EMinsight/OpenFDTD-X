// RefractiveIndexDb.cpp — refractiveindex.info の読み手 (RefractiveIndexDb.h 参照)
#include "RefractiveIndexDb.h"

#include <QRegularExpression>
#include <QStringList>

#include <algorithm>
#include <cmath>

namespace ofd {

namespace {

// "abc" / 'abc' の囲みを外す
QString unquote(QString s)
{
    s = s.trimmed();
    if (s.size() >= 2 &&
        ((s.startsWith('"')  && s.endsWith('"')) ||
         (s.startsWith('\'') && s.endsWith('\''))))
        s = s.mid(1, s.size() - 2);
    return s.trimmed();
}

int indentOf(const QString &line)
{
    int i = 0;
    while (i < line.size() && line[i] == ' ') ++i;
    return i;
}

// "  - KEY: value" / "  KEY: value" の KEY と value を取り出す
bool splitKey(const QString &line, QString *key, QString *value)
{
    QString s = line.trimmed();
    if (s.startsWith("- ")) s = s.mid(2).trimmed();
    const int c = s.indexOf(':');
    if (c < 0) return false;
    *key   = s.left(c).trimmed();
    *value = s.mid(c + 1).trimmed();
    return true;
}

QVector<double> numbers(const QString &s)
{
    QVector<double> v;
    const QStringList parts =
        s.split(QRegularExpression("[\\s,]+"), Qt::SkipEmptyParts);
    for (const QString &p : parts) {
        bool ok = false;
        const double d = p.toDouble(&ok);
        if (ok) v.push_back(d);
    }
    return v;
}

} // namespace

QString RiEntry::label() const
{
    const QString b = bookName.isEmpty() ? book : bookName;
    const QString p = pageName.isEmpty() ? page : pageName;
    return b + " / " + p;
}

// ── カタログ ────────────────────────────────────────────────────────────────
// - SHELF: main
//     name: "..."
//     content:
//       - BOOK: Ag
//           name: "Ag (Silver)"
//           content:
//             - PAGE: Johnson
//                 name: "..."
//                 data: main/Ag/nk/Johnson.yml
QVector<RiEntry> parseRiCatalog(const QByteArray &yaml)
{
    QVector<RiEntry> out;
    const QStringList lines = QString::fromUtf8(yaml).split('\n');

    QString shelf, book, bookName;
    RiEntry pending;
    bool inPage = false;

    auto flush = [&] {
        if (inPage && !pending.dataPath.isEmpty()) out.push_back(pending);
        inPage = false;
    };

    for (const QString &raw : lines) {
        const QString line = raw;
        if (line.trimmed().isEmpty() || line.trimmed().startsWith('#')) continue;

        QString key, val;
        if (!splitKey(line, &key, &val)) continue;

        if (key == "SHELF") {
            flush();
            shelf = unquote(val);
            book.clear(); bookName.clear();
        } else if (key == "BOOK") {
            flush();
            book = unquote(val);
            bookName.clear();
        } else if (key == "PAGE") {
            flush();
            pending = RiEntry{};
            pending.shelf = shelf;
            pending.book = book;
            pending.bookName = bookName;
            pending.page = unquote(val);
            inPage = true;
        } else if (key == "name") {
            // 直近に開いた要素の名前 (PAGE 中なら page の名前)
            if (inPage) pending.pageName = unquote(val);
            else if (!book.isEmpty()) bookName = unquote(val);
        } else if (key == "data") {
            if (inPage) pending.dataPath = unquote(val);
        } else if (key == "DIVIDER") {
            // 見出し。木構造には影響しない
        }
    }
    flush();
    return out;
}

// ── データファイル ──────────────────────────────────────────────────────────
RiData parseRiData(const QByteArray &yaml)
{
    RiData d;
    const QStringList lines = QString::fromUtf8(yaml).split('\n');

    bool inData = false;          // DATA: 節の中か
    QString blockKey;             // "REFERENCES" / "COMMENTS" (| ブロック)
    QString blockText;

    QString curType;              // 現在の DATA エントリの type
    bool    collecting = false;   // data: | の行を集めているか
    QString collected;

    auto finishEntry = [&] {
        if (curType.isEmpty()) return;
        if (curType.startsWith("tabulated")) {
            const QStringList rows = collected.split('\n', Qt::SkipEmptyParts);
            for (const QString &r : rows) {
                const QVector<double> v = numbers(r);
                if (curType == "tabulated nk") {
                    if (v.size() >= 3)
                        d.nkTable.push_back({ v[0], v[1], v[2] });
                } else if (curType == "tabulated n") {
                    if (v.size() >= 2)
                        d.nkTable.push_back({ v[0], v[1], -1.0 });
                } else if (curType == "tabulated k") {
                    if (v.size() >= 2)
                        d.kTable.push_back({ v[0], v[1] });
                }
            }
        }
        curType.clear();
        collected.clear();
        collecting = false;
    };

    for (const QString &raw : lines) {
        if (raw.trimmed().startsWith('#')) continue;

        const int ind = indentOf(raw);
        const bool topLevel = (ind == 0 && !raw.trimmed().isEmpty());

        // 列 0 の新しいキーが来たら DATA: 節もブロックも打ち切る
        // (PROPERTIES の thermal_dispersion を DATA と取り違えないため)
        if (topLevel) {
            finishEntry();
            if (!blockKey.isEmpty()) {
                if (blockKey == "REFERENCES") d.reference = blockText.trimmed();
                if (blockKey == "COMMENTS")   d.comments  = blockText.trimmed();
                blockKey.clear();
                blockText.clear();
            }
            inData = false;

            QString key, val;
            if (splitKey(raw, &key, &val)) {
                if (key == "DATA") { inData = true; continue; }
                if ((key == "REFERENCES" || key == "COMMENTS") &&
                    val.startsWith('|')) {
                    blockKey = key;
                    continue;
                }
            }
            continue;
        }

        if (!blockKey.isEmpty()) {          // | ブロックの中身
            blockText += raw.trimmed() + "\n";
            continue;
        }
        if (!inData) continue;

        if (collecting) {
            // data: | の続き。エントリ先頭 ("- type:") が来たら終わり
            if (raw.trimmed().startsWith("- ")) {
                finishEntry();
            } else {
                collected += raw.trimmed() + "\n";
                continue;
            }
        }

        QString key, val;
        if (!splitKey(raw, &key, &val)) continue;

        if (key == "type") {
            finishEntry();
            const QString t = unquote(val);
            if (t.startsWith("formula")) {
                const QVector<double> f = numbers(t);
                d.formula = f.isEmpty() ? -1 : int(f[0]);   // "formula A" は -1
                curType.clear();
            } else {
                curType = t;
            }
        } else if (key == "wavelength_range") {
            const QVector<double> v = numbers(val);
            if (v.size() >= 2) { d.fMin_um = v[0]; d.fMax_um = v[1]; }
        } else if (key == "coefficients") {
            d.coeff = numbers(val);
        } else if (key == "data") {
            collecting = true;
            collected.clear();
            if (!val.startsWith('|')) collected = val + "\n";   // 1 行書きの保険
        }
    }
    finishEntry();
    if (!blockKey.isEmpty()) {
        if (blockKey == "REFERENCES") d.reference = blockText.trimmed();
        if (blockKey == "COMMENTS")   d.comments  = blockText.trimmed();
    }

    std::sort(d.nkTable.begin(), d.nkTable.end(),
              [](const optics::NkSample &a, const optics::NkSample &b) {
                  return a.lambda_um < b.lambda_um;
              });
    std::sort(d.kTable.begin(), d.kTable.end());

    if (d.nkTable.empty() && !d.hasFormula()) {
        d.error = QStringLiteral("no tabulated data and no formula in DATA:");
        return d;
    }
    if (d.nkTable.empty() && !riFormulaSupported(d.formula)) {
        d.error = QStringLiteral("formula %1").arg(d.formula);
        return d;
    }
    d.ok = true;
    return d;
}

bool riFormulaSupported(int formula) { return formula >= 1 && formula <= 9; }

QString riPlainText(const QString &src)
{
    QString t = src;
    // Markdown のリンク [表示](URL) は表示だけ残す
    t.replace(QRegularExpression("\\[([^\\]]*)\\]\\([^)]*\\)"), "\\1");
    // HTML タグを落とす (<a href=...> <i> <b> <br> …)
    t.remove(QRegularExpression("<[^>]*>"));
    // よく出る実体参照だけ戻す
    t.replace("&amp;", "&").replace("&lt;", "<").replace("&gt;", ">")
     .replace("&quot;", "\"").replace("&nbsp;", " ");
    // Markdown の強調記号 (** / __ / * / _) を落とす
    t.replace(QRegularExpression("\\*\\*([^*]*)\\*\\*"), "\\1");
    t.replace(QRegularExpression("__([^_]*)__"), "\\1");
    // 行内の空白を詰める (改行は残す)
    const QStringList rows = t.split('\n');
    QStringList out;
    for (const QString &r : rows) {
        const QString c = r.simplified();
        if (!c.isEmpty()) out << c;
    }
    return out.join('\n');
}

// 各式の評価 (定義はヘッダの転記 = 上流仕様書 "Dispersion formulas.pdf")。
// 分母が 0 に近い・n² が正でない等の破綻は false (黙って NaN を出さない)。
bool riEvalN(const RiData &d, double lambda_um, double *n)
{
    if (!riFormulaSupported(d.formula) || d.coeff.isEmpty()) return false;
    if (d.fMax_um > d.fMin_um &&
        (lambda_um < d.fMin_um || lambda_um > d.fMax_um)) return false;

    const double l  = lambda_um;
    const double l2 = l * l;
    const QVector<double> &c = d.coeff;
    auto at = [&](int i) { return (i < c.size()) ? c[i] : 0.0; };
    const double tiny = 1e-18;
    double n2 = 0.0;

    switch (d.formula) {
    case 1:            // Sellmeier: n²−1 = C1 + Σ C(2i)λ²/(λ²−C(2i+1)²)
    case 2: {          // Sellmeier-2: 分母の共鳴波長が二乗済み
        double s = 1.0 + at(0);
        for (int i = 1; i + 1 < c.size(); i += 2) {
            const double b = c[i + 1];
            const double den = (d.formula == 1) ? (l2 - b * b) : (l2 - b);
            if (std::fabs(den) < tiny) return false;
            s += c[i] * l2 / den;
        }
        n2 = s;
        break;
    }
    case 3: {          // Polynomial: n² = C1 + Σ C(2i)·λ^C(2i+1)
        double s = at(0);
        for (int i = 1; i + 1 < c.size(); i += 2)
            s += c[i] * std::pow(l, c[i + 1]);
        n2 = s;
        break;
    }
    case 4: {          // C1 + 共鳴 2 項 (4 係数) + べき乗項
        double s = at(0);
        for (int base : { 1, 5 }) {          // C2..C5 と C6..C9
            if (base + 1 >= c.size()) break;
            const double den = l2 - std::pow(at(base + 2), at(base + 3));
            if (std::fabs(den) < tiny) return false;
            s += at(base) * std::pow(l, at(base + 1)) / den;
        }
        for (int i = 9; i + 1 < c.size(); i += 2)
            s += c[i] * std::pow(l, c[i + 1]);
        n2 = s;
        break;
    }
    case 5: {          // Cauchy: n = C1 + Σ C(2i)·λ^C(2i+1)
        double s = at(0);
        for (int i = 1; i + 1 < c.size(); i += 2)
            s += c[i] * std::pow(l, c[i + 1]);
        if (s <= 0.0) return false;
        n2 = s * s;
        break;
    }
    case 6: {          // Gases: n−1 = C1 + Σ C(2i)/(C(2i+1) − λ⁻²)
        double s = at(0);
        const double invl2 = 1.0 / l2;
        for (int i = 1; i + 1 < c.size(); i += 2) {
            const double den = c[i + 1] - invl2;
            if (std::fabs(den) < tiny) return false;
            s += c[i] / den;
        }
        const double nn = 1.0 + s;
        if (nn <= 0.0) return false;
        n2 = nn * nn;
        break;
    }
    case 7: {          // Herzberger: 分母の定数 0.028 は仕様書の定義そのもの
        const double den = l2 - 0.028;
        if (std::fabs(den) < tiny) return false;
        const double L = 1.0 / den;
        const double nn = at(0) + at(1) * L + at(2) * L * L
                        + at(3) * l2 + at(4) * l2 * l2 + at(5) * l2 * l2 * l2;
        if (nn <= 0.0) return false;
        n2 = nn * nn;
        break;
    }
    case 8: {          // Retro: r = (n²−1)/(n²+2) を n² に逆変換
        const double den = l2 - at(2);
        if (std::fabs(den) < tiny) return false;
        const double r = at(0) + at(1) * l2 / den + at(3) * l2;
        if (r >= 1.0) return false;         // 逆変換の分母が 0/負になる
        n2 = (1.0 + 2.0 * r) / (1.0 - r);
        break;
    }
    case 9: {          // Exotic
        const double den1 = l2 - at(2);
        if (std::fabs(den1) < tiny) return false;
        const double dl = l - at(4);
        const double den2 = dl * dl + at(5);
        if (std::fabs(den2) < tiny) return false;
        n2 = at(0) + at(1) / den1 + at(3) * dl / den2;
        break;
    }
    default:
        return false;
    }

    if (n2 <= 0.0) return false;
    if (n) *n = std::sqrt(n2);
    return true;
}

namespace {

// λ_um における k を線形補間 (範囲外は端の値を使わず「無し」にする)
bool kAt(const std::vector<std::pair<double, double>> &t, double lam, double *k)
{
    if (t.size() < 2 || lam < t.front().first || lam > t.back().first)
        return false;
    for (size_t i = 1; i < t.size(); ++i) {
        if (lam <= t[i].first) {
            const double x0 = t[i - 1].first, x1 = t[i].first;
            const double y0 = t[i - 1].second, y1 = t[i].second;
            const double f = (x1 > x0) ? (lam - x0) / (x1 - x0) : 0.0;
            if (k) *k = y0 + f * (y1 - y0);
            return true;
        }
    }
    return false;
}

} // namespace

NkTable riToNkTable(const RiData &d, int samples)
{
    NkTable t;
    if (!d.ok) { t.error = d.error; return t; }

    if (d.hasTable()) {
        t.points = d.nkTable;
        // k の表が別にあるなら重ねる (n の表 + k の表という書き方がある)
        if (!d.kTable.empty())
            for (optics::NkSample &s : t.points) {
                double k = 0.0;
                if (s.k < 0.0 && kAt(d.kTable, s.lambda_um, &k)) s.k = k;
            }
    } else {
        // 式のみ。k の表があればその波長に合わせる (無ければ範囲を等分)
        if (!d.kTable.empty()) {
            for (const auto &kv : d.kTable) {
                double n = 0.0;
                if (!riEvalN(d, kv.first, &n)) continue;
                t.points.push_back({ kv.first, n, kv.second });
            }
        }
        if (t.points.empty()) {
            const int m = std::max(2, samples);
            for (int i = 0; i < m; ++i) {
                const double lam = d.fMin_um +
                    (d.fMax_um - d.fMin_um) * double(i) / double(m - 1);
                double n = 0.0;
                if (riEvalN(d, lam, &n)) t.points.push_back({ lam, n, -1.0 });
            }
        }
    }

    if (t.points.empty()) {
        t.error = QStringLiteral("no usable points");
        return t;
    }
    std::sort(t.points.begin(), t.points.end(),
              [](const optics::NkSample &a, const optics::NkSample &b) {
                  return a.lambda_um < b.lambda_um;
              });
    for (const optics::NkSample &s : t.points)
        if (s.k >= 0.0) { t.hasK = true; break; }
    t.unit = QStringLiteral("um");
    t.rows = int(t.points.size());
    t.ok = true;
    return t;
}

} // namespace ofd
