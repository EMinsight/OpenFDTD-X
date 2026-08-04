// DatasetsTab.h — Datasets タブ (optics-tabs.jsx DatasetsTab 相当)。
// COMSOL 風の結果データ管理:
//   - 作業ディレクトリに実在する結果ファイルのツリー表示 (実行後に更新。
//     モックの固定ツリーは表示しない — 実行していない結果を見せない)
//   - 派生量定義フォーム (名前 / 式 / 単位 / 自動再計算 — 評価器は未実装)
//   - エクスポート (一括出力は未実装 — 実装済みの出力は各タブにある)
#pragma once
#include <QScrollArea>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTreeWidget;

namespace ofd {

class Project;

class DatasetsTab : public QScrollArea {
    Q_OBJECT
public:
    explicit DatasetsTab(Project *project, QWidget *parent = nullptr);

private slots:
    void rebuildTree();     // 作業ディレクトリの結果ファイルを再列挙
    void updateDomainVisibility();  // ドメイン別の出し分け (式の既定例 / Touchstone)

private:
    Project     *m_p;

    QTreeWidget *m_tree;
    QLabel      *m_wdLabel = nullptr;
    QLineEdit   *m_name, *m_expr, *m_unit;
    QCheckBox   *m_autoRecalc;
    QPushButton *m_expTouchstone = nullptr;  // Touchstone .s2p (音響系では非表示)
};

} // namespace ofd
