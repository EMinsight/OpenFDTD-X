// MiniPlot.cpp
#include "MiniPlot.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <cmath>

#include "../I18n.h"

using namespace ofd;

namespace {

void registerStrings()
{
    static bool done = false;
    if (done) return;
    done = true;
    I18n::reg("mplot_hint",
              "ドラッグで拡大、ダブルクリックで全体表示",
              "Drag to zoom, double-click to reset");
}

// ドラッグをズームとみなす最小ピクセル数 (クリックの手ぶれを拡大にしない)
const int kMinDragPx = 8;
// この距離 (px) 以内に点があればスナップして系列値を出す
const double kSnapPx = 24.0;

} // namespace

double MiniPlot::View::xPix(double x) const
{ return plot.left() + (x - xLo) / (xHi - xLo) * plot.width(); }
double MiniPlot::View::yPix(double y) const
{ return plot.bottom() - (y - yLo) / (yHi - yLo) * plot.height(); }
double MiniPlot::View::xData(double px) const
{ return xLo + (px - plot.left()) / plot.width() * (xHi - xLo); }
double MiniPlot::View::yData(double py) const
{ return yLo + (plot.bottom() - py) / plot.height() * (yHi - yLo); }

MiniPlot::MiniPlot(QWidget *parent)
    : QWidget(parent)
{
    registerStrings();
    setMinimumSize(220, 110);
    setMouseTracking(true);
    setToolTip(I18n::tr("mplot_hint"));
}

void MiniPlot::setSeries(const QVector<MiniSeries> &s)
{
    m_series = s;
    // データが変わったら視野を戻す — 古い拡大範囲を残すと、範囲外へ移った
    // データを「何も無い図」として黙って見せてしまう
    m_zoomed = false;
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

MiniPlot::View MiniPlot::computeView() const
{
    View v;
    v.plot = QRectF(44, 10, width() - 56, height() - 40);

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

    if (m_zoomed) { xLo = m_zxLo; xHi = m_zxHi; yLo = m_zyLo; yHi = m_zyHi; }
    v.xLo = xLo; v.xHi = xHi; v.yLo = yLo; v.yHi = yHi;
    return v;
}

void MiniPlot::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), palette().base());

    const View v = computeView();
    const QRectF &plot = v.plot;
    p.setPen(QPen(palette().mid().color(), 1));
    p.drawRect(plot);

    auto X = [&](double x) { return v.xPix(x); };
    auto Y = [&](double y) { return v.yPix(y); };

    // grid + tick labels
    QFont f = p.font();
    f.setPointSizeF(7.5);
    p.setFont(f);
    for (int i = 0; i <= 4; ++i) {
        const double gx = v.xLo + (v.xHi - v.xLo) * i / 4.0;
        const double gy = v.yLo + (v.yHi - v.yLo) * i / 4.0;
        p.setPen(QPen(palette().midlight().color(), 1, Qt::DotLine));
        p.drawLine(QPointF(X(gx), plot.top()), QPointF(X(gx), plot.bottom()));
        p.drawLine(QPointF(plot.left(), Y(gy)), QPointF(plot.right(), Y(gy)));
        p.setPen(palette().text().color());
        p.drawText(QRectF(X(gx) - 30, plot.bottom() + 2, 60, 12), Qt::AlignCenter,
                   QString::number(m_xPow10 ? std::pow(10.0, gx) : gx, 'g', 4));
        p.drawText(QRectF(0, Y(gy) - 6, plot.left() - 4, 12),
                   Qt::AlignRight | Qt::AlignVCenter, QString::number(gy, 'g', 3));
    }

    // series — 拡大中は範囲外の点が枠の外へはみ出すのでクリップする
    p.save();
    p.setClipRect(plot);
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
    p.restore();

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

    // 拡大中の目印 (どの状態か分からないまま操作させない)
    if (m_zoomed) {
        p.setPen(palette().text().color());
        p.drawText(QRectF(plot.right() - 60, plot.top() + 2, 58, 12),
                   Qt::AlignRight, QStringLiteral("zoom"));
    }

    // ラバーバンド
    if (m_dragging) {
        const QRectF band = QRectF(m_dragStart, m_dragCur).normalized()
                                .intersected(plot);
        QColor fill = palette().highlight().color();
        fill.setAlpha(40);
        p.fillRect(band, fill);
        p.setPen(QPen(palette().highlight().color(), 1, Qt::DashLine));
        p.drawRect(band);
    }

    // ホバー読み値 (ドラッグ中は出さない — バンドと重なって読めない)
    if (m_hover && !m_dragging && plot.contains(m_hoverPos)) {
        // 最寄りの点 (ピクセル距離) を探す
        double best = 1e300;
        QPointF bestPt;
        QString bestLabel;
        for (const MiniSeries &s : m_series)
            for (const QPointF &pt : s.pts) {
                const double dx = X(pt.x()) - m_hoverPos.x();
                const double dy = Y(pt.y()) - m_hoverPos.y();
                const double d2 = dx * dx + dy * dy;
                if (d2 < best) { best = d2; bestPt = pt; bestLabel = s.label; }
            }

        double xv, yv;
        bool snapped = false;
        if (best <= kSnapPx * kSnapPx) {
            xv = bestPt.x(); yv = bestPt.y(); snapped = true;
        } else {
            xv = v.xData(m_hoverPos.x()); yv = v.yData(m_hoverPos.y());
        }
        const QString xs =
            QString::number(m_xPow10 ? std::pow(10.0, xv) : xv, 'g', 5);
        const QString ys = QString::number(yv, 'g', 5);
        QString text = xs + ", " + ys;
        if (snapped && !bestLabel.isEmpty()) text = bestLabel + ": " + text;

        // 十字線 (スナップ時は点の位置、そうでなければカーソル位置)
        const QPointF c = snapped ? QPointF(X(xv), Y(yv))
                                  : QPointF(m_hoverPos);
        p.setPen(QPen(palette().mid().color(), 1, Qt::DotLine));
        p.drawLine(QPointF(c.x(), plot.top()), QPointF(c.x(), plot.bottom()));
        p.drawLine(QPointF(plot.left(), c.y()), QPointF(plot.right(), c.y()));
        if (snapped) {
            p.setPen(QPen(palette().text().color(), 1));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(c, 3.5, 3.5);
        }

        // 読み値の箱 (枠の中に収まる側へ出す)
        const QFontMetricsF fm(p.font());
        const QSizeF sz(fm.horizontalAdvance(text) + 10, fm.height() + 6);
        QPointF tl = c + QPointF(8, -sz.height() - 8);
        if (tl.x() + sz.width() > plot.right())
            tl.setX(c.x() - sz.width() - 8);
        if (tl.y() < plot.top()) tl.setY(c.y() + 8);
        const QRectF box(tl, sz);
        QColor bg = palette().base().color();
        bg.setAlpha(230);
        p.setPen(QPen(palette().mid().color(), 1));
        p.setBrush(bg);
        p.drawRect(box);
        p.setPen(palette().text().color());
        p.drawText(box, Qt::AlignCenter, text);
        p.setBrush(Qt::NoBrush);
    }
}

