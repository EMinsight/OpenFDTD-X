// OptimizeTab.h — 最適化タブ (ansys-tabs.jsx OptimizeTab 相当)。
// Ansys Lumerical / optiSLang 風のスイープ + 逆設計:
//   - 手法選択 (スイープ / PSO / 随伴 / GA / ベイズ / トポロジー)
//   - 最適化変数表 (ドメイン別の既定行、スイープ時は総ジョブ数を表示)
//   - 目的関数 FoM と制約条件 (ドメイン別)
//   - 手法別ハイパーパラメータ / 実行先 (ローカル・HPC・tidy3d)
// トポロジー最適化と tidy3d 実行先は光ドメインのみ。状態はローカル保持
// (Project に対応フィールドが無いため apply() での永続化は行わない)。
#pragma once
#include <QScrollArea>
#include <QString>
#include <QVector>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

namespace ofd {

class Project;
class SectionBox;

class OptimizeTab : public QScrollArea {
    Q_OBJECT
public:
    explicit OptimizeTab(Project *project, QWidget *parent = nullptr);

private slots:
    void rebuildDomain();     // ドメイン変更 → 変数表・FoM・制約・実行先
    void updateMode();        // 手法変更 → ヒント文・ハイパーパラメータ表示

private:
    void setMode(const QString &mode);

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

    // 実行 / Run
    QComboBox *m_target;                 // ローカル / HPC / tidy3d
    QCheckBox *m_pareto;                 // Paretoフロント出力 (多目的 FoM)
};

} // namespace ofd
