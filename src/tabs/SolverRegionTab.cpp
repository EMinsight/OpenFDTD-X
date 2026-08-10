// SolverRegionTab.cpp
#include "SolverRegionTab.h"
#include "../core/Project.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"
#include "TabHelpers.h"

#include <QApplication>
#include <QBrush>
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
    // 音響/水中音響用: FDTD/Lumerical 前提を含まないドメイン中立の文言
    ofd::I18n::reg("sreg_hint_ac",
        "シミュレーション時間・領域・メッシュを一括管理する中心オブジェクト。\n"
        "形状・音源・受音点はこの領域内に配置されます。",
        "The central object managing simulation time, region and mesh "
        "in one place.\n"
        "Geometry, sources and receivers are placed inside this region.");
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
    ofd::I18n::reg("sreg_cells", "セル数 (推定)", "Cell count (estimate)");
    ofd::I18n::reg("sreg_memory", "メモリ (推定)", "Memory (estimate)");
    ofd::I18n::reg("sreg_est_note",
        "セル数・メモリは現在のメッシュ設定 (メッシュタブ) からの実推定です "
        "(約 60 byte/セル換算)。計算時間の予測は実測モデルが無いため表示しません。",
        "Cell count and memory are estimated from the current mesh settings "
        "(Mesh tab), assuming ~60 bytes/cell. No compute-time prediction is "
        "shown (no measured model available).");
    // メッシュ精度プリセットの目標解像度 (λ/N @ 代表周波数) — スライダの説明
    ofd::I18n::reg("sreg_lambda_note", "(目標 λ/%1 @ %2)", "(target λ/%1 @ %2)");
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
    ofd::I18n::reg("sreg_dt_steps_fmt", "→ ステップ数 ~%1", "→ ~%1 steps");
    ofd::I18n::reg("sreg_dt_none", "— (メッシュ未定義)", "— (no mesh defined)");
    ofd::I18n::reg("sreg_stable", "安定", "Stable");
    ofd::I18n::reg("sreg_unstable", "不安定", "Unstable");
    ofd::I18n::reg("sreg_cfl", "CFL 係数", "CFL factor");
    ofd::I18n::reg("sreg_cfl_hint", "(0.99で安定限界、0.5で安全)",
                   "(0.99 = stability limit, 0.5 = safe)");
    ofd::I18n::reg("sreg_bc", "境界条件", "Boundary conditions");
    ofd::I18n::reg("sreg_bc_hint", "面別BC (詳細は「境界面」タブで設定)",
                   "Per-face BC (details in the \"Per-face BC\" tab)");
    ofd::I18n::reg("sreg_col_face", "面", "Face");
    ofd::I18n::reg("sreg_bc_pbc", "周期", "Periodic");
    ofd::I18n::reg("sreg_bc_derived",
        "面別表示は全般タブの吸収境界 (abc) と周期境界 (pbc) からの導出です "
        "(.ofd に面別の個別 BC はありません)。",
        "Per-face labels are derived from the ABC and PBC settings on the "
        "General tab (the .ofd format has no per-face BC).");
    ofd::I18n::reg("sreg_pml_profile", "PML プロファイル", "PML profile");
    ofd::I18n::reg("sreg_pml_layers", "PML 層数", "PML layers");
    ofd::I18n::reg("sreg_auto_sym", "対称性を自動検出して計算量削減 (未実装)",
                   "Auto-detect symmetry to reduce computation (not implemented)");
    ofd::I18n::reg("sreg_bc_unwired",
        "PML 層数以外の設定は現在計算へ反映されません (未実装)",
        "Settings other than PML layers are not applied to the solver yet "
        "(not implemented)");
    I18n::reg("sreg_uw_region", "解析領域の指定 (Project / .ofd へまだ配線されていません)",
              "the analysis-region settings (not wired to the Project or the .ofd yet)");
    I18n::reg("sreg_uw_mesh", "メッシュ細分化・サブピクセル・自動上書きの設定",
              "the mesh-refinement, subpixel and auto-override settings");
    ofd::I18n::reg("sreg_apply_time",
        "この節の値を計算へ反映する (.ofd の timestep / solver を書き換えます)",
        "Apply this section to the solver (writes the .ofd timestep / solver "
        "keys)");
    ofd::I18n::reg("sreg_apply_time_hint",
        "OFF のあいだ Δt はカーネルが Courant 限界から自動決定します "
        "(CFL = 0.99 相当)。ON にすると Δt = CFL 係数 × Courant 限界、"
        "反復回数 = シミュレーション時間 ÷ Δt、収束条件 = シャットオフレベル "
        "を「全般」タブと同じ値として書き込みます。",
        "While OFF the kernel picks Δt from the Courant limit itself "
        "(equivalent to CFL = 0.99). When ON, Δt = CFL factor x Courant "
        "limit, the iteration count = simulation time / Δt and the "
        "convergence criterion = the shutoff level are written into the same "
        "fields the General tab edits.");
    I18n::reg("sreg_uw_dt", "メッシュ精度スライダの Δt への反映",
              "the effect of the mesh-accuracy slider on the time step");
    I18n::reg("sreg_uw_dt_ok",
              "Δt (CFL 係数)・シミュレーション時間・自動シャットオフ "
              "— 上のチェックが ON のとき .ofd の timestep / solver へ書かれます",
              "the time step (CFL factor), the simulation time and the auto "
              "shutoff level — written to the .ofd timestep / solver keys when "
              "the checkbox above is ON");
    return true;
}();

