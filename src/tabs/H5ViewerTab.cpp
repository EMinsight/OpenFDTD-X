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
#include <QFileInfo>
#include <QFont>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QLinearGradient>
#include <QLocale>
#include <QPainter>
#include <QPushButton>
#include <QSlider>
#include <QStringList>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <functional>

using namespace ofd;

namespace {
// タブ専用語彙 (接頭辞 h5_) — file-local 登録
const bool s_i18n = [] {
    ofd::I18n::reg("h5_file_section", "HDF5 ファイル", "HDF5 File");
    ofd::I18n::reg("h5_file", "ファイル", "File");
    ofd::I18n::reg("h5_browse", "📁 参照…", "📁 Browse…");
    ofd::I18n::reg("h5_reload", "↻ 再読込", "↻ Reload");
    ofd::I18n::reg("h5_file_ph", "例: time_series_data.h5",
                   "e.g. time_series_data.h5");
    ofd::I18n::reg("h5_formats", "対応形式", "Supported formats");
    ofd::I18n::reg("h5_file_hint",
        "▸ 2D / 3D (frames×rows×cols) データセットのヒートマップ表示に対応。"
        "1D や ofd の 4D データセット (/data*/E 等) は表示未対応。",
        "▸ Displays 2D and 3D (frames×rows×cols) datasets as heatmaps. "
        "1D and 4D datasets (e.g. ofd's /data*/E) are not supported for "
        "display.");
    ofd::I18n::reg("h5_disabled",
        "HDF5 読取はこのビルドでは無効です (-DUSE_HDF5=ON でビルドしてください)",
        "HDF5 reading is disabled in this build (rebuild with -DUSE_HDF5=ON)");
    ofd::I18n::reg("h5_tree_section", "データセット", "Dataset tree");
    ofd::I18n::reg("h5_no_datasets", "(データセットなし)", "(no datasets)");
    ofd::I18n::reg("h5_selected", "選択中:", "Selected:");
    ofd::I18n::reg("h5_no_data",
        "データ未読込 — .h5 を開いてデータセットを選択してください",
        "No data loaded — open an .h5 file and select a dataset");
    ofd::I18n::reg("h5_unsupported",
        "このデータセットは表示未対応 (2D/3D のみ)",
        "This dataset cannot be displayed (only 2D/3D supported)");
    ofd::I18n::reg("h5_load_error", "読込エラー:", "Load error:");
    ofd::I18n::reg("h5_vis_section", "可視化", "Visualization");
    ofd::I18n::reg("h5_colormap", "カラーマップ", "Colormap");
    ofd::I18n::reg("h5_scale", "スケール", "Scale");
    ofd::I18n::reg("h5_auto", "自動", "Auto");
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
    ofd::I18n::reg("h5_stat_mean", "平均", "mean");
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

const double kSpeeds[5] = { 0.25, 0.5, 1.0, 2.0, 5.0 };

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

// 次元表示 "(200 × 128)"
QString dimsText(const QVector<qlonglong> &dims)
{
    QStringList parts;
    for (const qlonglong d : dims) parts << QString::number(d);
    return QString("(%1)").arg(parts.join(QString::fromUtf8(" × ")));
}
} // namespace

// ── FieldCanvas ─────────────────────────────────────────────────────────────
FieldCanvas::FieldCanvas(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(320, 320);
}

// mock の colormap_fn を転記。v は m_lo..m_hi で 0..1 へ正規化して着色する。
QColor FieldCanvas::mapColor(double v) const
{
    // レンジが潰れている (全セル同値) ときは中央色で塗る
    double t = (m_hi - m_lo > 0.0) ? (v - m_lo) / (m_hi - m_lo) : 0.5;
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

void FieldCanvas::setData(const QVector<double> &d, int rows, int cols)
{
    m_data = d;
    m_rows = rows;
    m_cols = cols;
    m_msg.clear();
    rebuildImage();
    update();
}

void FieldCanvas::setMessage(const QString &msg)
{
    m_data.clear();
    m_rows = m_cols = 0;
    m_img = QImage();
    m_msg = msg;
    update();
}

void FieldCanvas::setColormap(int c)
{
    m_cmap = c;
    rebuildImage();
    update();
}

void FieldCanvas::setScale(double lo, double hi)
{
    m_lo = lo;
    m_hi = hi;
    rebuildImage();
    update();
}

// 行列 → 1 セル 1 ピクセルの QImage (paintEvent でウィジェット全面へ拡大)
void FieldCanvas::rebuildImage()
{
    if (m_rows <= 0 || m_cols <= 0
            || m_data.size() < qsizetype(m_rows) * m_cols) {
        m_img = QImage();
        return;
    }
    m_img = QImage(m_cols, m_rows, QImage::Format_RGB32);
    for (int r = 0; r < m_rows; ++r) {
        QRgb *line = reinterpret_cast<QRgb *>(m_img.scanLine(r));
        const double *src = m_data.constData() + qsizetype(r) * m_cols;
        for (int c = 0; c < m_cols; ++c)
            line[c] = mapColor(src[c]).rgb();
    }
}

void FieldCanvas::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), Qt::black);

    if (m_img.isNull()) {
        // データ未読込 / 表示未対応 — 中央にメッセージ
        p.setPen(QColor("#9AA7B4"));
        QFont f = p.font();
        f.setPixelSize(12);
        p.setFont(f);
        const QString msg = m_msg.isEmpty() ? I18n::tr("h5_no_data") : m_msg;
        p.drawText(rect().adjusted(16, 16, -16, -16),
                   Qt::AlignCenter | Qt::TextWordWrap, msg);
        p.setPen(QPen(palette().mid().color(), 1));
        p.setBrush(Qt::NoBrush);
        p.drawRect(rect().adjusted(0, 0, -1, -1));
        return;
    }

    // 実データ行列を全面へ拡大描画 (row 0 が上端)
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.drawImage(rect(), m_img);

    if (m_grid) {
        QPen pen(QColor(255, 255, 255, 38));   // rgba(255,255,255,0.15)
        pen.setWidthF(1.0);
        p.setPen(pen);
        for (int g = 1; g < 4; ++g) {          // 1/4 刻みの補助線
            const double x = width() * g / 4.0;
            const double y = height() * g / 4.0;
            p.drawLine(QPointF(x, 0), QPointF(x, height()));
            p.drawLine(QPointF(0, y), QPointF(width(), y));
        }
    }
    if (m_axes) {
        // 軸は行列 index (列 0..cols-1 を下辺、行 0..rows-1 を左辺) で表示
        p.setPen(QColor(255, 255, 255, 150));
        QFont f = p.font();
        f.setPixelSize(10);
        p.setFont(f);
        const QFontMetrics fm(p.font());
        const QString cmax = QString::number(m_cols - 1);
        const QString rmax = QString::number(m_rows - 1);
        p.drawText(QPointF(4, height() - 4), "0");
        p.drawText(QPointF(width() - fm.horizontalAdvance(cmax) - 4,
                           height() - 4), cmax);
        p.drawText(QPointF(4, fm.ascent() + 2), "0");
        p.drawText(QPointF(4, height() - fm.height() - 6), rmax);
    }

    // 左上オーバーレイ: データセット名
    if (!m_name.isEmpty()) {
        QFont f = p.font();
        f.setPixelSize(10);
        p.setFont(f);
        const QFontMetrics fm(p.font());
        const int tw = fm.horizontalAdvance(m_name);
        p.fillRect(QRectF(8, 8, tw + 12, fm.height() + 6), QColor(0, 0, 0, 102));
        p.setPen(Qt::white);
        p.drawText(QPointF(14, 8 + fm.ascent() + 3), m_name);
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
    m_file = new QLineEdit(sf);
    m_file->setPlaceholderText(I18n::tr("h5_file_ph"));
    frow->addWidget(m_file, 1);
    m_browseBtn = new QPushButton(I18n::tr("h5_browse"), sf);
    connect(m_browseBtn, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, I18n::tr("h5_file_section"), {},
            "HDF5 (*.h5 *.hdf5);;All files (*)");
        if (path.isEmpty()) return;
        m_file->setText(path);
        loadFile();
    });
    frow->addWidget(m_browseBtn);
    m_reloadBtn = new QPushButton(I18n::tr("h5_reload"), sf);
    connect(m_reloadBtn, &QPushButton::clicked, this, [this] { loadFile(); });
    frow->addWidget(m_reloadBtn);
    sf->form()->addRow(I18n::tr("h5_file"), frow);
    connect(m_file, &QLineEdit::returnPressed, this, [this] { loadFile(); });
    auto *fmts = new QHBoxLayout();
    fmts->addWidget(makeBadge(".h5", sf));
    fmts->addWidget(makeBadge(".hdf5", sf));
    fmts->addStretch(1);
    sf->form()->addRow(I18n::tr("h5_formats"), fmts);
    auto *fhint = new QLabel(I18n::tr("h5_file_hint"), sf);
    fhint->setWordWrap(true);
    sf->vbox()->addWidget(fhint);
    v->addWidget(sf);

    // データセット / Dataset tree (実ファイルの列挙結果)
    auto *st = new SectionBox(I18n::tr("h5_tree_section"), body);
    m_tree = new QTreeWidget(st);
    m_tree->setColumnCount(2);
    m_tree->setHeaderHidden(true);
    m_tree->setIndentation(14);
    m_tree->setMaximumHeight(240);
    m_tree->setMinimumHeight(240);
    st->vbox()->addWidget(m_tree);
    m_selected = new QLabel(I18n::tr("h5_selected") + " -", st);
    st->vbox()->addWidget(m_selected);
    v->addWidget(st);

    // 可視化 / Visualization
    auto *sv = new SectionBox(I18n::tr("h5_vis_section"), body);
    m_cmap = new QComboBox(sv);
    m_cmap->addItems({ "Jet", "Viridis", "Seismic", "Gray" });
    sv->form()->addRow(I18n::tr("h5_colormap"), m_cmap);
    auto *scaleRow = new QHBoxLayout();
    m_autoScale = new QCheckBox(I18n::tr("h5_auto"), sv);
    m_autoScale->setChecked(true);
    m_scaleMin = new QLineEdit("-1", sv);
    m_scaleMin->setMaximumWidth(80);
    m_scaleMin->setEnabled(false);
    m_scaleMax = new QLineEdit("1", sv);
    m_scaleMax->setMaximumWidth(80);
    m_scaleMax->setEnabled(false);
    scaleRow->addWidget(m_autoScale);
    scaleRow->addWidget(m_scaleMin);
    scaleRow->addWidget(new QLabel(QString::fromUtf8("〜"), sv));
    scaleRow->addWidget(m_scaleMax);
    scaleRow->addStretch(1);
    sv->form()->addRow(I18n::tr("h5_scale"), scaleRow);
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

    // プレビュー / Preview
    m_previewBox = new SectionBox(I18n::tr("h5_preview"), body);
    auto *ph = new QHBoxLayout();
    m_canvas = new FieldCanvas(m_previewBox);
    ph->addWidget(m_canvas, 1);
    auto *barCol = new QVBoxLayout();
    m_barMax = new QLabel("-", m_previewBox);
    m_barMin = new QLabel("-", m_previewBox);
    m_bar = new ColorBar(m_previewBox);
    barCol->addWidget(m_barMax, 0, Qt::AlignHCenter);
    barCol->addWidget(m_bar, 1, Qt::AlignHCenter);
    barCol->addWidget(m_barMin, 0, Qt::AlignHCenter);
    ph->addLayout(barCol);
    m_previewBox->vbox()->addLayout(ph);
    v->addWidget(m_previewBox);

    // 再生コントロール / Playback (3D データセット選択時のみ有効)
    auto *sp = new SectionBox(I18n::tr("h5_playback"), body);
    auto *prow = new QHBoxLayout();
    m_playBtn = new QPushButton(I18n::tr("h5_play"), sp);
    m_playBtn->setMinimumWidth(64);
    m_firstBtn = new QPushButton(I18n::tr("h5_first"), sp);
    m_prevBtn  = new QPushButton(I18n::tr("h5_prev"), sp);
    m_nextBtn  = new QPushButton(I18n::tr("h5_next"), sp);
    m_lastBtn  = new QPushButton(I18n::tr("h5_last"), sp);
    prow->addWidget(m_playBtn);
    prow->addWidget(m_firstBtn);
    prow->addWidget(m_prevBtn);
    prow->addWidget(m_nextBtn);
    prow->addWidget(m_lastBtn);
    auto *loopBtn = new QPushButton(I18n::tr("h5_loop"), sp);
    ofd::tabhelp::markNotImplemented(loopBtn);   // 再生は常にループ (切替未実装)
    prow->addWidget(loopBtn);
    prow->addStretch(1);
    sp->vbox()->addLayout(prow);
    auto *fr = new QHBoxLayout();
    m_frameSlider = new QSlider(Qt::Horizontal, sp);
    m_frameSlider->setRange(0, 0);
    m_frameLabel = new QLabel("- / -", sp);
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

    // 時間断面 / Cross-sections (XY/XZ/YZ) — 未配線 (unwiredNote 付きで維持)
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
    m_secValue = new QLabel("15", sx);
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

    // 統計・派生量 / Statistics & derived — 表示中フレームの実計算値
    auto *ss = new SectionBox(I18n::tr("h5_stats_section"), body);
    auto *brow = new QHBoxLayout();
    m_statMin  = makeBadge("min: -", ss);
    m_statMax  = makeBadge("max: -", ss, "ok");
    m_statMean = makeBadge(I18n::tr("h5_stat_mean") + ": -", ss, "acc");
    brow->addWidget(m_statMin);
    brow->addWidget(m_statMax);
    brow->addWidget(m_statMean);
    brow->addStretch(1);
    ss->vbox()->addLayout(brow);
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
        if (m_nframes > 1)
            setFrame((m_frame + 1) % m_nframes);      // ループ再生
    });
    connect(m_playBtn, &QPushButton::clicked, this, [this] {
        if (m_timer->isActive()) {
            m_timer->stop();
            m_playBtn->setText(I18n::tr("h5_play"));
        } else if (m_nframes > 1) {
            m_timer->start();
            m_playBtn->setText(I18n::tr("h5_pause"));
        }
    });
    connect(m_firstBtn, &QPushButton::clicked, this, [this] { setFrame(0); });
    connect(m_prevBtn, &QPushButton::clicked, this, [this] {
        setFrame(m_frame - 1);
    });
    connect(m_nextBtn, &QPushButton::clicked, this, [this] {
        setFrame(m_frame + 1);
    });
    connect(m_lastBtn, &QPushButton::clicked, this, [this] {
        setFrame(m_nframes - 1);
    });
    connect(m_frameSlider, &QSlider::valueChanged, this, [this](int f) {
        setFrame(f);
    });
    connect(m_speed, &QComboBox::currentIndexChanged, this, [this](int i) {
        m_timer->setInterval(int(1000.0 / (30.0 * kSpeeds[qBound(0, i, 4)])));
    });
    connect(m_cmap, &QComboBox::currentIndexChanged, this, [this](int i) {
        m_canvas->setColormap(i);
        m_bar->setColormap(i);
    });
    connect(m_autoScale, &QCheckBox::toggled, this, [this](bool on) {
        m_scaleMin->setEnabled(!on);
        m_scaleMax->setEnabled(!on);
        if (on) {
            // 自動へ戻したら現在データの min/max へ再スケール
            if (!m_data.isEmpty())
                showData(m_data, m_rows, m_cols);
        } else {
            applyScale();
        }
    });
    connect(m_scaleMin, &QLineEdit::editingFinished, this, [this] { applyScale(); });
    connect(m_scaleMax, &QLineEdit::editingFinished, this, [this] { applyScale(); });
    connect(ckGrid, &QCheckBox::toggled, this, [this](bool on) {
        m_canvas->setShowGrid(on);
    });
    connect(ckAxes, &QCheckBox::toggled, this, [this](bool on) {
        m_canvas->setShowAxes(on);
    });
    connect(m_secSlider, &QSlider::valueChanged, this, [this](int val) {
        m_secValue->setText(QString::number(val));
    });
    connect(m_tree, &QTreeWidget::itemClicked, this,
            [this](QTreeWidgetItem *it, int) {
        const QVariant idx = it->data(0, Qt::UserRole);
        if (!idx.isValid()) return;     // グループノードは選択対象外
        selectDataset(idx.toInt());
    });

    setPlaybackEnabled(false);

    // HDF5 無効ビルド: 注記をタブ先頭に出し、読込 UI を無効化する
    if (!H5Reader::available()) {
        auto *note = new QLabel(I18n::tr("h5_disabled"), body);
        note->setWordWrap(true);
        note->setStyleSheet("padding:6px 8px; border:1px solid #C9A227; "
                            "border-radius:3px; color:#C9A227;");
        v->insertWidget(0, note);
        m_file->setEnabled(false);
        m_browseBtn->setEnabled(false);
        m_reloadBtn->setEnabled(false);
        m_tree->setEnabled(false);
        m_canvas->setMessage(I18n::tr("h5_disabled"));
    }
}

