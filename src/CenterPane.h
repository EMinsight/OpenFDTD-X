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

class QCheckBox;
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
    // 併せて 3D シーン (Viewport3D) にも同じ断面を重ねる (節点座標が
    // 取れる ofd/orcwa 系のみ。結果は result3DSliceStatus で通知する)。
    bool loadResultField(const QString &h5Path);

    // 3D シーンにだけ結果断面を流し込む (プロジェクトを開いたときに
    // 見つかった既存 HDF5 用)。2D 断面は「その実行が生成したもの」に
    // 限るゲートを維持したいので触らない。
    bool loadResult3DSlice(const QString &h5Path);

    // 結果表示のクリア (新規/別プロジェクト — 前の実行の残骸を出さない)
    void clearResultField();

    // 3D シーンに結果断面が載っているか (ツールバーのトグルの有効条件)
    bool hasResult3DSlice() const;

signals:
    // 3D シーンへの結果断面の反映結果。ok=false の detail は理由
    // (座標情報が無い等)。ログ出力は MainWindow が行う。
    void result3DSliceStatus(bool ok, const QString &detail);

private slots:
    void onTabChanged(int index);
    void saveSnapshot();

private:
    // ドメインで意味を持たない UI 項目の出し分け (setDomain から呼ばれる)
    void updateDomainVisibility(Domain d);
    // HDF5 の z 中央断面 + 節点座標 → Viewport3D::setResultSlice。
    // 座標が取れない (obpm の /field/Ixz 等) ときは 3D へ渡さず why に理由。
    bool applyResultSliceTo3D(const QString &h5Path, QString *why);
    // 「結果断面を重ねる」トグルの有効/ツールチップ更新
    void updateOverlayUi();

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
    QCheckBox *m_overlayCheck;    // 結果断面を重ねる (= スタイル Field の別表現)
    int      m_prevStyleIndex = 1;   // トグル OFF で戻す先 (1 = Solid)
    QSlider *m_azSlider, *m_elSlider;
    QLabel  *m_azLabel,  *m_elLabel;
};

} // namespace ofd
