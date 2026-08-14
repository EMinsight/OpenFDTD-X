// MiniPlot.cpp
#include "MiniPlot.h"

#include <QPainter>
#include <QPainterPath>
#include <cmath>

using namespace ofd;

MiniPlot::MiniPlot(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(220, 110);
}

void MiniPlot::setSeries(const QVector<MiniSeries> &s)
{
    m_series = s;
    update();
}

void MiniPlot::setLabels(const QString &x, const QString &y)
{
    m_xLabel = x;
    m_yLabel = y;
    update();
}

void MiniPlot::setYRange(double lo, double hi)
{
    m_fixedY = true;
    m_yLo = lo;
    m_yHi = hi;
    update();
}

void MiniPlot::clearYRange()
{
    m_fixedY = false;
    update();
}

void MiniPlot::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), palette().base());

    const QRectF plot(44, 10, width() - 56, height() - 40);
    p.setPen(QPen(palette().mid().color(), 1));
    p.drawRect(plot);

    // ranges
    double xLo = 1e300, xHi = -1e300, yLo = 1e300, yHi = -1e300;
    for (const MiniSeries &s : m_series)
        for (const QPointF &pt : s.pts) {
            xLo = std::min(xLo, pt.x()); xHi = std::max(xHi, pt.x());
            yLo = std::min(yLo, pt.y()); yHi = std::max(yHi, pt.y());
        }
    if (xLo > xHi) { xLo = 0; xHi = 1; }
    if (m_fixedY) { yLo = m_yLo; yHi = m_yHi; }
    if (yLo >= yHi) { yLo -= 0.5; yHi += 0.5; }
    if (xLo >= xHi) { xLo -= 0.5; xHi += 0.5; }
    // x 方向の余白 (既定 0 = 従来どおり)。端にある点 (動作点のマーカーなど)
    // が枠に重なって見えなくなるのを防ぐため、使う側が明示的に足す。
    if (m_xMargin > 0.0) {
        const double xPad = (xHi - xLo) * m_xMargin;
        xLo -= xPad; xHi += xPad;
    }
    const double yPad = m_fixedY ? 0 : (yHi - yLo) * 0.08;
    yLo -= yPad; yHi += yPad;

    auto X = [&](double x) { return plot.left() + (x - xLo) / (xHi - xLo) * plot.width(); };
    auto Y = [&](double y) { return plot.bottom() - (y - yLo) / (yHi - yLo) * plot.height(); };

    // grid + tick labels
    p.setPen(QPen(palette().midlight().color(), 1, Qt::DotLine));
    QFont f = p.font();
    f.setPointSizeF(7.5);
    p.setFont(f);
    for (int i = 0; i <= 4; ++i) {
        const double gx = xLo + (xHi - xLo) * i / 4.0;
        const double gy = yLo + (yHi - yLo) * i / 4.0;
        p.setPen(QPen(palette().midlight().color(), 1, Qt::DotLine));
        p.drawLine(QPointF(X(gx), plot.top()), QPointF(X(gx), plot.bottom()));
        p.drawLine(QPointF(plot.left(), Y(gy)), QPointF(plot.right(), Y(gy)));
        p.setPen(palette().text().color());
        p.drawText(QRectF(X(gx) - 30, plot.bottom() + 2, 60, 12), Qt::AlignCenter,
                   QString::number(m_xPow10 ? std::pow(10.0, gx) : gx, 'g', 4));
        p.drawText(QRectF(0, Y(gy) - 6, plot.left() - 4, 12),
                   Qt::AlignRight | Qt::AlignVCenter, QString::number(gy, 'g', 3));
    }

    // series
    for (const MiniSeries &s : m_series) {
        QPen pen(s.color, 1.8);
        if (s.dashed) pen.setStyle(Qt::DashLine);
        p.setPen(pen);
        if (m_impulse) {
            for (const QPointF &pt : s.pts)
                p.drawLine(QPointF(X(pt.x()), plot.bottom()),
                           QPointF(X(pt.x()), Y(pt.y())));
        } else {
            QPainterPath path;
            for (int i = 0; i < s.pts.size(); ++i) {
                const QPointF sp(X(s.pts[i].x()), Y(s.pts[i].y()));
                if (i == 0) path.moveTo(sp); else path.lineTo(sp);
            }
            p.drawPath(path);
        }
        if (s.markers) {
            p.setBrush(s.color);
            for (const QPointF &pt : s.pts)
                p.drawEllipse(QPointF(X(pt.x()), Y(pt.y())), 2.4, 2.4);
            p.setBrush(Qt::NoBrush);
        }
    }

    // labels + legend
    p.setPen(palette().text().color());
    p.drawText(QRectF(plot.left(), height() - 15, plot.width(), 14),
               Qt::AlignCenter, m_xLabel);
    p.save();
    p.translate(10, plot.center().y());
    p.rotate(-90);
    p.drawText(QRectF(-60, -6, 120, 12), Qt::AlignCenter, m_yLabel);
    p.restore();

    double lx = plot.left() + 6;
    for (const MiniSeries &s : m_series) {
        if (s.label.isEmpty()) continue;
        p.setPen(s.color);
        p.drawText(QPointF(lx, plot.top() + 12), s.label);
        lx += p.fontMetrics().horizontalAdvance(s.label) + 14;
    }
}
