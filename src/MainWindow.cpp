// MainWindow.cpp
#include "MainWindow.h"
#include "DomainBar.h"
#include "RightDock.h"
#include "TabNavigator.h"
#include "I18n.h"

#include "core/Project.h"
#include "io/H5Writer.h"
#include "io/Tidy3dExporter.h"
#include "io/Touchstone.h"

#include "widgets/EvViewer.h"
#include "widgets/PlotPanel.h"
#include "widgets/Viewport3D.h"

// 本家章立てタブ
#include "tabs/GeneralTab.h"
#include "tabs/MeshTab.h"
#include "tabs/MaterialTab.h"
#include "tabs/GeometryTab.h"
#include "tabs/SourceTab.h"
#include "tabs/Post1Tab.h"
#include "tabs/Post2Tab.h"
#include "tabs/OpticalTab.h"
#include "tabs/AcousticTab.h"
#include "tabs/UnderwaterTab.h"
#include "tabs/Tidy3dTab.h"
#include "tabs/GlassCatalogTab.h"
#include "tabs/RoomAcousticsTab.h"
// Workbench 拡張タブ (design mock 全カテゴリ)
#include "tabs/SolverRegionTab.h"
#include "tabs/MonitorsTab.h"
#include "tabs/PerFaceBCTab.h"
#include "tabs/ComponentsTab.h"
#include "tabs/MaterialExplorerTab.h"
#include "tabs/LensEditorTab.h"
#include "tabs/LayoutGDSTab.h"
#include "tabs/SchematicTab.h"
#include "tabs/PhotonicsSolversTab.h"
#include "tabs/AcousticSourceTab.h"
#include "tabs/OceanEnvironmentTab.h"
#include "tabs/SoundproofTab.h"
#include "tabs/OutdoorNoiseTab.h"
#include "tabs/CabinAcousticsTab.h"
#include "tabs/UltrasoundTab.h"
#include "tabs/FamilySolverTab.h"
#include "tabs/SolverSelectorTab.h"
#include "tabs/VerificationTab.h"
#include "tabs/OptimizeTab.h"
#include "tabs/ToleranceTab.h"
#include "tabs/ScriptsTab.h"
#include "tabs/MultiphysicsTab.h"
#include "tabs/AnalysisGroupsTab.h"
#include "tabs/DatasetsTab.h"
#include "tabs/H5ViewerTab.h"
#include "tabs/InteropTab.h"
#include "tabs/AntennaCharTab.h"
#include "tabs/TransmissionLineTab.h"
#include "tabs/ScatteringTab.h"
#include "tabs/CircuitSolversTab.h"
// ダイアログ
#include "dialogs/RunDialog.h"
#include "dialogs/CloudDialog.h"
#include "dialogs/AppGalleryDialog.h"
#include "dialogs/ResourceDialog.h"
#include "dialogs/GettingStartedDialog.h"

#include <QActionGroup>
#include <QApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QFileDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressBar>
#include <QRegularExpression>
#include <QSettings>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

using namespace ofd;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_project(new Project(this))
    , m_runner(new Runner(this))
{
    setObjectName("OpenFDTD_MainWindow");
    resize(1500, 940);
    setMinimumSize(1100, 700);

    m_expert = QSettings().value("ui/level", "standard").toString() == "expert";

    buildMenu();
    buildToolbar();
    buildCentral();
    buildDocks();
    buildStatusBar();

    connect(m_project, &Project::domainChanged, this, &MainWindow::onDomainChanged);
    connect(m_project, &Project::changed, this, &MainWindow::onProjectChanged);

    connect(m_runner, &Runner::progress, this, &MainWindow::onRunnerProgress);
    connect(m_runner, &Runner::logLine, this, &MainWindow::onRunnerLog);
    connect(m_runner, &Runner::finished, this, &MainWindow::onRunnerFinished);

    onDomainChanged(m_project->activeDomain());
    onProjectChanged();
    updateWindowTitle();
}

MainWindow::~MainWindow() = default;

