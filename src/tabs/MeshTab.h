// MeshTab.h — non-uniform mesh editor (メッシュタブ).
// Three coord/division tables map 1:1 to the xmesh/ymesh/zmesh lines.
#pragma once
#include <QScrollArea>
#include <QStringList>

class QCheckBox;
class QComboBox;
class QTableWidget;
class QLabel;

namespace ofd {

class Project;

class MeshTab : public QScrollArea {
    Q_OBJECT
public:
    explicit MeshTab(Project *project, QWidget *parent = nullptr);

private slots:
    void refresh();

private:
    void applyAxis(int axis);
    void refreshAxisInfo(int axis);
    void refreshStats();          // メッシュ統計 (セル数 / 最小Δx / CFL / メモリ)
    void updateMethodView();      // 入力方法 → 名前列の表示/ λ/n チェックの反映

    Project      *m_p;
    bool          m_updating = false;
    QTableWidget *m_table[3];
    QLabel       *m_info[3];
    QLabel       *m_total;

    // 入力方法 / Input method (mock: msh_method + msh_lambda_check)。
    // Project に対応フィールドが無いのでローカル状態 (既定値はモックのまま)。
    QComboBox    *m_method;        // 0=説明あり (with) / 1=説明なし (without)
    QCheckBox    *m_lambdaCheck;   // λ/n チェック
    QStringList   m_names[3];      // 名前列 (説明ありのときだけ表示する注記)

    // メッシュ統計 / Mesh Statistics (mock: msh_cells / msh_dx_min / CFL Δt)
    QLabel       *m_statCells;
    QLabel       *m_statCellsBreak;
    QLabel       *m_statDxMin;
    QLabel       *m_statLambda;
    QLabel       *m_statBadge;
    QLabel       *m_statCfl;
    QLabel       *m_statMem;
};

} // namespace ofd
