// GeneralTab.cpp
#include "GeneralTab.h"
#include "../core/Project.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"
#include "TabHelpers.h"

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
    I18n::reg("g_converge", "収束条件", "Convergence");
    I18n::reg("g_pml_sigma", "σ_max", "σ_max");
    I18n::reg("g_pml_sigma_unit", "×σopt", "×σopt");
    I18n::reg("g_pml_sigma_note",
              "(未実装 — .ofd に対応キーが無く、保存・計算に反映されません)",
              "(not implemented — the .ofd format has no matching key, so this "
              "value is neither saved nor applied)");
    I18n::reg("g_mur2", "Mur 2次", "Mur 2nd");
    I18n::reg("g_mur2_note",
              "Mur 2次は .ofd の abc キーに対応する値が無いため "
              "(本家は 0=Mur 1次 / 1=PML のみ)、保存時は Mur 1次 として "
              "書き出されます。",
              "The .ofd abc key has no 2nd-order Mur value (upstream accepts only "
              "0 = Mur 1st and 1 = PML), so this choice is written as Mur 1st.");
    I18n::reg("g_periodic_x", "X方向", "X-direction");
    I18n::reg("g_periodic_y", "Y方向", "Y-direction");
    I18n::reg("g_periodic_z", "Z方向", "Z-direction");
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

    // 収束条件 (mock: <Section title={t("g_converge")}> — 最大反復回数/収束判定値)。
    // 出力間隔 (nout) は .ofd の solver 行の一部なのでここに残す。
    auto *sSolver = new SectionBox(I18n::tr("g_converge"), body);
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
    // mock (tabs.jsx) の Seg には Mur 2次 もあるが、本家 .ofd の abc は
    // 0 (Mur 1次) / 1 (PML) の 2 値だけ (io/OfdIO.cpp)。よってこの項目は
    // UI のみのローカル状態で、保存時は Mur 1次 (abc = 0) として書き出す。
    m_abc->addItem(I18n::tr("g_mur2"));
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
    // apply() でも読まれない未実装値であることを、行内の注記で明示する
    // (Mur 2次 の g_mur2_note と同趣旨。行ごと表示/非表示されるので行内に置く)。
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
    auto *sigNote = new QLabel(I18n::tr("g_pml_sigma_note"), m_pmlSigmaRow);
    sigNote->setStyleSheet("color:#888888; font-size:11px;");
    sigRow->addWidget(sigNote);
    sigRow->addStretch(1);
    sAbc->form()->addRow(I18n::tr("g_pml_sigma"), m_pmlSigmaRow);
    // Mur 2次 を選んだときだけ出す注記 (保存されない旨)
    m_mur2Note = new QLabel(I18n::tr("g_mur2_note"), sAbc);
    m_mur2Note->setWordWrap(true);
    m_mur2Note->setStyleSheet("color:#888888; font-size:11px;");
    sAbc->vbox()->addWidget(m_mur2Note);
    v->addWidget(sAbc);

    // PBC
    auto *sPbc = new SectionBox(I18n::tr("g_periodic"), body);
    m_pbcSection = sPbc;
    auto *pbcRow = new QHBoxLayout();
    static const char *axisKey[3] = { "g_periodic_x", "g_periodic_y",
                                      "g_periodic_z" };
    for (int a = 0; a < 3; ++a) {
        m_pbc[a] = new QCheckBox(I18n::tr(axisKey[a]), sPbc);
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
    m_farSection = sFar;
    // 遠方界/近傍界の分割数に対する注意 (mock の warn 行)
    auto *farWarn = new QLabel(I18n::tr("g_far_warn"), sFar);
    farWarn->setWordWrap(true);
    sFar->vbox()->addWidget(farWarn);

    // advanced
    auto *sAdv = new SectionBox(I18n::tr("g_advanced"), body);
    m_advSection = sAdv;
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
    // 3 チェックとも apply() で読まれない (保存・計算に未反映)
    sOpt->vbox()->addWidget(ofd::tabhelp::unwiredNote(sOpt));
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
        // combo → .ofd abc: 0=Mur 1次, 1=PML, 2=Mur 2次 (format に無いので 0)
        m_mur2    = (m_abc->currentIndex() == 2);
        g.abc     = (m_abc->currentIndex() == 1) ? 1 : 0;
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

    // ドメイン切替 → FDTD/BPM 固有項目の表示切替
    connect(project, &Project::domainChanged, this,
            [this] { updateDomainVisibility(); });

    connect(project, &Project::loaded, this, &GeneralTab::refresh);
    refresh();
    updateDomainVisibility();
}

// combo index: 0 = Mur 1次 (.ofd abc=0), 1 = PML (.ofd abc=1),
//              2 = Mur 2次 (UI のみ / .ofd には無い → 保存時は abc=0)。
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
    m_mur2Note->setVisible(m_abc->currentIndex() == 2);
}

// ドメインに関係のない UI 項目を隠す (ドメイン監査の結果)。
// - 音響 (RIR 解析) / 水中音響 (BELLHOP レイトレース) には Mur/PML (ABC)・
//   PBC・遠方界周波数・Δt/Tw・整合損・偏波回転の概念が無い → 非表示。
// - 給電抵抗 rfeed は EM のみ (BPM では rfeed は無効キーワード、
//   音響の点音源に給電抵抗の概念は無い)。
// あくまで表示のみの切替で、apply() は隠れていても従来どおり全値を書く
// (シリアライズ出力は不変)。
void GeneralTab::updateDomainVisibility()
{
    const Domain d = m_p->activeDomain();
    const bool wave = (d == Domain::EM || d == Domain::Optical); // FDTD/BPM 系
    const bool em   = (d == Domain::EM);

    m_abcSection->setVisible(wave);
    m_pbcSection->setVisible(wave);
    m_farSection->setVisible(wave);

    // 詳細設定: Δt / Tw は波動ソルバのみ、rfeed は EM のみ (行ごと隠す)
    QFormLayout *f = m_advSection->form();
    f->setRowVisible(m_dt, wave);
    f->setRowVisible(m_tw, wave);
    f->setRowVisible(m_rfeed, em);

    // 計算条件オプション: 整合損 / 偏波回転は波動ソルバのみ
    // (イテレーション飛ばしと未実装注記は全ドメイン共通のまま)
    m_optMatch->setVisible(wave);
    m_optPol->setVisible(wave);
}

void GeneralTab::refresh()
{
    m_updating = true;
    const GeneralOpts &g = m_p->general();
    m_title->setText(g.title);
    m_maxiter->setValue(g.maxiter);
    m_nout->setValue(g.nout);
    m_converg->setText(QString::number(g.converg, 'g', 6));
    // abc=0 のときは UI の Mur 次数 (ローカル状態) を保つ
    m_abc->setCurrentIndex(g.abc == 1 ? 1 : (m_mur2 ? 2 : 0));
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
