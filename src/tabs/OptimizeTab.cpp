// OptimizeTab.cpp
#include "OptimizeTab.h"
#include "../core/Project.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"
#include "../Theme.h"
#include "TabHelpers.h"
#include <QProgressBar>
#include "../kernel/SweepRunner.h"
#include "../kernel/Runner.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFont>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

using namespace ofd;

// ── タブ専用語彙 (file-local 登録, 接頭辞 opz_) ─────────────────────────────
namespace {
const bool s_i18n = [] {
    using ofd::I18n;
    I18n::reg("opz_method", "手法", "Method");
    I18n::reg("opz_sweep", "パラメータスイープ", "Parameter sweep");
    I18n::reg("opz_pso", "粒子群最適化 (PSO)", "Particle swarm (PSO)");
    I18n::reg("opz_ga", "遺伝的アルゴリズム", "Genetic algorithm");
    I18n::reg("opz_gradient", "勾配法 (Adjoint)", "Gradient (adjoint)");
    I18n::reg("opz_bayesian", "ベイズ最適化", "Bayesian optimization");
    I18n::reg("opz_topology", "トポロジー最適化 (逆設計)",
              "Topology optimization (inverse design)");
    I18n::reg("opz_hint_sweep", "パラメータを格子状に走査。試験用に最適。",
              "Scans parameters on a grid. Best for exploratory studies.");
    I18n::reg("opz_hint_pso", "粒子群最適化。多峰性で堅牢、勾配情報不要。",
              "Particle swarm: robust on multi-modal problems, no gradients needed.");
    I18n::reg("opz_hint_ga", "遺伝的アルゴリズム。離散変数・トポロジー混合に強い。",
              "Genetic algorithm: strong with discrete variables and mixed topology.");
    I18n::reg("opz_hint_adjoint_opt",
              "随伴感度法による勾配最適化 (未実装)。",
              "Gradient optimization by adjoint sensitivity (not implemented).");
    I18n::reg("opz_hint_adjoint_other",
              "随伴感度法。光以外では一般的でなく実装制限あり。",
              "Adjoint sensitivity. Uncommon outside optics; implementation is limited.");
    I18n::reg("opz_hint_bayes", "ベイズ最適化。少評価回数で目的関数を近似学習。",
              "Bayesian optimization: learns a surrogate of the objective with few runs.");
    I18n::reg("opz_hint_topology",
              "トポロジー最適化。材料分布そのものを最適化変数に (光ICで主流)。",
              "Topology optimization: the material distribution itself becomes the "
              "design variable (mainstream for photonic ICs).");

    I18n::reg("opz_param", "パラメータ", "Parameters");
    I18n::reg("opz_c_var", "変数名", "Variable");
    I18n::reg("opz_c_init", "初期値", "Initial");
    I18n::reg("opz_c_min", "最小", "Min");
    I18n::reg("opz_c_max", "最大", "Max");
    I18n::reg("opz_c_div", "分割", "Steps");
    I18n::reg("opz_c_unit", "単位", "Unit");
    I18n::reg("opz_add_row", "＋ 変数を追加…", "+ Add variable…");
    I18n::reg("opz_jobs", "総ジョブ数:", "Total jobs:");

    I18n::reg("opz_obj", "目的関数 (FoM)", "Objective (FoM)");
    I18n::reg("opz_fom", "FoM 式", "FoM expression");
    I18n::reg("opz_maximize", "最大化", "Maximize");
    I18n::reg("opz_constraint", "制約条件", "Constraints");
    I18n::reg("opz_c_rule", "最小製造ルール 80nm", "Minimum feature rule 80 nm");
    I18n::reg("opz_c_size", "物体寸法上限", "Upper bound on object size");
    I18n::reg("opz_c_thick", "吸音材厚さ上限", "Upper bound on absorber thickness");
    I18n::reg("opz_c_sym", "対称性", "Symmetry");
    I18n::reg("opz_fom_em", "max(|S11(2.4~2.5GHz)|² ) — 帯域内反射の最大値",
              "max(|S11(2.4~2.5GHz)|² ) — worst in-band reflection");
    I18n::reg("opz_fom_opt", "T_drop(λ=1550) - 0.5 × T_thru(λ=1550)",
              "T_drop(λ=1550) - 0.5 × T_thru(λ=1550)");
    I18n::reg("opz_fom_ac", "C80(1kHz) — clarity を最大化",
              "C80(1kHz) — maximize clarity");
    I18n::reg("opz_fom_uw", "min(TL(50km, 100Hz)) — 50km での伝搬損失を最小化",
              "min(TL(50km, 100Hz)) — minimize transmission loss at 50 km");

    I18n::reg("opz_hyper", "ハイパーパラメータ", "Hyper-parameters");
    I18n::reg("opz_pop_size", "個体数", "Population size");
    I18n::reg("opz_iterations", "反復数", "Iterations");
    I18n::reg("opz_lr", "学習率", "Learning rate");
    I18n::reg("opz_warn", "注意:", "Note:");
    I18n::reg("opz_adjoint_warn", "%1ドメインでの随伴法は実験的機能",
              "The adjoint method is experimental in the %1 domain");
    I18n::reg("opz_design_region", "設計領域", "Design region");
    I18n::reg("opz_resolution", "解像度", "Resolution");
    I18n::reg("opz_filter_radius", "フィルタ半径", "Filter radius");

    I18n::reg("opz_run", "実行", "Run");
    I18n::reg("opz_run_optimize", "▶ 最適化実行", "▶ Run optimization");
    I18n::reg("opz_pause", "⏸ 一時停止", "⏸ Pause");
    I18n::reg("opz_stop", "■ 停止", "■ Stop");
    I18n::reg("opz_target", "ジョブ実行先", "Job target");
    // mock i18n の opt_pareto (多目的 FoM の結果ビュー)。en テーブルには
    // 最適化ブロックが無いので英語は "Pareto front" とする。
    I18n::reg("opz_pareto", "Paretoフロント", "Pareto front");
    I18n::reg("opz_pareto_tip",
              "多目的 FoM のとき非劣解集合 (Paretoフロント) を出力する予定 (未実装)。",
              "Planned output of the non-dominated set (Pareto front) for a "
              "multi-objective FoM (not implemented).");
    I18n::reg("opz_local", "ローカル", "Local");
    I18n::reg("opz_cluster", "HPC クラスター", "HPC cluster");
    I18n::reg("opz_tidy3d", "☁ tidy3d クラウド", "☁ tidy3d cloud");
    I18n::reg("opz_uw_method",
              "「掃引」以外の手法 (PSO / 随伴 / GA / ベイズ / トポロジー) の選択",
              "the choice of method other than sweep (PSO / adjoint / GA / "
              "Bayesian / topology)");
    I18n::reg("opz_uw_method_ok",
              "「掃引」— 下の「実行」から実際にカーネルを回します",
              "sweep — it actually runs the kernel from Run below");
    I18n::reg("opz_uw_vars",
              "変数名と初期値の列 (名前はドメイン別の例で、実在の量には"
              "結び付いていません)",
              "the variable-name and initial-value columns (the names are "
              "per-domain examples and are not bound to real quantities)");
    I18n::reg("opz_uw_vars_ok",
              "最小 / 最大 / 分割の列 — 掃引の範囲としてそのまま使われます",
              "the min / max / divisions columns, which are used as the sweep "
              "range");
    I18n::reg("opz_uw_con", "制約条件の設定",
              "the constraint settings");
    I18n::reg("opz_uw_topo", "トポロジー最適化の解像度とフィルタ半径",
              "the topology-optimisation resolution and filter radius");
    I18n::reg("opz_sweep_hint",
              "「掃引」手法はここから実際にカーネルを回します。1 点ずつ .ofd を"
              "書いて実行し、各点の結果から下の評価量を計算して最良点を出します。"
              "振れるのは掃引エンジンが対応している量だけです (形状寸法を振るには"
              "エンジン側の対応が要ります)。",
              "The sweep method actually runs the kernel from here: it writes and "
              "runs one .ofd per point, evaluates the figure of merit below for "
              "each and reports the best point. Only quantities the sweep engine "
              "supports can be varied (varying a geometry dimension needs engine "
              "support).");
    I18n::reg("opz_sweep_var", "振る量", "Variable");
    I18n::reg("opz_var_theta", "平面波の入射角 θ [deg]",
              "plane-wave incidence theta [deg]");
    I18n::reg("opz_var_phi", "平面波の入射角 φ [deg]",
              "plane-wave incidence phi [deg]");
    I18n::reg("opz_var_mesh", "メッシュ分割の倍率 [×]",
              "mesh division factor");
    I18n::reg("opz_fom_kind", "評価量 (FoM)", "Figure of merit");
    I18n::reg("opz_fom_ref", "反射 Ref [dB] — 最小化",
              "reflection Ref [dB] — minimise");
    I18n::reg("opz_fom_vswr", "VSWR — 最小化", "VSWR — minimise");
    I18n::reg("opz_fom_gain", "遠方界の最大 E-abs [dB] — 最大化",
              "peak far-field E-abs [dB] — maximise");
    I18n::reg("opz_fom_fb", "前後比 F/B [dB] — 最大化",
              "front-to-back ratio [dB] — maximise");
    I18n::reg("opz_fom_freq", "評価周波数", "Evaluation frequency");
    I18n::reg("opz_fom_freq_ph", "空 = 各点の最良値", "empty = best per point");
    I18n::reg("opz_col_value", "振った値", "Value");
    I18n::reg("opz_col_fom", "評価量", "FoM");
    I18n::reg("opz_col_state", "状態", "State");
    I18n::reg("opz_run_stop", "■ 中止", "■ Stop");
    I18n::reg("opz_run_need",
              "掃引の範囲が不正です — 変数表の最小 / 最大 / 分割を確認してください "
              "(分割は 2 以上、最小 ≠ 最大)。",
              "The sweep range is invalid — check min / max / divisions in the "
              "variable table (at least 2 divisions, min ≠ max).");
    I18n::reg("opz_run_notsweep",
              "▸ 実行できるのは「掃引」手法だけです。他の手法は最適化ループが"
              "未実装です。",
              "▸ Only the sweep method can run; the other methods have no "
              "optimisation loop yet.");
    I18n::reg("opz_run_running", "実行中: %1 / %2 点", "Running: %1 / %2 points");
    I18n::reg("opz_run_done", "完了: %1 点", "Done: %1 point(s)");
    I18n::reg("opz_run_stopped", "中止しました (%1 点まで)",
              "Stopped after %1 point(s)");
    I18n::reg("opz_best_fmt", "最良点: %1 = %2 のとき %3 = %4",
              "Best point: %3 = %4 at %1 = %2");
    I18n::reg("opz_best_none",
              "最良点はありません — どの点でも評価量が取れませんでした "
              "(給電点や遠方界の出力が必要です)。",
              "No best point — the figure of merit could not be evaluated at any "
              "point (a feed or far-field output is required).");
    I18n::reg("opz_state_ok", "正常", "ok");
    I18n::reg("opz_state_fail", "失敗", "failed");
    I18n::reg("opz_state_nofom", "評価不可", "no FoM");
    I18n::reg("opz_uw_run", "実行先 (ローカル / HPC / tidy3d) と Pareto 出力の選択",
              "the execution target (local / HPC / tidy3d) and the Pareto output "
              "option");
    I18n::reg("opz_uw_run_ok",
              "掃引の実行そのもの — 変数表の最小 / 最大 / 分割と上の評価量を使い、"
              "ローカルでカーネルを回します",
              "the sweep run itself — it uses min / max / divisions from the "
              "variable table and the figure of merit above, and runs the kernel "
              "locally");
    return true;
}();

// mock の defaultParams[domain] をそのまま転記
struct ParamRow { const char *name, *init, *min, *max, *div, *unit; };
const ParamRow kEmParams[2] = {
    { "patch_length", "30.0", "25.0", "35.0", "11", "mm" },
    { "feed_offset",  "5.0",  "3.0",  "7.0",  "11", "mm" },
};
const ParamRow kOptParams[2] = {
    { "ring_radius",  "5.0",  "4.0",  "6.0",  "11", "μm" },
    { "coupling_gap", "200",  "100",  "300",  "11", "nm" },
};
const ParamRow kAcParams[2] = {
    { "absorber_thick", "50", "20", "100", "9", "mm" },
    { "diffuser_depth", "30", "10", "60",  "6", "mm" },
};
const ParamRow kUwParams[2] = {
    { "sonar_depth", "50", "10", "200", "10", "m" },
    { "beam_angle",  "15", "5",  "30",  "6",  "°" },
};

QLabel *makeBadge(const QString &text, const char *kind, QWidget *parent)
{
    auto *b = new QLabel(text, parent);
    QString css = "border-radius:3px; padding:1px 6px; font-size:11px;";
    if (qstrcmp(kind, "ok") == 0)        css += "background:#DFF6DD; color:#0F7B0F;";
    else if (qstrcmp(kind, "warn") == 0) css += "background:#FFF4CE; color:#9D5D00;";
    else if (qstrcmp(kind, "acc") == 0)  css += "background:#DEECF9; color:#0078D4;";
    else                                  css += "background:palette(midlight);";
    b->setStyleSheet(css);
    return b;
}

QLabel *hintLabel(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setWordWrap(true);
    return l;
}

QLineEdit *numEdit(const char *value, int width, QWidget *parent)
{
    auto *e = new QLineEdit(QString::fromUtf8(value), parent);
    if (width > 0) e->setMaximumWidth(width);
    return e;
}
} // namespace