// ── Menus ───────────────────────────────────────────────────────────────────
void MainWindow::buildMenu()
{
    auto *mb = menuBar();
    auto *mFile = mb->addMenu(I18n::tr("m_file"));
    auto *mView = mb->addMenu(I18n::tr("m_view"));
    auto *mRun  = mb->addMenu(I18n::tr("m_run"));
    auto *mPost = mb->addMenu(I18n::tr("m_post"));
    auto *mTools= mb->addMenu(I18n::tr("m_tools"));
    auto *mHelp = mb->addMenu(I18n::tr("m_help"));

    mFile->addAction(I18n::tr("tb_new"), QKeySequence::New,
                     this, &MainWindow::newProject);
    mFile->addAction(I18n::tr("tb_open"), QKeySequence::Open,
                     this, [this] { openProject(); });
    mFile->addAction(I18n::tr("tb_save"), QKeySequence::Save,
                     this, &MainWindow::saveProject);
    mFile->addAction(I18n::tr("tb_saveas"), QKeySequence::SaveAs,
                     this, &MainWindow::saveProjectAs);
    mFile->addSeparator();
    mFile->addAction(I18n::tr("m_exit"), this, [] { qApp->quit(); });

    // 表示モード (標準 / エキスパート) — モックの uiLevel トグル相当
    auto *levelMenu = mView->addMenu(I18n::tr("m_uilevel"));
    auto *lvGroup = new QActionGroup(this);
    m_levelStandard = levelMenu->addAction(I18n::tr("uilevel_standard"));
    m_levelExpert   = levelMenu->addAction(I18n::tr("uilevel_expert"));
    for (auto *a : { m_levelStandard, m_levelExpert }) {
        a->setCheckable(true);
        lvGroup->addAction(a);
    }
    (m_expert ? m_levelExpert : m_levelStandard)->setChecked(true);
    connect(m_levelStandard, &QAction::triggered, this,
            [this] { setUiLevel(false); });
    connect(m_levelExpert, &QAction::triggered, this,
            [this] { setUiLevel(true); });

    auto *langMenu = mView->addMenu(I18n::tr("m_lang"));
    for (const auto &[code, label] : std::initializer_list<
             std::pair<const char *, const char *>>{
             { "ja", "日本語" }, { "en", "English" }, { "both", "日英 / JA+EN" } }) {
        langMenu->addAction(QString::fromUtf8(label), this, [this, code = QString(code)] {
            QSettings().setValue("ui/language", code);
            QMessageBox::information(this, "OpenFDTD-X", I18n::tr("m_lang_restart"));
        });
    }

    mRun->addAction(I18n::tr("tb_calc"), QKeySequence(Qt::Key_F5),
                    this, &MainWindow::runSimulation);
    mRun->addAction(I18n::tr("tb_stop"), QKeySequence(Qt::Key_Escape),
                    this, [this] { m_runner->stop(); });

    mPost->addAction(I18n::tr("tb_post"), this, &MainWindow::runPostProcess);
    mPost->addAction(I18n::tr("tb_plot2d"), this, &MainWindow::show2DPlot);
    mPost->addAction(I18n::tr("tb_plot3d"), this, &MainWindow::show3DPlot);
    mPost->addSeparator();
    mPost->addAction(I18n::tr("pp_export_csv"), this, [this] {
        const QString p = QFileDialog::getSaveFileName(
            this, I18n::tr("pp_export_csv"), "convergence.csv", "CSV (*.csv)");
        if (!p.isEmpty()) m_plotPanel->exportCsv(p);
    });
    mPost->addAction(I18n::tr("pp_export_h5"), this, &MainWindow::exportHdf5);
    mPost->addAction(I18n::tr("pp_export_s2p"), this, &MainWindow::exportTouchstone);

    mTools->addAction(I18n::tr("tb_cloud"), this, &MainWindow::showCloudDialog);
    mTools->addAction(I18n::tr("tb_resources"), this, &MainWindow::showResources);
    mTools->addAction(I18n::tr("tb_gettingstarted"),
                      this, &MainWindow::showGettingStarted);

    mHelp->addAction(I18n::tr("tb_gettingstarted"),
                     this, &MainWindow::showGettingStarted);
    mHelp->addAction("About OpenFDTD-X…", this, [this] {
        QMessageBox::about(this, "OpenFDTD-X",
            QStringLiteral("<b>OpenFDTD-X</b><br>%1<br><br>"
                           "OpenFDTD / OpenRCWA / OpenBPM GUI front-end")
                .arg(I18n::tr("app_subtitle")));
    });
}

