// CenterPane.cpp
#include "CenterPane.h"
#include "I18n.h"
#include "core/Project.h"
#include "io/H5Reader.h"
#include "widgets/FieldHeatmap.h"
#include "widgets/MeshPreview.h"
#include "widgets/PlotPanel.h"
#include "widgets/Viewport3D.h"

#include <QButtonGroup>
#include <algorithm>
#include <cmath>
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
    ofd::I18n::reg("vp_show",    "表示:",          "Show:");
    ofd::I18n::reg("vp_gizmo_notimpl",
                   "モード切替は未実装 (操作は Viewport のマウスドラッグ)",
                   "Mode switching not implemented (use mouse drag in the "
                   "viewport)");
    ofd::I18n::reg("vp_grid",    "グリッド",       "Grid");
    ofd::I18n::reg("vp_boundary","境界 (PML)",     "Boundary (PML)");
    ofd::I18n::reg("vp_vertex",  "頂点スナップ",   "Vertex snap");
    ofd::I18n::reg("vp_vertex_tip",
        "スナップ動作は未実装 (表示のみ)",
        "Snap behavior not implemented (display only)");
    ofd::I18n::reg("vp_rotlabel","Rotate:",        "Rotate:");
    ofd::I18n::reg("vp_style",      "3D:",   "3D:");
    ofd::I18n::reg("vp_style_wire", "Wire",  "Wire");
    ofd::I18n::reg("vp_style_solid","Solid", "Solid");
    ofd::I18n::reg("vp_style_field","+ Field","+ Field");
    ofd::I18n::reg("vp_style_rays", "+ Rays", "+ Rays");
    ofd::I18n::reg("vp_snapshot","📷 Snap",        "📷 Snap");
    ofd::I18n::reg("vp_slice_title",
        "近傍界面上分布 / Near-field slice", "Near-field slice");
    ofd::I18n::reg("vp_slice_result",
        "解析結果 %1 (正規化 |値|)", "Result %1 (normalised |value|)");
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

    // 操作ギズモ (モック由来のモード表示)。モード切替の実装が無いため
    // 無効表示にする — 押すとチェックが入って「切り替わった」ように見える
    // 状態にしない (実操作は Viewport3D のマウスドラッグ)。
    auto *gizmoGroup = new QButtonGroup(this);
    gizmoGroup->setExclusive(true);
    struct G { const char *glyph, *tipKey; };
    const G gizmos[] = { { "⊕", "vp_select" }, { "↔", "vp_move" },
                         { "⟳", "vp_rotate" }, { "⤢", "vp_scale" } };
    int gi = 0;
    for (const G &g : gizmos) {
        auto *b = new QToolButton(m_vpToolbar);
        b->setText(QString::fromUtf8(g.glyph));
        b->setToolTip(I18n::tr(g.tipKey) + QStringLiteral(" — ")
                      + I18n::tr("vp_gizmo_notimpl"));
        b->setCheckable(true);
        b->setEnabled(false);
        if (gi++ == 0) b->setChecked(true);
        gizmoGroup->addButton(b);
        h->addWidget(b);
    }

    // グリッド/境界は「表示」の切替 (実動作)。スナップではないので
    // ラベルを「表示:」にする。頂点スナップは未実装のため無効表示。
    h->addWidget(new QLabel(I18n::tr("vp_show"), m_vpToolbar));
    auto *grid = new QCheckBox(I18n::tr("vp_grid"), m_vpToolbar);
    grid->setChecked(true);
    auto *bnd = new QCheckBox(I18n::tr("vp_boundary"), m_vpToolbar);
    auto *vtx = new QCheckBox(I18n::tr("vp_vertex"), m_vpToolbar);
    vtx->setToolTip(I18n::tr("vp_vertex_tip"));
    vtx->setEnabled(false);
    h->addWidget(grid);
    h->addWidget(bnd);
    h->addWidget(vtx);

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
    // 既定は Solid (モックの TWEAK_DEFAULTS と同じ)。
    // 初期化時の setCurrentIndex で上の connect が走ると、ユーザーが何も
    // 触っていないのに QSettings へ書き込んでしまうので信号を止めておく。
    {
        const int vs = qBound(0,
            QSettings().value("ui/viewStyle", int(ViewStyle::Solid)).toInt(), 3);
        QSignalBlocker block(m_styleBox);
        m_styleBox->setCurrentIndex(vs);
        m_viewport->setViewStyle(ViewStyle(vs));
    }

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

// カーネルの HDF5 出力から 2D 断面へ実データを反映する。
// gui.md の規則どおり「その実行が生成したもの」だけを表示する判断は
// 呼び出し側 (MainWindow — 実行開始時刻と mtime の比較) が行う。
bool CenterPane::loadResultField(const QString &h5Path)
{
    if (!H5Reader::available()) return false;

    QVector<double> cells;
    int rows = 0, cols = 0;
    QString shown;

    // obpm の伝搬マップ |E(x,z)|^2 を最優先 (2D でそのまま表示できる)
    if (H5Reader::read2D(h5Path, QStringLiteral("/field/Ixz"),
                         cells, rows, cols)) {
        shown = QStringLiteral("/field/Ixz");
    } else {
        // 最終電界 |Efinal| = sqrt(re^2 + im^2)
        QVector<double> re, im;
        int r2 = 0, c2 = 0;
        if (H5Reader::read2D(h5Path, QStringLiteral("/field/Efinal_r"),
                             re, rows, cols)
            && H5Reader::read2D(h5Path, QStringLiteral("/field/Efinal_i"),
                                im, r2, c2)
            && r2 == rows && c2 == cols) {
            cells.resize(re.size());
            for (int i = 0; i < re.size(); ++i)
                cells[i] = std::sqrt(re[i] * re[i] + im[i] * im[i]);
            shown = QStringLiteral("|/field/Efinal|");
        } else {
            return false;   // 表示できる 2D 場が無い (ofd の /dataNNNNNN は未対応)
        }
    }

    // 0..1 へ正規化 (カラーバーの 0.0-1.0 表示と整合)
    double vmax = 0.0;
    for (const double v : cells) vmax = std::max(vmax, std::fabs(v));
    if (vmax > 0.0)
        for (double &v : cells) v = std::fabs(v) / vmax;

    m_heatmap->setData(cells, cols, rows);
    m_heatmap->setTitle(I18n::tr("vp_slice_result").arg(shown));
    return true;
}
