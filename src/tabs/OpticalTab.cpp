// OpticalTab.cpp
#include "OpticalTab.h"
#include "../core/Project.h"
#include "../io/ActivationCurve.h"
#include "../widgets/MiniPlot.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>

using namespace ofd;

OpticalTab::OpticalTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // solver method selection (FDTD / RCWA / BPM / FMM)
    auto *ss = new SectionBox(I18n::tr("opt_solver"), body);
    m_solver = new QComboBox(ss);
    m_solver->addItem(I18n::tr("opt_solver_fdtd"));
    m_solver->addItem(I18n::tr("opt_solver_rcwa"));
    m_solver->addItem(I18n::tr("opt_solver_bpm"));
    m_solver->addItem(I18n::tr("opt_solver_fmm"));
    ss->vbox()->addWidget(m_solver);
    auto *hint = new QLabel(I18n::tr("opt_kernel_hint"), ss);
    hint->setWordWrap(true);
    ss->vbox()->addWidget(hint);

    // per-method parameter pages
    m_solverStack = new QStackedWidget(ss);

    // [0] FDTD — uses the General/Mesh tabs; nothing extra here
    {
        auto *page = new QLabel(I18n::tr("opt_solver_fdtd"), m_solverStack);
        page->setWordWrap(true);
        page->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        m_solverStack->addWidget(page);
    }
    // [1] RCWA
    {
        auto *page = new SectionBox(I18n::tr("opt_rcwa_section"), m_solverStack);
        m_rcwaNx = new QSpinBox(page); m_rcwaNx->setRange(1, 101);
        m_rcwaNy = new QSpinBox(page); m_rcwaNy->setRange(1, 101);
        m_rcwaPx = new QLineEdit(page); m_rcwaPx->setMaximumWidth(100);
        m_rcwaPy = new QLineEdit(page); m_rcwaPy->setMaximumWidth(100);
        m_rcwaLayers = new QSpinBox(page); m_rcwaLayers->setRange(1, 1000);
        page->form()->addRow(I18n::tr("opt_rcwa_orders") + " Nx", m_rcwaNx);
        page->form()->addRow("Ny", m_rcwaNy);
        page->form()->addRow(I18n::tr("opt_rcwa_period") + " Λx", m_rcwaPx);
        page->form()->addRow("Λy", m_rcwaPy);
        page->form()->addRow(I18n::tr("opt_rcwa_layers"), m_rcwaLayers);
        m_solverStack->addWidget(page);
    }
    // [2] BPM
    {
        auto *page = new SectionBox(I18n::tr("opt_bpm_section"), m_solverStack);
        m_bpmAlgo = new QComboBox(page);
        m_bpmAlgo->addItems({ "FFT-BPM", "FDM-BPM", "Wide-Angle Padé(1,1)" });
        m_bpmDz = new QLineEdit(page); m_bpmDz->setMaximumWidth(100);
        m_bpmN0 = new QLineEdit(page); m_bpmN0->setMaximumWidth(100);
        m_bpmInput = new QComboBox(page);
        m_bpmInput->addItems({ "TE₀", "TE₁", "TM₀", "Gaussian" });
        page->form()->addRow(I18n::tr("opt_bpm_algo"), m_bpmAlgo);
        page->form()->addRow(I18n::tr("opt_bpm_dz"), m_bpmDz);
        page->form()->addRow(I18n::tr("opt_bpm_n0"), m_bpmN0);
        page->form()->addRow(I18n::tr("opt_bpm_input"), m_bpmInput);

        // ── 非線形 (TPA) / ONN 活性化 (Opt. Lett. 49, 5811 (2024)) ──
        auto *tpa = new SectionBox(I18n::tr("opt_tpa_section"), page);
        m_tpaEnable = new QCheckBox(I18n::tr("opt_tpa_enable"), tpa);
        m_tpaEnable->setToolTip(I18n::tr("opt_tpa_tip"));
        m_tpaMatId = new QSpinBox(tpa);
        m_tpaMatId->setRange(0, 255);
        m_tpaMatId->setToolTip(I18n::tr("opt_tpa_mat_tip"));
        m_tpaBeta = new QLineEdit(tpa);
        m_tpaBeta->setMaximumWidth(100);
        m_tpaBeta->setToolTip(I18n::tr("opt_tpa_beta_tip"));
        m_psEnable = new QCheckBox(I18n::tr("opt_ps_enable"), tpa);
        m_psEnable->setToolTip(I18n::tr("opt_ps_tip"));
        m_psPmin = new QLineEdit(tpa); m_psPmin->setMaximumWidth(100);
        m_psPmax = new QLineEdit(tpa); m_psPmax->setMaximumWidth(100);
        m_psPoints = new QSpinBox(tpa);
        m_psPoints->setRange(1, 100000);
        m_psScale = new QComboBox(tpa);
        m_psScale->addItem(I18n::tr("opt_ps_log"));   // 0 = log
        m_psScale->addItem(I18n::tr("opt_ps_lin"));   // 1 = lin
        tpa->form()->addRow(m_tpaEnable);
        tpa->form()->addRow(I18n::tr("opt_tpa_mat"), m_tpaMatId);
        tpa->form()->addRow(I18n::tr("opt_tpa_beta"), m_tpaBeta);
        tpa->form()->addRow(m_psEnable);
        tpa->form()->addRow(I18n::tr("opt_ps_pmin"), m_psPmin);
        tpa->form()->addRow(I18n::tr("opt_ps_pmax"), m_psPmax);
        tpa->form()->addRow(I18n::tr("opt_ps_points"), m_psPoints);
        tpa->form()->addRow(I18n::tr("opt_ps_scale"), m_psScale);
        m_tpaWarn = new QLabel(tpa);
        m_tpaWarn->setWordWrap(true);
        m_tpaWarn->setStyleSheet("color: #C42B1C;");
        m_tpaWarn->setVisible(false);
        tpa->vbox()->addWidget(m_tpaWarn);
        page->vbox()->addWidget(tpa);

        m_solverStack->addWidget(page);
    }
    // [3] FMM
    {
        auto *page = new SectionBox(I18n::tr("opt_fmm_section"), m_solverStack);
        m_fmmHarmonics = new QSpinBox(page);
        m_fmmHarmonics->setRange(1, 201);
        m_fmmLi = new QCheckBox(page);
        page->form()->addRow(I18n::tr("opt_fmm_harmonics"), m_fmmHarmonics);
        page->form()->addRow(I18n::tr("opt_fmm_li"), m_fmmLi);
        m_solverStack->addWidget(page);
    }
    ss->vbox()->addWidget(m_solverStack);
    v->addWidget(ss);

    // wavelength range
    auto *sw = new SectionBox(I18n::tr("opt_wavelength"), body);
    m_lambdaMin = new QLineEdit(sw); m_lambdaMin->setMaximumWidth(100);
    m_lambdaMax = new QLineEdit(sw); m_lambdaMax->setMaximumWidth(100);
    m_lambdaDiv = new QSpinBox(sw);  m_lambdaDiv->setRange(2, 100000);
    sw->form()->addRow(I18n::tr("opt_lambda_min"), m_lambdaMin);
    sw->form()->addRow(I18n::tr("opt_lambda_max"), m_lambdaMax);
    sw->form()->addRow(I18n::tr("opt_lambda_div"), m_lambdaDiv);
    v->addWidget(sw);

    // optical mode
    auto *sm = new SectionBox(I18n::tr("opt_mode"), body);
    m_mode = new QComboBox(sm);
    m_mode->addItem(I18n::tr("opt_bpf"));
    m_mode->addItem(I18n::tr("opt_wg"));
    m_mode->addItem(I18n::tr("opt_ring"));
    m_mode->addItem(I18n::tr("opt_mzi"));
    m_mode->addItem(I18n::tr("opt_meta"));
    m_mode->addItem(I18n::tr("opt_phc"));
    m_mode->addItem(I18n::tr("opt_nf2ff"));
    m_mode->addItem(I18n::tr("opt_spara"));
    sm->vbox()->addWidget(m_mode);
    v->addWidget(sm);

    // BPF spec
    auto *sb = new SectionBox(I18n::tr("opt_bpf_section"), body);
    m_bpfMin = new QLineEdit(sb); m_bpfMin->setMaximumWidth(100);
    m_bpfMax = new QLineEdit(sb); m_bpfMax->setMaximumWidth(100);
    m_bpfQ = new QLineEdit(sb); m_bpfQ->setMaximumWidth(100);
    sb->form()->addRow(I18n::tr("opt_band") + " min", m_bpfMin);
    sb->form()->addRow(I18n::tr("opt_band") + " max", m_bpfMax);
    sb->form()->addRow(I18n::tr("opt_q"), m_bpfQ);
    v->addWidget(sb);

    // Ring spec
    auto *sr = new SectionBox(I18n::tr("opt_ring_section"), body);
    m_ringR = new QLineEdit(sr); m_ringR->setMaximumWidth(100);
    m_ringGap = new QLineEdit(sr); m_ringGap->setMaximumWidth(100);
    sr->form()->addRow(I18n::tr("opt_radius"), m_ringR);
    sr->form()->addRow(I18n::tr("opt_gap"), m_ringGap);
    v->addWidget(sr);

    // ── ONN 活性化カーブ結果 (obpm 実行後に activation_curve.csv を表示) ──
    auto *so = new SectionBox(I18n::tr("opt_onn_section"), body);
    m_onnStatus = new QLabel(I18n::tr("opt_onn_no_data"), so);
    m_onnStatus->setWordWrap(true);
    so->vbox()->addWidget(m_onnStatus);
    m_onnPlotP = new MiniPlot(so);
    m_onnPlotP->setLabels(I18n::tr("opt_onn_pin"), I18n::tr("opt_onn_pout"));
    m_onnPlotP->setVisible(false);
    so->vbox()->addWidget(m_onnPlotP);
    m_onnPlotT = new MiniPlot(so);
    m_onnPlotT->setLabels(I18n::tr("opt_onn_pin"), I18n::tr("opt_onn_trans"));
    m_onnPlotT->setVisible(false);
    so->vbox()->addWidget(m_onnPlotT);
    m_onnTable = new QTableWidget(0, 3, so);
    m_onnTable->setHorizontalHeaderLabels(
        { I18n::tr("opt_onn_pin"), I18n::tr("opt_onn_pout"),
          I18n::tr("opt_onn_trans") });
    m_onnTable->horizontalHeader()->setSectionResizeMode(
        QHeaderView::Stretch);
    m_onnTable->verticalHeader()->setVisible(false);
    m_onnTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_onnTable->setMinimumHeight(140);
    m_onnTable->setVisible(false);
    so->vbox()->addWidget(m_onnTable);
    v->addWidget(so);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    auto applyCb = [this] { apply(); };
    connect(m_solver, &QComboBox::currentIndexChanged, this, [this](int i) {
        m_solverStack->setCurrentIndex(i);
        apply();
    });
    connect(m_mode, &QComboBox::currentIndexChanged, this, applyCb);
    connect(m_bpmAlgo, &QComboBox::currentIndexChanged, this, applyCb);
    connect(m_bpmInput, &QComboBox::currentIndexChanged, this, applyCb);
    connect(m_psScale, &QComboBox::currentIndexChanged, this, applyCb);
    for (auto *e : { m_lambdaMin, m_lambdaMax, m_rcwaPx, m_rcwaPy, m_bpmDz,
                     m_bpmN0, m_tpaBeta, m_psPmin, m_psPmax,
                     m_bpfMin, m_bpfMax, m_bpfQ, m_ringR, m_ringGap })
        connect(e, &QLineEdit::editingFinished, this, applyCb);
    for (auto *s : { m_lambdaDiv, m_rcwaNx, m_rcwaNy, m_rcwaLayers,
                     m_fmmHarmonics, m_tpaMatId, m_psPoints })
        connect(s, &QSpinBox::valueChanged, this, applyCb);
    connect(m_fmmLi, &QCheckBox::toggled, this, applyCb);
    connect(m_tpaEnable, &QCheckBox::toggled, this, applyCb);
    connect(m_psEnable, &QCheckBox::toggled, this, applyCb);

    connect(project, &Project::loaded, this, &OpticalTab::refresh);
    refresh();
}

