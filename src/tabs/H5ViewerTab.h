// H5ViewerTab.h — H5アニメタブ (h5-viewer.jsx 相当)。
// HDF5 時系列場データセットのアニメーションビューア:
//   - ファイル選択 + HDFView 風データセットツリー
//   - ヒートマップ/等高線/ベクトル場/3D等値面/線プロット (QPainter 合成場)
//   - QTimer による再生コントロール (0.25x〜5x, 30fps 基準)
//   - Jet/Viridis/Seismic/Gray カラーマップ + カラーバー
//   - 断面選択 / 動画・画像エクスポート / 統計 / 外部連携ボタン
#pragma once
#include <QScrollArea>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class QTimer;
class QTreeWidget;

namespace ofd {

class Project;
class SectionBox;

// モックの SVG ヒートマップ相当 — 数式で場を合成し QPainter で描画する
// キャンバス。v = sin(4r - t)·exp(-0.4r), t = frame/240·2π·4 (mock と同一)。
class FieldCanvas : public QWidget {
    Q_OBJECT
public:
    explicit FieldCanvas(QWidget *parent = nullptr);
    void setFrame(int f)                { m_frame = f; update(); }
    void setView(int v)                 { m_view = v; update(); }
    void setColormap(int c)             { m_cmap = c; update(); }
    void setScale(double lo, double hi) { m_lo = lo; m_hi = hi; update(); }
    void setShowGrid(bool on)           { m_grid = on; update(); }
    void setShowAxes(bool on)           { m_axes = on; update(); }
    void setIso(double v)               { m_iso = v; update(); }
    void setDatasetName(const QString &n) { m_name = n; update(); }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QColor mapColor(double v) const;   // mock の colormap_fn を転記

    int     m_frame = 0;
    int     m_view = 0;                // 0=heatmap 1=contour 2=vector 3=iso3d 4=line
    int     m_cmap = 0;                // 0=jet 1=viridis 2=seismic 3=grayscale
    double  m_lo = -1.0, m_hi = 1.0;
    bool    m_grid = false, m_axes = true;
    double  m_iso = 0.5;
    QString m_name = "E_surface";
};

// カラーバー (縦グラデーション, mock の CSS linear-gradient を転記)
class ColorBar : public QWidget {
    Q_OBJECT
public:
    explicit ColorBar(QWidget *parent = nullptr);
    void setColormap(int c) { m_cmap = c; update(); }
protected:
    void paintEvent(QPaintEvent *) override;
private:
    int m_cmap = 0;
};

class H5ViewerTab : public QScrollArea {
    Q_OBJECT
public:
    explicit H5ViewerTab(Project *project, QWidget *parent = nullptr);

private:
    void setFrame(int f);     // スライダー / タイマー / ボタン共通のフレーム更新
    void applyScale();        // スケール欄 → キャンバス + カラーバーラベル

    Project     *m_p;

    // ファイル / ツリー
    QLineEdit   *m_file;
    QTreeWidget *m_tree;
    QLabel      *m_selected;
    QString      m_dataset = "/monitors/E_surface";

    // 可視化
    QComboBox   *m_view, *m_cmap;
    QCheckBox   *m_autoScale;
    QLineEdit   *m_scaleMin, *m_scaleMax;
    QLabel      *m_isoLabel;
    QWidget     *m_isoField;
    QSlider     *m_isoSlider;
    QLabel      *m_isoValue;

    // プレビュー
    SectionBox  *m_previewBox;
    FieldCanvas *m_canvas;
    ColorBar    *m_bar;
    QLabel      *m_barMax, *m_barMin;

    // 再生
    QPushButton *m_playBtn;
    QSlider     *m_frameSlider;
    QLabel      *m_frameLabel;
    QComboBox   *m_speed;
    QTimer      *m_timer;
    int          m_frame = 0;

    // 断面
    QSlider     *m_secSlider;
    QLabel      *m_secValue;
};

} // namespace ofd
