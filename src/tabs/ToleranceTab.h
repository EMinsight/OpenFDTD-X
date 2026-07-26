// ToleranceTab.h — 製造ばらつき・歩留まり解析タブ (optics-tabs.jsx ToleranceTab 相当)。
// Zemax Tolerance + Lumerical yield analysis 相当:
//   - ばらつき要因表 (ドメイン別: 分布・σ・単位)
//   - モンテカルロ設定 (サンプル数 / サンプリング法)
//   - 合格条件 (ドメイン別の目標量と閾値)
//   - 結果 (歩留まり・3σ レンジ・性能分布 MiniPlot)
// 表示専用 (Project に対応フィールドが無いためローカル状態のみ)。
#pragma once
#include <QScrollArea>

class QComboBox;
class QLabel;
class QLineEdit;
class QTableWidget;

namespace ofd {

class Project;
class MiniPlot;
class SectionBox;

class ToleranceTab : public QScrollArea {
    Q_OBJECT
public:
    explicit ToleranceTab(Project *project, QWidget *parent = nullptr);

private slots:
    void rebuildDomain();     // ドメイン変更 → 要因表・合格条件・結果表示

private:
    Project      *m_p;

    SectionBox   *m_titleSec;
    QTableWidget *m_sources;      // ばらつき要因

    QLineEdit    *m_samples;      // モンテカルロ サンプル数
    QComboBox    *m_sampling;     // ランダム / LHS / Sobol

    QLabel       *m_goal;         // 合格条件: 目標量
    QLineEdit    *m_goalVal;
    QLabel       *m_goalUnit, *m_goalAt;

    QLabel       *m_yield;        // 歩留まりバッジ
    QLabel       *m_sigma3;       // 3σ range
    MiniPlot     *m_hist;         // 性能分布
};

} // namespace ofd
