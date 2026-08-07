// CenterPane.cpp
#include "CenterPane.h"
#include "I18n.h"
#include "core/Project.h"
#include "io/H5Reader.h"
#include "widgets/FieldHeatmap.h"
#include "io/ShdReader.h"
#include "widgets/MeshPreview.h"
#include "widgets/PlotPanel.h"
#include "widgets/Viewport3D.h"

#include <QButtonGroup>
#include <algorithm>
#include <cmath>
#include <QCheckBox>
#include <QComboBox>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QFileDialog>
#include <QMimeData>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSettings>
#include <QScrollBar>
#include <QSlider>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QTabBar>
#include <QToolButton>
#include <QVBoxLayout>

using namespace ofd;

namespace {
const bool s_i18n = [] {
    ofd::I18n::reg("vp_3d",      "🧊 3D シーン",   "🧊 3D scene");
    ofd::I18n::reg("vp_2d",      "📐 2D 断面",     "📐 2D slice");
    // 水中音響の TL 断面 (色は「小さい TL = 暖色」に反転して表示する)
    ofd::I18n::reg("vp_slice_tl",
                   "伝搬損失 TL [dB] — 距離 × 深度 (表示レンジ %1〜%2 dB、"
                   "暖色ほどよく届く)",
                   "Transmission loss TL [dB] \u2014 range x depth (display range "
                   "%1-%2 dB; warmer = better reach)");
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
    // PML は EM/光の用語なので全ドメイン共通の中立語にする
    ofd::I18n::reg("vp_boundary","吸収境界",       "Boundary");
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
    // 「近傍界」は EM の用語 — 初期タイトルは全ドメイン中立の表現にする
    // (結果読込後は vp_slice_result で置き換わる)
    ofd::I18n::reg("vp_slice_title",
        "場の断面分布 / Field slice", "Field slice");
    ofd::I18n::reg("vp_slice_result",
        "解析結果 %1 (正規化 |値|)", "Result %1 (normalised |value|)");
    // 結果断面の 3D 重ね表示 (スタイルコンボの Field と同じ状態を指す)
    ofd::I18n::reg("vp_overlay", "結果断面を重ねる", "Overlay result slice");
    ofd::I18n::reg("vp_overlay_tip",
        "3D シーンにソルバ出力の断面 (実データ) を重ねて表示する "
        "(3D スタイル「+ Field」と同じ状態)",
        "Overlay the solver's result slice (actual data) on the 3D scene "
        "(same state as the 3D style \"+ Field\")");
    ofd::I18n::reg("vp_overlay_none",
        "結果断面が未読込 — 計算を実行するか、結果 HDF5 "
        "(time_series_data.h5) のあるプロジェクトを開くと有効になります",
        "No result slice loaded — run the solver, or open a project whose "
        "folder contains a result HDF5 (time_series_data.h5)");
    ofd::I18n::reg("vp_slice3d_label",
        "%1: |E| z 中央断面 (%2)", "%1: |E| z-mid slice (%2)");
    ofd::I18n::reg("vp_slice3d_nofield",
        "3D 空間へ配置できる節点場 (ofd/orcwa の |E|) がありません (%1)",
        "No node field that can be placed in 3D space (|E| of ofd/orcwa) "
        "(%1)");
    ofd::I18n::reg("vp_slice3d_nogeom",
        "節点座標 (Xn/Yn/Zn) が無いため 3D 空間へ配置できません",
        "No grid coordinates (Xn/Yn/Zn), so it cannot be placed in 3D space");
    ofd::I18n::reg("vp_slice3d_mismatch",
        "節点座標の数が断面の格子と一致しません (%1x%2 に対し %3x%4)",
        "Grid coordinate counts do not match the slice (%3x%4 for a %1x%2 "
        "slice)");
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

    // 結果断面の 3D 重ね表示。スタイルコンボの「+ Field」と同じ状態を指す
    // (状態は m_styleBox 側が唯一の持ち主で、これはその別入口)。
    // 結果が無いうちは無効 + 理由をツールチップに出す。
    m_overlayCheck = new QCheckBox(I18n::tr("vp_overlay"), m_vpToolbar);
    h->addWidget(m_overlayCheck);

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
        if (i != int(ViewStyle::Field)) m_prevStyleIndex = i;
        // トグルはスタイルの表示であって別状態ではない (二重管理しない)
        QSignalBlocker block(m_overlayCheck);
        m_overlayCheck->setChecked(i == int(ViewStyle::Field));
    });
    // トグル ON → スタイルを Field に、OFF → Field 以外の直前のスタイルへ
    connect(m_overlayCheck, &QCheckBox::toggled, this, [this](bool on) {
        m_styleBox->setCurrentIndex(on ? int(ViewStyle::Field)
                                       : m_prevStyleIndex);
    });
    // 既定は Solid (モックの TWEAK_DEFAULTS と同じ)。
    // 初期化時の setCurrentIndex で上の connect が走ると、ユーザーが何も
    // 触っていないのに QSettings へ書き込んでしまうので信号を止めておく。
    {
        const int vs = qBound(0,
            QSettings().value("ui/viewStyle", int(ViewStyle::Solid)).toInt(), 3);
        QSignalBlocker block(m_styleBox);
        QSignalBlocker block2(m_overlayCheck);
        m_styleBox->setCurrentIndex(vs);
        m_viewport->setViewStyle(ViewStyle(vs));
        m_prevStyleIndex = (vs == int(ViewStyle::Field)) ? int(ViewStyle::Solid)
                                                         : vs;
        m_overlayCheck->setChecked(vs == int(ViewStyle::Field));
    }
    updateOverlayUi();   // 起動直後は結果が無いので無効 + 理由を出す

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

    // ドメイン切替でも出し分けを更新 (MainWindow::onDomainChanged →
    // setDomain 経由でも呼ばれるが、直結しておくと経路に依存しない)
    connect(project, &Project::domainChanged, this, [this](Domain d) {
        updateDomainVisibility(d);
    });

    onTabChanged(0);
    updateDomainVisibility(m_p->activeDomain());   // 初回反映

    // コンポーネントのドラッグを受け取れるようにする (下の dragEnterEvent。
    // 実際のドロップ先は Viewport3D で、ここは 3D シーンへの切替だけを行う)
    setAcceptDrops(true);
}