void OpticalTab::apply()
{
    if (m_updating) return;
    OpticalOpts &o = m_p->optical();
    o.solver = OpticalSolver(m_solver->currentIndex());
    o.mode   = OpticalMode(m_mode->currentIndex());
    o.lambdaMin = m_lambdaMin->text().toDouble();
    o.lambdaMax = m_lambdaMax->text().toDouble();
    o.lambdaDiv = m_lambdaDiv->value();
    o.rcwaNx = m_rcwaNx->value();
    o.rcwaNy = m_rcwaNy->value();
    o.rcwaPeriodX = m_rcwaPx->text().toDouble();
    o.rcwaPeriodY = m_rcwaPy->text().toDouble();
    o.rcwaLayers = m_rcwaLayers->value();
    o.bpmAlgorithm = m_bpmAlgo->currentIndex();
    o.bpmDz = m_bpmDz->text().toDouble();
    o.bpmRefIndex = m_bpmN0->text().toDouble();
    o.bpmInputMode = m_bpmInput->currentIndex();
    o.fmmHarmonics = m_fmmHarmonics->value();
    o.fmmLiRules = m_fmmLi->isChecked();
    o.bpfBandMin = m_bpfMin->text().toDouble();
    o.bpfBandMax = m_bpfMax->text().toDouble();
    o.bpfQ = m_bpfQ->text().toDouble();
    o.ringRadius_um = m_ringR->text().toDouble();
    o.ringGap_nm = m_ringGap->text().toDouble();

    // ── 非線形 (TPA) / ONN 活性化 — バリデーション付き ──
    // 不正値はモデルに書き込まず警告を表示する (β>0, 0<Pmin≤Pmax, 点数≥1)。
    o.tpaEnabled = m_tpaEnable->isChecked();
    o.tpaMaterialId = m_tpaMatId->value();
    o.powerSweepEnabled = m_psEnable->isChecked();
    o.psPoints = m_psPoints->value();          // QSpinBox が ≥1 を保証
    o.psLog = (m_psScale->currentIndex() == 0);
    QStringList warns;
    const double beta = m_tpaBeta->text().toDouble();
    if (beta > 0)
        o.tpaBeta_cmGW = beta;
    else if (m_tpaEnable->isChecked())
        warns << I18n::tr("opt_tpa_warn_beta");
    const double pmin = m_psPmin->text().toDouble();
    const double pmax = m_psPmax->text().toDouble();
    if (pmin > 0 && pmax >= pmin) {
        o.psPmin_W = pmin;
        o.psPmax_W = pmax;
    } else if (m_psEnable->isChecked()) {
        warns << I18n::tr("opt_ps_warn_range");
    }
    m_tpaWarn->setText(warns.join('\n'));
    m_tpaWarn->setVisible(!warns.isEmpty());
    updateTpaWidgetState();

    m_p->touch();
}

