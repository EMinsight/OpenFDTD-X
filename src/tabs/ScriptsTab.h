// ScriptsTab.h — スクリプトタブ (ansys-tabs.jsx ScriptsTab 相当)。
// Lumerical の埋め込み Python / LSF コンソール相当:
//   - 言語切替 (Python / LSF) + ドメイン別サンプルコードのエディタ
//   - 実行コンソール (ドメイン別の結果行)
//   - API 早見表 (共通 API + ドメイン別 API)
// 表示専用のプロトタイプ (実際のインタプリタ実行は行わない)。
#pragma once
#include <QScrollArea>
#include <QString>
#include <QVector>

class QPlainTextEdit;
class QPushButton;
class QTableWidget;

namespace ofd {

class Project;
class SectionBox;

class ScriptsTab : public QScrollArea {
    Q_OBJECT
public:
    explicit ScriptsTab(Project *project, QWidget *parent = nullptr);

private slots:
    void rebuild();          // ドメイン / 言語 → タイトル・コード・コンソール・API表

private:
    void setLang(const QString &lang);

    Project *m_p;
    QString  m_lang = "python";        // python | lsf

    QVector<QPushButton*> m_langBtns;  // property("lang") で識別
    SectionBox     *m_editorSec;
    QPlainTextEdit *m_editor;
    QPlainTextEdit *m_console;
    QTableWidget   *m_api;
};

} // namespace ofd