// コンポーネントライブラリのカードを 2D 断面などへドラッグしてきたときに、
// 配置先の「🧊 3D シーン」へ自動で切り替える (Ansys 系と同じ挙動)。
// 切り替えるだけでドロップは受けない — 受けるのは Viewport3D。
void CenterPane::handleComponentDragOver(QDragMoveEvent *e)
{
    if (!e->mimeData()->hasFormat(ComponentDrop::mimeType())) {
        e->ignore();
        return;
    }
    if (m_tabs->currentIndex() != 0) m_tabs->setCurrentIndex(0);
    e->ignore();   // カーソルが 3D シーンへ入った時点で Viewport3D が受ける
}

void CenterPane::dragEnterEvent(QDragEnterEvent *e) { handleComponentDragOver(e); }
void CenterPane::dragMoveEvent(QDragMoveEvent *e)   { handleComponentDragOver(e); }

// ドメインで意味を持たない UI 項目の出し分け。
// 現状は 3D ビュースタイル「+ Rays」のみ — 光 (ビーム経路) と
// 水中 (BELLHOP の音線) でだけ意味を持つので、EM/室内音響では
// コンボの項目自体を隠す。
void CenterPane::updateDomainVisibility(Domain d)
{
    const bool raysOk = (d == Domain::Optical || d == Domain::Underwater);
    const int rayRow = int(ViewStyle::Rays);

    // ポップアップの行を隠し、キーボード循環でも選ばれないよう無効化する
    if (auto *view = qobject_cast<QListView *>(m_styleBox->view()))
        view->setRowHidden(rayRow, !raysOk);
    if (auto *model = qobject_cast<QStandardItemModel *>(m_styleBox->model())) {
        if (QStandardItem *item = model->item(rayRow))
            item->setFlags(raysOk
                ? (item->flags() | Qt::ItemIsEnabled)
                : (item->flags() & ~Qt::ItemIsEnabled));
    }

    // 「結果断面を重ねる」を外したときの戻り先が Rays のままだと、Rays を
    // 出さないドメインで復活してしまう。隠したら戻り先も Solid にする。
    if (!raysOk && m_prevStyleIndex == rayRow)
        m_prevStyleIndex = int(ViewStyle::Solid);

    // 選択中に隠れた場合は既定スタイル (Solid) へフォールバック。
    // ユーザー操作ではないので QSettings へは書き込まない (シグナル抑止)。
    if (!raysOk && m_styleBox->currentIndex() == rayRow) {
        QSignalBlocker block(m_styleBox);
        m_styleBox->setCurrentIndex(int(ViewStyle::Solid));
        m_viewport->setViewStyle(ViewStyle::Solid);
    }
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
    updateDomainVisibility(d);
}

