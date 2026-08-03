// AudioWaveformView.cpp
#include "AudioWaveformView.h"
#include "../I18n.h"

#include <QMouseEvent>
#include <QPainter>
#include <algorithm>
#include <cmath>

using namespace ofd;

namespace {
// ── ウィジェット固有語彙 (awv_) ─────────────────────────────────────────────
const bool s_i18n = [] {
    I18n::reg("awv_empty", "信号なし — 「生成」または「読込」から開始",
              "No signal — start with Generate or Load");
    return true;
}();

// magma 風の擬似カラー (mock の r/g/b 式をそのまま移植)
QColor magmaColor(double p)
{
    const int r = static_cast<int>(255 * std::min(1.0, p * 2.0));
    const int g = static_cast<int>(
        255 * std::max(0.0, std::min(1.0, p * 1.9 - 0.55)));
    const int b = static_cast<int>(255 * std::max(0.0, std::min(1.0,
        p < 0.5 ? p * 1.6 : 1.7 - p * 1.5)));
    return QColor(r, g, b);
}
} // namespace

AudioWaveformView::AudioWaveformView(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(190);
    setCursor(Qt::CrossCursor);
    setAutoFillBackground(false);
}

void AudioWaveformView::setBuffer(const audioedit::AudioBuffer *buf)
{
    m_buf = buf;
    m_specDirty = true;
    if (!m_buf || m_selEnd > m_buf->sampleCount())
        clearSelection();
    update();
}

void AudioWaveformView::setViewMode(ViewMode mode)
{
    if (m_mode == mode) return;
    m_mode = mode;
    update();
}

void AudioWaveformView::setWindowKind(audioedit::WindowKind w)
{
    if (m_window == w) return;
    m_window = w;
    m_specDirty = true;
    update();
}

void AudioWaveformView::clearSelection()
{
    m_selStart = m_selEnd = 0;
    update();
}

std::size_t AudioWaveformView::sampleAt(int x) const
{
    if (!m_buf || m_buf->sampleCount() == 0 || width() <= 0) return 0;
    const double frac = std::max(0.0, std::min(1.0,
        static_cast<double>(x) / width()));
    return static_cast<std::size_t>(frac * m_buf->sampleCount());
}

void AudioWaveformView::mousePressEvent(QMouseEvent *e)
{
    if (!m_buf) return;
    m_dragging = true;
    m_dragAnchor = sampleAt(e->pos().x());
    m_selStart = m_selEnd = m_dragAnchor;
    update();
}

void AudioWaveformView::mouseMoveEvent(QMouseEvent *e)
{
    if (!m_dragging || !m_buf) return;
    const std::size_t p = sampleAt(e->pos().x());
    m_selStart = std::min(m_dragAnchor, p);
    m_selEnd   = std::max(m_dragAnchor, p);
    update();
}

void AudioWaveformView::mouseReleaseEvent(QMouseEvent *)
{
    if (!m_dragging) return;
    m_dragging = false;
    // 32 サンプル未満のドラッグはクリック扱い = 選択解除 (mock と同じ)
    if (m_selEnd - m_selStart < 32)
        clearSelection();
    emit selectionChanged(m_selStart, m_selEnd);
    update();
}

void AudioWaveformView::resizeEvent(QResizeEvent *)
{
    m_specDirty = true;
}

void AudioWaveformView::rebuildSpectrogram()
{
    if (!m_buf || width() <= 0 || height() <= 0) {
        m_specImage = QImage();
        return;
    }
    const int W = width(), H = height();
    const std::vector<float> map =
        audioedit::spectrogram(*m_buf, W, H, m_window);
    m_specImage = QImage(W, H, QImage::Format_RGB32);
    if (map.size() < static_cast<std::size_t>(W) * H) {
        m_specImage.fill(Qt::black);
        return;
    }
    for (int y = 0; y < H; ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(m_specImage.scanLine(y));
        for (int x = 0; x < W; ++x)
            line[x] = magmaColor(map[static_cast<std::size_t>(y) * W + x]).rgb();
    }
    m_specDirty = false;
}

void AudioWaveformView::paintEvent(QPaintEvent *)
{
    QPainter g(this);
    const int W = width(), H = height();
    g.fillRect(rect(), palette().base());

    if (!m_buf || m_buf->sampleCount() == 0 || m_buf->channelCount() == 0) {
        g.setPen(palette().color(QPalette::Mid));
        g.drawText(rect(), Qt::AlignCenter, I18n::tr("awv_empty"));
        return;
    }

    const std::vector<double> &d = m_buf->channels[0];
    const std::size_t N = d.size();

    if (m_mode == ViewMode::Waveform) {
        // 選択範囲
        if (hasSelection()) {
            const double x0 = static_cast<double>(m_selStart) / N * W;
            const double x1 = static_cast<double>(m_selEnd) / N * W;
            g.fillRect(QRectF(x0, 0, x1 - x0, H), QColor(90, 150, 220, 56));
        }
        // ゼロライン
        g.setPen(palette().color(QPalette::Midlight));
        g.drawLine(0, H / 2, W, H / 2);
        // min/max 包絡線 (1 ピクセル 1 縦線)
        g.setPen(QColor(0, 120, 212));
        const std::size_t step =
            std::max<std::size_t>(1, N / std::max(1, W));
        for (int x = 0; x < W; ++x) {
            double mn = 1.0, mx = -1.0;
            const std::size_t begin = static_cast<std::size_t>(x) * step;
            for (std::size_t i = begin; i < begin + step && i < N; ++i) {
                mn = std::min(mn, d[i]);
                mx = std::max(mx, d[i]);
            }
            if (mn > mx) continue;
            g.drawLine(QPointF(x + 0.5, H / 2.0 - mx * H * 0.47),
                       QPointF(x + 0.5, H / 2.0 - mn * H * 0.47));
        }
    } else {
        if (m_specDirty) rebuildSpectrogram();
        if (!m_specImage.isNull())
            g.drawImage(0, 0, m_specImage);
        if (hasSelection()) {
            const double x0 = static_cast<double>(m_selStart) / N * W;
            const double x1 = static_cast<double>(m_selEnd) / N * W;
            g.setPen(Qt::white);
            g.drawRect(QRectF(x0, 0, x1 - x0, H - 1));
        }
    }
}
