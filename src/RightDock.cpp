// RightDock.cpp
#include "RightDock.h"
#include "I18n.h"
#include "core/Project.h"
#include "widgets/LogConsole.h"
#include "Theme.h"

#include <QButtonGroup>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QSplitter>
#include <QStackedWidget>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

using namespace ofd;

namespace {
const bool s_i18n = [] {
    ofd::I18n::reg("rd_seg_tree",  "Tree",  "Tree");
    ofd::I18n::reg("rd_seg_log",   "Log",   "Log");
    ofd::I18n::reg("rd_seg_props", "Props", "Props");
    ofd::I18n::reg("rd_selected",  "選択中 / Selected", "Selected");
    ofd::I18n::reg("rd_no_sel",
        "ツリーで要素を選択してください。", "Select an item in the tree.");
    // 音響/水中ドメイン用の表記 (EM の feed/point は波源・観測点として
    // 意味を持たないため、ツリー上の見せ方だけ切り替える)
    ofd::I18n::reg("rd_src_point",  "点音源", "Point source");
    ofd::I18n::reg("rd_probe_recv", "受音点", "Receiver");
    return true;
}();
} // namespace

RightDock::RightDock(Project *project, QWidget *parent)
    : QWidget(parent), m_project(project)
{
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    // ── セグメント切替 (Tree / Log / Props) ──
    auto *head = new QWidget(this);
    auto *hh = new QHBoxLayout(head);
    hh->setContentsMargins(4, 3, 4, 3);
    hh->setSpacing(0);
    auto *segGroup = new QButtonGroup(this);
    segGroup->setExclusive(true);
    const char *segKeys[3] = { "rd_seg_tree", "rd_seg_log", "rd_seg_props" };
    for (int i = 0; i < 3; ++i) {
        auto *b = new QToolButton(head);
        b->setText(I18n::tr(segKeys[i]));
        b->setCheckable(true);
        b->setChecked(i == 0);
        // 高さは QSS の padding / font-size (密度設定) に任せる。
        // ここで固定すると Comfortable 密度で文字が下端で切れる。
        segGroup->addButton(b, i);
        hh->addWidget(b);
    }
    hh->addStretch(1);
    v->addWidget(head);

    m_stack = new QStackedWidget(this);

    m_tree = new QTreeWidget(m_stack);
    m_tree->setHeaderLabels({ I18n::tr("rd_project"), "" });
    m_tree->setColumnWidth(0, 160);
    m_tree->setRootIsDecorated(true);

    m_log = new LogConsole(m_stack);

    // Props ページ
    auto *propsScroll = new QScrollArea(m_stack);
    propsScroll->setWidgetResizable(true);
    propsScroll->setFrameShape(QFrame::NoFrame);
    auto *propsBody = new QWidget(propsScroll);
    auto *pv = new QVBoxLayout(propsBody);
    pv->setContentsMargins(8, 8, 8, 8);
    m_propEmpty = new QLabel(I18n::tr("rd_no_sel"), propsBody);
    m_propEmpty->setWordWrap(true);
    pv->addWidget(m_propEmpty);
    auto *formHost = new QWidget(propsBody);
    m_propForm = new QFormLayout(formHost);
    m_propForm->setContentsMargins(0, 0, 0, 0);
    pv->addWidget(formHost);
    pv->addStretch(1);
    propsScroll->setWidget(propsBody);

    m_stack->addWidget(m_tree);
    m_stack->addWidget(m_log);
    m_stack->addWidget(propsScroll);
    v->addWidget(m_stack, 1);

    connect(segGroup, &QButtonGroup::idClicked,
            m_stack, &QStackedWidget::setCurrentIndex);
    connect(m_tree, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem *cur, QTreeWidgetItem *) {
        showProperties(cur);
    });

    connect(project, &Project::changed, this, &RightDock::rebuildTree);
    connect(project, &Project::loaded,  this, &RightDock::rebuildTree);
    rebuildTree();
}

// ツリー選択 → Props ページ。ツリーは (名前, 補足) の 2 列なので、
// 選択項目とその親の情報をそのままプロパティ行として並べる。
void RightDock::showProperties(QTreeWidgetItem *item)
{
    while (m_propForm->rowCount() > 0) m_propForm->removeRow(0);
    if (!item) {
        m_propEmpty->setVisible(true);
        return;
    }
    m_propEmpty->setVisible(false);

    const auto mono = [this](const QString &text) {
        auto *l = new QLabel(text, m_propEmpty->parentWidget());
        l->setStyleSheet(Theme::monoQss());
        l->setTextInteractionFlags(Qt::TextSelectableByMouse);
        return l;
    };

    m_propForm->addRow(I18n::tr("rd_selected"), mono(item->text(0)));
    if (!item->text(1).isEmpty())
        m_propForm->addRow(QStringLiteral("値 / Value"), mono(item->text(1)));
    if (item->parent())
        m_propForm->addRow(QStringLiteral("分類 / Group"),
                           mono(item->parent()->text(0)));
    if (item->childCount() > 0)
        m_propForm->addRow(QStringLiteral("子要素 / Children"),
                           mono(QString::number(item->childCount())));
}

