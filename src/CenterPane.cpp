// CenterPane.cpp
#include "CenterPane.h"
#include "I18n.h"
#include "core/Project.h"
#include "widgets/FieldHeatmap.h"
#include "widgets/MeshPreview.h"
#include "widgets/PlotPanel.h"
#include "widgets/Viewport3D.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QSettings>
#include <QScrollBar>
#include <QSlider>
#include <QStackedWidget>
#include <QTabBar>
#include <QToolButton>
#include <QVBoxLayout>

using namespace ofd;

namespace {
const bool s_i18n = [] {
    ofd::I18n::reg("vp_3d",      "🧊 3D シーン",   "🧊 3D scene");
    ofd::I18n::reg("vp_2d",      "📐 2D 断面",     "📐 2D slice");
    ofd::I18n::reg("vp_plot",    "📊 結果プロット", "📊 Result plot");
    ofd::I18n::reg("vp_mesh",    "📏 メッシュ表示", "📏 Mesh view");
    ofd::I18n::reg("vp_reset",   "🔄 Reset",       "🔄 Reset");
    ofd::I18n::reg("vp_select",  "選択 (Q)",       "Select (Q)");
    ofd::I18n::reg("vp_move",    "平行移動 (G)",   "Move (G)");
    ofd::I18n::reg("vp_rotate",  "回転 (R)",       "Rotate (R)");
    ofd::I18n::reg("vp_scale",   "スケール (S)",   "Scale (S)");
    ofd::I18n::reg("vp_snap",    "Snap:",          "Snap:");
    ofd::I18n::reg("vp_grid",    "グリッド",       "Grid");
    ofd::I18n::reg("vp_boundary","境界 (PML)",     "Boundary (PML)");
    ofd::I18n::reg("vp_rotlabel","Rotate:",        "Rotate:");
    ofd::I18n::reg("vp_style",      "3D:",   "3D:");
    ofd::I18n::reg("vp_style_wire", "Wire",  "Wire");
    ofd::I18n::reg("vp_style_solid","Solid", "Solid");
    ofd::I18n::reg("vp_style_field","+ Field","+ Field");
    ofd::I18n::reg("vp_style_rays", "+ Rays", "+ Rays");
    ofd::I18n::reg("vp_snapshot","📷 Snap",        "📷 Snap");
    ofd::I18n::reg("vp_slice_title",
        "近傍界面上分布 / Near-field slice", "Near-field slice");
    ofd::I18n::reg("vp_saved",   "スクリーンショットを保存", "Save screenshot");
    return true;
}();
} // namespace

