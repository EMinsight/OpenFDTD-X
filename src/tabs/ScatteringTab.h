// ScatteringTab.h — 散乱特性タブ (openfdtd-family.jsx ScatteringTab 相当)。
// OpenFDTD ドキュメント §2.15「散乱」: 平面波入射に対する散乱体の解析。
//   入射波 (θ, φ, 偏波, 角度スイープ) ・RCS ・近傍/遠方界変換 (NTFF) ・その他散乱量。
// 入射波 (θ/φ/偏波) は Project::planewave() へ配線済み (SourceTab と同一モデル)。
// 入射角スイープは kernel/SweepRunner が同じ入力の θ (φ) 違いを N 回まわす
// (カーネルは 1 実行 1 planewave なので、スイープは GUI 側の役目)。
// 円偏波と RCS/NTFF/その他散乱量はカーネル未対応のためローカル状態のまま
// (未実装表示あり)。
#pragma once
#include <QScrollArea>
#include <QVector>

#include "../kernel/SweepRunner.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QTableWidget;

namespace ofd {

class Project;
class SectionBox;

class ScatteringTab : public QScrollArea {
    Q_OBJECT
public:
    explicit ScatteringTab(Project *project, QWidget *parent = nullptr);

    void apply();     // widgets → model (入射波 θ/φ/偏波 + スイープ設定)
    void refresh();   // model → widgets
    // <kernel>.log の "=== cross section ===" を読んで表に出す
    void refreshRcsResult();

    // 実行設定 (エンジン / スレッド数 / カーネル) は MainWindow が持つので
    // 外から与える。未設定なら CPU 既定で走る。
    void setRunConfig(const RunConfig &cfg) { m_runCfg = cfg; }

signals:
    // スイープの進捗・完了を計算コンソールへ出すために MainWindow が拾う
    void sweepLog(const QString &line);

private:
    void startSweep();
    void exportCsv();
    void updateSweepUi();

    SectionBox *checkSection(QWidget *parent, const char *titleKey,
                             const char *const *keys, const bool *checked, int n,
                             QVector<QCheckBox *> *out);

    Project *m_p;
    bool m_updating = false;    // refresh() 中の apply() 再入ガード

    // 入射波 / Incident wave (θ/φ/偏波は Project::planewave() の View)
    QLineEdit *m_theta, *m_phi;
    QComboBox *m_pol;                       // V(TE) / H(TM) / 円偏波
    QCheckBox *m_sweep;                     // 入射角スイープ (バイスタティック)
    QComboBox *m_sweepAxis = nullptr;       // θ / φ のどちらを振るか
    QLineEdit *m_sweepFrom, *m_sweepTo, *m_sweepPts;

    // スイープ実行
    SweepRunner  *m_sweeper = nullptr;
    RunConfig     m_runCfg;
    QPushButton  *m_sweepRun = nullptr;
    QPushButton  *m_sweepCsv = nullptr;
    QProgressBar *m_sweepProgress = nullptr;
    QLabel       *m_sweepStatus = nullptr;
    QTableWidget *m_sweepTable = nullptr;

    // RCS
    QCheckBox *m_rcsMono, *m_rcsBi, *m_rcsMatrix;
    QLabel       *m_rcsResultNote = nullptr;
    QTableWidget *m_rcsTable = nullptr;
    QComboBox *m_rcsUnit;                   // m² / dBsm / σ/λ²

    // NTFF
    QCheckBox *m_ntffExtract, *m_ntffWide;
    QComboBox *m_ntffSurface;               // 直方体閉曲面 / 球面

    // その他散乱量
    QVector<QCheckBox *> m_misc;
};

} // namespace ofd