void RightDock::appendLog(const QString &line)
{
    m_log->appendLine(line);
}

void RightDock::rebuildTree()
{
    m_tree->clear();

    // ドメインで意味を持たない項目は表示しない / 表記を切り替える
    // (Project::setActiveDomain() が changed() を発火するので、ドメイン切替
    //  時もここが呼び直される。表示のみの分岐でモデルには一切触らない)
    const Domain dom = m_project->activeDomain();
    const bool acoustic = (dom == Domain::Acoustic || dom == Domain::Underwater);

    auto *root = new QTreeWidgetItem(m_tree,
        { m_project->general().title.isEmpty() ? I18n::tr("untitled")
                                               : m_project->general().title });
    root->setExpanded(true);

    auto *mesh = new QTreeWidgetItem(root, { I18n::tr("rd_tree_mesh"),
        QStringLiteral("%L1 cells").arg(m_project->totalCells()) });
    static const char *axisName[3] = { "X", "Y", "Z" };
    for (int a = 0; a < 3; ++a) {
        const MeshAxis &ax = m_project->mesh(a);
        new QTreeWidgetItem(mesh, { axisName[a],
            QStringLiteral("%1 cells [%2, %3]")
                .arg(ax.totalCells())
                .arg(QString::number(ax.min(), 'g', 4),
                     QString::number(ax.max(), 'g', 4)) });
    }

    auto *mats = new QTreeWidgetItem(root, { I18n::tr("rd_tree_materials"),
        QString::number(m_project->materials().size()) });
    int id = 2;
    for (const Material &m : m_project->materials()) {
        // 音響/水中では誘電率表記は無意味 → 音響物性 (ρ, c, α) を表示
        const QString desc = acoustic
            ? QStringLiteral("ρ=%1 c=%2 α=%3")
                  .arg(m.rho).arg(m.soundSpeed).arg(m.absorption)
            : (m.type == 2)
            ? QStringLiteral("disp ε∞=%1").arg(m.einf)
            : QStringLiteral("εr=%1 σ=%2").arg(m.epsr).arg(m.esgm);
        new QTreeWidgetItem(mats, {
            QStringLiteral("#%1 %2").arg(id++).arg(m.name), desc });
    }

    auto *geom = new QTreeWidgetItem(root, { I18n::tr("rd_tree_geometry"),
        QString::number(m_project->geometries().size()) });
    int unit = 1;
    for (const Geometry &g : m_project->geometries())
        new QTreeWidgetItem(geom, {
            QStringLiteral("#%1 %2").arg(unit++).arg(
                g.name.isEmpty() ? I18n::tr("ge_shape_" + QString::number(g.shape))
                                 : g.name),
            QStringLiteral("mat %1").arg(g.materialId) });

    // planewave は音響/水中では意味を持たないため表示しない
    // (モデル上の enabled はそのまま — 表示だけ抑制し、件数も表示に合わせる)
    const bool showPw = m_project->planewave().enabled && !acoustic;
    auto *srcs = new QTreeWidgetItem(root, { I18n::tr("rd_tree_sources"),
        QString::number(m_project->feeds().size() + (showPw ? 1 : 0)) });
    for (const Feed &f : m_project->feeds())
        // 音響/水中は点音源 (dir 成分 Ex 等は無意味なので出さない)
        new QTreeWidgetItem(srcs, { acoustic
                ? I18n::tr("rd_src_point")
                : QStringLiteral("feed %1").arg(f.dir),
            QStringLiteral("(%1, %2, %3)").arg(f.x).arg(f.y).arg(f.z) });
    if (showPw)
        new QTreeWidgetItem(srcs, { "planewave",
            QStringLiteral("θ=%1 φ=%2").arg(m_project->planewave().theta)
                                       .arg(m_project->planewave().phi) });

    auto *pts = new QTreeWidgetItem(root, { I18n::tr("rd_tree_points"),
        QString::number(m_project->probes().size()) });
    for (const Probe &pr : m_project->probes())
        // 音響/水中は受音点 (dir 成分表示は抑制)
        new QTreeWidgetItem(pts, { acoustic
                ? I18n::tr("rd_probe_recv")
                : QStringLiteral("point %1").arg(pr.dir),
            QStringLiteral("(%1, %2, %3)").arg(pr.x).arg(pr.y).arg(pr.z) });

    // 集中定数負荷 (R/L/C) は EM 専用 — 他ドメインではデータが
    // 入っていてもノード自体を出さない (モデルは保持したまま)
    if (dom == Domain::EM && !m_project->loads().isEmpty()) {
        auto *lds = new QTreeWidgetItem(root, { I18n::tr("rd_tree_loads"),
            QString::number(m_project->loads().size()) });
        for (const Load &l : m_project->loads())
            new QTreeWidgetItem(lds, { QStringLiteral("%1").arg(l.kind),
                QStringLiteral("%1 @ (%2, %3, %4)")
                    .arg(l.value).arg(l.x).arg(l.y).arg(l.z) });
    }
}