// ファイルを列挙してツリーを再構築し、最初の表示可能データセットを選択する
void H5ViewerTab::loadFile()
{
    if (!H5Reader::available()) return;
    const QString path = m_file->text().trimmed();
    if (path.isEmpty()) return;

    m_timer->stop();
    m_playBtn->setText(I18n::tr("h5_play"));
    m_dsets.clear();
    m_dataset.clear();
    m_data.clear();
    m_rows = m_cols = 0;
    m_nframes = 0;
    m_filePath = path;

    QString err;
    if (!H5Reader::listDatasets(path, m_dsets, &err)) {
        m_tree->clear();
        m_selected->setText(I18n::tr("h5_selected") + " -");
        m_canvas->setDatasetName({});
        m_canvas->setMessage(I18n::tr("h5_load_error") + " " + err);
        clearStats();
        setPlaybackEnabled(false);
        m_previewBox->setTitle(I18n::tr("h5_preview"));
        return;
    }
    rebuildTree();

    // 最初の 2D/3D データセットを自動選択 (無ければ未読込表示のまま)
    int first = -1;
    for (int i = 0; i < m_dsets.size(); ++i) {
        const int nd = m_dsets[i].dims.size();
        if (nd == 2 || nd == 3) { first = i; break; }
    }
    if (first >= 0) {
        selectDataset(first);
    } else {
        m_selected->setText(I18n::tr("h5_selected") + " -");
        m_canvas->setDatasetName({});
        m_canvas->setMessage({});
        clearStats();
        setPlaybackEnabled(false);
        m_previewBox->setTitle(I18n::tr("h5_preview"));
    }
}

