// Viewport3D.h — central 3D view of the project (mesh region, geometry
// units, feeds, probes). QPainter-based orthographic wireframe so that no
// OpenGL context is required (works in headless / remote sessions too).
//
// Mouse: left-drag = orbit, middle-drag = pan, wheel = zoom, double = fit.
#pragma once
#include <QImage>
#include <QString>
#include <QVector>
#include <QWidget>
#include <QPointF>
#include "../core/Domain.h"

namespace ofd {

class Project;

// モックの TweaksPanel「3D ビュー / Viewport」に対応する描画スタイル。
//   Wireframe — 形状を薄い線画で
//   Solid     — 面塗り (既定)
//   Field     — Solid + 結果断面 (実データ) のオーバーレイ。
//               断面が未設定のときは合成パターンを描かず未読込を明示する
//   Rays      — Solid + サンプルのレイ線 (24本 × 4反射。ソルバ結果ではない)
enum class ViewStyle { Wireframe, Solid, Field, Rays };

class Viewport3D : public QWidget {
    Q_OBJECT
public:
    explicit Viewport3D(Project *project, QWidget *parent = nullptr);

    void setDomain(Domain d) { m_domain = d; update(); }
    void setSolidMode(bool solid) { m_solid = solid; update(); }
    bool solidMode() const { return m_solid; }

    void setViewStyle(ViewStyle s);
    ViewStyle viewStyle() const { return m_viewStyle; }
    // 暗いパレット時にビューポートの地色/線色を合わせる
    void setDarkPalette(bool dark) { m_dark = dark; update(); }

    double azimuth() const   { return m_azimuthDeg; }
    double elevation() const { return m_elevationDeg; }
    // グリッド/境界(PML)の表示切替 — モックの Snap/境界チェックボックス相当
    void setGridVisible(bool on)     { m_showGrid = on; update(); }
    void setBoundaryVisible(bool on) { m_showBoundary = on; update(); }

    // ── 結果断面 (ソルバが出した実データ) の 3D 表示 ────────────────────────
    // 3D 空間内の 1 平面として重ねて描く (ViewStyle::Field のとき)。
    //   cells : 振幅 (rows*cols, row-major)。0..1 に正規化済みでなくてよい
    //           (ウィジェット側で最大値正規化する)。行 0 = 第 2 軸の +側。
    //   axis  : 0=X 一定 (YZ 面) / 1=Y 一定 (XZ 面) / 2=Z 一定 (XY 面)
    //   pos_m : 固定軸の座標 [m]
    //   u0,u1 : 面内 第1軸の範囲 [m] (axis=0 は y, axis=1 は x, axis=2 は x)
    //   v0,v1 : 面内 第2軸の範囲 [m] (axis=0 は z, axis=1 は z, axis=2 は y)
    //   label : 凡例に出す説明 (データセット名・時刻など)
    void setResultSlice(const QVector<double> &cells, int rows, int cols,
                        int axis, double pos_m,
                        double u0, double u1, double v0, double v1,
                        const QString &label);
    void clearResultSlice();
    bool hasResultSlice() const
    { return m_sliceRows > 0 && m_sliceCols > 0 && !m_sliceCells.isEmpty(); }

public slots:
    void fitView();
    void setAzimuth(double deg);
    void setElevation(double deg);
    // 0 = XY (上から), 1 = YZ (X軸方向から), 2 = ZX (Y軸方向から)
    void setViewPlane(int plane);

signals:
    // マウス操作で視点が変わったときに発火 (ツールバーのスライダー同期用)
    void viewChanged(double azimuthDeg, double elevationDeg);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void wheelEvent(QWheelEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;

private:
    QPointF projectPoint(double x, double y, double z) const;
    // 面内座標 (u, v) [m] → 画面座標 (m_sliceAxis の平面上)
    QPointF projectSlicePoint(double u, double v) const;

    void drawResultSlice(QPainter &p);   // 実データ断面 (無ければ未読込の明示)
    void drawSliceLegend(QPainter &p, int decim);
    void drawRayOverlay(QPainter &p);
    void rebuildSliceImage();            // 断面データ → 色画像 (jet)

    Project *m_project;
    Domain   m_domain = Domain::EM;
    bool     m_solid = false;
    bool     m_showGrid = true;
    bool     m_showBoundary = false;
    bool     m_dark = false;
    ViewStyle m_viewStyle = ViewStyle::Solid;

    // 結果断面 (実データ)。空 = 未読込
    QVector<double> m_sliceCells;
    int      m_sliceRows = 0, m_sliceCols = 0;
    int      m_sliceAxis = 2;
    double   m_slicePos = 0.0;
    double   m_sliceU0 = 0.0, m_sliceU1 = 0.0;
    double   m_sliceV0 = 0.0, m_sliceV1 = 0.0;
    double   m_sliceMax = 0.0;       // 正規化に使う最大値 (実データの絶対値)
    QString  m_sliceLabel;
    QImage   m_sliceImg;             // 断面の色画像 (setResultSlice で作る)
    int      m_sliceDecim = 1;       // 画像化で束ねたセル数 (1 = 等倍)

    double   m_azimuthDeg = -60;
    double   m_elevationDeg = 25;
    double   m_zoom = 1.0;
    QPointF  m_panPx;
    QPointF  m_lastPos;
    Qt::MouseButton m_dragButton = Qt::NoButton;

    // cached scene transform (set in paintEvent)
    mutable double m_cx = 0, m_cy = 0, m_cz = 0, m_scale = 1.0;
};

} // namespace ofd
