// Viewport3D.cpp
#include "Viewport3D.h"
#include "../core/Project.h"
#include "../I18n.h"
#include "FieldHeatmap.h"     // jet カラーマップ (2D 断面表示と同じ配色)

#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QRectF>
#include <QTransform>
#include <QWheelEvent>
#include <algorithm>   // std::clamp (レイ反射の位置クランプ)
#include <cmath>

using namespace ofd;

namespace {
const bool s_i18n = [] {
    // 接頭辞は CenterPane と共通の vp_ (3D ビュー系の語彙)。
    ofd::I18n::reg("vp_fld_none",
        "結果未読込 — 計算を実行するか結果 HDF5 を開いてください",
        "No result loaded — run the solver or open a result HDF5 file");
    ofd::I18n::reg("vp_fld_none_hint",
        "偽の界分布は表示しません",
        "No synthetic field pattern is drawn here");
    ofd::I18n::reg("vp_fld_real",
        "結果断面 (ソルバ出力の実データ)",
        "Result slice (actual solver output)");
    ofd::I18n::reg("vp_fld_norm",
        "正規化 |値| (最大 %1)",
        "normalised |value| (max %1)");
    ofd::I18n::reg("vp_fld_decim",
        "表示のみ %1 セル毎に間引き",
        "display decimated: every %1 cells");
    ofd::I18n::reg("vp_rays_sample",
        "サンプル表示 — ソルバ結果ではありません (24本 × 4反射)",
        "Sample display — not solver results (24 rays x 4 bounces)");
    return true;
}();

// 断面の固定軸 (0=X, 1=Y, 2=Z) → 表示用の軸名
const char *sliceAxisName(int axis)
{
    return (axis == 0) ? "X" : (axis == 1) ? "Y" : "Z";
}
} // namespace

Viewport3D::Viewport3D(Project *project, QWidget *parent)
    : QWidget(parent), m_project(project)
{
    setObjectName("Viewport3D");
    setMinimumSize(320, 240);
    setMouseTracking(false);
    setAutoFillBackground(false);
    connect(project, &Project::changed, this, qOverload<>(&QWidget::update));
    connect(project, &Project::loaded, this, qOverload<>(&QWidget::update));
    // Field は実データの静止断面を描くだけなのでアニメーション用タイマーは
    // 持たない (ヘッドレス/リモートで CPU を回さない)。
}

void Viewport3D::setViewStyle(ViewStyle s)
{
    if (m_viewStyle == s) return;
    m_viewStyle = s;
    m_solid = (s != ViewStyle::Wireframe);
    update();
}

void Viewport3D::setResultSlice(const QVector<double> &cells, int rows,
                                int cols, int axis, double pos_m,
                                double u0, double u1, double v0, double v1,
                                const QString &label)
{
    const qint64 need = qint64(rows) * qint64(cols);
    if (rows <= 0 || cols <= 0 || qint64(cells.size()) < need) {
        clearResultSlice();
        return;
    }
    m_sliceCells = cells;
    m_sliceRows  = rows;
    m_sliceCols  = cols;
    m_sliceAxis  = qBound(0, axis, 2);
    m_slicePos   = pos_m;
    m_sliceU0 = u0; m_sliceU1 = u1;
    m_sliceV0 = v0; m_sliceV1 = v1;
    m_sliceLabel = label;
    // 正規化は「与えられた実データの最大値」で行う (勝手な下駄を履かせない)
    m_sliceMax = 0.0;
    for (qint64 i = 0; i < need; ++i) {
        const double v = std::fabs(m_sliceCells[int(i)]);
        if (std::isfinite(v) && v > m_sliceMax) m_sliceMax = v;
    }
    rebuildSliceImage();
    update();
}

void Viewport3D::clearResultSlice()
{
    if (!hasResultSlice() && m_sliceLabel.isEmpty()) return;
    m_sliceCells.clear();
    m_sliceRows = m_sliceCols = 0;
    m_sliceMax = 0.0;
    m_sliceLabel.clear();
    m_sliceImg = QImage();
    m_sliceDecim = 1;
    update();
}

void Viewport3D::fitView()
{
    m_zoom = 1.0;
    m_panPx = QPointF();
    update();
}

