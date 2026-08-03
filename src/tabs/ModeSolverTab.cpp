// ModeSolverTab.cpp
#include "ModeSolverTab.h"
#include "../widgets/MiniPlot.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"
#include "TabHelpers.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

using namespace ofd;
using namespace ofd::tabhelp;

// ── タブ固有語彙 (mds_) — file-local 登録 ───────────────────────────────────
namespace {
const bool s_i18n = [] {
    I18n::reg("mds_sec_solver", "導波路モードソルバ FDE / Waveguide mode solver",
              "Waveguide mode solver (FDE)");
    I18n::reg("mds_hint",
        "断面2Dの固有モード解析。neff/ng/分散/曲げ損失を算出し、モード波源・"
        "モード展開モニター・回路コンパクトモデルへ受け渡し。",
        "2D cross-section eigenmode analysis: neff/ng/dispersion/bend loss, "
        "feeding mode sources, expansion monitors and compact models.");
    I18n::reg("mds_approx_note",
        "⚠ 表示中の neff・指標は簡易近似式による目安です — 実 FDE ソルバ "
        "(OpenBPM モードソルバ) との連携は未実装。",
        "⚠ The neff values and metrics shown are quick approximations — "
        "integration with a real FDE solver (OpenBPM mode solver) is not "
        "implemented.");
    I18n::reg("mds_shape", "断面形状", "Cross-section");
    I18n::reg("mds_shape_strip", "ストリップ", "Strip");
    I18n::reg("mds_shape_rib", "リブ (スラブ付)", "Rib (with slab)");
    I18n::reg("mds_core", "コア寸法", "Core size");
    I18n::reg("mds_w", "幅", "Width");
    I18n::reg("mds_h", "高", "Height");
    I18n::reg("mds_slab", "スラブ", "Slab");
    I18n::reg("mds_mat", "材料", "Materials");
    I18n::reg("mds_mat_core", "コア: Si (n=3.476)", "Core: Si (n=3.476)");
    I18n::reg("mds_mat_sio2", "クラッド: SiO₂ (n=1.444)", "Cladding: SiO₂ (n=1.444)");
    I18n::reg("mds_mat_air", "クラッド: 空気 (n=1.0)", "Cladding: air (n=1.0)");
    I18n::reg("mds_mat_sin", "コア: SiN (n=2.0)", "Core: SiN (n=2.0)");
    I18n::reg("mds_mat_lnoi", "コア: LiNbO₃ (ne=2.138)", "Core: LiNbO₃ (ne=2.138)");
    I18n::reg("mds_mat_note", "材料Explorer と連動 · dn/dT 温度依存対応",
              "Linked to Material Explorer · dn/dT temperature dependence");
    I18n::reg("mds_wl_pol", "波長・偏波", "Wavelength / polarization");
    I18n::reg("mds_temp", "温度", "Temperature");
    I18n::reg("mds_run", "▶ モード計算", "▶ Compute modes");
    I18n::reg("mds_run_note", "メッシュ 10nm · PML境界 · 計算 <1秒",
              "10 nm mesh · PML boundaries · <1 s compute");
    I18n::reg("mds_sec_modes", "固有モード / Eigenmodes", "Eigenmodes");
    I18n::reg("mds_col_mode", "モード", "Mode");
    I18n::reg("mds_col_ng", "ng (群)", "ng (group)");
    I18n::reg("mds_col_guided", "導波", "Guided");
    I18n::reg("mds_col_loss", "伝搬損失 [dB/cm]", "Loss [dB/cm]");
    I18n::reg("mds_col_gamma", "閉込め係数 Γ", "Confinement Γ");
    I18n::reg("mds_guided", "導波", "Guided");
    I18n::reg("mds_cutoff", "カットオフ", "Cut-off");
    I18n::reg("mds_single_ok", "✓ シングルモード条件 満足 (高次モードはカットオフ)",
              "✓ Single-mode condition met (higher modes cut off)");
    I18n::reg("mds_single_ng", "⚠ マルチモード — 幅を %1nm 以下に縮小推奨",
              "⚠ Multi-mode — reduce width to ≤ %1 nm");
    I18n::reg("mds_show_field", "🗺 モード分布 |E|² 表示", "🗺 Show |E|² mode profile");
    I18n::reg("mds_to_source", "→ モード波源に設定", "→ Set as mode source");
    I18n::reg("mds_to_monitor", "→ モード展開モニターに登録",
              "→ Register as expansion monitor");
    I18n::reg("mds_to_schematic", "→ Schematic コンパクトモデル生成",
              "→ Generate Schematic compact model");
    I18n::reg("mds_sec_disp", "分散解析 / Dispersion", "Dispersion");
    I18n::reg("mds_sweep", "掃引", "Sweep");
    I18n::reg("mds_sweep_lambda", "波長 1500-1600nm", "Wavelength 1500-1600 nm");
    I18n::reg("mds_sweep_width", "幅 350-600nm", "Width 350-600 nm");
    I18n::reg("mds_sweep_temp", "温度 -40〜85°C", "Temperature -40 to 85 °C");
    I18n::reg("mds_sweep_run", "▶ 掃引実行", "▶ Run sweep");
    I18n::reg("mds_col_metric", "指標", "Metric");
    I18n::reg("mds_col_value", "値", "Value");
    I18n::reg("mds_col_use", "用途", "Use");
    I18n::reg("mds_sec_bend", "曲げ損失 / Bend loss", "Bend loss");
    I18n::reg("mds_col_radius", "曲げ半径", "Bend radius");
    I18n::reg("mds_col_rad_loss", "放射損失 [dB/90°]", "Radiation loss [dB/90°]");
    I18n::reg("mds_col_mismatch", "モード不整合 [dB/接続]",
              "Mode mismatch [dB/junction]");
    I18n::reg("mds_col_verdict", "判定", "Verdict");
    I18n::reg("mds_bend_dense", "高密度用", "For dense layout");
    I18n::reg("mds_bend_rec", "推奨", "Recommended");
    I18n::reg("mds_bend_low", "低損失", "Low loss");
    I18n::reg("mds_euler",
        "オイラー曲線 (クロソイド) で不整合損を低減 → GDS Layout の最小曲率DRCと連動",
        "Euler (clothoid) bends reduce mismatch loss → linked to GDS minimum "
        "curvature DRC");
    I18n::reg("mds_sec_corner", "プロセスコーナー / Corner analysis",
              "Corner analysis");
    I18n::reg("mds_corner_hint",
        "幅±10nm・高さ±5nm の4コーナーで neff/ng を一括評価 → ばらつき解析タブの"
        "モンテカルロと連携。",
        "Evaluate neff/ng across four ±10 nm width / ±5 nm height corners → "
        "feeds the tolerance tab's Monte Carlo.");
    I18n::reg("mds_col_corner", "コーナー", "Corner");
    I18n::reg("mds_col_dlambda", "Δλ共振 (Ring R=5μm)", "Δλ resonance (ring R=5 µm)");
    I18n::reg("mds_corner_nom", "公称", "Nominal");
    I18n::reg("mds_corner_pp", "幅+10 / 高+5", "W+10 / H+5");
    I18n::reg("mds_corner_mm", "幅-10 / 高-5", "W-10 / H-5");
    I18n::reg("mds_corner_note",
        "▸ リング共振器はコーナー間で ~2.4nm ずれる → 熱チューナ必須の根拠を定量化。",
        "▸ Ring resonances shift ~2.4 nm across corners → quantifies why "
        "thermal tuners are mandatory.");
    return true;
}();

// mock の簡易実効屈折率近似 (Si/SiO2, 1550nm) — 表示用のそれらしい値
double neffApprox(double w, double h, double slab, int mode)
{
    const double base = 2.35 + (w - 450) * 0.0009 + (h - 220) * 0.0018
                      + slab * 0.0008;
    return std::max(1.45, base - mode * 0.45);
}

QTableWidget *makeTable(QWidget *parent, const QStringList &headers)
{
    auto *t = new QTableWidget(0, headers.size(), parent);
    t->setHorizontalHeaderLabels(headers);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    t->horizontalHeader()->setStretchLastSection(true);
    t->verticalHeader()->setVisible(false);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionMode(QAbstractItemView::NoSelection);
    return t;
}

void fitTable(QTableWidget *t)
{
    t->resizeRowsToContents();
    int h = t->horizontalHeader()->height() + 2;
    for (int r = 0; r < t->rowCount(); ++r) h += t->rowHeight(r);
    t->setFixedHeight(h + 4);
}

} // namespace

