// MeshPreview.cpp
#include "MeshPreview.h"
#include "../core/Project.h"

#include <QPainter>

using namespace ofd;

MeshPreview::MeshPreview(Project *project, QWidget *parent)
    : QWidget(parent), m_p(project)
{
    setMinimumHeight(240);
    connect(project, &Project::changed, this, [this] { update(); });
    connect(project, &Project::loaded,  this, [this] { update(); });
}

// 軸のノード列を実座標のグリッド線位置へ展開する
static QVector<double> expandAxis(const MeshAxis &ax)
{
    QVector<double> out;
    if (!ax.isValid()) return out;
    for (int i = 0; i < ax.divs.size(); ++i) {
        const double a = ax.nodes[i], b = ax.nodes[i + 1];
        for (int k = 0; k < ax.divs[i]; ++k)
            out.push_back(a + (b - a) * k / ax.divs[i]);
    }
    out.push_back(ax.nodes.last());
    return out;
}

void MeshPreview::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    // 平面ごとの (横軸, 縦軸) の軸番号
    const int ha = (m_plane == 0) ? 0 : (m_plane == 1) ? 1 : 2;
    const int va = (m_plane == 0) ? 1 : (m_plane == 1) ? 2 : 0;
    static const char *planeName[3] = { "XY", "YZ", "ZX" };
    static const char *axName[3]    = { "X", "Y", "Z" };

    const QVector<double> hs = expandAxis(m_p->mesh(ha));
    const QVector<double> vs = expandAxis(m_p->mesh(va));

    const int headH = 22;
    p.setPen(palette().color(QPalette::WindowText));
    if (hs.size() < 2 || vs.size() < 2) {
        p.drawText(rect(), Qt::AlignCenter,
                   QStringLiteral("メッシュ未定義 / no mesh"));
        return;
    }
    p.drawText(QRect(0, 0, width(), headH), Qt::AlignLeft | Qt::AlignVCenter,
        QStringLiteral("%1 平面メッシュ — %2 %3 × %4 = %5 cells")
            .arg(planeName[m_plane])
            .arg(axName[ha], axName[va])
            .arg(m_p->mesh(ha).totalCells())
            .arg(m_p->mesh(va).totalCells())
            .arg(qint64(m_p->mesh(ha).totalCells()) * m_p->mesh(va).totalCells()));

    QRect area(8, headH + 4, width() - 16, height() - headH - 12);
    if (area.width() < 20 || area.height() < 20) return;
    p.fillRect(area, palette().color(QPalette::Base));

    const double h0 = hs.first(), h1 = hs.last();
    const double v0 = vs.first(), v1 = vs.last();
    const double sx = (h1 > h0) ? area.width()  / (h1 - h0) : 1.0;
    const double sy = (v1 > v0) ? area.height() / (v1 - v0) : 1.0;
    const auto mapX = [&](double x) { return area.left() + (x - h0) * sx; };
    // 縦軸は上向きが正になるよう反転
    const auto mapY = [&](double y) { return area.bottom() - (y - v0) * sy; };

    // ── グリッド線 (5本ごとに太線: モックと同じ強調) ──
    for (int i = 0; i < hs.size(); ++i) {
        const bool major = (i % 5 == 0);
        QColor c("#0078D4");
        c.setAlphaF(major ? 1.0 : 0.5);
        p.setPen(QPen(c, major ? 0.8 : 0.3));
        p.drawLine(QPointF(mapX(hs[i]), area.top()),
                   QPointF(mapX(hs[i]), area.bottom()));
    }
    for (int j = 0; j < vs.size(); ++j) {
        const bool major = (j % 5 == 0);
        QColor c("#0078D4");
        c.setAlphaF(major ? 1.0 : 0.5);
        p.setPen(QPen(c, major ? 0.8 : 0.3));
        p.drawLine(QPointF(area.left(),  mapY(vs[j])),
                   QPointF(area.right(), mapY(vs[j])));
    }

    // ── 物体形状ユニットを点線矩形で重ね描き ──
    QFont mono("Menlo");
    mono.setPointSizeF(9);
    p.setFont(mono);
    int unit = 1;
    for (const Geometry &g : m_p->geometries()) {
        // g[0..5] = X1 X2 Y1 Y2 Z1 Z2 → 表示平面の 2 軸を取り出す
        const double a1 = g.g[ha * 2], a2 = g.g[ha * 2 + 1];
        const double b1 = g.g[va * 2], b2 = g.g[va * 2 + 1];
        const QRectF r(QPointF(mapX(std::min(a1, a2)), mapY(std::max(b1, b2))),
                       QPointF(mapX(std::max(a1, a2)), mapY(std::min(b1, b2))));
        if (r.width() < 1 || r.height() < 1) { ++unit; continue; }

        // PEC (材質1) は橙、それ以外は青系 — モックの patch/substrate 配色
        const bool pec = (g.materialId == 1);
        const QColor edge = pec ? QColor("#D97706") : QColor("#3B82F6");
        QColor fill = edge;
        fill.setAlphaF(pec ? 0.15 : 0.08);
        p.setBrush(fill);
        p.setPen(QPen(edge, pec ? 1.5 : 1.0, Qt::DashLine));
        p.drawRect(r);
        p.setBrush(Qt::NoBrush);
        p.setPen(edge);
        p.drawText(r.adjusted(3, 2, -2, -2), Qt::AlignLeft | Qt::AlignTop,
            g.name.isEmpty()
                ? QStringLiteral("#%1 mat%2").arg(unit).arg(g.materialId)
                : g.name);
        ++unit;
    }

    p.setPen(QColor("#d4d4d4"));
    p.setBrush(Qt::NoBrush);
    p.drawRect(area.adjusted(0, 0, -1, -1));
}
