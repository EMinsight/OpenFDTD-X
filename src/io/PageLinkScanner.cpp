// PageLinkScanner.cpp
#include "PageLinkScanner.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>
#include <algorithm>

using namespace ofd;

// ── 配布ページのリンク抽出 ──────────────────────────────────────────────────
// HTML から href/src を拾い、ページ URL に対して相対解決したうえで
//   - データファイル (拡張子で判定)
//   - フォルダ / ページ (末尾 '/' や .html — 辿る候補)
// に振り分ける。**JS で組み立てるリンクは取れない** ので、0 件のときは
// その旨をはっきり出す (取れないものを取れるように見せない)。
namespace {

bool isDataExt(const QString &suffix)
{
    static const QStringList kExt = {
        "nc", "nc4", "grd", "asc", "xyz", "csv", "tsv", "txt", "dat",
        "zip", "gz", "bz2", "xz", "tar", "tgz", "7z", "h5", "hdf", "hdf5",
        "bty", "ssp", "env", "tif", "tiff", "geotiff"
    };
    return kExt.contains(suffix.toLower());
}

} // namespace

QVector<PageLink> ofd::scanPageLinks(const QString &pageUrl,
                                     const QByteArray &html, int limit,
                                     bool *truncated)
{
    const QUrl page(pageUrl);
    // 文字コードは配布ページによってまちまちだが、URL は ASCII なので
    // latin1 として走査すれば取りこぼさない (表示名も URL 由来にする)。
    const QString text = QString::fromLatin1(html);
    static const QRegularExpression re(
        QStringLiteral("(?:href|src)\\s*=\\s*[\"\']([^\"\']+)[\"\']"),
        QRegularExpression::CaseInsensitiveOption);
    QVector<PageLink> out;
    QSet<QString> seen;
    *truncated = false;
    auto it = re.globalMatch(text);
    while (it.hasNext()) {
        const QString raw = it.next().captured(1).trimmed();
        if (raw.isEmpty() || raw.startsWith(QLatin1Char('#'))
            || raw.startsWith(QLatin1String("javascript:"), Qt::CaseInsensitive)
            || raw.startsWith(QLatin1String("mailto:"), Qt::CaseInsensitive))
            continue;
        const QUrl u = page.resolved(QUrl(raw));
        const QString scheme = u.scheme().toLower();
        if (scheme != QLatin1String("http") && scheme != QLatin1String("https"))
            continue;
        const QString key = u.toString(QUrl::RemoveFragment);
        if (seen.contains(key)) continue;

        const QString path = u.path();
        const QString name = QFileInfo(path).fileName();
        const QString suffix = QFileInfo(path).suffix();
        PageLink link;
        link.url = key;
        if (isDataExt(suffix)) {
            link.isDir = false;
            link.name = name;
        } else if (path.endsWith(QLatin1Char('/')) || name.isEmpty()
                   || suffix.compare(QLatin1String("html"), Qt::CaseInsensitive) == 0
                   || suffix.compare(QLatin1String("htm"), Qt::CaseInsensitive) == 0
                   || suffix.isEmpty()) {
            link.isDir = true;
            link.name = name.isEmpty() ? u.host() + path : name;
        } else {
            continue;   // 画像・スタイル等
        }
        seen.insert(key);
        out.push_back(link);
        if (out.size() >= limit) { *truncated = true; break; }
    }
    // データファイルを先に、次にフォルダ。同種は名前順。
    std::sort(out.begin(), out.end(), [](const PageLink &a, const PageLink &b) {
        if (a.isDir != b.isDir) return !a.isDir;
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });
    return out;
}