// ── Toolbar ─────────────────────────────────────────────────────────────────
void MainWindow::buildToolbar()
{
    auto *tb = addToolBar("Main");
    tb->setObjectName("MainToolBar");
    tb->setMovable(false);
    tb->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    tb->setIconSize({ 16, 16 });
    auto icon = [this](QStyle::StandardPixmap sp) { return style()->standardIcon(sp); };

    // モック同様「新規」はアプリケーションギャラリーを開く
    auto *newAct = tb->addAction(icon(QStyle::SP_FileIcon), I18n::tr("tb_new"),
                                 this, &MainWindow::showGallery);
    newAct->setToolTip(I18n::tr("tb_gallery_tip"));
    tb->addAction(icon(QStyle::SP_DialogOpenButton), I18n::tr("tb_open"),
                  this, [this] { openProject(); });
    tb->addAction(icon(QStyle::SP_DialogSaveButton), I18n::tr("tb_save"),
                  this, &MainWindow::saveProject);
    tb->addSeparator();

    auto *runAct = tb->addAction(icon(QStyle::SP_MediaPlay), I18n::tr("tb_calc"),
                                 this, &MainWindow::runSimulation);
    if (auto *btn = qobject_cast<QToolButton *>(tb->widgetForAction(runAct)))
        btn->setObjectName("primaryAction");
    tb->addAction(icon(QStyle::SP_MediaSeekForward), I18n::tr("tb_post"),
                  this, &MainWindow::runPostProcess);
    tb->addAction(icon(QStyle::SP_FileDialogContentsView), I18n::tr("tb_plot2d"),
                  this, &MainWindow::show2DPlot);
    tb->addAction(icon(QStyle::SP_FileDialogListView), I18n::tr("tb_plot3d"),
                  this, &MainWindow::show3DPlot);
    tb->addSeparator();

    m_cloudAction = tb->addAction(icon(QStyle::SP_ArrowUp), I18n::tr("tb_cloud"),
                                  this, &MainWindow::showCloudDialog);

    // 📤 エクスポート (CSV / HDF5 / S2P / tidy3d py)
    auto *exportBtn = new QToolButton(tb);
    exportBtn->setText(I18n::tr("tb_export"));
    exportBtn->setIcon(icon(QStyle::SP_DialogSaveButton));
    exportBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    exportBtn->setPopupMode(QToolButton::InstantPopup);
    auto *exportMenu = new QMenu(exportBtn);
    exportMenu->addAction(I18n::tr("pp_export_csv"), this, [this] {
        const QString p = QFileDialog::getSaveFileName(
            this, I18n::tr("pp_export_csv"), "convergence.csv", "CSV (*.csv)");
        if (!p.isEmpty()) m_plotPanel->exportCsv(p);
    });
    exportMenu->addAction(I18n::tr("pp_export_h5"), this, &MainWindow::exportHdf5);
    exportMenu->addAction(I18n::tr("pp_export_s2p"), this, &MainWindow::exportTouchstone);
    exportMenu->addAction(I18n::tr("t3_export"), this, &MainWindow::exportTidy3d);
    exportBtn->setMenu(exportMenu);
    tb->addWidget(exportBtn);

    tb->addSeparator();
    tb->addAction(I18n::tr("tb_resources"), this, &MainWindow::showResources);
    tb->addAction(I18n::tr("tb_gettingstarted"),
                  this, &MainWindow::showGettingStarted);

    auto *spacer = new QWidget(tb);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    tb->addWidget(spacer);

    tb->addWidget(new QLabel(I18n::tr("run_engine") + ": ", tb));
    m_engineBox = new QComboBox(tb);
    m_engineBox->addItem(I18n::tr("run_cpu"));      // Engine::CPU
    m_engineBox->addItem(I18n::tr("run_cpu_mpi"));  // Engine::CPU_MPI
    m_engineBox->addItem(I18n::tr("run_gpu"));      // Engine::GPU
    m_engineBox->addItem(I18n::tr("run_gpu_mpi"));  // Engine::GPU_MPI
    tb->addWidget(m_engineBox);

    m_modeBox = new QComboBox(tb);
    m_modeBox->addItem(I18n::tr("run_both"));        // RunMode::Both
    m_modeBox->addItem(I18n::tr("run_solver_only")); // RunMode::Solver
    m_modeBox->addItem(I18n::tr("run_post_only"));   // RunMode::Post
    tb->addWidget(m_modeBox);

    tb->addWidget(new QLabel(" " + I18n::tr("run_threads") + ": ", tb));
    m_threadsBox = new QSpinBox(tb);
    m_threadsBox->setRange(1, 256);
    m_threadsBox->setValue(QSettings().value("run/threads", 4).toInt());
    tb->addWidget(m_threadsBox);
}

// エンジン選択肢: 光ドメインのみ tidy3d Cloud を追加 (モック準拠)
void MainWindow::updateEngineItems(Domain d)
{
    constexpr int kTidy3dIndex = 4;
    const bool has = m_engineBox->count() > kTidy3dIndex;
    if (d == Domain::Optical && !has)
        m_engineBox->addItem(I18n::tr("run_engine_tidy3d"));
    else if (d != Domain::Optical && has) {
        if (m_engineBox->currentIndex() >= kTidy3dIndex)
            m_engineBox->setCurrentIndex(0);
        m_engineBox->removeItem(kTidy3dIndex);
    }
}

