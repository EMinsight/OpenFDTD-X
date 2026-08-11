// GeometryTab.h — geometry unit editor (物体形状タブ) + STL import + voxelize.
// Maps 1:1 to the "geometry =" lines; unit order = ユニット番号 (later wins).
//
// モック (tabs.jsx の GeometryTab) の CAD パイプライン節も併せて持つ:
//   マウス操作 / STEP テセレーション (OCCT) / アセンブリツリー / 配置・変換 /
//   ジオメトリ検査 / 物性値割当 / 取込プレビュー / 取込済みモデル /
//   ボクセル化 / ボクセル統計 / メッシュ細分化 / 細分化領域
//
// 表示する数値は「実測・実計算した値」だけに限る (モックの固定サンプル値は
// 出さない):
//   - アセンブリツリー / 取込プレビュー / 取込済みモデル / ボクセル統計
//     … 取込 STL (io/MeshImporter) と staircase ボクセル化 (io/Voxelizer) の実測値。
//       未取込・未実行のときは「—」+ 何をすれば埋まるかの導線を出す。
//   - ジオメトリ検査 … io/MeshDiagnostics が実メッシュから数えた検出数
//       (自動修復は未実装なので検出のみと明記する)。
//   - 細分化領域 … Project::refineRegions() (.ofdx に永続化) を編集する表。
//       セル増は現在の基本格子から数えた見積り (細分化の実行は未実装)。
// テセレーション/物性値割当など計算に届かない設定は unwiredNote で明示する。
#pragma once
#include <QScrollArea>
#include "../core/Geometry.h"
#include "../io/MeshAxes.h"
#include "../io/MeshDiagnostics.h"
#include "../io/MeshRepair.h"
#include "../io/MeshImporter.h"

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class QSpinBox;
class QTableWidget;
class QTreeWidget;

namespace ofd {

class Project;
class SectionBox;
class UnitNav;
struct RefineRegion;   // core/Project.h — 細分化領域 (.ofdx へ永続化)

class GeometryTab : public QScrollArea {
    Q_OBJECT
public:
    explicit GeometryTab(Project *project, QWidget *parent = nullptr);

private slots:
    void runHealing();          // 取込メッシュを修復して検査し直す
    void applyAutoAxis();       // 主軸を検出して回転欄へ入れる
    void refresh();
    void importStl();
    void voxelizeImported();

protected:
    // タブが表示されるたびに細分化領域のセル増見積りを取り直す
    // (別タブで格子を変えたときに古い見積りを残さない)
    void showEvent(QShowEvent *e) override;

private:
    void applyTable();
    void updateDomainVisibility();   // ドメイン別の出し分け (表示のみ)

    // ── ユニット編集 (mock tabs.jsx:409-494) ────────────────────────────────
    QWidget *buildTransformSection();
    int  currentUnit() const;             // 選択中ユニット (-1 = 無し)
    void insertUnitAfterCurrent(const Geometry &g);   // 挿入/複製の共通処理

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
    void refreshAssemblyTree();   // アセンブリツリーを実メッシュで更新
    void refreshHealing();        // ジオメトリ検査の表を実検出数で更新

    // 細分化領域 (Project::refineRegions() ↔ 表)
    void refreshRegionTable();    // model → widgets
    void applyRegionTable();      // widgets → model
    void updateRefineBadge();     // 領域定義 + 基本格子からセル増を計算
    // 領域内に中心を持つ基本セル数 (現在の xmesh/ymesh/zmesh から数える)
    qint64 cellsInRegion(const RefineRegion &r) const;

    // 配置・変換 (placement) — 取込 STL への純幾何アフィン変換
    // (スケール → 中心合わせ → 回転 → オフセット)。StlImporter/Voxelizer 不変。
    ImportedMesh applyPlacement(const ImportedMesh &src) const;
    void reapplyPlacement();      // 配置設定の変更 → 変換をかけ直して表示更新
    void showMeasureDialog();     // 寸法測定ダイアログ (取込メッシュの計測)

    Project      *m_p;
    bool          m_updating = false;
    QTableWidget *m_table;
    UnitNav      *m_nav;
    QLabel       *m_importInfo;

