// OutdoorNoiseTab.h — 屋外騒音伝搬タブ (specialized-acoustics.jsx OutdoorNoiseTab 相当)。
// ISO 9613-2 / CNOSSOS-EU による環境騒音予測 (SoundPLAN / CadnaA 相当):
//   音源モデル   — 道路交通 / 鉄道 / 工場・設備 / 風力発電 / 航空機 を切替
//   伝搬経路     — A_div (幾何拡散) / A_atm / A_gr / A_bar / A_misc / C_met
//                  (実装済みは A_div と A_bar のみ。他はチェックしても効かない)
//   防音壁設計   — 高さ・位置・音源高さ・受音点から回折減衰 ΔL を実計算
//                  (前川チャート。オクターブ帯域別の表も出す)
//   騒音予測     — 基準距離のレベルから幾何拡散 + 回折減衰で受音点レベルを求め、
//                  騒音に係る環境基準 (平成10年環境庁告示第64号) と比較判定する
//                  断面図つき
// 音響ドメイン向けタブ。モックは静的プロトタイプのため、設定はローカル state
// (メンバ既定値) のみで Project へは永続化しない。
//
// 計算実体は Qt 非依存の src/acoustics/core/EnvironmentalNoise に置き、
// selftest から解析解・規格値と直接突き合わせる (GUI に式を書かない)。
#pragma once
#include <QColor>
#include <QScrollArea>
#include <QString>
#include <QVector>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QStackedWidget;
class QTableWidget;

namespace ofd {

class Project;

// 断面図に立てる等レベル線 1 本 (計算で求めた距離)。
struct NoiseContourMark {
    double  distM = 0;      // 音源からの距離 [m]
    double  levelDb = 0;    // そのレベル [dB]
    QString color;          // 描画色
};

// 断面図の描画データ。valid = false のときは「未計算」と描く
// (モックの固定 SVG を計算結果から描く図へ置き換えたもの)。
struct NoiseProfileData {
    bool   valid = false;
    double maxDistM = 0;      // 図の右端 [m]
    double srcHeightM = 0;
    bool   barrier = false;
    double barDistM = 0;
    double barHeightM = 0;
    bool   shadow = false;    // 壁が見通し線を遮っている
    double recvDistM = 0;
    double recvHeightM = 0;
    double recvLevelDb = 0;
    bool   limitValid = false;
    double limitDb = 0;
    bool   pass = false;      // 環境基準に適合
    QVector<NoiseContourMark> contours;
};

// 音源 — 防音壁 — 受音点の鉛直断面図 (地面・回折経路・等レベル線)。
class NoiseProfileView : public QWidget {
    Q_OBJECT
public:
    explicit NoiseProfileView(QWidget *parent = nullptr);
    void setData(const NoiseProfileData &d);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    NoiseProfileData m_data;
};

class OutdoorNoiseTab : public QScrollArea {
    Q_OBJECT
public:
    explicit OutdoorNoiseTab(Project *project, QWidget *parent = nullptr);

private slots:
    void refresh();
    // 幾何拡散 + 回折減衰 + 環境基準比較を計算し直して表示へ反映する
    void recompute();

private:
    QWidget *buildRoadPage();
    QWidget *buildRailPage();
    QWidget *buildIndustryPage();
    QWidget *buildWindPage();
    QWidget *buildAircraftPage();

    Project        *m_p;
    bool            m_updating = false;
    int             m_scenario = 0;      // mock: useState("road")
    QComboBox      *m_srcType;
    QStackedWidget *m_srcStack;

    // 道路交通 / road
    QLineEdit *m_traffic, *m_heavyRatio, *m_speed;
    QComboBox *m_pavement, *m_roadModel;
    // 鉄道 / rail
    QComboBox *m_trainType;
    QLineEdit *m_trainCount;
    QCheckBox *m_rolling, *m_structure, *m_aero;
    // 工場・設備 / industry
    QLineEdit *m_plantPwl;
    QComboBox *m_operation;
    QCheckBox *m_buildingIns, *m_directivity;
    // 風力発電 / wind
    QComboBox *m_turbine;
    QLineEdit *m_turbineCount;
    QCheckBox *m_swish, *m_lowFreq;
    // 航空機 / aircraft
    QComboBox *m_acType, *m_acMetric;
    QLineEdit *m_flights;

    // 伝搬経路 / propagation effects
    QCheckBox *m_aDiv, *m_aAtm, *m_aGr, *m_aBar, *m_aMisc, *m_cMet;
    QLineEdit *m_recvHeight, *m_recvDist;

    // 防音壁 / barrier
    QLineEdit    *m_srcHeight, *m_barHeight, *m_barPos, *m_barFreq;
    QComboBox    *m_barTop;
    QLabel       *m_barDelta;     // ΔL の計算結果 (または未計算の理由)
    QTableWidget *m_barBands;     // オクターブ帯域別 ΔL

    // 騒音予測 / prediction and compliance
    QLabel           *m_srcKind;      // 点音源 / 線音源 (音源種別から決まる)
    QLineEdit        *m_refLevel, *m_refDist;
    QLabel           *m_pwlNote;      // 工場 PWL からの換算の説明
    QComboBox        *m_areaType, *m_period;
    QLabel           *m_predResult;   // 予測レベルと内訳
    QLabel           *m_judge;        // 環境基準への適合判定
    QLabel           *m_stdSource;    // 基準値の出所
    NoiseProfileView *m_profile;
};

} // namespace ofd