void OpticalTab::updateTpaWidgetState()
{
    const bool tpa = m_tpaEnable->isChecked();
    m_tpaMatId->setEnabled(tpa);
    m_tpaBeta->setEnabled(tpa);
    const bool ps = m_psEnable->isChecked();
    m_psPmin->setEnabled(ps);
    m_psPmax->setEnabled(ps);
    m_psPoints->setEnabled(ps);
    m_psScale->setEnabled(ps);
}

void OpticalTab::refresh()
{
    m_updating = true;
    const OpticalOpts &o = m_p->optical();
    m_solver->setCurrentIndex(int(o.solver));
    m_solverStack->setCurrentIndex(int(o.solver));
    m_mode->setCurrentIndex(int(o.mode));
    m_lambdaMin->setText(QString::number(o.lambdaMin, 'g', 8));
    m_lambdaMax->setText(QString::number(o.lambdaMax, 'g', 8));
    m_lambdaDiv->setValue(o.lambdaDiv);
    m_rcwaNx->setValue(o.rcwaNx);
    m_rcwaNy->setValue(o.rcwaNy);
    m_rcwaPx->setText(QString::number(o.rcwaPeriodX, 'g', 8));
    m_rcwaPy->setText(QString::number(o.rcwaPeriodY, 'g', 8));
    m_rcwaLayers->setValue(o.rcwaLayers);
    m_bpmAlgo->setCurrentIndex(o.bpmAlgorithm);
    m_bpmDz->setText(QString::number(o.bpmDz, 'g', 8));
    m_bpmN0->setText(QString::number(o.bpmRefIndex, 'g', 8));
    m_bpmInput->setCurrentIndex(o.bpmInputMode);
    m_tpaEnable->setChecked(o.tpaEnabled);
    m_tpaMatId->setValue(o.tpaMaterialId);
    m_tpaBeta->setText(QString::number(o.tpaBeta_cmGW, 'g', 8));
    m_psEnable->setChecked(o.powerSweepEnabled);
    m_psPmin->setText(QString::number(o.psPmin_W, 'g', 8));
    m_psPmax->setText(QString::number(o.psPmax_W, 'g', 8));
    m_psPoints->setValue(o.psPoints);
    m_psScale->setCurrentIndex(o.psLog ? 0 : 1);
    m_tpaWarn->setVisible(false);
    updateTpaWidgetState();
    m_fmmHarmonics->setValue(o.fmmHarmonics);
    m_fmmLi->setChecked(o.fmmLiRules);
    m_bpfMin->setText(QString::number(o.bpfBandMin, 'g', 8));
    m_bpfMax->setText(QString::number(o.bpfBandMax, 'g', 8));
    m_bpfQ->setText(QString::number(o.bpfQ, 'g', 8));
    m_ringR->setText(QString::number(o.ringRadius_um, 'g', 8));
    m_ringGap->setText(QString::number(o.ringGap_nm, 'g', 8));
    m_updating = false;
}

