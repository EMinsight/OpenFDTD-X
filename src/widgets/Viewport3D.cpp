// Viewport3D.cpp
#include "Viewport3D.h"
#include "../core/Project.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QWheelEvent>
#include <algorithm>   // std::clamp (レイ反射の位置クランプ)
#include <cmath>

using namespace ofd;

Viewport3D::Viewport3D(Project *project, QWidget *parent)
    : QWidget(parent), m_project(project)
{
    setObjectName("Viewport3D");
    setMinimumSize(320, 240);
    setMouseTracking(false);
    setAutoFillBackground(false);
    connect(project, &Project::changed, this, qOverload<>(&QWidget::update));
    connect(project, &Project::loaded, this, qOverload<>(&QWidget::update));

    // Field オーバーレイ用のアニメーション。Field 以外では止めておく
    // (ヘッドレス/リモートで無駄に再描画しないため)。
    m_animTimer = new QTimer(this);
    m_animTimer->setInterval(50);
    connect(m_animTimer, &QTimer::timeout, this, [this] {
        ++m_animTick;
        update();
    });
}

void Viewport3D::setViewStyle(ViewStyle s)
{
    if (m_viewStyle == s) return;
    m_viewStyle = s;
    m_solid = (s != ViewStyle::Wireframe);
    if (s == ViewStyle::Field) m_animTimer->start();
    else                       m_animTimer->stop();
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

    // ビュースタイル別オーバーレイ (モックの fieldOverlay / rayOverlay)
    if (m_viewStyle == ViewStyle::Field) drawFieldOverlay(p);
    if (m_viewStyle == ViewStyle::Rays)  drawRayOverlay(p);

    // overlay text
    p.setPen(QColor(255,255,255,150));
    p.drawText(8, height() - 10,
               QStringLiteral("%1   cells: %L2   az %3°  el %4°")
               .arg(domainKey(m_domain))
               .arg(m_project->totalCells())
               .arg(int(m_azimuthDeg)).arg(int(m_elevationDeg)));
    // モックと同じ左上のスタイル注記
    if (m_viewStyle == ViewStyle::Field || m_viewStyle == ViewStyle::Rays) {
        p.setPen(QColor(accentColor(m_domain)));
        p.drawText(8, 16, m_viewStyle == ViewStyle::Field
            ? QStringLiteral("field overlay enabled")
            : QStringLiteral("raycast: 24 rays · 4 bounces"));
    }
}

// 界分布オーバーレイ — モックの v = sin(8r - 0.08t)·exp(-0.6r) を
// 領域中心の水平面上に市松模様で散布する。赤=正, 青=負, 透明度=|v|。
void Viewport3D::drawFieldOverlay(QPainter &p)
{
    const double ext = std::max({ m_project->mesh(0).max() - m_project->mesh(0).min(),
                                  m_project->mesh(1).max() - m_project->mesh(1).min(),
                                  m_project->mesh(2).max() - m_project->mesh(2).min() });
    if (ext <= 0) return;
    const double step = ext * 0.06;          // モックの 0.06 を領域スケールへ
    const int N = 28;
    const double zPlane = m_cz - ext * 0.05;

    p.setPen(Qt::NoPen);
    for (int i = -N; i <= N; ++i)
        for (int j = -N; j <= N; ++j) {
            if ((i + j) % 2 != 0) continue;   // 市松 (モックと同じ間引き)
            const double x = i * step, y = j * step;
            const double r = std::sqrt(x*x + y*y) / ext * 2.0;
            const double v = std::sin(r * 8.0 - m_animTick * 0.08) * std::exp(-r * 0.6);
            const double a = std::min(1.0, std::fabs(v) * 0.8);
            if (a < 0.02) continue;
            QColor c(v > 0 ? "#EF4444" : "#3B82F6");
            c.setAlphaF(a);
            p.setBrush(c);
            p.drawEllipse(projectPoint(m_cx + x, m_cy + y, zPlane), 1.6, 1.6);
        }
}

// レイトレースオーバーレイ — モックと同じ 24本 × 最大4反射。
// 領域境界で支配軸を反転させ、反射ごとにエネルギーを 0.7 倍する。
void Viewport3D::drawRayOverlay(QPainter &p)
{
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
