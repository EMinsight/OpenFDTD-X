// GeneralTab.cpp
#include "GeneralTab.h"
#include "../core/Project.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QDoubleValidator>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>

using namespace ofd;

// mock (tabs.jsx GeneralTab) 側にしか無かった文言を file-local に登録する。
// 接頭辞は本タブ専用の g_ (既存キーは I18n.cpp が優先される)。
namespace {
const bool s_i18nGeneral = [] {
    I18n::reg("g_pml_sigma", "σ_max", "σ_max");
    I18n::reg("g_pml_sigma_unit", "×σopt", "×σopt");
    I18n::reg("g_far_warn",
              "⚠ 近傍界の周波数分割が大きいと計算時間とメモリーが比例増加します",
              "⚠ A large near-field frequency division increases run time and "
              "memory proportionally");
    I18n::reg("g_opt", "計算条件オプション", "Run options");
    I18n::reg("g_opt_match", "整合損を含む", "Include matching loss");
    I18n::reg("g_opt_pol", "偏波回転", "Polarization rotation");
    I18n::reg("g_opt_iter_skip", "イテレーション飛ばし", "Iteration skip");
    return true;
}();
}

// scientific-notation line edit for [Hz] / [s] quantities
static QLineEdit *sciEdit(QWidget *parent)
{
    auto *e = new QLineEdit(parent);
    auto *v = new QDoubleValidator(e);
    v->setNotation(QDoubleValidator::ScientificNotation);
    e->setValidator(v);
    e->setMaximumWidth(120);
    return e;
}