void Viewport3D::setAzimuth(double deg)
{
    if (qFuzzyCompare(m_azimuthDeg, deg)) return;
    m_azimuthDeg = deg;
    update();
}

void Viewport3D::setElevation(double deg)
{
    const double v = qBound(-89.0, deg, 89.0);
    if (qFuzzyCompare(m_elevationDeg, v)) return;
    m_elevationDeg = v;
    update();
}

// モックの [XY][YZ][ZX] 軸タグ相当: 正射影で各主平面を正面に向ける
void Viewport3D::setViewPlane(int plane)
{
    switch (plane) {
    case 0: m_azimuthDeg =   0; m_elevationDeg =  89; break;  // XY (上から)
    case 1: m_azimuthDeg =  90; m_elevationDeg =   0; break;  // YZ (X軸方向)
    case 2: m_azimuthDeg =   0; m_elevationDeg =   0; break;  // ZX (Y軸方向)
    default: return;
    }
    update();
    emit viewChanged(m_azimuthDeg, m_elevationDeg);
}

QPointF Viewport3D::projectPoint(double x, double y, double z) const
{
    // center + rotate (azimuth around Z, then elevation around screen-X)
    const double az = m_azimuthDeg  * M_PI / 180.0;
    const double el = m_elevationDeg * M_PI / 180.0;
    const double dx = (x - m_cx) * m_scale;
    const double dy = (y - m_cy) * m_scale;
    const double dz = (z - m_cz) * m_scale;

    const double x1 =  dx * std::cos(az) + dy * std::sin(az);
    const double y1 = -dx * std::sin(az) + dy * std::cos(az);
    const double y2 =  y1 * std::cos(el) - dz * std::sin(el);
    // screen: x right, y down
    return QPointF(width()  / 2.0 + m_panPx.x() + x1,
                   height() / 2.0 + m_panPx.y() + y2);
}

