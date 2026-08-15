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

bool riFormulaSupported(int formula) { return formula == 1 || formula == 2; }

// formula 1 : n² = 1 + c0 + Σ c(2i-1)·λ² / (λ² − c(2i)²)
// formula 2 : n² = 1 + c0 + Σ c(2i-1)·λ² / (λ² − c(2i))
bool riEvalN(const RiData &d, double lambda_um, double *n)
{
    if (!riFormulaSupported(d.formula) || d.coeff.isEmpty()) return false;
    if (d.fMax_um > d.fMin_um &&
        (lambda_um < d.fMin_um || lambda_um > d.fMax_um)) return false;

    const double l2 = lambda_um * lambda_um;
    double s = 1.0 + d.coeff[0];
    for (int i = 1; i + 1 < d.coeff.size(); i += 2) {
        const double a = d.coeff[i];
        const double b = d.coeff[i + 1];
        const double den = (d.formula == 1) ? (l2 - b * b) : (l2 - b);
        if (std::fabs(den) < 1e-18) return false;
        s += a * l2 / den;
    }
    if (s <= 0.0) return false;
    if (n) *n = std::sqrt(s);
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