// ── OptimizeTab ─────────────────────────────────────────────────────────────
OptimizeTab::OptimizeTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // ── 手法 / Method (2行の排他ボタン列 = mock の 2 つの Seg) ───────────────
    auto *sMethod = new SectionBox(I18n::tr("opz_method"), body);
    auto addMethodBtn = [this](QHBoxLayout *row, const char *key,
                               const char *mode, QWidget *owner) {
        auto *b = new QPushButton(I18n::tr(key), owner);
        b->setCheckable(true);
        b->setStyleSheet("font-size:11px; padding:2px 8px;");
        b->setProperty("mode", QString::fromUtf8(mode));
        const QString m = QString::fromUtf8(mode);
        connect(b, &QPushButton::clicked, this, [this, m] { setMode(m); });
        row->addWidget(b);
        m_methodBtns.push_back(b);
        return b;
    };
    auto *row1 = new QHBoxLayout();
    row1->setSpacing(4);
    addMethodBtn(row1, "opz_sweep",    "sweep",   sMethod);
    addMethodBtn(row1, "opz_pso",      "pso",     sMethod);
    addMethodBtn(row1, "opz_gradient", "adjoint", sMethod);
    row1->addStretch(1);
    sMethod->vbox()->addLayout(row1);

    auto *row2 = new QHBoxLayout();
    row2->setSpacing(4);
    addMethodBtn(row2, "opz_ga",       "ga",      sMethod);
    addMethodBtn(row2, "opz_bayesian", "bayes",   sMethod);
    m_topologyBtn = addMethodBtn(row2, "opz_topology", "topology", sMethod);
    row2->addStretch(1);
    sMethod->vbox()->addLayout(row2);

    m_methodHint = hintLabel(QString(), sMethod);
    sMethod->vbox()->addWidget(m_methodHint);
    // 手法選択はローカル state のみ (Project へは書き込まれない)
    sMethod->vbox()->addWidget(tabhelp::unwiredNote(sMethod, I18n::tr("opz_uw_method"),
                                     I18n::tr("opz_uw_method_ok")));
    v->addWidget(sMethod);

    // ── パラメータ / Parameters ─────────────────────────────────────────────
    auto *sParam = new SectionBox(I18n::tr("opz_param"), body);
    m_params = new QTableWidget(3, 8, sParam);
    m_params->setHorizontalHeaderLabels({ QString(), "#",
        I18n::tr("opz_c_var"), I18n::tr("opz_c_init"), I18n::tr("opz_c_min"),
        I18n::tr("opz_c_max"), I18n::tr("opz_c_div"), I18n::tr("opz_c_unit") });
    m_params->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_params->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_params->horizontalHeader()->resizeSection(0, 24);
    m_params->verticalHeader()->setVisible(false);
    m_params->setMinimumHeight(120);
    sParam->vbox()->addWidget(m_params);
    m_jobs = new QLabel(sParam);
    sParam->vbox()->addWidget(m_jobs);
    // 変数表はドメイン別の既定例で、編集内容はどこにも読まれない
    sParam->vbox()->addWidget(tabhelp::unwiredNote(sParam, I18n::tr("opz_uw_vars"),
                                     I18n::tr("opz_uw_vars_ok")));
    v->addWidget(sParam);

    // ── 目的関数 (FoM) / Objective ──────────────────────────────────────────
    auto *sObj = new SectionBox(I18n::tr("opz_obj"), body);
    auto *fomRow = new QHBoxLayout();
    m_fom = new QLineEdit(sObj);
    fomRow->addWidget(m_fom, 1);
    fomRow->addWidget(makeBadge(I18n::tr("opz_maximize"), "acc", sObj));
    sObj->form()->addRow(I18n::tr("opz_fom"), fomRow);

    auto *conRow = new QHBoxLayout();
    m_cRuleOpt = new QCheckBox(I18n::tr("opz_c_rule"), sObj);
    m_cRuleOpt->setChecked(true);
    m_cSizeEm = new QCheckBox(I18n::tr("opz_c_size"), sObj);
    m_cSizeEm->setChecked(true);
    m_cThickAc = new QCheckBox(I18n::tr("opz_c_thick"), sObj);
    m_cThickAc->setChecked(true);
    m_cSym = new QCheckBox(I18n::tr("opz_c_sym"), sObj);
    conRow->addWidget(m_cRuleOpt);
    conRow->addWidget(m_cSizeEm);
    conRow->addWidget(m_cThickAc);
    conRow->addWidget(m_cSym);
    conRow->addStretch(1);
    sObj->form()->addRow(I18n::tr("opz_constraint"), conRow);
    sObj->form()->addRow(tabhelp::unwiredNote(sObj, I18n::tr("opz_uw_con")));
    v->addWidget(sObj);

    // ── ハイパーパラメータ / Hyper-parameters (mode != sweep のみ) ───────────
    m_hyperSec = new SectionBox(I18n::tr("opz_hyper"), body);
    // PSO / GA
    m_pagePop = new QWidget(m_hyperSec);
    {
        auto *f = new QFormLayout(m_pagePop);
        f->setContentsMargins(0, 0, 0, 0);
        m_pop = numEdit("40", 70, m_pagePop);
        m_iters = numEdit("100", 70, m_pagePop);
        f->addRow(I18n::tr("opz_pop_size"), m_pop);
        f->addRow(I18n::tr("opz_iterations"), m_iters);
    }
    m_hyperSec->vbox()->addWidget(m_pagePop);
    // Adjoint
    m_pageAdjoint = new QWidget(m_hyperSec);
    {
        auto *f = new QFormLayout(m_pageAdjoint);
        f->setContentsMargins(0, 0, 0, 0);
        m_lr = numEdit("0.02", 70, m_pageAdjoint);
        f->addRow(I18n::tr("opz_lr"), m_lr);
        m_adjointWarnRow = new QWidget(m_pageAdjoint);
        auto *wh = new QHBoxLayout(m_adjointWarnRow);
        wh->setContentsMargins(0, 0, 0, 0);
        wh->addWidget(makeBadge(I18n::tr("opz_warn"), "warn", m_adjointWarnRow));
        m_adjointWarn = hintLabel(QString(), m_adjointWarnRow);
        wh->addWidget(m_adjointWarn, 1);
        f->addRow(m_adjointWarnRow);
    }
    m_hyperSec->vbox()->addWidget(m_pageAdjoint);
    // Topology (光ドメインのみ)
    m_pageTopology = new QWidget(m_hyperSec);
    {
        auto *f = new QFormLayout(m_pageTopology);
        f->setContentsMargins(0, 0, 0, 0);
        auto *region = new QLabel("5 × 5 μm × 220nm", m_pageTopology);
        region->setStyleSheet(Theme::monoQss());
        f->addRow(I18n::tr("opz_design_region"), region);
        auto *resRow = new QHBoxLayout();
        m_res = numEdit("20", 70, m_pageTopology);
        resRow->addWidget(m_res);
        resRow->addWidget(new QLabel("nm/pixel", m_pageTopology));
        resRow->addStretch(1);
        f->addRow(I18n::tr("opz_resolution"), resRow);
        auto *filtRow = new QHBoxLayout();
        m_filter = numEdit("80", 70, m_pageTopology);
        filtRow->addWidget(m_filter);
        filtRow->addWidget(new QLabel("nm", m_pageTopology));
        filtRow->addStretch(1);
        f->addRow(I18n::tr("opz_filter_radius"), filtRow);
    }
    m_hyperSec->vbox()->addWidget(m_pageTopology);
    m_hyperSec->vbox()->addWidget(tabhelp::unwiredNote(m_hyperSec, I18n::tr("opz_uw_topo")));
    v->addWidget(m_hyperSec);

    // ── 実行 / Run ──────────────────────────────────────────────────────────
    auto *sRun = new SectionBox(I18n::tr("opz_run"), body);

    // ── 掃引の実行 (kernel/SweepRunner) ─────────────────────────────────
    // 「掃引」だけが実際にカーネルを回す。振れるのは SweepRunner が対応して
    // いる量に限る (平面波の入射角・メッシュ倍率)。形状寸法を振るには
    // SweepRunner 側の対応が要るので、まだ選択肢に出さない (絶対規則 5)。
    sRun->vbox()->addWidget(hintLabel(I18n::tr("opz_sweep_hint"), sRun));
    auto *swForm = new QFormLayout();
    swForm->setContentsMargins(0, 0, 0, 0);
    m_sweepVar = new QComboBox(sRun);
    m_sweepVar->addItem(I18n::tr("opz_var_theta"));   // 0
    m_sweepVar->addItem(I18n::tr("opz_var_phi"));     // 1
    m_sweepVar->addItem(I18n::tr("opz_var_mesh"));    // 2
    swForm->addRow(I18n::tr("opz_sweep_var"), m_sweepVar);
    m_fomKind = new QComboBox(sRun);
    m_fomKind->addItem(I18n::tr("opz_fom_ref"));      // 0 反射 (最小化)
    m_fomKind->addItem(I18n::tr("opz_fom_vswr"));     // 1 VSWR (最小化)
    m_fomKind->addItem(I18n::tr("opz_fom_gain"));     // 2 最大利得 (最大化)
    m_fomKind->addItem(I18n::tr("opz_fom_fb"));       // 3 前後比 (最大化)
    swForm->addRow(I18n::tr("opz_fom_kind"), m_fomKind);
    m_fomFreq = new QLineEdit(sRun);
    m_fomFreq->setPlaceholderText(I18n::tr("opz_fom_freq_ph"));
    m_fomFreq->setMaximumWidth(140);
    swForm->addRow(I18n::tr("opz_fom_freq"), m_fomFreq);
    sRun->vbox()->addLayout(swForm);

    auto *btnRow = new QHBoxLayout();
    m_runBtn = new QPushButton(I18n::tr("opz_run_optimize"), sRun);
    m_runBtn->setStyleSheet("font-weight:600;");
    connect(m_runBtn, &QPushButton::clicked, this, &OptimizeTab::startSweep);
    auto *pauseBtn = new QPushButton(I18n::tr("opz_pause"), sRun);
    // 一時停止はカーネル実行の中断・再開が要る (Runner に無い) ので未実装
    tabhelp::markNotImplemented(pauseBtn);
    btnRow->addWidget(m_runBtn);
    btnRow->addWidget(pauseBtn);
    btnRow->addStretch(1);
    sRun->vbox()->addLayout(btnRow);

    m_progress = new QProgressBar(sRun);
    m_progress->setVisible(false);
    sRun->vbox()->addWidget(m_progress);
    m_runStatus = hintLabel(QString(), sRun);
    sRun->vbox()->addWidget(m_runStatus);
    m_bestLabel = new QLabel(sRun);
    m_bestLabel->setWordWrap(true);
    m_bestLabel->setStyleSheet("font-weight:600;");
    m_bestLabel->setVisible(false);
    sRun->vbox()->addWidget(m_bestLabel);
    m_resultTable = new QTableWidget(0, 3, sRun);
    m_resultTable->setHorizontalHeaderLabels(
        { I18n::tr("opz_col_value"), I18n::tr("opz_col_fom"),
          I18n::tr("opz_col_state") });
    m_resultTable->horizontalHeader()->setStretchLastSection(true);
    m_resultTable->verticalHeader()->setVisible(false);
    m_resultTable->setMinimumHeight(140);
    m_resultTable->setVisible(false);
    sRun->vbox()->addWidget(m_resultTable);

    m_sweeper = new SweepRunner(this);
    connect(m_sweeper, &SweepRunner::pointFinished,
            this, &OptimizeTab::onPointFinished);
    connect(m_sweeper, &SweepRunner::finished,
            this, &OptimizeTab::onSweepFinished);

    m_target = new QComboBox(sRun);
    sRun->form()->addRow(I18n::tr("opz_target"), m_target);
    // Paretoフロント出力 (mock i18n の opt_pareto)。対応する Project フィールドが
    // 無いためローカル state のみ。
    m_pareto = new QCheckBox(I18n::tr("opz_pareto"), sRun);
    m_pareto->setToolTip(I18n::tr("opz_pareto_tip"));
    sRun->vbox()->addWidget(m_pareto);
    // 実行先・Pareto 出力もローカル state のみ
    sRun->vbox()->addWidget(tabhelp::unwiredNote(sRun, I18n::tr("opz_uw_run"),
                                     I18n::tr("opz_uw_run_ok")));
    v->addWidget(sRun);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    connect(project, &Project::domainChanged, this, &OptimizeTab::rebuildDomain);
    rebuildDomain();
}

