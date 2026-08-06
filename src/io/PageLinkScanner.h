// PageLinkScanner.h — 配布ページ (HTML) からデータファイルへのリンクを抜き出す。
//
// GEBCO / ETOPO / JODC / NOAA のような配布サイトは、ページやディレクトリを
// 辿った先に実ファイルが置いてある。ここは取得したページの HTML から
// href / src を拾い、ページ URL に対して相対解決したうえで
//   - データファイル (拡張子で判定 — .nc / .asc / .csv / .zip ほか)
//   - フォルダ・ページ (末尾 '/' や .html — 辿る候補)
// に振り分ける。取得そのものは呼び出し側 (OeDownloadManager, Qt6::Network)。
//
// **JavaScript で組み立てられるリンクや、検索フォーム経由の配布は取れない**。
// 0 件のときは呼び出し側がその旨を明示すること (取れないものを取れるように
// 見せない)。
#pragma once
#include <QByteArray>
#include <QString>
#include <QVector>

namespace ofd {

// 抜き出したリンク 1 件
struct PageLink {
    bool    isDir = false;   // true = フォルダ / ページ (辿る候補)
    QString name;            // 表示名 (URL 末尾のファイル名)
    QString url;             // 絶対 URL (フラグメント除去済み)
};

// html から候補リンクを抜き出す。limit 件で打ち切り、打ち切ったら
// *truncated = true。データファイルを先に、同種は名前順で返す。
QVector<PageLink> scanPageLinks(const QString &pageUrl, const QByteArray &html,
                                int limit, bool *truncated);

} // namespace ofd
