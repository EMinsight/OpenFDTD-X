// OutdoorNoiseTab.h — 屋外騒音伝搬タブ (specialized-acoustics.jsx OutdoorNoiseTab 相当)。
// ISO 9613-2 / CNOSSOS-EU による環境騒音予測 (SoundPLAN / CadnaA 相当):
//   音源モデル   — 道路交通 / 鉄道 / 工場・設備 / 風力発電 / 航空機 を切替
//   伝搬経路     — A_div (幾何拡散) / A_atm / A_gr / A_bar / A_misc / C_met
//   防音壁設計   — 高さ・位置・頂部形状 → 回折減衰 (Maekawa チャート)
//   等高線マップ — 道路断面の 50〜70 dB コンター (モックの SVG を QPainter で再現)
// 音響ドメイン向けタブ。モックは静的プロトタイプのため、設定はローカル state
// (メンバ既定値) のみで Project へは永続化しない。
#pragma once
#include <QScrollArea>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QStackedWidget;

namespace ofd {

class Project;

// 等高線マップ描画 (モックの <svg viewBox="0 0 340 160"> 相当)。
// 道路帯 + 5本の破線コンター (70/65/60/55/50 dB) + 防音壁 + 住宅列。
class NoiseContourView : public QWidget {
    Q_OBJECT
public:
    explicit NoiseContourView(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *) override;
};

class OutdoorNoiseTab : public QScrollArea {
    Q_OBJECT
public:
    explicit OutdoorNoiseTab(Project *project, QWidget *parent = nullptr);

private slots:
    void refresh();

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
    QLineEdit *m_recvHeight;

    // 防音壁 / barrier
    QLineEdit *m_barHeight, *m_barPos;
    QComboBox *m_barTop;
    QLabel    *m_barDelta;

    NoiseContourView *m_contour;
};

} // namespace ofd