// 列挙結果からパス階層のツリーを作る。リーフの UserRole = m_dsets の index。
void H5ViewerTab::rebuildTree()
{
    m_tree->clear();
    const QFileInfo fi(m_filePath);
    auto *root = new QTreeWidgetItem(m_tree);
    root->setText(0, QString::fromUtf8("📁 ") + fi.fileName());
    // ファイル情報行 = 実ファイルサイズ (QFileInfo)
    root->setText(1, QLocale().formattedDataSize(fi.size())
                         + QString::fromUtf8(" · HDF5"));
    root->setForeground(1, QColor("#888888"));

    QHash<QString, QTreeWidgetItem *> groups;   // "/field" などの中間ノード
    for (int i = 0; i < m_dsets.size(); ++i) {
        const H5DatasetInfo &ds = m_dsets[i];
        const QStringList parts = ds.path.split(QLatin1Char('/'),
                                                Qt::SkipEmptyParts);
        if (parts.isEmpty()) continue;
        QTreeWidgetItem *parent = root;
        QString acc;
        for (int k = 0; k < parts.size() - 1; ++k) {
            acc += QLatin1Char('/') + parts[k];
            auto it = groups.find(acc);
            if (it == groups.end()) {
                auto *g = new QTreeWidgetItem(parent);
                g->setText(0, QString::fromUtf8("📁 ") + parts[k]);
                it = groups.insert(acc, g);
            }
            parent = it.value();
        }
        auto *leaf = new QTreeWidgetItem(parent);
        leaf->setText(0, QString::fromUtf8("🗂 ") + parts.last());
        leaf->setText(1, ds.typeName + QString::fromUtf8(" · ")
                             + dimsText(ds.dims));
        leaf->setForeground(1, QColor("#888888"));
        leaf->setData(0, Qt::UserRole, i);
    }
    if (m_dsets.isEmpty()) {
        auto *empty = new QTreeWidgetItem(root);
        empty->setText(0, I18n::tr("h5_no_datasets"));
        empty->setForeground(0, QColor("#888888"));
    }
    m_tree->expandAll();
    m_tree->resizeColumnToContents(0);
}

