// ToleranceTab.h — 製造ばらつき・歩留まり解析タブ (optics-tabs.jsx ToleranceTab 相当)。
// Zemax Tolerance + Lumerical yield analysis 相当:
//   - ばらつき要因表 (ドメイン別: 分布・中心・σ/半幅・単位。中心と σ は編集可)
//   - モンテカルロ設定 (サンプル数 / サンプリング法)
//   - 合格条件 (ドメイン別の目標量と閾値)
//   - 結果 (入力変数の解析分布 / 被覆区間)
//
// 表示の区分 (捏造値を置かないための原則):
//   - 入力変数そのものの分布 (密度曲線・3σ 相当区間・標準偏差) は
//     core/ToleranceStats による実計算で、表の編集がそのまま反映される。
//   - 性能 (FoM) の分布・歩留まりは各サンプルでソルバーを回さないと出せない。
//     モンテカルロは未実装なので「未計算」と明示する (数値を出さない)。
//
// ばらつき要因は Project に対応フィールドが無いためタブ内のローカル状態
// (保存されない)。.ofdx への保存は未実装であることを画面に明示する。
#pragma once
#include <QScrollArea>
#include <QString>
#include <QVector>

#include "../core/ToleranceStats.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QTableWidget;
class QTableWidgetItem;

namespace ofd {

class Project;
class MiniPlot;
class SectionBox;

class ToleranceTab : public QScrollArea {
    Q_OBJECT
public:
    explicit ToleranceTab(Project *project, QWidget *parent = nullptr);

private slots:
    void rebuildDomain();          // ドメイン変更 → 要因表・合格条件を作り直す
    void onSourceEdited(QTableWidgetItem *item);   // 中心 / σ の編集を取り込む
    void updateDistribution();     // 選択中の変数の分布を計算して描画する

private:
    // ばらつき要因 1 行 (ローカル状態)。単位系は unit 列の表示に従う。
    struct VarRow {
        bool    enabled = true;
        QString name;
        QString unit;
        tolstat::Variable var;     // 分布 / 中心 / σ・半幅
    };

    void fillSourceTable();        // m_vars → 表
    void refreshVarChoices();      // 有効かつ連続な変数を選択コンボへ

    Project      *m_p;
    bool          m_updating = false;   // 表の再構築中は編集シグナルを無視する

    SectionBox   *m_titleSec;
    QTableWidget *m_sources;      // ばらつき要因
    QVector<VarRow> m_vars;       // 表の実体 (ドメイン別の既定値から作る)

    QLineEdit    *m_samples;      // モンテカルロ サンプル数
    QComboBox    *m_sampling;     // ランダム / LHS / Sobol

    QLabel       *m_goal;         // 合格条件: 目標量
    QLineEdit    *m_goalVal;
    QLabel       *m_goalUnit, *m_goalAt;

    QComboBox    *m_varBox;       // 分布を表示する変数
    QLabel       *m_yield;        // 歩留まり (未計算であることの表示)
    QLabel       *m_sigma3;       // 3σ 相当の被覆区間 (実計算)
    MiniPlot     *m_hist;         // 入力変数の確率密度
};

} // namespace ofd
