// CabinAcousticsTab.h — 車内・機内音響 (NVH) タブ
// (specialized-acoustics.jsx CabinAcousticsTab 相当)。
// 自動車 / EV / 鉄道 / 航空機の車内騒音を構造振動+音響連成で予測:
//   騒音源     — 対象ごとの寄与源 (エンジン透過/ロード/風切り/モーター…)
//   解析手法   — 帯域別 FEM-FDTD / FE-SEA ハイブリッド / SEA の使い分け表
//   車室モデル — CAD 読込 + 吸音内装 (いずれも未配線)
//   音響モード — 入力寸法からの剛壁直方体固有周波数 (実計算)
//   評価       — 耳位置 SPL / AI / Loudness / Sharpness (いずれも未計算)
//   対策検討   — 効果 [dB] / 重量 / コスト (利用者入力。重量のみ合計)
// モックは静的プロトタイプのため、設定はローカル state (メンバ既定値) のみで
// Project へは永続化しない。
//
// 音響モードの計算実体は Qt 非依存の src/acoustics/core/RoomModes に置き、
// selftest から解析解と直接突き合わせる (GUI に式を書かない)。
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

class CabinAcousticsTab : public QScrollArea {
    Q_OBJECT
public:
    explicit CabinAcousticsTab(Project *project, QWidget *parent = nullptr);

private slots:
    void refresh();
    void updateModes();      // 寸法 → 固有モード表 (RoomModes の実計算)
    void updateMetrics();    // 評価指標表 (値は未計算)
    void updateMeasures();   // 対策表の集計 (重量のみ)

private:
    QWidget *buildCarSources();
    QWidget *buildEvSources();
    QWidget *buildTrainSources();
    QWidget *buildAircraftSources();

    Project        *m_p;
    bool            m_updating = false;
    int             m_vehicleIdx = 0;   // mock: useState("car")
    QComboBox      *m_vehicle;
    QStackedWidget *m_srcStack;

    // 騒音源 (対象別) / noise sources
    QCheckBox *m_carEngine, *m_carRoad, *m_carWind, *m_carBoom;
    QCheckBox *m_evMotor, *m_evInverter, *m_evRoad, *m_evGear;
    QCheckBox *m_trRolling, *m_trTunnel, *m_trHvac;
    QCheckBox *m_acTbl, *m_acEngine, *m_acPressure;

    // 車室モデル / cabin model
    QLineEdit *m_cadFile;
    QCheckBox *m_absRoof, *m_absCarpet, *m_absDoor, *m_absSeat;

    // 車室音響モード / cabin acoustic modes (直方体近似)
    QCheckBox    *m_modal;
    QLineEdit    *m_dimL, *m_dimW, *m_dimH;   // 内寸 L×W×H [m]
    QLineEdit    *m_temp;                     // 室温 [℃] → 音速
    QLineEdit    *m_fmax;                     // 計算上限周波数 [Hz]
    QTableWidget *m_modeTable;
    QLabel       *m_modeSummary;

    // 評価 / metrics
    QCheckBox    *m_mSpl, *m_mAi, *m_mLoudness, *m_mSharpness;
    QTableWidget *m_metricTable;

    // 手法表 / 対策表
    QTableWidget *m_methodTable;
    QTableWidget *m_measureTable;
    QLabel       *m_measureSummary;
};

} // namespace ofd