void OptimizeTab::setMode(const QString &mode)
{
    m_mode = mode;
    updateMode();
}

// ── 掃引の実行 ──────────────────────────────────────────────────────────────
// 変数表の**最初にチェックの入った行**から最小 / 最大 / 分割を読み、
// SweepRunner に渡す。範囲が不正なら起動せず理由を出す (絶対規則 5)。
void OptimizeTab::startSweep()
{
    if (m_sweeper->isRunning()) {          // 実行中の押下は中止
        m_sweeper->stop();
        return;
    }
    if (m_mode != QLatin1String("sweep")) {
        m_runStatus->setText(I18n::tr("opz_run_notsweep"));
        return;
    }
    // 変数表から範囲を読む
    double from = 0, to = 0;
    int points = 0;
    bool found = false;
    for (int r = 0; r < m_params->rowCount() && !found; ++r) {
        const QTableWidgetItem *ck = m_params->item(r, 0);
        if (!ck || ck->checkState() != Qt::Checked) continue;
        const QTableWidgetItem *mn = m_params->item(r, 4);
        const QTableWidgetItem *mx = m_params->item(r, 5);
        const QTableWidgetItem *dv = m_params->item(r, 6);
        if (!mn || !mx || !dv) continue;
        bool a = false, b = false, c = false;
        const double f = mn->text().toDouble(&a);
        const double t = mx->text().toDouble(&b);
        const int    n = dv->text().toInt(&c);
        if (!a || !b || !c) continue;
        from = f; to = t; points = n; found = true;
    }
    if (!found || points < 2 || from == to) {
        m_runStatus->setText(I18n::tr("opz_run_need"));
        return;
    }

    SweepConfig cfg;
    switch (m_sweepVar->currentIndex()) {
    case 1:  cfg.kind = SweepKind::PlaneWavePhi;   break;
    case 2:  cfg.kind = SweepKind::MeshRefine;     break;
    default: cfg.kind = SweepKind::PlaneWaveTheta; break;
    }
    cfg.from = from;
    cfg.to = to;
    cfg.points = points;
    cfg.run = m_runCfg;
    // 1 点ずつポストまで走らせる (far1d.log が要る FoM があるため)
    cfg.run.mode = RunMode::Both;
    cfg.run.kernel = Runner::kernelForProject(*m_p);

    m_foms.clear();
    m_resultTable->setRowCount(0);
    m_resultTable->setVisible(true);
    m_bestLabel->setVisible(false);
    m_progress->setVisible(true);
    m_progress->setRange(0, cfg.points);
    m_progress->setValue(0);
    if (!m_sweeper->start(*m_p, cfg)) {
        m_progress->setVisible(false);
        m_runStatus->setText(I18n::tr("opz_run_need"));
        return;
    }
    m_runStatus->setText(I18n::tr("opz_run_running").arg(0).arg(cfg.points));
    updateRunUi();
}

