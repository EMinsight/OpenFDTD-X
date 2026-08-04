// ProjectTemplates.h — 応用ギャラリーのプロジェクトテンプレート
// (元 mock: app.jsx AppGalleryDialog の groups 配列)。
//
// テンプレート ID ごとに、シナリオに応じたメッシュ・物性値・形状・波源・
// 周波数と各ドメイン設定 (光/音響/水中/tidy3d) を投入した新規プロジェクトを
// 構成する。値は各分野の代表的な設定 (2.45 GHz ダイポール、WR-90 導波管、
// Si フォトニクス 1550 nm、シューボックスホール、Munk 型 SSP 等) に合わせ、
// 保存すればそのままカーネルに渡せる .ofd / .ofdx になることを selftest で
// 検証する。
//
// 適用は Project を clear() してから行う (現在の内容は置き換え)。シグナルは
// 発火しない — 呼び出し側 (MainWindow) が loaded()/changed() を emit する。
#pragma once
#include <QString>
#include <QStringList>

namespace ofd {

class Project;

namespace templates {

// domainKey ("em"/"optical"/"acoustic"/"underwater"/"tidy3d") の全テンプレ ID
QStringList idsFor(const QString &domainKey);

// テンプレートを適用する。displayName はプロジェクトタイトルに使う
// (空ならテンプレート既定のタイトル)。未知の ID は false。
bool apply(Project &p, const QString &domainKey, const QString &id,
           const QString &displayName = QString());

} // namespace templates
} // namespace ofd