// ── ONN 活性化カーブ結果表示 ────────────────────────────────────────────────
void OpticalTab::showActivationResult(const QString &workdir, double aeff_m2)
{
    if (workdir.isEmpty()) return;
    const QString csv = QDir(workdir).filePath("activation_curve.csv");
    if (!QFileInfo::exists(csv)) return;   // powersweep 無し実行 — 表示は不変

    QVector<ActivationPoint> pts;
    QString err;
    if (!ActivationCurve::load(csv, pts, &err)) {
        m_onnStatus->setText(I18n::tr("opt_onn_parse_err").arg(err));
        return;
    }

    // 実測カーブ (CSV) — P_out(P_in) と T(P_in)
    MiniSeries sp;
    sp.color = QColor("#0078D4");
    sp.markers = true;
    sp.label = I18n::tr("opt_onn_measured");
    MiniSeries st = sp;
    for (const ActivationPoint &a : pts) {
        sp.pts.push_back({ a.pin, a.pout });
        st.pts.push_back({ a.pin, a.T });
    }
    QVector<MiniSeries> seriesP{ sp }, seriesT{ st };

    // 解析解 T = 1 / (1 + β·(P/A_eff)·L) の重ね描き。
    // β [cm/GW] → [m/W] は ×1e-11。L は z メッシュの伝搬長 [m]。
    const MeshAxis &mz = m_p->mesh(2);
    const double L = mz.nodes.isEmpty()
        ? 0.0 : (mz.nodes.last() - mz.nodes.first());
    if (aeff_m2 > 0 && L > 0) {
        const double betaMW = m_p->optical().tpaBeta_cmGW * 1e-11;
        MiniSeries at, ap;
        at.color = ap.color = QColor("#C42B1C");
        at.dashed = ap.dashed = true;
        at.label = ap.label = I18n::tr("opt_onn_analytic");
        for (const ActivationPoint &a : pts) {
            const double T = 1.0 / (1.0 + betaMW * (a.pin / aeff_m2) * L);
            at.pts.push_back({ a.pin, T });
            ap.pts.push_back({ a.pin, a.pin * T });
        }
        seriesT.push_back(at);
        seriesP.push_back(ap);
    }

    m_onnPlotP->setSeries(seriesP);
    m_onnPlotT->setYRange(0, 1);
    m_onnPlotT->setSeries(seriesT);
    m_onnPlotP->setVisible(true);
    m_onnPlotT->setVisible(true);

    m_onnTable->setRowCount(pts.size());
    for (int i = 0; i < pts.size(); ++i) {
        auto cell = [](double v) {
            auto *it = new QTableWidgetItem(QString::number(v, 'g', 6));
            it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            return it;
        };
        m_onnTable->setItem(i, 0, cell(pts[i].pin));
        m_onnTable->setItem(i, 1, cell(pts[i].pout));
        m_onnTable->setItem(i, 2, cell(pts[i].T));
    }
    m_onnTable->setVisible(true);

    QString status = I18n::tr("opt_onn_loaded").arg(pts.size());
    if (aeff_m2 > 0)
        status += "  " + I18n::tr("opt_onn_aeff")
                             .arg(QString::number(aeff_m2, 'g', 4));
    m_onnStatus->setText(status);
}
