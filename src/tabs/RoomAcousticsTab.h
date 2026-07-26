// RoomAcousticsTab.h — ホール解析タブ (room-acoustics.jsx 相当)。
// 上部に「ホールモデル / Hall preset」(実在ホールの実測データ:
//   世界のコンサートホール5 + 日本のオペラ対応ホール, core/OperaHalls.h)。
// 10 のサブタブ:
//   客席カバレッジ — Barron統計モデルで G/C80/STI/RT を客席分布表示
//   エコーグラム   — シューボックス1次鏡像法の反射音列 + ITDG
//   IR解析         — Schroeder 逆積分の減衰曲線 + 帯域別指標 + 実測検証
//   残響計算       — Sabine/Eyring + 吸音バジェット + 帯域別 RT60
//   空間印象       — IACC / LF / BQI (ISO 3382-1)
//   ステージ       — ステージ支援 ST + 可変音響 + カップルドボリューム
//   吸音材DB       — 吸音率α・散乱係数s のライブラリと面割当
//   電気音響設計   — スピーカー配置 / STIマップ / ハウリング余裕 (GBF)
//   暗騒音 NC/NR   — オクターブ帯域騒音 vs NC 曲線
//   音響障害診断   — フラッター/ロングディレイエコー検出 + 改善提案
// 音響ドメイン選択時のみ表示される。設定は AcousticOpts (.ofdx) に永続化。
// ホールプリセット選択は室容積 V を AcousticOpts に反映する。
#pragma once
#include <QScrollArea>
#include "../core/RoomAcoustics.h"

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QTableWidget;
class QTabWidget;

namespace ofd {

class Project;
class MiniPlot;

// 扇形ホールの客席分布マップ (Barron 推定値をセル色で表示)
class CoverageMap : public QWidget {
    Q_OBJECT
public:
    explicit CoverageMap(Project *project, QWidget *parent = nullptr);
    void setMetric(int m)  { m_metric = m; recompute(); }
    void setBand(int b)    { m_band = b; recompute(); }
    void recompute();
    double mean() const  { return m_mean; }
    double stddev() const { return m_std; }
protected:
    void paintEvent(QPaintEvent *) override;
private:
    double cellValue(double r) const;
    Project *m_p;
    int m_metric = 0;    // 0=G(SPL), 1=C80, 2=STI, 3=RT
    int m_band = 3;      // 0..5 帯域 / 6=平均
    QVector<double> m_values;   // 計算済みセル値 (描画とセットで更新)
    double m_mean = 0, m_std = 0;
};

class RoomAcousticsTab : public QScrollArea {
    Q_OBJECT
public:
    explicit RoomAcousticsTab(Project *project, QWidget *parent = nullptr);

private slots:
    void refresh();          // model → widgets
    void recomputeAll();     // 派生値 (RT/エコーグラム/NC/障害) を再計算
    void exportReport();

private:
    QWidget *buildHallPresetSection();
    QWidget *buildCoveragePage();
    QWidget *buildEchogramPage();
    QWidget *buildIRPage();
    QWidget *buildReverbPage();
    QWidget *buildSpatialPage();
    QWidget *buildStagePage();
    QWidget *buildMaterialsPage();
    QWidget *buildReinforcePage();
    QWidget *buildNoisePage();
    QWidget *buildDefectsPage();
    void applyBudgetTable();
    void refreshBudgetDerived();
    void applyHallPreset();      // プリセットの V を AcousticOpts へ反映
    void refreshHallDerived();   // プリセット由来の派生表示を更新
    void receiverPos(int index, double out[3]) const;
    void sourcePos(double out[3]) const;

    Project    *m_p;
    bool        m_updating = false;
    QTabWidget *m_tabs;

    // hall preset (実在ホール実測データ)
    QPushButton *m_catConcert, *m_catOpera;
    QComboBox   *m_hallBox, *m_operaBox;
    QWidget     *m_concertPane, *m_operaPane;
    QLabel      *m_hallType, *m_hallInfo, *m_hallNote;
    QTableWidget *m_hallMetrics;
    QLabel      *m_operaType, *m_operaInfo, *m_operaClosed, *m_operaNote;
    QTableWidget *m_operaMetrics, *m_operaPit;

    // coverage
    CoverageMap *m_map;
    QComboBox   *m_metricBox, *m_bandBox;
    QLabel      *m_covStats;
    QTableWidget *m_seatTable;
    QLabel      *m_covRefNote;   // 実測 (公表値) 参照行

    // echogram
    QComboBox *m_rcvBox;
    MiniPlot  *m_echoPlot;
    QLabel    *m_itdgLabel;
    QTableWidget *m_reflTable;

    // IR解析 (Schroeder)
    MiniPlot     *m_schroederPlot;
    QTableWidget *m_irBandTable, *m_irValTable;

    // 空間印象 IACC/LF
    QTableWidget *m_spatialTable;

    // ステージ/可変音響
    QLabel *m_stageRtBadge;

    // reverb
    QDoubleSpinBox *m_roomL, *m_roomW, *m_roomH;
    QDoubleSpinBox *m_volume, *m_surface;
    QComboBox *m_occupancy, *m_formula;
    QTableWidget *m_budget;
    MiniPlot  *m_rtPlot;
    QLabel    *m_rtBadge;

    // noise
    QTableWidget *m_noise;
    MiniPlot  *m_ncPlot;
    QLabel    *m_ncBadge;

    // defects
    QTableWidget *m_defects;
    QLabel    *m_recommend;
};

} // namespace ofd
