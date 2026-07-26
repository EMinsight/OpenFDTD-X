// MainWindow.h — main application shell.
//
// Mirrors the HTML mock's structure (app.jsx):
//   ┌──────────────────────────────────────────────────┐
//   │ menubar                                          │
//   │ toolbar [New][Open][Save] [Run][Post][2D][3D]    │
//   │         [Cloud][Export] [Resources][はじめに]     │
//   │         …  engine / mode / threads               │
//   │ domain tabs [電磁 | 光 | 室内音響 | 水中]          │
//   ├──────┬──────────┬─────────────────────┬──────────┤
//   │ nav  │ page     │ center              │ right    │
//   │ 縦カテ│ QStacked │ Viewport3D/PlotPanel│ tree+log │
//   │ ゴリ  │ Widget   │ + EvViewer bar      │          │
//   └──────┴──────────┴─────────────────────┴──────────┘
//   statusbar: state | cells | mem | Δt | step | progress
//
// 左ナビは Workbench 風カテゴリ (Setup/Library/Solve/Post/ドメイン) 構成で、
// 標準/エキスパート表示モードとドメインで項目をフィルタする (TabNavigator)。
#pragma once
#include <QMainWindow>
#include "core/Domain.h"
#include "kernel/Runner.h"

class QStackedWidget;
class QLabel;
class QProgressBar;
class QComboBox;
class QSpinBox;
class QAction;

namespace ofd {

class Project;
class DomainBar;
class RightDock;
class TabNavigator;
class Viewport3D;
class PlotPanel;
class EvViewer;

// 具体型が必要なタブのみ前方宣言 (それ以外は QWidget* でナビへ登録する)
class OpticalTab;

class RunDialog;
class CloudDialog;
class AppGalleryDialog;
class ResourceDialog;
class GettingStartedDialog;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

public slots:
    void newProject();
    void openProject(const QString &path = {});
    void saveProject();
    void saveProjectAs();
    void runSimulation();
    void runPostProcess();
    void show2DPlot();
    void show3DPlot();
    void exportHdf5();
    void exportTouchstone();
    void exportTidy3d();
    void setDomain(ofd::Domain d);
    void setUiLevel(bool expert);
    void selectLeftTab(const QString &titlePart);
    void showGallery();
    void showResources();
    void showGettingStarted();
    void showCloudDialog();

private slots:
    void onDomainChanged(ofd::Domain d);
    void onProjectChanged();
    void onRunnerProgress(int step, int total);
    void onRunnerLog(const QString &line);
    void onRunnerFinished(bool ok);

private:
    void buildMenu();
    void buildToolbar();
    void buildCentral();
    void buildLeftNav(QWidget *parent);
    void buildDocks();
    void buildStatusBar();
    RunConfig currentRunConfig() const;
    void updateWindowTitle();
    void updateEngineItems(Domain d);

    Project *m_project = nullptr;
    Runner  *m_runner  = nullptr;
    bool     m_expert  = false;      // 表示モード (標準 / エキスパート)

    // ONN 光活性化: カーネルログ "ONN: A_eff = ... [m^2]" から抽出した
    // 実効断面積 (解析解の重ね描き用)。実行開始時に 0 へリセット。
    double   m_lastAeff_m2 = 0.0;

    DomainBar      *m_domainBar = nullptr;
    TabNavigator   *m_nav = nullptr;
    QStackedWidget *m_pages = nullptr;
    RightDock      *m_rightDock = nullptr;
    Viewport3D     *m_viewport = nullptr;
    PlotPanel      *m_plotPanel = nullptr;
    EvViewer       *m_evViewer = nullptr;
    QStackedWidget *m_centerStack = nullptr;

    // モーダル/モードレスダイアログ (遅延生成)
    RunDialog            *m_runDialog = nullptr;
    CloudDialog          *m_cloudDialog = nullptr;
    AppGalleryDialog     *m_galleryDialog = nullptr;
    ResourceDialog       *m_resourceDialog = nullptr;
    GettingStartedDialog *m_gettingStarted = nullptr;

    // Tab pages — TabNavigator へ登録される。ポインタ保持は選択切替用。
    // 型付きが必要なのは OpticalTab のみ (ONN 活性化カーブの表示を呼ぶため)。
    OpticalTab *m_tabOptical = nullptr;

    QWidget *m_tabGeneral = nullptr,  *m_tabMesh = nullptr,
            *m_tabMaterial = nullptr, *m_tabGeometry = nullptr,
            *m_tabSource = nullptr,   *m_tabPost1 = nullptr,
            *m_tabPost2 = nullptr,
            *m_tabAcoustic = nullptr, *m_tabUnderwater = nullptr,
            *m_tabTidy3d = nullptr,   *m_tabGlass = nullptr,
            *m_tabRoomAc = nullptr;
    // オペラ音響解析 (PR #1) — 音響ドメインの解析タブ
    QWidget *m_tabRirAnalysis = nullptr, *m_tabVocal = nullptr,
            *m_tabAuralization = nullptr;
    // 新設タブ (design mock の全カテゴリ)
    QWidget *m_tabSolverRegion = nullptr, *m_tabMonitors = nullptr,
            *m_tabPerFace = nullptr,      *m_tabComponents = nullptr,
            *m_tabMatExplorer = nullptr,  *m_tabLens = nullptr,
            *m_tabGds = nullptr,          *m_tabSchematic = nullptr,
            *m_tabPhotonics = nullptr,    *m_tabAcSource = nullptr,
            *m_tabOceanEnv = nullptr,     *m_tabSoundproof = nullptr,
            *m_tabOutdoor = nullptr,      *m_tabCabin = nullptr,
            *m_tabUltrasound = nullptr,   *m_tabFamily = nullptr,
            *m_tabSolverSel = nullptr,    *m_tabVerification = nullptr,
            *m_tabOptimize = nullptr,     *m_tabTolerance = nullptr,
            *m_tabScripts = nullptr,      *m_tabMultiphysics = nullptr,
            *m_tabAnalysisGroups = nullptr, *m_tabDatasets = nullptr,
            *m_tabH5Viewer = nullptr,     *m_tabInterop = nullptr,
            *m_tabAntennaChar = nullptr,  *m_tabTxLine = nullptr,
            *m_tabScattering = nullptr,   *m_tabCircuit = nullptr;

    QComboBox *m_engineBox = nullptr;
    QComboBox *m_modeBox = nullptr;
    QSpinBox  *m_threadsBox = nullptr;
    QAction   *m_cloudAction = nullptr;
    QAction   *m_levelStandard = nullptr;
    QAction   *m_levelExpert = nullptr;

    QLabel       *m_sbState = nullptr;
    QLabel       *m_sbCells = nullptr;
    QLabel       *m_sbMem = nullptr;
    QLabel       *m_sbDt = nullptr;
    QLabel       *m_sbStep = nullptr;
    QProgressBar *m_sbProgress = nullptr;
};

} // namespace ofd
