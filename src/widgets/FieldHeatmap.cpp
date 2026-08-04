// FieldHeatmap.cpp
#include "FieldHeatmap.h"
#include "../I18n.h"

#include <QFontInfo>
#include <QPainter>
#include <QtMath>

using namespace ofd;

namespace {
const bool s_i18n = [] {
    // プレースホルダ表示の明示 (実行結果と誤読させない — 絶対規則 5)
    ofd::I18n::reg("fh_demo",
        "デモ表示 — 実行結果ではありません "
        "(計算を実行するか結果 HDF5 を開くと実データに替わります)",
        "Demo pattern — not a simulation result "
        "(run a simulation or open a result HDF5 to replace it)");
    return true;
}();
} // namespace

FieldHeatmap::FieldHeatmap(QWidget *parent) : QWidget(parent)
{
    setMinimumHeight(240);
    fillDemoPattern();
}

// モックの解析パターン (v = |sin(4r)·exp(-0.4r)|)。
// setData が呼ばれるまで / clearData() で戻したときはデモ表示バナー付きで描く。
void FieldHeatmap::fillDemoPattern()
{
    const int n = 50;
    m_cols = m_rows = n;
    m_cells.resize(n * n);
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j) {
            const double x = (i - n / 2.0) / n * 4.0;
            const double y = (j - n / 2.0) / n * 4.0;
            const double r = std::sqrt(x * x + y * y);
            m_cells[j * n + i] = std::fabs(std::sin(r * 4.0) * std::exp(-r * 0.4));
        }
    m_demo = true;
}

// 実データを捨ててデモ表示へ戻す。プロジェクトを切り替えたときに
// 前のプロジェクトの結果が残らないようにするために使う (gui.md の規則)。
void FieldHeatmap::clearData()
{
    if (m_demo) return;
    fillDemoPattern();
    m_title.clear();
    update();
}

void FieldHeatmap::setData(const QVector<double> &cells, int cols, int rows)
{
    if (cols > 0 && rows > 0 && cells.size() >= cols * rows) {
        m_cells = cells;
        m_cols = cols;
        m_rows = rows;
        m_demo = false;
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
        // アプリ QSS はフォントを px で指定しているため pointSizeF() は -1 を
        // 返す。実効サイズは QFontInfo から取り、px 側で 1 段大きくする。
        QFont f = p.font();
        f.setPixelSize(QFontInfo(f).pixelSize() + 1);
        f.setBold(true);
        p.setFont(f);
        p.drawText(QRect(0, 0, width(), titleH), Qt::AlignLeft | Qt::AlignVCenter,
                   m_title);
        p.setFont(QFont());
    }

    // ── ヒートマップ本体 (黒背景 + セル塗り) ──
    p.fillRect(area, Qt::black);
    const double cw = double(area.width()) / m_cols;
    const double chh = double(area.height()) / m_rows;
    for (int i = 0; i < m_cols; ++i)
        for (int j = 0; j < m_rows; ++j) {
            const double v = m_cells[j * m_cols + i];
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

    // ── プレースホルダの明示バナー (setData 前は実行結果ではない) ──
    if (m_demo) {
        const QRect band(area.left(), area.bottom() - 24,
                         area.width(), 24);
        p.fillRect(band, QColor(0, 0, 0, 170));
        p.setPen(QColor("#FFD54F"));
        QFont f = p.font();
        f.setPointSizeF(9);
        p.setFont(f);
        p.drawText(band.adjusted(6, 0, -6, 0),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   I18n::tr("fh_demo"));
    }
}
