// CenterPane.h — 中央ペイン (app.jsx CenterPane 相当)。
//
//   ┌ vp-tabs   [🧊 3D シーン][📐 2D 断面][📊 結果プロット][📏 メッシュ表示] ┐
//   │ vp-toolbar [🔄 Reset][⊕↔⟳⤢][Snap:☑グリッド ☐頂点]                  │
//   │            [Rotate ──○── az][──○── el][XY][YZ][ZX][📷 Snap]         │
//   ├─────────────────────────────────────────────────────────────────────┤
//   │ QStackedWidget: Viewport3D / FieldHeatmap / PlotPanel / MeshPreview  │
//   └─────────────────────────────────────────────────────────────────────┘
//
// ギズモ (選択/移動/回転/スケール) はモックではマウス操作のヒント表示。
// Viewport3D の実操作 (orbit/pan/zoom) はそのまま生かす。
#pragma once
#include <QWidget>
#include "core/Domain.h"

class QSlider;
class QStackedWidget;
class QTabBar;
class QLabel;

namespace ofd {

class Project;
class Viewport3D;
class PlotPanel;
class FieldHeatmap;
class MeshPreview;

class CenterPane : public QWidget {
    Q_OBJECT
public:
    explicit CenterPane(Project *project, QWidget *parent = nullptr);

    Viewport3D *viewport() const { return m_viewport; }
    PlotPanel  *plotPanel() const { return m_plot; }

    void setDomain(Domain d);
    void showViewport();      // 3D シーンへ切替 (図形表示3D)
    void showPlot();          // 結果プロットへ切替 (図形表示2D)

private slots:
    void onTabChanged(int index);
    void saveSnapshot();

private:
    Project        *m_p;
    QTabBar        *m_tabs;
    QStackedWidget *m_stack;
    Viewport3D     *m_viewport;
    FieldHeatmap   *m_heatmap;
    PlotPanel      *m_plot;
    MeshPreview    *m_mesh;

    QWidget *m_vpToolbar;
    QSlider *m_azSlider, *m_elSlider;
    QLabel  *m_azLabel,  *m_elLabel;
};

} // namespace ofd