// ツリー選択 → 2D は即描画、3D はスライダを構成して先頭フレーム、他は未対応表示
void H5ViewerTab::selectDataset(int idx)
{
    if (idx < 0 || idx >= m_dsets.size()) return;
    const H5DatasetInfo &ds = m_dsets[idx];

    m_timer->stop();
    m_playBtn->setText(I18n::tr("h5_play"));
    m_dataset = ds.path;
    m_selected->setText(I18n::tr("h5_selected") + " " + ds.path
                        + "  " + ds.typeName + " " + dimsText(ds.dims));
    m_canvas->setDatasetName(ds.path.section(QLatin1Char('/'), -1));

    // ツリー側の選択表示も同期 (自動選択経由で呼ばれたとき)
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        // UserRole 一致のリーフを探す
        QTreeWidgetItem *found = nullptr;
        std::function<void(QTreeWidgetItem *)> walk =
            [&](QTreeWidgetItem *it) {
            if (found) return;
            if (it->data(0, Qt::UserRole).isValid()
                    && it->data(0, Qt::UserRole).toInt() == idx) {
                found = it;
                return;
            }
            for (int c = 0; c < it->childCount(); ++c) walk(it->child(c));
        };
        walk(m_tree->topLevelItem(i));
        if (found) { m_tree->setCurrentItem(found); break; }
    }

    const int nd = ds.dims.size();
    if (nd == 2) {
        // 2D → read2D して描画、時間スライダ無効
        m_nframes = 0;
        setPlaybackEnabled(false);
        m_frameLabel->setText("- / -");
        m_previewBox->setTitle(I18n::tr("h5_preview"));
        QVector<double> d;
        int rows = 0, cols = 0;
        QString err;
        if (!H5Reader::read2D(m_filePath, m_dataset, d, rows, cols, &err)) {
            m_canvas->setMessage(I18n::tr("h5_load_error") + " " + err);
            clearStats();
            return;
        }
        showData(d, rows, cols);
    } else if (nd == 3) {
        // 3D → スライダ範囲 0..nframes-1、先頭フレームを表示
        m_nframes = int(ds.dims[0]);
        m_frameSlider->blockSignals(true);
        m_frameSlider->setRange(0, std::max(0, m_nframes - 1));
        m_frameSlider->blockSignals(false);
        setPlaybackEnabled(true);
        m_frame = 0;
        setFrame(0);
    } else {
        // 1D / 4D+ → 表示未対応 (ofd の /data*/E は空間展開不能)
        m_nframes = 0;
        setPlaybackEnabled(false);
        m_frameLabel->setText("- / -");
        m_previewBox->setTitle(I18n::tr("h5_preview"));
        m_canvas->setMessage(I18n::tr("h5_unsupported"));
        clearStats();
    }
}

