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
//   │ nav  │ page     │ center (CenterPane) │ right    │
//   │ 縦カテ│ QStacked │ [3D][2D断面][プロット]│ Tree/Log │
//   │ ゴリ  │ Widget   │ [メッシュ]+EvViewer  │ /Props   │
//   └──────┴──────────┴─────────────────────┴──────────┘
//   statusbar: state | cells | mem | Δt | CFL | step | progress
//
// 左ナビは Workbench 風カテゴリ (Setup/Library/Solve/Post/ドメイン) 構成で、
// 標準/エキスパート表示モードとドメインで項目をフィルタする (TabNavigator)。
#pragma once
#include <QList>
#include <QMainWindow>
#include "core/Domain.h"
#include "kernel/Runner.h"
#include "Theme.h"

class QStackedWidget;
class QLabel;
class QProgressBar;
class QCheckBox;
class QComboBox;
class QSpinBox;
class QAction;
class QToolButton;

namespace ofd {

class Project;
class DomainBar;
class RightDock;
class TabNavigator;
class CenterPane;
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
    // オペラ音響の一括レポート (RIR 分析 + 歌声分析 → HTML / CSV)。
    // 実行済みの結果を集めるだけで分析は再実行しない。
    void exportAcousticReport();
    void setDomain(ofd::Domain d);
    void setUiLevel(bool expert);
    void setViewStyle(int index);        // CLI --view-style / 表示メニュー用
    void setThemeOverride(UiStyle style, UiTheme theme, Density density);
    void selectLeftTab(const QString &titlePart);
    void selectCenterTab(const QString &titlePart);  // CLI --center-tab 用
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
    // 実行中は実行設定 (エンジン / モード / スレッド数 / デバイス /
    // Resources) を触れなくする。RunConfig は起動時に 1 度スナップショット
    // されるので、実行中に変えても走っているジョブには一切効かない —
    // 効いているように見せない (絶対規則 5)。
    void setRunUiEnabled(bool enabled);
    void applyTheme();               // QSS 再生成 (スタイル/テーマ/密度/ドメイン)

    Project *m_project = nullptr;
    Runner  *m_runner  = nullptr;
    bool     m_expert  = false;      // 表示モード (標準 / エキスパート)
    UiStyle  m_uiStyle = UiStyle::Classic;
    UiTheme  m_uiTheme = UiTheme::Light;
    Density  m_density = Density::Normal;

    // ONN 光活性化: カーネルログ "ONN: A_eff = ... [m^2]" から抽出した
    // 実効断面積 (解析解の重ね描き用)。実行開始時に 0 へリセット。
    double   m_lastAeff_m2 = 0.0;

    // この実行が activation_curve.csv を生成する (= obpm + powersweep) か。
    // 実行開始時に決め、これが false の実行では ONN 結果表示を行わない
    // (過去の実行が残した CSV を無関係な実行の結果として再表示しない)。
    bool     m_expectActivation = false;
    // 解析解の重ね描きに使う β [cm/GW] と伝搬長 L [m] の実行開始時
    // スナップショット (実行中に UI を編集されても実測 CSV と対応させる)。
    double   m_runTpaBeta_cmGW = 0.0;
    double   m_runLength_m = 0.0;

    DomainBar      *m_domainBar = nullptr;
    TabNavigator   *m_nav = nullptr;
    CenterPane     *m_center = nullptr;
    QStackedWidget *m_pages = nullptr;
    RightDock      *m_rightDock = nullptr;
    Viewport3D     *m_viewport = nullptr;
    PlotPanel      *m_plotPanel = nullptr;
    EvViewer       *m_evViewer = nullptr;

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
    // 応用画面 (mock: em-applications / optical-applications)
    QWidget *m_tabEmc = nullptr,      *m_tabSar = nullptr,
            *m_tabChannel = nullptr,  *m_tabThinFilm = nullptr,
            *m_tabIllum = nullptr,    *m_tabDisplayOpt = nullptr;
    // handoff2 デザイン差分 (audio-editor / pic-tools / opera-analysis)
    QWidget *m_tabAudioEdit = nullptr, *m_tabModeSolver = nullptr,
            *m_tabAcSolver = nullptr;

    QComboBox *m_engineBox = nullptr;
    QComboBox *m_modeBox = nullptr;
    QSpinBox  *m_threadsBox = nullptr;
    QSpinBox  *m_deviceBox = nullptr;        // GPU デバイス番号
    QLabel    *m_deviceLabel = nullptr;
    QAction   *m_deviceAction = nullptr;     // ラベル/スピンの表示制御用
    QAction   *m_deviceBoxAction = nullptr;
    QAction   *m_cloudAction = nullptr;
    // 「計算」アクション。実行中は表示を「計算を中止」へ切り替える —
    // 同じボタンをもう一度押すと中止になるのが分からない、という指摘への対応。
    QAction   *m_runAction = nullptr;         // ツールバー側
    QAction   *m_runMenuAction = nullptr;     // 実行メニュー側 (F5)
    // Resources を開くアクション (ツールバーとツールメニューの 2 箇所)。
    // 実行中は両方まとめて無効化する。
    QList<QAction *> m_resourceActions;
    QAction   *m_cloudMenuAction = nullptr;   // ツールメニュー側 (同じ制約)
    // ドメインで意味を持たないエクスポートは無効化する (S2P = EM の
    // S パラメータ、tidy3d py = 光専用クラウドバックエンド)
    QAction   *m_s2pMenuAction = nullptr;     // ポストメニュー側
    QAction   *m_s2pExportAction = nullptr;   // ツールバーのエクスポート側
    QAction   *m_t3ExportAction = nullptr;
    QAction   *m_levelStandard = nullptr;
    QAction   *m_levelExpert = nullptr;
    // ナビ直下の表示モード切替 (標準では何項目隠れているかも出す)
    QCheckBox *m_levelCheck = nullptr;
    void updateLevelHint();

    QLabel       *m_sbState = nullptr;
    QLabel       *m_sbCells = nullptr;
    QLabel       *m_sbMem = nullptr;
    QLabel       *m_sbDt = nullptr;
    QLabel       *m_sbCfl = nullptr;
    QLabel       *m_sbStep = nullptr;
    QProgressBar *m_sbProgress = nullptr;

    // 現ドメインのカーネル未検出警告 (クリックでカーネルパス設定を開く)。
    // OpenFDTD (EM) は基幹カーネルなので起動直後から表示され得る。
    QToolButton  *m_sbKernelWarn = nullptr;
    void updateKernelWarn();

    // 実行開始時刻 [ms]。HDF5 結果の「この実行が生成したものか」判定に使う
    qint64 m_runStartMs = 0;
};

} // namespace ofd
