// H5ViewerTab.h — H5アニメタブ (h5-viewer.jsx 相当)。
// カーネルが出力する HDF5 (time_series_data.h5 等) を io/H5Reader で読み、
//   - データセットツリー (実ファイルの列挙結果, パス階層をグループ化)
//   - 2D データセットのヒートマップ表示 (jet/viridis/seismic/gray)
//   - 3D (frames×rows×cols) データセットのフレーム再生 (QTimer)
//   - 伝搬時系列 (E/H) の 3 面ビュー (XY/XZ/YZ を共通カラースケールで同時表示)
//   - 表示中フレームの min / max / 平均 の実計算表示
// を行う。1D / 4D 以上のデータセットは表示未対応 (ofd の /data*/E 等)。
#pragma once
#include "../io/MovieExport.h"
#include <QImage>
#include <QScrollArea>
#include <QVector>

#include "../io/H5Reader.h"   // H5DatasetInfo

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class QTimer;
class QTreeWidget;
class QTreeWidgetItem;

namespace ofd {

class Project;
class SectionBox;

// rows×cols の実データ行列をカラーマップで描くキャンバス。
// データ未読込・表示未対応時は中央にメッセージを表示する。
class FieldCanvas : public QWidget {
    Q_OBJECT
public:
    explicit FieldCanvas(QWidget *parent = nullptr);

    // 実データ行列をセットして再描画 (row-major, rows×cols)
    void setData(const QVector<double> &d, int rows, int cols);
    // データを破棄し、中央メッセージ表示に切り替える (空 = 既定の「未読込」文言)
    void setMessage(const QString &msg);
    void setColormap(int c);                 // 0=jet 1=viridis 2=seismic 3=gray
    void setScale(double lo, double hi);     // 正規化範囲 (表示側で 0..1 へ)
    void setShowGrid(bool on)  { m_grid = on; update(); }
    void setShowAxes(bool on)  { m_axes = on; update(); }
    void setDatasetName(const QString &n) { m_name = n; update(); }
    bool hasData() const { return !m_img.isNull(); }

    // エクスポート用: 行列を指定スケールで画像化する (1 セル = cellPx px、
    // ニアレスト拡大)。表示中のカラーマップを使う
    QImage renderImage(const QVector<double> &d, int rows, int cols,
                       int cellPx, double lo, double hi);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QColor mapColor(double v) const;   // mock の colormap_fn を転記 (正規化付き)
    void rebuildImage();               // m_data → m_img (1 セル = 1 ピクセル)

    QVector<double> m_data;
    int     m_rows = 0, m_cols = 0;
    QImage  m_img;                     // 事前レンダ済み行列画像
    QString m_msg;                     // データ無し時の中央表示文言
    int     m_cmap = 0;                // 0=jet 1=viridis 2=seismic 3=grayscale
    double  m_lo = 0.0, m_hi = 1.0;
    bool    m_grid = false, m_axes = true;
    QString m_name;                    // 左上オーバーレイ (データセット名)
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

    // 実行完了時に MainWindow から実行出力の .h5 を渡して読み込む
    void openFile(const QString &path);

private slots:
    void applyTimeRange();   // 時間範囲 → 再生対象フレームの絞り込み

private:
    void loadFile();                  // m_file のパスを列挙してツリー再構築
    void rebuildTree();               // m_dsets → パス階層ツリー
    void selectDataset(int idx);      // ツリー選択 → 読込 + 表示切替
    void loadCurrentFrame();          // 3D の現在フレームを読込・表示
    void showData(const QVector<double> &d, int rows, int cols);  // 描画+統計
    void setFrame(int f);             // スライダー / タイマー / ボタン共通
    void applyScale();                // 手動スケール欄 → キャンバス + バー
    void setScaleLabels(double lo, double hi);
    void setPlaybackEnabled(bool on); // 3D のときだけ再生 UI を有効化
    void clearStats();
    void updateDomainVisibility();    // ドメイン別の出し分け (時間単位 / Schroeder)
    void updateSliceControls();       // 断面 UI の範囲/有効化 (series のみ)
    int  sliceAxis() const;           // planeBox → 0=X/1=Y/2=Z

    // ── 3 面ビュー (XY/XZ/YZ 同時表示) ──
    bool multiActive() const;         // 3 面ビュー表示中か (伝搬時系列のみ)
    void updateMultiVisibility();     // 単一断面 ⇄ 3 面ビューの表示切替
    void loadMultiFrames();           // 3 断面を読み込み、共通スケールで描画
    void loadSliceCoords();           // /metadata/Xn|Yn|Zn (あれば) を読む
    QString sliceCaption(int axis) const;   // 軸名 + ノード番号 (+ 座標)
    // 全フレーム走査 (自動スケール / 書き出し) 用の断面読み出し。
    // 3 面ビュー時は 3 面を連結した 1 本の配列を返す
    bool scanFrameValues(int frame, QVector<double> &out);
    QImage multiImage(int frame, double lo, double hi, bool *ok);  // 3 面連結画像
    void updateExportNote();          // 3 面ビュー時の書き出し内容の明示
    void exportPngCurrent();          // 現在フレームを PNG 保存
    void exportCsvCurrent();          // 現在フレームの行列を CSV 保存
    // 開いている .h5 の実スキーマから h5py 読込コードを生成して保存する
    // (notebook=false: .py スクリプト / true: .ipynb ノートブック)
    void exportPythonScript(bool notebook);
    // 全フレームを PNG 連番に描き出す (video=true なら ffmpeg で動画化)
    void exportFrames(bool video, const QString &videoExt);
    QImage frameImage(int frame, double lo, double hi, bool *ok);

