// OptimizeTab.h — 最適化タブ (ansys-tabs.jsx OptimizeTab 相当)。
// Ansys Lumerical / optiSLang 風のスイープ + 逆設計:
//   - 手法選択 (スイープ / PSO / 随伴 / GA / ベイズ / トポロジー)
//   - 最適化変数表 (ドメイン別の既定行、スイープ時は総ジョブ数を表示)
//   - 目的関数 FoM と制約条件 (ドメイン別)
//   - 手法別ハイパーパラメータ / 実行先 (ローカル・HPC・tidy3d)
// トポロジー最適化と tidy3d 実行先は光ドメインのみ。状態はローカル保持
// (Project に対応フィールドが無いため apply() での永続化は行わない)。
// トポロジー節だけは core/DensityField へ配線してあり、設計領域・解像度・
// フィルタ半径・射影から密度場を作って図と数値を出し、「密度場を形状へ変換」
// で Project の形状ユニットを書き換える (結果は通常の geometry として残る)。
#pragma once
#include <QScrollArea>

#include "../core/Optimizer.h"
#include "../kernel/OptimizeFom.h"
#include <QString>
#include <QVector>
#include <memory>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QTableWidget;

namespace ofd {

class FieldHeatmap;
class Project;
class SectionBox;

class OptimizeTab : public QScrollArea {
    Q_OBJECT
public:
    explicit OptimizeTab(Project *project, QWidget *parent = nullptr);

private slots:
    void rebuildDomain();     // ドメイン変更 → 変数表・FoM・制約・実行先
    void updateMode();        // 手法変更 → ヒント文・ハイパーパラメータ表示
    // 「掃引」の実行 / 中止。実行中に押すと中止する (ScatteringTab と同じ作法)
    void startSweep();
    void onPointFinished(int index, const SweepResult &r);
    void onSweepFinished(bool ok);
    void updateRunUi();       // 手法・実行状態 → ボタンと注記
    // トポロジー: 設計領域・解像度・フィルタ・射影 → 画素格子と密度場の再計算
    void updateTopology();
    void applyTopology();     // 射影後の密度場 → 直方体ユニット

private:
    void setMode(const QString &mode);

    // ── 最適化ループ (PSO / GA) ────────────────────────────────────────────
    // 掃引が「決めた点を順に回す」のに対し、こちらは 1 世代ぶんを回してから
    // 次の世代を決める。SweepRunner の samples (複数パラメータ同時) を
    // 1 世代 = 1 回の start() として使う。
    void startOptimize();
    bool runGeneration();          // 現世代を投入する (false = 開始できない)
    void finishOptimize(bool ok);
    // 変数表のチェック行 → 設計変数 (対象量の列で実在の量に結び付ける)。
    // 1 行も取れなければ false。
    bool collectOptVars(QVector<SweepColumn> *cols,
                        std::vector<optim::Variable> *vars) const;

    Project   *m_p;
    QString    m_mode = "sweep";       // sweep|pso|adjoint|ga|bayes|topology

    // 手法 / Method
    QVector<QPushButton*> m_methodBtns;  // property("mode") で識別
    QPushButton *m_topologyBtn;          // 光ドメインのみ表示
    QLabel      *m_methodHint;

    // パラメータ / Parameters
    QTableWidget *m_params;
    QLabel       *m_jobs;                // 総ジョブ数 (スイープ時のみ)

    // 目的関数 / FoM
    QLineEdit *m_fom;
    QCheckBox *m_cRuleOpt, *m_cSizeEm, *m_cThickAc, *m_cSym;

    // ハイパーパラメータ / Hyper-parameters
    SectionBox *m_hyperSec;
    QWidget    *m_pagePop;               // PSO / GA
    QWidget    *m_pageAdjoint;           // 随伴
    QWidget    *m_pageTopology;          // トポロジー (光)
    QWidget    *m_adjointWarnRow;        // 光以外での注意バッジ
    QLabel     *m_adjointWarn;
    QLineEdit  *m_pop, *m_iters, *m_lr, *m_res, *m_filter;
    // トポロジー: 設計領域 (原点 μm/μm/nm・大きさ μm/μm/nm)・射影・材料番号
    QLineEdit  *m_topoX0 = nullptr, *m_topoY0 = nullptr, *m_topoZ0 = nullptr;
    QLineEdit  *m_topoW = nullptr,  *m_topoD = nullptr,  *m_topoT = nullptr;
    QLineEdit  *m_topoBeta = nullptr, *m_topoEta = nullptr, *m_topoMat = nullptr;
    QLabel     *m_topoGrid = nullptr, *m_topoFeat = nullptr, *m_topoFill = nullptr;
    QLabel     *m_topoWarn = nullptr;
    FieldHeatmap *m_topoMap = nullptr;
    QPushButton  *m_topoApply = nullptr;

    // 実行 / Run
    QComboBox *m_target;                 // ローカル / HPC / tidy3d
    QCheckBox *m_pareto;                 // Paretoフロント出力 (多目的 FoM)

    // ── 掃引の実行 (kernel/SweepRunner + kernel/OptimizeFom) ───────────────
    // 「掃引」手法だけが実際にカーネルを回す。他の手法 (PSO / 随伴 / GA /
    // ベイズ / トポロジー) は最適化ループが無いので従来どおり未実装。
    QComboBox    *m_sweepVar = nullptr;   // 何を振るか (SweepKind)
    QComboBox    *m_fomKind = nullptr;    // 何で良し悪しを決めるか
    QLineEdit    *m_fomFreq = nullptr;    // 評価周波数 [Hz] (空 = 各点の最良)
    QPushButton  *m_runBtn = nullptr;
    QProgressBar *m_progress = nullptr;
    QLabel       *m_runStatus = nullptr;
    QLabel       *m_bestLabel = nullptr;
    QTableWidget *m_resultTable = nullptr;
    SweepRunner  *m_sweeper = nullptr;
    RunConfig     m_runCfg;
    QVector<FomValue> m_foms;

    // 最適化ループの状態 (m_optimizing = true の間だけ有効)
    std::unique_ptr<optim::Optimizer> m_optimizer;
    bool                 m_optimizing = false;
    bool                 m_optStopped = false;
    QVector<SweepColumn> m_optCols;
    std::vector<double>  m_genFoms;      // 現世代の FoM (無効な点は NaN)
    int                  m_optGens = 0;  // 総世代数
    int                  m_optPop = 0;   // 1 世代の個体数
};

} // namespace ofd