// ── Central widget ──────────────────────────────────────────────────────────
void MainWindow::buildCentral()
{
    auto *central = new QWidget(this);
    auto *v = new QVBoxLayout(central);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    m_domainBar = new DomainBar(central);
    v->addWidget(m_domainBar);
    connect(m_domainBar, &DomainBar::domainSelected,
            m_project, &Project::setActiveDomain);

    auto *split = new QSplitter(Qt::Horizontal, central);
    split->setChildrenCollapsible(false);

    // left: カテゴリナビ + ページスタック
    auto *leftWrap = new QWidget(split);
    auto *lh = new QHBoxLayout(leftWrap);
    lh->setContentsMargins(0, 0, 0, 0);
    lh->setSpacing(0);
    buildLeftNav(leftWrap);
    lh->addWidget(m_nav);
    lh->addWidget(m_pages, 1);

    // center: viewport / plot stack + ev viewer bar
    auto *centerWrap = new QWidget(split);
    auto *cv = new QVBoxLayout(centerWrap);
    cv->setContentsMargins(0, 0, 0, 0);
    cv->setSpacing(2);

    m_centerStack = new QStackedWidget(centerWrap);
    m_viewport = new Viewport3D(m_project, m_centerStack);
    m_plotPanel = new PlotPanel(m_project, m_centerStack);
    m_centerStack->addWidget(m_viewport);
    m_centerStack->addWidget(m_plotPanel);
    cv->addWidget(m_centerStack, 1);

    m_evViewer = new EvViewer(centerWrap);
    cv->addWidget(m_evViewer);

    split->addWidget(leftWrap);
    split->addWidget(centerWrap);
    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 1);
    split->setSizes({ 620, 720 });

    v->addWidget(split, 1);
    setCentralWidget(central);
}

