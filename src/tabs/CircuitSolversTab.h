// CircuitSolversTab.h — 回路系電磁解析タブ (circuit-solvers.jsx CircuitSolversTab 相当)。
// FDTD (主ソルバ) を補完する回路抽出用ソルバ PEEC / FEM 準静的 / FEM 波動 を選び、
//   モデル/ポート → 抽出設定 → SPICE連成 → 結果 のサブタブで一連の流れを扱う。
//
//   - ポート定義表は `Project::circuitPorts()` のビュー。編集はモデルへ書き戻し、
//     .ofdx ("circuit.ports") に保存される。「抽出実行」は OpenPEEC / OpenFEM を
//     QProcess で起動する。
//   - SPICE 連成ページは外部の回路図から出したネットリスト (.cir/.sp) を読み
//     (io/SpiceNetlist)、R/L/C を **`.ofd` の `load` 行としてプロジェクトへ
//     追加**できる。配置 (方向・座標) は利用者が表で与える — ネットリストは
//     位置を持たないので、GUI が座標を推測して捏造してはいけない。
//   - 結果ページは **抽出結果ではない**: PEEC/FEM 抽出が未実装なので寄生
//     パラメータは存在しない。代わりに利用者が入力した集中定数 RLC の
//     |Z(f)| を解析式 (em/LumpedRlc) で表示する (プロジェクトに .ofd の
//     load 行があれば、その値でフォームを初期化する)。
#pragma once
#include <QFont>
#include <QScrollArea>

#include "../io/SpiceNetlist.h"

class QCheckBox;
class QComboBox;
class QPlainTextEdit;
class QProcess;
class QPushButton;
class QLabel;
class QLineEdit;
class QStackedWidget;
class QTableWidget;
class QTableWidgetItem;
class QTabWidget;

namespace ofd {

class MiniPlot;
class Project;

class CircuitSolversTab : public QScrollArea {
    Q_OBJECT
public:
    explicit CircuitSolversTab(Project *project, QWidget *parent = nullptr);

private slots:
    void solverChanged(int index);      // ソルバ切替 → 説明文・抽出ページ・推定時間
    void refresh();                     // model → widgets (ポート表 / RLC 初期値)
    void refreshPorts();                // model → ポート表のみ
    void applyExtract();                // PEEC ページ → Project::circuit()
    void refreshExtract();              // Project::circuit() → PEEC ページ
    void onPortItemChanged(QTableWidgetItem *item);   // ポート表 → model
    void updateResults();               // 集中定数モデル → 結果表 + |Z| 曲線

private:
    // 抽出実行 (OpenPEEC / OpenFEM を QProcess で起動する)。
    // 入力生成 → 起動 → zin.csv の読み取り → 結果表示 まで。
    void runExtraction();
    void onExtractionFinished(int exitCode);
    void showZinCsv(const QString &path);
    void showFemLog(const QString &path);

    QWidget *buildModelPage();          // モデル/ポート
    QWidget *buildExtractPage();        // 抽出設定 (+ FDTD連成)
    QWidget *buildSpicePage();          // SPICE連成
    QWidget *buildResultsPage();        // 結果 (集中定数モデルの |Z|)
    QWidget *buildPeecPage();
    // SPICE ネットリストの取込 (パスが変わったら読み直す)
    void browseNetlist();
    void loadNetlist(const QString &path);
    void addNetlistLoads();     // 表の選択行 → Project::loads()
    QWidget *buildFemqPage();
    QWidget *buildFemwPage();

    Project        *m_p;
    bool            m_updating = false; // refresh 中の再入ガード
    QFont           m_mono;             // ネット名/基準名の等幅フォント

    QComboBox      *m_solver;           // PEEC / FEM 準静的 / FEM 波動
    QLabel         *m_solverDesc;
    QTabWidget     *m_tabs;

    QStackedWidget *m_extractStack;     // ソルバ別の抽出設定
    // PEEC ページの入力 → Project::circuit() (CircuitIO が読む値そのもの)
    QLineEdit *m_peecMesh = nullptr;    // 導体分割幅 [mm]
    QCheckBox *m_peecCp = nullptr;      // 部分容量 Cp   → peecCapacitance
    QCheckBox *m_peecR = nullptr;       // 抵抗 (表皮効果) → peecSkinEffect
    QCheckBox *m_peecRpeec = nullptr;   // 遅延 PEEC      → peecRetardation
    QCheckBox *m_peecQuasi = nullptr;   // 準静的 (= 遅延なし。rPEEC と排他)
    QLineEdit *m_peecFmin = nullptr;    // 周波数範囲 [MHz]
    QLineEdit *m_peecFmax = nullptr;
    QLineEdit *m_peecFdiv = nullptr;    // 分割数
    QLabel         *m_estimate;         // 推定計算時間
    QPushButton    *m_runExtract = nullptr;
    QLabel         *m_extractStatus = nullptr;
    QPlainTextEdit *m_extractLog = nullptr;
    QTableWidget   *m_zinTable = nullptr;    // zin.csv (周波数 × Rin/Xin)
    QProcess       *m_proc = nullptr;
    QString         m_runDir;

    QTableWidget   *m_portTable;

    // SPICE 連成: ネットリスト取込
    QLineEdit      *m_netFile = nullptr;
    QLabel         *m_netStatus = nullptr;
    QTableWidget   *m_netTable = nullptr;
    QPushButton    *m_netAdd = nullptr;
    SpiceNetlist    m_netlist;          // 読み込み済みの内容

    // 結果ページ: 集中定数モデルの入力と表示
    QComboBox      *m_rlcTopology = nullptr;   // 直列 / 並列
    QLineEdit      *m_rlcR = nullptr;          // R [Ω]
    QLineEdit      *m_rlcL = nullptr;          // L [nH]
    QLineEdit      *m_rlcC = nullptr;          // C [pF]
    QLabel         *m_rlcSource = nullptr;     // 初期値の出所 (load 行 / 既定)
    QLabel         *m_resonance = nullptr;     // LC 共振 f0
    QTableWidget   *m_resultTable;
    MiniPlot       *m_zPlot;
};

} // namespace ofd
