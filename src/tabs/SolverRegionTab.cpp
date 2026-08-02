// SolverRegionTab.cpp
#include "SolverRegionTab.h"
#include "../core/Project.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"
#include "TabHelpers.h"

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QSlider>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

using namespace ofd;

// ── タブ固有語彙 (sreg_) — file-local 登録 ──────────────────────────────────
namespace {
const bool s_i18n = [] {
    ofd::I18n::reg("sreg_title", "ソルバ領域", "FDTD Solver Region");
    ofd::I18n::reg("sreg_hint",
        "Lumerical FDTD の中心オブジェクト。シミュレーション時間・領域・メッシュ・境界条件を一括管理。\n"
        "形状・波源・モニターはこの領域内に配置されます。",
        "The central Lumerical-FDTD object: simulation time, region, mesh and "
        "boundary conditions managed in one place.\n"
        "Geometry, sources and monitors are placed inside this region.");
    ofd::I18n::reg("sreg_region", "シミュレーション領域", "Simulation region");
    ofd::I18n::reg("sreg_dim", "次元", "Dimension");
    ofd::I18n::reg("sreg_dim_cyl", "円筒対称 (2.5D)", "Cylindrical (2.5D)");
    ofd::I18n::reg("sreg_xrange", "X 範囲", "X range");
    ofd::I18n::reg("sreg_yrange", "Y 範囲", "Y range");
    ofd::I18n::reg("sreg_zrange", "Z 範囲", "Z range");
    ofd::I18n::reg("sreg_mesh", "メッシュ設定", "Mesh");
    ofd::I18n::reg("sreg_mesh_acc", "メッシュ精度 (Lumerical風)",
                   "Mesh accuracy (Lumerical-style)");
    ofd::I18n::reg("sreg_acc_coarse",
        "▸ 粗 (1〜2): 高速・初期検討用、誤差 5〜10% (目安)",
        "▸ Coarse (1-2): fast, for initial studies, 5-10% error (rough guide)");
    ofd::I18n::reg("sreg_acc_std",
        "▸ 標準 (3): 一般的精度、誤差 ~2% (目安)",
        "▸ Standard (3): typical accuracy, ~2% error (rough guide)");
    ofd::I18n::reg("sreg_acc_mid",
        "▸ 中 (4): 推奨デフォルト、誤差 ~1% (目安)",
        "▸ Medium (4): recommended default, ~1% error (rough guide)");
    ofd::I18n::reg("sreg_acc_high",
        "▸ 高精度 (5〜6): 製品設計用、誤差 ~0.5% (目安)",
        "▸ High (5-6): for product design, ~0.5% error (rough guide)");
    ofd::I18n::reg("sreg_acc_max",
        "▸ 最高 (7〜8): 検証用、計算時間×16, 誤差 <0.2% (目安)",
        "▸ Highest (7-8): for verification, 16x compute time, <0.2% error "
        "(rough guide)");
    ofd::I18n::reg("sreg_cells", "セル数 (目安 — プリセット例)",
                   "Cell count (guide — preset example)");
    ofd::I18n::reg("sreg_memory", "メモリ (目安 — プリセット例)",
                   "Memory (guide — preset example)");
    ofd::I18n::reg("sreg_est_time", "計算時間 (目安 — プリセット例)",
                   "Compute time (guide — preset example)");
    ofd::I18n::reg("sreg_preset_note",
        "上の値はメッシュ精度プリセットごとの固定の目安表で、"
        "このプロジェクトからの実推定ではありません。"
        "実際のセル数・メモリ推定はメッシュタブを参照。",
        "The values above come from a fixed per-preset lookup table, not an "
        "estimate for this project. See the Mesh tab for the real cell-count "
        "and memory estimate.");
    ofd::I18n::reg("sreg_eta1", "<5秒", "<5 s");
    ofd::I18n::reg("sreg_eta2", "~20秒", "~20 s");
    ofd::I18n::reg("sreg_eta3", "~1分", "~1 min");
    ofd::I18n::reg("sreg_eta4", "~5分", "~5 min");
    ofd::I18n::reg("sreg_eta5", "~15分", "~15 min");
    ofd::I18n::reg("sreg_eta6", "~1時間", "~1 h");
    ofd::I18n::reg("sreg_eta7", "~4時間", "~4 h");
    ofd::I18n::reg("sreg_eta8", "~12時間", "~12 h");
    ofd::I18n::reg("sreg_advanced", "▼ 詳細メッシュ設定", "▼ Advanced");
    ofd::I18n::reg("sreg_mesh_type", "メッシュタイプ", "Mesh type");
    ofd::I18n::reg("sreg_mesh_auto", "自動非均一", "Auto non-uniform");
    ofd::I18n::reg("sreg_mesh_uniform", "均一", "Uniform");
    ofd::I18n::reg("sreg_mesh_override", "オーバーライド", "Override");
    ofd::I18n::reg("sreg_ref", "メッシュリファインメント", "Mesh refinement");
    ofd::I18n::reg("sreg_ref_stair", "階段", "Staircase");
    ofd::I18n::reg("sreg_ref_curve", "湾曲面 (Conformal)", "Conformal");
    ofd::I18n::reg("sreg_subpixel", "サブピクセル平均 (推奨)",
                   "Subpixel averaging (recommended)");
    ofd::I18n::reg("sreg_auto_override",
                   "メッシュオーバーライド領域を自動配置 (未実装)",
                   "Auto-place mesh override regions (not implemented)");
    ofd::I18n::reg("sreg_simtime", "シミュレーション時間", "Simulation time");
    ofd::I18n::reg("sreg_time", "時間 (FDTD)", "Time (FDTD)");
    ofd::I18n::reg("sreg_shutoff", "自動シャットオフ", "Auto shutoff");
    ofd::I18n::reg("sreg_shutoff_level", "レベル ≤", "level ≤");
    ofd::I18n::reg("sreg_dt_steps", "→ ステップ数 ~107,400", "→ ~107,400 steps");
    ofd::I18n::reg("sreg_stable", "安定", "Stable");
    ofd::I18n::reg("sreg_cfl", "CFL 係数", "CFL factor");
    ofd::I18n::reg("sreg_cfl_hint", "(0.99で安定限界、0.5で安全)",
                   "(0.99 = stability limit, 0.5 = safe)");
    ofd::I18n::reg("sreg_bc", "境界条件", "Boundary conditions");
    ofd::I18n::reg("sreg_bc_hint", "面別BC (詳細は「境界面」タブで設定)",
                   "Per-face BC (details in the \"Per-face BC\" tab)");
    ofd::I18n::reg("sreg_col_face", "面", "Face");
    ofd::I18n::reg("sreg_pml_profile", "PML プロファイル", "PML profile");
    ofd::I18n::reg("sreg_pml_layers", "PML 層数", "PML layers");
    ofd::I18n::reg("sreg_auto_sym", "対称性を自動検出して計算量削減 (未実装)",
                   "Auto-detect symmetry to reduce computation (not implemented)");
    ofd::I18n::reg("sreg_bc_unwired",
        "PML 層数以外の設定は現在計算へ反映されません (未実装)",
        "Settings other than PML layers are not applied to the solver yet "
        "(not implemented)");
    return true;
}();

// メッシュ精度 1〜8 の派生値表 (モックの配列をそのまま)
const qint64 kCells[8]     = { 8000, 27900, 88000, 280000, 720000,
                               1800000, 4200000, 8400000 };
const int    kLambdaDiv[8] = { 6, 10, 14, 18, 22, 30, 40, 50 };
const int    kMemMB[8]     = { 5, 18, 56, 180, 460, 1200, 2700, 5400 };
const char  *kEtaKeys[8]   = { "sreg_eta1", "sreg_eta2", "sreg_eta3",
                               "sreg_eta4", "sreg_eta5", "sreg_eta6",
                               "sreg_eta7", "sreg_eta8" };
} // namespace