    // ユニット編集 (平行移動/回転スライダー + 挿入/複製/ミラー)
    QSlider   *m_trSlider[3] = { nullptr, nullptr, nullptr };
    QSlider   *m_rotSlider[3] = { nullptr, nullptr, nullptr };
    QLabel    *m_trValue[3] = { nullptr, nullptr, nullptr };
    QLabel    *m_rotValue[3] = { nullptr, nullptr, nullptr };
    QComboBox *m_mirrorAxis = nullptr;
    QLabel    *m_xformWarn = nullptr;
    Geometry   m_dragBase;        // ドラッグ開始時のユニット (基準)
    int        m_dragUnit = -1;   // ドラッグ対象 (-1 = ドラッグ中でない)
    QSpinBox     *m_voxMat = nullptr;     // material id assigned to voxels
    QPushButton  *m_voxBtn = nullptr;
    ImportedMesh  m_lastMesh;             // 取込 STL (配置・変換の適用後)
    ImportedMesh  m_rawMesh;              // 取込 STL (変換前 — placement の基準)
    bool          m_hasMesh = false;
    // 取込時に 1 度だけ計算する位相検査結果 (位相はアフィン変換で不変なので
    // 配置・変換のたびに数え直さない — GUI スレッドでの再計算を避ける)
    MeshDiagnostics m_diag;

    // 3Dモデル取込 (CAD)
    QLineEdit      *m_cadFile = nullptr;
    // STEP テセレーション (OCCT)
    QDoubleSpinBox *m_tessDev = nullptr;
    QDoubleSpinBox *m_tessAngle = nullptr;
    QButtonGroup   *m_tessQuality = nullptr;
    QCheckBox      *m_tessParallel = nullptr;
    QCheckBox      *m_tessCurvature = nullptr;
    // アセンブリツリー (取込済み STL の実測値。未取込なら空 + 導線)
    QTreeWidget    *m_asmTree = nullptr;
    QLabel         *m_asmNone = nullptr;
    // 配置・変換
    QButtonGroup   *m_placeUnit = nullptr;
    QDoubleSpinBox *m_placeScale = nullptr;
    QDoubleSpinBox *m_placeOffset[3] = { nullptr, nullptr, nullptr };
    QDoubleSpinBox *m_placeRot[3] = { nullptr, nullptr, nullptr };
    QCheckBox      *m_placeCenter = nullptr;
    QCheckBox      *m_placeAutoAxis = nullptr;
    // ジオメトリ検査 (検出のみ — 修復は未実装)
    QLabel         *m_autoAxisNote = nullptr;  // 主軸検出の結果
    QPushButton    *m_healBtn = nullptr;    // 「▶ 修復を実行」
    QLabel         *m_healResult = nullptr; // 修復結果 (実行後のみ)
    QTableWidget   *m_healTable = nullptr;
    QLabel         *m_healNone = nullptr;   // 未取込 / 検査省略の説明
    // 物性値割当
    QButtonGroup   *m_mapMethod = nullptr;
    QComboBox      *m_mapDefault = nullptr;
    // 取込メッシュの部品 (OBJ の g / o / usemtl) → 材料番号の割当表。
    // 部品分けのあるファイルを読んだときだけ中身が入る
    void            rebuildPartTable();   // 取込後に部品表を作り直す
    QTableWidget   *m_mapTable = nullptr;
    QLabel         *m_mapNote = nullptr;
    // 取込プレビュー
    QLabel         *m_prevTri = nullptr;
    QLabel         *m_prevSolid = nullptr;
    QLabel         *m_prevVol = nullptr;
    QLabel         *m_prevBbox = nullptr;
    // 取込プレビュー: 未取込の説明 (取込後は隠す)
    QLabel         *m_prevNone = nullptr;
    // 取込済みモデル
    QTableWidget   *m_modelTable = nullptr;
    QLabel         *m_modelNone = nullptr;
    int             m_liveModelRow = -1;   // 実際に取り込んだ STL の行 (-1 = 無し)
    // ボクセル化
    QWidget        *m_voxSection = nullptr;       // Underwater で非表示
    QWidget        *m_voxStatsSection = nullptr;  // 同上
    QLabel         *m_voxDeltaHint = nullptr;     // 分解度Δ の補足 (ドメイン別)
    QLabel         *m_voxSurfHint = nullptr;      // 表面処理の解説 (ドメイン別)
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
    // 部分体積率 (PVF) を有効にして実行したときだけ意味を持つ統計
    bool            m_voxHasPvf = false;
    qint64          m_voxBoundary = 0;   // 面が横切ったセル数
    int             m_voxPvfN = 0;       // 1 セルあたりの再標本数 (軸方向)
    double          m_voxStairVol = 0.0; // 占有セルの総体積 [m³]
    double          m_voxPvfVol = 0.0;   // PVF の体積推定 [m³]
    double          m_voxMeshVol = 0.0;  // 取込メッシュの体積 [m³] (基準)
};

} // namespace ofd
