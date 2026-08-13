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

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QTabWidget;

namespace ofd {

class Project;
class MiniPlot;

// ── 拡声スピーカー (ホール寸法から自動配置する既定構成) ─────────────────────
// 機種ライブラリ (GLL) を持たないため指向性は無指向近似。位置・エイミングは
// 室寸法から決めるので、利用者が室モデルを変えれば追従する。
struct PaSpeaker {
    QString id;            // L / R / C / F / T
    QString kindKey;       // 種別の I18n キー
    double  pos[3] = { 0, 0, 0 };
    double  gainDb = 0;    // 相対ゲイン (既定 0 dB — 個別調整は未対応)
    bool    on = false;
    int     designRcv = 0; // 設計受音点 (P1..P4 のインデックス)
};

// 実測 IR 解析から取り出した 1 帯域 1 指標分の値 (ok=false は評価不能)
struct MeasuredValue {
    double v = 0;
    bool   ok = false;
};
// 実測 IR (WAV) の帯域別指標 125/250/500/1k/2k/4k Hz
struct MeasuredIrBands {
    MeasuredValue edt[6], t20[6], t30[6], c80[6], d50[6], ts[6];
    // 帯域内 INR [dB] (ピーク − 末尾ノイズフロア)。減衰時間が評価できるかを
    // 決める量なので、値が「—」になった理由を示すのに使う。
    MeasuredValue inr[6];
    bool    loaded = false;   // 解析が成功して値が入っているか
    QString status;           // 状態表示 (ファイル名 / エラー内容)
};

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

// 拡声系の STI 分布マップ。客席面のグリッド各点で、有効なスピーカーまでの
// 距離から Barron + MTF の STI を実計算する (無指向近似・無騒音仮定)。
class StiMapWidget : public QWidget {
    Q_OBJECT
public:
    explicit StiMapWidget(Project *project, QWidget *parent = nullptr);
    void setSpeakers(const QVector<PaSpeaker> &sp);   // 再計算も行う
    void recompute();
    double mean() const   { return m_mean; }
    double stddev() const { return m_std; }
    bool   valid() const  { return m_valid; }
protected:
    void paintEvent(QPaintEvent *) override;
private:
    Project *m_p;
    QVector<PaSpeaker> m_sp;
    QVector<double> m_values;
    double m_mean = 0, m_std = 0;
    bool   m_valid = false;
};

class RoomAcousticsTab : public QScrollArea {
    Q_OBJECT
public:
    explicit RoomAcousticsTab(Project *project, QWidget *parent = nullptr);

signals:
    // 「▶ 音響ソルバ連携で計算する」が押された → 音響ソルバ連携タブ
    // (ナビキー "acsolver") へ切り替えてほしい。実行そのものはあちらが持つ
    // (バイナリ解決が要るため自動起動はしない)。切替は MainWindow が中継する。
    void runSolverRequested();

private slots:
    void applySweepSettings();   // ESS 設定 widgets → model
    void refresh();          // model → widgets
    void recomputeAll();     // 派生値 (RT/エコーグラム/NC/障害) を再計算
    void exportReport();
    void resimulateImproved();   // 改善後の試算 (モデルは不変)

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
    void importMaterials();   // 吸音率 α の表 (CSV) を読んで材質表へ足す
    void refreshBudgetDerived();
    void refreshIrPage();        // IR解析: 帯域別指標 / 減衰曲線 / 検証表
    void refreshSpatialPage();   // 空間印象: LF/LFC (幾何) / G_late / 未計算欄
    void refreshReinforcePage(); // 電気音響: 配置 / ディレイ / STI / GBF
    void runMeasuredIr();        // 実測 IR (WAV) を解析して m_measIr を更新
    QVector<PaSpeaker> speakerLayout() const;   // 室寸法からの自動配置
    void applyNoiseSources();     // 騒音源内訳: widgets → model
    void refreshNoiseSources();   // 騒音源内訳: model → widgets
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
    QTableWidget *m_alphaTable = nullptr;      // 吸音率 α の表 (取込で行が増える)
    QLabel       *m_matImportNote = nullptr;   // 取込の説明と結果
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
    QComboBox    *m_irSrcBox;      // 0=統計推定 1=実測WAV
    QLineEdit    *m_irFileEdit;    // 実測 WAV (opera_analysis の rirPath と共有)
    QPushButton  *m_irRunBtn;
    QLabel       *m_irMethodNote, *m_irStatus, *m_irValNote, *m_irT2030Note;
    QCheckBox    *m_inrCheck = nullptr;   // 帯域内 INR 行の表示 (ISO 3382-2)
    QLabel       *m_inrNote = nullptr;    // INR の判定まとめ
    // ESS 逆畳み込み (.ofdx opera_analysis.sweep の View)
    QCheckBox    *m_essCheck = nullptr, *m_harmCheck = nullptr;
    QLineEdit    *m_sweepF1 = nullptr, *m_sweepF2 = nullptr, *m_sweepT = nullptr;
    QLabel       *m_sweepNote = nullptr;  // 逆畳み込みの結果 (帯域・THD・高調波)
    QString       m_sweepResult;          // 直近の逆畳み込みの要約 (実行結果のみ)
    MeasuredIrBands m_measIr;      // 実測解析の結果 (未解析なら loaded=false)
    QVector<QPointF> m_measDecay;  // 実測 Schroeder 減衰曲線 (x=秒, y=dB)

    // 空間印象 IACC/LF
    QTableWidget *m_spatialTable;
    QLabel       *m_spatialNote;

    // 電気音響設計
    QTableWidget *m_spTable, *m_delayTable, *m_gbfTable;
    QCheckBox    *m_haasCheck;
    StiMapWidget *m_stiMap;
    QLabel       *m_stiBadge, *m_stiUniBadge, *m_splNote;
    QLabel       *m_gbfBadge, *m_micPos, *m_spNote, *m_delayNote, *m_gbfNote;

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
    QTableWidget *m_noiseSrc;    // 騒音源内訳 (編集可)

    // defects
    QTableWidget *m_defects;
    QLabel    *m_recommend;
    QLabel    *m_resimResult;    // 改善後試算の結果表示
};

} // namespace ofd