SolverRegionTab::SolverRegionTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // ── ソルバ領域 / FDTD Solver Region (説明) ──────────────────────────────
    auto *st = new SectionBox(I18n::tr("sreg_title"), body);
    auto *hint = new QLabel(I18n::tr("sreg_hint"), st);
    hint->setWordWrap(true);
    st->vbox()->addWidget(hint);
    v->addWidget(st);

    // ── シミュレーション領域 / Simulation region ────────────────────────────
    auto *sr = new SectionBox(I18n::tr("sreg_region"), body);
    m_dim = new QComboBox(sr);
    m_dim->addItem("2D");
    m_dim->addItem("3D");
    m_dim->addItem(I18n::tr("sreg_dim_cyl"));
    m_dim->setCurrentIndex(1);           // 既定 "3d"
    sr->form()->addRow(I18n::tr("sreg_dim"), m_dim);

    auto rangeRow = [&sr](QLineEdit *&lo, QLineEdit *&hi,
                          const char *loDef, const char *hiDef, bool unitM) {
        auto *h = new QHBoxLayout();
        lo = new QLineEdit(loDef, sr); lo->setMaximumWidth(100);
        hi = new QLineEdit(hiDef, sr); hi->setMaximumWidth(100);
        h->addWidget(lo);
        h->addWidget(new QLabel("〜", sr));
        h->addWidget(hi);
        if (unitM) h->addWidget(new QLabel("m", sr));
        h->addStretch(1);
        return h;
    };
    sr->form()->addRow(I18n::tr("sreg_xrange"),
                       rangeRow(m_xMin, m_xMax, "-0.030", "0.030", true));
    sr->form()->addRow(I18n::tr("sreg_yrange"),
                       rangeRow(m_yMin, m_yMax, "-0.030", "0.030", false));
    sr->form()->addRow(I18n::tr("sreg_zrange"),
                       rangeRow(m_zMin, m_zMax, "0.000", "0.030", false));
    // 領域指定はまだ Project / .ofd へ配線されていない (絶対規則 5)
    sr->form()->addRow(tabhelp::unwiredNote(sr));
    v->addWidget(sr);

    // ── メッシュ設定 / Mesh ─────────────────────────────────────────────────
    auto *sm = new SectionBox(I18n::tr("sreg_mesh"), body);
    auto *accRow = new QHBoxLayout();
    m_meshAcc = new QSlider(Qt::Horizontal, sm);
    m_meshAcc->setRange(1, 8);
    m_meshAcc->setValue(2);              // 既定 meshAcc = 2
    m_meshAcc->setTickPosition(QSlider::TicksBelow);
    m_meshAcc->setTickInterval(1);
    accRow->addWidget(m_meshAcc, 1);
    m_meshAccVal = new QLabel("2", sm);
    m_meshAccVal->setMinimumWidth(28);
    m_meshAccVal->setAlignment(Qt::AlignCenter);
    m_meshAccVal->setStyleSheet("font-weight:600; color:#0078D4;");
    accRow->addWidget(m_meshAccVal);
    sm->form()->addRow(I18n::tr("sreg_mesh_acc"), accRow);

    m_meshHint = new QLabel(sm);
    m_meshHint->setWordWrap(true);
    sm->form()->addRow(m_meshHint);

    auto *cellRow = new QHBoxLayout();
    m_cells = new QLabel(sm);
    m_cellsNote = new QLabel(sm);
    cellRow->addWidget(m_cells);
    cellRow->addWidget(m_cellsNote);
    cellRow->addStretch(1);
    sm->form()->addRow(I18n::tr("sreg_cells"), cellRow);

    m_memory = new QLabel(sm);
    sm->form()->addRow(I18n::tr("sreg_memory"), m_memory);
    m_estTime = new QLabel(sm);
    sm->form()->addRow(I18n::tr("sreg_est_time"), m_estTime);

    // セル数/メモリ/計算時間は精度プリセットの固定表 (実推定ではない)
    sm->form()->addRow(tabhelp::sampleNote(sm));
    auto *presetNote = new QLabel(I18n::tr("sreg_preset_note"), sm);
    presetNote->setWordWrap(true);
    presetNote->setStyleSheet("font-size:11px; color:palette(mid);");
    sm->form()->addRow(presetNote);

    auto *sep = new QFrame(sm);          // sep-h
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Sunken);
    sm->form()->addRow(sep);
    sm->form()->addRow(new QLabel(I18n::tr("sreg_advanced"), sm));

    m_meshType = new QComboBox(sm);
    m_meshType->addItem(I18n::tr("sreg_mesh_auto"));
    m_meshType->addItem(I18n::tr("sreg_mesh_uniform"));
    m_meshType->addItem(I18n::tr("sreg_mesh_override"));
    sm->form()->addRow(I18n::tr("sreg_mesh_type"), m_meshType);

    m_meshRefine = new QComboBox(sm);
    m_meshRefine->addItem(I18n::tr("sreg_ref_stair"));
    m_meshRefine->addItem(I18n::tr("sreg_ref_curve"));
    m_meshRefine->addItem("Volume Average");
    m_meshRefine->addItem("Yu-Mittra");
    m_meshRefine->setCurrentIndex(1);    // 既定 "curve"
    sm->form()->addRow(I18n::tr("sreg_ref"), m_meshRefine);

    m_subpixel = new QCheckBox(I18n::tr("sreg_subpixel"), sm);
    m_subpixel->setChecked(true);
    sm->form()->addRow(m_subpixel);
    m_autoOverride = new QCheckBox(I18n::tr("sreg_auto_override"), sm);
    m_autoOverride->setChecked(true);
    sm->form()->addRow(m_autoOverride);
    sm->form()->addRow(tabhelp::unwiredNote(sm));
    v->addWidget(sm);

    // ── シミュレーション時間 / Simulation time ──────────────────────────────
    auto *ss = new SectionBox(I18n::tr("sreg_simtime"), body);
    auto *timeRow = new QHBoxLayout();
    m_simTime = new QLineEdit("1000", ss);
    m_simTime->setMaximumWidth(100);
    m_simTimeUnit = new QLabel(ss);
    timeRow->addWidget(m_simTime);
    timeRow->addWidget(m_simTimeUnit);
    timeRow->addStretch(1);
    ss->form()->addRow(I18n::tr("sreg_time"), timeRow);

    auto *shutRow = new QHBoxLayout();
    m_shutoffOn = new QCheckBox("ON", ss);
    m_shutoffOn->setChecked(true);
    m_shutoffLevel = new QLineEdit("1e-5", ss);
    m_shutoffLevel->setMaximumWidth(70);
    shutRow->addWidget(m_shutoffOn);
    shutRow->addWidget(new QLabel(I18n::tr("sreg_shutoff_level"), ss));
    shutRow->addWidget(m_shutoffLevel);
    shutRow->addStretch(1);
    ss->form()->addRow(I18n::tr("sreg_shutoff"), shutRow);

    auto *dtRow = new QHBoxLayout();
    dtRow->addWidget(new QLabel("9.31e-13 s", ss));
    dtRow->addWidget(new QLabel(I18n::tr("sreg_dt_steps"), ss));
    auto *stable = new QLabel(I18n::tr("sreg_stable"), ss);
    stable->setStyleSheet("color:#2E8B57; font-weight:600;");   // badge ok
    dtRow->addWidget(stable);
    dtRow->addStretch(1);
    ss->form()->addRow(QString::fromUtf8("Δt (CFL)"), dtRow);
    // Δt / ステップ数 / 「安定」バッジはモックの固定値 (CFL 計算は未実装)
    ss->form()->addRow(tabhelp::sampleNote(ss));

    auto *cflRow = new QHBoxLayout();
    m_cfl = new QLineEdit("0.99", ss);
    m_cfl->setMaximumWidth(70);
    cflRow->addWidget(m_cfl);
    cflRow->addWidget(new QLabel(I18n::tr("sreg_cfl_hint"), ss));
    cflRow->addStretch(1);
    ss->form()->addRow(I18n::tr("sreg_cfl"), cflRow);
    ss->form()->addRow(tabhelp::unwiredNote(ss));
    v->addWidget(ss);

    // ── 境界条件 / Boundary conditions ──────────────────────────────────────
    auto *sb = new SectionBox(I18n::tr("sreg_bc"), body);
    auto *bcHint = new QLabel(I18n::tr("sreg_bc_hint"), sb);
    bcHint->setWordWrap(true);
    sb->form()->addRow(bcHint);

    m_bcTable = new QTableWidget(1, 7, sb);
    m_bcTable->setHorizontalHeaderLabels({ I18n::tr("sreg_col_face"),
                                           "X-", "X+", "Y-", "Y+", "Z-", "Z+" });
    m_bcTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_bcTable->verticalHeader()->setVisible(false);
    m_bcTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_bcTable->setMaximumHeight(70);
    m_bcTable->setItem(0, 0, new QTableWidgetItem("BC"));
    static const char *kFaceBC[6] = { "PML", "PML", "PML", "PML", "PEC", "PML" };
    for (int c = 0; c < 6; ++c) {
        auto *it = new QTableWidgetItem(kFaceBC[c]);
        it->setTextAlignment(Qt::AlignCenter);
        if (c == 4) {                    // Z- の PEC は accent バッジ
            it->setForeground(QColor("#0078D4"));
            QFont f = it->font(); f.setBold(true); it->setFont(f);
        }
        m_bcTable->setItem(0, c + 1, it);
    }
    sb->form()->addRow(m_bcTable);
    // 面別 BC 表はモックの固定値 (境界面タブの実設定とは連動していない)
    sb->form()->addRow(tabhelp::sampleNote(sb));

    m_pmlProfile = new QComboBox(sb);
    m_pmlProfile->addItems({ "Standard", "Stabilized", "CPML" });
    sb->form()->addRow(I18n::tr("sreg_pml_profile"), m_pmlProfile);

    m_pmlLayers = new QSpinBox(sb);
    m_pmlLayers->setRange(1, 64);
    m_pmlLayers->setValue(8);            // モック既定 (refresh で Project 値に)
    sb->form()->addRow(I18n::tr("sreg_pml_layers"), m_pmlLayers);

    m_autoSym = new QCheckBox(I18n::tr("sreg_auto_sym"), sb);
    sb->form()->addRow(m_autoSym);
    // PML 層数だけは Project (GeneralOpts) に永続化される — 他は未配線
    auto *bcUnwired = new QLabel(I18n::tr("sreg_bc_unwired"), sb);
    bcUnwired->setWordWrap(true);
    bcUnwired->setStyleSheet("font-size:11px; color:palette(mid);");
    sb->form()->addRow(bcUnwired);
    v->addWidget(sb);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    connect(m_meshAcc, &QSlider::valueChanged, this,
            [this] { updateMeshDerived(); });
    connect(m_pmlLayers, &QSpinBox::valueChanged, this,
            [this] { apply(); });
    connect(project, &Project::domainChanged, this,
            [this] { updateDomainDeps(); });
    connect(project, &Project::loaded, this, &SolverRegionTab::refresh);
    refresh();
}

