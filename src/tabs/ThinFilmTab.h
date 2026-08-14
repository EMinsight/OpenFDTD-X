// ThinFilmTab.h — 薄膜多層膜設計タブ (optical-applications.jsx ThinFilmTab 相当)。
// Essential Macleod / OptiLayer / TFCalc 相当の設計環境:
//   - プリセット (ARコート / DBR / バンドパス / ダイクロイック / Low-E / 偏光子)
//     を選ぶと λ₀ における四分の一波長 (QWOT) 起点の層構成が組み上がる
//   - 層構成   : 入射媒質 + 層スタック表 (材料/n/k/物理膜厚/nd·λ₀⁻¹/役割) + 基板
//   - 分光特性 : 入射角・波長範囲 + R/T スペクトル MiniPlot + 指標表
//   - 最適化設計: ターゲット表とメリット関数 + 膜厚の最適化
//     (単純降下法 = Nelder-Mead と遺伝的アルゴリズムの 2 つ。層数・材料を
//      変える needle / tunneling は未実装)
//   - 製造・誤差: 膜厚誤差のモンテカルロ歩留まり (層間の相関つき) と膜厚感度
//
// 数値はすべて src/optics/ThinFilmStack (特性行列法, Qt 非依存) による実計算で、
// 屈折率は src/optics/MaterialDispersion (公刊 Sellmeier) と core/GlassCatalog
// から取る。層構成はこのタブが保持する編集可能なデータで、表の編集が
// そのまま計算に反映される (.ofd / .ofdx への永続化は未対応)。
#pragma once
#include <QScrollArea>
#include <QString>
#include <QVector>

#include "../optics/ThinFilmStack.h"

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QTabWidget;

namespace ofd {

class MiniPlot;
class Project;

class ThinFilmTab : public QScrollArea {
    Q_OBJECT
public:
    explicit ThinFilmTab(Project *project, QWidget *parent = nullptr);

private slots:
    void presetChanged(int index);      // プリセット → 層構成・波長・ターゲット
    void recompute();                   // 層構成 → TMM → 図・指標・Merit・感度
    void runMonteCarlo();               // 製造誤差モンテカルロ (ボタン)
    void expandPeriodic();              // 周期記法 → 層構成 (展開ボタン)
    void runOptimization();             // 膜厚最適化 (シンプレックス法)
    void exportRecipe();                // 成膜レシピをテキストで書き出す
    void showSensitivity();             // 層ごとの膜厚感度を一覧表示

private:
    // 編集対象の層 1 枚 (表と 1:1)
    struct StackLayer {
        QString mat;                    // MaterialDispersion の材料 id
        double  k = 0.0;                // 消衰係数 (利用者入力。カタログには無い)
        double  d_nm = 0.0;             // 物理膜厚 [nm]
        bool    enabled = true;         // 計算に含めるか
    };
    // ターゲット表 1 行 (メリット関数・歩留まり判定の入力)
    struct TargetRow {
        double lam0 = 400, lam1 = 700;  // [nm]
        int    quantity = 0;            // 0=R 1=T
        int    pol = 0;                 // 0=平均 1=s 2=p
        double goal = 0.0;              // [%]
        double tol = 0.5;               // [%]
        double weight = 1.0;
        int    samples = 21;
    };

    QWidget *buildStackPage();
    QWidget *buildSpecPage();
    QWidget *buildDesignPage();
    QWidget *buildMfgPage();

    void applyLayerTable();             // 表 → m_stack (膜厚・k・有効)
    void rebuildLayerTable();           // m_stack → 表 (行構成ごと作り直す)
    void updateDerivedCells();          // n(λ₀)・光学膜厚・役割だけ更新
    void applyTargetTable();            // 表 → m_targets
    void rebuildTargetTable();          // m_targets → 表

    std::vector<optics::TargetBand> targetBands() const;
    // 現在の設定で λ [nm] の層構成を返すコールバックを作る
    optics::StackAtLambda makeStackFn() const;
    double lambda0() const;
    double aoiDeg() const;

    Project      *m_p;
    bool          m_updating;

    // 設計データ (表の実体)
    QVector<StackLayer> m_stack;
    QVector<TargetRow>  m_targets;

    // 上段 (プリセット)
    QComboBox    *m_preset;
    QLabel       *m_layerBadge;         // "N 層" (badge acc)
    QLabel       *m_targetLabel;        // "目標: …"
    QTabWidget   *m_tabs;

    // 層構成
    QComboBox    *m_incident;
    QComboBox    *m_substrate;
    QLineEdit    *m_lambda0;
    QTableWidget *m_layerTable;
    QLineEdit    *m_periodic;
    QCheckBox    *m_useDispersion;
    QCheckBox    *m_useAbsorption;

    // 分光特性
    QLineEdit    *m_aoi;
    QCheckBox    *m_angleSweep;
    QCheckBox    *m_splitSP;
    QLineEdit    *m_lamMin;
    QLineEdit    *m_lamMax;
    MiniPlot     *m_specPlot;
    // R/T/A 表示 (A = 1 − R − T を図へ足す)
    QPushButton  *m_rtaBtn = nullptr;
    QLabel       *m_rtaNote = nullptr;
    bool          m_showA = false;
    // 角度-波長マップ (上の曲線と同じ計算を角度ごとに並べたもの)
    QPushButton  *m_mapBtn = nullptr;
    class AngleLambdaMap *m_map = nullptr;
    QLabel       *m_mapNote = nullptr;
    bool          m_showMap = false;
    QTableWidget *m_specTable;
    QLabel       *m_specNote;           // 評価波長域・除外点数

    // 最適化設計
    QButtonGroup *m_method;
    QCheckBox    *m_varThickness;
    QCheckBox    *m_varCount;
    QCheckBox    *m_varMaterial;
    QTableWidget *m_targetTable;
    QLabel       *m_meritLabel;

    // 製造・誤差
    QButtonGroup *m_deposition;
    QLineEdit    *m_thickErr;
    QCheckBox    *m_systematic;
    QCheckBox    *m_correlated;
    QLineEdit    *m_correlation = nullptr;   // 相関係数 ρ (0..1)
    QButtonGroup *m_monitoring;
    QPushButton  *m_mcButton;
    QLabel       *m_yieldBadge;
    QLabel       *m_sensitiveLabel;
};

} // namespace ofd
