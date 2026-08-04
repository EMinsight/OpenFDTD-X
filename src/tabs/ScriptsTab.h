// ScriptsTab.h — スクリプトタブ (ansys-tabs.jsx ScriptsTab 相当)。
// Lumerical の埋め込み Python / LSF コンソール相当:
//   - 言語切替 (Python / LSF) + ドメイン別サンプルコードのエディタ
//   - 実行コンソール (ドメイン別の結果行)
//   - API 早見表 (共通 API + ドメイン別 API)
// スクリプトの読込/保存/サンプル再挿入は動作する。
// インタプリタ実行 (実行/中断) は未実装。
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
    void loadScript();       // 読込: QFileDialog → エディタ
    void saveScript();       // 保存: エディタ → テキストファイル
    void insertSample();     // 現在の言語×ドメインのサンプルを再挿入

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
