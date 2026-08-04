// SolverRegionTab.h — ソルバ領域タブ (ansys-workflow.jsx SolverRegionTab 相当)。
// Lumerical FDTD の中心オブジェクト「FDTD region」を再現:
//   シミュレーション時間・領域・メッシュ精度 (1〜8)・境界条件を一括管理。
// メッシュ精度スライダから推定セル数/メモリ/計算時間を表示 (モックの表をそのまま)。
// PML 層数のみ GeneralOpts::pmlL に永続化、その他はローカル状態。
#pragma once
#include <QScrollArea>

class QCheckBox;
class QComboBox;
class QFormLayout;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QSlider;
class QSpinBox;
class QTableWidget;

namespace ofd {

class Project;
class SectionBox;

class SolverRegionTab : public QScrollArea {
    Q_OBJECT
public:
    explicit SolverRegionTab(Project *project, QWidget *parent = nullptr);

private slots:
    void refresh();

private:
    void apply();
    void updateMeshDerived();   // メッシュ精度 → 推定セル数/メモリ/時間/ヒント
    void updateDomainDeps();    // ドメイン → 基準波長・周波数表示/時間単位/項目の出し分け

    Project   *m_p;
    bool       m_updating = false;

    // ドメイン出し分け用 (音響系では FDTD 固有項目を隠す)
    QLabel      *m_hint;        // 冒頭ヒント (ドメイン別に文言切替)
    QFormLayout *m_meshForm;    // メッシュ設定セクションのフォーム
    QFormLayout *m_timeForm;    // シミュレーション時間セクションのフォーム
    QHBoxLayout *m_dtRow;       // Δt (CFL) 行
    QLabel      *m_dtNote;      // Δt 行のサンプル注記
    QHBoxLayout *m_cflRow;      // CFL 係数行
    SectionBox  *m_bcBox;       // 境界条件セクション (PML は音響系で無意味)

    // シミュレーション領域 / Simulation region
    QComboBox *m_dim;
    QLineEdit *m_xMin, *m_xMax, *m_yMin, *m_yMax, *m_zMin, *m_zMax;

    // メッシュ / Mesh
    QSlider   *m_meshAcc;
    QLabel    *m_meshAccVal, *m_meshHint;
    QLabel    *m_cells, *m_cellsNote, *m_memory, *m_estTime;
    QComboBox *m_meshType, *m_meshRefine;
    QCheckBox *m_subpixel, *m_autoOverride;

    // シミュレーション時間 / Simulation time
    QLineEdit *m_simTime;
    QLabel    *m_simTimeUnit;
    QCheckBox *m_shutoffOn;
    QLineEdit *m_shutoffLevel;
    QLineEdit *m_cfl;

    // 境界条件 / Boundary conditions
    QTableWidget *m_bcTable;
    QComboBox *m_pmlProfile;
    QSpinBox  *m_pmlLayers;
    QCheckBox *m_autoSym;
};

} // namespace ofd