// メッシュ精度 1〜8 の目標解像度 (λ/N) — Lumerical 風プリセットの定義値
const int kLambdaDiv[8] = { 6, 10, 14, 18, 22, 30, 40, 50 };
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
    m_hint = new QLabel(I18n::tr("sreg_hint"), st);   // 文言はドメイン別に切替
    m_hint->setWordWrap(true);
    st->vbox()->addWidget(m_hint);
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
    sr->form()->addRow(tabhelp::unwiredNote(sr, I18n::tr("sreg_uw_region")));
    v->addWidget(sr);

    // ── メッシュ設定 / Mesh ─────────────────────────────────────────────────
    auto *sm = new SectionBox(I18n::tr("sreg_mesh"), body);
    m_meshForm = sm->form();             // ドメイン別の行出し分けに使う
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
    // プリセットの目標解像度 (λ/N @ 代表周波数) はスライダの説明として表示
    m_cellsNote = new QLabel(sm);
    m_cellsNote->setStyleSheet("font-size:11px; color:palette(mid);");
    accRow->addWidget(m_cellsNote);
    sm->form()->addRow(I18n::tr("sreg_mesh_acc"), accRow);

    m_meshHint = new QLabel(sm);
    m_meshHint->setWordWrap(true);
    sm->form()->addRow(m_meshHint);

    // セル数/メモリは Project の実メッシュからの推定 (updateEstimates が更新)
    m_cells = new QLabel(sm);
    sm->form()->addRow(I18n::tr("sreg_cells"), m_cells);
    m_memory = new QLabel(sm);
    sm->form()->addRow(I18n::tr("sreg_memory"), m_memory);

    auto *estNote = new QLabel(I18n::tr("sreg_est_note"), sm);
    estNote->setWordWrap(true);
    estNote->setStyleSheet("font-size:11px; color:palette(mid);");
    sm->form()->addRow(estNote);

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
    sm->form()->addRow(tabhelp::unwiredNote(sm, I18n::tr("sreg_uw_mesh")));
    v->addWidget(sm);

    // ── シミュレーション時間 / Simulation time ──────────────────────────────
    auto *ss = new SectionBox(I18n::tr("sreg_simtime"), body);
    m_timeForm = ss->form();             // ドメイン別の行出し分けに使う
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

    // Δt = CFL 係数 × Courant 限界 / ステップ数 = 時間 ÷ Δt (updateEstimates)
    m_dtRow = new QHBoxLayout();
    m_dtVal = new QLabel(ss);
    m_dtSteps = new QLabel(ss);
    m_stable = new QLabel(ss);
    m_dtRow->addWidget(m_dtVal);
    m_dtRow->addWidget(m_dtSteps);
    m_dtRow->addWidget(m_stable);
    m_dtRow->addStretch(1);
    ss->form()->addRow(QString::fromUtf8("Δt (CFL)"), m_dtRow);

    m_cflRow = new QHBoxLayout();
    m_cfl = new QLineEdit("0.99", ss);
    m_cfl->setMaximumWidth(70);
    m_cflRow->addWidget(m_cfl);
    m_cflRow->addWidget(new QLabel(I18n::tr("sreg_cfl_hint"), ss));
    m_cflRow->addStretch(1);
    ss->form()->addRow(I18n::tr("sreg_cfl"), m_cflRow);

    // この節を .ofd (timestep / solver) へ書くかどうか。既定 OFF =
    // GeneralOpts に触らない = 出力バイト列は従来どおり (絶対規則 2)。
    m_applyTime = new QCheckBox(I18n::tr("sreg_apply_time"), ss);
    ss->form()->addRow(m_applyTime);
    auto *applyHint = new QLabel(I18n::tr("sreg_apply_time_hint"), ss);
    applyHint->setWordWrap(true);
    applyHint->setStyleSheet("color:#666; font-size:11px;");
    ss->form()->addRow(applyHint);
    ss->form()->addRow(tabhelp::unwiredNote(ss, I18n::tr("sreg_uw_dt"),
                                            I18n::tr("sreg_uw_dt_ok")));
    v->addWidget(ss);

    // ── 境界条件 / Boundary conditions ──────────────────────────────────────
    auto *sb = new SectionBox(I18n::tr("sreg_bc"), body);
    m_bcBox = sb;                        // PML/PEC は音響系ではセクション毎隠す
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
    for (int c = 0; c < 6; ++c) {        // 中身は updateEstimates が導出する
        auto *it = new QTableWidgetItem;
        it->setTextAlignment(Qt::AlignCenter);
        m_bcTable->setItem(0, c + 1, it);
    }
    sb->form()->addRow(m_bcTable);
    // .ofd は面別の個別 BC を持たない — abc/pbc からの導出表示であることを注記
    auto *bcDerived = new QLabel(I18n::tr("sreg_bc_derived"), sb);
    bcDerived->setWordWrap(true);
    bcDerived->setStyleSheet("font-size:11px; color:palette(mid);");
    sb->form()->addRow(bcDerived);

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
    // CFL 係数・シミュレーション時間は Δt/ステップ数表示に効き、
    // 「計算へ反映する」が ON のときは GeneralOpts へも書く
    connect(m_cfl, &QLineEdit::textChanged, this,
            [this] { updateEstimates(); applyTime(); });
    connect(m_simTime, &QLineEdit::textChanged, this,
            [this] { updateEstimates(); applyTime(); });
    connect(m_shutoffOn, &QCheckBox::toggled, this, [this] { applyTime(); });
    connect(m_shutoffLevel, &QLineEdit::textChanged, this,
            [this] { applyTime(); });
    connect(m_applyTime, &QCheckBox::toggled, this, [this] { applyTime(); });
    connect(project, &Project::domainChanged, this,
            [this] { updateDomainDeps(); });
    // メッシュ/全般設定の編集 (他タブ含む) で実推定を追従させる。
    // Δt / 反復回数 / 収束条件は「全般」タブも書くので、そちらの変更も
    // 取り込む (このタブの欄にフォーカスがあるあいだは上書きしない —
    // CFL 欄は 1 打鍵ごとに applyTime() が走るため、書式化されると入力できない)
    connect(project, &Project::changed, this, [this] {
        updateEstimates();
        if (m_updating) return;
        QWidget *f = QApplication::focusWidget();
        if (f && (f == this || isAncestorOf(f))) return;
        refresh();
    });
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

