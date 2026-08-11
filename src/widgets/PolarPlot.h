// PolarPlot.h — 配光曲線 (極座標) の描画 (QPainter, チャートライブラリ不要)。
//
// 軸対称の光度分布 I(θ) を、上向きを θ = 0 として左右対称に描く
// (照明分野の慣行どおり下向き = θ = 180° が真下)。同心円は光度の等値線で、
// 放射状の目盛は 30° ごと。半値 (I(0)/2) の円を破線で重ね、ビーム角
// (FWHM) が図の上で読めるようにする。
//
// 値は「ビン中心の光度」の列として渡す。ビン幅は 180° / 個数。
#pragma once
#include <QString>
#include <QVector>
#include <QWidget>

namespace ofd {

class PolarPlot : public QWidget {
    Q_OBJECT
public:
    explicit PolarPlot(QWidget *parent = nullptr);

    // values[k] = θ = (k+0.5)·180/N での光度。単位はラベル用の文字列。
    void setData(const QVector<double> &values, const QString &unit);
    void setTitle(const QString &t) { m_title = t; update(); }
    // 半値円を描くかどうか (既定 on)
    void setHalfCircle(bool on) { m_half = on; update(); }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QVector<double> m_v;
    QString m_unit, m_title;
    bool    m_half = true;
};

} // namespace ofd
