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

class QComboBox;
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
    void setViewStyleIndex(int i);   // 0=Wire 1=Solid 2=Field 3=Rays (CLI/メニュー用)
    void showViewport();      // 3D シーンへ切替 (図形表示3D)
    void showPlot();          // 結果プロットへ切替 (図形表示2D)

    // カーネルの HDF5 出力 (time_series_data.h5) から 2D 断面へ実データを
    // 反映する。/field/Ixz (obpm 伝搬マップ) → |Efinal| → ofd/orcwa の
    // ノード場 (/data%06d/E + /metadata 格子から z 中央断面を再構成) の順に
    // 試し、読めたらデモ表示を置き換えて true。
    bool loadResultField(const QString &h5Path);

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
    QComboBox *m_styleBox;
    QSlider *m_azSlider, *m_elSlider;
    QLabel  *m_azLabel,  *m_elLabel;
};

} // namespace ofd
