// MainWindow.cpp
#include "MainWindow.h"

#include "widgets/OfdPreviewDialog.h"
#include "DomainBar.h"
#include "RightDock.h"
#include "TabNavigator.h"
#include "CenterPane.h"
#include "Theme.h"
#include "I18n.h"

#include "core/NavCatalog.h"
#include "core/Project.h"
#include "core/ProjectTemplates.h"
#include "io/ActivationCurve.h"
#include "io/BellhopIO.h"
#include "io/ShdReader.h"
#include "io/H5Writer.h"
#include "io/KernelResultReader.h"
#include "io/Tidy3dExporter.h"
#include "io/Touchstone.h"

#include "widgets/EvViewer.h"
#include "widgets/EvCanvas.h"
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
// オペラ音響解析 (PR #1)
#include "tabs/RirAnalysisTab.h"
#include "tabs/VocalAnalysisTab.h"
#include "tabs/AuralizationTab.h"
#include "acoustics/qt/AcousticReportBuilder.h"
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
#include "tabs/AudioEditorTab.h"
#include "tabs/ModeSolverTab.h"
#include "tabs/AcousticSolverTab.h"
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
// 応用画面 (mock 更新分)
#include "tabs/EmcTab.h"
#include "tabs/SarTab.h"
#include "tabs/ChannelTab.h"
#include "tabs/ThinFilmTab.h"
#include "tabs/IlluminationTab.h"
#include "tabs/DisplayOpticsTab.h"
// ダイアログ
#include "dialogs/RunDialog.h"
#include "dialogs/CloudDialog.h"
#include "dialogs/AppGalleryDialog.h"
#include "dialogs/ResourceDialog.h"
#include "dialogs/KernelPathDialog.h"
#include "dialogs/GettingStartedDialog.h"

#include <QActionGroup>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QFileDialog>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QMenuBar>
#include <QDebug>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QStyle>
#include <QTextStream>
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
    // 日本語ラベルのツールバー一式 (新規〜スレッド数) が収まる既定幅。
    // モック同様に実行設定を右寄せしたまま欠けないようにする。
    resize(1680, 940);
    setMinimumSize(1100, 700);

    {
        QSettings st;
        m_expert  = st.value("ui/level", "standard").toString() == "expert";
        m_uiStyle = Theme::styleFromKey(st.value("ui/style", "classic").toString());
        m_uiTheme = Theme::themeFromKey(st.value("ui/theme", "light").toString());
        m_density = Theme::densityFromKey(st.value("ui/density", "normal").toString());
    }

    buildMenu();
    buildToolbar();
    buildCentral();
    buildDocks();
    buildStatusBar();

    connect(m_project, &Project::domainChanged, this, &MainWindow::onDomainChanged);
    connect(m_project, &Project::changed, this, &MainWindow::onProjectChanged);
    // 未保存状態 (*) をタイトルへ即座に反映する
    connect(m_project, &Project::modifiedChanged, this,
            [this] { updateWindowTitle(); });

    // 3D シーンへの結果断面の反映結果をログに出す (ドック生成後に接続)
    connect(m_center, &CenterPane::result3DSliceStatus, this,
            [this](bool ok, const QString &detail) {
        m_rightDock->appendLog(ok ? I18n::tr("log_slice3d")
                                  : I18n::tr("log_slice3d_skip").arg(detail));
    });

    connect(m_runner, &Runner::progress, this, &MainWindow::onRunnerProgress);
    connect(m_runner, &Runner::logLine, this, &MainWindow::onRunnerLog);
    connect(m_runner, &Runner::finished, this, &MainWindow::onRunnerFinished);

    onDomainChanged(m_project->activeDomain());
    onProjectChanged();
    updateWindowTitle();
}

MainWindow::~MainWindow() = default;

