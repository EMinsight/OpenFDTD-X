// OptimizeTab.cpp
#include "OptimizeTab.h"
#include "../core/DensityField.h"
#include "../core/ParetoFront.h"
#include "../core/Project.h"
#include "../widgets/FieldHeatmap.h"
#include "../widgets/MiniPlot.h"
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
#include <QStringList>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <limits>

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
              "チェックすると各点で 2 つ目の評価量も計算し、どちらかを良くすると"
              "もう片方が悪くなる境目 (非劣解集合) を結果表と下の図に出します。",
              "When checked, a second figure of merit is evaluated at every "
              "point and the non-dominated set — the trade-off boundary — is "
              "marked in the result table and drawn below.");
    I18n::reg("opz_fom_kind2", "第 2 の評価量", "Second figure of merit");
    I18n::reg("opz_col_fom2", "第 2 の評価量", "FoM 2");
    I18n::reg("opz_col_front", "非劣解", "Non-dominated");
    I18n::reg("opz_front_yes", "はい", "yes");
    I18n::reg("opz_front_no", "—", "-");
    I18n::reg("opz_pareto_plot", "Pareto フロント", "Pareto front");
    I18n::reg("opz_pareto_note",
              "全 %1 点のうち非劣解は %2 点。図はフロントを「%3」の順に並べた"
              "もので、%4 が単調に入れ替わる (これがトレードオフそのもの)。"
              "ハイパーボリューム (最も悪い点を参照点にした改善面積) は %5。",
              "%2 of the %1 points are non-dominated. The plot orders the front "
              "by \"%3\"; \"%4\" then varies monotonically — that is the "
              "trade-off itself. The hypervolume (improvement area against the "
              "worst point as reference) is %5.");
    I18n::reg("opz_pareto_none",
              "非劣解を作れる点がありません (2 つの評価量が両方とも取れた点が"
              "要ります)。",
              "There is no point from which a front can be built — a point needs "
              "both figures of merit to be evaluated.");
    I18n::reg("opz_pareto_same",
              "2 つの評価量に同じものが選ばれているので、非劣解は最良点 1 つに"
              "なります。違う評価量を選んでください。",
              "The two figures of merit are the same, so the front collapses to "
              "the single best point. Pick a different second one.");
    I18n::reg("opz_local", "ローカル", "Local");
    I18n::reg("opz_cluster", "HPC クラスター", "HPC cluster");
    I18n::reg("opz_tidy3d", "☁ tidy3d クラウド", "☁ tidy3d cloud");
    I18n::reg("opz_uw_method",
              "「随伴」(カーネルが感度を返さないため) と「ベイズ」(代理モデルが"
              "未実装) と「トポロジー」(密度場のパラメータ化はできますが、"
              "反復を回す感度が取れません) の選択",
              "the adjoint method (the kernel returns no sensitivities), "
              "Bayesian optimisation (no surrogate model) and topology "
              "optimisation (the density parametrisation works, but there are no "
              "sensitivities to iterate with)");
    I18n::reg("opz_uw_method_ok",
              "「掃引」「PSO」「GA」— 下の「実行」から実際にカーネルを回します "
              "(PSO / GA は 1 世代ぶんをまとめて実行し、評価量で採点して次の"
              "世代を作ります)",
              "sweep, PSO and GA — they actually run the kernel from Run below "
              "(PSO and GA execute one generation as a batch, score it with the "
              "figure of merit and build the next generation)");
    I18n::reg("opz_uw_vars",
              "変数名と単位の列 (自由記入のラベルです — 計算に使うのは"
              "「対象量」の列)",
              "the variable-name and unit columns (free-form labels — the "
              "quantity column is what the run actually uses)");
    I18n::reg("opz_uw_vars_ok",
              "対象量・初期値・最小 / 最大 / 分割の列 — 掃引の範囲と、"
              "PSO / GA の設計変数 (初期値は 1 個体目の出発点) になります",
              "the quantity, initial-value and min / max / divisions columns — "
              "they become the sweep range and the PSO / GA design variables "
              "(the initial value seeds the first individual)");
    I18n::reg("opz_c_quant", "対象量", "Quantity");
    I18n::reg("opz_q_theta", "平面波 θ [deg]", "Plane wave theta [deg]");
    I18n::reg("opz_q_phi",   "平面波 φ [deg]", "Plane wave phi [deg]");
    I18n::reg("opz_q_mesh",  "メッシュ分割の倍率", "Mesh division factor");
    I18n::reg("opz_q_eps1",  "材料#1 εr への加算", "Material #1 epsr offset");
    I18n::reg("opz_q_eps2",  "材料#2 εr への加算", "Material #2 epsr offset");
    I18n::reg("opz_opt_hint",
              "PSO / GA はここから実際にカーネルを回します。1 世代ぶんの設計点を"
              "まとめて実行し、上の評価量で採点して次の世代を作ります。"
              "動かせるのは「対象量」の列に出ている量だけです "
              "(形状寸法を動かすには掃引エンジン側の対応が要ります)。",
              "PSO and GA run the kernel from here. Each generation is executed "
              "as a batch, scored with the figure of merit above, and used to "
              "build the next generation. Only the quantities offered in the "
              "quantity column can be varied (varying geometry needs support in "
              "the sweep engine).");
    I18n::reg("opz_opt_cost",
              "総ジョブ数 = 個体数 %1 × 世代 %2 = %3 回のカーネル実行",
              "Total jobs = population %1 x generations %2 = %3 kernel runs");
    I18n::reg("opz_opt_running",
              "世代 %1 / %2 — %3 / %4 点",
              "Generation %1 / %2 - point %3 / %4");
    I18n::reg("opz_opt_done", "最適化終了 (%1 世代 / 有効評価 %2 点)",
              "Optimisation finished (%1 generations, %2 valid evaluations)");
    I18n::reg("opz_opt_stopped", "最適化を中止しました (%1 世代 / 有効評価 %2 点)",
              "Optimisation stopped (%1 generations, %2 valid evaluations)");
    I18n::reg("opz_opt_best", "最良: %1 → %2 = %3",
              "Best: %1 -> %2 = %3");
    I18n::reg("opz_opt_none",
              "有効な評価が 1 点もありませんでした (カーネルの出力を確認してください)",
              "No evaluation succeeded (check the kernel output)");
    I18n::reg("opz_opt_need",
              "チェックした行に「対象量」と数値の最小 / 最大が要ります "
              "(最小 < 最大)。個体数は 2 以上、世代は 1 以上。",
              "A checked row needs a quantity and numeric min / max "
              "(min < max). Population must be at least 2 and generations at "
              "least 1.");
    I18n::reg("opz_opt_notrun",
              "この手法 (%1) には最適化ループがありません。掃引 / PSO / GA が"
              "実行できます。",
              "There is no optimisation loop for this method (%1). Sweep, PSO "
              "and GA can run.");
    I18n::reg("opz_run_optimize2", "▶ 最適化を実行", "▶ Run optimisation");
    I18n::reg("opz_uw_con", "制約条件の設定",
              "the constraint settings");
    I18n::reg("opz_uw_topo",
              "トポロジー最適化の反復そのもの (カーネルが感度 ∂FoM/∂ρ を返さない"
              "ので随伴法が組めず、画素数ぶんの設計変数を PSO / GA で回すのは"
              "現実的ではありません)",
              "the topology-optimisation iteration itself (the kernel returns no "
              "sensitivity dFoM/drho, so no adjoint method is possible, and "
              "running one design variable per pixel through PSO / GA is not "
              "practical)");
    I18n::reg("opz_uw_topo_ok",
              "設計領域・解像度・フィルタ半径・射影 β・閾値 η — 密度場の"
              "パラメータ化 (core/DensityField) に入り、下の「密度場を形状へ変換」で"
              "実際の直方体ユニットになります",
              "the design region, resolution, filter radius, projection beta and "
              "threshold eta — they drive the density parametrisation "
              "(core/DensityField), and the button below turns the field into "
              "actual box units");
    // 密度場パラメータ化 (トポロジー最適化)
    I18n::reg("opz_topo_org", "設計領域の原点 x0 / y0 / z0",
              "Design-region origin x0 / y0 / z0");
    I18n::reg("opz_topo_size", "設計領域の 幅 / 奥行 / 厚み",
              "Design-region width / depth / thickness");
    I18n::reg("opz_topo_beta", "射影の急峻さ β", "Projection sharpness beta");
    I18n::reg("opz_topo_eta", "閾値 η", "Threshold eta");
    I18n::reg("opz_topo_mat", "構造材料の番号", "Structure material index");
    I18n::reg("opz_topo_grid", "画素数 / 設計変数",
              "Pixel count / design variables");
    I18n::reg("opz_topo_grid_fmt", "%1 × %2 画素 = %3 変数 (ピッチ %4 × %5 nm)",
              "%1 x %2 pixels = %3 variables (pitch %4 x %5 nm)");
    I18n::reg("opz_topo_feat", "最小形状寸法 (フィルタ直径)",
              "Minimum feature size (filter diameter)");
    I18n::reg("opz_topo_fill", "初期密度場 (現在の形状から)",
              "Initial density field (from the current geometry)");
    I18n::reg("opz_topo_fill_fmt",
              "充填率 %1 % / 非離散度 M_nd = %2 / 閾値以上の矩形 %3 個",
              "fill %1 % / non-discreteness M_nd = %2 / %3 rectangles above the "
              "threshold");
    I18n::reg("opz_topo_skip",
              "内外判定を持たない形状 %1 個 (三角柱・角錐台・円錐台) は密度場に"
              "入れていません。",
              "%1 unit(s) whose shape has no inside test (prisms and frusta) are "
              "not included in the density field.");
    I18n::reg("opz_topo_small",
              "フィルタ半径が画素ピッチより小さいので、フィルタは何もしません "
              "(最小形状寸法は画素 1 個ぶんのままです)。",
              "The filter radius is smaller than the pixel pitch, so the filter "
              "does nothing (the minimum feature size stays one pixel).");
    I18n::reg("opz_topo_big",
              "設計変数が %1 個あります。感度を使わない手法では現実的な数では"
              "ありません (プレビューと形状変換は動きます)。",
              "There are %1 design variables — not a practical number for a "
              "method without sensitivities (the preview and the conversion "
              "still work).");
    I18n::reg("opz_topo_toomany",
              "画素が %1 個あります。上限 %2 個を超えるので密度場は作りません "
              "(解像度を粗くするか設計領域を小さくしてください)。",
              "There are %1 pixels, above the %2 limit, so the density field is "
              "not built (use a coarser resolution or a smaller design region).");
    I18n::reg("opz_topo_bad",
              "設計領域か解像度が不正です (幅・奥行・厚み・解像度はすべて正の値)。",
              "The design region or the resolution is invalid (width, depth, "
              "thickness and resolution must all be positive).");
    I18n::reg("opz_topo_preview", "フィルタ + 射影後の密度場",
              "Density field after filter + projection");
    I18n::reg("opz_topo_apply", "密度場を形状へ変換",
              "Convert the density field into geometry");
    I18n::reg("opz_topo_apply_hint",
              "設計領域に完全に入っているユニットを、射影後の密度場を閾値 η で"
              "切って作った直方体で置き換えます (元に戻すには取り消しを使って"
              "ください)。",
              "Replaces the units that lie entirely inside the design region with "
              "boxes obtained by thresholding the projected density field at eta "
              "(use undo to revert).");
    I18n::reg("opz_topo_applied", "%1 ユニットを %2 個の直方体で置き換えました。",
              "Replaced %1 unit(s) with %2 box(es).");
    I18n::reg("opz_topo_applied_none",
              "閾値以上の画素が無いので、置き換えるものがありません。",
              "No pixel is above the threshold, so there is nothing to place.");
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
              "▸ 実行できるのは「掃引」「PSO」「GA」です。随伴・ベイズ・"
              "トポロジーは最適化ループが未実装です。",
              "▸ Sweep, PSO and GA can run. The adjoint, Bayesian and topology "
              "methods have no optimisation loop yet.");
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
    I18n::reg("opz_uw_run", "実行先の選択 (ローカル / HPC / tidy3d — "
              "投入経路が無いので常にローカルで回します)",
              "the execution-target selection (local / HPC / tidy3d — there is "
              "no submission path, so runs always happen locally)");
    I18n::reg("opz_uw_run_ok",
              "掃引の実行そのもの (変数表の最小 / 最大 / 分割と上の評価量を使い、"
              "ローカルでカーネルを回します) と、Pareto フロント出力 "
              "(各点で 2 つ目の評価量も計算し、非劣解を表と図に出します)",
              "the sweep run itself — it uses min / max / divisions from the "
              "variable table and the figure of merit above, and runs the kernel "
              "locally — and the Pareto front output, which evaluates a second "
              "figure of merit at every point and marks the non-dominated set in "
              "the table and the plot");
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
    m_params = new QTableWidget(3, 9, sParam);
    m_params->setHorizontalHeaderLabels({ QString(), "#",
        I18n::tr("opz_c_var"), I18n::tr("opz_c_init"), I18n::tr("opz_c_min"),
        I18n::tr("opz_c_max"), I18n::tr("opz_c_div"), I18n::tr("opz_c_unit"),
        I18n::tr("opz_c_quant") });
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
    //
    // 密度場のパラメータ化 (core/DensityField) をここへ配線する。設計領域と
    // 解像度が画素格子を、フィルタ半径が最小形状寸法を、β / η が二値化の
    // 強さを決める。初期密度場は現在の形状を設計領域へラスタ化して作るので、
    // 「この設定でどこまで細かい形が表せるか」が図と数字で読める。
    // 最適化の反復そのものは感度が取れないため未実装 (opz_uw_topo)。
    m_pageTopology = new QWidget(m_hyperSec);
    {
        auto *f = new QFormLayout(m_pageTopology);
        f->setContentsMargins(0, 0, 0, 0);

        auto *orgRow = new QHBoxLayout();
        m_topoX0 = numEdit("0", 62, m_pageTopology);
        m_topoY0 = numEdit("0", 62, m_pageTopology);
        m_topoZ0 = numEdit("0", 62, m_pageTopology);
        orgRow->addWidget(m_topoX0);
        orgRow->addWidget(new QLabel("μm", m_pageTopology));
        orgRow->addWidget(m_topoY0);
        orgRow->addWidget(new QLabel("μm", m_pageTopology));
        orgRow->addWidget(m_topoZ0);
        orgRow->addWidget(new QLabel("nm", m_pageTopology));
        orgRow->addStretch(1);
        f->addRow(I18n::tr("opz_topo_org"), orgRow);

        auto *sizeRow = new QHBoxLayout();
        m_topoW = numEdit("5", 62, m_pageTopology);
        m_topoD = numEdit("5", 62, m_pageTopology);
        m_topoT = numEdit("220", 62, m_pageTopology);
        sizeRow->addWidget(m_topoW);
        sizeRow->addWidget(new QLabel("μm", m_pageTopology));
        sizeRow->addWidget(m_topoD);
        sizeRow->addWidget(new QLabel("μm", m_pageTopology));
        sizeRow->addWidget(m_topoT);
        sizeRow->addWidget(new QLabel("nm", m_pageTopology));
        sizeRow->addStretch(1);
        f->addRow(I18n::tr("opz_topo_size"), sizeRow);

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

        auto *projRow = new QHBoxLayout();
        m_topoBeta = numEdit("8", 62, m_pageTopology);
        projRow->addWidget(m_topoBeta);
        projRow->addSpacing(12);
        projRow->addWidget(new QLabel(I18n::tr("opz_topo_eta"), m_pageTopology));
        m_topoEta = numEdit("0.5", 62, m_pageTopology);
        projRow->addWidget(m_topoEta);
        projRow->addStretch(1);
        f->addRow(I18n::tr("opz_topo_beta"), projRow);

        m_topoMat = numEdit("2", 62, m_pageTopology);
        f->addRow(I18n::tr("opz_topo_mat"), m_topoMat);

        m_topoGrid = new QLabel(m_pageTopology);
        m_topoGrid->setStyleSheet(Theme::monoQss());
        f->addRow(I18n::tr("opz_topo_grid"), m_topoGrid);
        m_topoFeat = new QLabel(m_pageTopology);
        m_topoFeat->setStyleSheet(Theme::monoQss());
        f->addRow(I18n::tr("opz_topo_feat"), m_topoFeat);
        m_topoFill = new QLabel(m_pageTopology);
        m_topoFill->setStyleSheet(Theme::monoQss());
        f->addRow(I18n::tr("opz_topo_fill"), m_topoFill);

        m_topoWarn = hintLabel(QString(), m_pageTopology);
        f->addRow(m_topoWarn);

        m_topoMap = new FieldHeatmap(m_pageTopology);
        m_topoMap->setMinimumHeight(190);
        m_topoMap->setTitle(I18n::tr("opz_topo_preview"));
        m_topoMap->setLegend(QStringLiteral("ρ̄"), QString(), QStringLiteral("1.0"));
        f->addRow(m_topoMap);

        m_topoApply = new QPushButton(I18n::tr("opz_topo_apply"), m_pageTopology);
        f->addRow(m_topoApply);
        f->addRow(hintLabel(I18n::tr("opz_topo_apply_hint"), m_pageTopology));

        const QVector<QLineEdit *> edits = { m_topoX0, m_topoY0, m_topoZ0,
                                             m_topoW,  m_topoD,  m_topoT,
                                             m_res,    m_filter,
                                             m_topoBeta, m_topoEta, m_topoMat };
        for (QLineEdit *e : edits)
            connect(e, &QLineEdit::editingFinished, this, &OptimizeTab::updateTopology);
        connect(m_topoApply, &QPushButton::clicked, this, &OptimizeTab::applyTopology);
    }
    m_hyperSec->vbox()->addWidget(m_pageTopology);
    m_hyperSec->vbox()->addWidget(tabhelp::unwiredNote(m_hyperSec, I18n::tr("opz_uw_topo"),
                                                       I18n::tr("opz_uw_topo_ok")));
    v->addWidget(m_hyperSec);

    // ── 実行 / Run ──────────────────────────────────────────────────────────
    auto *sRun = new SectionBox(I18n::tr("opz_run"), body);

    // ── 掃引の実行 (kernel/SweepRunner) ─────────────────────────────────
    // 「掃引」だけが実際にカーネルを回す。振れるのは SweepRunner が対応して
    // いる量に限る (平面波の入射角・メッシュ倍率)。形状寸法を振るには
    // SweepRunner 側の対応が要るので、まだ選択肢に出さない (絶対規則 5)。
    sRun->vbox()->addWidget(hintLabel(I18n::tr("opz_sweep_hint"), sRun));
    sRun->vbox()->addWidget(hintLabel(I18n::tr("opz_opt_hint"), sRun));
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
    m_fomKind2 = new QComboBox(sRun);
    for (const char *k : { "opz_fom_ref", "opz_fom_vswr", "opz_fom_gain",
                           "opz_fom_fb" })
        m_fomKind2->addItem(I18n::tr(k));
    m_fomKind2->setCurrentIndex(2);          // 既定は 1 つ目と別のものにする
    swForm->addRow(I18n::tr("opz_fom_kind2"), m_fomKind2);
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
    // Pareto を外しているときは 2 つ目の評価量を選ぶ意味が無い
    connect(m_pareto, &QCheckBox::toggled, this, [this](bool on) {
        if (m_fomKind2) m_fomKind2->setEnabled(on);
    });
    m_fomKind2->setEnabled(false);
    m_paretoPlot = new MiniPlot(sRun);
    m_paretoPlot->setMinimumHeight(150);
    m_paretoPlot->setVisible(false);
    sRun->vbox()->addWidget(m_paretoPlot);
    m_paretoNote = hintLabel(QString(), sRun);
    m_paretoNote->setVisible(false);
    sRun->vbox()->addWidget(m_paretoNote);
    // 実行先の選択だけがローカル state のまま
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
// 手法キー → 表示名 (未実装の手法を名指しで説明するため)
static QString methodLabel(const QString &mode)
{
    if (mode == QLatin1String("adjoint"))  return I18n::tr("opz_gradient");
    if (mode == QLatin1String("bayes"))    return I18n::tr("opz_bayesian");
    if (mode == QLatin1String("topology")) return I18n::tr("opz_topology");
    if (mode == QLatin1String("pso"))      return I18n::tr("opz_pso");
    if (mode == QLatin1String("ga"))       return I18n::tr("opz_ga");
    return I18n::tr("opz_sweep");
}