// シミュレーション時間の節 → .ofd の timestep / solver。
//
//   timestep = CFL 係数 × Courant 限界 (Project::courantDt() はカーネルの
//              sol/setup.c:setup_timestep() と同じ式)
//   solver   = <反復回数> <出力間隔> <収束条件>
//              反復回数 = シミュレーション時間 ÷ Δt、収束条件 = シャットオフ
//              レベル (シャットオフ OFF なら 0 = 打ち切らない。カーネルは
//              fsum < fmax*converg で判定するので 0 は決して成立しない)
//
// チェックが OFF のあいだは GeneralOpts に一切書かない。CFL 欄は refresh() が
// 丸めた文字列を入れるので、**利用者が実際に編集したときだけ** Δt を作り直す
// (他の欄をいじっただけで読み込んだ timestep が丸め値に化けるのを防ぐ)。
void SolverRegionTab::applyTime()
{
    if (m_updating || !m_applyTime->isChecked()) return;

    const Domain d = m_p->activeDomain();
    if (d != Domain::EM && d != Domain::Optical) return;  // 行ごと隠している

    GeneralOpts &g = m_p->general();

    bool okCfl = false;
    const double cfl = m_cfl->text().toDouble(&okCfl);
    const double dt0 = m_p->courantDt();
    if (okCfl && cfl > 0 && dt0 > 0 && m_cfl->text() != m_cflShown)
        g.dt = cfl * dt0;

    // 反復回数は「今の Δt」から数える (g.dt が 0 = 自動なら Courant 限界)
    const double dt = (g.dt > 0) ? g.dt : dt0;
    bool okT = false;
    const double t = m_simTime->text().toDouble(&okT);
    if (okT && t > 0 && dt > 0) {
        const double steps = t * m_simTimeScale / dt;
        if (steps >= 1.0 && steps < 1e9)
            g.maxiter = int(steps + 0.5);
    }

    bool okLv = false;
    const double lv = m_shutoffLevel->text().toDouble(&okLv);
    g.converg = (m_shutoffOn->isChecked() && okLv && lv > 0) ? lv : 0.0;

    m_p->touch();
}