void CenterPane::setViewStyleIndex(int i)
{
    m_styleBox->setCurrentIndex(qBound(0, i, 3));   // combo → setViewStyle へ伝播
}

void CenterPane::showViewport() { m_tabs->setCurrentIndex(0); }
void CenterPane::showPlot()     { m_tabs->setCurrentIndex(2); }

void CenterPane::selectTabContaining(const QString &titlePart)
{
    for (int i = 0; i < m_tabs->count(); ++i)
        if (m_tabs->tabText(i).contains(titlePart, Qt::CaseInsensitive)) {
            m_tabs->setCurrentIndex(i);
            return;
        }
}

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
    // 中身が HDF5 でなければここで止める。この後は同じファイルに対して
    // データセットを最大 6 回試すので、判定しないと同じ失敗を繰り返す。
    if (!H5Reader::isHdf5(h5Path)) return false;

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
            // ofd/orcwa のノード場 → z 中央断面 |E| (io/H5Reader が再構成)
            QString group;
            if (!H5Reader::readOfdMidSlice(h5Path, cells, rows, cols, &group))
                return false;   // 表示できる 2D 場が無い
            shown = QStringLiteral("|E| z-mid (%1)").arg(group);
        }
    }

    // 0..1 へ正規化 (カラーバーの 0.0-1.0 表示と整合)
    double vmax = 0.0;
    for (const double v : cells) vmax = std::max(vmax, std::fabs(v));
    if (vmax > 0.0)
        for (double &v : cells) v = std::fabs(v) / vmax;

    m_heatmap->setData(cells, cols, rows);
    m_heatmap->setTitle(I18n::tr("vp_slice_result").arg(shown));

    // 同じ結果を 3D シーンにも重ねる (節点座標が取れる ofd/orcwa 系のみ)
    QString why;
    const bool ok3d = applyResultSliceTo3D(h5Path, &why);
    emit result3DSliceStatus(ok3d, why);
    return true;
}

// 水中音響 (bellhopcxx) の TL 音場 (<ケース名>.shd) を 2D 断面へ反映する。
bool CenterPane::loadTlField(const QString &shdPath)
{
    ShdField f;
    QString err;
    if (!ShdReader::read(shdPath, f, &err)) return false;

    // TL は「小さいほどよく届く」ので、表示は 0 = 届かない / 1 = よく届く
    // に反転する (他ドメインの |E| 断面と見え方の向きを揃えるため)。
    // 表示レンジは最小 TL から 60 dB (TL 図の慣用幅)。
    const double lo = f.minTL, hi = std::min(f.maxTL, f.minTL + 60.0);
    const double span = std::max(1.0, hi - lo);
    QVector<double> cells(f.tl_dB.size());
    for (int i = 0; i < f.tl_dB.size(); ++i) {
        const float tl = f.tl_dB[i];
        cells[i] = (tl >= ShdField::kNoField)
                       ? 0.0                                   // 到達なし
                       : std::clamp((hi - tl) / span, 0.0, 1.0);
    }
    m_heatmap->setData(cells, f.nrr, f.nrz);
    m_heatmap->setTitle(I18n::tr("vp_slice_tl")
                            .arg(f.minTL, 0, 'f', 1).arg(hi, 0, 'f', 1));
    m_tabs->setCurrentIndex(1);   // 2D 断面へ切り替える
    return true;
}

