// AcousticTab.h — 室内音響タブ (RT60/C80/D50/STI, マイクアレイ, 可聴化).
// Settings persist in the .ofdx sidecar.
#pragma once
#include <QScrollArea>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class QTableWidget;
class QDoubleSpinBox;

namespace ofd {

class Project;

class AcousticTab : public QScrollArea {
    Q_OBJECT
public:
    explicit AcousticTab(Project *project, QWidget *parent = nullptr);

private slots:
    void refresh();
    void runConvolve();        // 可聴化経路 (ConvolutionEngine) へ委譲
    void applyReceiverCount(); // 受音点数スピン → 受音点リストの伸縮

private:
    void apply();
    void updateSolverView();   // ソルバー切替 → 説明文と条件付きパネルの表示
    // 受音点リスト (AcousticOpts::receivers) の表 ↔ モデル
    void refreshReceivers();
    void applyReceivers();
    // 材質設定 = 吸音バジェット (AcousticOpts::absorption) の表 ↔ モデル
    void refreshSurfaces();
    void applySurfaces();

    Project   *m_p;
    bool       m_updating = false;

    QCheckBox *m_rt60, *m_c80, *m_d50, *m_sti, *m_edt, *m_irf, *m_aural;
    QComboBox *m_sampleRate;
    QComboBox *m_directivity;
    QDoubleSpinBox *m_spl;
    QSpinBox  *m_micCount;

    // ── モック (tabs.jsx AcousticTab) 追加分 ──────────────────────────────
    // LF / 位置・向き / 解析タイプ / 帯域 / 受音点リストは AcousticOpts
    // (.ofdx) に永続化。ソルバー・可聴化ソース/出力形式はローカル状態。
    QCheckBox *m_lf;                    // LF (側方音エネルギー)
    QLineEdit *m_srcPos, *m_srcAim;     // 位置(x,y,z) [m] / 向き(θ,φ) [deg]
    QTableWidget *m_micTable;           // 受音点表 (AcousticOpts::receivers)

    QComboBox *m_solver;                // FDTD / Ray / Image-Source / Hybrid
    QLabel    *m_solverDesc;
    QWidget   *m_rayPanel, *m_ismPanel, *m_hybridPanel;
    QSpinBox  *m_numRays, *m_maxBounces, *m_rayCrossover;
    QCheckBox *m_specular, *m_diffuse;
    QComboBox *m_rayBandRes;
    QSpinBox  *m_ismOrder;
    QCheckBox *m_ismVisibility;
    QSpinBox  *m_hybridSplit;

    QComboBox *m_analysisType;          // IRF / RT60 / STI
    QCheckBox *m_thirdOctave;
    QComboBox *m_bandRange;

    QComboBox *m_auralSource;
    QCheckBox *m_outMono, *m_outStereo, *m_outBinaural, *m_outAmbi;
    // 材質設定 = 吸音バジェット (AcousticOpts::absorption) の View。
    // 同じデータを RoomAcousticsTab の吸音バジェット表とも共有する。
    QTableWidget *m_surfTable;
};

} // namespace ofd