void SolverRegionTab::refresh()
{
    m_updating = true;
    m_pmlLayers->setValue(m_p->general().pmlL);
    updateDomainDeps();          // m_simTimeScale を先に確定させる (時間欄の単位)

    // 既に timestep が入っているファイルは「反映する」状態として表示する
    // (書いてあるのに OFF と見せると、画面と .ofd が食い違う)
    const GeneralOpts &g = m_p->general();
    const double dt0 = m_p->courantDt();
    m_applyTime->setChecked(g.dt > 0);
    if (g.dt > 0 && dt0 > 0) {
        m_cflShown = QString::number(g.dt / dt0, 'g', 6);
        m_cfl->setText(m_cflShown);
        const double t = g.maxiter * g.dt / m_simTimeScale;
        if (t > 0) m_simTime->setText(QString::number(t, 'g', 6));
    } else {
        m_cflShown.clear();
    }
    m_shutoffOn->setChecked(g.converg > 0);
    if (g.converg > 0)
        m_shutoffLevel->setText(QString::number(g.converg, 'g', 6));

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
    updateDomainDeps();
}

void SolverRegionTab::updateDomainDeps()
{
    const Domain d = m_p->activeDomain();
    const int a = m_meshAcc->value();

    // 目標解像度注記の基準周波数/波長・時間単位・秒換算係数 (ドメイン別の代表値)
    QString ref, unit;
    switch (d) {
    case Domain::Optical:
        ref = QStringLiteral("1550nm");  unit = QStringLiteral("fs");
        m_simTimeScale = 1e-15; break;
    case Domain::Acoustic:               // 室内音響: 可聴帯域の代表値
        ref = QStringLiteral("1kHz");    unit = QStringLiteral("ms");
        m_simTimeScale = 1e-3;  break;
    case Domain::Underwater:             // 水中音響: ソナー帯域の代表値
        ref = QStringLiteral("3.5kHz");  unit = QStringLiteral("s");
        m_simTimeScale = 1.0;   break;
    default:                             // EM
        ref = QStringLiteral("2.5GHz");  unit = QStringLiteral("ns");
        m_simTimeScale = 1e-9;  break;
    }
    m_cellsNote->setText(I18n::tr("sreg_lambda_note")
        .arg(kLambdaDiv[a - 1]).arg(ref));
    m_simTimeUnit->setText(unit);

    // 電磁 FDTD (EM/光) 固有の項目は音響系ドメインでは隠す (混乱防止)。
    // 隠すだけでモデル書き込み (apply の pmlL) は従来どおり行う。
    const bool em = (d == Domain::EM || d == Domain::Optical);
    m_hint->setText(I18n::tr(em ? "sreg_hint" : "sreg_hint_ac"));
    m_meshForm->setRowVisible(m_meshRefine, em);   // Conformal/Yu-Mittra
    m_meshForm->setRowVisible(m_subpixel, em);     // サブピクセル平均
    m_timeForm->setRowVisible(m_dtRow, em);        // Δt (CFL) + 安定バッジ
    m_timeForm->setRowVisible(m_cflRow, em);       // CFL 係数
    m_bcBox->setVisible(em);                       // PML/PEC 境界条件セクション

    updateEstimates();                             // 単位換算が変わるので再計算
}