void OptimizeTab::startSweep()
{
    if (m_sweeper->isRunning()) {          // 実行中の押下は中止
        m_optStopped = true;                // 最適化なら次の世代へ進めない
        m_sweeper->stop();
        return;
    }
    if (m_mode == QLatin1String("pso") || m_mode == QLatin1String("ga")) {
        startOptimize();
        return;
    }
    if (m_mode != QLatin1String("sweep")) {
        m_runStatus->setText(I18n::tr("opz_opt_notrun").arg(methodLabel(m_mode)));
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

    prepareResultTable();
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
    const bool pareto = m_pareto && m_pareto->isChecked();
    if (pareto) {
        const FomKind kind2 = static_cast<FomKind>(m_fomKind2->currentIndex());
        m_foms2.push_back(evaluateFom(kind2, r, okFreq ? freq : 0.0));
    }
    if (m_optimizing) {
        // 評価できなかった点は NaN で返す (0 で埋めない — 最良に採られる)
        m_genFoms.push_back(fv.valid
                                ? fv.value
                                : std::numeric_limits<double>::quiet_NaN());
    }

    const int row = m_resultTable->rowCount();
    m_resultTable->insertRow(row);
    auto ro = [](const QString &t) {
        auto *it = new QTableWidgetItem(t);
        it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        return it;
    };
    QString shown = r.label.isEmpty() ? QString::number(r.value, 'g', 6)
                                      : r.label;
    if (m_optimizing && m_optimizer) {
        // 「#3」では何を試した点か分からないので設計ベクトルを出す
        const std::vector<std::vector<double>> &pts = m_optimizer->ask();
        if (index >= 0 && index < static_cast<int>(pts.size())) {
            QStringList parts;
            for (int j = 0; j < m_optCols.size()
                            && j < static_cast<int>(pts[index].size()); ++j)
                parts << QStringLiteral("%1=%2").arg(m_optCols[j].label)
                             .arg(QString::number(pts[index][static_cast<size_t>(j)],
                                                  'g', 4));
            if (!parts.isEmpty()) shown = parts.join(QStringLiteral(", "));
        }
    }
    m_resultTable->setItem(row, 0, ro(shown));
    m_resultTable->setItem(row, 1, ro(fv.valid
                                          ? QString::number(fv.value, 'f', 3)
                                          : QStringLiteral("—")));
    int col = 2;
    if (pareto) {
        const FomValue &f2 = m_foms2.back();
        m_resultTable->setItem(row, col++, ro(f2.valid
                                   ? QString::number(f2.value, 'f', 3)
                                   : QStringLiteral("—")));
        // 非劣解かどうかは全点が揃うまで決まらない (ここでは空にしておく)
        m_resultTable->setItem(row, col++, ro(QString()));
    }
    m_resultTable->setItem(row, col, ro(I18n::tr(
        !r.ok ? "opz_state_fail" : (fv.valid ? "opz_state_ok"
                                             : "opz_state_nofom"))));
    m_progress->setValue(index + 1);
    if (m_optimizing && m_optimizer)
        m_runStatus->setText(I18n::tr("opz_opt_running")
                                 .arg(m_optimizer->generation() + 1)
                                 .arg(m_optGens)
                                 .arg(index + 1).arg(m_optPop));
    else
        m_runStatus->setText(I18n::tr("opz_run_running")
                                 .arg(index + 1).arg(m_progress->maximum()));
}

void OptimizeTab::onSweepFinished(bool ok)
{
    if (m_optimizing) {
        m_optimizer->tell(m_genFoms);
        // 中止されたか、世代を使い切ったか、次の世代が作れないなら終わり
        if (!ok && m_optStopped) { finishOptimize(false); return; }
        if (m_optimizer->done() || !runGeneration()) finishOptimize(true);
        return;
    }
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
    updateParetoFront();
    updateRunUi();
}

// ── Pareto フロント (core/ParetoFront) ─────────────────────────────────────
// 2 つの評価量を両方とも取れた点だけで非劣解集合を作り、結果表へ印を付けて
// フロントを図にする。向きは FoM の種別が持っているので `fomMaximizes()` を
// そのまま渡す (呼び出し側で符号を書かない — OptimizeFom.h)。
void OptimizeTab::updateParetoFront()
{
    if (!m_pareto || !m_pareto->isChecked()) return;
    if (m_foms2.size() != m_foms.size()) return;

    const FomKind k1 = static_cast<FomKind>(m_fomKind->currentIndex());
    const FomKind k2 = static_cast<FomKind>(m_fomKind2->currentIndex());
    const bool max1 = fomMaximizes(k1), max2 = fomMaximizes(k2);

    std::vector<pareto::Point> pts;
    pts.reserve(static_cast<size_t>(m_foms.size()));
    for (int i = 0; i < m_foms.size(); ++i) {
        pareto::Point p;
        p.a = m_foms[i].value;
        p.b = m_foms2[i].value;
        p.valid = m_foms[i].valid && m_foms2[i].valid;
        pts.push_back(p);
    }

    const std::vector<int> sorted = pareto::frontSortedByA(pts, max1, max2);
    m_paretoNote->setVisible(true);
    if (sorted.empty()) {
        m_paretoPlot->setVisible(false);
        m_paretoNote->setText(I18n::tr("opz_pareto_none"));
        return;
    }

    // 表へ印を付ける (「非劣解」列は 3 列目)
    std::vector<char> onFront(static_cast<size_t>(m_foms.size()), 0);
    for (int i : sorted) onFront[static_cast<size_t>(i)] = 1;
    for (int r = 0; r < m_resultTable->rowCount()
                    && r < static_cast<int>(onFront.size()); ++r)
        if (auto *it = m_resultTable->item(r, 3))
            it->setText(I18n::tr(onFront[static_cast<size_t>(r)]
                                     ? "opz_front_yes" : "opz_front_no"));

    // 参照点 = 両目的で最も悪い点。ハイパーボリュームはそこからの改善面積
    double worstA = pts[static_cast<size_t>(sorted[0])].a;
    double worstB = pts[static_cast<size_t>(sorted[0])].b;
    for (const pareto::Point &p : pts) {
        if (!p.valid) continue;
        worstA = max1 ? std::min(worstA, p.a) : std::max(worstA, p.a);
        worstB = max2 ? std::min(worstB, p.b) : std::max(worstB, p.b);
    }
    const double hv = pareto::hypervolume(pts, max1, max2, worstA, worstB);

    QVector<MiniSeries> series;
    MiniSeries f;
    f.markers = true;
    f.label = I18n::tr("opz_pareto_plot");
    for (int i : sorted)
        f.pts.push_back(QPointF(pts[static_cast<size_t>(i)].a,
                                pts[static_cast<size_t>(i)].b));
    series.push_back(f);
    m_paretoPlot->setLabels(m_fomKind->currentText(), m_fomKind2->currentText());
    m_paretoPlot->setSeries(series);
    m_paretoPlot->setVisible(true);

    int validCount = 0;
    for (const pareto::Point &p : pts) if (p.valid) ++validCount;
    m_paretoNote->setText(
        (m_fomKind->currentIndex() == m_fomKind2->currentIndex())
            ? I18n::tr("opz_pareto_same")
            : I18n::tr("opz_pareto_note")
                  .arg(validCount)
                  .arg(static_cast<int>(sorted.size()))
                  .arg(m_fomKind->currentText())
                  .arg(m_fomKind2->currentText())
                  .arg(QString::number(hv, 'g', 4)));
}

// 結果表を実行前の状態に戻す。Pareto を出すときだけ「第 2 の評価量」と
// 「非劣解」の列を足す (掃引と最適化ループで同じ手順を踏むための共有部分)。
void OptimizeTab::prepareResultTable()
{
    m_foms.clear();
    m_foms2.clear();
    m_resultTable->setRowCount(0);
    const bool pareto = m_pareto && m_pareto->isChecked();
    m_resultTable->setColumnCount(pareto ? 5 : 3);
    if (pareto)
        m_resultTable->setHorizontalHeaderLabels(
            { I18n::tr("opz_col_value"), I18n::tr("opz_col_fom"),
              I18n::tr("opz_col_fom2"), I18n::tr("opz_col_front"),
              I18n::tr("opz_col_state") });
    else
        m_resultTable->setHorizontalHeaderLabels(
            { I18n::tr("opz_col_value"), I18n::tr("opz_col_fom"),
              I18n::tr("opz_col_state") });
    m_resultTable->setVisible(true);
    m_bestLabel->setVisible(false);
    m_paretoPlot->setVisible(false);
    m_paretoNote->setVisible(false);
}

// 実行ボタンの文言と有効・無効 (掃引 / PSO / GA だけが実行できる)
void OptimizeTab::updateRunUi()
{
    if (!m_runBtn) return;
    const bool running = m_sweeper && m_sweeper->isRunning();
    const bool sweep = (m_mode == QLatin1String("sweep"));
    const bool loop  = (m_mode == QLatin1String("pso")
                     || m_mode == QLatin1String("ga"));
    m_runBtn->setText(I18n::tr(running ? "opz_run_stop"
                                       : (loop ? "opz_run_optimize2"
                                               : "opz_run_optimize")));
    m_runBtn->setEnabled(sweep || loop || running);
    if (!sweep && !loop && !running && m_runStatus->text().isEmpty())
        m_runStatus->setText(I18n::tr("opz_opt_notrun")
                                 .arg(methodLabel(m_mode)));
    // 掃引で振る量の選択は掃引のときだけ意味がある (PSO/GA は変数表の
    // 「対象量」の列を使う)
    if (m_sweepVar) m_sweepVar->setEnabled(!running && sweep);
    for (QWidget *w : { static_cast<QWidget *>(m_fomKind),
                        static_cast<QWidget *>(m_fomFreq) })
        if (w) w->setEnabled(!running);
    if (m_params) m_params->setEnabled(!running);
}

// ── トポロジー: 密度場のパラメータ化 (core/DensityField) ───────────────────
namespace {

// 画面の 3 つの入力欄から設計領域を組み立てる (原点 μm/μm/nm + 大きさ)
topo::Region regionFrom(const QLineEdit *x0, const QLineEdit *y0,
                        const QLineEdit *z0, const QLineEdit *w,
                        const QLineEdit *d, const QLineEdit *t)
{
    topo::Region r;
    r.x0_m = x0->text().toDouble() * 1e-6;
    r.y0_m = y0->text().toDouble() * 1e-6;
    r.z0_m = z0->text().toDouble() * 1e-9;
    r.x1_m = r.x0_m + w->text().toDouble() * 1e-6;
    r.y1_m = r.y0_m + d->text().toDouble() * 1e-6;
    r.z1_m = r.z0_m + t->text().toDouble() * 1e-9;
    return r;
}

// ユニットの外接直方体が設計領域に完全に入っているか (置き換える対象)
bool unitInsideRegion(const Geometry &u, const topo::Region &r)
{
    if (Geometry::paramCount(u.shape) < 6) return false;
    const double xlo = std::min(u.g[0], u.g[1]), xhi = std::max(u.g[0], u.g[1]);
    const double ylo = std::min(u.g[2], u.g[3]), yhi = std::max(u.g[2], u.g[3]);
    const double zlo = std::min(u.g[4], u.g[5]), zhi = std::max(u.g[4], u.g[5]);
    return xlo >= r.x0_m && xhi <= r.x1_m && ylo >= r.y0_m && yhi <= r.y1_m
        && zlo >= r.z0_m && zhi <= r.z1_m;
}

} // namespace

void OptimizeTab::updateTopology()
{
    if (!m_topoGrid) return;

    const topo::Region r = regionFrom(m_topoX0, m_topoY0, m_topoZ0,
                                      m_topoW, m_topoD, m_topoT);
    const double res_m = m_res->text().toDouble() * 1e-9;
    const topo::Grid g = topo::gridFor(r, res_m);
    if (!g.valid()) {
        m_topoGrid->setText(QStringLiteral("—"));
        m_topoFeat->setText(QStringLiteral("—"));
        m_topoFill->setText(QStringLiteral("—"));
        m_topoWarn->setText(I18n::tr("opz_topo_bad"));
        m_topoWarn->setVisible(true);
        m_topoMap->clearData();
        m_topoApply->setEnabled(false);
        return;
    }

    // 画素数の上限。ここを超える格子は GUI スレッドで同期に回すには重い
    // (ラスタ化とフィルタが画素数に比例する)。警告だけ出して回すと固まるので
    // 計算そのものを止める。
    const int kMaxPixels = 250000;   // 500 × 500

    const double radius_m = m_filter->text().toDouble() * 1e-9;
    const double beta = m_topoBeta->text().toDouble();
    const double eta  = m_topoEta->text().toDouble();

    m_topoGrid->setText(I18n::tr("opz_topo_grid_fmt")
                            .arg(g.nx).arg(g.ny).arg(g.count())
                            .arg(g.pitchX_m * 1e9, 0, 'f', 2)
                            .arg(g.pitchY_m * 1e9, 0, 'f', 2));
    m_topoFeat->setText(QStringLiteral("%1 nm")
                            .arg(topo::minFeature_m(radius_m) * 1e9, 0, 'f', 1));

    if (g.count() > kMaxPixels) {
        m_topoFill->setText(QStringLiteral("—"));
        m_topoWarn->setText(I18n::tr("opz_topo_toomany")
                                .arg(g.count()).arg(kMaxPixels));
        m_topoWarn->setVisible(true);
        m_topoMap->clearData();
        m_topoApply->setEnabled(false);
        return;
    }

    int skipped = 0;
    const QVector<Geometry> &units = m_p->geometries();
    const std::vector<double> rho0 =
        topo::rasterize(units.constData(), units.size(), r, g, &skipped);
    const std::vector<double> rho =
        topo::project(topo::filter(rho0, g, radius_m), beta, eta);
    const std::vector<topo::Rect> rects = topo::rectangles(rho, g, eta);

    m_topoFill->setText(I18n::tr("opz_topo_fill_fmt")
                            .arg(topo::volumeFraction(rho) * 100.0, 0, 'f', 1)
                            .arg(topo::nonDiscreteness(rho), 0, 'f', 3)
                            .arg(static_cast<int>(rects.size())));

    QStringList warn;
    if (skipped > 0) warn << I18n::tr("opz_topo_skip").arg(skipped);
    if (radius_m < std::min(g.pitchX_m, g.pitchY_m))
        warn << I18n::tr("opz_topo_small");
    if (g.count() > 20000) warn << I18n::tr("opz_topo_big").arg(g.count());
    m_topoWarn->setText(warn.join(QStringLiteral(" ")));
    m_topoWarn->setVisible(!warn.isEmpty());

    QVector<double> cells;
    cells.reserve(g.count());
    for (double v : rho) cells.push_back(v);
    m_topoMap->setData(cells, g.nx, g.ny);
    m_topoApply->setEnabled(!rects.empty());
}

// 射影後の密度場を閾値 η で切り、設計領域に入っているユニットを
// その直方体分解で置き換える。
void OptimizeTab::applyTopology()
{
    const topo::Region r = regionFrom(m_topoX0, m_topoY0, m_topoZ0,
                                      m_topoW, m_topoD, m_topoT);
    const topo::Grid g = topo::gridFor(r, m_res->text().toDouble() * 1e-9);
    if (!g.valid() || g.count() > 250000) return;   // updateTopology と同じ上限

    const double radius_m = m_filter->text().toDouble() * 1e-9;
    const double eta = m_topoEta->text().toDouble();
    QVector<Geometry> &units = m_p->geometries();
    const std::vector<double> rho =
        topo::project(topo::filter(topo::rasterize(units.constData(), units.size(),
                                                   r, g, nullptr),
                                   g, radius_m),
                      m_topoBeta->text().toDouble(), eta);
    const std::vector<topo::Rect> rects = topo::rectangles(rho, g, eta);
    if (rects.empty()) {
        m_topoWarn->setText(I18n::tr("opz_topo_applied_none"));
        m_topoWarn->setVisible(true);
        return;
    }

    const int matId = m_topoMat->text().toInt();
    QVector<Geometry> kept;
    int removed = 0;
    for (const Geometry &u : units) {
        if (unitInsideRegion(u, r)) { ++removed; continue; }
        kept.push_back(u);
    }
    for (const Geometry &u : topo::toGeometry(rects, r, g, matId))
        kept.push_back(u);
    units = kept;
    m_p->touch();

    // 先に読み直してから結果を出す (updateTopology は注記欄を上書きするため)
    updateTopology();
    m_topoWarn->setText(I18n::tr("opz_topo_applied")
                            .arg(removed).arg(static_cast<int>(rects.size())));
    m_topoWarn->setVisible(true);
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
        // 対象量 — この列だけが実在の量に結び付く (名前の列は自由記入のラベル)
        auto *q = new QComboBox(m_params);
        q->addItem(I18n::tr("opz_q_theta"));   // 0
        q->addItem(I18n::tr("opz_q_phi"));     // 1
        q->addItem(I18n::tr("opz_q_mesh"));    // 2
        q->addItem(I18n::tr("opz_q_eps1"));    // 3
        q->addItem(I18n::tr("opz_q_eps2"));    // 4
        q->setCurrentIndex(r == 0 ? 0 : 1);
        m_params->setCellWidget(r, 8, q);
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
    m_params->setSpan(2, 1, 1, 8);

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
    // 表に出るタイミングで現在の形状から密度場を作り直す (形状は他タブでも
    // 変わるので、開いたときに読み直さないと古い図が残る)
    if (m_mode == "topology" && d == Domain::Optical) updateTopology();
    m_adjointWarnRow->setVisible(d != Domain::Optical);
    m_adjointWarn->setText(I18n::tr("opz_adjoint_warn")
        .arg(domainKey(d).toUpper()));
}

// ── 最適化ループ (PSO / GA) ────────────────────────────────────────────────
// 掃引が「決めた点を順に回す」のに対し、こちらは 1 世代ぶんを回してから
// 評価値を Optimizer へ返して次の世代を作る。1 世代 = SweepRunner の
// samples 1 回ぶん。

bool OptimizeTab::collectOptVars(QVector<SweepColumn> *cols,
                                 std::vector<optim::Variable> *vars) const
{
    cols->clear();
    vars->clear();
    for (int r = 0; r < m_params->rowCount(); ++r) {
        const QTableWidgetItem *ck = m_params->item(r, 0);
        if (!ck || ck->checkState() != Qt::Checked) continue;
        const auto *q = qobject_cast<QComboBox*>(m_params->cellWidget(r, 8));
        const QTableWidgetItem *mn = m_params->item(r, 4);
        const QTableWidgetItem *mx = m_params->item(r, 5);
        if (!q || !mn || !mx) continue;
        bool a = false, b = false;
        const double lo = mn->text().toDouble(&a);
        const double hi = mx->text().toDouble(&b);
        if (!a || !b || !(hi > lo)) continue;

        SweepColumn c;
        switch (q->currentIndex()) {
        case 1: c.param = SweepParam::PlaneWavePhi;    break;
        case 2: c.param = SweepParam::MeshRefine;      break;
        case 3: c.param = SweepParam::MaterialEpsrDelta; c.index = 1; break;
        case 4: c.param = SweepParam::MaterialEpsrDelta; c.index = 2; break;
        default: c.param = SweepParam::PlaneWaveTheta; break;
        }
        c.label = q->currentText();

        optim::Variable v;
        v.lo = lo;
        v.hi = hi;
        const QTableWidgetItem *it = m_params->item(r, 3);
        if (it) {
            bool okInit = false;
            const double x = it->text().toDouble(&okInit);
            if (okInit) { v.init = x; v.hasInit = true; }
        }
        cols->push_back(c);
        vars->push_back(v);
    }
    return !cols->isEmpty();
}

void OptimizeTab::startOptimize()
{
    QVector<SweepColumn> cols;
    std::vector<optim::Variable> vars;
    if (!collectOptVars(&cols, &vars)) {
        m_runStatus->setText(I18n::tr("opz_opt_need"));
        return;
    }
    const int pop = m_pop->text().toInt();
    const int gens = m_iters->text().toInt();

    optim::Options o;
    o.method = (m_mode == QLatin1String("ga")) ? optim::Method::Genetic
                                               : optim::Method::ParticleSwarm;
    o.population = pop;
    o.generations = gens;
    // 向きは FoM の種別が持っている (呼び出し側で書かない — OptimizeFom.h)
    o.maximize = fomMaximizes(static_cast<FomKind>(m_fomKind->currentIndex()));

    auto opt = std::make_unique<optim::Optimizer>(vars, o);
    if (!opt->valid()) {
        m_runStatus->setText(I18n::tr("opz_opt_need"));
        return;
    }
    m_optimizer = std::move(opt);
    m_optCols = cols;
    m_optGens = gens;
    m_optPop = pop;
    m_optimizing = true;
    m_optStopped = false;

    prepareResultTable();
    m_runStatus->setText(I18n::tr("opz_opt_cost")
                             .arg(pop).arg(gens).arg(pop * gens));
    if (!runGeneration()) {
        m_optimizing = false;
        m_optimizer.reset();
        m_runStatus->setText(I18n::tr("opz_opt_need"));
        return;
    }
    updateRunUi();
}

bool OptimizeTab::runGeneration()
{
    if (!m_optimizer || m_optimizer->done()) return false;
    const std::vector<std::vector<double>> &pts = m_optimizer->ask();
    if (pts.empty()) return false;

    SweepConfig cfg;
    cfg.columns = m_optCols;
    cfg.samples.reserve(static_cast<int>(pts.size()));
    for (const std::vector<double> &p : pts) {
        QVector<double> row;
        row.reserve(static_cast<int>(p.size()));
        for (double v : p) row.push_back(v);
        cfg.samples.push_back(row);
    }
    cfg.run = m_runCfg;
    cfg.run.mode = RunMode::Both;      // far1d.log が要る FoM があるため
    cfg.run.kernel = Runner::kernelForProject(*m_p);

    m_genFoms.clear();
    m_progress->setVisible(true);
    m_progress->setRange(0, cfg.samples.size());
    m_progress->setValue(0);
    return m_sweeper->start(*m_p, cfg);
}

void OptimizeTab::finishOptimize(bool ok)
{
    m_progress->setVisible(false);
    const int gens = m_optimizer ? m_optimizer->generation() : 0;
    const int evals = m_optimizer ? m_optimizer->evaluations() : 0;
    m_runStatus->setText(I18n::tr(ok ? "opz_opt_done" : "opz_opt_stopped")
                             .arg(gens).arg(evals));

    if (m_optimizer && m_optimizer->hasBest()) {
        QStringList parts;
        const std::vector<double> &x = m_optimizer->best();
        for (int j = 0; j < m_optCols.size()
                        && j < static_cast<int>(x.size()); ++j)
            parts << QStringLiteral("%1 = %2").arg(m_optCols[j].label)
                         .arg(QString::number(x[static_cast<size_t>(j)], 'g', 6));
        m_bestLabel->setText(I18n::tr("opz_opt_best")
                                 .arg(parts.join(QStringLiteral(", ")),
                                      m_fomKind->currentText(),
                                      QString::number(m_optimizer->bestValue(),
                                                      'f', 3)));
    } else {
        m_bestLabel->setText(I18n::tr("opz_opt_none"));
    }
    m_bestLabel->setVisible(true);

    m_optimizing = false;
    m_optStopped = false;
    updateRunUi();
}
