// PolarPlot.cpp — 配光曲線 (極座標) の描画 (詳細は .h)
#include "PolarPlot.h"

#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>

#include <algorithm>
#include <cmath>

using namespace ofd;

namespace {
constexpr double kPi = 3.14159265358979323846;

// 目盛の刻みを 1/2/5 × 10^n に丸める
double niceStep(double raw)
{
    if (!(raw > 0.0)) return 1.0;
    const double e = std::pow(10.0, std::floor(std::log10(raw)));
    const double m = raw / e;
    if (m <= 1.0) return e;
    if (m <= 2.0) return 2.0 * e;
    if (m <= 5.0) return 5.0 * e;
    return 10.0 * e;
}
} // namespace

PolarPlot::PolarPlot(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(360, 320);
}

void PolarPlot::setData(const QVector<double> &values, const QString &unit)
{
    m_v = values;
    m_unit = unit;
    update();
}

void PolarPlot::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), palette().color(QPalette::Base));

    const QColor fg = palette().color(QPalette::WindowText);
    const QColor grid("#d4d4d4");

    const int titleH = m_title.isEmpty() ? 0 : 22;
    if (titleH) {
        QFont tf = p.font();
        tf.setBold(true);
        p.setFont(tf);
        p.setPen(fg);
        p.drawText(QRect(0, 4, width(), 18), Qt::AlignHCenter, m_title);
        p.setFont(QFont());
    }

    // 上向き θ = 0、下向き θ = 180° (照明分野の慣行)
    const int margin = 34;
    const QRect area(margin, titleH + margin,
                     width() - 2 * margin, height() - titleH - 2 * margin);
    const double R = 0.5 * std::min(area.width(), area.height());
    if (R < 20.0) return;
    const QPointF c(area.center());

    double vmax = 0.0;
    for (double v : m_v) vmax = std::max(vmax, v);
    if (!(vmax > 0.0)) {
        p.setPen(fg);
        p.drawText(area, Qt::AlignCenter, QStringLiteral("—"));
        return;
    }
    const double step = niceStep(vmax / 4.0);
    const double full = std::ceil(vmax / step) * step;

    // ── 同心円 (光度の等値線) ──────────────────────────────────────────────
    p.setPen(QPen(grid, 1.0));
    QFont mono("Menlo");
    mono.setPointSizeF(8.5);
    for (double v = step; v <= full + 1e-9; v += step) {
        const double r = R * v / full;
        p.drawEllipse(c, r, r);
        p.setFont(mono);
        p.setPen(QColor("#909090"));
        p.drawText(QRectF(c.x() + 3, c.y() - r - 13, 70, 12), Qt::AlignLeft,
                   QString::number(v, 'g', 3));
        p.setPen(QPen(grid, 1.0));
    }

    // ── 放射状の目盛 (30° ごと) ────────────────────────────────────────────
    p.setFont(mono);
    for (int a = 0; a < 360; a += 30) {
        const double rad = (a - 90) * kPi / 180.0;   // 画面上で真上が 0°
        const QPointF e(c.x() + R * std::cos(rad), c.y() + R * std::sin(rad));
        p.setPen(QPen(grid, 1.0));
        p.drawLine(c, e);
        // ラベルは θ (0 が上、180 が下)。左右は対称なので絶対値で書く
        const int th = (a <= 180) ? a : 360 - a;
        const QPointF t(c.x() + (R + 15) * std::cos(rad),
                        c.y() + (R + 15) * std::sin(rad));
        p.setPen(QColor("#909090"));
        p.drawText(QRectF(t.x() - 16, t.y() - 7, 32, 14), Qt::AlignCenter,
                   QString::number(th) + QString::fromUtf8("°"));
    }

    // ── 半値円 (ビーム角がこの円との交点で読める) ──────────────────────────
    if (m_half && !m_v.isEmpty() && m_v[0] > 0.0) {
        const double rh = R * (0.5 * m_v[0]) / full;
        if (rh > 1.0 && rh < R) {
            QPen hp(QColor("#E06C00"), 1.2, Qt::DashLine);
            p.setPen(hp);
            p.drawEllipse(c, rh, rh);
        }
    }

    // ── 配光曲線 (軸対称なので左右へ折り返す) ──────────────────────────────
    const int n = m_v.size();
    const double dth = 180.0 / n;
    QPainterPath path;
    auto point = [&](int k, int sign) {
        const double th = dth * (k + 0.5);
        const double rad = (sign * th - 90.0) * kPi / 180.0;
        const double r = R * m_v[k] / full;
        return QPointF(c.x() + r * std::cos(rad), c.y() + r * std::sin(rad));
    };
    for (int k = 0; k < n; ++k) {                 // 右半分 (θ 増加)
        const QPointF q = point(k, +1);
        if (k == 0) path.moveTo(q); else path.lineTo(q);
    }
    for (int k = n - 1; k >= 0; --k)              // 左半分 (折り返し)
        path.lineTo(point(k, -1));
    path.closeSubpath();

    p.setPen(QPen(QColor("#0078D4"), 1.8));
    p.setBrush(QColor(0, 120, 212, 40));
    p.drawPath(path);

    // ── 単位 ───────────────────────────────────────────────────────────────
    if (!m_unit.isEmpty()) {
        p.setPen(fg);
        p.setFont(mono);
        p.drawText(QRect(4, height() - 18, width() - 8, 14),
                   Qt::AlignRight, m_unit);
    }
}
