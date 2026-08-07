// ScriptsTab.h — スクリプトタブ (ansys-tabs.jsx ScriptsTab 相当)。
// Lumerical の埋め込み Python / LSF コンソール相当:
//   - 言語切替 (Python / LSF) + ドメイン別サンプルコードのエディタ
//   - 実行コンソール (ドメイン別の結果行)
//   - API 早見表 (共通 API + ドメイン別 API)
// スクリプトの読込/保存/サンプル再挿入は動作する。
// 実行は **Python のみ** — 外部 `python3` (無ければ `python`) を
// QProcess で起動し、標準出力/標準エラーをコンソールへ流す。
// LSF は Lumerical 固有の言語でインタプリタが存在しないため実行不可
// (実行ボタンを無効化し、理由をツールチップに出す)。
#pragma once
#include <QScrollArea>
#include <QString>
#include <QVector>

class QPlainTextEdit;
class QPushButton;
class QProcess;
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
    void runScript();        // 実行: python3 を subprocess 起動しコンソールへ
    void abortScript();      // 中断: 実行中のプロセスを止める
    void updateRunButtons(); // 言語と python3 の有無で実行可否を決める

private:
    void setLang(const QString &lang);

    Project *m_p;
    QString  m_lang = "python";        // python | lsf

    QVector<QPushButton*> m_langBtns;  // property("lang") で識別
    SectionBox     *m_editorSec;
    // ── 実行 (python3 の subprocess) ──
    QProcess       *m_proc = nullptr;      // 実行中のみ非 null
    QPushButton    *m_runBtn = nullptr;
    QPushButton    *m_abortBtn = nullptr;
    QString         m_scriptPath;          // 実行に使った一時ファイル
    QPlainTextEdit *m_editor;
    QPlainTextEdit *m_console;
    QTableWidget   *m_api;
};

} // namespace ofd