// 左ナビ: 全タブ生成 + カテゴリ/ドメイン/表示レベル登録 (app.jsx LeftDock 準拠)
void MainWindow::buildLeftNav(QWidget *parent)
{
    m_nav = new TabNavigator(parent);
    m_pages = new QStackedWidget(parent);

    auto *P = m_project;
    // ── Setup ──
    m_tabGeometry     = new GeometryTab(P);
    m_tabMaterial     = new MaterialTab(P);
    m_tabSolverRegion = new SolverRegionTab(P);
    m_tabSource       = new SourceTab(P);
    m_tabMonitors     = new MonitorsTab(P);
    m_tabGeneral      = new GeneralTab(P);
    m_tabMesh         = new MeshTab(P);
    m_tabPerFace      = new PerFaceBCTab(P);
    // ── Library ──
    m_tabComponents   = new ComponentsTab(P);
    m_tabMatExplorer  = new MaterialExplorerTab(P);
    m_tabGlass        = new GlassCatalogTab(P);
    m_tabLens         = new LensEditorTab(P);
    m_tabGds          = new LayoutGDSTab(P);
    m_tabSchematic    = new SchematicTab(P);
    m_tabPhotonics    = new PhotonicsSolversTab(P);
    m_tabAcSource     = new AcousticSourceTab(P);
    m_tabOceanEnv     = new OceanEnvironmentTab(P);
    m_tabRoomAc       = new RoomAcousticsTab(P);
    m_tabSoundproof   = new SoundproofTab(P);
    m_tabOutdoor      = new OutdoorNoiseTab(P);
    m_tabCabin        = new CabinAcousticsTab(P);
    m_tabUltrasound   = new UltrasoundTab(P);
    // ── Solve ──
    m_tabFamily       = new FamilySolverTab(P);
    m_tabSolverSel    = new SolverSelectorTab(P);
    m_tabVerification = new VerificationTab(P);
    m_tabOptimize     = new OptimizeTab(P);
    m_tabTolerance    = new ToleranceTab(P);
    m_tabScripts      = new ScriptsTab(P);
    m_tabMultiphysics = new MultiphysicsTab(P);
    m_tabTidy3d       = new Tidy3dTab(P);
    // ── Post ──
    m_tabAnalysisGroups = new AnalysisGroupsTab(P);
    m_tabDatasets     = new DatasetsTab(P);
    m_tabH5Viewer     = new H5ViewerTab(P);
    m_tabInterop      = new InteropTab(P);
    m_tabAntennaChar  = new AntennaCharTab(P);
    m_tabTxLine       = new TransmissionLineTab(P);
    m_tabScattering   = new ScatteringTab(P);
    m_tabCircuit      = new CircuitSolversTab(P);
    // ── ドメイン別 ──
    m_tabOptical      = new OpticalTab(P);
    m_tabAcoustic     = new AcousticTab(P);
    m_tabUnderwater   = new UnderwaterTab(P);
    m_tabPost1        = new Post1Tab(P);
    m_tabPost2        = new Post2Tab(P);

    using D = Domain;
    const QVector<D> ALL;                       // 空 = 全ドメイン
    struct Def {
        const char *key, *cat, *label;
        QWidget *page;
        QVector<D> domains;
        bool core;
    };
    const QVector<Def> defs = {
        // セットアップ (Lumerical canonical order ①〜⑤ + 詳細)
        { "geometry",     "cat_setup", "nav_geometry",     m_tabGeometry,     ALL, true  },
        { "material",     "cat_setup", "nav_material",     m_tabMaterial,     ALL, true  },
        { "solverregion", "cat_setup", "nav_solverregion", m_tabSolverRegion, ALL, true  },
        { "source",       "cat_setup", "nav_source",       m_tabSource,       ALL, true  },
        { "monitors",     "cat_setup", "nav_monitors",     m_tabMonitors,     ALL, true  },
        { "general",      "cat_setup", "nav_general",      m_tabGeneral,      ALL, false },
        { "mesh",         "cat_setup", "nav_mesh",         m_tabMesh,         ALL, false },
        { "perface",      "cat_setup", "nav_perface",      m_tabPerFace,      ALL, false },
        // ライブラリ
        { "components",   "cat_library", "nav_components",   m_tabComponents,  ALL, true },
        { "matexplorer",  "cat_library", "nav_matexplorer",  m_tabMatExplorer,
          { D::EM, D::Optical }, true },
        { "glasscatalog", "cat_library", "nav_glasscatalog", m_tabGlass,
          { D::Optical }, false },
        { "lens",         "cat_library", "nav_lens",         m_tabLens,
          { D::Optical }, false },
        { "layoutgds",    "cat_library", "nav_layoutgds",    m_tabGds,
          { D::Optical }, false },
        { "schematic",    "cat_library", "nav_schematic",    m_tabSchematic,
          { D::Optical }, false },
        { "photonics",    "cat_library", "nav_photonics",    m_tabPhotonics,
          { D::Optical }, true },
        { "acsource",     "cat_library", "nav_acsource",     m_tabAcSource,
          { D::Acoustic, D::Underwater }, true },
        { "oceanenv",     "cat_library", "nav_oceanenv",     m_tabOceanEnv,
          { D::Underwater }, true },
        { "roomac",       "cat_library", "nav_roomac",       m_tabRoomAc,
          { D::Acoustic }, true },
        { "soundproof",   "cat_library", "nav_soundproof",   m_tabSoundproof,
          { D::Acoustic }, true },
        { "outdoor",      "cat_library", "nav_outdoor",      m_tabOutdoor,
          { D::Acoustic }, true },
        { "cabin",        "cat_library", "nav_cabin",        m_tabCabin,
          { D::Acoustic }, true },
        { "ultrasound",   "cat_library", "nav_ultrasound",   m_tabUltrasound,
          { D::Acoustic }, true },
        // 解析
        { "family",       "cat_solve", "nav_family",       m_tabFamily,       ALL, true  },
        { "solver",       "cat_solve", "nav_solver",       m_tabSolverSel,    ALL, false },
        { "verification", "cat_solve", "nav_verification", m_tabVerification, ALL, false },
        { "optimize",     "cat_solve", "nav_optimize",     m_tabOptimize,     ALL, false },
        { "tolerance",    "cat_solve", "nav_tolerance",    m_tabTolerance,    ALL, false },
        { "scripts",      "cat_solve", "nav_scripts",      m_tabScripts,      ALL, false },
        { "multiphysics", "cat_solve", "nav_multiphysics", m_tabMultiphysics, ALL, false },
        { "tidy3d",       "cat_solve", "nav_tidy3d",       m_tabTidy3d,
          { D::Optical }, false },
        // ポスト
        { "analysisgroups", "cat_post", "nav_analysisgroups", m_tabAnalysisGroups,
          ALL, false },
        { "datasets",     "cat_post", "nav_datasets",     m_tabDatasets,     ALL, true  },
        { "h5viewer",     "cat_post", "nav_h5viewer",     m_tabH5Viewer,     ALL, true  },
        { "interop",      "cat_post", "nav_interop",      m_tabInterop,      ALL, true  },
        { "antennachar",  "cat_post", "nav_antennachar",  m_tabAntennaChar,
          { D::EM }, false },
        { "txline",       "cat_post", "nav_txline",       m_tabTxLine,
          { D::EM }, false },
        { "scattering",   "cat_post", "nav_scattering",   m_tabScattering,
          { D::EM }, false },
        { "circuit",      "cat_post", "nav_circuit",      m_tabCircuit,
          { D::EM }, true },
        { "post1",        "cat_post", "nav_post1",        m_tabPost1,        ALL, true  },
        { "post2",        "cat_post", "nav_post2",        m_tabPost2,        ALL, false },
        // ドメイン別カテゴリ
        { "optical",      "cat_dom_optical",    "nav_optical",    m_tabOptical,
          { D::Optical }, true },
        { "acoustic",     "cat_dom_acoustic",   "nav_acoustic",   m_tabAcoustic,
          { D::Acoustic }, true },
        { "underwater",   "cat_dom_underwater", "nav_underwater", m_tabUnderwater,
          { D::Underwater }, true },
    };

    for (const Def &d : defs) {
        m_pages->addWidget(d.page);
        m_nav->addEntry({ d.key, d.cat, d.label, d.page, d.domains, d.core });
    }

    connect(m_nav, &TabNavigator::pageSelected,
            m_pages, &QStackedWidget::setCurrentWidget);
}

