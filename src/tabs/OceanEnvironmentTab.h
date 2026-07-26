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

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QTimer;

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

// 地形断面図 — 海域水深から合成した海底断面 (J-EGG500 / ETOPO 断面のモック)
class OeBathyView : public QWidget {
    Q_OBJECT
public:
    explicit OeBathyView(QWidget *parent = nullptr);
    void setDepth(double depth) { m_depth = depth; update(); }
protected:
    void paintEvent(QPaintEvent *) override;
private:
    double m_depth = 4200;
};

// データセット取得マネージャ (モーダル) — オフラインファースト設計。
// ① オフライン媒体取込 (推奨・既定) / ② 直接DL (明示許可時のみ, 進捗シミュレーション)
class OeDownloadManager : public QDialog {
    Q_OBJECT
public:
    explicit OeDownloadManager(double lat, double lon,
                               QWidget *parent = nullptr);
private:
    struct Job {
        QString name, src, size;
        int    state = 0;    // 0=idle, 1=run, 2=done
        double pct = 0;
    };
    void rebuildJobsTable();
    void startJob(int i);
    void tick();

    QCheckBox    *m_netAllow;
    QLabel       *m_netBadge;
    QTableWidget *m_jobsTable;
    QVector<Job>  m_jobs;
    QTimer       *m_timer;
};

class OceanEnvironmentTab : public QScrollArea {
    Q_OBJECT
public:
    explicit OceanEnvironmentTab(Project *project, QWidget *parent = nullptr);

private slots:
    void requery();          // 緯度経度 → 海域照会 + SSP 再計算 + 表示更新
    void applyToSolver();    // SSP/底質を UnderwaterOpts へ反映
    void openDownloadManager();

private:
    static const OeRegion &findRegion(double lat, double lon);
    void computeSsp();       // oeTempProfile + Mackenzie 式 (mock の数式を移植)

    Project *m_p;
    const OeRegion      *m_region = nullptr;
    QVector<OeSspPoint>  m_ssp;
    int                  m_cMinIdx = 0;

    // location query
    QLineEdit *m_lat, *m_lon;
    QComboBox *m_month;
    QLabel    *m_queryBadge;

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

    // apply
    QCheckBox *m_chkSsp, *m_chkBty, *m_chkBottom;
};

} // namespace ofd
