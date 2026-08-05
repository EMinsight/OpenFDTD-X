// UltrasoundTab.h — 超音波解析タブ (specialized-acoustics.jsx UltrasoundTab 相当)。
// MHz 帯の超音波伝搬 (k-Wave / Field II 相当) を 4 用途で切替:
//   医療イメージング — アレイ型式・素子数・中心周波数・フォーカス深度
//   HIFU治療         — 集束型式・音響出力・焦点音圧 (非線形域)
//   NDT非破壊検査    — 探触子 (垂直/斜角/PAUT/TOFD) と検査対象
//   パラメトリックアレイ — 1次周波数と差音 (指向性スピーカー)
// 媒質表は用途で切替 (NDT: 鋼/CFRP/水、医療: 水/軟組織/脂肪/肝臓/骨)。
// 非線形 (B/A) の既定 ON は HIFU のときだけ (モックの checked={app==="hifu"})。
// モックは静的プロトタイプのため、設定はローカル state のみで永続化しない。
// 媒質表は文献値データベース (src/acoustics/core/FocusedField) の表示で、
// 選択した行は HIFU の焦点音場計算に使われ、ボタンで Project の材料 (ρ, c)
// へも反映できる。焦点音圧・MI・Gol'dberg 数は O'Neil (1949) の閉形式による
// 実計算 (固定のサンプル値ではない)。
#pragma once
#include <QScrollArea>

#include "../acoustics/core/FocusedField.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
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
    void updateHifu();         // 入力 + 選択媒質 → 焦点音場 (実計算)
    void refreshMaterials();   // Project::materials() → 反映先コンボ
    void applyMediumToMaterial();  // 選択媒質の ρ, c を材料へ書き込む

private:
    ofd::acoustics::ultrasound::Medium currentMedium() const;
    QWidget *buildMedicalTrans();
    QWidget *buildHifuTrans();
    QWidget *buildFocusSection(QWidget *parent);
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
    // HIFU (入力 + 実計算の表示欄)
    QComboBox *m_hifuType;
    QLineEdit *m_hifuFreq, *m_hifuPower;
    QLineEdit *m_hifuAperture = nullptr;   // 開口径 (直径) [mm]
    QLineEdit *m_hifuFocal = nullptr;      // 曲率半径 R [mm]
    QLabel *m_hifuMedium = nullptr;        // 評価に使った媒質
    QLabel *m_hifuPressure = nullptr;      // 焦点音圧
    QLabel *m_hifuIntensity = nullptr;     // 焦点強度
    QLabel *m_hifuGain = nullptr;          // 音圧利得 kh
    QLabel *m_hifuAtten = nullptr;         // 音源→焦点の減衰
    QLabel *m_hifuBeamWidth = nullptr;     // −6 dB 幅
    QLabel *m_hifuMi = nullptr;            // 機械指標 MI
    QLabel *m_hifuShock = nullptr;         // 衝撃形成距離
    QLabel *m_hifuGoldberg = nullptr;      // Gol'dberg 数
    QLabel *m_hifuRegime = nullptr;        // 非線形域バッジ
    QLabel *m_hifuNote = nullptr;          // 仮定と適用範囲の注記
    QWidget *m_focusSection = nullptr;     // 焦点音場セクション (HIFU のみ表示)
    // NDT
    QComboBox *m_probeType, *m_ndtTarget;
    QLineEdit *m_ndtFreq;
    // パラメトリックアレイ / sonar
    QLineEdit *m_primFreq, *m_diffFreq;

    // 媒質 / medium (文献値データベース + 材料への反映)
    QTableWidget *m_medTable;
    QCheckBox    *m_powerLaw, *m_nonlinear;
    QComboBox    *m_matTarget = nullptr;     // 反映先の材料
    QPushButton  *m_applyMedium = nullptr;
    QLabel       *m_applyStatus = nullptr;

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
