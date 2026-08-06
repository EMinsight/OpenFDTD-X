// OceanEnvironmentTab.h — 海洋環境データ取得タブ (ocean-environment.jsx 相当)。
// 緯度・経度を入力 → ローカル配置済みデータセットから
// 海底地形 (ETOPO/J-EGG500)・水温/塩分プロファイル (JODC/WOA23) を照会し、
// Mackenzie (1981) 式で音速プロファイル (SSP) を自動生成して
// Bellhop/PE/FDTD (水中音響タブ) の入力に接続する。
// 付属: OeDownloadManager — オフラインファースト設計のデータセット取得ダイアログ
// (既定はオフライン媒体取込、直接DLは明示許可時のみ)。水中ドメイン専用。
#pragma once
#include <QDialog>
#include <QScrollArea>
#include <QVector>

#include "../core/Project.h"   // BathyPoint

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QTableWidget;


namespace ofd {

class Project;
class SectionBox;

// 代表海域の WOA 風プロファイル (mock の OE_REGIONS 相当)
struct OeRegion {
    const char *name;                        // 地域名 (UTF-8, データそのまま)
    double latMin, latMax, lonMin, lonMax;   // 緯度・経度範囲
    double depth;                            // 水深 [m]
    double sst;                              // 表層水温 [°C]
    double sss;                              // 表層塩分 [psu]
    const char *type;   // japan_sea/kuroshio/okhotsk/shelf/deep_pacific/strait/med
};

// SSP 1層 (深度・水温・塩分・音速)
struct OeSspPoint { int z; double T, S, c; };

// 音速プロファイル図 — c を右向き, 深度を下向きに描画。SOFAR軸マーカ付き。
class OeSspView : public QWidget {
    Q_OBJECT
public:
    explicit OeSspView(QWidget *parent = nullptr);
    void setProfile(const QVector<OeSspPoint> &ssp, double depth, int cMinIdx);
protected:
    void paintEvent(QPaintEvent *) override;
private:
    QVector<OeSspPoint> m_ssp;
    double m_depth = 1;
    int    m_cMinIdx = 0;
};

// 地形断面図 — 伝搬経路に沿った海底断面。実データ (GEBCO/ETOPO/J-EGG500 を
// 配置済みの場合) と、データが無いときの合成断面の両方を描く。
// どちらを描いているかは必ず図中に出す (合成を実地形と見せない)。
class OeBathyView : public QWidget {
    Q_OBJECT
public:
    explicit OeBathyView(QWidget *parent = nullptr);
    // 断面 (距離[km], 水深[m]) と出所。source が空なら合成断面として描く。
    void setSection(const QVector<BathyPoint> &pts, const QString &source);
protected:
    void paintEvent(QPaintEvent *) override;
private:
    QVector<BathyPoint> m_pts;
    QString m_source;      // 空 = 合成
};

// データセット取得マネージャ (モーダル) — オフラインファースト設計。
// ① 取得済みファイルのフォルダ取込 (実コピー) / ② 公式配布ページを
// ブラウザで開く。アプリ内直接ダウンロードは未実装 (進捗を装う表示は
// 置かない — 実際に起きたことだけを表示する)。
class OeDownloadManager : public QDialog {
    Q_OBJECT
public:
    explicit OeDownloadManager(QWidget *parent = nullptr);
    ~OeDownloadManager() override;
private:
    void importFiles();      // ファイル選択 → データセットフォルダへコピー
    void startDownload();    // URL → データセットフォルダへ直接ダウンロード
    void abortDownload();

    QLabel *m_importResult = nullptr;

    // ③ 直接ダウンロード (Qt6::Network がある構成のみ)。
    // 進捗は実際の受信バイト数だけを出す (擬似進捗は出さない)。
    QLineEdit    *m_url = nullptr;
    QPushButton  *m_dlBtn = nullptr, *m_abortBtn = nullptr;
    QProgressBar *m_dlProgress = nullptr;
    QLabel       *m_dlResult = nullptr;
    class Impl;
    Impl *m_impl = nullptr;    // QNetworkAccessManager 一式 (非ネットワーク
                               // ビルドでは nullptr)
};

class OceanEnvironmentTab : public QScrollArea {
    Q_OBJECT
public:
    explicit OceanEnvironmentTab(Project *project, QWidget *parent = nullptr);

private slots:
    void requery();          // 緯度経度 → 海域照会 + SSP 再計算 + 表示更新
    void applyToSolver();    // SSP/底質を UnderwaterOpts へ反映
    void openDownloadManager();
    void rebuildDatasetTable();   // データセットフォルダを走査して配置状態を更新

private:
    static const OeRegion &findRegion(double lat, double lon);
    void computeSsp();       // oeTempProfile + Mackenzie 式 (mock の数式を移植)
    // 測点・方位・距離から地形断面を作る。ローカルデータセットに水深
    // グリッドがあればそれを大圏サンプリングし、無ければ合成断面にする。
    void computeSection();
    void loadSiteFromProject();   // .ofdx の測点・方位・距離 → 入力欄

    Project *m_p;
    const OeRegion      *m_region = nullptr;
    QVector<OeSspPoint>  m_ssp;
    int                  m_cMinIdx = 0;

    // location query
    QLineEdit *m_lat, *m_lon;
    QComboBox *m_month;
    QLabel    *m_queryBadge;

    // local datasets (実フォルダ走査)
    QTableWidget *m_dsTable = nullptr;
    QLabel       *m_dsFolderLabel = nullptr;

    // query result
    SectionBox   *m_resultSection;
    QLabel       *m_bDepth, *m_bSst, *m_bSss, *m_bBottom;
    QTableWidget *m_sspTable;
    QLabel       *m_layerNote;

    // SSP / bathymetry
    OeSspView   *m_sspView;
    QLabel      *m_sspNote;
    OeBathyView *m_bathy;
    QLineEdit   *m_bearing, *m_dist;
    QLabel      *m_btyNote = nullptr;      // 出所 (実データ or 合成) の表示
    QVector<BathyPoint> m_section;         // 直近に作った断面
    QString      m_sectionSource;          // 出所 ("" = 合成)

    // apply
    QCheckBox *m_chkSsp, *m_chkBty, *m_chkBottom;
};

} // namespace ofd