// 3D データセットの現在フレームを読み込んで表示する
void H5ViewerTab::loadCurrentFrame()
{
    QVector<double> d;
    int rows = 0, cols = 0;
    QString err;
    if (!H5Reader::readFrame(m_filePath, m_dataset, m_frame, d, rows, cols,
                             &err)) {
        m_canvas->setMessage(I18n::tr("h5_load_error") + " " + err);
        clearStats();
        return;
    }
    showData(d, rows, cols);
}

// 行列をキャンバスへ渡し、min / max / 平均 を実計算して統計・スケールを更新
void H5ViewerTab::showData(const QVector<double> &d, int rows, int cols)
{
    m_data = d;
    m_rows = rows;
    m_cols = cols;

    double lo = 0.0, hi = 0.0, sum = 0.0;
    if (!d.isEmpty()) {
        lo = hi = d[0];
        for (const double v : d) {
            lo = std::min(lo, v);
            hi = std::max(hi, v);
            sum += v;
        }
    }
    const double mean = d.isEmpty() ? 0.0 : sum / double(d.size());

    m_statMin->setText(QString("min: %1").arg(lo, 0, 'g', 4));
    m_statMax->setText(QString("max: %1").arg(hi, 0, 'g', 4));
    m_statMean->setText(QString("%1: %2").arg(I18n::tr("h5_stat_mean"))
                            .arg(mean, 0, 'g', 4));

    if (m_autoScale->isChecked()) {
        m_canvas->setScale(lo, hi);
        setScaleLabels(lo, hi);
    } else {
        // 手動スケールはそのまま (キャンバス側の設定を維持)
        applyScale();
    }
    m_canvas->setData(d, rows, cols);
}

