// AnalysisGroupsTab.h — 解析グループタブ (ansys-workflow.jsx AnalysisGroupsTab 相当)。
// モニター+スクリプトをまとめた再利用可能なポスト処理単位 (Lumerical の
// Analysis Group 風)。同じ解析を異なるシミュレーションで使い回せる。
//   - 登録済みグループ表 (ドメイン毎に内容が変わる)
//   - ライブラリから読込 (標準 / コミュニティ / ファイル)
//   - 新規作成フォーム (名前 + 入力モニター + LSF/Python)
#pragma once
#include <QScrollArea>

class QComboBox;
class QLineEdit;
class QListWidget;
class QTableWidget;

namespace ofd {

class Project;

class AnalysisGroupsTab : public QScrollArea {
    Q_OBJECT
public:
    explicit AnalysisGroupsTab(Project *project, QWidget *parent = nullptr);

private slots:
    void refresh();          // ドメインに応じて登録済みグループ表を再構築

private:
    Project      *m_p;
    bool          m_updating = false;

    QTableWidget *m_groups;
    QLineEdit    *m_name;
    QListWidget  *m_monitors;
    QComboBox    *m_script;
};

} // namespace ofd
