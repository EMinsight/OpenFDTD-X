// UnderwaterTab.h — 水中音響タブ (ソルバー, SSP, SOFAR, 海底底質, 海面, ソナー, TL).
// Settings persist in the .ofdx sidecar.
#pragma once
#include <QPointF>
#include <QScrollArea>
#include <QVector>

class QDoubleSpinBox;
class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QTableWidget;

namespace ofd {

struct ShdField;

// 伝搬損失 (TL) 断面のヒートマップ。bellhopcxx の <ケース名>.shd を
// ShdReader で読んだ結果を、距離 (横) × 深度 (縦) で描く。
// 空のまま (計算前) は「未計算」とだけ表示する — 前回実行の残りを
// 別の実行の結果として見せないため、Runner の完了時にだけ設定する。
class UwTlView : public QWidget {
    Q_OBJECT
public:
    explicit UwTlView(QWidget *parent = nullptr);
    void setField(const ShdField &f, double rangeMax_km, double depthMax_m,
                  const QString &caption);
    void clear();
protected:
    void paintEvent(QPaintEvent *) override;
private:
    QVector<float> m_tl;
    int    m_nz = 0, m_nr = 0;
    double m_lo = 0, m_hi = 0;
    double m_rangeMax = 0, m_depthMax = 0;
    QString m_caption;
};

class Project;

// SSP プロファイル図 (x = 音速 c, y = 深度 — 下向き)。
// mock (tabs.jsx UnderwaterTab) の <MiniPlot yRange={[5000,0]}> 相当。MiniPlot は
// Y 軸の反転に非対応なので、海洋屋の慣習どおり深度が下向きになる専用描画にする。
class UwSspPlot : public QWidget {
public:
    explicit UwSspPlot(QWidget *parent = nullptr);
    // pts: x = c [m/s], y = depth [m] (深度昇順)
    void setProfile(const QVector<QPointF> &pts);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QVector<QPointF> m_pts;
};

class UnderwaterTab : public QScrollArea {
    Q_OBJECT
public:
    explicit UnderwaterTab(Project *project, QWidget *parent = nullptr);

private slots:
    void refresh();

private:
public:
    // 実行完了時に MainWindow から呼ぶ (作業ディレクトリとケース名)。
    // .shd が読めなければ理由を表示する。
    void showTlResult(const QString &workingDir, const QString &caseName);
    // 同上 — 「計算モード = 到達時間」で出る <ケース名>.arr を受け取り、
    // 受信インパルス応答を作れる状態にする。無ければ欄を伏せる。
    void showArrivalResult(const QString &workingDir, const QString &caseName);

signals:
    // 受信 IR を WAV へ書き出した直後に発行する。MainWindow が可聴化タブへ
    // 橋渡しして「ウェット音を作る」まで繋げる (タブ間の直接依存を作らない)。
    void receivedIrExported(const QString &wavPath);

public:

private:
    void apply();
    void applySsp();
    void updateSolverView();   // ソルバー切替 → 説明文と条件付きパネルの表示
    void updateDerived();      // 基準音速 c₀ / SOFAR 深度 / SSP プロファイル図

    Project        *m_p;
    bool            m_updating = false;

    QDoubleSpinBox *m_temp, *m_salinity;
    QTableWidget   *m_ssp;
    QCheckBox      *m_sofar;
    QComboBox      *m_bottomType;
    QDoubleSpinBox *m_bottomC, *m_bottomRho;
    QDoubleSpinBox *m_bottomAlpha;        // 吸収係数 α [dB/λ] (.ofdx
                                          // bottom_alpha_db_lambda → BellhopIO)
    QDoubleSpinBox *m_sonarFreq, *m_sonarSL, *m_rangeMax;

    // ── モック (tabs.jsx UnderwaterTab) 追加分 ────────────────────────────
    // Project に対応フィールドが無いものはローカル状態 (既定値はモックのまま)。
    QComboBox      *m_solver;             // FDTD / Bellhop / PE / ハイブリッド
    QLabel         *m_solverDesc;
    QWidget        *m_bellhopPanel, *m_pePanel;
    QSpinBox       *m_numRays;
    QComboBox      *m_beamType, *m_calcMode;
    QDoubleSpinBox *m_angMin, *m_angMax;
    QCheckBox      *m_visRay, *m_visTL, *m_visEcho;
    QComboBox      *m_peAlgo;
    QDoubleSpinBox *m_peAngular;

    UwTlView       *m_tlView = nullptr;   // TL 断面 (.shd の可視化)
    QLabel         *m_tlNote = nullptr;

    // ── 受信インパルス応答 (.arr → IR → WAV) ─────────────────────────────
    QWidget        *m_irBox = nullptr;       // 到達が無いときは隠す
    QComboBox      *m_irDepth = nullptr;     // 受波器 深度
    QComboBox      *m_irRange = nullptr;     // 受波器 距離
    QComboBox      *m_irFs = nullptr;        // 書き出す fs
    QPushButton    *m_irExport = nullptr;
    QLabel         *m_irNote = nullptr;
    QString         m_arrPath;               // 直近の実行の .arr
    void exportReceivedIr();

    QLabel         *m_c0;                 // 基準音速 c₀ (計算値)
    QLabel         *m_sofarHint;          // → SOFARチャネル深度 ~1200m
    UwSspPlot      *m_sspPlot;

    QDoubleSpinBox *m_waveHeight;         // 波高 [m]
    QCheckBox      *m_surfSpecular, *m_surfBragg;

    QComboBox      *m_sonarDir;           // 全方位 / 指向性 / アレイ
    QDoubleSpinBox *m_beamWidth;

    QCheckBox      *m_tlSpread, *m_tlAbsorb, *m_tlScatter, *m_tlSurface;
    QDoubleSpinBox *m_tlRangeMin, *m_tlRangeMax;   // max は rangeMax_km と同期
};

} // namespace ofd
