// SolverRegionTab.h — ソルバ領域タブ (ansys-workflow.jsx SolverRegionTab 相当)。
// Lumerical FDTD の中心オブジェクト「FDTD region」を再現:
//   シミュレーション時間・領域・メッシュ精度 (1〜8)・境界条件を一括管理。
// セル数/メモリは Project の実メッシュから、Δt/ステップ数は CFL 係数 ×
// Project::courantDt() から実推定を表示。面別 BC 表は abc/pbc 設定からの導出表示。
// PML 層数のみ GeneralOpts::pmlL に永続化、その他はローカル状態。
//
// シミュレーション時間の節だけは「計算へ反映する」チェックで .ofd の
// `timestep` / `solver` へ書き込める (applyTime)。既定は OFF で、OFF の間は
// GeneralOpts に一切触らない = 出力バイト列は従来どおり (絶対規則 2)。
#pragma once
#include <QScrollArea>
#include <QString>

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
    void applyTime();           // Δt/反復回数/収束条件 → GeneralOpts (チェック時のみ)
    void updateMeshDerived();   // メッシュ精度 → 精度ヒント/目標解像度表示
    void updateDomainDeps();    // ドメイン → 基準波長・周波数表示/時間単位/項目の出し分け
    void updateEstimates();     // Project → セル数/メモリ/Δt/ステップ数/面別 BC 表

    Project   *m_p;
    bool       m_updating = false;

    // ドメイン出し分け用 (音響系では FDTD 固有項目を隠す)
    QLabel      *m_hint;        // 冒頭ヒント (ドメイン別に文言切替)
    QFormLayout *m_meshForm;    // メッシュ設定セクションのフォーム
    QFormLayout *m_timeForm;    // シミュレーション時間セクションのフォーム
    QHBoxLayout *m_dtRow;       // Δt (CFL) 行
    QHBoxLayout *m_cflRow;      // CFL 係数行
    SectionBox  *m_bcBox;       // 境界条件セクション (PML は音響系で無意味)

    // シミュレーション領域 / Simulation region
    QComboBox *m_dim;
    QLineEdit *m_xMin, *m_xMax, *m_yMin, *m_yMax, *m_zMin, *m_zMax;

    // メッシュ / Mesh
    QSlider   *m_meshAcc;
    QLabel    *m_meshAccVal, *m_meshHint;
    QLabel    *m_cells, *m_cellsNote, *m_memory;
    QComboBox *m_meshType, *m_meshRefine;
    QCheckBox *m_subpixel, *m_autoOverride;

    // シミュレーション時間 / Simulation time
    QLineEdit *m_simTime;
    QLabel    *m_simTimeUnit;
    double     m_simTimeScale = 1e-9;  // 時間欄の単位 → 秒 (ドメイン別)
    QCheckBox *m_shutoffOn;
    QLineEdit *m_shutoffLevel;
    QLineEdit *m_cfl;
    QLabel    *m_dtVal, *m_dtSteps, *m_stable;  // Δt / ステップ数 / 安定バッジ
    QCheckBox *m_applyTime;     // ON = この節を .ofd (timestep/solver) へ書く
    QString    m_cflShown;      // refresh() が表示した CFL 文字列 (編集検出用)

    // 境界条件 / Boundary conditions
    QTableWidget *m_bcTable;
    QComboBox *m_pmlProfile;
    QSpinBox  *m_pmlLayers;
    QCheckBox *m_autoSym;
};

} // namespace ofd