void OptimizeTab::onPointFinished(int index, const SweepResult &r)
{
    const FomKind kind = static_cast<FomKind>(m_fomKind->currentIndex());
    bool okFreq = false;
    const double freq = m_fomFreq->text().trimmed().toDouble(&okFreq);
    const FomValue fv = evaluateFom(kind, r, okFreq ? freq : 0.0);
    m_foms.push_back(fv);

    const int row = m_resultTable->rowCount();
    m_resultTable->insertRow(row);
    auto ro = [](const QString &t) {
        auto *it = new QTableWidgetItem(t);
        it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        return it;
    };
    m_resultTable->setItem(row, 0, ro(r.label.isEmpty()
                                          ? QString::number(r.value, 'g', 6)
                                          : r.label));
    m_resultTable->setItem(row, 1, ro(fv.valid
                                          ? QString::number(fv.value, 'f', 3)
                                          : QStringLiteral("—")));
    m_resultTable->setItem(row, 2, ro(I18n::tr(
        !r.ok ? "opz_state_fail" : (fv.valid ? "opz_state_ok"
                                             : "opz_state_nofom"))));
    m_progress->setValue(index + 1);
    m_runStatus->setText(I18n::tr("opz_run_running")
                             .arg(index + 1).arg(m_progress->maximum()));
}

