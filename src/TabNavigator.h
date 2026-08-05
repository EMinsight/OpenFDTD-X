// TabNavigator.h — カテゴリ付き縦タブナビ (app.jsx LeftDock の qt-tabbar 相当)。
//
// モックの Workbench 風カテゴリ構成を再現する:
//   セットアップ / Setup → ライブラリ / Library → 解析 / Solve → ポスト / Post
//   → ドメイン別カテゴリ (電磁/光/音響/水中)
// 各エントリは (ドメイン集合, 標準/エキスパート) でフィルタされ、
// rebuild() が現在のドメインと UI レベルに合う項目だけを並べる。
// 選択すると pageSelected(QWidget*) を発火 — MainWindow が QStackedWidget を切替。
#pragma once
#include <QListWidget>
#include <QVector>
#include "core/Domain.h"

namespace ofd {

class TabNavigator : public QListWidget {
    Q_OBJECT
public:
    // domains: 空 = 全ドメイン共通。core=false はエキスパート時のみ表示。
    struct Entry {
        QString  key;          // "geometry" など (選択維持・CLI 用)
        QString  categoryKey;  // I18n キー ("cat_setup" …)
        QString  labelKey;     // I18n キー ("nav_geometry" …)
        QWidget *page = nullptr;
        QVector<Domain> domains;
        bool     core = true;
        // 音響/水中ドメインでの代替ラベルキー (空 = labelKey をそのまま使う)。
        // 例: 「④ 波源」は音響/水中では「④ 音源」と表記する。
        QString  acLabelKey;
    };

    explicit TabNavigator(QWidget *parent = nullptr);

    void addEntry(const Entry &e);
    // 現在のドメイン/レベルで項目を組み直す。可能なら以前の選択を維持。
    void rebuild(Domain d, bool expert);
    QString currentKey() const;
    // そのドメイン/レベルで表示される項目数 (カテゴリ見出しを除く)。
    // 「標準表示で何項目隠れているか」の表示に使う。
    int pageCount(Domain d, bool expert) const;
    bool selectKey(const QString &key);          // key 完全一致
    bool selectByLabel(const QString &part);     // ラベル部分一致 (CLI --left-tab)

signals:
    void pageSelected(QWidget *page);

private:
    void emitCurrent();
    QVector<Entry> m_entries;
};

} // namespace ofd