// ── construction ────────────────────────────────────────────────────────────
ModeSolverTab::ModeSolverTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // ── ソルバ設定 ──────────────────────────────────────────────────────────
    auto *s1 = new SectionBox(I18n::tr("mds_sec_solver"), body);
    auto *hint = new QLabel(I18n::tr("mds_hint"), s1);
    hint->setWordWrap(true);
    s1->vbox()->addWidget(hint);
    // 近似値であることの明示 (絶対規則 5)
    auto *approx = new QLabel(I18n::tr("mds_approx_note"), s1);
    approx->setWordWrap(true);
    approx->setStyleSheet("color:#B58900;");
    s1->vbox()->addWidget(approx);

    m_shape = new QComboBox(s1);
    m_shape->addItem(I18n::tr("mds_shape_strip"));
    m_shape->addItem(I18n::tr("mds_shape_rib"));
    s1->form()->addRow(I18n::tr("mds_shape"), m_shape);

    auto *core = new QHBoxLayout();
    core->addWidget(new QLabel(I18n::tr("mds_w"), s1));
    m_width = new QDoubleSpinBox(s1);
    m_width->setRange(100, 5000); m_width->setDecimals(0);
    m_width->setValue(450); m_width->setSingleStep(10);
    core->addWidget(m_width);
    core->addWidget(new QLabel(QStringLiteral("nm ×"), s1));
    core->addWidget(new QLabel(I18n::tr("mds_h"), s1));
    m_height = new QDoubleSpinBox(s1);
    m_height->setRange(50, 2000); m_height->setDecimals(0);
    m_height->setValue(220); m_height->setSingleStep(10);
    core->addWidget(m_height);
    core->addWidget(new QLabel(QStringLiteral("nm"), s1));
    core->addWidget(new QLabel(I18n::tr("mds_slab"), s1));
    m_slab = new QDoubleSpinBox(s1);
    m_slab->setRange(0, 1000); m_slab->setDecimals(0);
    m_slab->setValue(0); m_slab->setSingleStep(10);
    m_slab->setEnabled(false);
    core->addWidget(m_slab);
    core->addWidget(new QLabel(QStringLiteral("nm"), s1));
    core->addStretch(1);
    s1->form()->addRow(I18n::tr("mds_core"), core);

    auto *mat = new QHBoxLayout();
    mat->addWidget(new QLabel(I18n::tr("mds_mat_core"), s1));
    m_material = new QComboBox(s1);
    m_material->addItem(I18n::tr("mds_mat_sio2"));
    m_material->addItem(I18n::tr("mds_mat_air"));
    m_material->addItem(I18n::tr("mds_mat_sin"));
    m_material->addItem(I18n::tr("mds_mat_lnoi"));
    mat->addWidget(m_material);
    auto *matNote = new QLabel(I18n::tr("mds_mat_note"), s1);
    mat->addWidget(matNote);
    mat->addStretch(1);
    s1->form()->addRow(I18n::tr("mds_mat"), mat);

    auto *wl = new QHBoxLayout();
    m_lambda = new QDoubleSpinBox(s1);
    m_lambda->setRange(200, 20000); m_lambda->setDecimals(0);
    m_lambda->setValue(1550); m_lambda->setSingleStep(10);
    wl->addWidget(m_lambda);
    wl->addWidget(new QLabel(QStringLiteral("nm"), s1));
    m_pol = new QComboBox(s1);
    m_pol->addItem(QStringLiteral("TE"));
    m_pol->addItem(QStringLiteral("TM"));
    wl->addWidget(m_pol);
    wl->addWidget(new QLabel(I18n::tr("mds_temp"), s1));
    m_temp = new QDoubleSpinBox(s1);
    m_temp->setRange(-80, 300); m_temp->setDecimals(0);
    m_temp->setValue(25);
    wl->addWidget(m_temp);
    wl->addWidget(new QLabel(QStringLiteral("°C"), s1));
    wl->addStretch(1);
    s1->form()->addRow(I18n::tr("mds_wl_pol"), wl);

    auto *runRow = new QHBoxLayout();
    auto *btnRun = new QPushButton(I18n::tr("mds_run"), s1);
    markNotImplemented(btnRun);   // 実 FDE 計算は未実装 (表は近似式で追従)
    runRow->addWidget(btnRun);
    runRow->addWidget(new QLabel(I18n::tr("mds_run_note"), s1));
    runRow->addStretch(1);
    s1->vbox()->addLayout(runRow);
    v->addWidget(s1);

    // ── 固有モード ──────────────────────────────────────────────────────────
    auto *s2 = new SectionBox(I18n::tr("mds_sec_modes"), body);
    m_modeTable = makeTable(s2, { I18n::tr("mds_col_mode"),
        QStringLiteral("neff"), I18n::tr("mds_col_ng"),
        I18n::tr("mds_col_guided"), I18n::tr("mds_col_loss"),
        I18n::tr("mds_col_gamma") });
    s2->vbox()->addWidget(m_modeTable);
    m_singleModeBadge = new QLabel(s2);
    m_singleModeBadge->setWordWrap(true);
    s2->vbox()->addWidget(m_singleModeBadge);
    s2->vbox()->addWidget(sampleNote(s2));
    auto *btnRow = new QHBoxLayout();
    for (const char *key : { "mds_show_field", "mds_to_source",
                             "mds_to_monitor", "mds_to_schematic" }) {
        auto *b = new QPushButton(I18n::tr(key), s2);
        markNotImplemented(b);
        btnRow->addWidget(b);
    }
    btnRow->addStretch(1);
    s2->vbox()->addLayout(btnRow);
    v->addWidget(s2);

    // ── 分散解析 ────────────────────────────────────────────────────────────
    auto *s3 = new SectionBox(I18n::tr("mds_sec_disp"), body);
    auto *swRow = new QHBoxLayout();
    auto *sweepSel = new QComboBox(s3);
    sweepSel->addItem(I18n::tr("mds_sweep_lambda"));
    sweepSel->addItem(I18n::tr("mds_sweep_width"));
    sweepSel->addItem(I18n::tr("mds_sweep_temp"));
    swRow->addWidget(new QLabel(I18n::tr("mds_sweep"), s3));
    swRow->addWidget(sweepSel);
    auto *btnSweep = new QPushButton(I18n::tr("mds_sweep_run"), s3);
    markNotImplemented(btnSweep);
    swRow->addWidget(btnSweep);
    swRow->addStretch(1);
    s3->vbox()->addLayout(swRow);
    m_dispPlot = new MiniPlot(s3);
    m_dispPlot->setLabels(QStringLiteral("λ [nm]"), QStringLiteral("neff"));
    m_dispPlot->setMinimumHeight(120);
    s3->vbox()->addWidget(m_dispPlot);
    auto *dispTable = makeTable(s3, { I18n::tr("mds_col_metric"),
        I18n::tr("mds_col_value"), I18n::tr("mds_col_use") });
    const char *kDisp[][3] = {
        { "群速度分散 D", "-1180 ps/nm/km", "高速変調の波形歪み評価" },
        { "複屈折 Δn (TE-TM)", "0.72", "偏波依存性" },
        { "dneff/dT", "1.86e-4 /K", "熱チューニング設計 (ヒーター)" },
        { "dneff/dw (感度)", "9.0e-4 /nm", "製造ばらつき→コーナー解析へ" },
    };
    for (const auto &row : kDisp) {
        const int r = dispTable->rowCount();
        dispTable->insertRow(r);
        for (int c = 0; c < 3; ++c)
            dispTable->setItem(r, c, roItem(QString::fromUtf8(row[c])));
    }
    fitTable(dispTable);
    s3->vbox()->addWidget(dispTable);
    s3->vbox()->addWidget(sampleNote(s3));
    v->addWidget(s3);

    // ── 曲げ損失 ────────────────────────────────────────────────────────────
    auto *s4 = new SectionBox(I18n::tr("mds_sec_bend"), body);
    auto *bendTable = makeTable(s4, { I18n::tr("mds_col_radius"),
        I18n::tr("mds_col_rad_loss"), I18n::tr("mds_col_mismatch"),
        I18n::tr("mds_col_verdict") });
    const struct { const char *r, *rad, *mis; const char *verdictKey; }
    kBend[] = {
        { "3 μm",  "0.082",  "0.041", "mds_bend_dense" },
        { "5 μm",  "0.011",  "0.018", "mds_bend_rec" },
        { "10 μm", "0.001",  "0.006", "mds_bend_low" },
        { "20 μm", "<0.001", "0.002", nullptr },
    };
    for (const auto &row : kBend) {
        const int r = bendTable->rowCount();
        bendTable->insertRow(r);
        bendTable->setItem(r, 0, roItem(QString::fromUtf8(row.r)));
        bendTable->setItem(r, 1, roItem(QString::fromUtf8(row.rad)));
        bendTable->setItem(r, 2, roItem(QString::fromUtf8(row.mis)));
        bendTable->setItem(r, 3, roItem(row.verdictKey
            ? I18n::tr(row.verdictKey) : QStringLiteral("—")));
    }
    fitTable(bendTable);
    s4->vbox()->addWidget(bendTable);
    auto *euler = new QLabel(I18n::tr("mds_euler"), s4);
    euler->setWordWrap(true);
    s4->vbox()->addWidget(euler);
    s4->vbox()->addWidget(sampleNote(s4));
    v->addWidget(s4);

    // ── プロセスコーナー ────────────────────────────────────────────────────
    auto *s5 = new SectionBox(I18n::tr("mds_sec_corner"), body);
    auto *cornerHint = new QLabel(I18n::tr("mds_corner_hint"), s5);
    cornerHint->setWordWrap(true);
    s5->vbox()->addWidget(cornerHint);
    m_cornerTable = makeTable(s5, { I18n::tr("mds_col_corner"),
        QStringLiteral("neff"), I18n::tr("mds_col_dlambda") });
    s5->vbox()->addWidget(m_cornerTable);
    auto *cornerNote = new QLabel(I18n::tr("mds_corner_note"), s5);
    cornerNote->setWordWrap(true);
    s5->vbox()->addWidget(cornerNote);
    s5->vbox()->addWidget(sampleNote(s5));
    v->addWidget(s5);
    v->addStretch(1);

    // ── 接続: 入力変更 → 近似再計算 ─────────────────────────────────────────
    connect(m_shape, &QComboBox::currentIndexChanged, this, [this](int i) {
        m_slab->setEnabled(i == 1);
        m_slab->setValue(i == 1 ? 90 : 0);
        recalc();
    });
    const auto onSpin = qOverload<double>(&QDoubleSpinBox::valueChanged);
    connect(m_width, onSpin, this, &ModeSolverTab::recalc);
    connect(m_height, onSpin, this, &ModeSolverTab::recalc);
    connect(m_slab, onSpin, this, &ModeSolverTab::recalc);
    connect(m_pol, &QComboBox::currentIndexChanged, this,
            [this](int) { recalc(); });

    recalc();
    setWidget(body);
    setWidgetResizable(true);
}

