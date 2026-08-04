// SarTab.h — SAR / 生体電磁解析タブ (em-applications.jsx SarTab 相当)。
// 人体内の比吸収率 (SAR) を IEC 62704 / IEEE 1528 に沿って算出し、
// ICNIRP / FCC の電波防護指針との適合を判定する (OpenFDTD 本家の代表用途)。
// 人体モデルの選択で追加行が入れ替わる:
//   SAM頭部ファントム — 液剤 (HSL/BSL) と離隔距離
//   平板ファントム     — 追加行なし
//   数値人体モデル     — voxel モデル選択と組織数 (Gabriel 誘電率)
// モックは静的プロトタイプのため、設定は全てローカル state (.ofd 非対応)。
#pragma once
#include <QScrollArea>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QTableWidget;

namespace ofd {

class Project;

class SarTab : public QScrollArea {
    Q_OBJECT
public:
    explicit SarTab(Project *project, QWidget *parent = nullptr);

private slots:
    void refresh();
    void onModelChanged();         // 人体モデル → 追加行の出し入れ
    void updatePointSar();         // σ/ρ/|E| → 点 SAR (src/em/SarMetrics)

private:
    QWidget *buildVoxelPane(QWidget *parent);
    QWidget *buildHeadPane(QWidget *parent);
    QWidget *buildPointSarSection(QWidget *parent);
    void fillMetricsTable();       // 周波数・曝露区分 → 規格の指針値を引き直す

    Project *m_p;
    bool     m_updating = false;
    int      m_modelIdx = 0;       // mock: useState("head") 0=head 1=flat 2=voxel

    // 人体モデル / human model
    QButtonGroup *m_model = nullptr;
    QWidget      *m_voxelPane = nullptr;
    QComboBox    *m_voxelModel = nullptr;
    QLabel       *m_tissueCount = nullptr;
    QCheckBox    *m_gabriel = nullptr;
    QWidget      *m_headPane = nullptr;
    QButtonGroup *m_liquid = nullptr;
    QLineEdit    *m_gap = nullptr;

    // 曝露源 / exposure source
    QComboBox *m_device = nullptr;
    QLineEdit *m_freq = nullptr;
    QCheckBox *m_multiBand = nullptr;
    QLineEdit *m_txPower = nullptr;
    QComboBox *m_category = nullptr;    // 一般環境 / 職業 (指針値の選択に使う)

    // 点 SAR 換算 / point SAR (定義式の実計算)
    QLineEdit *m_ptSigma = nullptr;
    QLineEdit *m_ptRho = nullptr;
    QLineEdit *m_ptField = nullptr;
    QLineEdit *m_ptCp = nullptr;
    QLineEdit *m_ptTime = nullptr;
    QLabel    *m_ptResult = nullptr;

    // 評価指標 / metrics
    QTableWidget *m_metrics = nullptr;
    QCheckBox    *m_bioHeat = nullptr;
    QCheckBox    *m_uncertainty = nullptr;
    QCheckBox    *m_zoning = nullptr;
};

} // namespace ofd