    Project     *m_p;

    // ファイル / ツリー
    QLineEdit   *m_file;
    QPushButton *m_browseBtn, *m_reloadBtn;
    QTreeWidget *m_tree;
    QLabel      *m_selected;
    QString      m_filePath;                  // 読込済みファイル
    QVector<H5DatasetInfo> m_dsets;           // 列挙結果 (UserRole = index)

    // 選択中データセットの状態
    QString      m_dataset;                   // 例 "/field/Ixz"
    bool         m_seriesMode = false;        // ofd 伝搬時系列を再生中か
    QString      m_seriesComp;                // "E" / "H"
    H5OfdSeriesInfo m_seriesInfo;             // 格子サイズ (断面 UI の範囲)
    int          m_nframes = 0;               // 3D のフレーム数 (2D は 0)
    QVector<double> m_data;                   // 表示中の行列
    int          m_rows = 0, m_cols = 0;

    // 可視化
    QComboBox   *m_cmap;
    QCheckBox   *m_autoScale;
    QLineEdit   *m_scaleMin, *m_scaleMax;

    // プレビュー
    SectionBox  *m_previewBox;
    FieldCanvas *m_canvas;
    ColorBar    *m_bar;
    QLabel      *m_barMax, *m_barMin;

    // 統計 (実計算)
    QLabel      *m_statMin, *m_statMax, *m_statMean;
    QPushButton *m_schroederBtn = nullptr;    // Schroeder 減衰 (室内音響のみ表示)

    // 再生
    QPushButton *m_playBtn, *m_firstBtn, *m_prevBtn, *m_nextBtn, *m_lastBtn;
    QPushButton *m_loopBtn = nullptr; // ループ切替 (checkable, 既定 ON)
    QSlider     *m_frameSlider;
    QLabel      *m_frameLabel;
    QComboBox   *m_speed;
    QTimer      *m_timer;
    int          m_frame = 0;
    double timeUnitToSeconds() const;         // 表示単位 → 秒
    movie::MovieOptions movieOptions(bool gif) const;  // 動画設定 → ffmpeg

    QLabel      *m_timeUnit = nullptr;        // 時間範囲の単位 (ドメイン別 ps/ms/s)
    // 時間範囲での絞り込み (/timeseries/time が読めるときだけ有効)
    QLineEdit   *m_rangeLo = nullptr, *m_rangeHi = nullptr;
    QCheckBox   *m_rangeOnly = nullptr;
    QLabel      *m_rangeNote = nullptr;
    QVector<double> m_frameTimes;             // 各フレームの時刻 [s] (空 = 不明)
    int          m_playFirst = 0, m_playLast = -1;   // 再生対象 [first, last]
    // 動画設定 (ffmpeg へ渡す)
    QLineEdit   *m_movieFps = nullptr;
    QComboBox   *m_movieRes = nullptr, *m_movieCodec = nullptr;

    // 断面 (伝搬時系列でのみ有効 — 軸と位置を選ぶ)
    QComboBox   *m_planeBox = nullptr;
    QSlider     *m_secSlider;
    QLabel      *m_secValue;
    QLabel      *m_secNote = nullptr;
    // 軸ごとの断面位置 (0=X/1=Y/2=Z のノード番号。-1 = 未設定 → 中央)。
    // 位置スライダは planeBox で選んだ面 (主断面) の軸を編集する
    int          m_secPos[3] = { -1, -1, -1 };
    QVector<double> m_coord[3];       // /metadata/Xn|Yn|Zn [m] (無ければ空)

    // 3 面ビュー (XY/XZ/YZ 同時表示)
    QCheckBox   *m_multiChk = nullptr;
    QWidget     *m_multiWrap = nullptr;
    FieldCanvas *m_multiCanvas[3] = { nullptr, nullptr, nullptr };
    QLabel      *m_multiCaption[3] = { nullptr, nullptr, nullptr };

    // エクスポート
    QPushButton *m_expMp4 = nullptr, *m_expGif = nullptr,
                *m_expPng = nullptr, *m_expPngSeq = nullptr,
                *m_expCsv = nullptr;
    QLabel      *m_expStatus = nullptr;
    QLabel      *m_expMultiNote = nullptr;    // 3 面ビュー時の書き出し内容
    bool         m_exporting = false;
};

} // namespace ofd
