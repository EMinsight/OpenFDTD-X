// FieldHeatmap.cpp
#include "FieldHeatmap.h"

#include <QPainter>
#include <QtMath>

using namespace ofd;

FieldHeatmap::FieldHeatmap(QWidget *parent) : QWidget(parent)
{
    setMinimumHeight(240);
    // モックの解析パターンを初期表示に (v = |sin(4r)·exp(-0.4r)|)
    const int n = m_n;
    m_cells.resize(n * n);
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j) {
            const double x = (i - n / 2.0) / n * 4.0;
            const double y = (j - n / 2.0) / n * 4.0;
            const double r = std::sqrt(x * x + y * y);
            m_cells[j * n + i] = std::fabs(std::sin(r * 4.0) * std::exp(-r * 0.4));
        }
}

void FieldHeatmap::setData(const QVector<double> &cells, int n)
{
    if (n > 0 && cells.size() >= n * n) {
        m_cells = cells;
        m_n = n;
        update();
    }
}

// jet 風カラーマップ (モックの colorMap と同じ折れ線)
QColor FieldHeatmap::jet(double t)
{
    t = qBound(0.0, t, 1.0);
    const auto ch = [](double v) {
        return int(255.0 * qBound(0.0, v, 1.0));
    };
    return QColor(ch(1.5 - std::fabs(4 * t - 3)),
                  ch(1.5 - std::fabs(4 * t - 2)),
                  ch(1.5 - std::fabs(4 * t - 1)));
}

void FieldHeatmap::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    const int barW = 58;
    const int titleH = m_title.isEmpty() ? 0 : 22;
    QRect area(0, titleH, width() - barW - 10, height() - titleH);
    if (area.width() <= 0 || area.height() <= 0) return;

    if (titleH) {
        p.setPen(palette().color(QPalette::Text));
        QFont f = p.font();
        f.setPointSizeF(f.pointSizeF() + 1);
        f.setBold(true);
        p.setFont(f);
        p.drawText(QRect(0, 0, width(), titleH), Qt::AlignLeft | Qt::AlignVCenter,
                   m_title);
        p.setFont(QFont());
    }

    // ── ヒートマップ本体 (黒背景 + セル塗り) ──
    p.fillRect(area, Qt::black);
    const double cw = double(area.width()) / m_n;
    const double chh = double(area.height()) / m_n;
    for (int i = 0; i < m_n; ++i)
        for (int j = 0; j < m_n; ++j) {
            const double v = m_cells[j * m_n + i];
            p.fillRect(QRectF(area.left() + i * cw, area.top() + j * chh,
                              cw + 1.0, chh + 1.0), jet(v));
        }
    p.setPen(QColor("#d4d4d4"));
    p.drawRect(area.adjusted(0, 0, -1, -1));

    // ── カラーバー ──
    const int bx = width() - barW + 12;
    const int by = area.top() + 18;
    const int bh = area.height() - 52;
    if (bh > 10) {
        QLinearGradient g(0, by + bh, 0, by);   // 下=0.0, 上=1.0
        for (int s = 0; s <= 10; ++s)
            g.setColorAt(s / 10.0, jet(s / 10.0));
        p.fillRect(QRect(bx, by, 18, bh), g);
        p.setPen(QColor("#d4d4d4"));
        p.drawRect(QRect(bx, by, 18, bh));

        p.setPen(palette().color(QPalette::WindowText));
        QFont mono("Menlo");
        mono.setPointSizeF(9);
        p.setFont(mono);
        p.drawText(QRect(bx - 6, by - 16, 40, 14), Qt::AlignLeft, "1.0");
        p.drawText(QRect(bx - 6, by + bh + 2, 40, 14), Qt::AlignLeft, "0.0");
        p.drawText(QRect(bx - 6, by + bh + 18, 40, 14), Qt::AlignLeft, "|E|");
        mono.setPointSizeF(8);
        p.setFont(mono);
        p.drawText(QRect(bx - 6, by + bh + 32, 40, 14), Qt::AlignLeft, "V/m");
    }
}
