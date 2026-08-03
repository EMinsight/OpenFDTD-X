// ScatteringTab.cpp
#include "ScatteringTab.h"
#include "../core/Project.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"
#include "TabHelpers.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

using namespace ofd;

// ── タブ専用語彙 (file-local 登録, 接頭辞 sct_) ─────────────────────────────
namespace {
const bool s_i18n = [] {
    using ofd::I18n;
    I18n::reg("sct_title", "散乱特性 (OpenFDTD §2.15)", "Scattering (OpenFDTD §2.15)");
    I18n::reg("sct_hint",
              "平面波入射に対する散乱体の解析。RCS、シールド効果、透過率など。",
              "Analysis of a scatterer under plane-wave incidence: RCS, shielding "
              "effectiveness, transmittance and more.");

    // 入射波 / Incident wave
    I18n::reg("sct_inc", "入射波", "Incident wave");
    I18n::reg("sct_inc_dir", "入射方向 (θ, φ)", "Incidence direction (θ, φ)");
    I18n::reg("sct_pol", "偏波", "Polarization");
    I18n::reg("sct_pol_v", "V (TE)", "V (TE)");
    I18n::reg("sct_pol_h", "H (TM)", "H (TM)");
    I18n::reg("sct_pol_cp", "円偏波", "Circular");
    I18n::reg("sct_sweep", "入射角スイープ (バイスタティック)",
              "Incidence-angle sweep (bistatic)");
    I18n::reg("sct_sweep_range", "スイープ範囲", "Sweep range");
    I18n::reg("sct_points", "点", "points");

    // RCS
    I18n::reg("sct_rcs", "RCS / レーダ断面積", "RCS / Radar cross-section");
    I18n::reg("sct_rcs_mono", "モノスタティックRCS σ(θ_inc=θ_obs)",
              "Monostatic RCS σ(θ_inc = θ_obs)");
    I18n::reg("sct_rcs_bi", "バイスタティックRCS σ(θ_inc, θ_obs)",
              "Bistatic RCS σ(θ_inc, θ_obs)");
    I18n::reg("sct_unit", "単位", "Unit");
    I18n::reg("sct_rcs_matrix", "偏波散乱行列 [HH, HV, VH, VV]",
              "Polarimetric scattering matrix [HH, HV, VH, VV]");

    // NTFF
    I18n::reg("sct_ntff", "近傍/遠方界変換 (NTFF)", "Near-to-far-field transform (NTFF)");
    I18n::reg("sct_ntff_extract", "散乱波抽出 (入射波除去)",
              "Scattered-field extraction (incident field removed)");
    I18n::reg("sct_ntff_surface", "変換面", "Transform surface");
    I18n::reg("sct_ntff_box", "直方体閉曲面", "Closed box surface");
    I18n::reg("sct_ntff_sphere", "球面", "Spherical surface");
    I18n::reg("sct_ntff_wide", "広帯域RCS (FFT)", "Wideband RCS (FFT)");

    // その他散乱量
    I18n::reg("sct_misc", "その他散乱量", "Other scattering quantities");
    I18n::reg("sct_m_se", "シールド効果 SE [dB]", "Shielding effectiveness SE [dB]");
    I18n::reg("sct_m_fss", "透過率 / 反射率 (FSS用)",
              "Transmittance / reflectance (for FSS)");
    I18n::reg("sct_m_abs", "吸収断面積 σ_abs", "Absorption cross-section σ_abs");
    I18n::reg("sct_m_ext", "消散断面積 σ_ext (光散乱)",
              "Extinction cross-section σ_ext (optical scattering)");
    I18n::reg("sct_m_mie", "効率係数 Q_sca, Q_abs (Mie)",
              "Efficiency factors Q_sca, Q_abs (Mie)");
    return true;
}();

const char *const kMiscKeys[] = { "sct_m_se", "sct_m_fss", "sct_m_abs",
                                  "sct_m_ext", "sct_m_mie" };
const bool kMiscOn[] = { false, false, false, false, false };

QLineEdit *numEdit(const char *value, int width, QWidget *parent)
{
    auto *e = new QLineEdit(QString::fromUtf8(value), parent);
    if (width > 0) e->setMaximumWidth(width);
    return e;
}
} // namespace

