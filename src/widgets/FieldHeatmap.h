// FieldHeatmap.h — 2D 断面の界分布ヒートマップ (app.jsx FieldHeatmap 相当)。
//
// 近傍界の面上分布を jet カラーマップで表示する。カーネルの近傍界出力を
// まだ読み込んでいない状態ではモックと同じ解析パターン
//   v = |sin(4r) · exp(-0.4r)|
// をプレースホルダとして描く (モックの見た目をそのまま再現)。
// 右側にカラーバー (0.0〜1.0, |E| V/m) を添える。
#pragma once
#include <QVector>
#include <QWidget>

namespace ofd {

class FieldHeatmap : public QWidget {
    Q_OBJECT
public:
    explicit FieldHeatmap(QWidget *parent = nullptr);

    // 実データ (row-major, n×n, 0..1 正規化済み) を与えると解析パターンを置換する
    void setData(const QVector<double> &cells, int n);
    void setTitle(const QString &t) { m_title = t; update(); }

    static QColor jet(double t);      // 0..1 → jet 色

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QVector<double> m_cells;
    int      m_n = 50;
    QString  m_title;
    bool     m_demo = true;   // まだ setData されていない = プレースホルダ表示中
};

} // namespace ofd