void Viewport3D::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor("#1d2430"));

    // scene extents from the mesh
    double lo[3], hi[3];
    bool any = false;
    for (int a = 0; a < 3; ++a) {
        const MeshAxis &ax = m_project->mesh(a);
        lo[a] = ax.min(); hi[a] = ax.max();
        if (hi[a] > lo[a]) any = true;
    }
    if (!any) { lo[0]=lo[1]=lo[2]=-0.5; hi[0]=hi[1]=hi[2]=0.5; }

    m_cx = (lo[0] + hi[0]) / 2;
    m_cy = (lo[1] + hi[1]) / 2;
    m_cz = (lo[2] + hi[2]) / 2;
    const double ext = std::max({ hi[0]-lo[0], hi[1]-lo[1], hi[2]-lo[2], 1e-12 });
    m_scale = 0.55 * std::min(width(), height()) / ext * m_zoom;

    const QColor accent(accentColor(m_domain));

    // axis triad (bottom-left)
    {
        const double L = ext * 0.18;
        const QPointF o  = projectPoint(lo[0], lo[1], lo[2]);
        const QPointF px = projectPoint(lo[0] + L, lo[1], lo[2]);
        const QPointF py = projectPoint(lo[0], lo[1] + L, lo[2]);
        const QPointF pz = projectPoint(lo[0], lo[1], lo[2] + L);
        p.setPen(QPen(QColor("#e05555"), 2)); p.drawLine(o, px); p.drawText(px, "X");
        p.setPen(QPen(QColor("#4fb24f"), 2)); p.drawLine(o, py); p.drawText(py, "Y");
        p.setPen(QPen(QColor("#5b8fd9"), 2)); p.drawLine(o, pz); p.drawText(pz, "Z");
    }

    // mesh region box
    auto drawBox = [&](const double a[3], const double b[3], const QPen &pen) {
        const QPointF v[8] = {
            projectPoint(a[0],a[1],a[2]), projectPoint(b[0],a[1],a[2]),
            projectPoint(b[0],b[1],a[2]), projectPoint(a[0],b[1],a[2]),
            projectPoint(a[0],a[1],b[2]), projectPoint(b[0],a[1],b[2]),
            projectPoint(b[0],b[1],b[2]), projectPoint(a[0],b[1],b[2]),
        };
        static const int e[12][2] = { {0,1},{1,2},{2,3},{3,0},
                                      {4,5},{5,6},{6,7},{7,4},
                                      {0,4},{1,5},{2,6},{3,7} };
        p.setPen(pen);
        for (auto &ed : e) p.drawLine(v[ed[0]], v[ed[1]]);
    };
    drawBox(lo, hi, QPen(QColor(255,255,255,70), 1, Qt::DashLine));

    // PML 境界の可視化 (境界チェックボックス) — 解析領域を内側へ縮めた箱
    if (m_showBoundary && m_project->general().abc == 1) {
        const int L = qMax(1, m_project->general().pmlL);
        double plo[3], phi[3];
        for (int a = 0; a < 3; ++a) {
            const MeshAxis &ax = m_project->mesh(a);
            const double d = (ax.minSpacing() < 1e307) ? ax.minSpacing() * L : 0.0;
            plo[a] = lo[a] + d;
            phi[a] = hi[a] - d;
        }
        if (plo[0] < phi[0] && plo[1] < phi[1] && plo[2] < phi[2])
            drawBox(plo, phi, QPen(QColor(245, 158, 11, 150), 1, Qt::DotLine));
    }

    // mesh grid ticks on the bottom face (z = lo[2])
    if (m_showGrid) {
        p.setPen(QPen(QColor(255,255,255,28), 1));
        const MeshAxis &mx = m_project->mesh(0);
        const MeshAxis &my = m_project->mesh(1);
        for (int i = 0; i < mx.divs.size(); ++i) {
            const double x0 = mx.nodes[i], x1 = mx.nodes[i+1];
            for (int k = 0; k <= mx.divs[i]; ++k) {
                const double x = x0 + (x1 - x0) * k / mx.divs[i];
                p.drawLine(projectPoint(x, lo[1], lo[2]),
                           projectPoint(x, hi[1], lo[2]));
            }
        }
        for (int i = 0; i < my.divs.size(); ++i) {
            const double y0 = my.nodes[i], y1 = my.nodes[i+1];
            for (int k = 0; k <= my.divs[i]; ++k) {
                const double y = y0 + (y1 - y0) * k / my.divs[i];
                p.drawLine(projectPoint(lo[0], y, lo[2]),
                           projectPoint(hi[0], y, lo[2]));
            }
        }
    }

    // geometry units
    int unit = 0;
    for (const Geometry &g : m_project->geometries()) {
        ++unit;
        QColor col = accent;
        col.setAlpha(m_solid ? 110 : 230);
        const QPen pen(col.lighter(120), 1.4);

        // all 6-parameter shapes are drawn from their bounding box; the
        // ellipsoid/cylinder shapes additionally show an inscribed outline
        double a[3] = { g.g[0], g.g[2], g.g[4] };
        double b[3] = { g.g[1], g.g[3], g.g[5] };
        if (Geometry::paramCount(g.shape) == 8) {
            // 8-param shapes: use min/max of the coordinate list as a hull
            a[0] = std::min({g.g[0], g.g[1]}); b[0] = std::max({g.g[0], g.g[1]});
            a[1] = std::min({g.g[2], g.g[3]}); b[1] = std::max({g.g[2], g.g[3]});
            a[2] = std::min({g.g[4], g.g[5], g.g[6], g.g[7]});
            b[2] = std::max({g.g[4], g.g[5], g.g[6], g.g[7]});
        }

        if (m_solid) {
            // shade the top face
            QPainterPath path;
            path.moveTo(projectPoint(a[0], a[1], b[2]));
            path.lineTo(projectPoint(b[0], a[1], b[2]));
            path.lineTo(projectPoint(b[0], b[1], b[2]));
            path.lineTo(projectPoint(a[0], b[1], b[2]));
            path.closeSubpath();
            p.fillPath(path, col);
        }
        drawBox(a, b, pen);

        if (g.shape == 2 || (g.shape >= 11 && g.shape <= 13)) {
            // inscribed ellipse outline on the mid plane
            p.setPen(pen);
            const int N = 36;
            QPolygonF poly;
            for (int k = 0; k <= N; ++k) {
                const double t = 2 * M_PI * k / N;
                double x = (a[0]+b[0])/2, y = (a[1]+b[1])/2, z = (a[2]+b[2])/2;
                const double rx = (b[0]-a[0])/2, ry = (b[1]-a[1])/2,
                             rz = (b[2]-a[2])/2;
                switch (g.shape) {
                    case 11: y += ry*std::cos(t); z += rz*std::sin(t); break;
                    case 12: x += rx*std::cos(t); z += rz*std::sin(t); break;
                    default: x += rx*std::cos(t); y += ry*std::sin(t); break;
                }
                poly << projectPoint(x, y, z);
            }
            p.drawPolyline(poly);
        }

        p.setPen(QColor(255,255,255,140));
        p.drawText(projectPoint(b[0], b[1], b[2]) + QPointF(3,-3),
                   g.name.isEmpty() ? QStringLiteral("#%1").arg(unit) : g.name);
    }

    // feeds (red diamonds) and probes (green circles)
    p.setPen(Qt::NoPen);
    for (const Feed &f : m_project->feeds()) {
        const QPointF c = projectPoint(f.x, f.y, f.z);
        QPolygonF d; d << c+QPointF(0,-5) << c+QPointF(5,0)
                       << c+QPointF(0,5)  << c+QPointF(-5,0);
        p.setBrush(QColor("#ff5252"));
        p.drawPolygon(d);
    }
    for (const Probe &pr : m_project->probes()) {
        const QPointF c = projectPoint(pr.x, pr.y, pr.z);
        p.setBrush(QColor("#69d069"));
        p.drawEllipse(c, 4, 4);
    }
    if (m_project->planewave().enabled) {
        // incident direction arrow from outside the box
        const double th = m_project->planewave().theta * M_PI / 180.0;
        const double ph = m_project->planewave().phi   * M_PI / 180.0;
        const double R = ext * 0.75;
        const QPointF from = projectPoint(m_cx + R*std::sin(th)*std::cos(ph),
                                          m_cy + R*std::sin(th)*std::sin(ph),
                                          m_cz + R*std::cos(th));
        const QPointF to = projectPoint(m_cx, m_cy, m_cz);
        p.setPen(QPen(QColor("#ffd24d"), 2));
        p.drawLine(from, to);
        p.setBrush(QColor("#ffd24d"));
        p.drawEllipse(to, 3, 3);
    }

    // ビュースタイル別オーバーレイ
    if (m_viewStyle == ViewStyle::Field) drawResultSlice(p);
    if (m_viewStyle == ViewStyle::Rays)  drawRayOverlay(p);

    // overlay text
    p.setPen(QColor(255,255,255,150));
    p.drawText(8, height() - 10,
               QStringLiteral("%1   cells: %L2   az %3°  el %4°")
               .arg(domainKey(m_domain))
               .arg(m_project->totalCells())
               .arg(int(m_azimuthDeg)).arg(int(m_elevationDeg)));
}

