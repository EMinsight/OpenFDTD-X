// H5ViewerTab.cpp
#include "H5ViewerTab.h"
#include "../core/Project.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"
#include "TabHelpers.h"

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QFileDialog>
#include <QFont>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLinearGradient>
#include <QPainter>
#include <QPolygonF>
#include <QPushButton>
#include <QSlider>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

using namespace ofd;

namespace {
// タブ専用語彙 (接頭辞 h5_) — file-local 登録
const bool s_i18n = [] {
    ofd::I18n::reg("h5_file_section", "HDF5 ファイル", "HDF5 File");
    ofd::I18n::reg("h5_file", "ファイル", "File");
    ofd::I18n::reg("h5_browse", "📁 参照…", "📁 Browse…");
    ofd::I18n::reg("h5_reload", "↻ 再読込", "↻ Reload");
    ofd::I18n::reg("h5_file_ph", "例: patch.h5 (読取は未実装)",
                   "e.g. patch.h5 (reading not implemented)");
    ofd::I18n::reg("h5_demo_banner",
                   "デモ表示 — 実データ未読込 (HDF5 読取は未実装)",
                   "Demo display — no real data loaded (HDF5 reading not "
                   "implemented)");
    ofd::I18n::reg("h5_formats", "対応形式", "Supported formats");
    ofd::I18n::reg("h5_fmt_ofdout", ".ofd.out → 変換", ".ofd.out → convert");
    ofd::I18n::reg("h5_file_hint",
        "▸ HDF5 の実ファイル読取は未実装 — このタブは表示設計のプレビューです "
        "(ツリー・可視化はサンプルデータ)",
        "▸ Reading actual HDF5 files is not implemented yet — this tab is a "
        "preview of the viewer design (tree and plots show sample data)");
    ofd::I18n::reg("h5_tree_section", "データセット", "Dataset tree");
    ofd::I18n::reg("h5_tree_hint",
        "HDFView 風のツリー表示 (固定サンプル)。実ファイルの解析は未実装。",
        "HDFView-style tree (fixed sample). Parsing a real file is not "
        "implemented yet.");
    ofd::I18n::reg("h5_selected", "選択中:", "Selected:");
    ofd::I18n::reg("h5_vis_section", "可視化", "Visualization");
    ofd::I18n::reg("h5_view_mode", "表示方式", "View mode");
    ofd::I18n::reg("h5_view_heatmap", "ヒートマップ", "Heatmap");
    ofd::I18n::reg("h5_view_contour", "等高線", "Contour");
    ofd::I18n::reg("h5_view_vector", "ベクトル場", "Vector field");
    ofd::I18n::reg("h5_view_iso3d", "3D等値面", "3D isosurface");
    ofd::I18n::reg("h5_view_line", "線プロット", "Line plot");
    ofd::I18n::reg("h5_colormap", "カラーマップ", "Colormap");
    ofd::I18n::reg("h5_scale", "スケール", "Scale");
    ofd::I18n::reg("h5_auto", "自動", "Auto");
    ofd::I18n::reg("h5_iso_level", "等値面レベル", "Isosurface level");
    ofd::I18n::reg("h5_iso_text", "3D 等値面", "3D isosurface");
    ofd::I18n::reg("h5_show_grid", "グリッド表示", "Show grid");
    ofd::I18n::reg("h5_show_axes", "軸ラベル", "Axis labels");
    ofd::I18n::reg("h5_overlay_geom", "物体オーバーレイ", "Geometry overlay");
    ofd::I18n::reg("h5_overlay_mon", "モニター位置", "Monitor positions");
    ofd::I18n::reg("h5_preview", "プレビュー", "Preview");
    ofd::I18n::reg("h5_playback", "再生コントロール", "Playback");
    ofd::I18n::reg("h5_play", "▶ 再生", "▶ Play");
    ofd::I18n::reg("h5_pause", "⏸ 一時停止", "⏸ Pause");
    ofd::I18n::reg("h5_first", "⏮ 先頭", "⏮ First");
    ofd::I18n::reg("h5_prev", "◀ 前", "◀ Prev");
    ofd::I18n::reg("h5_next", "次 ▶", "Next ▶");
    ofd::I18n::reg("h5_last", "末尾 ⏭", "Last ⏭");
    ofd::I18n::reg("h5_loop", "⟳ ループ", "⟳ Loop");
    ofd::I18n::reg("h5_frame", "フレーム", "Frame");
    ofd::I18n::reg("h5_speed", "速度", "Speed");
    ofd::I18n::reg("h5_time_range", "時間範囲", "Time range");
    ofd::I18n::reg("h5_range_only", "範囲限定再生", "Play range only");
    ofd::I18n::reg("h5_xsec_section", "時間断面", "Cross-sections (XY/XZ/YZ)");
    ofd::I18n::reg("h5_sec_xy", "XY 面 (Z=固定)", "XY plane (Z fixed)");
    ofd::I18n::reg("h5_sec_xz", "XZ 面 (Y=固定)", "XZ plane (Y fixed)");
    ofd::I18n::reg("h5_sec_yz", "YZ 面 (X=固定)", "YZ plane (X fixed)");
    ofd::I18n::reg("h5_sec_pos", "位置", "Position");
    ofd::I18n::reg("h5_sec_multi", "複数断面同時表示 (3面ビュー)",
                   "Show multiple sections (3-plane view)");
    ofd::I18n::reg("h5_export", "エクスポート", "Export");
    ofd::I18n::reg("h5_exp_mp4", "🎥 MP4 動画", "🎥 MP4 movie");
    ofd::I18n::reg("h5_exp_gif", "🎞 GIF アニメ", "🎞 GIF animation");
    ofd::I18n::reg("h5_exp_png", "📸 PNG (現フレーム)", "📸 PNG (current frame)");
    ofd::I18n::reg("h5_exp_pngseq", "🖼 PNG 連番 (全フレーム)",
                   "🖼 PNG sequence (all frames)");
    ofd::I18n::reg("h5_exp_csv", "📊 CSV (時系列)", "📊 CSV (time series)");
    ofd::I18n::reg("h5_movie", "動画設定", "Movie settings");
    ofd::I18n::reg("h5_resolution", "解像度", "Resolution");
    ofd::I18n::reg("h5_codec", "コーデック", "Codec");
    ofd::I18n::reg("h5_embed_bar", "タイムスタンプ・カラーバー埋込",
                   "Embed timestamp && colorbar");
    ofd::I18n::reg("h5_embed_geom", "物体オーバーレイ埋込",
                   "Embed geometry overlay");
    ofd::I18n::reg("h5_stats_section", "統計・派生量", "Statistics && derived");
    ofd::I18n::reg("h5_stat_energy", "時間平均エネルギー: 4.8e-3 J/m³",
                   "Time-avg energy: 4.8e-3 J/m³");
    ofd::I18n::reg("h5_stat_series", "📈 全体時系列 (max/RMS)",
                   "📈 Overall time series (max/RMS)");
    ofd::I18n::reg("h5_stat_schroeder", "📈 Schroeder減衰", "📈 Schroeder decay");
    ofd::I18n::reg("h5_stat_fft", "📈 FFT スペクトログラム", "📈 FFT spectrogram");
    ofd::I18n::reg("h5_stat_lineint", "📈 線積分", "📈 Line integral");
    ofd::I18n::reg("h5_integration", "連携", "Integration");
    ofd::I18n::reg("h5_int_python", "🐍 Python (h5py) で開く",
                   "🐍 Open in Python (h5py)");
    ofd::I18n::reg("h5_int_paraview", "🌐 ParaView ファイル出力 (.vtk)",
                   "🌐 ParaView file export (.vtk)");
    ofd::I18n::reg("h5_int_matlab", "📦 Matlab .mat 変換",
                   "📦 Convert to Matlab .mat");
    return true;
}();

const int    kTotalFrames = 240;
const double kSpeeds[5] = { 0.25, 0.5, 1.0, 2.0, 5.0 };
const double kPi = 3.14159265358979323846;

// バッジ風ラベル (mock .badge 相当, 最小限のスタイル)
QLabel *makeBadge(const QString &text, QWidget *parent, const char *kind = "")
{
    auto *l = new QLabel(text, parent);
    const char *color = "#8A8A8A";
    if (qstrcmp(kind, "ok") == 0)  color = "#2E8B57";
    if (qstrcmp(kind, "acc") == 0) color = "#0078D4";
    l->setStyleSheet(QString("padding:1px 6px; border:1px solid %1; "
                             "border-radius:3px; color:%1; font-size:11px;")
                         .arg(QString::fromUtf8(color)));
    return l;
}
} // namespace

