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
class QLabel;
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

    // ドメイン別の表示切替: 音響 (RIR 解析) / 水中音響 (BELLHOP) では
    // 波動 FDTD/BPM 固有の項目 (ABC / PBC / 遠方界周波数 / Δt / Tw /
    // 整合損 / 偏波回転) を隠し、給電抵抗 rfeed は EM のみ表示する
    // (BPM では rfeed は無効キーワード)。
    // 表示のみの切替であり、apply()/refresh() のモデル入出力は分岐しない。
    void updateDomainVisibility();

    Project *m_p;
    bool m_updating = false;

    QLineEdit *m_title;
    QSpinBox  *m_maxiter;
    QSpinBox  *m_nout;
    QLineEdit *m_converg;
    SectionBox *m_abcSection;        // PML 行の表示切替に使う
    QComboBox *m_abc;
    // mock の Seg にある「Mur 2次」。.ofd の abc は 0/1 のみなので UI 専用の
    // ローカル状態として持ち、保存時は Mur 1次 (abc=0) に落とす。
    bool       m_mur2 = false;
    QLabel    *m_mur2Note = nullptr;  // 保存されない旨の注記
    QSpinBox  *m_pmlL;
    QDoubleSpinBox *m_pmlM;
    QLineEdit *m_pmlR0;
    QDoubleSpinBox *m_pmlSigma;      // σ_max スケール (ローカル状態)
    QWidget   *m_pmlSigmaRow;        // σ_max + 単位 (行ごと隠すための入れ物)
    QCheckBox *m_pbc[3];
    SectionBox *m_pbcSection;        // ドメイン別表示切替に使う
    QLineEdit *m_f1min, *m_f1max; QSpinBox *m_f1div;
    QLineEdit *m_f2min, *m_f2max; QSpinBox *m_f2div;
    SectionBox *m_farSection;        // 解析周波数2 (遠方界) — g_far_warn 含む
    SectionBox *m_advSection;        // 詳細設定 (Δt/Tw/rfeed の行切替に使う)
    QLineEdit *m_dt, *m_tw, *m_rfeed;
    QCheckBox *m_plot3dgeom;
    // 計算条件オプション (mock g_opt) — ローカル状態
    QCheckBox *m_optMatch, *m_optPol, *m_optIterSkip;
};

} // namespace ofd