// ── Docks ───────────────────────────────────────────────────────────────────
void MainWindow::buildDocks()
{
    m_rightDock = new RightDock(m_project, this);
    auto *dock = new QDockWidget(I18n::tr("rd_project"), this);
    dock->setObjectName("rightDock");
    dock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);
    dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    dock->setWidget(m_rightDock);
    dock->setMinimumWidth(260);
    addDockWidget(Qt::RightDockWidgetArea, dock);
}

// ── Statusbar ───────────────────────────────────────────────────────────────
void MainWindow::buildStatusBar()
{
    auto *sb = statusBar();
    m_sbState = new QLabel("● " + I18n::tr("sb_ready"));
    m_sbCells = new QLabel;
    m_sbMem = new QLabel;
    m_sbDt = new QLabel;
    m_sbStep = new QLabel;
    m_sbProgress = new QProgressBar;
    m_sbProgress->setRange(0, 100);
    m_sbProgress->setFixedWidth(140);
    m_sbProgress->setVisible(false);

    sb->addWidget(m_sbState);
    sb->addPermanentWidget(m_sbCells);
    sb->addPermanentWidget(m_sbMem);
    sb->addPermanentWidget(m_sbDt);
    sb->addPermanentWidget(m_sbStep);
    sb->addPermanentWidget(m_sbProgress);
    sb->addPermanentWidget(new QLabel("Qt " QT_VERSION_STR));
}

// ── Domain switching ────────────────────────────────────────────────────────
void MainWindow::onDomainChanged(Domain d)
{
    m_nav->rebuild(d, m_expert);

    // cloud submission is optical-only
    m_cloudAction->setEnabled(d == Domain::Optical);
    m_cloudAction->setText(d == Domain::Optical
        ? I18n::tr("tb_cloud") : I18n::tr("tb_cloud_optical_only"));
    updateEngineItems(d);

    m_domainBar->setActiveDomain(d);
    m_viewport->setDomain(d);
    m_plotPanel->setDomain(d);

    setStyleSheet(QStringLiteral(
        "QToolButton#primaryAction { background: %1; color: white;"
        " font-weight: 600; border-radius: 3px; padding: 3px 10px; }")
        .arg(accentColor(d)));
}

void MainWindow::setUiLevel(bool expert)
{
    if (m_expert == expert) return;
    m_expert = expert;
    QSettings().setValue("ui/level", expert ? "expert" : "standard");
    (expert ? m_levelExpert : m_levelStandard)->setChecked(true);
    m_nav->rebuild(m_project->activeDomain(), m_expert);
}

void MainWindow::onProjectChanged()
{
    m_sbCells->setText(QStringLiteral("cells: %L1").arg(m_project->totalCells()));
    m_sbMem->setText(QStringLiteral("mem: %1 MB")
        .arg(m_project->estimatedMemoryMB(), 0, 'f', 1));
    const double dt = m_project->courantDt();
    m_sbDt->setText(dt > 0
        ? QStringLiteral("Δt(CFL): %1 s").arg(QString::number(dt, 'g', 3))
        : QStringLiteral("Δt: -"));
}

// ── File actions ────────────────────────────────────────────────────────────
void MainWindow::updateWindowTitle()
{
    const QString file = m_project->filePath().isEmpty()
        ? I18n::tr("untitled")
        : QFileInfo(m_project->filePath()).fileName();
    setWindowTitle(QStringLiteral("OpenFDTD-X — %1").arg(file));
}