// ── FieldCanvas ─────────────────────────────────────────────────────────────
FieldCanvas::FieldCanvas(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(320, 320);
}

// mock の colormap_fn をそのまま転記
QColor FieldCanvas::mapColor(double v) const
{
    double t = (v - m_lo) / (m_hi - m_lo);
    t = std::max(0.0, std::min(1.0, t));
    auto c255 = [](double x) {
        return int(std::lround(255.0 * std::max(0.0, std::min(1.0, x))));
    };
    switch (m_cmap) {
    case 0:   // jet
        return QColor(c255(1.5 - std::fabs(4.0 * t - 3.0)),
                      c255(1.5 - std::fabs(4.0 * t - 2.0)),
                      c255(1.5 - std::fabs(4.0 * t - 1.0)));
    case 1:   // viridis (近似)
        return QColor(c255(0.27 + 0.65 * t),
                      c255(0.0 + 0.9 * t),
                      c255(0.33 + 0.4 * (1.0 - t)));
    case 2:   // seismic: blue-white-red (diverging)
        if (t < 0.5) {
            const int k = c255(t * 2.0);
            return QColor(k, k, 255);
        } else {
            const int k = c255((1.0 - t) * 2.0);
            return QColor(255, k, k);
        }
    default: {  // grayscale
        const int g = c255(t);
        return QColor(g, g, g);
    }
    }
}

