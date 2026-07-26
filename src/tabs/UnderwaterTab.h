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
class QSpinBox;
class QTableWidget;

namespace ofd {

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

    QLabel         *m_c0;                 // 基準音速 c₀ (計算値)
    QLabel         *m_sofarHint;          // → SOFARチャネル深度 ~1200m
    UwSspPlot      *m_sspPlot;

    QDoubleSpinBox *m_bottomAlpha;        // 吸収係数 α [dB/λ]

    QDoubleSpinBox *m_waveHeight;         // 波高 [m]
    QCheckBox      *m_surfSpecular, *m_surfBragg;

    QComboBox      *m_sonarDir;           // 全方位 / 指向性 / アレイ
    QDoubleSpinBox *m_beamWidth;

    QCheckBox      *m_tlSpread, *m_tlAbsorb, *m_tlScatter, *m_tlSurface;
    QDoubleSpinBox *m_tlRangeMin, *m_tlRangeMax;   // max は rangeMax_km と同期
};

} // namespace ofd
