// CircuitSolversTab.h — 回路系電磁解析タブ (circuit-solvers.jsx CircuitSolversTab 相当)。
// FDTD (主ソルバ) を補完する回路抽出用ソルバ PEEC / FEM 準静的 / FEM 波動 を選び、
//   モデル/ポート → 抽出設定 → SPICE連成 → 結果 のサブタブで一連の流れを扱う。
//   結果タブは抽出パラメータ表と |Z| (PDNインピーダンス) の MiniPlot を表示。
// Static prototype: 表と値はモックのものをそのまま保持 (Project へは永続化しない)。
#pragma once
#include <QScrollArea>

class QCheckBox;
class QComboBox;
class QLabel;
class QStackedWidget;
class QTableWidget;
class QTabWidget;

namespace ofd {

class MiniPlot;
class Project;

class CircuitSolversTab : public QScrollArea {
    Q_OBJECT
public:
    explicit CircuitSolversTab(Project *project, QWidget *parent = nullptr);

private slots:
    void solverChanged(int index);      // ソルバ切替 → 説明文・抽出ページ・推定時間

private:
    QWidget *buildModelPage();          // モデル/ポート
    QWidget *buildExtractPage();        // 抽出設定 (+ FDTD連成)
    QWidget *buildSpicePage();          // SPICE連成
    QWidget *buildResultsPage();        // 結果 (表 + |Z| プロット)
    QWidget *buildPeecPage();
    QWidget *buildFemqPage();
    QWidget *buildFemwPage();
    void     updateZPlot();             // モックの数式で |Z|(log f) を生成

    Project        *m_p;

    QComboBox      *m_solver;           // PEEC / FEM 準静的 / FEM 波動
    QLabel         *m_solverDesc;
    QTabWidget     *m_tabs;

    QStackedWidget *m_extractStack;     // ソルバ別の抽出設定
    QLabel         *m_estimate;         // 推定計算時間

    QTableWidget   *m_portTable;
    QTableWidget   *m_resultTable;
    MiniPlot       *m_zPlot;
};

} // namespace ofd
