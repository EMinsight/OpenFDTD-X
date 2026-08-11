// AnalysisGroupsTab.h — 解析グループタブ (ansys-workflow.jsx AnalysisGroupsTab 相当)。
// モニター+スクリプトをまとめた再利用可能なポスト処理単位 (Lumerical の
// Analysis Group 風)。同じ解析を異なるシミュレーションで使い回せる。
//   - 登録済みグループ表 = Project::analysisGroups() のビュー
//     (追加/削除/編集は .ofdx "analysis_groups" へ永続化。
//      ドメイン別の既定行は新規時の初期値)
//   - ライブラリから読込 (標準 / コミュニティ / ファイル — 未実装)
//   - 新規作成フォーム (名前 + 入力モニター (Project::monitors()) + LSF/Python)
#pragma once
#include <QScrollArea>
#include <QString>
#include <QStringList>

class QComboBox;
class QLineEdit;
class QListWidget;
class QTableWidget;
class QPushButton;
class QPlainTextEdit;
class QProcess;

namespace ofd {

class Project;

class AnalysisGroupsTab : public QScrollArea {
    Q_OBJECT
public:
    explicit AnalysisGroupsTab(Project *project, QWidget *parent = nullptr);

private slots:
    void refresh();          // ドメイン切替 / 読込での再構築

private:
    void addGroup(const QString &name, const QString &monitors);
    void applyGroups();          // 表 → Project::analysisGroups()
    void rebuildGroups();        // Project::analysisGroups() → 表
    void rebuildMonitorChoices();// Project::monitors() → 候補リスト
    void pickScript();           // 選択行にスクリプトファイルを設定する
    void runScript();            // 選択行のスクリプトを QProcess で実行する

    Project      *m_p;
    bool          m_updating = false;

    QTableWidget *m_groups;
    QLineEdit    *m_name;
    QListWidget  *m_monitors;
    QStringList   m_monNames;    // 候補リストの現在の中身 (再構築の抑制用)
    QComboBox    *m_script;
    // スクリプト実行 (QProcess)。実行中は多重起動させない
    QPushButton  *m_pickBtn = nullptr;
    QPushButton  *m_runBtn = nullptr;
    QPlainTextEdit *m_runLog = nullptr;
    QProcess     *m_proc = nullptr;
};

} // namespace ofd