// 面内座標 (u, v) [m] → 画面座標。固定軸は m_sliceAxis / m_slicePos。
QPointF Viewport3D::projectSlicePoint(double u, double v) const
{
    switch (m_sliceAxis) {
    case 0:  return projectPoint(m_slicePos, u, v);   // YZ 面 (X 一定)
    case 1:  return projectPoint(u, m_slicePos, v);   // XZ 面 (Y 一定)
    default: return projectPoint(u, v, m_slicePos);   // XY 面 (Z 一定)
    }
}

// 結果断面オーバーレイ — setResultSlice() で渡された実データを 3D 空間の
// 該当平面に描く。面の 4 隅を投影し、色画像をアフィン変換で貼るだけ
// (1 枚の平面なので自己遮蔽は無く深度ソート不要)。
// 断面が未設定のときは合成パターンを描かず「未読込」を明示する
// (存在しない結果を界分布らしく見せない — CLAUDE.md 絶対規則 5)。
void Viewport3D::drawResultSlice(QPainter &p)
{
    if (!hasResultSlice()) {
        // ── 未読込の明示 ────────────────────────────────────────────────
        const QString msg  = I18n::tr("vp_fld_none");
        const QString hint = I18n::tr("vp_fld_none_hint");
        const QFontMetrics fm(p.font());
        const int w = std::max(fm.horizontalAdvance(msg),
                               fm.horizontalAdvance(hint)) + 28;
        const int h = fm.height() * 2 + 24;
        const QRectF box((width() - w) / 2.0, (height() - h) / 2.0, w, h);
        p.setPen(QPen(QColor(245, 158, 11, 200), 1));
        p.setBrush(QColor(20, 26, 36, 215));
        p.drawRoundedRect(box, 6, 6);
        p.setPen(QColor(245, 158, 11));
        p.drawText(QRectF(box.x(), box.y() + 8, box.width(), fm.height()),
                   Qt::AlignHCenter | Qt::AlignVCenter, msg);
        p.setPen(QColor(255, 255, 255, 165));
        p.drawText(QRectF(box.x(), box.y() + 10 + fm.height(), box.width(),
                          fm.height()),
                   Qt::AlignHCenter | Qt::AlignVCenter, hint);
        p.setBrush(Qt::NoBrush);
        return;
    }

    // 断面の 4 隅を投影する。画像の (0,0) は行 0 = 第 2 軸の +側
    const QPointF c00 = projectSlicePoint(m_sliceU0, m_sliceV1);
    const QPointF c10 = projectSlicePoint(m_sliceU1, m_sliceV1);
    const QPointF c11 = projectSlicePoint(m_sliceU1, m_sliceV0);
    const QPointF c01 = projectSlicePoint(m_sliceU0, m_sliceV0);

    // 正射影なので断面の像は必ずアフィン変換で表せる。セル毎に四辺形を描くと
    // 隣接セルの縁が重なって透明度が飽和する (下の形状が透けない) ため、
    // 色画像を 1 回だけ貼る。25 万セルでも drawImage 1 回で済む。
    if (!m_sliceImg.isNull()) {
        const double iw = m_sliceImg.width(), ih = m_sliceImg.height();
        const QPolygonF src{ QPointF(0, 0), QPointF(iw, 0),
                             QPointF(iw, ih), QPointF(0, ih) };
        const QPolygonF dst{ c00, c10, c11, c01 };
        QTransform t;
        if (QTransform::quadToQuad(src, dst, t)) {
            p.save();
            // データを補間しない (実際のセル解像度をそのまま見せる)
            p.setRenderHint(QPainter::SmoothPixmapTransform, false);
            p.setRenderHint(QPainter::Antialiasing, false);
            p.setOpacity(0.72);     // 形状ワイヤが透ける程度の透明度
            p.setTransform(t, true);
            p.drawImage(QPointF(0, 0), m_sliceImg);
            p.restore();
        }
    }

    // 断面の外枠 (面の位置を分かりやすく)
    const QPolygonF outline{ c00, c10, c11, c01 };
    p.setPen(QPen(QColor(255, 255, 255, 110), 1));
    p.setBrush(Qt::NoBrush);
    p.drawPolygon(outline);

    drawSliceLegend(p, m_sliceDecim);
}