// PML 層数のみ Project (GeneralOpts) に対応するので永続化
void SolverRegionTab::apply()
{
    if (m_updating) return;
    m_p->general().pmlL = m_pmlLayers->value();
    m_p->touch();
}

void SolverRegionTab::refresh()
{
    m_updating = true;
    m_pmlLayers->setValue(m_p->general().pmlL);
    m_updating = false;
    updateMeshDerived();
    updateDomainDeps();
}

void SolverRegionTab::updateMeshDerived()
{
    const int a = m_meshAcc->value();     // 1..8
    m_meshAccVal->setText(QString::number(a));
    const char *hintKey = (a <= 2) ? "sreg_acc_coarse"
                        : (a == 3) ? "sreg_acc_std"
                        : (a == 4) ? "sreg_acc_mid"
                        : (a <= 6) ? "sreg_acc_high"
                                   : "sreg_acc_max";
    m_meshHint->setText(I18n::tr(hintKey));
    m_cells->setText(QLocale(QLocale::English).toString(qlonglong(kCells[a - 1])));
    m_memory->setText(QStringLiteral("%1 MB").arg(kMemMB[a - 1]));
    m_estTime->setText(I18n::tr(kEtaKeys[a - 1]));
    updateDomainDeps();
}

void SolverRegionTab::updateDomainDeps()
{
    const bool optical = (m_p->activeDomain() == Domain::Optical);
    const int a = m_meshAcc->value();
    m_cellsNote->setText(QStringLiteral("(λ/%1 @ %2)")
        .arg(kLambdaDiv[a - 1])
        .arg(optical ? QStringLiteral("1550nm") : QStringLiteral("2.5GHz")));
    m_simTimeUnit->setText(optical ? QStringLiteral("fs") : QStringLiteral("ns"));
}