// ── 近似計算 (mock の neffCalc をそのまま移植) ──────────────────────────────
void ModeSolverTab::recalc()
{
    const double w = m_width->value(), h = m_height->value(),
                 slab = m_slab->value();
    const bool te = m_pol->currentIndex() == 0;

    m_modeTable->setRowCount(0);
    bool mode1Guided = false;
    for (int m = 0; m < 3; ++m) {
        const double neff = neffApprox(w, h, slab, m);
        const bool guided = neff > 1.48;
        if (m == 1) mode1Guided = guided;
        const double ng = neff + 1.85;
        const double loss = (m == 0) ? 1.8 : (m == 1) ? 4.2 : 12.5;
        const int r = m_modeTable->rowCount();
        m_modeTable->insertRow(r);
        m_modeTable->setItem(r, 0, roItem(
            QStringLiteral("%1%2").arg(te ? "TE" : "TM").arg(m)));
        m_modeTable->setItem(r, 1, roItem(QString::number(neff, 'f', 4)));
        m_modeTable->setItem(r, 2, roItem(
            guided ? QString::number(ng, 'f', 3) : QStringLiteral("—")));
        m_modeTable->setItem(r, 3, roItem(
            guided ? I18n::tr("mds_guided") : I18n::tr("mds_cutoff")));
        m_modeTable->setItem(r, 4, roItem(
            guided ? QString::number(loss, 'f', 1) : QStringLiteral("—")));
        m_modeTable->setItem(r, 5, roItem(
            guided ? QString::number(0.82 - m * 0.25, 'f', 2)
                   : QStringLiteral("—")));
    }
    fitTable(m_modeTable);
    m_singleModeBadge->setText(!mode1Guided
        ? I18n::tr("mds_single_ok")
        : I18n::tr("mds_single_ng")
              .arg(std::max(300.0, w - 100.0), 0, 'f', 0));

    // 分散カーブ (近似式の波長スロープ)
    QVector<QPointF> pts;
    const double n0 = neffApprox(w, h, slab, 0);
    for (int i = 0; i < 21; ++i)
        pts.append(QPointF(1500 + i * 5, n0 - (i - 10) * 0.0011));
    MiniSeries series;
    series.pts = pts;
    m_dispPlot->setSeries({ series });

    // コーナー表
    m_cornerTable->setRowCount(0);
    const struct { const char *key; double dw, dh; const char *dl; }
    kCorners[] = {
        { "mds_corner_nom",  0,  0, "—" },
        { "mds_corner_pp",  10,  5, "+1.21 nm" },
        { "mds_corner_mm", -10, -5, "-1.19 nm" },
    };
    for (const auto &c : kCorners) {
        const int r = m_cornerTable->rowCount();
        m_cornerTable->insertRow(r);
        m_cornerTable->setItem(r, 0, roItem(I18n::tr(c.key)));
        m_cornerTable->setItem(r, 1, roItem(QString::number(
            neffApprox(w + c.dw, h + c.dh, slab, 0), 'f', 4)));
        m_cornerTable->setItem(r, 2, roItem(QString::fromUtf8(c.dl)));
    }
    fitTable(m_cornerTable);
}