void OptimizeTab::onSweepFinished(bool ok)
{
    m_progress->setVisible(false);
    const QVector<SweepResult> &rs = m_sweeper->results();
    m_runStatus->setText(I18n::tr(ok ? "opz_run_done" : "opz_run_stopped")
                             .arg(rs.size()));

    const FomKind kind = static_cast<FomKind>(m_fomKind->currentIndex());
    const int best = bestPointIndex(kind, m_foms);
    if (best >= 0 && best < rs.size()) {
        m_bestLabel->setText(I18n::tr("opz_best_fmt")
                                 .arg(m_sweepVar->currentText())
                                 .arg(QString::number(rs[best].value, 'g', 6))
                                 .arg(m_fomKind->currentText())
                                 .arg(QString::number(m_foms[best].value,
                                                      'f', 3)));
        m_bestLabel->setVisible(true);
        m_resultTable->selectRow(best);
    } else {
        m_bestLabel->setText(I18n::tr("opz_best_none"));
        m_bestLabel->setVisible(true);
    }
    updateRunUi();
}

// 実行ボタンの文言と有効・無効 (掃引以外では押せない)
void OptimizeTab::updateRunUi()
{
    if (!m_runBtn) return;
    const bool running = m_sweeper && m_sweeper->isRunning();
    const bool sweep = (m_mode == QLatin1String("sweep"));
    m_runBtn->setText(I18n::tr(running ? "opz_run_stop" : "opz_run_optimize"));
    m_runBtn->setEnabled(sweep || running);
    if (!sweep && !running && m_runStatus->text().isEmpty())
        m_runStatus->setText(I18n::tr("opz_run_notsweep"));
    for (QWidget *w : { static_cast<QWidget *>(m_sweepVar),
                        static_cast<QWidget *>(m_fomKind),
                        static_cast<QWidget *>(m_fomFreq) })
        if (w) w->setEnabled(!running);
}

