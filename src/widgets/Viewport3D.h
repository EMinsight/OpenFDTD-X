// Viewport3D.h — central 3D view of the project (mesh region, geometry
// units, feeds, probes). QPainter-based orthographic wireframe so that no
// OpenGL context is required (works in headless / remote sessions too).
//
// Mouse: left-drag = orbit, middle-drag = pan, wheel = zoom, double = fit.
#pragma once
#include <QWidget>
#include <QPointF>
#include "../core/Domain.h"

class QTimer;

namespace ofd {

class Project;

// モックの TweaksPanel「3D ビュー / Viewport」に対応する描画スタイル。
//   Wireframe — 形状を薄い線画で
//   Solid     — 面塗り (既定)
//   Field     — Solid + 界分布オーバーレイ (アニメーション)
//   Rays      — Solid + レイトレース線 (24本 × 4反射)
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

    void drawFieldOverlay(QPainter &p);
    void drawRayOverlay(QPainter &p);

    Project *m_project;
    Domain   m_domain = Domain::EM;
    bool     m_solid = false;
    bool     m_showGrid = true;
    bool     m_showBoundary = false;
    bool     m_dark = false;
    ViewStyle m_viewStyle = ViewStyle::Solid;
    int      m_animTick = 0;         // Field オーバーレイの位相
    QTimer  *m_animTimer = nullptr;  // Field のときだけ動かす

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
