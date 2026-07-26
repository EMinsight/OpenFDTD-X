// UltrasoundTab.h — 超音波解析タブ (specialized-acoustics.jsx UltrasoundTab 相当)。
// MHz 帯の超音波伝搬 (k-Wave / Field II 相当) を 4 用途で切替:
//   医療イメージング — アレイ型式・素子数・中心周波数・フォーカス深度
//   HIFU治療         — 集束型式・音響出力・焦点音圧 (非線形域)
//   NDT非破壊検査    — 探触子 (垂直/斜角/PAUT/TOFD) と検査対象
//   パラメトリックアレイ — 1次周波数と差音 (指向性スピーカー)
// 媒質表は用途で切替 (NDT: 鋼/CFRP/水、医療: 水/軟組織/脂肪/肝臓/骨)。
// 非線形 (B/A) の既定 ON は HIFU のときだけ (モックの checked={app==="hifu"})。
// モックは静的プロトタイプのため、設定はローカル state のみで永続化しない。
#pragma once
#include <QScrollArea>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QStackedWidget;
class QTableWidget;

namespace ofd {

class Project;

class UltrasoundTab : public QScrollArea {
    Q_OBJECT
public:
    explicit UltrasoundTab(Project *project, QWidget *parent = nullptr);

private slots:
    void refresh();
    void onAppChanged();       // 用途 → トランスデューサ/媒質/出力 を切替

private:
    QWidget *buildMedicalTrans();
    QWidget *buildHifuTrans();
    QWidget *buildNdtTrans();
    QWidget *buildSonarTrans();
    QWidget *buildMedicalOut();
    QWidget *buildHifuOut();
    QWidget *buildNdtOut();
    QWidget *buildSonarOut();
    void fillMediumTable(bool ndt);

    Project        *m_p;
    bool            m_updating = false;
    int             m_appIdx = 0;      // mock: useState("medical") 0=medical
    QComboBox      *m_app;
    QStackedWidget *m_transStack;
    QStackedWidget *m_outStack;

    // 医療イメージング / medical
    QComboBox *m_arrayType;
    QLineEdit *m_elements, *m_centerFreq, *m_bandwidth, *m_focusDepth;
    // HIFU
    QComboBox *m_hifuType;
    QLineEdit *m_hifuFreq, *m_hifuPower;
    // NDT
    QComboBox *m_probeType, *m_ndtTarget;
    QLineEdit *m_ndtFreq;
    // パラメトリックアレイ / sonar
    QLineEdit *m_primFreq, *m_diffFreq;

    // 媒質 / medium
    QTableWidget *m_medTable;
    QCheckBox    *m_powerLaw, *m_nonlinear;

    // ビームフォーミング / beamforming
    QCheckBox *m_txFocus, *m_rxFocus, *m_apod, *m_planeWave;
    QLineEdit *m_steerAngle;

    // 出力 / outputs
    QCheckBox *m_oBeam, *m_oPsf, *m_oBmode, *m_oMiTi;          // medical
    QCheckBox *m_oIspta, *m_oCem43, *m_oCavitation;            // hifu
    QCheckBox *m_oScan, *m_oDac, *m_oSaft;                     // ndt
    QCheckBox *m_oDiffEff, *m_oBeamPattern;                    // sonar
};

} // namespace ofd