void OptimizeTab::rebuildDomain()
{
    const Domain d = m_p->activeDomain();

    // トポロジー最適化は光ドメインのみ (mock の条件付き Seg 項目)
    const bool optical = (d == Domain::Optical);
    m_topologyBtn->setVisible(optical);
    if (!optical && m_mode == "topology") m_mode = "sweep";

    // ── 変数表 (ドメイン別既定行 + 追加行) ─────────────────────────────────
    const ParamRow *rows = kEmParams;
    switch (d) {
        case Domain::Optical:    rows = kOptParams; break;
        case Domain::Acoustic:   rows = kAcParams;  break;
        case Domain::Underwater: rows = kUwParams;  break;
        default:                 rows = kEmParams;  break;
    }
    m_params->clearSpans();
    m_params->setRowCount(3);
    for (int r = 0; r < 2; ++r) {
        auto *ck = new QTableWidgetItem;
        ck->setCheckState(Qt::Checked);
        ck->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        m_params->setItem(r, 0, ck);
        auto *num = new QTableWidgetItem(QString::number(r + 1));
        num->setFlags(num->flags() & ~Qt::ItemIsEditable);
        m_params->setItem(r, 1, num);
        m_params->setItem(r, 2, new QTableWidgetItem(QString::fromUtf8(rows[r].name)));
        m_params->setItem(r, 3, new QTableWidgetItem(QString::fromUtf8(rows[r].init)));
        m_params->setItem(r, 4, new QTableWidgetItem(QString::fromUtf8(rows[r].min)));
        m_params->setItem(r, 5, new QTableWidgetItem(QString::fromUtf8(rows[r].max)));
        m_params->setItem(r, 6, new QTableWidgetItem(QString::fromUtf8(rows[r].div)));
        auto *unit = new QTableWidgetItem(QString::fromUtf8(rows[r].unit));
        unit->setFlags(unit->flags() & ~Qt::ItemIsEditable);
        m_params->setItem(r, 7, unit);
        for (int c = 3; c <= 6; ++c)
            m_params->item(r, c)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }
    // ＋ 変数を追加… 行
    auto *addCk = new QTableWidgetItem;
    addCk->setCheckState(Qt::Unchecked);
    addCk->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
    m_params->setItem(2, 0, addCk);
    auto *addIt = new QTableWidgetItem(I18n::tr("opz_add_row"));
    addIt->setFlags(addIt->flags() & ~Qt::ItemIsEditable);
    QFont af = addIt->font();
    af.setItalic(true);
    addIt->setFont(af);
    m_params->setItem(2, 1, addIt);
    m_params->setSpan(2, 1, 1, 7);

    // 総ジョブ数 (スイープ時のみ表示 / mock の div0 × div1)
    const int d0 = QString::fromUtf8(rows[0].div).toInt();
    const int d1 = QString::fromUtf8(rows[1].div).toInt();
    m_jobs->setText(QStringLiteral("%1 %2 × %3 = %4")
        .arg(I18n::tr("opz_jobs")).arg(d0).arg(d1).arg(d0 * d1));

    // ── FoM 既定式 / 制約条件 ──────────────────────────────────────────────
    const char *fomKey = (d == Domain::Optical)    ? "opz_fom_opt"
                       : (d == Domain::Acoustic)   ? "opz_fom_ac"
                       : (d == Domain::Underwater) ? "opz_fom_uw"
                                                   : "opz_fom_em";
    m_fom->setText(I18n::tr(fomKey));
    m_cRuleOpt->setVisible(optical);
    m_cSizeEm->setVisible(d == Domain::EM);
    m_cThickAc->setVisible(d == Domain::Acoustic);

    // ── 実行先 (tidy3d は光ドメインのみ) ───────────────────────────────────
    const QString keep = m_target->currentText();
    m_target->clear();
    m_target->addItem(I18n::tr("opz_local"));
    m_target->addItem(I18n::tr("opz_cluster"));
    if (optical) m_target->addItem(I18n::tr("opz_tidy3d"));
    const int idx = m_target->findText(keep);
    m_target->setCurrentIndex(idx >= 0 ? idx : 0);

    updateMode();
}