// 断面データ → 色画像 (行 0 = 第 2 軸の +側)。setResultSlice のたびに 1 回
// だけ作り、再描画では貼るだけにする。巨大格子は平均で束ねて画像サイズを
// 抑える (束ねたら m_sliceDecim に残して凡例に出す)。
void Viewport3D::rebuildSliceImage()
{
    m_sliceImg = QImage();
    m_sliceDecim = 1;
    if (!hasResultSlice()) return;

    const int rows = m_sliceRows, cols = m_sliceCols;
    const int maxDim = 1024;
    int step = 1;
    while ((cols + step - 1) / step > maxDim || (rows + step - 1) / step > maxDim)
        ++step;
    m_sliceDecim = step;

    const int w = (cols + step - 1) / step;
    const int h = (rows + step - 1) / step;
    QImage img(w, h, QImage::Format_ARGB32);
    if (img.isNull()) return;
    const double inv = (m_sliceMax > 0.0) ? 1.0 / m_sliceMax : 0.0;
    for (int r = 0; r < h; ++r) {
        QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(r));
        const int r1 = std::min((r + 1) * step, rows);
        for (int c = 0; c < w; ++c) {
            const int c1 = std::min((c + 1) * step, cols);
            double sum = 0.0;
            int n = 0;
            for (int rr = r * step; rr < r1; ++rr) {
                const int base = rr * cols;
                for (int cc = c * step; cc < c1; ++cc) {
                    const double v = m_sliceCells[base + cc];
                    if (std::isfinite(v)) { sum += std::fabs(v); ++n; }
                }
            }
            if (n == 0) { line[c] = qRgba(0, 0, 0, 0); continue; }  // 値なし
            const double t = qBound(0.0, sum / n * inv, 1.0);
            const QColor col = FieldHeatmap::jet(t);
            line[c] = qRgba(col.red(), col.green(), col.blue(), 255);
        }
    }
    m_sliceImg = img;
}

