// NkCsv.cpp
#include "NkCsv.h"

#include <QFile>
#include <QRegularExpression>
#include <QStringList>
#include <QTextStream>

#include <algorithm>
#include <cmath>

namespace ofd {

namespace {

// 区切りは , / ; / TAB / 連続する空白のどれでもよい。
QStringList splitFields(const QString &line)
{
    static const QRegularExpression sep(QStringLiteral("[,;\\t ]+"));
    return line.split(sep, Qt::SkipEmptyParts);
}

// ヘッダ行の単位語を探す。見つからなければ空文字。
// 「nm」を含む語は μm より優先度が高くならないよう、語単位で見る。
QString unitFromHeaderLine(const QString &line)
{
    const QString s = line.toLower();
    // μ は UTF-8 で 2 バイトになるので QString のまま比較する
    if (s.contains(QStringLiteral("nm"))) return QStringLiteral("nm");
    if (s.contains(QStringLiteral("um")) || s.contains(QString::fromUtf8("μm"))
        || s.contains(QString::fromUtf8("µm"))
        || s.contains(QStringLiteral("micron")))
        return QStringLiteral("um");
    // 単独の "m" や "[m]" は「メートル」。"nm"/"um" は上で処理済み
    static const QRegularExpression metre(QStringLiteral("(^|[^a-z])m([^a-z]|$)"));
    if (metre.match(s).hasMatch()) return QStringLiteral("m");
    return QString();
}

double toMicrometre(double v, const QString &unit)
{
    if (unit == QStringLiteral("nm")) return v * 1.0e-3;
    if (unit == QStringLiteral("m"))  return v * 1.0e6;
    return v;                                   // "um"
}

} // namespace

NkTable parseNkCsv(const QString &text, const QString &unitOverride)
{
    NkTable t;

    // ── 1 パス目: 行を数値の並びへ ────────────────────────────────────────
    struct Row { double w, n, k; };
    std::vector<Row> rows;
    QString headerUnit;
    bool sawHeader = false;

    const QStringList lines = text.split(QRegularExpression("\r\n|\n|\r"));
    for (const QString &raw : lines) {
        const QString line = raw.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) continue;

        const QStringList f = splitFields(line);
        if (f.size() < 2) { ++t.skipped; continue; }

        bool okW = false, okN = false;
        const double w = f[0].toDouble(&okW);
        const double n = f[1].toDouble(&okN);
        if (!okW || !okN) {
            // 数値でない行は、最初の 1 本だけヘッダとして扱う。
            // 2 本目以降は壊れた行として数える (読み飛ばした事実を残す)。
            if (!sawHeader && rows.empty()) {
                sawHeader = true;
                headerUnit = unitFromHeaderLine(line);
            } else {
                ++t.skipped;
            }
            continue;
        }
        double k = -1.0;                        // 既定は「k のデータ無し」
        if (f.size() >= 3) {
            bool okK = false;
            const double kv = f[2].toDouble(&okK);
            if (okK) { k = kv; t.hasK = true; }
        }
        if (!std::isfinite(w) || !std::isfinite(n) || !(w > 0.0)) {
            ++t.skipped;
            continue;
        }
        rows.push_back({ w, n, k });
    }
    t.rows = static_cast<int>(rows.size());

    if (rows.size() < 2) {
        t.error = QStringLiteral("too few numeric rows");
        return t;                               // points は空のまま
    }

    // ── 単位を決める ──────────────────────────────────────────────────────
    if (unitOverride == QStringLiteral("nm")
        || unitOverride == QStringLiteral("um")
        || unitOverride == QStringLiteral("m")) {
        t.unit = unitOverride;
        t.unitFromHeader = false;
    } else if (!headerUnit.isEmpty()) {
        t.unit = headerUnit;
        t.unitFromHeader = true;
    } else {
        // 中央値の桁で決める。平均ではなく中央値にするのは、
        // 外れ値 1 点で単位が変わらないようにするため。
        std::vector<double> w;
        w.reserve(rows.size());
        for (const Row &r : rows) w.push_back(r.w);
        std::sort(w.begin(), w.end());
        const double med = w[w.size() / 2];
        t.unit = (med < 0.01) ? QStringLiteral("m")
               : (med < 100.0) ? QStringLiteral("um")
                               : QStringLiteral("nm");
        t.unitFromHeader = false;
    }

    // ── 波長を μm へそろえ、昇順・重複除去 ────────────────────────────────
    std::vector<optics::NkSample> pts;
    pts.reserve(rows.size());
    for (const Row &r : rows) {
        optics::NkSample s;
        s.lambda_um = toMicrometre(r.w, t.unit);
        s.n = r.n;
        s.k = r.k;
        pts.push_back(s);
    }
    std::stable_sort(pts.begin(), pts.end(),
                     [](const optics::NkSample &a, const optics::NkSample &b) {
                         return a.lambda_um < b.lambda_um;
                     });
    std::vector<optics::NkSample> uniq;
    uniq.reserve(pts.size());
    for (const optics::NkSample &s : pts) {
        if (!uniq.empty()
            && std::fabs(s.lambda_um - uniq.back().lambda_um)
                   <= 1.0e-12 * std::max(1.0, std::fabs(s.lambda_um))) {
            ++t.duplicates;                     // 最初の値を残す (後勝ちにしない)
            continue;
        }
        uniq.push_back(s);
    }

    if (uniq.size() < 2) {
        t.error = QStringLiteral("fewer than two distinct wavelengths");
        return t;
    }
    t.points = uniq;
    t.ok = true;
    return t;
}

NkTable readNkCsv(const QString &path, const QString &unitOverride)
{
    NkTable t;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        t.error = QStringLiteral("cannot open the file");
        return t;
    }
    QTextStream in(&f);
    const QString text = in.readAll();
    f.close();
    return parseNkCsv(text, unitOverride);
}

} // namespace ofd
