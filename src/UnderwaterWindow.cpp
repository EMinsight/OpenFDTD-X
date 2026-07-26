// UnderwaterWindow.cpp
#include "UnderwaterWindow.h"
#include "I18n.h"

#include "core/Project.h"
#include "widgets/Viewport3D.h"

#include "tabs/OceanEnvironmentTab.h"
#include "tabs/UnderwaterTab.h"
#include "tabs/AcousticSourceTab.h"
#include "tabs/H5ViewerTab.h"
#include "tabs/InteropTab.h"

#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QVBoxLayout>

using namespace ofd;

namespace {
// OpenUWA 専用の語彙 (接頭辞 uwa_)
const bool s_i18n = [] {
    I18n::reg("uwa_app_title", "OpenUWA — 水中音響解析 (OpenFDTD-X 派生)",
                               "OpenUWA — Underwater Acoustics (OpenFDTD-X derivative)");
    I18n::reg("uwa_engines", "共有エンジン: FDTD / Bellhop / PE / 法線モード",
                             "Shared engines: FDTD / Bellhop / PE / Normal modes");
    I18n::reg("uwa_t_oceanenv",   "🌏 海洋環境",              "🌏 Ocean environment");
    I18n::reg("uwa_t_underwater", "〜 伝搬解析 (SSP/Bellhop/PE)",
                                  "〜 Propagation (SSP/Bellhop/PE)");
    I18n::reg("uwa_t_acsource",   "🎤 音源/指向性",           "🎤 Source / directivity");
    I18n::reg("uwa_t_h5viewer",   "🎬 H5アニメ",              "🎬 H5 animation");
    I18n::reg("uwa_t_interop",    "🔗 ツール連携",            "🔗 Tool interop");
    I18n::reg("uwa_sb_domain", "ドメイン: 水中音響 (固定)",
                               "Domain: underwater acoustics (fixed)");
    I18n::reg("uwa_sb_compat",
        "プロジェクト互換: .ofdx (本体と共通) / .env .bty .ssp",
        "Project compatibility: .ofdx (shared with the main app) / .env .bty .ssp");
    return true;
}();
} // namespace

UnderwaterWindow::UnderwaterWindow(QWidget *parent)
    : QMainWindow(parent), m_project(new Project(this))
{
    setObjectName("OpenUWA_MainWindow");
    resize(1440, 900);
    setMinimumSize(1000, 660);

    // 水中音響に固定 (ドメイン切替は持たない)
    m_project->setActiveDomain(Domain::Underwater);

    buildUi();
    setWindowTitle(I18n::tr("uwa_app_title"));
}

void UnderwaterWindow::buildUi()
{
    auto *central = new QWidget(this);
    auto *v = new QVBoxLayout(central);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    // ── タイトル帯 (モックの qt-titlebar) ──
    auto *title = new QWidget(central);
    title->setObjectName("DomainBar");   // Theme の暗色帯スタイルを流用する
    title->setAttribute(Qt::WA_StyledBackground, true);
    title->setFixedHeight(30);
    auto *th = new QHBoxLayout(title);
    th->setContentsMargins(10, 0, 10, 0);
    th->setSpacing(8);
    auto *icon = new QLabel(QStringLiteral("〜"), title);
    icon->setStyleSheet("color:#26A69A; font-weight:600;");
    auto *name = new QLabel(I18n::tr("uwa_app_title"), title);
    name->setStyleSheet("font-weight:600;");
    auto *engines = new QLabel(I18n::tr("uwa_engines"), title);
    engines->setStyleSheet("font-size:11px;");
    th->addWidget(icon);
    th->addWidget(name);
    th->addStretch(1);
    th->addWidget(engines);
    v->addWidget(title);

    // ── 左: 水中関連タブ / 右: ビューポート ──
    auto *split = new QSplitter(Qt::Horizontal, central);
    split->setChildrenCollapsible(false);

    m_tabs = new QTabWidget(split);
    m_tabs->setDocumentMode(true);
    m_tabs->addTab(new OceanEnvironmentTab(m_project), I18n::tr("uwa_t_oceanenv"));
    m_tabs->addTab(new UnderwaterTab(m_project),       I18n::tr("uwa_t_underwater"));
    m_tabs->addTab(new AcousticSourceTab(m_project),   I18n::tr("uwa_t_acsource"));
    m_tabs->addTab(new H5ViewerTab(m_project),         I18n::tr("uwa_t_h5viewer"));
    m_tabs->addTab(new InteropTab(m_project),          I18n::tr("uwa_t_interop"));

    m_viewport = new Viewport3D(m_project, split);
    m_viewport->setDomain(Domain::Underwater);

    split->addWidget(m_tabs);
    split->addWidget(m_viewport);
    // モックの「幅 46% / 最小 420」
    m_tabs->setMinimumWidth(420);
    split->setStretchFactor(0, 46);
    split->setStretchFactor(1, 54);
    split->setSizes({ 662, 778 });
    v->addWidget(split, 1);

    setCentralWidget(central);

    // ── ステータスバー ──
    statusBar()->addWidget(new QLabel(I18n::tr("uwa_sb_domain")));
    statusBar()->addPermanentWidget(new QLabel(I18n::tr("uwa_sb_compat")));
}

void UnderwaterWindow::openProject(const QString &path)
{
    QString err;
    if (!m_project->load(path, &err)) {
        QMessageBox::warning(this, I18n::tr("tb_open"), err);
        return;
    }
    // 分離アプリは水中固定 — 読み込んだファイルのドメインには追従しない
    m_project->setActiveDomain(Domain::Underwater);
    setWindowTitle(QStringLiteral("%1 — %2")
        .arg(QFileInfo(path).fileName(), I18n::tr("uwa_app_title")));
}

void UnderwaterWindow::selectTab(const QString &titlePart)
{
    for (int i = 0; i < m_tabs->count(); ++i)
        if (m_tabs->tabText(i).contains(titlePart, Qt::CaseInsensitive)) {
            m_tabs->setCurrentIndex(i);
            return;
        }
}