// プロジェクトを開いたときに見つかった既存 HDF5 用 — 3D シーンだけへ流す。
// 2D 断面は「その実行が生成したもの」に限る (呼び出し側のゲート) ので
// ここでは触らない。どのファイルの結果かは断面の凡例に出る。
bool CenterPane::loadResult3DSlice(const QString &h5Path)
{
    QString why;
    const bool ok = applyResultSliceTo3D(h5Path, &why);
    emit result3DSliceStatus(ok, why);
    return ok;
}

// HDF5 の z 中央断面 (実データ) を 3D 空間の該当平面へ置く。
// 位置と寸法は節点座標 [m] から決める。取れないファイル (obpm の
// /field/Ixz など格子情報を持たない出力) では 3D へ渡さない —
// 座標不明の断面を適当な位置に置くと結果の読み違いになるため。
bool CenterPane::applyResultSliceTo3D(const QString &h5Path, QString *why)
{
    const auto fail = [&](const QString &msg) {
        if (why) *why = msg;
        m_viewport->clearResultSlice();
        updateOverlayUi();
        return false;
    };
    if (!H5Reader::available())
        return fail(QStringLiteral("HDF5 disabled (USE_HDF5=OFF)"));
    if (!H5Reader::isHdf5(h5Path))
        return fail(QStringLiteral("%1 is not an HDF5 file").arg(h5Path));

    // 2D 断面と同じ再構成 (z 中央断面の |E|)。正規化前の実値を使う
    // (Viewport3D 側が最大値で正規化し、その最大値を凡例に出す)。
    QVector<double> cells;
    int rows = 0, cols = 0;
    QString group, err;
    if (!H5Reader::readOfdMidSlice(h5Path, cells, rows, cols, &group, &err))
        return fail(I18n::tr("vp_slice3d_nofield").arg(err));

    QVector<double> xs, ys, zs;
    if (!H5Reader::ofdGridCoords(h5Path, xs, ys, zs, &err)
        || xs.size() < 2 || ys.size() < 2 || zs.isEmpty())
        return fail(I18n::tr("vp_slice3d_nogeom"));
    // 断面 (rows = Ny+1, cols = Nx+1) と節点数が食い違うファイルは
    // 対応が取れないので置かない (寸法を推測しない)
    if (xs.size() != cols || ys.size() != rows)
        return fail(I18n::tr("vp_slice3d_mismatch")
                        .arg(cols).arg(rows).arg(xs.size()).arg(ys.size()));

    // readOfdMidSlice が使う固定 k は (Nz+1-1)/2 (新旧レイアウト共通)
    const double zmid = zs[(zs.size() - 1) / 2];
    const QString label = I18n::tr("vp_slice3d_label")
                              .arg(QFileInfo(h5Path).fileName(), group);
    // axis=2 (XY 面): 面内 第1軸 = x (列)、第2軸 = y (行、行 0 = +y 側)
    m_viewport->setResultSlice(cells, rows, cols, 2, zmid,
                               xs.first(), xs.last(),
                               ys.first(), ys.last(), label);
    if (why) why->clear();
    updateOverlayUi();
    return m_viewport->hasResultSlice();
}

bool CenterPane::hasResult3DSlice() const
{
    return m_viewport->hasResultSlice();
}

// 新規/別プロジェクト — 前の実行の残骸を結果として見せない。
// 2D ヒートマップは実データを解析パターンへ戻す API を持たないので、
// タイトルで「前に開いていたファイルの結果」と明示する。
void CenterPane::clearResultField()
{
    m_viewport->clearResultSlice();
    updateOverlayUi();
    // 2D 断面も実データを捨ててプレースホルダへ戻す。前のプロジェクトの
    // 結果を新しいプロジェクトの結果と誤読させない (gui.md の規則)。
    m_heatmap->clearData();
    m_heatmap->setTitle(I18n::tr("vp_slice_title"));
}

// 「結果断面を重ねる」トグルの有効条件 = 3D に載せられる実データがあること
void CenterPane::updateOverlayUi()
{
    const bool ok = m_viewport->hasResultSlice();
    m_overlayCheck->setEnabled(ok);
    m_overlayCheck->setToolTip(ok ? I18n::tr("vp_overlay_tip")
                                  : I18n::tr("vp_overlay_none"));
}