void MiniPlot::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton) { QWidget::mousePressEvent(e); return; }
    m_dragging = true;
    m_dragStart = m_dragCur = e->pos();
    update();
}

void MiniPlot::mouseMoveEvent(QMouseEvent *e)
{
    m_hover = true;
    m_hoverPos = e->pos();
    if (m_dragging) m_dragCur = e->pos();
    update();
}

void MiniPlot::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton || !m_dragging) {
        QWidget::mouseReleaseEvent(e);
        return;
    }
    m_dragging = false;
    const QRect band = QRect(m_dragStart, e->pos()).normalized();
    if (band.width() >= kMinDragPx && band.height() >= kMinDragPx) {
        const View v = computeView();
        const QRectF r = QRectF(band).intersected(v.plot);
        if (r.width() > 2 && r.height() > 2) {
            // ピクセル → データ。y は上下が反転している点に注意
            m_zxLo = v.xData(r.left());
            m_zxHi = v.xData(r.right());
            m_zyLo = v.yData(r.bottom());
            m_zyHi = v.yData(r.top());
            if (m_zxLo < m_zxHi && m_zyLo < m_zyHi) m_zoomed = true;
        }
    }
    update();
}

void MiniPlot::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        m_zoomed = false;
        m_dragging = false;
        update();
    } else {
        QWidget::mouseDoubleClickEvent(e);
    }
}

void MiniPlot::leaveEvent(QEvent *e)
{
    m_hover = false;
    m_dragging = false;
    update();
    QWidget::leaveEvent(e);
}