// ── Menus ───────────────────────────────────────────────────────────────────
// 保存内容のプレビュー。自動実行 (--screenshot) 中はモーダルにすると先へ
// 進めないので出しっぱなしにし、撮影対象として呼び出し側へ返す。
QWidget *MainWindow::showOfdPreview()
{
    auto *dlg = new OfdPreviewDialog(m_project, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    if (automation()) { dlg->show(); return dlg; }
    dlg->exec();
    return nullptr;
}

void MainWindow::buildMenu()
{
    auto *mb = menuBar();
    auto *mFile = mb->addMenu(I18n::tr("m_file"));
    auto *mEdit = mb->addMenu(I18n::tr("m_edit"));
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
    // 保存前に .ofd / .ofdx の中身を見る (読み取り専用)
    mFile->addAction(I18n::tr("m_ofd_preview"), this, [this] {
        OfdPreviewDialog(m_project, this).exec();
    });
    mFile->addSeparator();
    // オペラ音響の一括レポート。ドメインに関係なく置くが、分析が未実行の
    // ときは書き出さずに理由を伝える。
    mFile->addAction(I18n::tr("m_acoustic_report"), this,
                     &MainWindow::exportAcousticReport);
    mFile->addSeparator();
    mFile->addAction(I18n::tr("m_exit"), this, [] { qApp->quit(); });

    // 編集メニュー — モックのメニューバーに合わせる。表の行編集は各タブが
    // 持つため、ここはドメイン横断で意味のある操作だけを置く。
    mEdit->addAction(I18n::tr("me_undo"), QKeySequence::Undo, this, [this] {
        statusBar()->showMessage(I18n::tr("me_undo_na"), 3000);
    });
    mEdit->addAction(I18n::tr("me_redo"), QKeySequence::Redo, this, [this] {
        statusBar()->showMessage(I18n::tr("me_undo_na"), 3000);
    });
    mEdit->addSeparator();
    mEdit->addAction(I18n::tr("me_select_tab"), QKeySequence("Ctrl+L"), this, [this] {
        m_nav->setFocus();
    });
    mEdit->addAction(I18n::tr("m_uilevel"), this, [this] {
        setUiLevel(!m_expert);
    });

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

    // テーマ / 密度 / UIスタイル (モックの TweaksPanel 相当)
    struct Opt { const char *labelKey; const char *value; };
    auto addRadioMenu = [this, mView](const char *titleKey, const char *setting,
                                      const QVector<Opt> &opts, const char *def) {
        auto *menu = mView->addMenu(I18n::tr(titleKey));
        auto *grp = new QActionGroup(this);
        const QString cur = QSettings().value(setting, def).toString();
        for (const Opt &o : opts) {
            auto *a = menu->addAction(I18n::tr(o.labelKey));
            a->setCheckable(true);
            a->setChecked(cur == QString::fromLatin1(o.value));
            grp->addAction(a);
            const QString key = setting, val = QString::fromLatin1(o.value);
            connect(a, &QAction::triggered, this, [this, key, val] {
                QSettings().setValue(key, val);
                if (key == "ui/theme")        m_uiTheme = Theme::themeFromKey(val);
                else if (key == "ui/density") m_density = Theme::densityFromKey(val);
                else                          m_uiStyle = Theme::styleFromKey(val);
                applyTheme();
            });
        }
    };
    addRadioMenu("m_theme", "ui/theme",
        { { "theme_light", "light" }, { "theme_dark", "dark" } }, "light");
    addRadioMenu("m_density", "ui/density",
        { { "density_compact", "compact" }, { "density_normal", "normal" },
          { "density_comfortable", "comfortable" } }, "normal");
    addRadioMenu("m_uistyle", "ui/style",
        { { "uistyle_classic", "classic" }, { "uistyle_modern", "modern" },
          { "uistyle_scientific", "scientific" } }, "classic");

    auto *langMenu = mView->addMenu(I18n::tr("m_lang"));
    for (const auto &[code, label] : std::initializer_list<
             std::pair<const char *, const char *>>{
             { "ja", "日本語" }, { "en", "English" }, { "both", "日英 / JA+EN" } }) {
        langMenu->addAction(QString::fromUtf8(label), this, [this, code = QString(code)] {
            QSettings().setValue("ui/language", code);
            QMessageBox::information(this, "OpenFDTD-X", I18n::tr("m_lang_restart"));
        });
    }

    m_runMenuAction = mRun->addAction(I18n::tr("tb_calc"),
                                      QKeySequence(Qt::Key_F5),
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
    m_s2pMenuAction =
        mPost->addAction(I18n::tr("pp_export_s2p"), this,
                         &MainWindow::exportTouchstone);

    // クラウド送信は光ドメイン専用 (showCloudDialog が非光では何もしないため
    // ツールバー側と同様にドメインで有効/無効を切り替える)
    m_cloudMenuAction =
        mTools->addAction(I18n::tr("tb_cloud"), this,
                          &MainWindow::showCloudDialog);
    m_resourceActions << mTools->addAction(I18n::tr("tb_resources"), this,
                                           &MainWindow::showResources);
    // カーネルの場所を GUI から設定 (Finder / Dock 起動では環境変数が
    // 届かないため。QSettings に永続化 — kernel/Runner が探索時に参照)
    mTools->addAction(I18n::tr("m_kernel_paths"), this, [this] {
        KernelPathDialog dlg(this, m_project);
        dlg.exec();
        updateKernelWarn();
    });
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

    m_runAction = tb->addAction(icon(QStyle::SP_MediaPlay), I18n::tr("tb_calc"),
                                this, &MainWindow::runSimulation);
    m_runAction->setToolTip(I18n::tr("tb_calc"));
    if (auto *btn = qobject_cast<QToolButton *>(tb->widgetForAction(m_runAction)))
        btn->setObjectName("primaryAction");
    tb->addAction(icon(QStyle::SP_MediaSeekForward), I18n::tr("tb_post"),
                  this, &MainWindow::runPostProcess);
    // 2D = 結果プロット (波形/収束/周波数特性/放射パターン)、3D = 3D シーン。
    // 何が出るのかをツールチップで明示する (「2D 側が何か分からない」対策)
    QAction *plot2d = tb->addAction(icon(QStyle::SP_FileDialogContentsView),
                                    I18n::tr("tb_plot2d"),
                                    this, &MainWindow::show2DPlot);
    plot2d->setToolTip(I18n::tr("tb_plot2d_tip"));
    QAction *plot3d = tb->addAction(icon(QStyle::SP_FileDialogListView),
                                    I18n::tr("tb_plot3d"),
                                    this, &MainWindow::show3DPlot);
    plot3d->setToolTip(I18n::tr("tb_plot3d_tip"));
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
    m_s2pExportAction =
        exportMenu->addAction(I18n::tr("pp_export_s2p"), this,
                              &MainWindow::exportTouchstone);
    m_t3ExportAction =
        exportMenu->addAction(I18n::tr("t3_export"), this,
                              &MainWindow::exportTidy3d);
    exportBtn->setMenu(exportMenu);
    tb->addWidget(exportBtn);

    tb->addSeparator();
    m_resourceActions << tb->addAction(I18n::tr("tb_resources"), this,
                                       &MainWindow::showResources);
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
    // QSS の padding でコンボが太るとツールバー右端が溢れてスレッド数が
    // 画面外に出るため、上限幅を与えておく
    m_engineBox->setMaximumWidth(150);
    tb->addWidget(m_engineBox);

    m_modeBox = new QComboBox(tb);
    m_modeBox->addItem(I18n::tr("run_both"));        // RunMode::Both
    m_modeBox->addItem(I18n::tr("run_solver_only")); // RunMode::Solver
    m_modeBox->addItem(I18n::tr("run_post_only"));   // RunMode::Post
    m_modeBox->setMaximumWidth(150);
    tb->addWidget(m_modeBox);

    tb->addWidget(new QLabel(" " + I18n::tr("run_threads") + ": ", tb));
    m_threadsBox = new QSpinBox(tb);
    m_threadsBox->setRange(1, 256);
    m_threadsBox->setValue(QSettings().value("run/threads", 4).toInt());
    m_threadsBox->setMaximumWidth(70);
    tb->addWidget(m_threadsBox);

    // GPU デバイス番号 — GPU 系エンジンのときだけ意味を持つので連動表示する
    m_deviceLabel = new QLabel(" " + I18n::tr("run_device") + ": ", tb);
    m_deviceBox = new QSpinBox(tb);
    m_deviceBox->setRange(0, 15);
    m_deviceBox->setValue(QSettings().value("run/device", 0).toInt());
    m_deviceBox->setMaximumWidth(60);
    m_deviceAction = tb->addWidget(m_deviceLabel);
    m_deviceBoxAction = tb->addWidget(m_deviceBox);
    const auto syncDevice = [this] {
        const bool gpu = (m_engineBox->currentIndex() == int(Engine::GPU)
                       || m_engineBox->currentIndex() == int(Engine::GPU_MPI));
        m_deviceAction->setVisible(gpu);
        m_deviceBoxAction->setVisible(gpu);
    };
    connect(m_engineBox, &QComboBox::currentIndexChanged, this,
            [this, syncDevice](int) { syncDevice(); syncTabRunConfig(); });
    syncDevice();
    // 自前で Runner を回すタブ (散乱の入射角スイープ) へ実行設定を配る。
    // ツールバーの値が唯一の正 — タブ側に別の設定を持たせない。
    connect(m_threadsBox, &QSpinBox::valueChanged, this,
            [this](int) { syncTabRunConfig(); });
    connect(m_deviceBox, &QSpinBox::valueChanged, this,
            [this](int) { syncTabRunConfig(); });
}

// ツールバーの実行設定を、自前で Runner を回すタブへ配る。
// タブ生成前・ツールバー生成前のどちらから呼ばれても安全 (null ガード)。
void MainWindow::syncTabRunConfig()
{
    if (!m_engineBox || !m_threadsBox || !m_deviceBox) return;
    const RunConfig cfg = currentRunConfig();
    if (auto *sct = qobject_cast<ScatteringTab *>(m_tabScattering))
        sct->setRunConfig(cfg);
    if (auto *ver = qobject_cast<VerificationTab *>(m_tabVerification))
        ver->setRunConfig(cfg);
    if (auto *tol = qobject_cast<ToleranceTab *>(m_tabTolerance))
        tol->setRunConfig(cfg);
}

// エンジン選択肢: 光ドメインのみ tidy3d Cloud を追加 (モック準拠)。
// 室内音響 (AcousticRunner) は MPI/GPU エンジンを持たないため CPU のみ有効化する。
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

    if (auto *model = qobject_cast<QStandardItemModel *>(m_engineBox->model())) {
        const bool cpuOnly = (d == Domain::Acoustic);
        // 実機の可用性を見る — mpiexec も _mpi バイナリも無いのに
        // 「CPU+MPI」を選べると、実行して初めて失敗する (絶対規則 5)。
        const Runner::Availability av =
            Runner::checkAvailability(Runner::kernelForProject(*m_project));
        for (int i = int(Engine::CPU_MPI); i <= int(Engine::GPU_MPI)
                 && i < model->rowCount(); ++i) {
            auto *item = model->item(i);
            if (!item) continue;
            const bool needsMpi = (i == int(Engine::CPU_MPI)
                                   || i == int(Engine::GPU_MPI));
            const bool needsCuda = (i == int(Engine::GPU)
                                    || i == int(Engine::GPU_MPI));
            bool ok = !cpuOnly;
            QString why;
            if (cpuOnly) {
                why = I18n::tr("run_engine_cpu_only");
            } else {
                // 仕様上できない組合せ (orcwa の MPI/CUDA 版は FDTD 専用で
                // RCWA を計算できない、obpm の MPI 版は BPM を計算しない) を
                // バイナリの有無より先に見る — 「入れれば使える」と誤解させない
                why = Runner::engineUnsupportedReason(*m_project, Engine(i));
                if (!why.isEmpty()) ok = false;
                if (ok && needsMpi && !av.mpi) { ok = false; why = av.mpiReason; }
                if (ok && needsCuda && !av.cuda) { ok = false; why = av.cudaReason; }
            }
            item->setEnabled(ok);
            item->setToolTip(ok ? QString()
                                : I18n::tr("run_engine_na").arg(why));
        }
        // 現在の選択が使えなくなったら CPU へ落とす (使えない設定で走らせない)
        const int cur = m_engineBox->currentIndex();
        if (cur < kTidy3dIndex && cur > int(Engine::CPU)) {
            auto *item = model->item(cur);
            if (item && !item->isEnabled())
                m_engineBox->setCurrentIndex(int(Engine::CPU));
        }
    }
}

// 実行中の実行設定ロック。
// RunConfig は Runner::start の直前に 1 度だけ組み立てられ、以降は
// m_cfg として固定される。したがって実行中にエンジンやスレッド数を
// 変えても **走っているジョブには一切効かない** (次回の実行にだけ効く)。
// 触れる UI を残すと「反映されている」と誤解させるので、実行中は無効化し
// 理由をツールチップで出す (絶対規則 5)。
void MainWindow::setRunUiEnabled(bool enabled)
{
    const QString why = enabled ? QString() : I18n::tr("run_locked_tip");
    QWidget *const widgets[] = { m_engineBox, m_modeBox, m_threadsBox,
                                 m_deviceBox };
    for (QWidget *w : widgets) {
        if (!w) continue;
        w->setEnabled(enabled);
        w->setToolTip(why);
    }
    for (QAction *a : m_resourceActions) {
        if (!a) continue;
        a->setEnabled(enabled);
        a->setToolTip(why);
    }
    // ダイアログを開いたまま実行に入った場合も閉じる (開いていれば
    // 適用ボタンから設定を変えられてしまうため)
    if (!enabled && m_resourceDialog && m_resourceDialog->isVisible())
        m_resourceDialog->reject();

    // 「計算」ボタンは実行中は中止ボタンになる。同じ見た目のまま挙動だけ
    // 変わると「2 回押すとキャンセルになるのが分からない」ので、
    // ラベル・アイコン・ツールチップを実行中の意味に差し替える。
    const QString label = enabled ? I18n::tr("tb_calc")
                                  : I18n::tr("tb_calc_stop");
    const QString tip = enabled ? I18n::tr("tb_calc")
                                : I18n::tr("tb_calc_stop_tip");
    const QIcon ic = style()->standardIcon(
        enabled ? QStyle::SP_MediaPlay : QStyle::SP_MediaStop);
    for (QAction *a : { m_runAction, m_runMenuAction }) {
        if (!a) continue;
        a->setText(label);
        a->setToolTip(tip);
        if (a == m_runAction) a->setIcon(ic);
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
    // ナビ列 = カテゴリナビ + 表示モード切替。
    // 標準表示ではエキスパート専用タブ (全般/メッシュ詳細/検証 …) が隠れる。
    // 切替がメニューの奥だけだと「項目が少ない」と誤解されるため、
    // 隠れている項目数と一緒にナビ直下へ常時出す。
    auto *navCol = new QWidget(leftWrap);
    auto *navV = new QVBoxLayout(navCol);
    navV->setContentsMargins(0, 0, 0, 0);
    navV->setSpacing(0);
    navV->addWidget(m_nav, 1);
    m_levelCheck = new QCheckBox(I18n::tr("nav_expert"), navCol);
    m_levelCheck->setChecked(m_expert);   // QSettings から復元した値に合わせる
    m_levelCheck->setToolTip(I18n::tr("nav_expert_tip"));
    m_levelCheck->setContentsMargins(6, 3, 6, 4);
    connect(m_levelCheck, &QCheckBox::toggled, this, &MainWindow::setUiLevel);
    navV->addWidget(m_levelCheck);
    lh->addWidget(navCol);
    lh->addWidget(m_pages, 1);
    // ナビ + ページの実用最小幅 (これを下回るとタブ内の表が読めなくなる)
    leftWrap->setMinimumWidth(430);

    // center: ビュータブ + ツールバー + ページスタック (CenterPane) + ev viewer bar
    auto *centerWrap = new QWidget(split);
    auto *cv = new QVBoxLayout(centerWrap);
    cv->setContentsMargins(0, 0, 0, 0);
    cv->setSpacing(2);

    m_center = new CenterPane(m_project, centerWrap);
    m_viewport = m_center->viewport();
    m_plotPanel = m_center->plotPanel();
    cv->addWidget(m_center, 1);

    m_evViewer = new EvViewer(centerWrap);
    cv->addWidget(m_evViewer);
    // 「アプリ内に描画」で ev2d を開く → 中央の「カーネル作図」タブへ
    connect(m_evViewer, &EvViewer::showNativeRequested, this,
            [this](const QString &path) {
        if (auto *ev = m_center->evCanvas()) {
            QString err;
            if (!ev->load(path, &err)) {
                m_rightDock->appendLog(err);
                return;
            }
            m_center->selectTabContaining(I18n::tr("vp_ev"));
        }
    });

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
    m_tabAudioEdit    = new AudioEditorTab(P);
    m_tabModeSolver   = new ModeSolverTab(P);
    m_tabAcSolver     = new AcousticSolverTab(P);
    m_tabOceanEnv     = new OceanEnvironmentTab(P);
    m_tabRoomAc       = new RoomAcousticsTab(P);
    m_tabRirAnalysis  = new RirAnalysisTab(P);
    m_tabVocal        = new VocalAnalysisTab(P);
    m_tabAuralization = new AuralizationTab(P);
    m_tabSoundproof   = new SoundproofTab(P);
    m_tabOutdoor      = new OutdoorNoiseTab(P);
    m_tabCabin        = new CabinAcousticsTab(P);
    m_tabUltrasound   = new UltrasoundTab(P);
    // ── Solve ──
    m_tabFamily       = new FamilySolverTab(P);
    m_tabSolverSel    = new SolverSelectorTab(P);
    m_tabVerification = new VerificationTab(P);
    // 自動収束テストも自前で Runner を回す — 進捗を計算コンソールへ
    if (auto *ver = qobject_cast<VerificationTab *>(m_tabVerification))
        connect(ver, &VerificationTab::sweepLog, this,
                [this](const QString &line) { m_rightDock->appendLog(line); });
    m_tabOptimize     = new OptimizeTab(P);
    m_tabTolerance    = new ToleranceTab(P);
    // モンテカルロも自前で Runner を回す — 進捗を計算コンソールへ
    if (auto *tol = qobject_cast<ToleranceTab *>(m_tabTolerance))
        connect(tol, &ToleranceTab::sweepLog, this,
                [this](const QString &line) { m_rightDock->appendLog(line); });
    m_tabScripts      = new ScriptsTab(P);
    m_tabMultiphysics = new MultiphysicsTab(P);
    m_tabTidy3d       = new Tidy3dTab(P);
    // ── Post ──
    m_tabAnalysisGroups = new AnalysisGroupsTab(P);
    m_tabDatasets     = new DatasetsTab(P);
    m_tabH5Viewer     = new H5ViewerTab(P);
    // H5アニメの現在フレーム → 3D シーン (タブ ↔ CenterPane の直接依存を
    // 作らないよう MainWindow が中継する。RoomAcousticsTab と同じ流儀)
    if (auto *h5v = qobject_cast<H5ViewerTab *>(m_tabH5Viewer)) {
        connect(h5v, &H5ViewerTab::sceneSlicesReady, this,
                [this](const QVector<H5SliceForScene> &sl) {
                    m_center->showAnimationSlice(sl);
                });
        connect(h5v, &H5ViewerTab::sceneSliceCleared, this,
                [this] { m_center->clearAnimationSlice(); });
    }
    m_tabInterop      = new InteropTab(P);
    m_tabAntennaChar  = new AntennaCharTab(P);
    m_tabTxLine       = new TransmissionLineTab(P);
    m_tabScattering   = new ScatteringTab(P);
    // 入射角スイープは自前で Runner を N 回まわす。実行設定 (エンジン /
    // スレッド数) はツールバー側が正なので開くたびに渡し、進捗ログは
    // 通常実行と同じ計算コンソールへ出す。
    if (auto *sct = qobject_cast<ScatteringTab *>(m_tabScattering))
        connect(sct, &ScatteringTab::sweepLog, this,
                [this](const QString &line) { m_rightDock->appendLog(line); });
    m_tabCircuit      = new CircuitSolversTab(P);
    m_tabEmc          = new EmcTab(P);
    m_tabSar          = new SarTab(P);
    m_tabChannel      = new ChannelTab(P);
    m_tabThinFilm     = new ThinFilmTab(P);
    m_tabIllum        = new IlluminationTab(P);
    m_tabDisplayOpt   = new DisplayOpticsTab(P);
    // ── ドメイン別 ──
    m_tabOptical      = new OpticalTab(P);
    m_tabAcoustic     = new AcousticTab(P);
    m_tabUnderwater   = new UnderwaterTab(P);
    m_tabPost1        = new Post1Tab(P);
    m_tabPost2        = new Post2Tab(P);

    // 音響ソルバ連携が契約検証済み RIR を設定したら、実測RIR分析タブの
    // WAV 欄へ反映する。RirAnalysisTab は Project::loaded にしか繋いで
    // いない (他タブの touch() で編集中の入力欄を上書きしないため) ので、
    // 「実行完了」という単発イベントだけをここで橋渡しする — タブ同士が
    // 直接依存しないよう、両方を持つ MainWindow が中継役になる。
    if (auto *acSolver = qobject_cast<AcousticSolverTab *>(m_tabAcSolver)) {
        if (auto *rirTab = qobject_cast<RirAnalysisTab *>(m_tabRirAnalysis)) {
            connect(acSolver, &AcousticSolverTab::rirAssigned,
                    rirTab, &RirAnalysisTab::applySolverRir);
        }
    }
    // 水中音響で合成した受信インパルス応答 (WAV) を「音響編集・解析」へ渡す。
    // 合成 → そのまま畳み込み (リバーブ) へ、が 1 手で繋がるようにする。
    if (auto *uwTab = qobject_cast<UnderwaterTab *>(m_tabUnderwater)) {
        if (auto *aeTab = qobject_cast<AudioEditorTab *>(m_tabAudioEdit)) {
            connect(uwTab, &UnderwaterTab::receivedIrExported, this,
                    [this, aeTab](const QString &wav) {
                        aeTab->openWavFile(wav);
                        m_nav->selectKey(QStringLiteral("audioeditor"));
                    });
        }
    }
    // 音響解析タブの「進め方」パネルの行クリック → 左ナビの該当タブへ移動。
    // AcousticTab はナビのキーを投げるだけで、切替はナビを持つ MainWindow が
    // 行う (タブ同士が直接依存しない)。標準表示で隠れているタブは selectKey が
    // 見つけられないので、その場合は何もしない (行の「対応タブ」列に名前が
    // 出ているので利用者は自分で辿れる)。
    if (auto *acTab = qobject_cast<AcousticTab *>(m_tabAcoustic)) {
        connect(acTab, &AcousticTab::navigateRequested, this,
                [this](const QString &key) { m_nav->selectKey(key); });
    }
    // 「④ 音源 (励振)」タブの「🎤 音源/WAV/指向性 タブへ」→ 音源リストへ移動。
    // 音源設定が 2 系統 (ソルバ励振 = feed / 音源リスト = .ofdx) ある混乱への
    // 対応で、逆向き (音源リスト → feed 反映) は AcousticSourceTab が持つ。
    if (auto *srcTab = qobject_cast<SourceTab *>(m_tabSource)) {
        connect(srcTab, &SourceTab::navigateRequested, this,
                [this](const QString &key) { m_nav->selectKey(key); });
    }
    // ホール解析の「▶ 音響ソルバ連携で計算する」→ 音響ソルバ連携タブへ移動。
    // 実行まで自動で走らせないのは、外部ソルバーのバイナリ解決が未確定だと
    // 実行できないため (あちらで解決結果を確認してから ▶ 実行 を押す)。
    if (auto *roomTab = qobject_cast<RoomAcousticsTab *>(m_tabRoomAc)) {
        connect(roomTab, &RoomAcousticsTab::runSolverRequested, this, [this] {
            if (m_nav->selectKey(QStringLiteral("acsolver")))
                statusBar()->showMessage(I18n::tr("mw_goto_acsolver"), 8000);
        });
    }

    using D = Domain;
    const QVector<D> ALL;                       // 空 = 全ドメイン
    // カテゴリ (見出し) は core/NavCatalog.h の対応表から引く — ここに直書き
    // すると検証 (selftest) と実装の二重管理になるため。並び順はこの defs が
    // 決めるので、NavCatalog.h の表と同じ順に並べること
    // (同じカテゴリの項目が離れると見出しが 2 回出る)。
    struct Def {
        const char *key, *label;
        QWidget *page;
        QVector<D> domains;
        bool core;
        // 音響/水中ドメインでの代替ラベルキー (nullptr = 共通ラベルのまま)
        const char *acLabel = nullptr;
    };
    const QVector<Def> defs = {
        // セットアップ (Lumerical canonical order ①〜⑤ + 詳細)
        { "geometry",     "nav_geometry",     m_tabGeometry,     ALL, true  },
        { "material",     "nav_material",     m_tabMaterial,     ALL, true  },
        { "solverregion", "nav_solverregion", m_tabSolverRegion, ALL, true  },
        // 音響/水中では「波源」ではなく「音源」(SourceTab 内の文言と揃える)
        { "source",       "nav_source",       m_tabSource,       ALL, true,
          "nav_source_ac" },
        { "monitors",     "nav_monitors",     m_tabMonitors,     ALL, true  },
        { "general",      "nav_general",      m_tabGeneral,      ALL, false },
        { "mesh",         "nav_mesh",         m_tabMesh,         ALL, false },
        // 面別 BC (PML/PEC/PMC/ブロッホ/接地面) は EM/光 FDTD 専用の概念 —
        // 音響の境界は吸音率 (RoomAcousticsTab)、水中は海面/海底 (OceanEnvTab) が担う
        { "perface",      "nav_perface",      m_tabPerFace,
          { D::EM, D::Optical }, false },
        // ライブラリ — 「そこから選んで使う」部品・素材のカタログだけを置く
        { "components",   "nav_components",   m_tabComponents,  ALL, true },
        { "matexplorer",  "nav_matexplorer",  m_tabMatExplorer,
          { D::EM, D::Optical }, true },
        { "glasscatalog", "nav_glasscatalog", m_tabGlass,
          { D::Optical }, false },
        { "lens",         "nav_lens",         m_tabLens,
          { D::Optical }, false },
        { "layoutgds",    "nav_layoutgds",    m_tabGds,
          { D::Optical }, false },
        { "schematic",    "nav_schematic",    m_tabSchematic,
          { D::Optical }, false },
        // 応用 — ドメイン固有の応用解析タブ。カタログ (ライブラリ) と用途別の
        // ワークフローが混ざっていると探しにくいので分けてある。
        // TabNavigator::rebuild は「カテゴリキーが変わったところ」で見出しを
        // 出すので、同じカテゴリの項目は必ず連続させること。
        { "photonics",    "nav_photonics",    m_tabPhotonics,
          { D::Optical }, true },
        // モードソルバ FDE (pic-tools.jsx — mock の並びどおり photonics の直後)
        { "modesolver",   "nav_modesolver",   m_tabModeSolver,
          { D::Optical }, true },
        { "thinfilm",     "nav_thinfilm",     m_tabThinFilm,
          { D::Optical }, true },
        { "illum",        "nav_illum",        m_tabIllum,
          { D::Optical }, true },
        { "displayopt",   "nav_displayopt",   m_tabDisplayOpt,
          { D::Optical }, true },
        { "acsource",     "nav_acsource",     m_tabAcSource,
          { D::Acoustic, D::Underwater }, true },
        // 音響編集・解析 (audio-editor.jsx — mock の並びどおり acsource の直後)
        { "audioedit",    "nav_audioedit",    m_tabAudioEdit,
          { D::Acoustic, D::Underwater }, true },
        { "oceanenv",     "nav_oceanenv",     m_tabOceanEnv,
          { D::Underwater }, true },
        { "roomac",       "nav_roomac",       m_tabRoomAc,
          { D::Acoustic }, true },
        // 音響ソルバ連携 (opera-analysis.jsx AcousticSolverTab)
        { "acsolver",     "nav_acsolver",     m_tabAcSolver,
          { D::Acoustic }, true },
        { "soundproof",   "nav_soundproof",   m_tabSoundproof,
          { D::Acoustic }, true },
        { "outdoor",      "nav_outdoor",      m_tabOutdoor,
          { D::Acoustic }, true },
        { "cabin",        "nav_cabin",        m_tabCabin,
          { D::Acoustic }, true },
        { "ultrasound",   "nav_ultrasound",   m_tabUltrasound,
          { D::Acoustic }, true },
        // 解析
        { "family",       "nav_family",       m_tabFamily,       ALL, true  },
        { "solver",       "nav_solver",       m_tabSolverSel,    ALL, false },
        { "verification", "nav_verification", m_tabVerification, ALL, false },
        { "optimize",     "nav_optimize",     m_tabOptimize,     ALL, false },
        { "tolerance",    "nav_tolerance",    m_tabTolerance,    ALL, false },
        { "scripts",      "nav_scripts",      m_tabScripts,      ALL, false },
        { "multiphysics", "nav_multiphysics", m_tabMultiphysics, ALL, false },
        { "tidy3d",       "nav_tidy3d",       m_tabTidy3d,
          { D::Optical }, false },
        // ポスト
        { "analysisgroups", "nav_analysisgroups", m_tabAnalysisGroups,
          ALL, false },
        { "datasets",     "nav_datasets",     m_tabDatasets,     ALL, true  },
        { "h5viewer",     "nav_h5viewer",     m_tabH5Viewer,     ALL, true  },
        { "interop",      "nav_interop",      m_tabInterop,      ALL, true  },
        { "antennachar",  "nav_antennachar",  m_tabAntennaChar,
          { D::EM }, false },
        { "txline",       "nav_txline",       m_tabTxLine,
          { D::EM }, false },
        { "scattering",   "nav_scattering",   m_tabScattering,
          { D::EM }, false },
        { "circuit",      "nav_circuit",      m_tabCircuit,
          { D::EM }, true },
        { "emc",          "nav_emc",          m_tabEmc,
          { D::EM }, true },
        { "sar",          "nav_sar",          m_tabSar,
          { D::EM }, true },
        { "channel",      "nav_channel",      m_tabChannel,
          { D::EM }, true },
        { "post1",        "nav_post1",        m_tabPost1,        ALL, true  },
        { "post2",        "nav_post2",        m_tabPost2,        ALL, false },
        // ドメイン別カテゴリ
        { "optical",      "nav_optical",    m_tabOptical,
          { D::Optical }, true },
        { "acoustic",     "nav_acoustic",   m_tabAcoustic,
          { D::Acoustic }, true },
        // オペラ音響解析 (PR #1) — 実測RIR / 歌声 / 可聴化
        { "riranalysis",  "t_riranalysis",  m_tabRirAnalysis,
          { D::Acoustic }, true },
        { "vocalanalysis","t_vocalanalysis", m_tabVocal,
          { D::Acoustic }, true },
        { "auralization", "t_auralization", m_tabAuralization,
          { D::Acoustic }, true },
        { "underwater",   "nav_underwater", m_tabUnderwater,
          { D::Underwater }, true },
    };

    QString lastCat;
    for (const Def &d : defs) {
        // 登録漏れは「タブが消える」より「見出しがずれる」方が害が小さいので、
        // 直前のカテゴリに寄せたうえで警告を出す (開発時に気付ける)。
        const char *cat = navcat::categoryFor(d.key);
        if (!cat) {
            qWarning("nav: '%s' is missing from core/NavCatalog.h", d.key);
        } else {
            lastCat = QString::fromLatin1(cat);
        }
        m_pages->addWidget(d.page);
        m_nav->addEntry({ d.key, lastCat, d.label, d.page, d.domains, d.core,
                          d.acLabel ? QString::fromLatin1(d.acLabel)
                                    : QString() });
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
    m_sbCfl = new QLabel;
    m_sbStep = new QLabel;
    m_sbProgress = new QProgressBar;
    m_sbProgress->setRange(0, 100);
    m_sbProgress->setFixedWidth(140);
    m_sbProgress->setVisible(false);

    // 現ドメインのカーネル未検出警告。OpenFDTD (EM) は基幹カーネルなので
    // 未導入なら起動直後から見える。クリックでカーネルパス設定を開く。
    m_sbKernelWarn = new QToolButton(this);
    m_sbKernelWarn->setAutoRaise(true);
    m_sbKernelWarn->setToolTip(I18n::tr("sb_kernel_missing_tip"));
    m_sbKernelWarn->setStyleSheet("QToolButton { color: #B8860B; }");
    m_sbKernelWarn->setVisible(false);
    connect(m_sbKernelWarn, &QToolButton::clicked, this, [this] {
        KernelPathDialog dlg(this, m_project);
        dlg.exec();
        updateKernelWarn();
    });

    sb->addWidget(m_sbState);
    sb->addWidget(m_sbKernelWarn);
    sb->addPermanentWidget(m_sbCells);
    sb->addPermanentWidget(m_sbMem);
    sb->addPermanentWidget(m_sbDt);
    sb->addPermanentWidget(m_sbCfl);
    sb->addPermanentWidget(m_sbStep);
    sb->addPermanentWidget(m_sbProgress);
    sb->addPermanentWidget(new QLabel("Qt " QT_VERSION_STR));
}

// 現ドメインが起動するカーネルが実在するかを確認し、無ければ警告を出す。
// 表示は「実際に解決できなかった」事実のみ (存在しない状態を隠さない)。
void MainWindow::updateKernelWarn()
{
    if (!m_sbKernelWarn) return;
    RunConfig cfg;
    cfg.kernel = Runner::kernelForProject(*m_project);
    const bool missing = Runner::resolvedSolverPath(cfg).isEmpty();
    if (missing)
        m_sbKernelWarn->setText(I18n::tr("sb_kernel_missing")
                                    .arg(Runner::solverBinary(cfg)));
    m_sbKernelWarn->setVisible(missing);
}

// ── Domain switching ────────────────────────────────────────────────────────
void MainWindow::onDomainChanged(Domain d)
{
    // ドメイン/表示レベルに応じてナビ項目を組み直す
    // (旧実装の removeTab/addTab は TabNavigator::rebuild が担う)
    m_nav->rebuild(d, m_expert);
    updateLevelHint();    // 隠れている項目数はドメインで変わる
    updateKernelWarn();   // ドメインが変われば必要なカーネルも変わる

    // cloud submission is optical-only
    m_cloudAction->setEnabled(d == Domain::Optical);
    if (m_cloudMenuAction) m_cloudMenuAction->setEnabled(d == Domain::Optical);
    m_cloudAction->setText(d == Domain::Optical
        ? I18n::tr("tb_cloud") : I18n::tr("tb_cloud_optical_only"));
    updateEngineItems(d);

    // ドメインで意味を持たないエクスポートの無効化:
    //   S2P (S パラメータ) = 給電点を持つ EM のみ / tidy3d py = 光のみ
    if (m_s2pMenuAction)   m_s2pMenuAction->setEnabled(d == Domain::EM);
    if (m_s2pExportAction) m_s2pExportAction->setEnabled(d == Domain::EM);
    if (m_t3ExportAction)  m_t3ExportAction->setEnabled(d == Domain::Optical);

    // ステータスバーの Δt / CFL は時間発展 FDTD の量 — 光 (RCWA/BPM) と
    // 水中 (BELLHOP = 周波数領域レイトレース) では表示しない
    const bool fdtdLike = (d == Domain::EM || d == Domain::Acoustic);
    if (m_sbDt)  m_sbDt->setVisible(fdtdLike);
    if (m_sbCfl) m_sbCfl->setVisible(fdtdLike);

    m_domainBar->setActiveDomain(d);
    m_center->setDomain(d);

    // アクセント色はドメイン依存なのでテーマ全体を貼り替える。
    // (ウィジェット単位の setStyleSheet はアプリ側 QSS に勝ってしまうため使わない)
    applyTheme();
}

// 現在の (スタイル, テーマ, 密度, ドメイン) で QSS を生成し直して適用する
void MainWindow::applyTheme()
{
    // QSS だけでなくパレット / カラースキームも揃える (Theme::apply に集約)。
    // QSS が触らない要素が OS の外観を拾って配色が混ざるのを防ぐ。
    Theme::apply(m_uiStyle, m_uiTheme, m_density, m_project->activeDomain());
    // QPainter 描画のウィジェットは QSS の対象外なので明示的に伝える
    m_viewport->setDarkPalette(Theme::isDarkPalette(m_uiStyle, m_uiTheme));
}

void MainWindow::setUiLevel(bool expert)
{
    if (m_expert == expert) return;
    m_expert = expert;
    QSettings().setValue("ui/level", expert ? "expert" : "standard");
    (expert ? m_levelExpert : m_levelStandard)->setChecked(true);
    if (m_levelCheck && m_levelCheck->isChecked() != expert) {
        QSignalBlocker block(m_levelCheck);   // 再入防止
        m_levelCheck->setChecked(expert);
    }
    m_nav->rebuild(m_project->activeDomain(), m_expert);
    updateLevelHint();
}

// 表示モードのラベルを「今どちらか」「標準だと何項目隠れているか」が
// 分かる形に更新する (ドメインで隠れる数が変わるので都度数え直す)。
void MainWindow::updateLevelHint()
{
    if (!m_levelCheck) return;
    const Domain d = m_project->activeDomain();
    const int shown  = m_nav->pageCount(d, m_expert);
    const int all    = m_nav->pageCount(d, true);
    const int hidden = all - shown;
    m_levelCheck->setText(m_expert || hidden <= 0
        ? I18n::tr("nav_expert")
        : I18n::tr("nav_expert_hidden").arg(hidden));
}

void MainWindow::onProjectChanged()
{
    updateKernelWarn();   // 光ドメインはソルバー設定でカーネルが変わる
    // エンジンの選択肢は「ドメイン × 光ソルバー × RCWA 層スタックの有効性」で
    // 変わる (orcwa の MPI/CUDA 版は RCWA を計算できない、等)。changed() は
    // 編集のたびに来るので、この署名が変わったときだけ再評価する
    // (updateEngineItems はバイナリ探索を伴う)。
    if (m_engineBox) {
        const OpticalOpts &oo = m_project->optical();
        const QString sig = QStringLiteral("%1:%2:%3")
            .arg(int(m_project->activeDomain()))
            .arg(int(oo.solver))
            .arg(isValidRcwaStack(oo.rcwaLayerList) ? 1 : 0);
        if (sig != m_engineSig) {
            m_engineSig = sig;
            updateEngineItems(m_project->activeDomain());
        }
    }
    m_sbCells->setText(QStringLiteral("cells: %L1").arg(m_project->totalCells()));
    m_sbMem->setText(QStringLiteral("mem: %1 MB")
        .arg(m_project->estimatedMemoryMB(), 0, 'f', 1));
    const double dtLimit = m_project->courantDt();
    // 実際に使う Δt: 0 なら自動 (= CFL 限界)
    const double dt = (m_project->general().dt > 0) ? m_project->general().dt
                                                    : dtLimit;
    m_sbDt->setText(dt > 0
        ? QStringLiteral("%1: %2 s").arg(I18n::tr("sb_dt"),
                                         QString::number(dt, 'g', 3))
        : QStringLiteral("Δt: -"));
    // クーラン数 = Δt / Δt_CFL (1.0 を超えると発散)
    m_sbCfl->setText((dt > 0 && dtLimit > 0)
        ? QStringLiteral("%1: %2").arg(I18n::tr("sb_cfl"),
              QString::number(dt / dtLimit, 'f', 2))
        : QStringLiteral("%1: -").arg(I18n::tr("sb_cfl")));
}

// ── File actions ────────────────────────────────────────────────────────────
void MainWindow::updateWindowTitle()
{
    const QString file = m_project->filePath().isEmpty()
        ? I18n::tr("untitled")
        : QFileInfo(m_project->filePath()).fileName();
    // 未保存の変更は * で示す。これが無いと「保存を押しても何も起きない」の
    // ように見え、保存できているのか判断できない。
    setWindowTitle(QStringLiteral("OpenFDTD-X — %1%2")
                       .arg(file, m_project->isModified()
                                      ? QStringLiteral(" *") : QString()));
}

void MainWindow::newProject()
{
    // 前の実行の結果 (3D の結果断面など) を新しいプロジェクトへ持ち越さない
    m_center->clearResultField();
    m_project->clear();
    emit m_project->loaded();
    emit m_project->changed();
    updateWindowTitle();
}

namespace {
// --screenshot などの自動実行中か (モーダルを出さない判断に使う)
bool g_automation = false;
} // namespace

void MainWindow::setAutomation(bool on) { g_automation = on; }
bool MainWindow::automation() { return g_automation; }

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
        // 自動実行ではモーダルを出さない (押す人が居ないので止まる)。
        // 同じ内容を標準エラーとステータスバーへ出して先へ進む
        if (automation()) {
            qWarning().noquote()
                << QStringLiteral("%1: %2").arg(I18n::tr("tb_open"), err);
            statusBar()->showMessage(err);
        } else {
            QMessageBox::warning(this, I18n::tr("tb_open"), err);
        }
        return;
    }
    m_evViewer->setWorkdir(QFileInfo(p).path());
    // 同じフォルダにカーネルの作図出力があれば「カーネル作図」画面へ載せる
    // (どのファイルを見ているかは常に見える — H5 アニメと同じ扱い)
    if (auto *ev = m_center->evCanvas()) {
        const QString evp = QFileInfo(p).dir().filePath(
            QStringLiteral("ev.ev2"));
        if (QFileInfo::exists(evp)) ev->load(evp);
        else                        ev->clear();
    }
    // 同じフォルダのポストデータ (far2d.log / near2d.log) も読む
    m_center->loadPostMaps(QFileInfo(p).path());
    // 前のプロジェクトの結果を残さない (別プロジェクトの結果断面が
    // そのまま 3D シーンに残るのを防ぐ — .claude/rules/gui.md)
    m_center->clearResultField();
    // プロジェクトのディレクトリに既存の HDF5 結果があれば H5 アニメタブへ
    // 自動セットする (どのファイルを見ているかは同タブに常に明示される。
    // 「この実行の結果」とは扱わない — 2D 断面への反映は実行時の mtime
    // ゲート付き経路のみが行う)
    const QString h5 = QFileInfo(p).dir()
                           .filePath(QStringLiteral("time_series_data.h5"));
    if (QFileInfo::exists(h5) && !H5Reader::isHdf5(h5)) {
        // 名前は .h5 でも中身が HDF5 でない (途中で止まった実行が残した
        // 空ファイル等)。黙って読みにいくと HDF5 ライブラリのエラースタックが
        // 出るだけで何が起きたか分からないので、理由をログに出して読まない。
        m_rightDock->appendLog(I18n::tr("log_h5_not_hdf5").arg(h5));
    } else if (QFileInfo::exists(h5)) {
        if (auto *viewer = qobject_cast<H5ViewerTab *>(m_tabH5Viewer))
            viewer->openFile(h5);
        m_rightDock->appendLog(I18n::tr("log_h5_found").arg(h5));
        // 3D シーンへは重ねられる (どのファイルの結果かは断面の凡例に出る)。
        // 2D 断面と違い実行ゲートを掛けないのは、H5 アニメタブと同じく
        // 「開いたファイルの中身」を出所付きで見せるだけだから。
        m_center->loadResult3DSlice(h5);
    }
    updateWindowTitle();
}

void MainWindow::saveProject()
{
    if (m_project->filePath().isEmpty()) {
        saveProjectAs();
        return;
    }
    QString err;
    if (!m_project->save(m_project->filePath(), &err)) {
        QMessageBox::warning(this, I18n::tr("tb_save"), err);
        return;
    }
    reportSaved(m_project->filePath());
}

// 保存できたことを利用者に見える形で伝える。成功時に何も出ないと
// 「ボタンが効いていない」と区別が付かない (実際にそう見えていた)。
// .ofd と同時に .ofdx サイドカーも書くので、両方の名前を出す。
void MainWindow::reportSaved(const QString &path)
{
    const QFileInfo fi(path);
    const QString ofdx = fi.completeBaseName() + QStringLiteral(".ofdx");
    statusBar()->showMessage(
        I18n::tr("save_ok").arg(fi.fileName(), ofdx), 5000);
    m_rightDock->appendLog(
        I18n::tr("save_ok_log").arg(fi.absoluteFilePath(), ofdx));
    updateWindowTitle();
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
    reportSaved(p);
}

// ── Dialogs ─────────────────────────────────────────────────────────────────
void MainWindow::showGallery()
{
    if (!m_galleryDialog) {
        m_galleryDialog = new AppGalleryDialog(this);
        connect(m_galleryDialog, &AppGalleryDialog::templatePicked, this,
                [this](const QString &domain, const QString &id,
                       const QString &name) {
            // シナリオに応じたメッシュ・物性値・形状・波源・周波数・
            // ドメイン設定を投入した新規プロジェクトを作成する
            // (core/ProjectTemplates)。tidy3d グループは光ドメインの
            // クラウドテンプレートとして扱われる。
            if (!templates::apply(*m_project, domain, id, name)) {
                // 未知 ID (レジストリ外) — 従来どおりドメインとタイトルのみ
                if (domain != "tidy3d")
                    m_project->setActiveDomain(domainFromKey(domain));
                m_project->general().title = name;
            }
            emit m_project->loaded();
            emit m_project->changed();
            updateWindowTitle();
        });
        // フッタのボタンを実動作へ接続 (以前は閉じるだけだった)
        connect(m_galleryDialog, &AppGalleryDialog::openFileRequested,
                this, [this] { openProject(); });
        connect(m_galleryDialog, &AppGalleryDialog::blankRequested,
                this, &MainWindow::newProject);
    }
    m_galleryDialog->open();
}

void MainWindow::showResources()
{
    if (!m_resourceDialog) {
        m_resourceDialog = new ResourceDialog(this);
        // 適用された値をツールバーのスレッド数へ反映する
        // (2 箇所で別の値を持たない — 実行に使うのは 1 つ)
        connect(m_resourceDialog, &ResourceDialog::applied, this,
                [this](int processes, int threads) {
                    m_threadsBox->setValue(threads);
                    m_rightDock->appendLog(
                        I18n::tr("run_res_applied").arg(processes).arg(threads));
                    updateEngineItems(m_project->activeDomain());
                });
    }
    m_resourceDialog->open();
}

void MainWindow::showGettingStarted()
{
    if (!m_gettingStarted) {
        m_gettingStarted = new GettingStartedDialog(this);
        connect(m_gettingStarted, &GettingStartedDialog::jumpTo, this,
                [this](const QString &target) {
            if (target == "gallery") { showGallery(); return; }
            if (target == "run")     { runSimulation(); return; }
            // 検証タブはエキスパート表示のみ。標準表示のままだと選択が
            // 静かに失敗して「押しても何も起きない」になるため、明示的な
            // ジャンプ要求として表示レベルを切替えてから選択する。
            if (target == QLatin1String("verification") && !m_expert) {
                setUiLevel(true);
                statusBar()->showMessage(I18n::tr("gsd_expert_switched"), 4000);
            }
            selectLeftTab(target);
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
    // MPI プロセス数は Resources ダイアログの設定を使う。
    // 旧実装はここを設定しておらず、RunConfig の既定 2 が常に使われていた
    // (画面で何を設定しても mpiexec -n 2 になっていた)。
    cfg.processes = qMax(1, ResourceDialog::savedProcesses());
    cfg.device = m_deviceBox->value();
    // 表示バックエンドに「HTML出力」を選んでいるときだけ ofd_post へ -html を
    // 渡す。従来ここが繋がっておらず、-html は一度も渡されないのに EvViewer は
    // ev2d.htm を探しにいくため、既定設定では必ず「出力ファイルが
    // 見つかりません」になっていた。
    cfg.evHtml = m_evViewer && m_evViewer->needsHtmlOutput();
    QSettings().setValue("run/device", cfg.device);
    QSettings().setValue("run/threads", cfg.threads);

    // カーネル選択はドメインとソルバー設定から決める (Runner と共用の規則)。
    // 光: RCWA → orcwa / BPM → obpm / FMM → orcwa (RCWA と同一手法)。
    cfg.kernel = Runner::kernelForProject(*m_project);
    return cfg;
}

void MainWindow::runSimulation()
{
    if (m_runner->isRunning()) {
        // 実行中の押下は「中止」。押した本人が意図しているとは限らない
        // (連打・誤操作) ので必ず確認してから殺す。計算は数分〜数十分単位で、
        // 中止すると結果は残らない。
        QMessageBox box(QMessageBox::Question, I18n::tr("tb_calc_stop"),
                        I18n::tr("run_stop_confirm"),
                        QMessageBox::Yes | QMessageBox::No, this);
        if (auto *yes = box.button(QMessageBox::Yes))
            yes->setText(I18n::tr("tb_calc_stop"));
        if (auto *no = box.button(QMessageBox::No))
            no->setText(I18n::tr("run_stop_continue"));
        box.setDefaultButton(QMessageBox::No);
        if (box.exec() == QMessageBox::Yes) m_runner->stop();
        return;
    }
    // エンジンに tidy3d Cloud を選択中はクラウド送信ダイアログへ
    if (m_engineBox->currentIndex() > 3) {
        showCloudDialog();
        return;
    }
    // エンジン × ソルバー設定の仕様上できない組合せ (orcwa の MPI/CUDA 版は
    // FDTD 専用で RCWA を計算できない、等)。選択肢は updateEngineItems が
    // 無効化しているが、ソルバー設定をあとから変えた場合に備えて直前にも見る
    // (走らせて初めて失敗する組合せを開始しない — 絶対規則 5)。
    {
        const QString why = Runner::engineUnsupportedReason(
            *m_project, Engine(qMin(m_engineBox->currentIndex(), 3)));
        if (!why.isEmpty()) {
            QMessageBox::warning(this, I18n::tr("tb_calc"),
                                 I18n::tr("run_engine_na").arg(why));
            updateEngineItems(m_project->activeDomain());
            return;
        }
    }
    // RCWA / FMM (どちらも orcwa カーネル): 層スタックが空または不正なら
    // 実行前にエラー表示して止める。OfdIO 側のゲートは不正な rcwa 設定を
    // 書き出さないため、そのまま走らせると「rcwa キーの無い入力で orcwa が
    // 必ず失敗する」— 確実に失敗する実行を開始しない (.claude/rules/gui.md)。
    if (m_project->activeDomain() == Domain::Optical) {
        const OpticalOpts &oo = m_project->optical();
        if ((oo.solver == OpticalSolver::RCWA ||
             oo.solver == OpticalSolver::FMM) &&
            !isValidRcwaStack(oo.rcwaLayerList)) {
            QMessageBox::warning(this, I18n::tr("tb_calc"),
                                 I18n::tr("run_rcwa_stack_err"));
            return;
        }
    }
    // 音響ドメイン: 計算ボタンは ofd (電磁 FDTD) の波動アナロジー実行で、
    // 音響指標の定量値は得られない (ADR-0004 — 音響 FDTD は外部専用ソルバー)。
    // 定量計算と誤解しないよう初回に確認する (絶対規則 5)。ユーザー操作から
    // しか到達しないので、ヘッドレスのスクリーンショット実行では出ない。
    if (m_project->activeDomain() == Domain::Acoustic &&
        !QSettings().value("run/acousticAnalogyWarned", false).toBool()) {
        QMessageBox box(QMessageBox::Question, I18n::tr("tb_calc"),
                        I18n::tr("run_acoustic_analogy"),
                        QMessageBox::Ok | QMessageBox::Cancel, this);
        if (auto *ok = box.button(QMessageBox::Ok))
            ok->setText(I18n::tr("run_aa_continue"));
        auto *dontShow = new QCheckBox(I18n::tr("run_aa_dont_show"), &box);
        box.setCheckBox(dontShow);
        if (box.exec() != QMessageBox::Ok) return;
        if (dontShow->isChecked())
            QSettings().setValue("run/acousticAnalogyWarned", true);
    }
    m_plotPanel->clearConvergence();
    // 前回実行の結果カーブを消し、実行中は収束履歴を前面にする
    // (「この実行が生成したもの」だけを表示する — .claude/rules/gui.md)
    m_plotPanel->clearRunResults();
    m_plotPanel->showConvergence();
    m_lastAeff_m2 = 0.0;
    // モックの計算コンソール冒頭 2 行
    m_rightDock->appendLog("=== " + I18n::tr("log_starting") + " ===");
    m_rightDock->appendLog(I18n::tr("log_validate"));

    const RunConfig cfg = currentRunConfig();
    // ONN 活性化カーブは「この実行が生成したもの」だけを表示する。
    m_expectActivation = Runner::producesActivationCurve(*m_project, cfg);
    // 解析解の β / L はここでスナップショットする (実行中の UI 編集で
    // 実測 CSV と対応しない解析解が重ならないように)。
    m_runTpaBeta_cmGW = m_project->optical().tpaBeta_cmGW;
    const MeshAxis &mz = m_project->mesh(2);
    m_runLength_m = mz.nodes.isEmpty()
        ? 0.0 : (mz.nodes.last() - mz.nodes.first());
    // 前回実行が残した activation_curve.csv を今回の結果と取り違えないよう、
    // ソルバー段が走る実行では起動前に消しておく。
    if (cfg.mode != RunMode::Post) {
        const QString wd = Runner::resolveWorkingDir(m_project, cfg);
        if (!wd.isEmpty())
            QFile::remove(QDir(wd).filePath(QStringLiteral(
                "activation_curve.csv")));
    }

    m_sbProgress->setVisible(true);
    m_sbProgress->setValue(0);
    m_sbState->setText("● " + I18n::tr("sb_running"));

    if (!m_runDialog)
        m_runDialog = new RunDialog(m_runner, this);
    m_runDialog->clearLog();
    m_runDialog->show();

    // cfg は上でスナップショット済み — 再計算せずそのまま渡す
    // (実行前クリーンアップ判定と同一の設定で走らせる)。
    m_runStartMs = QDateTime::currentMSecsSinceEpoch();
    setRunUiEnabled(false);
    m_runner->start(m_project, cfg);
    m_evViewer->setWorkdir(m_runner->workingDir());
}

void MainWindow::runPostProcess()
{
    if (m_runner->isRunning()) return;
    // エンジンに tidy3d Cloud を選んだままのポスト処理は、currentRunConfig の
    // qMin により GPU_MPI へ落ちて選択と実行内容が食い違う。実行せず理由を出す。
    if (m_engineBox->currentIndex() > 3) {
        QMessageBox::information(this, I18n::tr("tb_post"),
                                 I18n::tr("run_post_cloud_na"));
        return;
    }
    RunConfig cfg = currentRunConfig();
    cfg.mode = RunMode::Post;
    // ポスト処理は activation_curve.csv を作らない (残存 CSV を結果として
    // 表示しない)。
    m_expectActivation = false;
    m_sbState->setText("● " + I18n::tr("sb_running"));
    m_runStartMs = QDateTime::currentMSecsSinceEpoch();
    setRunUiEnabled(false);
    m_runner->start(m_project, cfg);
    m_evViewer->setWorkdir(m_runner->workingDir());
}

// CLI --ui-* 用: QSettings を汚さずセッション限りでテーマを差し替える
void MainWindow::setThemeOverride(UiStyle style, UiTheme theme, Density density)
{
    m_uiStyle = style;
    m_uiTheme = theme;
    m_density = density;
    applyTheme();
}

void MainWindow::setViewStyle(int index)
{
    m_center->setViewStyleIndex(index);
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

void MainWindow::selectCenterTab(const QString &titlePart)
{
    m_center->selectTabContaining(titlePart);
}

void MainWindow::show2DPlot()
{
    m_center->showPlot();
}

void MainWindow::show3DPlot()
{
    m_center->showViewport();
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

// ── オペラ音響の一括レポート ────────────────────────────────────────────────
// 実行済みの RIR 分析 / 歌声分析の結果を 1 ファイルにまとめる。分析の
// 再実行はしない (未実行の系統はレポート上に「未実行」と明示される)。
void MainWindow::exportAcousticReport()
{
    const OperaAcousticSettings &op = m_project->operaAcoustic();

    AcousticReportInput in;
    in.projectTitle        = m_project->general().title;
    in.rirFile             = QFileInfo(op.rirPath).fileName();
    in.voiceFile           = QFileInfo(op.voicePath).fileName();
    in.calibrationState    = op.calibrationState;
    in.calibrationOffsetDb = op.calibrationOffsetDb;
    // G (音の強さ) の分母。ファイル名だけを載せる (パスは出さない — 他の
    // ファイル欄と同じ扱い)
    in.stConditionDeclared  = op.stConditionDeclared;
    in.strengthRefMode      = op.strengthRefMode;
    in.strengthRefFile      = QFileInfo(op.strengthRefFile).fileName();
    in.strengthRefLevelDb   = op.strengthRefLevelDb;
    in.strengthRefDistanceM = op.strengthRefDistanceM;
    in.auralizationDryFile = QFileInfo(op.auralizationDryFile).fileName();
    in.auralizationOutputFile =
        QFileInfo(op.auralizationOutputFile).fileName();

    if (auto *rirTab = qobject_cast<RirAnalysisTab *>(m_tabRirAnalysis)) {
        in.hasRir = rirTab->hasResult();
        if (in.hasRir) in.rir = rirTab->result();
    }
    if (auto *vocalTab = qobject_cast<VocalAnalysisTab *>(m_tabVocal)) {
        in.hasVocal = vocalTab->hasResult();
        if (in.hasVocal) in.vocal = vocalTab->result();
    }

    const QString caption = I18n::tr("m_acoustic_report");
    // 両方とも未実行なら空のレポートを作らず、理由を伝えて終わる。
    if (!AcousticReportBuilder::hasAnyResult(in)) {
        QMessageBox::information(this, caption, I18n::tr("rep_none_msg"));
        return;
    }

    const QString path = QFileDialog::getSaveFileName(
        this, caption, QStringLiteral("opera_acoustics_report.html"),
        "HTML (*.html);;CSV (*.csv)");
    if (path.isEmpty()) return;

    // 拡張子で書式を決める (既定は HTML)
    const bool asCsv = path.endsWith(QStringLiteral(".csv"), Qt::CaseInsensitive);
    const QString content = asCsv ? AcousticReportBuilder::buildCsv(in)
                                  : AcousticReportBuilder::buildHtml(in);

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, caption, f.errorString());
        return;
    }
    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    out << content;
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
    // ONN パワースイープ: obpm が出す実効断面積を控えておく (解析解用)
    const double aeff = ActivationCurve::aeffFromLogLine(line);
    if (aeff > 0) m_lastAeff_m2 = aeff;
}

void MainWindow::onRunnerFinished(bool ok)
{
    // 実行設定のロック解除 (FailedToStart 経由でもここへ来る)
    setRunUiEnabled(true);
    m_sbProgress->setVisible(false);
    m_sbState->setText("● " + (ok ? I18n::tr("sb_done") : I18n::tr("sb_failed")));
    // カーネルの HDF5 出力 (time_series_data.h5) を 2D 断面へ反映する。
    // 「この実行が生成したもの」だけを表示するため、実行開始以降に更新された
    // ファイルに限る (残存ファイルの再表示をしない — .claude/rules/gui.md)。
    if (ok) {
        const QString h5 = QDir(m_runner->workingDir())
                               .filePath(QStringLiteral("time_series_data.h5"));
        const QFileInfo fi(h5);
        if (fi.exists()
            && fi.lastModified().toMSecsSinceEpoch() >= m_runStartMs
            && !H5Reader::isHdf5(h5)) {
            // カーネルが書きかけ / 空のまま終わったケース。読みにいかず
            // 理由を出す (エラースタックの山より 1 行の方が分かる)。
            m_rightDock->appendLog(I18n::tr("log_h5_not_hdf5").arg(h5));
        } else if (fi.exists()
            && fi.lastModified().toMSecsSinceEpoch() >= m_runStartMs) {
            if (m_center->loadResultField(h5))
                m_rightDock->appendLog(
                    I18n::tr("log_h5_slice").arg(fi.fileName()));
            // 伝搬時系列 (ofd の /timeseries、obpm の /field/frames 等) は
            // H5 アニメタブで再生する — この実行の h5 を読み込んでおく
            if (auto *viewer = qobject_cast<H5ViewerTab *>(m_tabH5Viewer)) {
                viewer->openFile(h5);
                m_rightDock->appendLog(I18n::tr("log_h5_anime"));
            }
        }
    }
    // カーネルログの給電点表と far1d.log の遠方界パターンを結果プロットへ
    // 反映する (この実行が更新したファイルに限る)。反映できたら結果プロット
    // タブへ切替えて「計算後に結果が画面に出る」動線にする。
    if (ok) {
        const QDir wd(m_runner->workingDir());
        auto freshFile = [&](const QString &name) -> QString {
            const QFileInfo fi(wd.filePath(name));
            return (fi.exists() &&
                    fi.lastModified().toMSecsSinceEpoch() >= m_runStartMs)
                       ? fi.absoluteFilePath() : QString();
        };
        const Kernel k = m_runner->config().kernel;
        const QString logName =
            k == Kernel::FDTD ? QStringLiteral("ofd.log") :
            k == Kernel::RCWA ? QStringLiteral("orcwa.log") :
            k == Kernel::BPM  ? QStringLiteral("obpm.log") : QString();
        QVector<FeedSweep> sweeps;
        QVector<FarPattern> patterns;
        if (!logName.isEmpty()) {
            const QString logPath = freshFile(logName);
            if (!logPath.isEmpty())
                sweeps = KernelResultReader::readFeedSweeps(logPath);
            const QString farPath = freshFile(QStringLiteral("far1d.log"));
            if (!farPath.isEmpty())
                patterns = KernelResultReader::readFar1d(farPath);
        }
        // 熱解析レイヤ (ofd が入力キー無しで常に出す診断) を連成タブへ。
        // この実行が更新したログに限る (残存ログを再表示しない)。
        if (!logName.isEmpty()) {
            if (auto *mp = qobject_cast<MultiphysicsTab *>(m_tabMultiphysics)) {
                const QString lp = freshFile(logName);
                if (lp.isEmpty()) mp->clearThermal();
                else              mp->loadThermalFrom(lp);
            }
        }
        if (!sweeps.isEmpty() || !patterns.isEmpty()) {
            m_plotPanel->setRunResults(sweeps, patterns);
            if (!sweeps.isEmpty()) {
                int n = 0;
                for (const FeedSweep &s : sweeps) n += s.points.size();
                m_rightDock->appendLog(
                    I18n::tr("log_freqchar").arg(logName).arg(n));
            }
            if (!patterns.isEmpty())
                m_rightDock->appendLog(
                    I18n::tr("log_farpattern").arg(patterns.size()));
            m_center->showPlot();
        } else if (!logName.isEmpty()) {
            // 結果プロットに出せるものが何も無かった。ポスト(1) のチェックを
            // 入れても結果プロットは変わらない (あちらは ev2d / HTML 向け) —
            // 何を足せば出るのかをここで言う (絶対規則 5)。
            m_rightDock->appendLog(I18n::tr("log_noplotdata").arg(logName));
        }
        // 作図出力 (ev.ev2 / ev.ev3) — 図形表示の実体。EvViewer をこの実行の
        // 作業ディレクトリへ向け、生成を計算コンソールに知らせる
        QStringList evFiles;
        for (const char *name : { "ev.ev2", "ev.ev3", "ev2d.ev2", "ev3d.ev3",
                                  "ev2d.htm", "ev3d.htm" })
            if (!freshFile(QString::fromLatin1(name)).isEmpty())
                evFiles << QString::fromLatin1(name);
        if (!evFiles.isEmpty()) {
            m_evViewer->setWorkdir(wd.path());   // 中で ev.ev2 を読み直す
            // 中央の「カーネル作図」画面へも読み込む — 外部ビューワーも
            // ブラウザも無い環境で図が見られるようにする
            if (auto *ev = m_center->evCanvas()) {
                const QString p2 = wd.filePath(QStringLiteral("ev.ev2"));
                QString everr;
                if (QFileInfo::exists(p2) && ev->load(p2, &everr))
                    m_rightDock->appendLog(
                        I18n::tr("log_ev_native").arg(ev->pageCount()));
            }
            m_rightDock->appendLog(
                I18n::tr("log_ev_ready").arg(evFiles.join(QStringLiteral(", "))));
        } else {
            // ポスト段が作図を出さなかった。どちらの経路で見るにせよ
            // ファイルが無いことが原因なので、それを言う (無言にしない)。
            m_rightDock->appendLog(I18n::tr("log_ev_none"));
        }
        // ポスト表示 (ev を使わない場マップ) — far2d.log / near2d.log を
        // 直接読む。作図出力 (ev) の有無とは無関係なので、
        // 上の if / else とは別に必ず行う。
        m_center->loadPostMaps(wd.path());
        // ポスト処理のテキスト表 (feed.log / point.log / far0d.log /
        // near1d.log) を結果プロットへ。ev.ev2 が出ていなくても、ポストの
        // チェックを入れた項目の中身がここで見える (ev2d/ev3d を使わない経路)。
        // この実行が更新したファイルに限る (残存ログを再表示しない)。
        {
            QVector<PostTable> tables;
            for (const char *name : { "feed.log", "point.log",
                                      "far0d.log", "near1d.log" }) {
                const QString path = freshFile(QString::fromLatin1(name));
                if (!path.isEmpty())
                    tables += KernelResultReader::readPostTables(path);
            }
            m_plotPanel->setPostTables(tables);
            if (!tables.isEmpty())
                m_rightDock->appendLog(
                    I18n::tr("log_posttables").arg(tables.size()));
        }
    }
    // ONN 活性化カーブは、この実行が obpm + powersweep だったときだけ
    // 表示する (他カーネルの実行で過去の CSV を再表示しない)。
    if (ok && m_expectActivation)
        m_tabOptical->showActivationResult(m_runner->workingDir(),
                                           m_lastAeff_m2, m_runTpaBeta_cmGW,
                                           m_runLength_m);
    // 水中音響 (bellhopcxx): 結果は .prt (ログ) と .shd (TL 音場) に出る。
    // .prt の先頭行と .shd の生成有無・サイズを計算コンソールへ出す。
    // .shd (SHDFIL) のバイナリ読解・TL 図化は未実装 (外部の可視化ツールを
    // 案内する — 未実装機能を動作済みと表示しない)。
    if (ok && m_runner->config().kernel == Kernel::Bellhop) {
        const QDir wd(m_runner->workingDir());
        const QString base = BellhopIO::caseName(*m_project);
        const QFileInfo shd(wd.filePath(base + ".shd"));
        m_rightDock->appendLog("=== bellhopcxx ===");
        QFile prt(wd.filePath(base + ".prt"));
        if (prt.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QStringList lines =
                QString::fromUtf8(prt.readAll()).split(QLatin1Char('\n'));
            for (const QString &l : lines)
                if (!l.trimmed().isEmpty()) { m_rightDock->appendLog(l); break; }
        }
        m_rightDock->appendLog(shd.exists()
            ? I18n::tr("uw_shd_ok").arg(shd.fileName())
                                   .arg(QLocale().formattedDataSize(shd.size()))
            : I18n::tr("uw_shd_missing"));
        // TL 音場を読んで水中音響タブの「TL 断面」へ反映する
        // (この実行が生成した .shd だけを対象にする)
        auto *uwTab = qobject_cast<UnderwaterTab *>(m_tabUnderwater);
        // 到達ファイル (.arr) — 「計算モード = 到達時間」のときに出る。
        // 受信インパルス応答の作成欄をこの実行の結果で更新する。
        if (uwTab) uwTab->showArrivalResult(m_runner->workingDir(), base);
        {
            const QFileInfo arr(wd.filePath(base + ".arr"));
            if (arr.exists())
                m_rightDock->appendLog(
                    I18n::tr("uw_arr_ok").arg(arr.fileName())
                        .arg(QLocale().formattedDataSize(arr.size())));
        }
        if (shd.exists()) {
            if (uwTab) uwTab->showTlResult(m_runner->workingDir(), base);
            ShdField f;
            QString shdErr;
            if (ShdReader::read(shd.absoluteFilePath(), f, &shdErr)) {
                m_rightDock->appendLog(
                    QStringLiteral("TL: %1 x %2, %3..%4 dB")
                        .arg(f.nrz).arg(f.nrr)
                        .arg(f.minTL, 0, 'f', 1).arg(f.maxTL, 0, 'f', 1));
                // 結果ペインの 2D 断面にも同じ TL 場を流す (他ドメインの
                // 結果表示と同じ場所に出す — タブ内の小窓だけにしない)
                if (m_center && m_center->loadTlField(shd.absoluteFilePath()))
                    m_rightDock->appendLog(I18n::tr("uw_shd_slice_ok"));
            } else {
                m_rightDock->appendLog(QStringLiteral("shd: ") + shdErr);
            }
        }
    }
}
