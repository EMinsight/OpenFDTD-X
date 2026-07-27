// EmcTab.h — EMC/EMI 規格適合解析タブ (em-applications.jsx EmcTab 相当)。
// CISPR / FCC / IEC 61000 の試験配置そのままを FDTD で再現し、試作前に
// 規格逸脱を予測する。モードは 3 択で、表示するセクションが切り替わる:
//   放射エミッション — 試験配置 / 放射源 / 判定結果 (表+スペクトル) / 対策検討
//   伝導エミッション — LISN・電流プローブ・CDN と QP/AV 検波
//   イミュニティ     — RS / ESD / EFT / サージ の試験レベルと誘導電圧
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

class MiniPlot;
class Project;

class EmcTab : public QScrollArea {
    Q_OBJECT
public:
    explicit EmcTab(Project *project, QWidget *parent = nullptr);

private slots:
    void refresh();
    void onModeChanged();          // モード → 表示セクションを切替

private:
    QWidget *buildEmissionPage();
    QWidget *buildConductedPage();
    QWidget *buildImmunityPage();
    void fillComplianceTable();
    void fillMitigationTable();
    void updateSpectrumPlot();

    Project *m_p;
    bool     m_updating = false;
    int      m_modeIdx = 0;        // mock: useState("emission") 0=emission

    QButtonGroup *m_mode = nullptr;
    QComboBox    *m_standard = nullptr;

    // ── 放射エミッション / emission ─────────────────────────────────────────
    QWidget      *m_emissionPage = nullptr;
    QButtonGroup *m_site = nullptr;
    QLineEdit    *m_distance = nullptr;
    QLineEdit    *m_antHeight = nullptr;
    QLineEdit    *m_clock = nullptr;
    QCheckBox    *m_turnTable = nullptr;
    QCheckBox    *m_bothPol = nullptr;
    QCheckBox    *m_gndPec = nullptr;
    QCheckBox    *m_gndCable = nullptr;
    QCheckBox    *m_srcSwitching = nullptr;
    QCheckBox    *m_srcCommonMode = nullptr;
    QCheckBox    *m_srcSlit = nullptr;
    QTableWidget *m_compTable = nullptr;
    MiniPlot     *m_spectrum = nullptr;
    QTableWidget *m_mitTable = nullptr;
    QLabel       *m_mitBadge = nullptr;

    // ── 伝導エミッション / conducted ────────────────────────────────────────
    QWidget      *m_conductedPage = nullptr;
    QButtonGroup *m_condSetup = nullptr;
    QLineEdit    *m_condFreq = nullptr;
    QCheckBox    *m_detQp = nullptr;
    QCheckBox    *m_detAv = nullptr;

    // ── イミュニティ / immunity ─────────────────────────────────────────────
    QWidget      *m_immunityPage = nullptr;
    QButtonGroup *m_immTest = nullptr;
    QLineEdit    *m_immLevel = nullptr;
    QLineEdit    *m_esdVolt = nullptr;
    QCheckBox    *m_immField = nullptr;
    QCheckBox    *m_immInduced = nullptr;
    QLabel       *m_immBadge = nullptr;
};

} // namespace ofd
