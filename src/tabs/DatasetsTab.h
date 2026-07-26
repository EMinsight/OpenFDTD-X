// DatasetsTab.h — Datasets タブ (optics-tabs.jsx DatasetsTab 相当)。
// COMSOL 風の結果データ管理:
//   - データセット→派生量→プロットグループ→レポートのツリー表示
//   - 派生量定義フォーム (名前 / 式 / 単位 / 自動再計算)
//   - エクスポート (PNG/SVG, CSV, HDF5, Auto-report, PDF, Touchstone)
#pragma once
#include <QScrollArea>

class QCheckBox;
class QLineEdit;
class QTreeWidget;

namespace ofd {

class Project;

class DatasetsTab : public QScrollArea {
    Q_OBJECT
public:
    explicit DatasetsTab(Project *project, QWidget *parent = nullptr);

private:
    Project     *m_p;

    QTreeWidget *m_tree;
    QLineEdit   *m_name, *m_expr, *m_unit;
    QCheckBox   *m_autoRecalc;
};

} // namespace ofd
