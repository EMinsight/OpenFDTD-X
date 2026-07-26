// GeneralTab.h — solver / ABC / PBC / frequency settings (全般タブ).
// Maps 1:1 to the .ofd keys: title, solver, abc, pbc, frequency1/2,
// timestep, pulsewidth, rfeed, plot3dgeom.
#pragma once
#include <QScrollArea>

class QLineEdit;
class QSpinBox;
class QComboBox;
class QCheckBox;
class QDoubleSpinBox;
class QWidget;

namespace ofd {

class Project;
class SectionBox;

class GeneralTab : public QScrollArea {
    Q_OBJECT
public:
    explicit GeneralTab(Project *project, QWidget *parent = nullptr);

private slots:
    void refresh();   // model → widgets

private:
    // mock (tabs.jsx GeneralTab) の {state.abc === "pml" && …} 相当:
    // ABC が PML のときだけ PML 詳細行を見せる。
    void updateAbcView();

    Project *m_p;
    bool m_updating = false;

    QLineEdit *m_title;
    QSpinBox  *m_maxiter;
    QSpinBox  *m_nout;
    QLineEdit *m_converg;
    SectionBox *m_abcSection;        // PML 行の表示切替に使う
    QComboBox *m_abc;
    QSpinBox  *m_pmlL;
    QDoubleSpinBox *m_pmlM;
    QLineEdit *m_pmlR0;
    QDoubleSpinBox *m_pmlSigma;      // σ_max スケール (ローカル状態)
    QWidget   *m_pmlSigmaRow;        // σ_max + 単位 (行ごと隠すための入れ物)
    QCheckBox *m_pbc[3];
    QLineEdit *m_f1min, *m_f1max; QSpinBox *m_f1div;
    QLineEdit *m_f2min, *m_f2max; QSpinBox *m_f2div;
    QLineEdit *m_dt, *m_tw, *m_rfeed;
    QCheckBox *m_plot3dgeom;
    // 計算条件オプション (mock g_opt) — ローカル状態
    QCheckBox *m_optMatch, *m_optPol, *m_optIterSkip;
};

} // namespace ofd
