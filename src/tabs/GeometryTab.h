// GeometryTab.h — geometry unit editor (物体形状タブ) + STL import + voxelize.
// Maps 1:1 to the "geometry =" lines; unit order = ユニット番号 (later wins).
//
// モック (tabs.jsx の GeometryTab) の CAD パイプライン節も併せて持つ:
//   マウス操作 / STEP テセレーション (OCCT) / アセンブリツリー / 配置・変換 /
//   ジオメトリ修復 / 物性値割当 / 取込プレビュー / 取込済みモデル /
//   ボクセル化 / ボクセル統計 / メッシュ細分化 / 細分化領域
// これらは Project に対応フィールドが無いためローカル状態 (モック既定値) で
// 保持し、実際に動く STL 取込 (io/StlImporter) と staircase ボクセル化
// (io/Voxelizer) の結果だけを表示に反映する。
#pragma once
#include <QScrollArea>
#include "../io/StlImporter.h"

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QTreeWidget;

namespace ofd {

class Project;
class SectionBox;
class UnitNav;

class GeometryTab : public QScrollArea {
    Q_OBJECT
public:
    explicit GeometryTab(Project *project, QWidget *parent = nullptr);

private slots:
    void refresh();
    void importStl();
    void voxelizeImported();

private:
    void applyTable();

    // ── モックの CAD パイプライン節 (tabs.jsx GeometryTab) ──────────────────
    void     addCadImportRows(SectionBox *s);   // 3Dモデル取込 の形式・ファイル行
    QWidget *buildMouseSection();
    QWidget *buildTessellationSection();
    QWidget *buildAssemblySection();
    QWidget *buildPlacementSection();
    QWidget *buildHealingSection();
    QWidget *buildMaterialMapSection();
    QWidget *buildPreviewSection();
    QWidget *buildImportedSection();
    QWidget *buildVoxelSection();
    QWidget *buildVoxelStatsSection();
    QWidget *buildRefineSection();
    QWidget *buildRefinedRegionsSection();

    void refreshImportBadges();   // 取込プレビューを実メッシュで更新
    void refreshVoxelStats();     // ボクセル統計を実結果で更新

    Project      *m_p;
    bool          m_updating = false;
    QTableWidget *m_table;
    UnitNav      *m_nav;
    QLabel       *m_importInfo;
    QSpinBox     *m_voxMat = nullptr;     // material id assigned to voxels
    QPushButton  *m_voxBtn = nullptr;
    ImportedMesh  m_lastMesh;             // most recently imported STL
    bool          m_hasMesh = false;

    // 3Dモデル取込 (CAD)
    QLineEdit      *m_cadFile = nullptr;
    // STEP テセレーション (OCCT)
    QDoubleSpinBox *m_tessDev = nullptr;
    QDoubleSpinBox *m_tessAngle = nullptr;
    QButtonGroup   *m_tessQuality = nullptr;
    QCheckBox      *m_tessParallel = nullptr;
    QCheckBox      *m_tessCurvature = nullptr;
    // アセンブリツリー
    QTreeWidget    *m_asmTree = nullptr;
    // 配置・変換
    QButtonGroup   *m_placeUnit = nullptr;
    QDoubleSpinBox *m_placeScale = nullptr;
    QDoubleSpinBox *m_placeOffset[3] = { nullptr, nullptr, nullptr };
    QDoubleSpinBox *m_placeRot[3] = { nullptr, nullptr, nullptr };
    QCheckBox      *m_placeCenter = nullptr;
    QCheckBox      *m_placeAutoAxis = nullptr;
    // ジオメトリ修復
    QTableWidget   *m_healTable = nullptr;
    // 物性値割当
    QButtonGroup   *m_mapMethod = nullptr;
    QComboBox      *m_mapDefault = nullptr;
    // 取込プレビュー
    QLabel         *m_prevTri = nullptr;
    QLabel         *m_prevSolid = nullptr;
    QLabel         *m_prevVol = nullptr;
    QLabel         *m_prevBbox = nullptr;
    // 取込済みモデル
    QTableWidget   *m_modelTable = nullptr;
    int             m_liveModelRow = -1;   // 実際に取り込んだ STL の行 (-1 = 無し)
    // ボクセル化
    QDoubleSpinBox *m_voxDelta = nullptr;
    QButtonGroup   *m_voxInside = nullptr;
    QButtonGroup   *m_voxSurface = nullptr;
    QCheckBox      *m_voxPvf = nullptr;
    QCheckBox      *m_voxMerge = nullptr;
    QCheckBox      *m_voxOctree = nullptr;
    QSpinBox       *m_voxOctLevel = nullptr;
    QCheckBox      *m_voxGpu = nullptr;
    QLabel         *m_voxBadge = nullptr;
    // ボクセル統計
    QLabel         *m_statOcc = nullptr;
    QLabel         *m_statBnd = nullptr;
    QLabel         *m_statErr = nullptr;
    QLabel         *m_statConf = nullptr;
    // メッシュ細分化
    QButtonGroup   *m_refMethod = nullptr;
    QCheckBox      *m_refEdge = nullptr;
    QCheckBox      *m_refCurve = nullptr;
    QCheckBox      *m_refThin = nullptr;
    QCheckBox      *m_refHighEps = nullptr;
    QSpinBox       *m_refRatio = nullptr;
    QSpinBox       *m_refTransition = nullptr;
    QSpinBox       *m_refLambdaN = nullptr;
    QCheckBox      *m_refAutoCheck = nullptr;
    QCheckBox      *m_refShowViol = nullptr;
    QLabel         *m_refBadge = nullptr;
    QTableWidget   *m_refTable = nullptr;

    // 直近のボクセル化結果 (統計表示用)
    qint64          m_voxOccupied = 0;
    qint64          m_voxTotal = 0;
    bool            m_hasVox = false;
};

} // namespace ofd