void MainWindow::newProject()
{
    m_project->clear();
    emit m_project->loaded();
    emit m_project->changed();
    updateWindowTitle();
}

void MainWindow::openProject(const QString &path)
{
    QString p = path;
    if (p.isEmpty()) {
        p = QFileDialog::getOpenFileName(this, I18n::tr("tb_open"), {},
            "OpenFDTD (*.ofd);;All files (*)");
        if (p.isEmpty()) return;
    }
    QString err;
    if (!m_project->load(p, &err)) {
        QMessageBox::warning(this, I18n::tr("tb_open"), err);
        return;
    }
    m_evViewer->setWorkdir(QFileInfo(p).path());
    updateWindowTitle();
}

void MainWindow::saveProject()
{
    if (m_project->filePath().isEmpty()) {
        saveProjectAs();
        return;
    }
    QString err;
    if (!m_project->save(m_project->filePath(), &err))
        QMessageBox::warning(this, I18n::tr("tb_save"), err);
}

void MainWindow::saveProjectAs()
{
    const QString p = QFileDialog::getSaveFileName(this, I18n::tr("tb_saveas"),
        m_project->general().title.isEmpty() ? "project.ofd"
            : m_project->general().title + ".ofd",
        "OpenFDTD (*.ofd)");
    if (p.isEmpty()) return;
    QString err;
    if (!m_project->save(p, &err)) {
        QMessageBox::warning(this, I18n::tr("tb_save"), err);
        return;
    }
    m_evViewer->setWorkdir(QFileInfo(p).path());
    updateWindowTitle();
}

// ── Dialogs ─────────────────────────────────────────────────────────────────
void MainWindow::showGallery()
{
    if (!m_galleryDialog) {
        m_galleryDialog = new AppGalleryDialog(this);
        connect(m_galleryDialog, &AppGalleryDialog::templatePicked, this,
                [this](const QString &domain, const QString &name) {
            // tidy3d グループは光ドメインのクラウドテンプレート
            if (domain != "tidy3d")
                m_project->setActiveDomain(domainFromKey(domain));
            m_project->general().title = name;
            m_project->touch();
            updateWindowTitle();
        });
    }
    m_galleryDialog->open();
}

void MainWindow::showResources()
{
    if (!m_resourceDialog)
        m_resourceDialog = new ResourceDialog(this);
    m_resourceDialog->open();
}

void MainWindow::showGettingStarted()
{
    if (!m_gettingStarted) {
        m_gettingStarted = new GettingStartedDialog(this);
        connect(m_gettingStarted, &GettingStartedDialog::jumpTo, this,
                [this](const QString &target) {
            if (target == "gallery")   showGallery();
            else if (target == "run")  runSimulation();
            else                       selectLeftTab(target);
        });
    }
    m_gettingStarted->open();
}

void MainWindow::showCloudDialog()
{
    if (m_project->activeDomain() != Domain::Optical) return;
    if (!m_cloudDialog) {
        m_cloudDialog = new CloudDialog(m_project, this);
        connect(m_cloudDialog, &CloudDialog::submitted,
                this, &MainWindow::exportTidy3d);
    }
    m_cloudDialog->open();
}

// ── Run ─────────────────────────────────────────────────────────────────────
RunConfig MainWindow::currentRunConfig() const
{
    RunConfig cfg;
    cfg.engine = Engine(qMin(m_engineBox->currentIndex(), 3));
    cfg.mode = (m_modeBox->currentIndex() == 1) ? RunMode::Solver
             : (m_modeBox->currentIndex() == 2) ? RunMode::Post
                                                : RunMode::Both;
    cfg.threads = m_threadsBox->value();
    QSettings().setValue("run/threads", cfg.threads);

    // 光ドメイン: RCWA/BPM は姉妹カーネル (orcwa / obpm) を使う
    if (m_project->activeDomain() == Domain::Optical) {
        switch (m_project->optical().solver) {
            case OpticalSolver::RCWA: cfg.kernel = Kernel::RCWA; break;
            case OpticalSolver::BPM:  cfg.kernel = Kernel::BPM;  break;
            default:                  cfg.kernel = Kernel::FDTD; break;
        }
    }
    return cfg;
}

void MainWindow::runSimulation()
{
    if (m_runner->isRunning()) {
        m_runner->stop();
        return;
    }
    // エンジンに tidy3d Cloud を選択中はクラウド送信ダイアログへ
    if (m_engineBox->currentIndex() > 3) {
        showCloudDialog();
        return;
    }
    m_plotPanel->clearConvergence();
    m_sbProgress->setVisible(true);
    m_sbProgress->setValue(0);
    m_sbState->setText("● " + I18n::tr("sb_running"));

    if (!m_runDialog)
        m_runDialog = new RunDialog(m_runner, this);
    m_runDialog->clearLog();
    m_runDialog->show();

    m_runner->start(m_project, currentRunConfig());
    m_evViewer->setWorkdir(m_runner->workingDir());
}