GeneralTab::GeneralTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // title
    auto *sTitle = new SectionBox(I18n::tr("g_title"), body);
    m_title = new QLineEdit(sTitle);
    sTitle->form()->addRow(I18n::tr("g_title"), m_title);
    v->addWidget(sTitle);

    // solver
    auto *sSolver = new SectionBox(I18n::tr("g_solver"), body);
    m_maxiter = new QSpinBox(sSolver);
    m_maxiter->setRange(1, 100000000);
    m_nout = new QSpinBox(sSolver);
    m_nout->setRange(1, 1000000);
    m_converg = sciEdit(sSolver);
    sSolver->form()->addRow(I18n::tr("g_max_iter"), m_maxiter);
    sSolver->form()->addRow(I18n::tr("g_nout"), m_nout);
    sSolver->form()->addRow(I18n::tr("g_conv_tol"), m_converg);
    v->addWidget(sSolver);

    // ABC
    auto *sAbc = new SectionBox(I18n::tr("g_abc"), body);
    m_abcSection = sAbc;
    m_abc = new QComboBox(sAbc);
    m_abc->addItem(I18n::tr("g_mur1"));   // abc = 0
    m_abc->addItem(I18n::tr("g_pml"));    // abc = 1 L m R0
    m_pmlL = new QSpinBox(sAbc);
    m_pmlL->setRange(1, 64);
    m_pmlM = new QDoubleSpinBox(sAbc);
    m_pmlM->setRange(0.1, 10.0);
    m_pmlM->setSingleStep(0.5);
    m_pmlR0 = sciEdit(sAbc);
    sAbc->form()->addRow(I18n::tr("g_abc"), m_abc);
    sAbc->form()->addRow(I18n::tr("g_pml_layers"), m_pmlL);
    sAbc->form()->addRow(I18n::tr("g_pml_order"), m_pmlM);
    sAbc->form()->addRow(I18n::tr("g_pml_r0"), m_pmlR0);
    // σ_max スケール (mock の g_pml_sigma) — .ofd には無い量なのでローカル状態。
    m_pmlSigma = new QDoubleSpinBox(sAbc);
    m_pmlSigma->setRange(0.1, 10.0);
    m_pmlSigma->setDecimals(3);
    m_pmlSigma->setSingleStep(0.1);
    m_pmlSigma->setValue(1.5);
    m_pmlSigmaRow = new QWidget(sAbc);
    auto *sigRow = new QHBoxLayout(m_pmlSigmaRow);
    sigRow->setContentsMargins(0, 0, 0, 0);
    sigRow->addWidget(m_pmlSigma);
    sigRow->addWidget(new QLabel(I18n::tr("g_pml_sigma_unit"), m_pmlSigmaRow));
    sigRow->addStretch(1);
    sAbc->form()->addRow(I18n::tr("g_pml_sigma"), m_pmlSigmaRow);
    v->addWidget(sAbc);

    // PBC
    auto *sPbc = new SectionBox(I18n::tr("g_periodic"), body);
    auto *pbcRow = new QHBoxLayout();
    static const char *axisName[3] = { "X", "Y", "Z" };
    for (int a = 0; a < 3; ++a) {
        m_pbc[a] = new QCheckBox(axisName[a], sPbc);
        pbcRow->addWidget(m_pbc[a]);
    }
    pbcRow->addStretch(1);
    sPbc->vbox()->addLayout(pbcRow);
    v->addWidget(sPbc);

    // frequency1 / frequency2
    auto addFreqSection = [&](const QString &title, QLineEdit *&fmin,
                              QLineEdit *&fmax, QSpinBox *&fdiv) {
        auto *s = new SectionBox(title, body);
        fmin = sciEdit(s);
        fmax = sciEdit(s);
        fdiv = new QSpinBox(s);
        fdiv->setRange(0, 100000);
        auto *row = new QHBoxLayout();
        row->addWidget(new QLabel(I18n::tr("g_freq_min"), s));
        row->addWidget(fmin);
        row->addWidget(new QLabel(I18n::tr("g_freq_max"), s));
        row->addWidget(fmax);
        row->addWidget(new QLabel(I18n::tr("g_freq_div"), s));
        row->addWidget(fdiv);
        row->addStretch(1);
        s->vbox()->addLayout(row);
        v->addWidget(s);
        return s;
    };
    addFreqSection(I18n::tr("g_freq1"), m_f1min, m_f1max, m_f1div);
    auto *sFar = addFreqSection(I18n::tr("g_freq2"), m_f2min, m_f2max, m_f2div);
    // 遠方界/近傍界の分割数に対する注意 (mock の warn 行)
    auto *farWarn = new QLabel(I18n::tr("g_far_warn"), sFar);
    farWarn->setWordWrap(true);
    sFar->vbox()->addWidget(farWarn);

    // advanced
    auto *sAdv = new SectionBox(I18n::tr("g_advanced"), body);
    m_dt = sciEdit(sAdv);
    m_tw = sciEdit(sAdv);
    m_rfeed = sciEdit(sAdv);
    m_plot3dgeom = new QCheckBox(sAdv);
    sAdv->form()->addRow(I18n::tr("g_timestep"), m_dt);
    sAdv->form()->addRow(I18n::tr("g_pulsewidth"), m_tw);
    sAdv->form()->addRow(I18n::tr("g_rfeed"), m_rfeed);
    sAdv->form()->addRow(I18n::tr("g_plot3dgeom"), m_plot3dgeom);
    v->addWidget(sAdv);

    // 計算条件オプション (mock の g_opt) — 本家 .ofd には無いのでローカル状態。
    auto *sOpt = new SectionBox(I18n::tr("g_opt"), body);
    m_optMatch     = new QCheckBox(I18n::tr("g_opt_match"), sOpt);
    m_optPol       = new QCheckBox(I18n::tr("g_opt_pol"), sOpt);
    m_optIterSkip  = new QCheckBox(I18n::tr("g_opt_iter_skip"), sOpt);
    for (auto *c : { m_optMatch, m_optPol, m_optIterSkip })
        sOpt->vbox()->addWidget(c);
    v->addWidget(sOpt);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    // widgets → model
    auto apply = [this] {
        if (m_updating) return;
        GeneralOpts &g = m_p->general();
        g.title   = m_title->text();
        g.maxiter = m_maxiter->value();
        g.nout    = m_nout->value();
        g.converg = m_converg->text().toDouble();
        g.abc     = m_abc->currentIndex();
        g.pmlL    = m_pmlL->value();
        g.pmlM    = m_pmlM->value();
        g.pmlR0   = m_pmlR0->text().toDouble();
        g.pbcX = m_pbc[0]->isChecked();
        g.pbcY = m_pbc[1]->isChecked();
        g.pbcZ = m_pbc[2]->isChecked();
        g.f1min = m_f1min->text().toDouble();
        g.f1max = m_f1max->text().toDouble();
        g.f1div = m_f1div->value();
        g.f2min = m_f2min->text().toDouble();
        g.f2max = m_f2max->text().toDouble();
        g.f2div = m_f2div->value();
        g.dt = m_dt->text().toDouble();
        g.tw = m_tw->text().toDouble();
        g.rfeed = m_rfeed->text().toDouble();
        g.plot3dgeom = m_plot3dgeom->isChecked() ? 1 : 0;
        m_p->touch();
    };
    for (auto *e : { m_title, m_converg, m_pmlR0, m_f1min, m_f1max,
                     m_f2min, m_f2max, m_dt, m_tw, m_rfeed })
        connect(e, &QLineEdit::editingFinished, this, apply);
    for (auto *s : { m_maxiter, m_nout, m_pmlL, m_f1div, m_f2div })
        connect(s, &QSpinBox::valueChanged, this, apply);
    connect(m_pmlM, &QDoubleSpinBox::valueChanged, this, apply);
    connect(m_abc, &QComboBox::currentIndexChanged, this, apply);
    for (auto *c : { m_pbc[0], m_pbc[1], m_pbc[2], m_plot3dgeom })
        connect(c, &QCheckBox::toggled, this, apply);

    // ABC 種別 → PML 詳細行の表示 (mock の条件付きレンダリング)
    connect(m_abc, &QComboBox::currentIndexChanged,
            this, &GeneralTab::updateAbcView);

    connect(project, &Project::loaded, this, &GeneralTab::refresh);
    refresh();
}