void FieldCanvas::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    const double W = width(), H = height();
    p.fillRect(rect(), Qt::black);

    const double sx = W / 50.0, sy = H / 50.0;
    const double t = double(m_frame) / kTotalFrames * 2.0 * kPi * 4.0;

    if (m_view == 0 || m_view == 1) {
        // ヒートマップ / 等高線: v = sin(4r - t)·exp(-0.4r)
        p.setPen(Qt::NoPen);
        for (int i = 0; i < 50; ++i) {
            for (int j = 0; j < 50; ++j) {
                const double x = (i - 25.0) / 50.0 * 4.0;
                const double y = (j - 25.0) / 50.0 * 4.0;
                const double r = std::sqrt(x * x + y * y);
                const double v = std::sin(r * 4.0 - t) * std::exp(-r * 0.4);
                QColor c = mapColor(v);
                if (m_view == 1 && std::fabs(v) < 0.05)
                    c.setAlphaF(0.3);   // 等高線: ゼロ交差付近を暗く
                p.fillRect(QRectF(i * sx, j * sy, 1.05 * sx, 1.05 * sy), c);
            }
        }
    } else if (m_view == 2) {
        // ベクトル場: mag = 0.6·cos(4r - t)·exp(-0.4r)
        p.setRenderHint(QPainter::Antialiasing);
        QPen pen(QColor("#7AB2FF"));
        pen.setWidthF(0.2 * sx);
        p.setPen(pen);
        for (int i = 2; i < 50; i += 5) {
            for (int j = 2; j < 50; j += 5) {
                const double x = (i - 25.0) / 25.0 * 4.0;
                const double y = (j - 25.0) / 25.0 * 4.0;
                const double r = std::sqrt(x * x + y * y) + 0.01;
                const double mag = 0.6 * std::cos(r * 4.0 - t) * std::exp(-r * 0.4);
                const double dx = mag * x / r * 3.0;
                const double dy = mag * y / r * 3.0;
                p.drawLine(QPointF(i * sx, j * sy),
                           QPointF((i + dx) * sx, (j + dy) * sy));
            }
        }
    } else if (m_view == 3) {
        // 3D等値面 (プレースホルダテキスト)
        p.setPen(QColor("#7AB2FF"));
        QFont f = p.font();
        f.setPixelSize(std::max(10, int(3.0 * sy)));
        p.setFont(f);
        p.drawText(rect(), Qt::AlignCenter,
            I18n::tr("h5_iso_text") + QString(" (iso=%1)").arg(m_iso, 0, 'f', 2));
    } else {
        // 線プロット: y = 25 - 15·sin(0.3x - t)·exp(-0.05|x|)
        p.setRenderHint(QPainter::Antialiasing);
        QPen pen(QColor("#7AB2FF"));
        pen.setWidthF(0.4 * sx);
        p.setPen(pen);
        QPolygonF poly;
        for (int i = 0; i < 50; ++i) {
            const double x = i - 25.0;
            const double yv = 25.0
                - 15.0 * std::sin(x * 0.3 - t) * std::exp(-std::fabs(x) * 0.05);
            poly << QPointF(i * sx, yv * sy);
        }
        p.setBrush(Qt::NoBrush);
        p.drawPolyline(poly);
    }

    if (m_grid) {
        QPen pen(QColor(255, 255, 255, 38));   // rgba(255,255,255,0.15)
        pen.setWidthF(std::max(0.5, 0.1 * sx));
        p.setPen(pen);
        for (int g : { 10, 20, 30, 40 }) {
            p.drawLine(QPointF(g * sx, 0), QPointF(g * sx, H));
            p.drawLine(QPointF(0, g * sy), QPointF(W, g * sy));
        }
    }
    if (m_axes) {
        p.setPen(QColor(255, 255, 255, 128));
        QFont f = p.font();
        f.setPixelSize(std::max(8, int(2.0 * sy)));
        p.setFont(f);
        p.drawText(QPointF(0.5 * sx, 49.0 * sy), "-15mm");
        p.drawText(QPointF(44.0 * sx, 49.0 * sy), "+15mm");
        p.drawText(QPointF(0.5 * sx, 3.0 * sy), "+15mm");
    }

    // 左上オーバーレイ: t [ps] とデータセット名
    QFont f = p.font();
    f.setPixelSize(10);
    p.setFont(f);
    const QString l1 = QString("t = %1 ps").arg(m_frame * 4.17, 0, 'f', 2);
    const QString l2 = m_name;
    const QFontMetrics fm(p.font());
    const int tw = std::max(fm.horizontalAdvance(l1), fm.horizontalAdvance(l2));
    p.fillRect(QRectF(8, 8, tw + 12, 2 * fm.height() + 6), QColor(0, 0, 0, 102));
    p.setPen(Qt::white);
    p.drawText(QPointF(14, 8 + fm.ascent() + 2), l1);
    p.drawText(QPointF(14, 8 + fm.height() + fm.ascent() + 3), l2);

    // ── デモ表示の明示バナー (描画は解析式 — 実データではない) ──
    // FieldHeatmap の m_demo バナーと同じ方式。HDF5 読取実装まで常時表示。
    {
        const QRect band(0, height() - 24, width(), 24);
        p.fillRect(band, QColor(0, 0, 0, 170));
        p.setPen(QColor("#FFD54F"));
        QFont bf = p.font();
        bf.setPixelSize(11);
        p.setFont(bf);
        p.drawText(band.adjusted(6, 0, -6, 0),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   I18n::tr("h5_demo_banner"));
    }

    // 枠線
    p.setPen(QPen(palette().mid().color(), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(rect().adjusted(0, 0, -1, -1));
}

// ── ColorBar ────────────────────────────────────────────────────────────────
ColorBar::ColorBar(QWidget *parent)
    : QWidget(parent)
{
    setFixedWidth(18);
    setMinimumHeight(120);
}

void ColorBar::paintEvent(QPaintEvent *)
{
    // mock の linear-gradient(to top, …) を転記
    static const char *kStops[4][6] = {
        { "#000080", "#0000ff", "#00ffff", "#ffff00", "#ff0000", "#800000" },
        { "#440154", "#3b528b", "#21918c", "#5ec962", "#fde725", nullptr },
        { "#0000ff", "#ffffff", "#ff0000", nullptr,   nullptr,   nullptr },
        { "#000000", "#ffffff", nullptr,   nullptr,   nullptr,   nullptr },
    };
    const int cmap = qBound(0, m_cmap, 3);
    int n = 0;
    while (n < 6 && kStops[cmap][n]) ++n;

    QPainter p(this);
    QLinearGradient grad(0, height(), 0, 0);   // 下→上
    for (int i = 0; i < n; ++i)
        grad.setColorAt(n <= 1 ? 0.0 : double(i) / (n - 1),
                        QColor(kStops[cmap][i]));
    p.fillRect(rect(), grad);
    p.setPen(QPen(palette().mid().color(), 1));
    p.drawRect(rect().adjusted(0, 0, -1, -1));
}

// ── H5ViewerTab ─────────────────────────────────────────────────────────────
H5ViewerTab::H5ViewerTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // HDF5 ファイル / File
    auto *sf = new SectionBox(I18n::tr("h5_file_section"), body);
    auto *frow = new QHBoxLayout();
    // 既定値 "patch.h5" は読める見た目になるので空 + placeholder にする
    m_file = new QLineEdit(sf);
    m_file->setPlaceholderText(I18n::tr("h5_file_ph"));
    frow->addWidget(m_file, 1);
    // 参照… はファイル選択のみ実配線 (読取は未実装)。再読込は未実装で無効化。
    auto *browseBtn = new QPushButton(I18n::tr("h5_browse"), sf);
    connect(browseBtn, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, I18n::tr("h5_file_section"), {},
            "HDF5 (*.h5 *.hdf5);;All files (*)");
        if (!path.isEmpty()) m_file->setText(path);
    });
    frow->addWidget(browseBtn);
    auto *reloadBtn = new QPushButton(I18n::tr("h5_reload"), sf);
    ofd::tabhelp::markNotImplemented(reloadBtn);
    frow->addWidget(reloadBtn);
    sf->form()->addRow(I18n::tr("h5_file"), frow);
    auto *fmts = new QHBoxLayout();
    fmts->addWidget(makeBadge(".h5", sf));
    fmts->addWidget(makeBadge(".hdf5", sf));
    fmts->addWidget(makeBadge(".nc (NetCDF4)", sf));
    fmts->addWidget(makeBadge(I18n::tr("h5_fmt_ofdout"), sf));
    fmts->addStretch(1);
    sf->form()->addRow(I18n::tr("h5_formats"), fmts);
    auto *fhint = new QLabel(I18n::tr("h5_file_hint"), sf);
    fhint->setWordWrap(true);
    sf->vbox()->addWidget(fhint);
    v->addWidget(sf);

    // データセット / Dataset tree (HDFView 風)
    auto *st = new SectionBox(I18n::tr("h5_tree_section"), body);
    auto *thint = new QLabel(I18n::tr("h5_tree_hint"), st);
    thint->setWordWrap(true);
    st->vbox()->addWidget(thint);
    m_tree = new QTreeWidget(st);
    m_tree->setColumnCount(2);
    m_tree->setHeaderHidden(true);
    m_tree->setIndentation(14);
    m_tree->setMaximumHeight(240);
    m_tree->setMinimumHeight(240);
    auto node = [](QTreeWidgetItem *parent, QTreeWidget *tree, const char *icon,
                   const char *name, const char *info, const char *path = nullptr) {
        auto *it = parent
            ? new QTreeWidgetItem(parent)
            : new QTreeWidgetItem(tree);
        it->setText(0, QString::fromUtf8(icon) + " " + QString::fromUtf8(name));
        it->setText(1, QString::fromUtf8(info));
        it->setForeground(1, QColor("#888888"));
        if (path)   // データセットのみクリックで選択可
            it->setData(0, Qt::UserRole, QString::fromUtf8(path));
        return it;
    };
    auto *root = node(nullptr, m_tree, "📁", "patch.h5", "168.4 MB · HDF5 1.14");
    auto *meta = node(root, m_tree, "📁", "meta", "プロジェクト情報");
    node(meta, m_tree, "·", "title", "\"patch_antenna_2.5GHz\"");
    node(meta, m_tree, "·", "solver", "\"OpenFDTD 5.0\"");
    node(meta, m_tree, "·", "timestamp", "\"2026-05-28T14:23:01\"");
    auto *grid = node(root, m_tree, "📁", "grid", "座標系");
    node(grid, m_tree, "🗂", "x", "float64 · (31,)", "/grid/x");
    node(grid, m_tree, "🗂", "y", "float64 · (31,)", "/grid/y");
    node(grid, m_tree, "🗂", "z", "float64 · (31,)", "/grid/z");
    node(grid, m_tree, "🗂", "t", "float64 · (240,) · Δt=9.31e-13s", "/grid/t");
    auto *mons = node(root, m_tree, "📁", "monitors", "");
    auto *esurf = node(mons, m_tree, "🗂", "E_surface",
                       "complex64 · (240,30,30) · |E|, V/m", "/monitors/E_surface");
    node(mons, m_tree, "🗂", "H_surface",
         "complex64 · (240,30,30) · |H|, A/m", "/monitors/H_surface");
    node(mons, m_tree, "🗂", "E_volume",
         "complex64 · (240,30,30,31) · 3D", "/monitors/E_volume");
    node(mons, m_tree, "🗂", "feed_wave",
         "float64 · (1000,) · 給電点時間波形", "/monitors/feed_wave");
    node(mons, m_tree, "🗂", "probe_1",
         "float64 · (1000,) · 観測点時間波形", "/monitors/probe_1");
    auto *res = node(root, m_tree, "📁", "results", "");
    node(res, m_tree, "🗂", "S11", "complex64 · (100,) · 周波数応答", "/results/S11");
    node(res, m_tree, "🗂", "impedance", "complex64 · (100,)", "/results/impedance");
    node(res, m_tree, "🗂", "far_field",
         "float64 · (36,72) · 遠方界(θ,φ)", "/results/far_field");
    root->setExpanded(true);
    meta->setExpanded(false);
    grid->setExpanded(true);
    mons->setExpanded(true);
    res->setExpanded(true);
    m_tree->setCurrentItem(esurf);
    m_tree->resizeColumnToContents(0);
    st->vbox()->addWidget(m_tree);
    // ツリー内容は固定サンプル (実ファイルの読取・解析は未実装)
    st->vbox()->addWidget(ofd::tabhelp::sampleNote(st));
    m_selected = new QLabel(I18n::tr("h5_selected") + " " + m_dataset, st);
    st->vbox()->addWidget(m_selected);
    v->addWidget(st);

    // 可視化 / Visualization
    auto *sv = new SectionBox(I18n::tr("h5_vis_section"), body);
    m_view = new QComboBox(sv);
    m_view->addItem(I18n::tr("h5_view_heatmap"));
    m_view->addItem(I18n::tr("h5_view_contour"));
    m_view->addItem(I18n::tr("h5_view_vector"));
    m_view->addItem(I18n::tr("h5_view_iso3d"));
    m_view->addItem(I18n::tr("h5_view_line"));
    sv->form()->addRow(I18n::tr("h5_view_mode"), m_view);
    m_cmap = new QComboBox(sv);
    m_cmap->addItems({ "Jet", "Viridis", "Seismic", "Gray" });
    sv->form()->addRow(I18n::tr("h5_colormap"), m_cmap);
    auto *scaleRow = new QHBoxLayout();
    m_autoScale = new QCheckBox(I18n::tr("h5_auto"), sv);
    m_autoScale->setChecked(true);
    m_scaleMin = new QLineEdit("-1", sv);
    m_scaleMin->setMaximumWidth(60);
    m_scaleMin->setEnabled(false);
    m_scaleMax = new QLineEdit("1", sv);
    m_scaleMax->setMaximumWidth(60);
    m_scaleMax->setEnabled(false);
    scaleRow->addWidget(m_autoScale);
    scaleRow->addWidget(m_scaleMin);
    scaleRow->addWidget(new QLabel(QString::fromUtf8("〜"), sv));
    scaleRow->addWidget(m_scaleMax);
    scaleRow->addStretch(1);
    sv->form()->addRow(I18n::tr("h5_scale"), scaleRow);
    // 等値面レベル (iso3d 選択時のみ表示)
    m_isoLabel = new QLabel(I18n::tr("h5_iso_level"), sv);
    m_isoField = new QWidget(sv);
    auto *isoRow = new QHBoxLayout(m_isoField);
    isoRow->setContentsMargins(0, 0, 0, 0);
    m_isoSlider = new QSlider(Qt::Horizontal, m_isoField);
    m_isoSlider->setRange(0, 20);       // step 0.05
    m_isoSlider->setValue(10);          // 0.50
    m_isoValue = new QLabel("0.50", m_isoField);
    isoRow->addWidget(m_isoSlider, 1);
    isoRow->addWidget(m_isoValue);
    sv->form()->addRow(m_isoLabel, m_isoField);
    m_isoLabel->hide();
    m_isoField->hide();
    auto *checks = new QHBoxLayout();
    auto *ckGrid = new QCheckBox(I18n::tr("h5_show_grid"), sv);
    auto *ckAxes = new QCheckBox(I18n::tr("h5_show_axes"), sv);
    ckAxes->setChecked(true);
    checks->addWidget(ckGrid);
    checks->addWidget(ckAxes);
    // オーバーレイ 2 種はどこにも配線されていない → 未実装として無効化
    auto *ckOvGeom = new QCheckBox(I18n::tr("h5_overlay_geom"), sv);
    auto *ckOvMon  = new QCheckBox(I18n::tr("h5_overlay_mon"), sv);
    ofd::tabhelp::markNotImplemented(ckOvGeom);
    ofd::tabhelp::markNotImplemented(ckOvMon);
    checks->addWidget(ckOvGeom);
    checks->addWidget(ckOvMon);
    checks->addStretch(1);
    sv->form()->addRow(checks);
    v->addWidget(sv);

    // プレビュー (frame N / 240)
    m_previewBox = new SectionBox(
        I18n::tr("h5_preview") + QString(" (frame 0 / %1)").arg(kTotalFrames), body);
    auto *ph = new QHBoxLayout();
    m_canvas = new FieldCanvas(m_previewBox);
    ph->addWidget(m_canvas, 1);
    auto *barCol = new QVBoxLayout();
    m_barMax = new QLabel("+1.00", m_previewBox);
    m_barMin = new QLabel("-1.00", m_previewBox);
    m_bar = new ColorBar(m_previewBox);
    barCol->addWidget(m_barMax, 0, Qt::AlignHCenter);
    barCol->addWidget(m_bar, 1, Qt::AlignHCenter);
    barCol->addWidget(m_barMin, 0, Qt::AlignHCenter);
    barCol->addSpacing(6);
    barCol->addWidget(new QLabel("|E|", m_previewBox), 0, Qt::AlignHCenter);
    barCol->addWidget(new QLabel("V/m", m_previewBox), 0, Qt::AlignHCenter);
    ph->addLayout(barCol);
    m_previewBox->vbox()->addLayout(ph);
    v->addWidget(m_previewBox);

    // 再生コントロール / Playback
    auto *sp = new SectionBox(I18n::tr("h5_playback"), body);
    auto *prow = new QHBoxLayout();
    m_playBtn = new QPushButton(I18n::tr("h5_play"), sp);
    m_playBtn->setMinimumWidth(64);
    auto *firstBtn = new QPushButton(I18n::tr("h5_first"), sp);
    auto *prevBtn  = new QPushButton(I18n::tr("h5_prev"), sp);
    auto *nextBtn  = new QPushButton(I18n::tr("h5_next"), sp);
    auto *lastBtn  = new QPushButton(I18n::tr("h5_last"), sp);
    prow->addWidget(m_playBtn);
    prow->addWidget(firstBtn);
    prow->addWidget(prevBtn);
    prow->addWidget(nextBtn);
    prow->addWidget(lastBtn);
    auto *loopBtn = new QPushButton(I18n::tr("h5_loop"), sp);
    ofd::tabhelp::markNotImplemented(loopBtn);   // 再生は常にループ (切替未実装)
    prow->addWidget(loopBtn);
    prow->addStretch(1);
    sp->vbox()->addLayout(prow);
    auto *fr = new QHBoxLayout();
    m_frameSlider = new QSlider(Qt::Horizontal, sp);
    m_frameSlider->setRange(0, kTotalFrames - 1);
    m_frameLabel = new QLabel(QString("0 / %1").arg(kTotalFrames - 1), sp);
    m_frameLabel->setMinimumWidth(80);
    m_frameLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    fr->addWidget(m_frameSlider, 1);
    fr->addWidget(m_frameLabel);
    sp->form()->addRow(I18n::tr("h5_frame"), fr);
    m_speed = new QComboBox(sp);
    m_speed->addItems({ "0.25x", "0.5x", "1x", "2x", "5x" });
    m_speed->setCurrentIndex(2);
    sp->form()->addRow(I18n::tr("h5_speed"), m_speed);
    auto *tr0 = new QHBoxLayout();
    auto *rangeLo = new QLineEdit("0", sp);
    rangeLo->setMaximumWidth(60);
    auto *rangeHi = new QLineEdit("1000", sp);
    rangeHi->setMaximumWidth(60);
    tr0->addWidget(rangeLo);
    tr0->addWidget(new QLabel(QString::fromUtf8("〜"), sp));
    tr0->addWidget(rangeHi);
    tr0->addWidget(new QLabel("ps", sp));
    auto *ckRangeOnly = new QCheckBox(I18n::tr("h5_range_only"), sp);
    ofd::tabhelp::markNotImplemented(ckRangeOnly);
    tr0->addWidget(ckRangeOnly);
    tr0->addStretch(1);
    sp->form()->addRow(I18n::tr("h5_time_range"), tr0);
    // 時間範囲の入力は再生に反映されない (未実装)
    sp->form()->addRow(ofd::tabhelp::unwiredNote(sp));
    v->addWidget(sp);

    // 時間断面 / Cross-sections (XY/XZ/YZ)
    auto *sx = new SectionBox(I18n::tr("h5_xsec_section"), body);
    auto *xrow = new QHBoxLayout();
    auto *planeBox = new QComboBox(sx);
    planeBox->addItem(I18n::tr("h5_sec_xy"));
    planeBox->addItem(I18n::tr("h5_sec_xz"));
    planeBox->addItem(I18n::tr("h5_sec_yz"));
    xrow->addWidget(planeBox);
    xrow->addWidget(new QLabel(I18n::tr("h5_sec_pos"), sx));
    m_secSlider = new QSlider(Qt::Horizontal, sx);
    m_secSlider->setRange(0, 30);
    m_secSlider->setValue(15);
    xrow->addWidget(m_secSlider, 1);
    m_secValue = new QLabel("15 (Z=1.5mm)", sx);
    xrow->addWidget(m_secValue);
    sx->vbox()->addLayout(xrow);
    auto *ckMulti = new QCheckBox(I18n::tr("h5_sec_multi"), sx);
    ofd::tabhelp::markNotImplemented(ckMulti);
    sx->vbox()->addWidget(ckMulti);
    // 断面の選択・位置はプレビュー描画に反映されない (ラベル更新のみ)
    sx->vbox()->addWidget(ofd::tabhelp::unwiredNote(sx));
    v->addWidget(sx);

    // エクスポート / Export
    auto *se = new SectionBox(I18n::tr("h5_export"), body);
    auto *erow = new QHBoxLayout();
    // エクスポートは全て未実装 → 無効化
    for (const char *key : { "h5_exp_mp4", "h5_exp_gif", "h5_exp_png",
                             "h5_exp_pngseq", "h5_exp_csv" }) {
        auto *b = new QPushButton(I18n::tr(QLatin1String(key)), se);
        ofd::tabhelp::markNotImplemented(b);
        erow->addWidget(b);
    }
    erow->addStretch(1);
    se->vbox()->addLayout(erow);
    auto *mrow = new QHBoxLayout();
    mrow->addWidget(new QLabel("FPS", se));
    auto *fps = new QLineEdit("30", se);
    fps->setMaximumWidth(60);
    mrow->addWidget(fps);
    mrow->addWidget(new QLabel(I18n::tr("h5_resolution"), se));
    auto *resBox = new QComboBox(se);
    resBox->addItems({ QString::fromUtf8("1920 × 1080"),
                       QString::fromUtf8("3840 × 2160 (4K)"),
                       QString::fromUtf8("1280 × 720") });
    mrow->addWidget(resBox);
    mrow->addWidget(new QLabel(I18n::tr("h5_codec"), se));
    auto *codecBox = new QComboBox(se);
    codecBox->addItems({ "H.264", "H.265", "VP9" });
    mrow->addWidget(codecBox);
    mrow->addStretch(1);
    se->form()->addRow(I18n::tr("h5_movie"), mrow);
    auto *echecks = new QHBoxLayout();
    auto *ckEmbed = new QCheckBox(I18n::tr("h5_embed_bar"), se);
    ckEmbed->setChecked(true);
    echecks->addWidget(ckEmbed);
    echecks->addWidget(new QCheckBox(I18n::tr("h5_embed_geom"), se));
    echecks->addStretch(1);
    se->form()->addRow(echecks);
    // 動画設定・埋込オプションはエクスポート未実装のため反映先が無い
    se->vbox()->addWidget(ofd::tabhelp::unwiredNote(se));
    v->addWidget(se);

    // 統計・派生量 / Statistics & derived
    auto *ss = new SectionBox(I18n::tr("h5_stats_section"), body);
    auto *brow = new QHBoxLayout();
    brow->addWidget(makeBadge("max |E|: 1.42 V/m @ frame 87", ss, "ok"));
    brow->addWidget(makeBadge("RMS: 0.31 V/m", ss));
    brow->addWidget(makeBadge(I18n::tr("h5_stat_energy"), ss));
    brow->addStretch(1);
    ss->vbox()->addLayout(brow);
    // 統計値は固定サンプル (実データが無いので算出できない)
    ss->vbox()->addWidget(ofd::tabhelp::sampleNote(ss));
    auto *srow = new QHBoxLayout();
    for (const char *key : { "h5_stat_series", "h5_stat_schroeder",
                             "h5_stat_fft", "h5_stat_lineint" }) {
        auto *b = new QPushButton(I18n::tr(QLatin1String(key)), ss);
        ofd::tabhelp::markNotImplemented(b);
        srow->addWidget(b);
    }
    srow->addStretch(1);
    ss->vbox()->addLayout(srow);
    v->addWidget(ss);

    // 連携 / Integration
    auto *sg = new SectionBox(I18n::tr("h5_integration"), body);
    auto *grow = new QHBoxLayout();
    // 外部連携は全て未実装 → 無効化
    auto *pyBtn  = new QPushButton(I18n::tr("h5_int_python"), sg);
    auto *jupBtn = new QPushButton("📊 Jupyter Notebook", sg);
    auto *pvBtn  = new QPushButton(I18n::tr("h5_int_paraview"), sg);
    auto *mlBtn  = new QPushButton(I18n::tr("h5_int_matlab"), sg);
    for (QPushButton *b : { pyBtn, jupBtn, pvBtn, mlBtn })
        ofd::tabhelp::markNotImplemented(b);
    grow->addWidget(pyBtn);
    grow->addWidget(jupBtn);
    grow->addWidget(pvBtn);
    grow->addWidget(mlBtn);
    grow->addStretch(1);
    sg->vbox()->addLayout(grow);
    v->addWidget(sg);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    // ── 接続 ──
    m_timer = new QTimer(this);
    m_timer->setInterval(int(1000.0 / (30.0 * kSpeeds[2])));
    connect(m_timer, &QTimer::timeout, this, [this] {
        setFrame((m_frame + 1) % kTotalFrames);      // ループ再生
    });
    connect(m_playBtn, &QPushButton::clicked, this, [this] {
        if (m_timer->isActive()) {
            m_timer->stop();
            m_playBtn->setText(I18n::tr("h5_play"));
        } else {
            m_timer->start();
            m_playBtn->setText(I18n::tr("h5_pause"));
        }
    });
    connect(firstBtn, &QPushButton::clicked, this, [this] { setFrame(0); });
    connect(prevBtn, &QPushButton::clicked, this, [this] {
        setFrame(std::max(0, m_frame - 1));
    });
    connect(nextBtn, &QPushButton::clicked, this, [this] {
        setFrame(std::min(kTotalFrames - 1, m_frame + 1));
    });
    connect(lastBtn, &QPushButton::clicked, this, [this] {
        setFrame(kTotalFrames - 1);
    });
    connect(m_frameSlider, &QSlider::valueChanged, this, [this](int f) {
        setFrame(f);
    });
    connect(m_speed, &QComboBox::currentIndexChanged, this, [this](int i) {
        m_timer->setInterval(int(1000.0 / (30.0 * kSpeeds[qBound(0, i, 4)])));
    });
    connect(m_view, &QComboBox::currentIndexChanged, this, [this](int i) {
        m_canvas->setView(i);
        m_isoLabel->setVisible(i == 3);
        m_isoField->setVisible(i == 3);
    });
    connect(m_cmap, &QComboBox::currentIndexChanged, this, [this](int i) {
        m_canvas->setColormap(i);
        m_bar->setColormap(i);
    });
    connect(m_autoScale, &QCheckBox::toggled, this, [this](bool on) {
        m_scaleMin->setEnabled(!on);
        m_scaleMax->setEnabled(!on);
    });
    connect(m_scaleMin, &QLineEdit::editingFinished, this, [this] { applyScale(); });
    connect(m_scaleMax, &QLineEdit::editingFinished, this, [this] { applyScale(); });
    connect(m_isoSlider, &QSlider::valueChanged, this, [this](int val) {
        const double iso = val * 0.05;
        m_isoValue->setText(QString::number(iso, 'f', 2));
        m_canvas->setIso(iso);
    });
    connect(ckGrid, &QCheckBox::toggled, this, [this](bool on) {
        m_canvas->setShowGrid(on);
    });
    connect(ckAxes, &QCheckBox::toggled, this, [this](bool on) {
        m_canvas->setShowAxes(on);
    });
    connect(m_secSlider, &QSlider::valueChanged, this, [this](int val) {
        m_secValue->setText(QString("%1 (Z=%2mm)")
                                .arg(val).arg(val * 0.1, 0, 'f', 1));
    });
    connect(m_tree, &QTreeWidget::itemClicked, this,
            [this](QTreeWidgetItem *it, int) {
        const QString path = it->data(0, Qt::UserRole).toString();
        if (path.isEmpty()) return;     // グループ/属性は選択対象外
        m_dataset = path;
        m_selected->setText(I18n::tr("h5_selected") + " " + path);
        m_canvas->setDatasetName(path.section('/', -1));
    });
}

void H5ViewerTab::setFrame(int f)
{
    m_frame = f;
    m_canvas->setFrame(f);
    m_frameSlider->blockSignals(true);
    m_frameSlider->setValue(f);
    m_frameSlider->blockSignals(false);
    m_frameLabel->setText(QString("%1 / %2").arg(f).arg(kTotalFrames - 1));
    m_previewBox->setTitle(I18n::tr("h5_preview")
        + QString(" (frame %1 / %2)").arg(f).arg(kTotalFrames));
}

void H5ViewerTab::applyScale()
{
    const double lo = m_scaleMin->text().toDouble();
    const double hi = m_scaleMax->text().toDouble();
    if (lo >= hi) return;
    m_canvas->setScale(lo, hi);
    m_barMax->setText(QString("+%1").arg(hi, 0, 'f', 2));
    m_barMin->setText(QString::number(lo, 'f', 2));
}