CenterPane::CenterPane(Project *project, QWidget *parent)
    : QWidget(parent), m_p(project)
{
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(2);

    // ── ビュータブ ──
    m_tabs = new QTabBar(this);
    m_tabs->setDrawBase(false);
    m_tabs->setExpanding(false);
    m_tabs->addTab(I18n::tr("vp_3d"));
    m_tabs->addTab(I18n::tr("vp_2d"));
    m_tabs->addTab(I18n::tr("vp_plot"));
    m_tabs->addTab(I18n::tr("vp_mesh"));
    v->addWidget(m_tabs);

    // ── ビューポートツールバー (3D シーンのときだけ有効) ──
    m_vpToolbar = new QWidget(this);
    auto *h = new QHBoxLayout(m_vpToolbar);
    h->setContentsMargins(6, 2, 6, 2);
    h->setSpacing(6);

    auto *reset = new QToolButton(m_vpToolbar);
    reset->setText(I18n::tr("vp_reset"));
    h->addWidget(reset);

    // 操作ギズモ (モックでは操作モードの示唆。実操作は Viewport3D のマウス)
    auto *gizmoGroup = new QButtonGroup(this);
    gizmoGroup->setExclusive(true);
    struct G { const char *glyph, *tipKey; };
    const G gizmos[] = { { "⊕", "vp_select" }, { "↔", "vp_move" },
                         { "⟳", "vp_rotate" }, { "⤢", "vp_scale" } };
    int gi = 0;
    for (const G &g : gizmos) {
        auto *b = new QToolButton(m_vpToolbar);
        b->setText(QString::fromUtf8(g.glyph));
        b->setToolTip(I18n::tr(g.tipKey));
        b->setCheckable(true);
        if (gi++ == 0) b->setChecked(true);
        gizmoGroup->addButton(b);
        h->addWidget(b);
    }

    h->addWidget(new QLabel(I18n::tr("vp_snap"), m_vpToolbar));
    auto *grid = new QCheckBox(I18n::tr("vp_grid"), m_vpToolbar);
    grid->setChecked(true);
    auto *bnd = new QCheckBox(I18n::tr("vp_boundary"), m_vpToolbar);
    h->addWidget(grid);
    h->addWidget(bnd);

    // 3D ビュースタイル (モックの TweaksPanel「3D ビュー / Viewport」相当)
    h->addWidget(new QLabel(I18n::tr("vp_style"), m_vpToolbar));
    m_styleBox = new QComboBox(m_vpToolbar);
    m_styleBox->addItem(I18n::tr("vp_style_wire"));   // ViewStyle::Wireframe
    m_styleBox->addItem(I18n::tr("vp_style_solid"));  // ViewStyle::Solid
    m_styleBox->addItem(I18n::tr("vp_style_field"));  // ViewStyle::Field
    m_styleBox->addItem(I18n::tr("vp_style_rays"));   // ViewStyle::Rays
    h->addWidget(m_styleBox);

    h->addWidget(new QLabel(I18n::tr("vp_rotlabel"), m_vpToolbar));
    m_azSlider = new QSlider(Qt::Horizontal, m_vpToolbar);
    m_azSlider->setRange(-180, 180);
    m_azSlider->setFixedWidth(80);
    m_azLabel = new QLabel(m_vpToolbar);
    m_azLabel->setFixedWidth(36);
    m_elSlider = new QSlider(Qt::Horizontal, m_vpToolbar);
    m_elSlider->setRange(-89, 89);
    m_elSlider->setFixedWidth(80);
    m_elLabel = new QLabel(m_vpToolbar);
    m_elLabel->setFixedWidth(36);
    h->addWidget(m_azSlider);
    h->addWidget(m_azLabel);
    h->addWidget(m_elSlider);
    h->addWidget(m_elLabel);

    h->addStretch(1);
    // 主平面プリセット
    const char *planeTag[3] = { "XY", "YZ", "ZX" };
    for (int i = 0; i < 3; ++i) {
        auto *b = new QToolButton(m_vpToolbar);
        b->setText(planeTag[i]);
        h->addWidget(b);
        connect(b, &QToolButton::clicked, this, [this, i] {
            m_viewport->setViewPlane(i);
        });
    }
    auto *snap = new QToolButton(m_vpToolbar);
    snap->setText(I18n::tr("vp_snapshot"));
    h->addWidget(snap);

    // ツールバーは中身が多く、そのままだと最小幅が広くなって QSplitter が
    // 左ペインを潰してしまう。横スクロール可の器へ入れて最小幅を切り離す。
    auto *tbScroll = new QScrollArea(this);
    tbScroll->setWidget(m_vpToolbar);
    tbScroll->setWidgetResizable(true);
    tbScroll->setFrameShape(QFrame::NoFrame);
    tbScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tbScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    // 高さはツールバー本体 + 横スクロールバーぶんを確保する
    // (これを足さないとスクロールバーが本体を食って中身が切れる)
    tbScroll->setFixedHeight(m_vpToolbar->sizeHint().height()
                             + tbScroll->horizontalScrollBar()->sizeHint().height()
                             + 2);
    v->addWidget(tbScroll);

    // ── ページスタック ──
    m_stack = new QStackedWidget(this);
    m_viewport = new Viewport3D(m_p, m_stack);
    m_heatmap  = new FieldHeatmap(m_stack);
    m_heatmap->setTitle(I18n::tr("vp_slice_title"));
    m_plot     = new PlotPanel(m_p, m_stack);
    m_mesh     = new MeshPreview(m_p, m_stack);
    m_stack->addWidget(m_viewport);
    m_stack->addWidget(m_heatmap);
    m_stack->addWidget(m_plot);
    m_stack->addWidget(m_mesh);
    v->addWidget(m_stack, 1);

    // ── 配線 ──
    connect(m_tabs, &QTabBar::currentChanged, this, &CenterPane::onTabChanged);
    connect(reset, &QToolButton::clicked, m_viewport, &Viewport3D::fitView);
    connect(grid, &QCheckBox::toggled, m_viewport, &Viewport3D::setGridVisible);
    connect(bnd,  &QCheckBox::toggled, m_viewport, &Viewport3D::setBoundaryVisible);
    connect(snap, &QToolButton::clicked, this, &CenterPane::saveSnapshot);
    connect(m_styleBox, &QComboBox::currentIndexChanged, this, [this](int i) {
        m_viewport->setViewStyle(ViewStyle(i));
        QSettings().setValue("ui/viewStyle", i);
    });
    // 既定は Solid (モックの TWEAK_DEFAULTS と同じ)
    const int vs = QSettings().value("ui/viewStyle", int(ViewStyle::Solid)).toInt();
    m_styleBox->setCurrentIndex(qBound(0, vs, 3));
    m_viewport->setViewStyle(ViewStyle(qBound(0, vs, 3)));

    connect(m_azSlider, &QSlider::valueChanged, this, [this](int val) {
        m_viewport->setAzimuth(val);
        m_azLabel->setText(QString::number(val) + "°");
    });
    connect(m_elSlider, &QSlider::valueChanged, this, [this](int val) {
        m_viewport->setElevation(val);
        m_elLabel->setText(QString::number(val) + "°");
    });
    // マウスで回したらスライダー側を追従させる (シグナル往復は block で防ぐ)
    connect(m_viewport, &Viewport3D::viewChanged, this,
            [this](double az, double el) {
        QSignalBlocker b1(m_azSlider), b2(m_elSlider);
        m_azSlider->setValue(qRound(az));
        m_elSlider->setValue(qRound(el));
        m_azLabel->setText(QString::number(qRound(az)) + "°");
        m_elLabel->setText(QString::number(qRound(el)) + "°");
    });

    // 初期値を Viewport3D の既定視点に合わせる
    m_azSlider->setValue(qRound(m_viewport->azimuth()));
    m_elSlider->setValue(qRound(m_viewport->elevation()));
    m_azLabel->setText(QString::number(qRound(m_viewport->azimuth())) + "°");
    m_elLabel->setText(QString::number(qRound(m_viewport->elevation())) + "°");

    onTabChanged(0);
}

void CenterPane::onTabChanged(int index)
{
    m_stack->setCurrentIndex(index);
    // ギズモ/回転操作は 3D シーンのみ意味を持つ
    m_vpToolbar->setEnabled(index == 0);
}

void CenterPane::setDomain(Domain d)
{
    m_viewport->setDomain(d);
    m_plot->setDomain(d);
}

void CenterPane::setViewStyleIndex(int i)
{
    m_styleBox->setCurrentIndex(qBound(0, i, 3));   // combo → setViewStyle へ伝播
}

void CenterPane::showViewport() { m_tabs->setCurrentIndex(0); }
void CenterPane::showPlot()     { m_tabs->setCurrentIndex(2); }

void CenterPane::saveSnapshot()
{
    const QString path = QFileDialog::getSaveFileName(
        this, I18n::tr("vp_saved"), "viewport.png", "PNG (*.png)");
    if (path.isEmpty()) return;
    m_stack->currentWidget()->grab().save(path);
}