// 結果断面の凡例 — カラーバー (0..1) + 実データである旨 + label
void Viewport3D::drawSliceLegend(QPainter &p, int decim)
{
    const QFontMetrics fm(p.font());
    // ── 左上: 実データ表記 + データセット名/時刻 + 断面位置 ─────────────
    QStringList lines;
    lines << I18n::tr("vp_fld_real");
    QString sub = QStringLiteral("%1 = %2 m")
                      .arg(QLatin1String(sliceAxisName(m_sliceAxis)))
                      .arg(QString::number(m_slicePos, 'g', 4));
    if (!m_sliceLabel.isEmpty())
        sub = m_sliceLabel + QStringLiteral("   ") + sub;
    lines << sub;
    lines << QStringLiteral("%1 x %2").arg(m_sliceCols).arg(m_sliceRows)
             + QStringLiteral("   ")
             + I18n::tr("vp_fld_norm")
                   .arg(QString::number(m_sliceMax, 'g', 4));
    if (decim > 1)
        lines << I18n::tr("vp_fld_decim").arg(decim);

    int w = 0;
    for (const QString &s : lines) w = std::max(w, fm.horizontalAdvance(s));
    const QRectF box(6, 6, w + 16, fm.height() * lines.size() + 12);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(15, 20, 28, 175));
    p.drawRoundedRect(box, 4, 4);
    p.setBrush(Qt::NoBrush);
    for (int i = 0; i < lines.size(); ++i) {
        p.setPen(i == 0 ? QColor(accentColor(m_domain))
                        : QColor(255, 255, 255, 175));
        p.drawText(QRectF(box.x() + 8, box.y() + 6 + fm.height() * i,
                          box.width() - 16, fm.height()),
                   Qt::AlignLeft | Qt::AlignVCenter, lines[i]);
    }

    // ── 右辺: カラーバー (0.0 〜 1.0) ───────────────────────────────────
    const int bh = qBound(60, height() - 120, 160);
    const int bw = 12;
    const int bx = width() - bw - 46;
    const int by = 24;
    if (bx <= 0 || bh <= 0) return;
    for (int i = 0; i < bh; ++i) {
        const double t = 1.0 - double(i) / double(bh - 1);
        p.setPen(FieldHeatmap::jet(t));
        p.drawLine(bx, by + i, bx + bw, by + i);
    }
    p.setPen(QColor(255, 255, 255, 150));
    p.drawRect(bx, by, bw, bh);
    p.drawText(QRectF(bx + bw + 3, by - fm.height() / 2.0, 40, fm.height()),
               Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("1.0"));
    p.drawText(QRectF(bx + bw + 3, by + bh - fm.height() / 2.0, 40,
                      fm.height()),
               Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("0.0"));
}

