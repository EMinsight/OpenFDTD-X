#include "SeriesCsv.h"

#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

#include <algorithm>
#include <numeric>

namespace ofd {
namespace io {

cmp::Series parseSeriesCsv(const QString &text, const SeriesCsvOptions &opt)
{
    cmp::Series s;
    if (opt.xCol < 0 || opt.yCol < 0) return s;
    static const QRegularExpression sep(QStringLiteral("[,;\\s]+"));
    const int need = std::max(opt.xCol, opt.yCol) + 1;

    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString &raw : lines) {
        const QString line = raw.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) continue;
        const QStringList parts = line.split(sep, Qt::SkipEmptyParts);
        if (parts.size() < need) continue;
        bool okx = false, oky = false;
        const double x = parts[opt.xCol].toDouble(&okx);
        const double y = parts[opt.yCol].toDouble(&oky);
        if (!okx || !oky) continue;          // 見出し行はここで落ちる
        s.x.push_back(x);
        s.y.push_back(y);
    }
    if (s.x.size() < 2) return cmp::Series();

    // x 昇順に並べ替える (補間が昇順を前提にしている)。既に昇順なら触らない。
    if (!std::is_sorted(s.x.begin(), s.x.end())) {
        std::vector<std::size_t> idx(s.x.size());
        std::iota(idx.begin(), idx.end(), std::size_t(0));
        std::stable_sort(idx.begin(), idx.end(),
                         [&](std::size_t a, std::size_t b) { return s.x[a] < s.x[b]; });
        cmp::Series t;
        t.x.reserve(s.x.size());
        t.y.reserve(s.y.size());
        for (std::size_t k : idx) { t.x.push_back(s.x[k]); t.y.push_back(s.y[k]); }
        return t;
    }
    return s;
}

cmp::Series readSeriesCsv(const QString &path, const SeriesCsvOptions &opt)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return cmp::Series();
    QTextStream ts(&f);
    return parseSeriesCsv(ts.readAll(), opt);
}

} // namespace io
} // namespace ofd