// Project の実データからの推定表示: セル数/メモリ (実メッシュ)、
// Δt = CFL 係数 × Project::courantDt()、ステップ数 = シミュレーション時間 ÷ Δt、
// 面別 BC 表 = GeneralOpts の abc/pbc からの導出。
// ラベル/テーブル表示のみ更新し、モデルへは書き込まない。
void SolverRegionTab::updateEstimates()
{
    // ── セル数 / メモリ: Project の実メッシュから ──────────────────────────
    const QLocale loc(QLocale::English);
    m_cells->setText(loc.toString(qlonglong(m_p->totalCells())));
    const double mb = m_p->estimatedMemoryMB();
    m_memory->setText(mb >= 1024.0
        ? QStringLiteral("%1 GB").arg(mb / 1024.0, 0, 'f', 1)
        : QStringLiteral("%1 MB").arg(mb, 0, 'f', (mb < 10.0) ? 1 : 0));

    // ── Δt / ステップ数 / 安定バッジ ────────────────────────────────────────
    bool okCfl = false;
    const double cfl = m_cfl->text().toDouble(&okCfl);
    const double dt0 = m_p->courantDt();     // Courant 安定限界 (CFL=1 の Δt)
    if (!okCfl || cfl <= 0 || dt0 <= 0) {
        m_dtVal->setText(I18n::tr("sreg_dt_none"));
        m_dtSteps->clear();
        m_stable->clear();
    } else {
        const double dt = cfl * dt0;
        m_dtVal->setText(QStringLiteral("%1 s").arg(QString::number(dt, 'e', 2)));
        bool okT = false;
        const double t = m_simTime->text().toDouble(&okT);   // 単位はドメイン別
        const double steps = okT && t > 0 ? t * m_simTimeScale / dt : 0;
        if (steps >= 1.0 && steps < 9e15)
            m_dtSteps->setText(I18n::tr("sreg_dt_steps_fmt")
                .arg(loc.toString(qlonglong(steps + 0.5))));
        else
            m_dtSteps->clear();
        const bool stable = (cfl <= 1.0);    // Courant 条件: CFL ≤ 1 で安定
        m_stable->setText(I18n::tr(stable ? "sreg_stable" : "sreg_unstable"));
        m_stable->setStyleSheet(stable ? "color:#2E8B57; font-weight:600;"
                                       : "color:#C42B1C; font-weight:600;");
    }

    // ── 面別 BC 表: abc (0=Mur-1, 1=PML) と pbcX/Y/Z からの導出表示 ─────────
    const GeneralOpts &g = m_p->general();
    const QString abcName = (g.abc == 1) ? QStringLiteral("PML")
                                         : QStringLiteral("Mur-1");
    const bool pbc[6] = { g.pbcX, g.pbcX, g.pbcY, g.pbcY, g.pbcZ, g.pbcZ };
    for (int c = 0; c < 6; ++c) {
        QTableWidgetItem *it = m_bcTable->item(0, c + 1);
        if (!it) continue;
        it->setText(pbc[c] ? I18n::tr("sreg_bc_pbc") : abcName);
        // 周期境界の面はアクセント表示 (既定の吸収境界と区別)
        it->setForeground(pbc[c] ? QBrush(QColor("#0078D4")) : QBrush());
        QFont f = it->font();
        f.setBold(pbc[c]);
        it->setFont(f);
    }
}