// レイトレースオーバーレイ — モックと同じ 24本 × 最大4反射。
// 領域境界で支配軸を反転させ、反射ごとにエネルギーを 0.7 倍する。
// **ソルバの計算結果ではなく見た目のサンプル** なので、その旨を画面に明示する
// (未実装のものを動作済みに見せない — CLAUDE.md 絶対規則 5)。
void Viewport3D::drawRayOverlay(QPainter &p)
{
    // 先に注記を描く (以降で return しても必ず出る)
    {
        const QString msg = I18n::tr("vp_rays_sample");
        const QFontMetrics fm(p.font());
        const QRectF box(6, 6, fm.horizontalAdvance(msg) + 16,
                         fm.height() + 10);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(15, 20, 28, 175));
        p.drawRoundedRect(box, 4, 4);
        p.setBrush(Qt::NoBrush);
        p.setPen(QColor(245, 158, 11));
        p.drawText(box.adjusted(8, 0, -8, 0),
                   Qt::AlignLeft | Qt::AlignVCenter, msg);
    }

    double lo[3], hi[3];
    for (int a = 0; a < 3; ++a) {
        lo[a] = m_project->mesh(a).min();
        hi[a] = m_project->mesh(a).max();
        if (!(hi[a] > lo[a])) return;
    }
    // 波源: feed があればその位置、無ければ領域中心
    double src[3] = { m_cx, m_cy, m_cz };
    if (!m_project->feeds().isEmpty()) {
        const Feed &f = m_project->feeds().first();
        src[0] = f.x; src[1] = f.y; src[2] = f.z;
    }

    const int N = 24, bounces = 4;
    const double diag = std::sqrt((hi[0]-lo[0])*(hi[0]-lo[0])
                                + (hi[1]-lo[1])*(hi[1]-lo[1])
                                + (hi[2]-lo[2])*(hi[2]-lo[2]));
    const double stepLen = diag * 0.02;

    QColor col(accentColor(m_domain));
    col.setAlphaF(0.55);
    p.setPen(QPen(col, 0.9));
    p.setBrush(Qt::NoBrush);

    for (int i = 0; i < N; ++i) {
        const double theta = double(i) / N * 2.0 * M_PI;
        const double phi = M_PI / 2.0 + std::sin(i * 0.7) * 0.4;
        double dir[3] = { std::cos(theta) * std::sin(phi),
                          std::cos(phi),
                          std::sin(theta) * std::sin(phi) };
        double pos[3] = { src[0], src[1], src[2] };

        QPainterPath path;
        path.moveTo(projectPoint(pos[0], pos[1], pos[2]));
        double energy = 1.0;
        for (int b = 0; b < bounces; ++b) {
            for (int s = 0; s < 80; ++s) {
                for (int k = 0; k < 3; ++k) pos[k] += dir[k] * stepLen;
                if (pos[0] < lo[0] || pos[0] > hi[0] ||
                    pos[1] < lo[1] || pos[1] > hi[1] ||
                    pos[2] < lo[2] || pos[2] > hi[2]) break;
            }
            path.lineTo(projectPoint(pos[0], pos[1], pos[2]));
            // 最も外へ出ている軸で反射させ、位置を領域内へ戻す
            int axis = 0;
            double worst = 0;
            for (int k = 0; k < 3; ++k) {
                const double over = std::max(lo[k] - pos[k], pos[k] - hi[k]);
                if (over > worst) { worst = over; axis = k; }
            }
            dir[axis] *= -1.0;
            pos[axis] = std::clamp(pos[axis], lo[axis], hi[axis]);
            energy *= 0.7;
            if (energy < 0.1) break;
        }
        p.drawPath(path);
    }
}

void Viewport3D::mousePressEvent(QMouseEvent *e)
{
    m_lastPos = e->position();
    m_dragButton = e->button();
}

void Viewport3D::mouseMoveEvent(QMouseEvent *e)
{
    const QPointF d = e->position() - m_lastPos;
    m_lastPos = e->position();
    if (m_dragButton == Qt::LeftButton) {
        m_azimuthDeg  += d.x() * 0.5;
        m_elevationDeg = qBound(-89.0, m_elevationDeg + d.y() * 0.5, 89.0);
        update();
        emit viewChanged(m_azimuthDeg, m_elevationDeg);   // ツールバー同期
    } else if (m_dragButton == Qt::MiddleButton) {
        m_panPx += d;
        update();
    }
}

void Viewport3D::wheelEvent(QWheelEvent *e)
{
    const double f = std::pow(1.0015, e->angleDelta().y());
    m_zoom = qBound(0.05, m_zoom * f, 50.0);
    update();
}

void Viewport3D::mouseDoubleClickEvent(QMouseEvent *)
{
    fitView();
}