void H5ViewerTab::setFrame(int f)
{
    if (m_nframes <= 0) return;
    f = qBound(0, f, m_nframes - 1);
    m_frame = f;
    m_frameSlider->blockSignals(true);
    m_frameSlider->setValue(f);
    m_frameSlider->blockSignals(false);
    m_frameLabel->setText(QString("%1 / %2").arg(f).arg(m_nframes - 1));
    m_previewBox->setTitle(I18n::tr("h5_preview")
        + QString(" (frame %1 / %2)").arg(f).arg(m_nframes));
    loadCurrentFrame();
}

void H5ViewerTab::applyScale()
{
    const double lo = m_scaleMin->text().toDouble();
    const double hi = m_scaleMax->text().toDouble();
    if (lo >= hi) return;
    m_canvas->setScale(lo, hi);
    setScaleLabels(lo, hi);
}

void H5ViewerTab::setScaleLabels(double lo, double hi)
{
    m_barMax->setText(QString::number(hi, 'g', 4));
    m_barMin->setText(QString::number(lo, 'g', 4));
    // 手動欄も現在の実レンジを反映 (自動→手動の切替時に出発点になる)
    if (m_autoScale->isChecked()) {
        m_scaleMin->setText(QString::number(lo, 'g', 6));
        m_scaleMax->setText(QString::number(hi, 'g', 6));
    }
}

void H5ViewerTab::setPlaybackEnabled(bool on)
{
    m_playBtn->setEnabled(on);
    m_firstBtn->setEnabled(on);
    m_prevBtn->setEnabled(on);
    m_nextBtn->setEnabled(on);
    m_lastBtn->setEnabled(on);
    m_frameSlider->setEnabled(on);
    if (!on && m_timer->isActive()) {
        m_timer->stop();
        m_playBtn->setText(I18n::tr("h5_play"));
    }
}

void H5ViewerTab::clearStats()
{
    m_data.clear();
    m_rows = m_cols = 0;
    m_statMin->setText("min: -");
    m_statMax->setText("max: -");
    m_statMean->setText(I18n::tr("h5_stat_mean") + ": -");
    m_barMax->setText("-");
    m_barMin->setText("-");
}
