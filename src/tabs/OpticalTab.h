// OpticalTab.h — 光解析タブ: FDTD/RCWA/BPM/FMM ソルバー切替 + 光モード設定.
// Settings persist in the .ofdx sidecar; RCWA/BPM run the OpenRCWA (orcwa) /
// OpenBPM (obpm) sister kernels through Runner.
//
// 非線形 (TPA) / ONN 活性化 (Honda, Shoji, Amemiya, Opt. Lett. 49, 5811
// (2024)): BPM セクションで tpa / powersweep キーを設定し、obpm 実行後の
// activation_curve.csv を活性化カーブ P_out(P_in)・透過率 T(P_in) として表示。
#pragma once
#include <QScrollArea>

class QComboBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class QCheckBox;
class QStackedWidget;
class QTableWidget;

namespace ofd {

class Project;
class MiniPlot;

class OpticalTab : public QScrollArea {
    Q_OBJECT
public:
    explicit OpticalTab(Project *project, QWidget *parent = nullptr);

    // obpm 実行完了後に MainWindow から呼ばれる。workdir の
    // activation_curve.csv があれば読み込んで ONN 活性化カーブを表示する。
    // aeff_m2 はカーネルログ "ONN: A_eff = ... [m^2]" から抽出した値 (0=不明)。
    void showActivationResult(const QString &workdir, double aeff_m2);

private slots:
    void refresh();

private:
    void apply();
    void updateTpaWidgetState();

    Project   *m_p;
    bool       m_updating = false;

    QComboBox *m_solver;
    QStackedWidget *m_solverStack;
    QComboBox *m_mode;
    QLineEdit *m_lambdaMin, *m_lambdaMax;
    QSpinBox  *m_lambdaDiv;

    // RCWA
    QSpinBox  *m_rcwaNx, *m_rcwaNy, *m_rcwaLayers;
    QLineEdit *m_rcwaPx, *m_rcwaPy;
    // BPM
    QComboBox *m_bpmAlgo, *m_bpmInput;
    QLineEdit *m_bpmDz, *m_bpmN0;
    // BPM: 非線形 (TPA) / ONN 活性化
    QCheckBox *m_tpaEnable, *m_psEnable;
    QSpinBox  *m_tpaMatId, *m_psPoints;
    QLineEdit *m_tpaBeta, *m_psPmin, *m_psPmax;
    QComboBox *m_psScale;
    QLabel    *m_tpaWarn;
    // FMM
    QSpinBox  *m_fmmHarmonics;
    QCheckBox *m_fmmLi;
    // BPF / Ring
    QLineEdit *m_bpfMin, *m_bpfMax, *m_bpfQ;
    QLineEdit *m_ringR, *m_ringGap;

    // ONN 活性化カーブ結果表示
    QLabel       *m_onnStatus;
    MiniPlot     *m_onnPlotP, *m_onnPlotT;
    QTableWidget *m_onnTable;
};

} // namespace ofd
