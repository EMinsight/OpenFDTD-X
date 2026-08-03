// AudioWaveformView.h — 音響編集タブの波形 / スペクトログラム表示
// (元 mock: audio-editor.jsx の canvas 描画部)。
//
// QPainter 描画のみ (外部ライブラリ不使用)。ドラッグで範囲選択し、
// selectionChanged(startSample, endSample) を発行する。スペクトログラムは
// バッファ / 窓関数の変更時にのみ再計算して QImage にキャッシュする
// (再描画のたびの FFT を避ける)。
#pragma once
#include <QImage>
#include <QWidget>
#include <cstddef>

#include "../audio/AudioEditEngine.h"

namespace ofd {

class AudioWaveformView : public QWidget {
    Q_OBJECT
public:
    enum class ViewMode { Waveform, Spectrogram };

    explicit AudioWaveformView(QWidget *parent = nullptr);

    // バッファは所有しない (呼び出し側が寿命を管理)。nullptr = 信号なし
    void setBuffer(const audioedit::AudioBuffer *buf);
    void setViewMode(ViewMode mode);
    void setWindowKind(audioedit::WindowKind w);

    bool hasSelection() const { return m_selEnd > m_selStart; }
    std::size_t selectionStart() const { return m_selStart; }
    std::size_t selectionEnd() const { return m_selEnd; }
    void clearSelection();

signals:
    // 範囲選択の確定 (start == end は選択解除)
    void selectionChanged(std::size_t startSample, std::size_t endSample);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void resizeEvent(QResizeEvent *) override;

private:
    std::size_t sampleAt(int x) const;
    void rebuildSpectrogram();

    const audioedit::AudioBuffer *m_buf = nullptr;
    ViewMode    m_mode = ViewMode::Waveform;
    audioedit::WindowKind m_window = audioedit::WindowKind::Hann;
    std::size_t m_selStart = 0, m_selEnd = 0;
    bool        m_dragging = false;
    std::size_t m_dragAnchor = 0;
    QImage      m_specImage;      // スペクトログラムのキャッシュ
    bool        m_specDirty = true;
};

} // namespace ofd
