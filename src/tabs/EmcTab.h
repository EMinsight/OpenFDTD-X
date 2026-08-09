// EmcTab.h — EMC/EMI 規格適合解析タブ (em-applications.jsx EmcTab 相当)。
// CISPR / FCC / IEC 61000 の試験配置そのままを FDTD で再現し、試作前に
// 規格逸脱を予測する。モードは 3 択で、表示するセクションが切り替わる:
//   放射エミッション — 試験配置 / 放射源 / 判定結果 (限度値) / 対策検討
//   伝導エミッション — LISN・電流プローブ・CDN と QP/AV 検波
//   イミュニティ     — RS / ESD / EFT / サージ の試験レベルと派生量
//
// 数値の出所を混同させないための区別 (絶対規則 5):
//   - **限度値** (判定結果の「規格限度」列と曲線) は規格の公表値 = 実データ。
//     `em/EmcStandards` が持ち、測定距離は逆距離則で換算する。
//   - **被測定値・マージン・判定** は、FDTD の遠方界 (far1d.log) と利用者が
//     与えた放射電力 P_t から予測値を出せるときだけ埋まる。far1d.log が無い
//     か P_t が未入力なら「—」のまま (`em/RadiatedEmission` 参照)。
//     予測であって実測ではないことを画面に明示する。
//   - **対策検討の改善量** と **シールド SE** は入力値から古典式で実計算する
//     (挿入損失 / 開口 SE / SE = A+R+B)。
//   - **イミュニティ** は試験レベルから規格の定義どおり決まる量 (電力密度・
//     ESD 電流) だけを出し、筐体内部電界・誘導電圧の判定は「—」。
// 設定は全てローカル state (.ofd / .ofdx には保存しない)。
#pragma once
#include <QPointF>
#include <QScrollArea>
#include <QString>
#include <QVector>

#include "../em/EmcStandards.h"
#include "../io/KernelResultReader.h"

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
    void updateCompliance();       // 規格・クラス・距離 → 限度値表・予測・曲線
    void updateMitigation();       // 対策パラメータ → 改善量とシールド SE
    void updateImmunity();         // 試験レベル → 規格の定義値
    void browseFar1d();            // far1d.log を選ぶ
    void reloadFar1d();            // パスが変わったら読み直して再計算

private:
    QWidget *buildEmissionPage();
    QWidget *buildPredictionSection(QWidget *page);
    // far1d.log + P_t + 測定距離 → 予測表と m_predPoints。
    // seg/n は限度値区間 (マージン列に使う)、distM は測定距離 [m]。
    void rebuildPrediction(const em::emc::LimitSegment *seg, int n,
                           double distM);
    // far1d.log の自動探索 (プロジェクトの隣)。見つからなければ空。
    QString guessFar1dPath() const;
    QWidget *buildConductedPage();
    QWidget *buildImmunityPage();
    void buildMitigationRows();    // 対策表の行を作る (初回のみ)

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
    // 判定結果 (限度値 = 実データ / 被測定値 = 未取得)
    QComboBox    *m_class = nullptr;        // Class A / B
    QLabel       *m_compNote = nullptr;     // 何が実データで何が未取得か
    QLabel       *m_distNote = nullptr;     // 距離換算の説明
    QLabel       *m_projFreqNote = nullptr; // プロジェクト解析周波数との関係
    QTableWidget *m_compTable = nullptr;
    MiniPlot     *m_spectrum = nullptr;
    // 予測 (FDTD の遠方界 + 利用者が与えた放射電力)
    QLineEdit    *m_predFile = nullptr;     // far1d.log のパス
    QLineEdit    *m_predPower = nullptr;    // 放射電力 P_t [W]
    QLineEdit    *m_predMaxDim = nullptr;   // 放射体の最大寸法 [m] (遠方界判定)
    QCheckBox    *m_predGround = nullptr;   // グランド反射 +6 dB を加算
    QLabel       *m_predStatus = nullptr;   // 読み込み状態と適用限界
    QTableWidget *m_predTable = nullptr;
    QVector<FarPattern> m_far1d;            // 読み込み済みパターン
    QString      m_far1dLoaded;             // m_far1d の出所 (再読込の判定)
    QVector<QPointF> m_predPoints;          // 予測スペクトル (MHz, dBμV/m)
    // 対策検討 (古典式による実計算)
    QLineEdit    *m_mitFreq = nullptr;      // 評価周波数 [MHz]
    QLineEdit    *m_mitZc = nullptr;        // 回路インピーダンス [Ω]
    QTableWidget *m_mitTable = nullptr;
    QLabel       *m_mitBadge = nullptr;     // 合計改善量
    QComboBox    *m_shieldMat = nullptr;
    QLineEdit    *m_shieldThick = nullptr;  // 板厚 [mm]
    QLineEdit    *m_apLen = nullptr;        // 開口の最長寸法 [mm]
    QLineEdit    *m_apCount = nullptr;      // 開口数
    QLabel       *m_shieldOut = nullptr;    // δ / A / R / B / SE
    QLabel       *m_shieldNet = nullptr;    // 開口 SE と正味 SE

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
    QLabel       *m_immDerived = nullptr;   // 規格の定義値 (実計算)
    QLabel       *m_immBadge = nullptr;     // 判定 = 「—」
};

} // namespace ofd
