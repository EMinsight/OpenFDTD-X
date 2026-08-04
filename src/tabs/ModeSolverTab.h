// ModeSolverTab.h — 〓 モードソルバ FDE (元 mock: pic-tools.jsx ModeSolverTab)。
//
// 導波路断面 (x,y) の固有モードを内蔵 FDE ソルバ (src/optics/FdeModeSolver) で
// 実計算して表示する画面 (光ドメイン)。屈折率は src/optics/MaterialDispersion の
// 公刊 Sellmeier 係数から n(λ, T) として取り込むため、材料・波長・温度が
// そのまま計算に効く。
//
// 実計算する量: neff / ng (=neff−λ·dneff/dλ) / 閉込め係数 Γ / 実効断面積 Aeff /
//               群速度分散 D / 複屈折 Δn / dneff/dT / dneff/dw /
//               プロセスコーナーの neff・ng・リング共振波長シフト。
// 本ソルバの範囲外 (画面に明示する — CLAUDE.md 絶対規則 5):
//   - 伝搬損失 [dB/cm] : 実屈折率のみを扱うため散乱損・吸収損は求まらない
//   - 曲げ損失         : 曲がり導波路の漏れモード解析が別途必要 (固定サンプル値)
//   - モード波源 / モード展開モニター / Schematic への受け渡し (受け側モデル未実装)
#pragma once
#include <QPointF>
#include <QScrollArea>
#include <QString>
#include <QVector>
#include <atomic>
#include <memory>
#include <string>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QTableWidget;
class QTimer;

namespace ofd {

class FieldHeatmap;
class MiniPlot;
class Project;

// FDE 計算のデータ受け渡し用の値型。ワーカースレッドへ値渡しするため
// QWidget / Project を参照しない (Qt の値型のみ)。
namespace mds {

// 画面入力から作った計算条件一式
struct Setup {
    double w_um = 0.0, h_um = 0.0, slab_um = 0.0;   // コア幅・高さ・スラブ厚
    double lambda_um = 0.0, temp_C = 0.0;
    double dx_um = 0.0;              // 目標格子間隔 (実値は断面組み立てで丸まる)
    double marginRatio = 1.6;        // 窓の余白 (コア寸法比)
    bool   te = true;                // true = TE (Ex 主成分) / false = TM
    std::string coreId, cladId, subId;   // MaterialDispersion の材料 id
};

// 1 モードの計算結果 (表示に必要な分だけ)
struct ModeRow {
    QString name;                    // "TE0" 等 (neff 降順の並び)
    double  neff = 0.0, gamma = 0.0, aeff_um2 = 0.0;
    bool    hasNg = false;
    double  ng = 0.0;
    QVector<double> intensity;       // |E|² (0..1, row-major, 先頭行が +y 側)
    int     nx = 0, ny = 0;
    double  dx_um = 0.0, dy_um = 0.0;
};

// 分散解析の結果 (has* が false の項目は「未計算」と表示する)
struct Dispersion {
    bool   hasD = false;      double D_ps_nm_km = 0.0;
    bool   hasBiref = false;  double biref = 0.0;        // neff(TE0) − neff(TM0)
    bool   hasDnDt = false;   double dneff_dT = 0.0;     // [1/K]
    bool   hasDnDw = false;   double dneff_dw = 0.0;     // [1/nm]
    QVector<QPointF> curve;                              // 掃引カーブ
    QString xLabel, note;
};

// プロセスコーナー 1 点
struct CornerRow {
    QString name;
    bool   ok = false;                  // 導波モードが求まったか
    double neff = 0.0;
    bool   hasNg = false;  double ng = 0.0;
    bool   hasDl = false;  double dlambda_nm = 0.0;   // Δλ = λ·Δneff/ng
};

} // namespace mds

class ModeSolverTab : public QScrollArea {
    Q_OBJECT
public:
    explicit ModeSolverTab(Project *project, QWidget *parent = nullptr);

    // 設計検討ツール (モデル非結合) のため apply/refresh は何もしない
    void apply() {}
    void refresh() {}

private:
    // 入力 → 計算条件。材料の λ 有効範囲外などは false + 理由を返す
    bool buildSetup(mds::Setup &s, QString &err) const;
    void updateIndexLabel();     // 屈折率プレビュー更新 + 既存結果の破棄
    void clearResults();
    void setBusy(bool busy, int totalSteps, const QString &status);
    void runModes();
    void runSweep();
    void runCorners();
    void showModes();
    void showDispersion();
    void showCorners();
    void updateFieldView();

    Project *m_p;

    QComboBox      *m_shape = nullptr;      // ストリップ / リブ
    QDoubleSpinBox *m_width = nullptr, *m_height = nullptr, *m_slab = nullptr;
    QComboBox      *m_matCore = nullptr, *m_matClad = nullptr, *m_matSub = nullptr;
    QDoubleSpinBox *m_lambda = nullptr, *m_temp = nullptr;
    QComboBox      *m_pol = nullptr;        // TE / TM
    QLabel         *m_indexLabel = nullptr; // n(λ,T) のプレビュー / 入力エラー
    QLabel         *m_status = nullptr;     // 計算状態
    QPushButton    *m_btnRun = nullptr, *m_btnSweep = nullptr,
                   *m_btnCorner = nullptr, *m_btnField = nullptr;
    QVector<QWidget *> m_inputs;            // 計算中に無効化する入力群

    QTableWidget *m_modeTable = nullptr;
    QLabel       *m_singleModeBadge = nullptr;
    QLabel       *m_gridNote = nullptr;      // 離散化誤差の注記 (計算後のみ)
    FieldHeatmap *m_field = nullptr;
    QLabel       *m_fieldCaption = nullptr;
    QComboBox    *m_sweepSel = nullptr;
    MiniPlot     *m_dispPlot = nullptr;
    QTableWidget *m_dispTable = nullptr;
    QLabel       *m_dispNote = nullptr;      // 未算出の理由・除外点の注記
    QTableWidget *m_cornerTable = nullptr;
    QLabel       *m_cornerNote = nullptr;

    // 非同期実行 (GUI スレッドを塞がない — .claude/rules/gui.md)
    bool  m_busy = false;
    int   m_steps = 0;                              // 進捗の総ステップ数
    QString m_busyLabel;
    QTimer *m_poll = nullptr;
    std::shared_ptr<std::atomic<int>> m_progress;

    QVector<mds::ModeRow>   m_modes;
    mds::Dispersion         m_disp;
    QVector<mds::CornerRow> m_corners;
};

} // namespace ofd