void OptimizeTab::updateMode()
{
    updateRunUi();
    const Domain d = m_p->activeDomain();
    for (QPushButton *b : m_methodBtns)
        b->setChecked(b->property("mode").toString() == m_mode);

    // 手法ヒント (mock の三項演算子群をそのまま転記)
    const char *hintKey = "opz_hint_sweep";
    if (m_mode == "pso")           hintKey = "opz_hint_pso";
    else if (m_mode == "ga")       hintKey = "opz_hint_ga";
    else if (m_mode == "bayes")    hintKey = "opz_hint_bayes";
    else if (m_mode == "topology") hintKey = "opz_hint_topology";
    else if (m_mode == "adjoint")  hintKey = (d == Domain::Optical)
                                       ? "opz_hint_adjoint_opt"
                                       : "opz_hint_adjoint_other";
    m_methodHint->setText(I18n::tr(hintKey));

    // 総ジョブ数はスイープ時のみ
    m_jobs->setVisible(m_mode == "sweep");

    // ハイパーパラメータ: mode != sweep で表示、内容は手法別
    m_hyperSec->setVisible(m_mode != "sweep");
    m_pagePop->setVisible(m_mode == "pso" || m_mode == "ga");
    m_pageAdjoint->setVisible(m_mode == "adjoint");
    m_pageTopology->setVisible(m_mode == "topology" && d == Domain::Optical);
    m_adjointWarnRow->setVisible(d != Domain::Optical);
    m_adjointWarn->setText(I18n::tr("opz_adjoint_warn")
        .arg(domainKey(d).toUpper()));
}