void MainWindow::runPostProcess()
{
    if (m_runner->isRunning()) return;
    RunConfig cfg = currentRunConfig();
    cfg.mode = RunMode::Post;
    m_sbState->setText("● " + I18n::tr("sb_running"));
    m_runner->start(m_project, cfg);
    m_evViewer->setWorkdir(m_runner->workingDir());
}

void MainWindow::setDomain(Domain d)
{
    m_project->setActiveDomain(d);
}

void MainWindow::selectLeftTab(const QString &titlePart)
{
    if (m_nav->selectKey(titlePart)) return;
    m_nav->selectByLabel(titlePart);
}

void MainWindow::show2DPlot()
{
    m_centerStack->setCurrentWidget(m_plotPanel);
}

void MainWindow::show3DPlot()
{
    m_centerStack->setCurrentWidget(m_viewport);
}

void MainWindow::exportHdf5()
{
    const QString p = QFileDialog::getSaveFileName(this, I18n::tr("pp_export_h5"),
        (m_project->general().title.isEmpty() ? "project"
            : m_project->general().title) + ".h5",
        "HDF5 (*.h5)");
    if (p.isEmpty()) return;
    QString err;
    // Pass the live convergence history the PlotPanel collected during the run.
    if (!H5Writer::write(p, *m_project, m_plotPanel->steps(),
                         m_plotPanel->eAvg(), m_plotPanel->hAvg(), &err))
        QMessageBox::warning(this, I18n::tr("pp_export_h5"), err);
}

void MainWindow::exportTouchstone()
{
    // The kernel's post step already writes a Touchstone file (test.snp,
    // "# Hz S MA R 50") into the working directory. Copy the most recent
    // *.snp / *.s?p there to the user's chosen path.
    const QString wd = m_runner->workingDir();
    QString src;
    if (!wd.isEmpty()) {
        const QStringList snp = QDir(wd).entryList(
            { "*.snp", "*.s1p", "*.s2p" }, QDir::Files, QDir::Time);
        if (!snp.isEmpty()) src = QDir(wd).filePath(snp.first());
    }
    if (src.isEmpty()) {
        QMessageBox::information(this, I18n::tr("pp_export_s2p"),
            I18n::tr("s2p_run_first"));
        return;
    }
    const QString dst = QFileDialog::getSaveFileName(this, I18n::tr("pp_export_s2p"),
        QFileInfo(src).fileName(), "Touchstone (*.snp *.s2p *.s1p)");
    if (dst.isEmpty()) return;
    QFile::remove(dst);
    if (!QFile::copy(src, dst))
        QMessageBox::warning(this, I18n::tr("pp_export_s2p"),
                             I18n::tr("s2p_copy_failed"));
}

void MainWindow::exportTidy3d()
{
    if (m_project->activeDomain() != Domain::Optical) return;
    const QString p = QFileDialog::getSaveFileName(this, I18n::tr("t3_export"),
        m_project->tidy3d().projectName + ".py", "Python (*.py)");
    if (p.isEmpty()) return;
    QString err;
    if (!Tidy3dExporter::exportTo(p, *m_project, &err))
        QMessageBox::warning(this, I18n::tr("t3_export"), err);
}

// ── Runner feedback ─────────────────────────────────────────────────────────
void MainWindow::onRunnerProgress(int step, int total)
{
    if (total > 0)
        m_sbProgress->setValue(int(100.0 * step / total));
    m_sbStep->setText(QStringLiteral("step: %1 / %2").arg(step).arg(total));
}

void MainWindow::onRunnerLog(const QString &line)
{
    m_rightDock->appendLog(line);
    if (m_runDialog) m_runDialog->appendLine(line);
    // feed convergence points to the plot ("%7d %f %f")
    static const QRegularExpression stepRe(
        "^\\s*(\\d+)\\s+([-+0-9.eE]+)\\s+([-+0-9.eE]+)\\s*$");
    const auto m = stepRe.match(line);
    if (m.hasMatch())
        m_plotPanel->addConvergencePoint(m.captured(1).toInt(),
                                         m.captured(2).toDouble(),
                                         m.captured(3).toDouble());
}

void MainWindow::onRunnerFinished(bool ok)
{
    m_sbProgress->setVisible(false);
    m_sbState->setText("● " + (ok ? I18n::tr("sb_done") : I18n::tr("sb_failed")));
}