// combo index: 0 = Mur 1次 (.ofd abc=0), 1 = PML (.ofd abc=1)。
// PML 以外では層数 / 次数 / R0 / σ_max の行をラベルごと隠す。
void GeneralTab::updateAbcView()
{
    const bool pml = (m_abc->currentIndex() == 1);
    QFormLayout *f = m_abcSection->form();
    QWidget *rows[4] = { m_pmlL, m_pmlM, m_pmlR0, m_pmlSigmaRow };
    for (QWidget *w : rows) {
        w->setVisible(pml);
        if (QWidget *lab = f->labelForField(w)) lab->setVisible(pml);
    }
}

void GeneralTab::refresh()
{
    m_updating = true;
    const GeneralOpts &g = m_p->general();
    m_title->setText(g.title);
    m_maxiter->setValue(g.maxiter);
    m_nout->setValue(g.nout);
    m_converg->setText(QString::number(g.converg, 'g', 6));
    m_abc->setCurrentIndex(g.abc);
    m_pmlL->setValue(g.pmlL);
    m_pmlM->setValue(g.pmlM);
    m_pmlR0->setText(QString::number(g.pmlR0, 'g', 6));
    m_pbc[0]->setChecked(g.pbcX);
    m_pbc[1]->setChecked(g.pbcY);
    m_pbc[2]->setChecked(g.pbcZ);
    m_f1min->setText(QString::number(g.f1min, 'g', 10));
    m_f1max->setText(QString::number(g.f1max, 'g', 10));
    m_f1div->setValue(g.f1div);
    m_f2min->setText(QString::number(g.f2min, 'g', 10));
    m_f2max->setText(QString::number(g.f2max, 'g', 10));
    m_f2div->setValue(g.f2div);
    m_dt->setText(QString::number(g.dt, 'g', 6));
    m_tw->setText(QString::number(g.tw, 'g', 6));
    m_rfeed->setText(QString::number(g.rfeed, 'g', 6));
    m_plot3dgeom->setChecked(g.plot3dgeom != 0);
    m_updating = false;
    updateAbcView();
}
