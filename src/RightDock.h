// RightDock.h — project tree + run log + properties (right side of the window).
//
// モック (app.jsx RightDock) と同じ 3 セグメント構成:
//   Tree  — プロジェクトツリー (メッシュ/物性値/形状/波源/観測点)
//   Log   — カーネル実行ログ
//   Props — ツリーで選択中の要素のプロパティ + ビュー設定
#pragma once
#include <QWidget>

class QTreeWidget;
class QTreeWidgetItem;
class QStackedWidget;
class QFormLayout;
class QLabel;

namespace ofd {

class Project;
class LogConsole;

class RightDock : public QWidget {
    Q_OBJECT
public:
    explicit RightDock(Project *project, QWidget *parent = nullptr);

    void appendLog(const QString &line);

private slots:
    void rebuildTree();
    void showProperties(QTreeWidgetItem *item);

private:
    Project     *m_project;
    QStackedWidget *m_stack;
    QTreeWidget *m_tree;
    LogConsole  *m_log;

    // Props ページ
    QFormLayout *m_propForm;
    QLabel      *m_propEmpty;
};

} // namespace ofd
