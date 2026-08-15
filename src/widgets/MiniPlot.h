// MiniPlot.h — small reusable XY line plot (QPainter, no chart dependency).
// Equivalent of the React <MiniPlot> in the mock: one or more series with
// axis labels, auto or fixed Y range. Used by GlassCatalogTab (dispersion
// curve) and RoomAcousticsTab (RT60 by band, NC curves, echogram …).
//
// 対話機能 (負債 #9):
//   - ホバー: 最寄りの点へスナップして値を読み出す (遠いときはカーソル座標)
//   - ドラッグ: ラバーバンドで拡大 (x/y 両方)。setSeries で解除される
//     (データが変わったのに古い視野を残すと空の図を黙って見せることになる)
//   - ダブルクリック: 全体表示へ戻す
#pragma once
#include <QColor>
#include <QString>
#include <QVector>
#include <QPointF>
#include <QWidget>

namespace ofd {

struct MiniSeries {
    QVector<QPointF> pts;      // x ascending
    QColor  color = QColor("#0078D4");
    bool    dashed = false;
    bool    markers = false;
    QString label;
};

class MiniPlot : public QWidget {
    Q_OBJECT
public:
    explicit MiniPlot(QWidget *parent = nullptr);

    void setSeries(const QVector<MiniSeries> &s);
    void setLabels(const QString &x, const QString &y);
    void setYRange(double lo, double hi);   // fixed; call clearYRange to auto
    void clearYRange();
    // 棒 (impulse) モード: 各点を x 位置の縦線として描く (エコーグラム用)
    void setImpulseMode(bool on) { m_impulse = on; update(); }
    // x が log10 値のとき、目盛りを 10^x (実周波数) で表示する
    void setXTickPow10(bool on) { m_xPow10 = on; update(); }
    // x 範囲の左右に data 幅の frac 倍だけ余白を足す (既定 0 = 従来どおり)。
    // 端に来る点が枠と重なって見えなくなる図でだけ使う。
    void setXMargin(double frac) { m_xMargin = frac; update(); }

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    // 描画とマウス処理が共有する視野 (プロット矩形とデータ範囲)。
    // paintEvent の中だけで計算していた写像を取り出したもの — 2 か所で
    // 別々に計算すると必ずずれる。
    struct View {
        QRectF plot;
        double xLo = 0, xHi = 1, yLo = 0, yHi = 1;
        double xPix(double x) const;
        double yPix(double y) const;
        double xData(double px) const;
        double yData(double py) const;
    };
    View computeView() const;

    QVector<MiniSeries> m_series;
    QString m_xLabel, m_yLabel;
    bool    m_fixedY = false;
    double  m_yLo = 0, m_yHi = 1;
    bool    m_impulse = false;
    bool    m_xPow10 = false;
    double  m_xMargin = 0.0;

    // 対話状態
    bool    m_hover = false;
    QPoint  m_hoverPos;
    bool    m_dragging = false;
    QPoint  m_dragStart, m_dragCur;
    bool    m_zoomed = false;
    double  m_zxLo = 0, m_zxHi = 1, m_zyLo = 0, m_zyHi = 1;
};

} // namespace ofd