ScatteringTab::ScatteringTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // ── 散乱特性 (説明) ────────────────────────────────────────────────────
    auto *sTop = new SectionBox(I18n::tr("sct_title"), body);
    auto *hint = new QLabel(I18n::tr("sct_hint"), sTop);
    hint->setWordWrap(true);
    sTop->vbox()->addWidget(hint);
    v->addWidget(sTop);

    // ── 入射波 / Incident wave ─────────────────────────────────────────────
    auto *sInc = new SectionBox(I18n::tr("sct_inc"), body);
    auto *dirRow = new QHBoxLayout();
    m_theta = numEdit("90", 70, sInc);
    m_phi   = numEdit("0", 70, sInc);
    dirRow->addWidget(m_theta);
    dirRow->addWidget(m_phi);
    dirRow->addWidget(new QLabel("°", sInc));
    dirRow->addStretch(1);
    sInc->form()->addRow(I18n::tr("sct_inc_dir"), dirRow);

    m_pol = new QComboBox(sInc);
    m_pol->addItem(I18n::tr("sct_pol_v"));
    m_pol->addItem(I18n::tr("sct_pol_h"));
    m_pol->addItem(I18n::tr("sct_pol_cp"));
    m_pol->setCurrentIndex(1);                   // 既定 "hh"
    sInc->form()->addRow(I18n::tr("sct_pol"), m_pol);

    m_sweep = new QCheckBox(I18n::tr("sct_sweep"), sInc);
    sInc->form()->addRow(m_sweep);

    auto *swRow = new QHBoxLayout();
    m_sweepFrom = numEdit("0", 70, sInc);
    m_sweepTo   = numEdit("180", 70, sInc);
    m_sweepPts  = numEdit("37", 70, sInc);
    swRow->addWidget(m_sweepFrom);
    swRow->addWidget(new QLabel("〜", sInc));
    swRow->addWidget(m_sweepTo);
    swRow->addWidget(new QLabel("°  (", sInc));
    swRow->addWidget(m_sweepPts);
    swRow->addWidget(new QLabel(I18n::tr("sct_points") + QStringLiteral(")"), sInc));
    swRow->addStretch(1);
    sInc->form()->addRow(I18n::tr("sct_sweep_range"), swRow);
    // このタブの入力はまだ Project / .ofd へ配線されていない (絶対規則 5)
    sInc->form()->addRow(tabhelp::unwiredNote(sInc));
    v->addWidget(sInc);

    // ── RCS ────────────────────────────────────────────────────────────────
    auto *sRcs = new SectionBox(I18n::tr("sct_rcs"), body);
    m_rcsMono = new QCheckBox(I18n::tr("sct_rcs_mono"), sRcs);
    m_rcsMono->setChecked(true);
    sRcs->form()->addRow(m_rcsMono);
    m_rcsBi = new QCheckBox(I18n::tr("sct_rcs_bi"), sRcs);
    sRcs->form()->addRow(m_rcsBi);
    m_rcsUnit = new QComboBox(sRcs);
    m_rcsUnit->addItems({ "m²", "dBsm", "σ/λ²" });
    m_rcsUnit->setCurrentIndex(1);                // 既定 "dbsm"
    sRcs->form()->addRow(I18n::tr("sct_unit"), m_rcsUnit);
    m_rcsMatrix = new QCheckBox(I18n::tr("sct_rcs_matrix"), sRcs);
    sRcs->form()->addRow(m_rcsMatrix);
    sRcs->form()->addRow(tabhelp::unwiredNote(sRcs));
    v->addWidget(sRcs);

    // ── 近傍/遠方界変換 / NTFF ─────────────────────────────────────────────
    auto *sNtff = new SectionBox(I18n::tr("sct_ntff"), body);
    m_ntffExtract = new QCheckBox(I18n::tr("sct_ntff_extract"), sNtff);
    m_ntffExtract->setChecked(true);
    sNtff->form()->addRow(m_ntffExtract);
    m_ntffSurface = new QComboBox(sNtff);
    m_ntffSurface->addItem(I18n::tr("sct_ntff_box"));
    m_ntffSurface->addItem(I18n::tr("sct_ntff_sphere"));
    m_ntffSurface->setCurrentIndex(0);            // 既定 "box"
    sNtff->form()->addRow(I18n::tr("sct_ntff_surface"), m_ntffSurface);
    m_ntffWide = new QCheckBox(I18n::tr("sct_ntff_wide"), sNtff);
    sNtff->form()->addRow(m_ntffWide);
    sNtff->form()->addRow(tabhelp::unwiredNote(sNtff));
    v->addWidget(sNtff);

    // ── その他散乱量 ──────────────────────────────────────────────────────
    auto *sMisc = checkSection(body, "sct_misc", kMiscKeys, kMiscOn,
                               int(sizeof(kMiscKeys) / sizeof(kMiscKeys[0])),
                               &m_misc);
    sMisc->vbox()->addWidget(tabhelp::unwiredNote(sMisc));
    v->addWidget(sMisc);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    // スイープ範囲はスイープ ON のときだけ編集可 (モックのラベル順を維持)
    auto syncSweep = [this] {
        const bool on = m_sweep->isChecked();
        m_sweepFrom->setEnabled(on);
        m_sweepTo->setEnabled(on);
        m_sweepPts->setEnabled(on);
    };
    connect(m_sweep, &QCheckBox::toggled, this, syncSweep);
    syncSweep();
}

SectionBox *ScatteringTab::checkSection(QWidget *parent, const char *titleKey,
                                        const char *const *keys, const bool *checked,
                                        int n, QVector<QCheckBox *> *out)
{
    auto *s = new SectionBox(I18n::tr(titleKey), parent);
    for (int i = 0; i < n; ++i) {
        auto *ck = new QCheckBox(I18n::tr(keys[i]), s);
        ck->setChecked(checked[i]);
        s->vbox()->addWidget(ck);
        out->push_back(ck);
    }
    return s;
}
