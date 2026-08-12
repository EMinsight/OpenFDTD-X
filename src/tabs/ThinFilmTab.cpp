// ThinFilmTab.cpp
#include "ThinFilmTab.h"
#include "TabHelpers.h"
#include "../core/GlassCatalog.h"
#include "../core/Project.h"
#include "../optics/FilmNotation.h"
#include "../optics/MaterialDispersion.h"
#include "../optics/ThinFilmStack.h"
#include "../widgets/MiniPlot.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"
#include "../Theme.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFont>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStringList>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <map>
#include <cmath>
#include <utility>
#include <vector>

using namespace ofd;

// ── タブ専用語彙 (file-local 登録, 接頭辞 tfc_) ──────────────────────────────
namespace {
const bool s_i18n = [] {
    using ofd::I18n;
    // 上段
    I18n::reg("tfc_title", "薄膜多層膜設計 / Multilayer coating design",
              "Multilayer coating design");
    I18n::reg("tfc_hint",
              "特性行列法 (Abeles 行列) で R/T/A・反射位相・群遅延を厳密計算する。"
              "層構成・波長・入射角の編集は即座に反映される。"
              "膜厚の最適化はシンプレックス法で行える "
              "(層数・材料を変えるニードル法ほかは未実装)。",
              "Reflectance, transmittance, absorptance, reflection phase and "
              "group delay are computed exactly with the characteristic-matrix "
              "(Abeles) method. Edits to the stack, wavelength and angle apply "
              "immediately. Thicknesses can be optimised with the simplex "
              "method; the needle method and other algorithms that change the "
              "layer count or materials are not implemented.");
    I18n::reg("tfc_preset", "プリセット", "Preset");
    I18n::reg("tfc_layers_n", "%1 層", "%1 layers");
    I18n::reg("tfc_target_fmt", "目標: %1", "Target: %1");
    I18n::reg("tfc_preset_note",
              "▸ プリセットは λ₀ における四分の一波長 (QWOT) 起点の設計であり、"
              "上の「目標」を満たすよう最適化した製品設計ではない。"
              "達成度は「分光特性」タブの指標と「最適化設計」タブの Merit で確認する。",
              "▸ Each preset is a quarter-wave (QWOT) starting design at λ₀, not a "
              "design optimised to meet the target above. Check the metrics on the "
              "Spectral response tab and the merit value on the Optimization tab.");

    // プリセット名
    I18n::reg("tfc_p_ar",   "ARコート (可視域)", "AR coating (visible)");
    I18n::reg("tfc_p_dbr",  "DBRミラー", "DBR mirror");
    I18n::reg("tfc_p_bpf",  "バンドパスフィルタ", "Bandpass filter");
    I18n::reg("tfc_p_dich", "ダイクロイックミラー", "Dichroic mirror");
    I18n::reg("tfc_p_heat", "熱線反射 (Low-E)", "Heat-reflective (Low-E)");
    I18n::reg("tfc_p_pol",  "薄膜偏光子 (MacNeille)",
              "Thin-film polarizer (MacNeille)");
    // プリセットの目標仕様
    I18n::reg("tfc_t_ar",   "R < 0.5% @ 450-650nm", "R < 0.5% @ 450-650nm");
    I18n::reg("tfc_t_dbr",  "R > 99.9% @ 1550nm", "R > 99.9% @ 1550nm");
    I18n::reg("tfc_t_bpf",  "T>90% @ 1530-1570, OD>4 外側",
              "T>90% @ 1530-1570, OD>4 outside");
    I18n::reg("tfc_t_dich", "R>95% <500nm, T>95% >550nm",
              "R>95% <500nm, T>95% >550nm");
    I18n::reg("tfc_t_heat", "可視T>70%, 赤外R>85%", "Visible T>70%, IR R>85%");
    I18n::reg("tfc_t_pol",  "Tp>95%, Ts<1% @45°", "Tp>95%, Ts<1% @45°");

    // サブタブ
    I18n::reg("tfc_tab_stack",  "層構成", "Layer stack");
    I18n::reg("tfc_tab_spec",   "分光特性", "Spectral response");
    I18n::reg("tfc_tab_design", "最適化設計", "Optimization");
    I18n::reg("tfc_tab_mfg",    "製造・誤差", "Manufacturing / tolerance");

    // 層構成
    I18n::reg("tfc_stack_sec", "層構成 / Layer stack", "Layer stack");
    I18n::reg("tfc_substrate", "基板", "Substrate");
    I18n::reg("tfc_sub_bk7",      "N-BK7 (Schott)", "N-BK7 (Schott)");
    I18n::reg("tfc_sub_sio2",     "溶融石英 SiO₂ (Malitson 1965)",
              "Fused silica SiO₂ (Malitson 1965)");
    I18n::reg("tfc_sub_si",       "Si (赤外, Salzberg 1957)",
              "Si (infrared, Salzberg 1957)");
    I18n::reg("tfc_sub_sapphire", "サファイア Al₂O₃ (Malitson 1962)",
              "Sapphire Al₂O₃ (Malitson 1962)");
    I18n::reg("tfc_sub_pmma",     "PMMA (樹脂, Sultanova 2009)",
              "PMMA (polymer, Sultanova 2009)");
    I18n::reg("tfc_incident", "入射媒質", "Incident medium");
    I18n::reg("tfc_inc_air", "空気 (n = 1)", "Air (n = 1)");
    I18n::reg("tfc_inc_bk7", "N-BK7 キューブ (プリズム貼合せ)",
              "N-BK7 cube (cemented prism)");
    I18n::reg("tfc_lambda0", "設計波長 λ₀ [nm]", "Design wavelength λ₀ [nm]");
    I18n::reg("tfc_c_mat",   "材料", "Material");
    I18n::reg("tfc_c_n",     "n @λ₀", "n @λ₀");
    I18n::reg("tfc_c_k",     "k", "k");
    I18n::reg("tfc_c_dphys", "物理膜厚 [nm]", "Physical thickness [nm]");
    I18n::reg("tfc_c_qwot",  "光学膜厚 n·d/λ₀", "Optical thickness n·d/λ₀");
    I18n::reg("tfc_c_role",  "役割", "Role");
    I18n::reg("tfc_r_outer", "最外層", "Outermost layer");
    I18n::reg("tfc_r_high",  "高屈折率", "High index");
    I18n::reg("tfc_r_low",   "低屈折率", "Low index");
    I18n::reg("tfc_r_sub",   "基板側", "Substrate side");
    I18n::reg("tfc_layer_add", "＋ 層を追加 (この行をクリック)",
              "+ Add a layer (click this row)");
    I18n::reg("tfc_layer_del", "選択行の層を削除", "Delete the selected layer");
    I18n::reg("tfc_layer_enable", "この層を計算に含める",
              "Include this layer in the computation");
    I18n::reg("tfc_periodic", "周期記法", "Periodic notation");
    I18n::reg("tfc_expand", "展開", "Expand");
    I18n::reg("tfc_dispersion", "材料の分散 (n の λ依存) を使用",
              "Use material dispersion (λ-dependent n)");
    I18n::reg("tfc_absorption", "吸収 (k) を考慮", "Include absorption (k)");
    I18n::reg("tfc_matsource",
              "材料 n は公刊 Sellmeier 係数 (MaterialDispersion / GlassCatalog) から。"
              "k はカタログに無いため利用者入力 (既定 0 = 無損失)。",
              "n comes from published Sellmeier coefficients (MaterialDispersion / "
              "GlassCatalog). k is not in the catalogue and is entered by the user "
              "(default 0 = lossless).");
    I18n::reg("tfc_stack_note",
              "▸ 編集できるのは材料・k・物理膜厚と各層の有効/無効。n と光学膜厚は "
              "λ₀ における計算値。層の順序は入射側 → 基板側。",
              "▸ Editable: material, k, physical thickness and the per-layer "
              "enable flag. n and the optical thickness are computed at λ₀. "
              "Layers are ordered from the incident side to the substrate.");

    // 分光特性
    I18n::reg("tfc_spec_sec", "分光特性 / Spectral response", "Spectral response");
    I18n::reg("tfc_aoi", "入射角", "Angle of incidence");
    I18n::reg("tfc_aoi_unit", "° · ", "° · ");
    I18n::reg("tfc_angle_sweep", "角度スイープ 0-60° (λ₀ 固定)",
              "Angle sweep 0-60° (at λ₀)");
    I18n::reg("tfc_split_sp", "s/p 偏光を分離", "Separate s / p polarization");
    I18n::reg("tfc_lam_range", "波長範囲", "Wavelength range");
    I18n::reg("tfc_y_rt", "R, T [%]", "R, T [%]");
    I18n::reg("tfc_y_r",  "R [%]", "R [%]");
    I18n::reg("tfc_x_lam", "λ [nm]", "λ [nm]");
    I18n::reg("tfc_x_aoi", "入射角 [°]", "Angle of incidence [°]");
    I18n::reg("tfc_c_metric", "指標", "Metric");
    I18n::reg("tfc_c_pol_s",  "s 偏光", "s-polarization");
    I18n::reg("tfc_c_pol_p",  "p 偏光", "p-polarization");
    I18n::reg("tfc_c_cond",   "評価条件", "Evaluation condition");
    I18n::reg("tfc_m_ravg",  "平均反射率 R̄", "Mean reflectance R̄");
    I18n::reg("tfc_m_rmax",  "最大反射率", "Peak reflectance");
    I18n::reg("tfc_m_rmin",  "最小反射率", "Minimum reflectance");
    I18n::reg("tfc_m_tavg",  "平均透過率 T̄", "Mean transmittance T̄");
    I18n::reg("tfc_m_amax",  "最大吸収率 A", "Peak absorptance A");
    I18n::reg("tfc_m_gdr",   "群遅延リップル (反射, P-P)",
              "Group-delay ripple (reflection, peak-to-peak)");
    I18n::reg("tfc_cond_fmt", "λ %1–%2 nm / %3°", "λ %1–%2 nm / %3°");
    I18n::reg("tfc_spec_note",
              "▸ 評価波長域 %1–%2 nm (%3 点)。前提: 各層は完全コヒーレント、"
              "基板は半無限 (裏面反射を含まない)、入射媒質は無損失。"
              "群遅延は反射位相の数値微分 τ = (λ²/2πc)·dφ/dλ。",
              "▸ Evaluated over %1–%2 nm (%3 points). Assumptions: every layer is "
              "fully coherent, the substrate is semi-infinite (no back-surface "
              "reflection) and the incident medium is lossless. The group delay is "
              "the numerical derivative of the reflection phase, "
              "τ = (λ²/2πc)·dφ/dλ.");
    I18n::reg("tfc_spec_skip",
              " 材料データの有効範囲外の %1 点は外挿せず除外した。",
              " %1 points outside the validity range of the material data were "
              "dropped rather than extrapolated.");
    I18n::reg("tfc_spec_fail",
              "⚠ この設定では計算できない — 材料データの有効範囲に入る波長が無い。"
              "波長範囲・材料・基板を見直すこと。",
              "⚠ Cannot compute with these settings — no wavelength falls inside "
              "the validity range of the material data. Revise the wavelength "
              "range, the materials or the substrate.");
    I18n::reg("tfc_na", "—", "—");
    I18n::reg("tfc_btn_rta",   "📊 R/T/A スペクトル", "📊 R/T/A spectra");
    I18n::reg("tfc_btn_map",   "🌈 角度-波長マップ", "🌈 Angle-wavelength map");
    I18n::reg("tfc_btn_field", "📐 電界分布 (層内)",
              "📐 Field distribution (inside the stack)");
    I18n::reg("tfc_btn_fdtd",  "🔍 FDTDで検証", "🔍 Verify with FDTD");

    // 最適化設計
    I18n::reg("tfc_design_sec", "最適化設計 / Optimization", "Optimization");
    I18n::reg("tfc_method", "手法", "Method");
    I18n::reg("tfc_m_simplex", "単純降下法", "Simplex descent");
    I18n::reg("tfc_m_needle",  "ニードル法 (層追加)", "Needle method (layer insertion)");
    I18n::reg("tfc_m_tunnel",  "トンネル法 (大域)", "Tunneling method (global)");
    I18n::reg("tfc_m_ga",      "遺伝的アルゴリズム", "Genetic algorithm");
    I18n::reg("tfc_vars", "変数", "Variables");
    I18n::reg("tfc_v_thick", "各層の膜厚", "Thickness of each layer");
    I18n::reg("tfc_v_count", "層数 (ニードル)", "Layer count (needle)");
    I18n::reg("tfc_v_mat",   "材料選択 (離散)", "Material choice (discrete)");
    I18n::reg("tfc_targets", "ターゲット", "Targets");
    I18n::reg("tfc_c_lamrange", "λ範囲 [nm]", "λ range [nm]");
    I18n::reg("tfc_c_quantity", "量", "Quantity");
    I18n::reg("tfc_c_pol", "偏光", "Polarization");
    I18n::reg("tfc_c_goal",     "目標 [%]", "Goal [%]");
    I18n::reg("tfc_c_tol",      "許容 [%]", "Tolerance [%]");
    I18n::reg("tfc_c_weight",   "重み", "Weight");
    I18n::reg("tfc_run_opt", "▶ 最適化実行", "▶ Run optimization");
    I18n::reg("tfc_merit_fmt", "Merit = %1  (現在の層構成の実計算値 / %2 点で評価)",
              "Merit = %1  (computed for the present stack over %2 points)");
    I18n::reg("tfc_merit_na", "Merit = — (評価できない: ターゲット帯域に"
                              "計算可能な波長が無い)",
              "Merit = — (cannot evaluate: no computable wavelength in the "
              "target bands)");
    I18n::reg("tfc_merit_note",
              "▸ Merit はターゲット表と現在の層構成から実計算した値 "
              "(F = √(Σw((Q−目標)/許容)²/Σw)、Furman & Tikhonravov 1992)。"
              "F ≤ 1 が「平均して許容内」。「最適化実行」は膜厚だけを動かす"
              "シンプレックス法 (Nelder-Mead 1965) で、改善したときだけ層構成に"
              "書き戻す。層数・材料を変える needle / tunneling / GA は未実装。",
              "▸ The merit value is computed from the target table and the present "
              "stack (F = √(Σw((Q−goal)/tol)²/Σw), Furman & Tikhonravov 1992); "
              "F ≤ 1 means \"within tolerance on average\". \"Run optimisation\" "
              "uses the simplex method (Nelder-Mead 1965) on the layer "
              "thicknesses only and writes the result back only when it "
              "improves. The needle / tunneling / GA methods, which change the "
              "layer count or the materials, are not implemented.");
    I18n::reg("tfc_q_r", "R", "R");
    I18n::reg("tfc_q_t", "T", "T");
    I18n::reg("tfc_pol_avg", "無偏光", "unpolarized");
    I18n::reg("tfc_pol_s", "s", "s");
    I18n::reg("tfc_pol_p", "p", "p");

    // 製造・誤差
    I18n::reg("tfc_mfg_sec", "製造誤差・歩留まり / Manufacturing tolerance",
              "Manufacturing tolerance and yield");
    I18n::reg("tfc_depo", "成膜法", "Deposition process");
    I18n::reg("tfc_d_eb",   "EB蒸着", "E-beam evaporation");
    I18n::reg("tfc_d_ibs",  "IBS (高精度)", "IBS (high precision)");
    I18n::reg("tfc_d_ald",  "ALD", "ALD");
    I18n::reg("tfc_d_sput", "スパッタ", "Sputtering");
    I18n::reg("tfc_thickerr", "膜厚誤差", "Thickness error");
    I18n::reg("tfc_thickerr_unit", "% (1σ, ランダム)", "% (1σ, random)");
    I18n::reg("tfc_systematic", "系統誤差 (成膜レートドリフト)",
              "Systematic error (deposition-rate drift)");
    I18n::reg("tfc_correlated", "層間の誤差相関", "Layer-to-layer error correlation");
    I18n::reg("tfc_monitor", "モニタリング", "Monitoring");
    I18n::reg("tfc_mon_quartz",  "水晶振動子", "Quartz crystal");
    I18n::reg("tfc_mon_optical", "光学モニタ", "Optical monitor");
    I18n::reg("tfc_mon_both",    "併用", "Both");
    I18n::reg("tfc_run_mc", "▶ モンテカルロ (1000回)", "▶ Monte Carlo (1000 runs)");
    I18n::reg("tfc_yield_pending", "歩留まり — (モンテカルロ未実行)",
              "Yield — (Monte Carlo not run)");
    I18n::reg("tfc_yield_fmt", "歩留まり %1% (%2/%3 試行が全ターゲット帯域で許容内)",
              "Yield %1% (%2 of %3 trials within tolerance in every target band)");
    I18n::reg("tfc_yield_fail", "歩留まり — (計算できない: ターゲット帯域に"
                                "計算可能な波長が無い)",
              "Yield — (cannot compute: no computable wavelength in the target "
              "bands)");
    I18n::reg("tfc_sens_fmt", "最も敏感な層: #%1 (%2) — 感度 %3 %/nm "
                              "(ターゲット帯域平均, Δd = ±%4 nm の中心差分)",
              "Most sensitive layer: #%1 (%2) — sensitivity %3 %/nm (mean over the "
              "target bands, central difference with Δd = ±%4 nm)");
    I18n::reg("tfc_sens_na", "膜厚感度 — (評価できない)",
              "Thickness sensitivity — (cannot evaluate)");
    I18n::reg("tfc_mc_note",
              "▸ 合格判定は「全ターゲット帯域の全サンプル点で |値 − 目標| ≤ 許容」。"
              "膜厚誤差は 1σ の相対値を全層へ独立に適用し、系統誤差を有効にすると"
              "全層共通のドリフト (同じ 1σ) を加算する。乱数は固定 seed なので"
              "同じ設定なら同じ結果になる。",
              "▸ A trial passes when |value − goal| ≤ tolerance holds at every "
              "sampled wavelength of every target band. The thickness error is "
              "applied independently to each layer as a relative 1σ value; the "
              "systematic option adds one common drift (same 1σ) to all layers. "
              "The random seed is fixed, so identical settings give identical "
              "results.");
    I18n::reg("tfc_mfg_unwired",
              "▸ 成膜法・モニタリング・層間の誤差相関はモンテカルロに未反映 (未実装)。",
              "▸ The deposition process, the monitoring method and the "
              "layer-to-layer error correlation are not used by the Monte Carlo "
              "yet (not implemented).");
    I18n::reg("tfc_btn_recipe", "📄 成膜レシピ書出 (装置向け)",
              "📄 Export deposition recipe (for the coater)");
    I18n::reg("tfc_btn_sens", "📊 感度解析", "📊 Sensitivity analysis");

    // 周期記法の展開
    I18n::reg("tfc_expand_tip",
              "周期記法を層構成へ展開します。記号への材料割当 (H=Si3N4 など) と、"
              "λ₀ を変えるなら @ 波長 も書いてください。",
              "Expands the shorthand into a layer stack. Give a material for "
              "every symbol (e.g. H=Si3N4), and '@ <wavelength>' if λ₀ changes.");
    I18n::reg("tfc_expand_title", "周期記法の展開", "Expand shorthand notation");
    I18n::reg("tfc_expand_bad", "記法を解釈できません: %1",
              "Cannot parse the notation: %1");
    I18n::reg("tfc_expand_nomat",
              "記号 '%1' に材料が割り当てられていません "
              "(例: 末尾に「%1=SiO2」と書く)。層構成は変更していません。",
              "No material is assigned to the symbol '%1' (add e.g. \"%1=SiO2\" "
              "at the end). The stack was left unchanged.");
    I18n::reg("tfc_expand_badmat",
              "材料 '%1' の屈折率が %2 nm で得られません "
              "(材料表に無いか、分散式の有効範囲外)。層構成は変更していません。",
              "The refractive index of '%1' is not available at %2 nm (unknown "
              "material, or outside the validity range of its dispersion "
              "formula). The stack was left unchanged.");

    // 膜厚最適化
    I18n::reg("tfc_run_opt_tip",
              "ターゲット表の Merit を最小化するように膜厚だけを動かします "
              "(シンプレックス法)。改善したときだけ層構成へ書き戻します。",
              "Minimises the merit value of the target table by moving the layer "
              "thicknesses only (simplex method). The stack is updated only when "
              "the merit improves.");
    I18n::reg("tfc_opt_title", "膜厚最適化", "Thickness optimisation");
    I18n::reg("tfc_opt_novar",
              "「膜厚」にチェックを入れてください (動かせる変数がありません)。",
              "Tick \"thickness\" — there is no variable to move.");
    I18n::reg("tfc_opt_nolayer", "有効な層がありません。",
              "There is no enabled layer.");
    I18n::reg("tfc_opt_notarget", "ターゲットが 1 行もありません。",
              "The target table is empty.");
    I18n::reg("tfc_opt_fail",
              "最適化できません (ターゲット帯域に計算可能な波長が無い)。",
              "Cannot optimise: no computable wavelength in the target bands.");
    I18n::reg("tfc_opt_noimprove",
              "Merit = %1 から改善しませんでした。層構成は変更していません "
              "(層数や材料を変える手法が要ります)。",
              "No improvement over merit = %1, so the stack was left unchanged "
              "(a method that changes the layer count or the materials would be "
              "needed).");
    I18n::reg("tfc_opt_done", "Merit %1 → %2 (%3 反復, %4)",
              "Merit %1 → %2 (%3 iterations, %4)");
    I18n::reg("tfc_opt_conv", "収束", "converged");
    I18n::reg("tfc_opt_maxiter", "反復上限", "iteration limit reached");

    // 成膜レシピ / 感度一覧
    I18n::reg("tfc_recipe_title", "成膜レシピを保存", "Save the deposition recipe");
    I18n::reg("tfc_recipe_filter", "CSV (*.csv);;すべてのファイル (*)",
              "CSV (*.csv);;All files (*)");
    I18n::reg("tfc_sens_title", "膜厚感度 (層別)", "Thickness sensitivity by layer");
    I18n::reg("tfc_sens_head",
              "各層の膜厚を ±0.5 nm 動かしたときの対象量の変化 "
              "(ターゲット帯域の平均, 中心差分)。太字が最も敏感な層。",
              "Change of the target quantity when each layer moves by ±0.5 nm "
              "(mean over the target bands, central difference). The most "
              "sensitive layer is shown in bold.");
    I18n::reg("tfc_sens_note",
              "▸ 無効にした層は最適化・感度・歩留まりのいずれにも含めていません。",
              "▸ Disabled layers are excluded from the optimisation, the "
              "sensitivity and the yield alike.");
    I18n::reg("tfc_sens_fail",
              "感度を評価できません (ターゲット帯域に計算可能な波長が無い)。",
              "Cannot evaluate the sensitivity: no computable wavelength in the "
              "target bands.");
    I18n::reg("tfc_c_layer", "層", "Layer");
    I18n::reg("tfc_c_sens", "感度 [%/nm]", "Sensitivity [%/nm]");
    return true;
}();

// ── 材料 (公刊分散) の解決 ──────────────────────────────────────────────────
// 材料 id が "glass:<銘柄>" なら core/GlassCatalog、それ以外は
// optics/MaterialDispersion (公刊 Sellmeier) を引く。どちらも有効範囲外では
// false を返し、値を書き換えない (外挿した「それらしい値」を作らない)。
QString glassPrefix() { return QStringLiteral("glass:"); }
// Schott N-BK7 データシートの分散式適用範囲 (0.30–2.50 μm)。GlassCatalog は
// 範囲を持たないのでここで制限する。
const double kGlassLamMinUm = 0.30;
const double kGlassLamMaxUm = 2.50;
// 円周率 (MSVC は M_PI を既定で定義しないので自前で持つ)
const double kPi = 3.14159265358979323846;

bool indexOf(const QString &id, double lam_nm, double &n)
{
    if (!(lam_nm > 0.0)) return false;
    const double um = lam_nm / 1000.0;
    if (id.startsWith(glassPrefix())) {
        if (um < kGlassLamMinUm || um > kGlassLamMaxUm) return false;
        const QString name = id.mid(glassPrefix().size());
        for (const Glass &g : GlassCatalog::all())
            if (g.name == name && g.hasSellmeier()) {
                const double v = g.n(um);
                if (!(v > 0.0)) return false;
                n = v;
                return true;
            }
        return false;
    }
    return optics::refractiveIndex(id.toUtf8().constData(), um, n);
}

// 表示名 (材料 id → 短い表示)
QString matLabel(const QString &id)
{
    if (id.startsWith(glassPrefix())) return id.mid(glassPrefix().size());
    return id;
}

// ── プリセット ──────────────────────────────────────────────────────────────
struct PresetDef {
    const char *key, *nameKey, *targetKey;
    double lam0;              // 設計波長 λ₀ [nm]
    double lamMin, lamMax;    // 表示波長範囲 [nm]
    double aoi;               // 入射角 [deg] (macneille なら計算値で上書き)
    bool   macneille;
    int    incident;          // 入射媒質 combo index (0=空気 1=N-BK7)
    int    substrate;         // 基板 combo index
    const char *hi, *lo;      // 高 / 低屈折率材料 id
};

// 高/低屈折率材料は MaterialDispersion に公刊 Sellmeier がある材料から選ぶ。
// TiO₂ は 0.43–1.5 μm でしか係数が定義されていないので可視のみに使う。
const PresetDef kPresets[6] = {
    { "ar",   "tfc_p_ar",   "tfc_t_ar",    550,  400,  800,  0, false, 0, 0,
      "Si3N4", "SiO2" },
    { "dbr",  "tfc_p_dbr",  "tfc_t_dbr",  1550, 1300, 1800,  0, false, 0, 0,
      "Si3N4", "SiO2" },
    { "bpf",  "tfc_p_bpf",  "tfc_t_bpf",  1550, 1450, 1650,  0, false, 0, 0,
      "Si3N4", "SiO2" },
    { "dich", "tfc_p_dich", "tfc_t_dich",  500,  430,  780, 45, false, 0, 0,
      "TiO2",  "SiO2" },
    { "heat", "tfc_p_heat", "tfc_t_heat", 1400,  400, 2200,  0, false, 0, 0,
      "Si3N4", "SiO2" },
    { "pol",  "tfc_p_pol",  "tfc_t_pol",  1550, 1450, 1650, 45, true,  1, 0,
      "Si3N4", "SiO2" },
};

// 層 1 枚のプリセット記述 (光学膜厚 n·d/λ₀ で与える。0.25 = 四分の一波長)
struct PLayer { const char *mat; double qwot; };

// 各プリセットの層構成 (入射側 → 基板側)。すべて古典的な四分の一波長設計:
//   AR    : W コート (L λ/4 · H λ/2 · M λ/4) — Macleod 4th ed. §4.3
//   DBR   : (H L)^12 H の四分の一波長積層 — Macleod §5.2
//   BPF   : 単一空洞ファブリペロー (H L)^4 H · 2L · H (L H)^4 — Macleod §7.2
//   Dich  : (H L)^9 の四分の一波長積層
//   Low-E : (H L)^4 H の四分の一波長積層 (赤外反射)
//   Pol   : (H L)^10 H を MacNeille 角で使う
std::vector<PLayer> presetLayers(int idx)
{
    const PresetDef &P = kPresets[idx];
    std::vector<PLayer> v;
    auto H = [&](double q) { v.push_back({ P.hi, q }); };
    auto L = [&](double q) { v.push_back({ P.lo, q }); };
    switch (idx) {
    case 0:                                       // AR (W コート)
        L(0.25); H(0.50); v.push_back({ "Al2O3", 0.25 });
        break;
    case 1:                                       // DBR
        for (int i = 0; i < 12; ++i) { H(0.25); L(0.25); }
        H(0.25);
        break;
    case 2:                                       // バンドパス (単一空洞)
        for (int i = 0; i < 4; ++i) { H(0.25); L(0.25); }
        H(0.25); L(0.50); H(0.25);
        for (int i = 0; i < 4; ++i) { L(0.25); H(0.25); }
        break;
    case 3:                                       // ダイクロイック
        for (int i = 0; i < 9; ++i) { H(0.25); L(0.25); }
        break;
    case 4:                                       // Low-E
        for (int i = 0; i < 4; ++i) { H(0.25); L(0.25); }
        H(0.25);
        break;
    default:                                      // 偏光子
        for (int i = 0; i < 10; ++i) { H(0.25); L(0.25); }
        H(0.25);
        break;
    }
    return v;
}

// ターゲット (プリセットの「目標」を数値化したもの。goal/tol は %)
struct PTarget {
    double lam0, lam1;
    int    quantity;   // 0=R 1=T
    int    pol;        // 0=無偏光 1=s 2=p
    double goal, tol, weight;
    int    samples;
};

std::vector<PTarget> presetTargets(int idx)
{
    switch (idx) {
    case 0: return { { 450, 650, 0, 0,   0.0,  0.5, 1.0, 21 },
                     { 700, 750, 0, 0,   0.0,  2.0, 0.3, 11 } };
    case 1: return { { 1540, 1560, 0, 0, 100.0, 0.1, 1.0, 11 } };
    // OD > 4 は透過率 10⁻⁴ = 0.01 %
    case 2: return { { 1530, 1570, 1, 0, 100.0, 10.0, 1.0, 21 },
                     { 1450, 1500, 1, 0,   0.0,  0.01, 0.3, 11 } };
    case 3: return { { 430, 500, 0, 0, 100.0, 5.0, 1.0, 11 },
                     { 550, 700, 1, 0, 100.0, 5.0, 1.0, 11 } };
    case 4: return { { 400,  700, 1, 0, 100.0, 30.0, 1.0, 11 },
                     { 1200, 1600, 0, 0, 100.0, 15.0, 1.0, 11 } };
    default: return { { 1500, 1600, 1, 2, 100.0, 5.0, 1.0, 11 },
                      { 1500, 1600, 1, 1,   0.0, 1.0, 1.0, 11 } };
    }
}

// バッジ (mock の .badge ok / warn / err / acc)
QString badgeCss(const char *kind)
{
    QString css = "border-radius:3px; padding:1px 6px; font-size:11px;";
    if (qstrcmp(kind, "ok") == 0)        css += "background:#DFF6DD; color:#0F7B0F;";
    else if (qstrcmp(kind, "warn") == 0) css += "background:#FFF4CE; color:#9D5D00;";
    else if (qstrcmp(kind, "err") == 0)  css += "background:#FDE7E9; color:#B91C1C;";
    else if (qstrcmp(kind, "acc") == 0)  css += "background:#DEECF9; color:#0078D4;";
    else                                 css += "background:palette(midlight);";
    return css;
}

QLabel *makeBadge(const QString &text, const char *kind, QWidget *parent)
{
    auto *b = new QLabel(text, parent);
    b->setTextFormat(Qt::PlainText);
    b->setStyleSheet(badgeCss(kind));
    return b;
}

QLabel *hintLabel(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setTextFormat(Qt::PlainText);
    l->setWordWrap(true);
    return l;
}

QLabel *noteLabel(const QString &text, QWidget *parent)
{
    auto *l = hintLabel(text, parent);
    l->setStyleSheet("font-size:11px; color:palette(mid);");
    return l;
}

QLineEdit *numEdit(const QString &value, int width, QWidget *parent)
{
    auto *e = new QLineEdit(value, parent);
    if (width > 0) e->setMaximumWidth(width);
    return e;
}

QCheckBox *makeCheck(const QString &text, bool on, QWidget *parent)
{
    auto *c = new QCheckBox(text, parent);
    c->setChecked(on);
    return c;
}

// <Seg> 相当: 排他 checkable QPushButton の一列
QButtonGroup *segRow(QHBoxLayout *row, const QStringList &labels, int current,
                     QWidget *parent)
{
    auto *g = new QButtonGroup(parent);
    g->setExclusive(true);
    for (int i = 0; i < labels.size(); ++i) {
        auto *b = new QPushButton(labels.at(i), parent);
        b->setCheckable(true);
        b->setStyleSheet("font-size:11px; padding:2px 8px;");
        if (i == current) b->setChecked(true);
        g->addButton(b, i);
        row->addWidget(b);
    }
    row->addStretch(1);
    return g;
}

QTableWidgetItem *textItem(const QString &s)
{
    auto *it = new QTableWidgetItem(s);
    it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    return it;
}

QTableWidgetItem *numItem(const QString &s)
{
    auto *it = textItem(s);
    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return it;
}

// 編集可能な数値セル
QTableWidgetItem *editNumItem(const QString &s)
{
    auto *it = new QTableWidgetItem(s);
    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return it;
}

QTableWidgetItem *monoItem(const QString &s)
{
    auto *it = textItem(s);
    QFont f = it->font();
    // 実在するファミリのみ指定 (Theme が環境ごとに解決済み)
    if (const QString mf = Theme::monoFontFamily(); !mf.isEmpty())
        f.setFamily(mf);
    f.setStyleHint(QFont::Monospace);
    it->setFont(f);
    return it;
}

// SectionBox::form() は 1 セクションに 1 つだけなので、表より後ろに来る
// <Row label> 用に独立したフォームを vbox の末尾へ足す
QFormLayout *appendForm(SectionBox *s)
{
    auto *f = new QFormLayout();
    f->setRowWrapPolicy(QFormLayout::DontWrapRows);
    f->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    f->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    f->setLabelAlignment(Qt::AlignLeft);
    f->setHorizontalSpacing(8);
    f->setVerticalSpacing(4);
    s->vbox()->addLayout(f);
    return f;
}

// 基板 combo index → 材料 id
const char *kSubstrateIds[5] = { "glass:N-BK7", "SiO2", "Si", "Al2O3", "PMMA" };
// 入射媒質 combo index → 材料 id
const char *kIncidentIds[2] = { "Air", "glass:N-BK7" };

double pct(double v) { return v * 100.0; }

QString fmtPct(double v, int prec = 3)
{
    return QString::number(pct(v), 'f', prec) + " %";
}
} // namespace

// ── ThinFilmTab ─────────────────────────────────────────────────────────────
ThinFilmTab::ThinFilmTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project), m_updating(false),
      m_preset(nullptr), m_layerBadge(nullptr), m_targetLabel(nullptr),
      m_tabs(nullptr),
      m_incident(nullptr), m_substrate(nullptr), m_lambda0(nullptr),
      m_layerTable(nullptr), m_periodic(nullptr),
      m_useDispersion(nullptr), m_useAbsorption(nullptr),
      m_aoi(nullptr), m_angleSweep(nullptr), m_splitSP(nullptr),
      m_lamMin(nullptr), m_lamMax(nullptr), m_specPlot(nullptr),
      m_specTable(nullptr), m_specNote(nullptr),
      m_method(nullptr), m_varThickness(nullptr), m_varCount(nullptr),
      m_varMaterial(nullptr), m_targetTable(nullptr), m_meritLabel(nullptr),
      m_deposition(nullptr), m_thickErr(nullptr), m_systematic(nullptr),
      m_correlated(nullptr), m_monitoring(nullptr), m_mcButton(nullptr),
      m_yieldBadge(nullptr), m_sensitiveLabel(nullptr)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // ── 薄膜多層膜設計 (プリセット) ─────────────────────────────────────────
    auto *sTop = new SectionBox(I18n::tr("tfc_title"), body);
    sTop->vbox()->addWidget(hintLabel(I18n::tr("tfc_hint"), sTop));

    m_preset = new QComboBox(sTop);
    for (int i = 0; i < 6; ++i)
        m_preset->addItem(I18n::tr(kPresets[i].nameKey));
    sTop->form()->addRow(I18n::tr("tfc_preset"), m_preset);

    auto *pRow = new QHBoxLayout();
    m_layerBadge = makeBadge(QString(), "acc", sTop);
    pRow->addWidget(m_layerBadge);
    m_targetLabel = hintLabel(QString(), sTop);
    pRow->addWidget(m_targetLabel);
    pRow->addStretch(1);
    sTop->form()->addRow(pRow);
    sTop->vbox()->addWidget(noteLabel(I18n::tr("tfc_preset_note"), sTop));
    v->addWidget(sTop);

    // ── サブタブ ────────────────────────────────────────────────────────────
    m_tabs = new QTabWidget(body);
    m_tabs->setDocumentMode(true);
    m_tabs->addTab(buildStackPage(),  I18n::tr("tfc_tab_stack"));
    m_tabs->addTab(buildSpecPage(),   I18n::tr("tfc_tab_spec"));
    m_tabs->addTab(buildDesignPage(), I18n::tr("tfc_tab_design"));
    m_tabs->addTab(buildMfgPage(),    I18n::tr("tfc_tab_mfg"));
    v->addWidget(m_tabs);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    connect(m_preset, &QComboBox::currentIndexChanged,
            this, &ThinFilmTab::presetChanged);
    presetChanged(0);                    // 既定 "ar"
}

// ── プリセット → 層構成・波長・入射角・ターゲット ───────────────────────────
void ThinFilmTab::presetChanged(int index)
{
    index = qBound(0, index, 5);
    const PresetDef &P = kPresets[index];

    const bool wasUpdating = m_updating;
    m_updating = true;

    m_targetLabel->setText(
        I18n::tr("tfc_target_fmt").arg(I18n::tr(P.targetKey)));
    m_incident->setCurrentIndex(qBound(0, P.incident, 1));
    m_substrate->setCurrentIndex(qBound(0, P.substrate, 4));
    m_lambda0->setText(QString::number(P.lam0, 'g', 6));
    m_lamMin->setText(QString::number(P.lamMin, 'g', 6));
    m_lamMax->setText(QString::number(P.lamMax, 'g', 6));

    // 入射角: MacNeille 型偏光子は H/L 界面がブルースター条件を満たす角度
    //   n₀ sinθ₀ = nH·nL / √(nH² + nL²)
    // (MacNeille, US Patent 2,403,731 (1946); Macleod 4th ed. §9.5)
    double aoi = P.aoi;
    if (P.macneille) {
        double nH = 0, nL = 0, n0 = 0;
        if (indexOf(QString::fromUtf8(P.hi), P.lam0, nH) &&
            indexOf(QString::fromUtf8(P.lo), P.lam0, nL) &&
            indexOf(QString::fromUtf8(kIncidentIds[qBound(0, P.incident, 1)]),
                    P.lam0, n0)) {
            const double s = nH * nL / std::sqrt(nH * nH + nL * nL) / n0;
            if (s > 0.0 && s < 1.0)
                aoi = std::asin(s) * 180.0 / kPi;
        }
    }
    m_aoi->setText(QString::number(aoi, 'f', 2));

    // 層構成 (光学膜厚 n·d/λ₀ → 物理膜厚 d = qwot·λ₀/n(λ₀))
    m_stack.clear();
    for (const PLayer &pl : presetLayers(index)) {
        StackLayer L;
        L.mat = QString::fromUtf8(pl.mat);
        L.k = 0.0;
        double n = 0.0;
        L.d_nm = indexOf(L.mat, P.lam0, n) && n > 0.0 ? pl.qwot * P.lam0 / n : 0.0;
        L.enabled = true;
        m_stack.push_back(L);
    }

    // ターゲット
    m_targets.clear();
    for (const PTarget &t : presetTargets(index)) {
        TargetRow r;
        r.lam0 = t.lam0; r.lam1 = t.lam1;
        r.quantity = t.quantity; r.pol = t.pol;
        r.goal = t.goal; r.tol = t.tol; r.weight = t.weight;
        r.samples = t.samples;
        m_targets.push_back(r);
    }

    m_updating = wasUpdating;
    rebuildLayerTable();
    rebuildTargetTable();
    recompute();
}

double ThinFilmTab::lambda0() const
{
    bool ok = false;
    const double v = m_lambda0 ? m_lambda0->text().trimmed().toDouble(&ok) : 0.0;
    return (ok && v > 0.0) ? v : 550.0;
}

double ThinFilmTab::aoiDeg() const
{
    bool ok = false;
    const double v = m_aoi ? m_aoi->text().trimmed().toDouble(&ok) : 0.0;
    return (ok && v >= 0.0 && v < 90.0) ? v : 0.0;
}

// ── 層構成 / Layer stack ────────────────────────────────────────────────────
QWidget *ThinFilmTab::buildStackPage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setSpacing(8);

    auto *s = new SectionBox(I18n::tr("tfc_stack_sec"), page);

    // 入射媒質 + 基板 + 設計波長
    m_incident = new QComboBox(s);
    m_incident->addItem(I18n::tr("tfc_inc_air"));
    m_incident->addItem(I18n::tr("tfc_inc_bk7"));
    s->form()->addRow(I18n::tr("tfc_incident"), m_incident);

    m_substrate = new QComboBox(s);
    m_substrate->addItem(I18n::tr("tfc_sub_bk7"));
    m_substrate->addItem(I18n::tr("tfc_sub_sio2"));
    m_substrate->addItem(I18n::tr("tfc_sub_si"));
    m_substrate->addItem(I18n::tr("tfc_sub_sapphire"));
    m_substrate->addItem(I18n::tr("tfc_sub_pmma"));
    s->form()->addRow(I18n::tr("tfc_substrate"), m_substrate);

    m_lambda0 = numEdit(QStringLiteral("550"), 90, s);
    s->form()->addRow(I18n::tr("tfc_lambda0"), m_lambda0);

    // 層スタック表 (層 + 「層を追加」行)
    m_layerTable = new QTableWidget(0, 8, s);
    m_layerTable->setHorizontalHeaderLabels({ QString(), "#", I18n::tr("tfc_c_mat"),
                                              I18n::tr("tfc_c_n"),
                                              I18n::tr("tfc_c_k"),
                                              I18n::tr("tfc_c_dphys"),
                                              I18n::tr("tfc_c_qwot"),
                                              I18n::tr("tfc_c_role") });
    m_layerTable->verticalHeader()->setVisible(false);
    m_layerTable->verticalHeader()->setDefaultSectionSize(24);
    m_layerTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_layerTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_layerTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_layerTable->setMaximumHeight(220);
    s->vbox()->addWidget(m_layerTable);

    auto *delBtn = new QPushButton(I18n::tr("tfc_layer_del"), s);
    auto *btnRow0 = new QHBoxLayout();
    btnRow0->addWidget(delBtn);
    btnRow0->addStretch(1);
    s->vbox()->addLayout(btnRow0);
    s->vbox()->addWidget(noteLabel(I18n::tr("tfc_stack_note"), s));

    // 表より後ろの <Row> は独立フォームへ (順序をモックどおりに保つ)
    QFormLayout *f2 = appendForm(s);

    // 周期記法 (Macleod 記法)。プレースホルダは記入例で、値としては持たない
    // (現在の層構成を表すものではないため)。「展開」で層構成へ変換する。
    auto *perRow = new QHBoxLayout();
    m_periodic = numEdit(QString(), 0, s);
    m_periodic->setPlaceholderText(
        QStringLiteral("Air | (H L)^12 H | Sub    H=Si3N4 L=SiO2 @ 1550nm"));
    perRow->addWidget(m_periodic, 1);
    auto *expandBtn = new QPushButton(I18n::tr("tfc_expand"), s);
    expandBtn->setToolTip(I18n::tr("tfc_expand_tip"));
    connect(expandBtn, &QPushButton::clicked, this, &ThinFilmTab::expandPeriodic);
    perRow->addWidget(expandBtn);
    f2->addRow(I18n::tr("tfc_periodic"), perRow);

    // 材料オプション
    auto *optRow = new QHBoxLayout();
    m_useDispersion = makeCheck(I18n::tr("tfc_dispersion"), true, s);
    m_useAbsorption = makeCheck(I18n::tr("tfc_absorption"), true, s);
    optRow->addWidget(m_useDispersion);
    optRow->addWidget(m_useAbsorption);
    optRow->addStretch(1);
    f2->addRow(optRow);
    f2->addRow(noteLabel(I18n::tr("tfc_matsource"), s));
    v->addWidget(s);

    // ── 配線 ──
    connect(m_incident,  &QComboBox::currentIndexChanged, this, &ThinFilmTab::recompute);
    connect(m_substrate, &QComboBox::currentIndexChanged, this, &ThinFilmTab::recompute);
    connect(m_lambda0, &QLineEdit::editingFinished, this, &ThinFilmTab::recompute);
    connect(m_useDispersion, &QCheckBox::toggled, this, &ThinFilmTab::recompute);
    connect(m_useAbsorption, &QCheckBox::toggled, this, &ThinFilmTab::recompute);
    connect(m_layerTable, &QTableWidget::itemChanged, this, [this] {
        if (m_updating) return;
        applyLayerTable();
        recompute();
    });
    // 最終行 (「層を追加」) のクリックで層を 1 枚足す
    connect(m_layerTable, &QTableWidget::cellClicked, this, [this](int row, int) {
        if (row != m_stack.size()) return;
        StackLayer L;
        L.mat = m_stack.isEmpty() ? QStringLiteral("SiO2") : m_stack.last().mat;
        double n = 0.0;
        L.d_nm = indexOf(L.mat, lambda0(), n) && n > 0.0
                     ? 0.25 * lambda0() / n : 100.0;
        m_stack.push_back(L);
        rebuildLayerTable();
        recompute();
    });
    connect(delBtn, &QPushButton::clicked, this, [this] {
        const int row = m_layerTable->currentRow();
        if (row < 0 || row >= m_stack.size()) return;
        m_stack.removeAt(row);
        rebuildLayerTable();
        recompute();
    });

    v->addStretch(1);
    return page;
}

// ── 分光特性 / Spectral response ────────────────────────────────────────────
QWidget *ThinFilmTab::buildSpecPage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setSpacing(8);

    auto *s = new SectionBox(I18n::tr("tfc_spec_sec"), page);

    auto *aoiRow = new QHBoxLayout();
    m_aoi = numEdit(QStringLiteral("0"), 70, s);
    aoiRow->addWidget(m_aoi);
    aoiRow->addWidget(new QLabel(I18n::tr("tfc_aoi_unit"), s));
    m_angleSweep = makeCheck(I18n::tr("tfc_angle_sweep"), false, s);
    m_splitSP    = makeCheck(I18n::tr("tfc_split_sp"),    true,  s);
    aoiRow->addWidget(m_angleSweep);
    aoiRow->addWidget(m_splitSP);
    aoiRow->addStretch(1);
    s->form()->addRow(I18n::tr("tfc_aoi"), aoiRow);

    auto *lamRow = new QHBoxLayout();
    m_lamMin = numEdit(QStringLiteral("400"), 70, s);
    m_lamMax = numEdit(QStringLiteral("800"), 70, s);
    lamRow->addWidget(m_lamMin);
    lamRow->addWidget(new QLabel("〜", s));
    lamRow->addWidget(m_lamMax);
    lamRow->addWidget(new QLabel("nm", s));
    lamRow->addStretch(1);
    s->form()->addRow(I18n::tr("tfc_lam_range"), lamRow);

    // R/T スペクトル (recompute() で実計算)
    m_specPlot = new MiniPlot(s);
    m_specPlot->setMinimumSize(360, 140);
    s->vbox()->addWidget(m_specPlot);

    // 指標表 (すべて特性行列法の計算値)
    m_specTable = new QTableWidget(6, 4, s);
    m_specTable->setHorizontalHeaderLabels({ I18n::tr("tfc_c_metric"),
                                             I18n::tr("tfc_c_pol_s"),
                                             I18n::tr("tfc_c_pol_p"),
                                             I18n::tr("tfc_c_cond") });
    m_specTable->verticalHeader()->setVisible(false);
    m_specTable->verticalHeader()->setDefaultSectionSize(26);
    m_specTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_specTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_specTable->setMaximumHeight(210);
    const char *metricKeys[6] = { "tfc_m_ravg", "tfc_m_rmax", "tfc_m_rmin",
                                  "tfc_m_tavg", "tfc_m_amax", "tfc_m_gdr" };
    for (int r = 0; r < 6; ++r) {
        m_specTable->setItem(r, 0, textItem(I18n::tr(metricKeys[r])));
        for (int c = 1; c < 4; ++c)
            m_specTable->setItem(r, c, numItem(I18n::tr("tfc_na")));
    }
    s->vbox()->addWidget(m_specTable);
    m_specNote = noteLabel(QString(), s);
    s->vbox()->addWidget(m_specNote);

    auto *btnRow = new QHBoxLayout();
    // R/T は上の図と表が実計算値。以下 4 つの追加ビューは未配線 →
    // 無効化 + 「未実装」ツールチップ
    auto *rtaBtn   = new QPushButton(I18n::tr("tfc_btn_rta"), s);
    auto *mapBtn   = new QPushButton(I18n::tr("tfc_btn_map"), s);
    auto *fieldBtn = new QPushButton(I18n::tr("tfc_btn_field"), s);
    auto *fdtdBtn  = new QPushButton(I18n::tr("tfc_btn_fdtd"), s);
    for (QPushButton *b : { rtaBtn, mapBtn, fieldBtn, fdtdBtn }) {
        tabhelp::markNotImplemented(b, I18n::tr(tabhelp::notimpl::kEngine));
        btnRow->addWidget(b);
    }
    btnRow->addStretch(1);
    s->vbox()->addLayout(btnRow);
    v->addWidget(s);

    connect(m_aoi,    &QLineEdit::editingFinished, this, &ThinFilmTab::recompute);
    connect(m_lamMin, &QLineEdit::editingFinished, this, &ThinFilmTab::recompute);
    connect(m_lamMax, &QLineEdit::editingFinished, this, &ThinFilmTab::recompute);
    connect(m_angleSweep, &QCheckBox::toggled, this, &ThinFilmTab::recompute);
    connect(m_splitSP,    &QCheckBox::toggled, this, &ThinFilmTab::recompute);

    v->addStretch(1);
    return page;
}

// ── 最適化設計 / Optimization ───────────────────────────────────────────────
QWidget *ThinFilmTab::buildDesignPage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setSpacing(8);

    auto *s = new SectionBox(I18n::tr("tfc_design_sec"), page);

    auto *mRow = new QHBoxLayout();
    mRow->setSpacing(4);
    m_method = segRow(mRow, { I18n::tr("tfc_m_simplex"), I18n::tr("tfc_m_needle"),
                              I18n::tr("tfc_m_tunnel"),  I18n::tr("tfc_m_ga") },
                      0, s);                     // 既定 "simplex" (唯一の実装)
    // 実装しているのは膜厚のシンプレックス法だけ。層数や材料を変える
    // needle / tunneling / GA は未実装なので選べないようにする。
    for (QAbstractButton *b : m_method->buttons())
        if (m_method->id(b) != 0) tabhelp::markNotImplemented(b, I18n::tr(tabhelp::notimpl::kEngine));
    s->form()->addRow(I18n::tr("tfc_method"), mRow);

    auto *vRow = new QHBoxLayout();
    m_varThickness = makeCheck(I18n::tr("tfc_v_thick"), true,  s);
    m_varCount     = makeCheck(I18n::tr("tfc_v_count"), true,  s);
    m_varMaterial  = makeCheck(I18n::tr("tfc_v_mat"),   false, s);
    // 動かせるのは膜厚だけ (層数・材料の探索は未実装)
    tabhelp::markNotImplemented(m_varCount, I18n::tr(tabhelp::notimpl::kEngine));
    tabhelp::markNotImplemented(m_varMaterial, I18n::tr(tabhelp::notimpl::kEngine));
    for (QCheckBox *c : { m_varThickness, m_varCount, m_varMaterial })
        vRow->addWidget(c);
    vRow->addStretch(1);
    s->form()->addRow(I18n::tr("tfc_vars"), vRow);

    // ターゲット表 (Merit と歩留まり判定の入力。編集可)
    m_targetTable = new QTableWidget(0, 6, s);
    m_targetTable->setHorizontalHeaderLabels({ I18n::tr("tfc_c_lamrange"),
                                               I18n::tr("tfc_c_quantity"),
                                               I18n::tr("tfc_c_pol"),
                                               I18n::tr("tfc_c_goal"),
                                               I18n::tr("tfc_c_tol"),
                                               I18n::tr("tfc_c_weight") });
    m_targetTable->verticalHeader()->setVisible(false);
    m_targetTable->verticalHeader()->setDefaultSectionSize(24);
    m_targetTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_targetTable->setMaximumHeight(110);
    s->form()->addRow(I18n::tr("tfc_targets"), m_targetTable);

    auto *runRow = new QHBoxLayout();
    auto *runBtn = new QPushButton(I18n::tr("tfc_run_opt"), s);
    runBtn->setDefault(true);                    // q-btn primary
    runBtn->setToolTip(I18n::tr("tfc_run_opt_tip"));
    connect(runBtn, &QPushButton::clicked, this, &ThinFilmTab::runOptimization);
    runRow->addWidget(runBtn);
    m_meritLabel = hintLabel(QString(), s);
    runRow->addWidget(m_meritLabel);
    runRow->addStretch(1);
    s->vbox()->addLayout(runRow);
    s->vbox()->addWidget(noteLabel(I18n::tr("tfc_merit_note"), s));
    v->addWidget(s);

    connect(m_targetTable, &QTableWidget::itemChanged, this, [this] {
        if (m_updating) return;
        applyTargetTable();
        recompute();
    });

    v->addStretch(1);
    return page;
}

// ── 製造誤差・歩留まり / Manufacturing tolerance ────────────────────────────
QWidget *ThinFilmTab::buildMfgPage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setSpacing(8);

    auto *s = new SectionBox(I18n::tr("tfc_mfg_sec"), page);

    auto *dRow = new QHBoxLayout();
    dRow->setSpacing(4);
    m_deposition = segRow(dRow, { I18n::tr("tfc_d_eb"), I18n::tr("tfc_d_ibs"),
                                  I18n::tr("tfc_d_ald"), I18n::tr("tfc_d_sput") },
                          1, s);                 // 既定 "ibs"
    // 成膜法ごとの誤差モデルは未実装 (代表値を勝手に当てはめない)
    for (QAbstractButton *b : m_deposition->buttons()) tabhelp::markNotImplemented(b, I18n::tr(tabhelp::notimpl::kEngine));
    s->form()->addRow(I18n::tr("tfc_depo"), dRow);

    auto *eRow = new QHBoxLayout();
    m_thickErr = numEdit(QStringLiteral("0.5"), 70, s);
    eRow->addWidget(m_thickErr);
    eRow->addWidget(new QLabel(I18n::tr("tfc_thickerr_unit"), s));
    eRow->addStretch(1);
    s->form()->addRow(I18n::tr("tfc_thickerr"), eRow);

    auto *cRow = new QHBoxLayout();
    m_systematic = makeCheck(I18n::tr("tfc_systematic"), true,  s);
    m_correlated = makeCheck(I18n::tr("tfc_correlated"), false, s);
    tabhelp::markNotImplemented(m_correlated, I18n::tr(tabhelp::notimpl::kModel));   // 相関モデルは未実装
    cRow->addWidget(m_systematic);
    cRow->addWidget(m_correlated);
    cRow->addStretch(1);
    s->form()->addRow(cRow);

    auto *monRow = new QHBoxLayout();
    monRow->setSpacing(4);
    m_monitoring = segRow(monRow, { I18n::tr("tfc_mon_quartz"),
                                    I18n::tr("tfc_mon_optical"),
                                    I18n::tr("tfc_mon_both") },
                          1, s);                 // 既定 "optical"
    // モニタリング方式による誤差の違いは未実装
    for (QAbstractButton *b : m_monitoring->buttons()) tabhelp::markNotImplemented(b, I18n::tr(tabhelp::notimpl::kEngine));
    s->form()->addRow(I18n::tr("tfc_monitor"), monRow);

    auto *mcRow = new QHBoxLayout();
    m_mcButton = new QPushButton(I18n::tr("tfc_run_mc"), s);
    m_mcButton->setDefault(true);                // q-btn primary
    mcRow->addWidget(m_mcButton);
    mcRow->addStretch(1);
    s->vbox()->addLayout(mcRow);

    auto *yRow = new QHBoxLayout();
    m_yieldBadge = makeBadge(I18n::tr("tfc_yield_pending"), "", s);
    yRow->addWidget(m_yieldBadge);
    yRow->addStretch(1);
    s->vbox()->addLayout(yRow);
    m_sensitiveLabel = hintLabel(I18n::tr("tfc_sens_na"), s);
    s->vbox()->addWidget(m_sensitiveLabel);
    s->vbox()->addWidget(noteLabel(I18n::tr("tfc_mc_note"), s));
    s->vbox()->addWidget(noteLabel(I18n::tr("tfc_mfg_unwired"), s));

    auto *btnRow = new QHBoxLayout();
    auto *recipeBtn = new QPushButton(I18n::tr("tfc_btn_recipe"), s);
    auto *sensBtn   = new QPushButton(I18n::tr("tfc_btn_sens"), s);
    connect(recipeBtn, &QPushButton::clicked, this, &ThinFilmTab::exportRecipe);
    connect(sensBtn,   &QPushButton::clicked, this, &ThinFilmTab::showSensitivity);
    btnRow->addWidget(recipeBtn);
    btnRow->addWidget(sensBtn);
    btnRow->addStretch(1);
    s->vbox()->addLayout(btnRow);
    v->addWidget(s);

    connect(m_mcButton, &QPushButton::clicked, this, &ThinFilmTab::runMonteCarlo);
    // 誤差設定を変えたら前回の歩留まりは無効 (実行結果でないものを残さない)
    connect(m_thickErr, &QLineEdit::editingFinished, this, [this] {
        m_yieldBadge->setText(I18n::tr("tfc_yield_pending"));
        m_yieldBadge->setStyleSheet(badgeCss(""));
    });
    connect(m_systematic, &QCheckBox::toggled, this, [this] {
        m_yieldBadge->setText(I18n::tr("tfc_yield_pending"));
        m_yieldBadge->setStyleSheet(badgeCss(""));
    });

    v->addStretch(1);
    return page;
}

// ── 表 → データ ────────────────────────────────────────────────────────────
void ThinFilmTab::applyLayerTable()
{
    for (int r = 0; r < m_stack.size(); ++r) {
        if (QTableWidgetItem *it = m_layerTable->item(r, 0))
            m_stack[r].enabled = (it->checkState() == Qt::Checked);
        if (QTableWidgetItem *it = m_layerTable->item(r, 4)) {
            bool ok = false;
            const double v = it->text().trimmed().toDouble(&ok);
            if (ok && v >= 0.0) m_stack[r].k = v;
        }
        if (QTableWidgetItem *it = m_layerTable->item(r, 5)) {
            bool ok = false;
            const double v = it->text().trimmed().toDouble(&ok);
            if (ok && v >= 0.0) m_stack[r].d_nm = v;
        }
    }
}

void ThinFilmTab::applyTargetTable()
{
    for (int r = 0; r < m_targets.size(); ++r) {
        auto num = [this, r](int c, double &dst, double lo) {
            QTableWidgetItem *it = m_targetTable->item(r, c);
            if (!it) return;
            bool ok = false;
            const double v = it->text().trimmed().toDouble(&ok);
            if (ok && v >= lo) dst = v;
        };
        // λ 範囲は "450-650" 形式
        if (QTableWidgetItem *it = m_targetTable->item(r, 0)) {
            const QStringList parts = it->text().split(QLatin1Char('-'),
                                                       Qt::SkipEmptyParts);
            if (parts.size() == 2) {
                bool a = false, b = false;
                const double l0 = parts[0].trimmed().toDouble(&a);
                const double l1 = parts[1].trimmed().toDouble(&b);
                if (a && b && l0 > 0 && l1 > 0) {
                    m_targets[r].lam0 = std::min(l0, l1);
                    m_targets[r].lam1 = std::max(l0, l1);
                }
            }
        }
        if (QTableWidgetItem *it = m_targetTable->item(r, 1)) {
            const QString t = it->text().trimmed().toUpper();
            if (t == QLatin1String("R")) m_targets[r].quantity = 0;
            else if (t == QLatin1String("T")) m_targets[r].quantity = 1;
        }
        if (QTableWidgetItem *it = m_targetTable->item(r, 2)) {
            const QString t = it->text().trimmed().toLower();
            if (t == QLatin1String("s")) m_targets[r].pol = 1;
            else if (t == QLatin1String("p")) m_targets[r].pol = 2;
            else m_targets[r].pol = 0;
        }
        num(3, m_targets[r].goal, 0.0);
        num(4, m_targets[r].tol, 1e-9);
        num(5, m_targets[r].weight, 1e-9);
    }
}

// ── データ → 表 ────────────────────────────────────────────────────────────
void ThinFilmTab::rebuildLayerTable()
{
    const bool wasUpdating = m_updating;
    m_updating = true;

    m_layerTable->clearSpans();
    m_layerTable->setRowCount(0);
    m_layerTable->setRowCount(m_stack.size() + 1);

    const std::vector<optics::MaterialInfo> &mats = optics::materials();
    for (int r = 0; r < m_stack.size(); ++r) {
        const StackLayer &L = m_stack[r];
        auto *sel = new QTableWidgetItem();
        sel->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        sel->setCheckState(L.enabled ? Qt::Checked : Qt::Unchecked);
        sel->setToolTip(I18n::tr("tfc_layer_enable"));
        m_layerTable->setItem(r, 0, sel);
        m_layerTable->setItem(r, 1, numItem(QString::number(r + 1)));

        auto *combo = new QComboBox(m_layerTable);
        int current = -1;
        for (size_t i = 0; i < mats.size(); ++i) {
            combo->addItem(QString::fromUtf8(mats[i].id));
            combo->setItemData(int(i), QString::fromUtf8(mats[i].label),
                               Qt::ToolTipRole);
            if (QString::fromUtf8(mats[i].id) == L.mat) current = int(i);
        }
        if (current >= 0) combo->setCurrentIndex(current);
        connect(combo, &QComboBox::currentTextChanged, this,
                [this, r](const QString &id) {
                    if (r < 0 || r >= m_stack.size()) return;
                    m_stack[r].mat = id;
                    recompute();     // 表の作り直しはしない (送信元を壊さない)
                });
        m_layerTable->setCellWidget(r, 2, combo);

        m_layerTable->setItem(r, 3, numItem(I18n::tr("tfc_na")));
        m_layerTable->setItem(r, 4, editNumItem(QString::number(L.k, 'f', 4)));
        m_layerTable->setItem(r, 5, editNumItem(QString::number(L.d_nm, 'f', 2)));
        m_layerTable->setItem(r, 6, numItem(I18n::tr("tfc_na")));
        m_layerTable->setItem(r, 7, textItem(QString()));
    }
    // 追加行 (チェック無し + 残り列を結合したイタリック行)
    const int addRow = m_stack.size();
    auto *addSel = new QTableWidgetItem();
    addSel->setFlags(Qt::ItemIsEnabled);
    m_layerTable->setItem(addRow, 0, addSel);
    auto *addIt = textItem(I18n::tr("tfc_layer_add"));
    QFont italic = addIt->font();
    italic.setItalic(true);
    addIt->setFont(italic);
    m_layerTable->setItem(addRow, 1, addIt);
    m_layerTable->setSpan(addRow, 1, 1, 7);

    m_layerBadge->setText(I18n::tr("tfc_layers_n").arg(m_stack.size()));
    m_updating = wasUpdating;
}

void ThinFilmTab::rebuildTargetTable()
{
    const bool wasUpdating = m_updating;
    m_updating = true;
    m_targetTable->setRowCount(m_targets.size());
    for (int r = 0; r < m_targets.size(); ++r) {
        const TargetRow &t = m_targets[r];
        m_targetTable->setItem(r, 0, monoItem(QString::number(t.lam0, 'g', 6) + "-" +
                                              QString::number(t.lam1, 'g', 6)));
        m_targetTable->item(r, 0)->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable |
                                            Qt::ItemIsEditable);
        m_targetTable->setItem(r, 1, new QTableWidgetItem(
            I18n::tr(t.quantity == 1 ? "tfc_q_t" : "tfc_q_r")));
        m_targetTable->setItem(r, 2, new QTableWidgetItem(
            t.pol == 1 ? I18n::tr("tfc_pol_s")
                       : t.pol == 2 ? I18n::tr("tfc_pol_p")
                                    : I18n::tr("tfc_pol_avg")));
        m_targetTable->setItem(r, 3, editNumItem(QString::number(t.goal, 'g', 6)));
        m_targetTable->setItem(r, 4, editNumItem(QString::number(t.tol, 'g', 6)));
        m_targetTable->setItem(r, 5, editNumItem(QString::number(t.weight, 'g', 6)));
    }
    m_updating = wasUpdating;
}

// ── 計算 ────────────────────────────────────────────────────────────────────
std::vector<optics::TargetBand> ThinFilmTab::targetBands() const
{
    std::vector<optics::TargetBand> out;
    for (const TargetRow &t : m_targets) {
        optics::TargetBand b;
        b.lam0_nm = t.lam0;
        b.lam1_nm = t.lam1;
        b.q   = (t.quantity == 1) ? optics::Quantity::T : optics::Quantity::R;
        b.pol = (t.pol == 1) ? optics::PolMode::S
              : (t.pol == 2) ? optics::PolMode::P : optics::PolMode::Average;
        b.goal   = t.goal / 100.0;
        b.tol    = (t.tol > 0.0) ? t.tol / 100.0 : 1e-9;
        b.weight = (t.weight > 0.0) ? t.weight : 1e-9;
        b.samples = std::max(1, t.samples);
        out.push_back(b);
    }
    return out;
}

// 現在の設定から λ [nm] の層構成を返すコールバックを作る。
// 材料データの有効範囲外では false を返す (外挿しない)。
optics::StackAtLambda ThinFilmTab::makeStackFn() const
{
    struct Setup {
        QString incId, subId;
        std::vector<std::pair<QString, double>> layers;   // 材料 id, 膜厚
        std::vector<double> k;
        double lam0;
        bool dispersion;
    };
    Setup st;
    st.incId = QString::fromUtf8(kIncidentIds[qBound(0, m_incident->currentIndex(), 1)]);
    st.subId = QString::fromUtf8(kSubstrateIds[qBound(0, m_substrate->currentIndex(), 4)]);
    st.lam0 = lambda0();
    st.dispersion = m_useDispersion->isChecked();
    const bool absorb = m_useAbsorption->isChecked();
    for (const StackLayer &L : m_stack) {
        if (!L.enabled) continue;
        st.layers.push_back({ L.mat, L.d_nm });
        st.k.push_back(absorb ? L.k : 0.0);
    }

    return [st](double lam_nm, optics::StackSample &out) {
        // 分散を切ったときは λ₀ の屈折率を全波長に使う (近似であることは
        // チェックボックスの表記そのもの)
        const double lref = st.dispersion ? lam_nm : st.lam0;
        double n0 = 0.0, ns = 0.0;
        if (!indexOf(st.incId, lref, n0)) return false;
        if (!indexOf(st.subId, lref, ns)) return false;
        out.n0 = n0;
        out.nsub = ns;
        out.ksub = 0.0;
        out.layers.clear();
        out.layers.reserve(st.layers.size());
        for (size_t i = 0; i < st.layers.size(); ++i) {
            double n = 0.0;
            if (!indexOf(st.layers[i].first, lref, n)) return false;
            optics::FilmLayer L;
            L.n = n;
            L.k = st.k[i];
            L.d_nm = st.layers[i].second;
            out.layers.push_back(L);
        }
        return true;
    };
}

void ThinFilmTab::recompute()
{
    if (m_updating) return;
    if (!m_specPlot || !m_specTable || !m_meritLabel) return;

    const double aoi = aoiDeg();
    const double lam0 = lambda0();
    const optics::StackAtLambda fn = makeStackFn();

    // ── n(λ₀) / 光学膜厚 / 役割の列を更新 ──
    updateDerivedCells();

    // ── スペクトル (図と指標表) ──
    bool aOk = false, bOk = false;
    double lo = m_lamMin->text().trimmed().toDouble(&aOk);
    double hi = m_lamMax->text().trimmed().toDouble(&bOk);
    if (!aOk || lo <= 0) lo = 400.0;
    if (!bOk || hi <= 0) hi = 800.0;
    if (hi < lo) std::swap(lo, hi);
    const int want = 121;
    const std::vector<optics::SpectrumPoint> sp =
        optics::spectrum(fn, lo, hi, want, aoi, true);

    // プロット (横軸は λ、角度スイープ時は入射角)。
    // R が小さい設計 (AR 等) では R と T を同じ軸に載せると R の起伏が
    // 潰れるので、R が 10 % 未満に収まっているときは R だけを描く。
    // どちらを描いたかは軸ラベルに出す (推測させない)。
    const bool split = m_splitSP->isChecked();
    const bool sweep = m_angleSweep->isChecked();
    std::vector<optics::AnglePoint> ap;
    if (sweep) ap = optics::angleSweep(fn, lam0, 0.0, 60.0, 61);

    double rPeak = 0.0;
    if (sweep) for (const optics::AnglePoint &p : ap)
                   rPeak = std::max(rPeak, std::max(p.Rs, p.Rp));
    else       for (const optics::SpectrumPoint &p : sp)
                   rPeak = std::max(rPeak, std::max(p.Rs, p.Rp));
    const bool rOnly = (rPeak < 0.10);

    MiniSeries rs, rp, ts, tp;
    rs.color = QColor("#0078D4");  rp.color = QColor("#0078D4"); rp.dashed = true;
    ts.color = QColor("#107C10");  tp.color = QColor("#107C10"); tp.dashed = true;
    auto addPoint = [&](double x, double Rs, double Rp, double Ts, double Tp) {
        if (split) {
            rs.pts.push_back({ x, pct(Rs) });
            rp.pts.push_back({ x, pct(Rp) });
            ts.pts.push_back({ x, pct(Ts) });
            tp.pts.push_back({ x, pct(Tp) });
        } else {
            rs.pts.push_back({ x, pct(0.5 * (Rs + Rp)) });
            ts.pts.push_back({ x, pct(0.5 * (Ts + Tp)) });
        }
    };
    if (sweep)
        for (const optics::AnglePoint &p : ap)
            addPoint(p.aoi_deg, p.Rs, p.Rp, p.Ts, p.Tp);
    else
        for (const optics::SpectrumPoint &p : sp)
            addPoint(p.lambda_nm, p.Rs, p.Rp, p.Ts, p.Tp);

    QVector<MiniSeries> series;
    series << rs;
    if (split) series << rp;
    if (!rOnly) {
        series << ts;
        if (split) series << tp;
    }
    m_specPlot->setLabels(I18n::tr(sweep ? "tfc_x_aoi" : "tfc_x_lam"),
                          I18n::tr(rOnly ? "tfc_y_r" : "tfc_y_rt"));
    m_specPlot->setSeries(series);

    // ── 指標表 ──
    const bool wasUpdating = m_updating;
    m_updating = true;
    if (sp.empty()) {
        for (int r = 0; r < 6; ++r)
            for (int c = 1; c < 4; ++c)
                m_specTable->item(r, c)->setText(I18n::tr("tfc_na"));
        m_specNote->setText(I18n::tr("tfc_spec_fail"));
    } else {
        double rAvg[2] = { 0, 0 }, rMax[2] = { 0, 0 }, rMin[2] = { 1, 1 };
        double tAvg[2] = { 0, 0 }, aMax[2] = { 0, 0 };
        double gdLo[2] = { 0, 0 }, gdHi[2] = { 0, 0 };
        bool gdAny = false;
        for (const optics::SpectrumPoint &p : sp) {
            const double R[2] = { p.Rs, p.Rp };
            const double T[2] = { p.Ts, p.Tp };
            const double A[2] = { p.As, p.Ap };
            const double G[2] = { p.gds_ps, p.gdp_ps };
            for (int i = 0; i < 2; ++i) {
                rAvg[i] += R[i]; tAvg[i] += T[i];
                rMax[i] = std::max(rMax[i], R[i]);
                rMin[i] = std::min(rMin[i], R[i]);
                aMax[i] = std::max(aMax[i], A[i]);
                if (p.gdValid) {
                    if (!gdAny) { gdLo[i] = gdHi[i] = G[i]; }
                    else { gdLo[i] = std::min(gdLo[i], G[i]);
                           gdHi[i] = std::max(gdHi[i], G[i]); }
                }
            }
            if (p.gdValid) gdAny = true;
        }
        const double npts = double(sp.size());
        for (int i = 0; i < 2; ++i) { rAvg[i] /= npts; tAvg[i] /= npts; }
        const QString cond = I18n::tr("tfc_cond_fmt")
                                 .arg(QString::number(sp.front().lambda_nm, 'f', 0),
                                      QString::number(sp.back().lambda_nm, 'f', 0),
                                      QString::number(aoi, 'f', 1));
        for (int i = 0; i < 2; ++i) {
            const int c = 1 + i;
            m_specTable->item(0, c)->setText(fmtPct(rAvg[i]));
            m_specTable->item(1, c)->setText(fmtPct(rMax[i]));
            m_specTable->item(2, c)->setText(fmtPct(rMin[i]));
            m_specTable->item(3, c)->setText(fmtPct(tAvg[i]));
            m_specTable->item(4, c)->setText(fmtPct(aMax[i]));
            m_specTable->item(5, c)->setText(
                gdAny ? QString::number(gdHi[i] - gdLo[i], 'f', 3) + " ps"
                      : I18n::tr("tfc_na"));
        }
        for (int r = 0; r < 6; ++r) m_specTable->item(r, 3)->setText(cond);
        QString note = I18n::tr("tfc_spec_note")
                           .arg(QString::number(sp.front().lambda_nm, 'f', 1),
                                QString::number(sp.back().lambda_nm, 'f', 1))
                           .arg(int(sp.size()));
        if (int(sp.size()) < want)
            note += I18n::tr("tfc_spec_skip").arg(want - int(sp.size()));
        m_specNote->setText(note);
    }
    m_updating = wasUpdating;

    // ── Merit (現在の層構成の評価値。最適化はしていない) ──
    const std::vector<optics::TargetBand> tb = targetBands();
    const optics::MeritResult mr = optics::merit(fn, tb, aoi);
    if (mr.valid)
        m_meritLabel->setText(I18n::tr("tfc_merit_fmt")
                                  .arg(QString::number(mr.merit, 'f', 4))
                                  .arg(mr.used));
    else
        m_meritLabel->setText(I18n::tr("tfc_merit_na"));

    // ── 膜厚感度 (最も敏感な層) ──
    const double delta = 0.5;              // 中心差分の片側摂動 [nm]
    const optics::SensitivityResult sr =
        optics::thicknessSensitivity(fn, tb, aoi, delta);
    // 感度の index は「有効な層」だけの並びなので、表の行番号へ戻す
    QVector<int> activeRows;
    for (int i = 0; i < m_stack.size(); ++i)
        if (m_stack[i].enabled) activeRows.push_back(i);
    if (sr.valid && sr.worst >= 0 && sr.worst < activeRows.size()) {
        const int row = activeRows[sr.worst];
        m_sensitiveLabel->setText(
            I18n::tr("tfc_sens_fmt")
                .arg(row + 1)
                .arg(matLabel(m_stack[row].mat),
                     QString::number(sr.dQ_pctPerNm[size_t(sr.worst)], 'f', 3),
                     QString::number(delta, 'f', 1)));
    } else {
        m_sensitiveLabel->setText(I18n::tr("tfc_sens_na"));
    }

    // 層構成が変わったら前回の歩留まりは無効 (実行結果でないものを残さない)
    m_yieldBadge->setText(I18n::tr("tfc_yield_pending"));
    m_yieldBadge->setStyleSheet(badgeCss(""));
}

// n(λ₀)・光学膜厚・役割の列だけ更新する (行構成やセルウィジェットは触らない)
void ThinFilmTab::updateDerivedCells()
{
    if (!m_layerTable) return;
    const bool wasUpdating = m_updating;
    m_updating = true;
    const double lam0 = lambda0();

    // 役割判定用に n の平均を取る
    double nSum = 0.0;
    int    nCnt = 0;
    std::vector<double> ns(size_t(m_stack.size()), 0.0);
    std::vector<char>   nOk(size_t(m_stack.size()), 0);
    for (int r = 0; r < m_stack.size(); ++r) {
        double n = 0.0;
        if (indexOf(m_stack[r].mat, lam0, n)) {
            ns[size_t(r)] = n; nOk[size_t(r)] = 1;
            nSum += n; ++nCnt;
        }
    }
    const double nMean = (nCnt > 0) ? nSum / nCnt : 0.0;

    for (int r = 0; r < m_stack.size(); ++r) {
        if (r >= m_layerTable->rowCount()) break;
        QTableWidgetItem *nIt = m_layerTable->item(r, 3);
        QTableWidgetItem *qIt = m_layerTable->item(r, 6);
        QTableWidgetItem *rIt = m_layerTable->item(r, 7);
        if (!nIt || !qIt || !rIt) continue;
        if (nOk[size_t(r)]) {
            const double n = ns[size_t(r)];
            nIt->setText(QString::number(n, 'f', 4));
            qIt->setText(lam0 > 0 ? QString::number(n * m_stack[r].d_nm / lam0,
                                                    'f', 3)
                                  : I18n::tr("tfc_na"));
            const char *role = (r == 0) ? "tfc_r_outer"
                             : (r == m_stack.size() - 1) ? "tfc_r_sub"
                             : (n >= nMean) ? "tfc_r_high" : "tfc_r_low";
            rIt->setText(I18n::tr(role));
        } else {
            nIt->setText(I18n::tr("tfc_na"));
            qIt->setText(I18n::tr("tfc_na"));
            rIt->setText(I18n::tr("tfc_na"));
        }
    }
    m_updating = wasUpdating;
}

// ── 周期記法の展開 ──────────────────────────────────────────────────────────
// `Air | (H L)^12 H | Sub  H=Si3N4 L=SiO2 @ 1550nm` を層構成へ展開する。
// 記号に材料が割り当てられていない / 材料が材料表に無い / その波長で屈折率が
// 得られない場合は **何も変更せず** 理由を出す (誤った層構成を作らない)。
void ThinFilmTab::expandPeriodic()
{
    const QString title = I18n::tr("tfc_expand_title");
    const optics::NotationResult r =
        optics::parseNotation(m_periodic->text().toStdString());
    if (!r.ok) {
        QMessageBox::warning(this, title,
                             I18n::tr("tfc_expand_bad")
                                 .arg(QString::fromStdString(r.error)));
        return;
    }

    // 設計波長: 記法に `@ …` があればそれを使う (先に決める — 屈折率の解決に要る)
    const double lam0 = (r.lambda0_nm > 0.0) ? r.lambda0_nm : lambda0();

    // 記号 → 材料 id。割当が無い記号があれば失敗させる。
    std::map<char, QString> mat;
    for (const optics::NotationLayer &L : r.layers) {
        if (mat.count(L.symbol)) continue;
        const auto it = r.assign.find(L.symbol);
        if (it == r.assign.end()) {
            QMessageBox::warning(this, title,
                                 I18n::tr("tfc_expand_nomat")
                                     .arg(QChar(L.symbol)));
            return;
        }
        const QString id = QString::fromStdString(it->second);
        double n = 0.0;
        if (!indexOf(id, lam0, n) || !(n > 0.0)) {
            QMessageBox::warning(this, title,
                                 I18n::tr("tfc_expand_badmat")
                                     .arg(id)
                                     .arg(QString::number(lam0, 'g', 6)));
            return;
        }
        mat[L.symbol] = id;
    }

    // 入射媒質 / 基板: 記法にあれば combo を合わせる (合わなければ現状維持)
    auto matchCombo = [](QComboBox *box, const char *const *ids, int count,
                         const std::string &name) {
        if (name.empty()) return;
        const QString want = QString::fromStdString(name).trimmed();
        for (int i = 0; i < count; ++i) {
            const QString id = QString::fromUtf8(ids[i]);
            if (id.compare(want, Qt::CaseInsensitive) == 0
                || matLabel(id).compare(want, Qt::CaseInsensitive) == 0) {
                box->setCurrentIndex(i);
                return;
            }
        }
    };

    const bool wasUpdating = m_updating;
    m_updating = true;
    matchCombo(m_incident,  kIncidentIds,  2, r.incident);
    matchCombo(m_substrate, kSubstrateIds, 5, r.substrate);
    if (r.lambda0_nm > 0.0) m_lambda0->setText(QString::number(lam0, 'g', 6));

    // 光学膜厚 (QWOT) → 物理膜厚 d = qwot·λ₀/(4·n(λ₀))
    m_stack.clear();
    for (const optics::NotationLayer &L : r.layers) {
        StackLayer sl;
        sl.mat = mat[L.symbol];
        sl.k = 0.0;
        double n = 0.0;
        indexOf(sl.mat, lam0, n);
        sl.d_nm = L.qwot * lam0 / (4.0 * n);
        sl.enabled = true;
        m_stack.push_back(sl);
    }
    m_updating = wasUpdating;

    rebuildLayerTable();
    recompute();
}

// ── 膜厚最適化 (シンプレックス法) ──────────────────────────────────────────
// 層数・材料は固定し、有効な層の**物理膜厚だけ**を動かしてメリット関数を
// 最小化する。needle / tunneling / GA (層数や材料を変える手法) は未実装で、
// ボタン自体が無効化してある。
void ThinFilmTab::runOptimization()
{
    const QString title = I18n::tr("tfc_opt_title");
    if (!m_varThickness->isChecked()) {
        QMessageBox::information(this, title, I18n::tr("tfc_opt_novar"));
        return;
    }
    // 有効な層だけを最適化対象にする (makeStackFn が無効層を除くのと揃える)
    std::vector<int> idx;
    std::vector<double> d0;
    for (int i = 0; i < m_stack.size(); ++i)
        if (m_stack[i].enabled) { idx.push_back(i); d0.push_back(m_stack[i].d_nm); }
    if (d0.empty()) {
        QMessageBox::information(this, title, I18n::tr("tfc_opt_nolayer"));
        return;
    }
    const std::vector<optics::TargetBand> tb = targetBands();
    if (tb.empty()) {
        QMessageBox::information(this, title, I18n::tr("tfc_opt_notarget"));
        return;
    }

    optics::OptimizeOptions o;      // 既定 (600 反復・膜厚 1〜5000 nm)
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const optics::OptimizeResult res =
        optics::optimizeThickness(makeStackFn(), tb, aoiDeg(), d0, o);
    QApplication::restoreOverrideCursor();

    if (!res.valid || res.d_nm.size() != idx.size()) {
        QMessageBox::warning(this, title, I18n::tr("tfc_opt_fail"));
        return;
    }
    // 改善しなかった場合も結果を書き戻さない (公称設計を壊さない)
    if (!(res.meritEnd < res.meritStart)) {
        QMessageBox::information(this, title,
                                 I18n::tr("tfc_opt_noimprove")
                                     .arg(QString::number(res.meritStart, 'g', 4)));
        return;
    }

    const bool wasUpdating = m_updating;
    m_updating = true;
    for (size_t j = 0; j < idx.size(); ++j)
        m_stack[idx[j]].d_nm = res.d_nm[j];
    m_updating = wasUpdating;

    rebuildLayerTable();
    recompute();
    QMessageBox::information(this, title,
                             I18n::tr("tfc_opt_done")
                                 .arg(QString::number(res.meritStart, 'g', 4))
                                 .arg(QString::number(res.meritEnd, 'g', 4))
                                 .arg(res.iterations)
                                 .arg(I18n::tr(res.converged ? "tfc_opt_conv"
                                                             : "tfc_opt_maxiter")));
}

// ── 成膜レシピの書き出し ────────────────────────────────────────────────────
// 実際に成膜するときに要るもの (層順・材料・物理膜厚・光学膜厚) と、その値が
// 何を前提にしているか (λ₀・入射角・分散/吸収の扱い) を 1 ファイルにまとめる。
void ThinFilmTab::exportRecipe()
{
    const double lam0 = lambda0();
    QString out;
    out += QStringLiteral("# OpenFDTD-X thin-film recipe\n");
    out += QStringLiteral("# lambda0[nm] = %1\n").arg(lam0, 0, 'g', 8);
    out += QStringLiteral("# aoi[deg] = %1\n").arg(aoiDeg(), 0, 'g', 6);
    out += QStringLiteral("# incident = %1\n")
               .arg(matLabel(QString::fromUtf8(
                   kIncidentIds[qBound(0, m_incident->currentIndex(), 1)])));
    out += QStringLiteral("# substrate = %1\n")
               .arg(matLabel(QString::fromUtf8(
                   kSubstrateIds[qBound(0, m_substrate->currentIndex(), 4)])));
    out += QStringLiteral("# dispersion = %1, absorption = %2\n")
               .arg(m_useDispersion->isChecked() ? "on" : "off")
               .arg(m_useAbsorption->isChecked() ? "on" : "off");
    out += QStringLiteral("# thickness tolerance 1sigma[%] = %1%2\n")
               .arg(m_thickErr->text().trimmed())
               .arg(m_systematic->isChecked() ? " (+ systematic drift)" : "");
    out += QStringLiteral("# deposition order: layer 1 is next to the incident "
                          "medium (deposit from the substrate side upward)\n");
    out += QStringLiteral("layer,material,n@lambda0,k,d_phys[nm],"
                          "optical_thickness[nd/lambda0],enabled\n");

    for (int i = 0; i < m_stack.size(); ++i) {
        const StackLayer &L = m_stack[i];
        double n = 0.0;
        const bool haveN = indexOf(L.mat, lam0, n) && n > 0.0;
        out += QStringLiteral("%1,%2,%3,%4,%5,%6,%7\n")
                   .arg(i + 1)
                   .arg(matLabel(L.mat))
                   .arg(haveN ? QString::number(n, 'f', 5)
                              : I18n::tr("tfc_na"))
                   .arg(L.k, 0, 'f', 5)
                   .arg(L.d_nm, 0, 'f', 3)
                   .arg(haveN ? QString::number(n * L.d_nm / lam0, 'f', 5)
                              : I18n::tr("tfc_na"))
                   .arg(L.enabled ? 1 : 0);
    }
    tabhelp::saveTextFile(this, I18n::tr("tfc_recipe_title"),
                          QStringLiteral("thinfilm_recipe.csv"),
                          I18n::tr("tfc_recipe_filter"), out);
}

// ── 膜厚感度の一覧 ──────────────────────────────────────────────────────────
// 「どの層をどれだけ精密に作る必要があるか」を層ごとに出す。まとめ行
// (最悪層) はタブ本体に出ているので、ここは全層の内訳を見せる。
void ThinFilmTab::showSensitivity()
{
    const QString title = I18n::tr("tfc_sens_title");
    const optics::SensitivityResult s =
        optics::thicknessSensitivity(makeStackFn(), targetBands(), aoiDeg(), 0.5);
    if (!s.valid || s.dQ_pctPerNm.empty()) {
        QMessageBox::warning(this, title, I18n::tr("tfc_sens_fail"));
        return;
    }
    // 感度は「有効な層」だけに対して返るので、対応する層番号を作る
    std::vector<int> idx;
    for (int i = 0; i < m_stack.size(); ++i)
        if (m_stack[i].enabled) idx.push_back(i);
    if (idx.size() != s.dQ_pctPerNm.size()) {
        QMessageBox::warning(this, title, I18n::tr("tfc_sens_fail"));
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(title);
    auto *v = new QVBoxLayout(&dlg);
    v->addWidget(new QLabel(I18n::tr("tfc_sens_head"), &dlg));

    auto *tbl = new QTableWidget(int(idx.size()), 4, &dlg);
    tbl->setHorizontalHeaderLabels({ I18n::tr("tfc_c_layer"),
                                     I18n::tr("tfc_c_mat"),
                                     I18n::tr("tfc_c_dphys"),
                                     I18n::tr("tfc_c_sens") });
    tbl->verticalHeader()->setVisible(false);
    tbl->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tbl->setEditTriggers(QAbstractItemView::NoEditTriggers);
    for (size_t r = 0; r < idx.size(); ++r) {
        const StackLayer &L = m_stack[idx[r]];
        tbl->setItem(int(r), 0, numItem(QString::number(idx[r] + 1)));
        tbl->setItem(int(r), 1, textItem(matLabel(L.mat)));
        tbl->setItem(int(r), 2, numItem(QString::number(L.d_nm, 'f', 2)));
        tbl->setItem(int(r), 3,
                     numItem(QString::number(s.dQ_pctPerNm[r], 'f', 4)));
        if (int(r) == s.worst)
            for (int c = 0; c < 4; ++c) {
                QFont f = tbl->item(int(r), c)->font();
                f.setBold(true);
                tbl->item(int(r), c)->setFont(f);
            }
    }
    tbl->setMinimumSize(460, 260);
    v->addWidget(tbl);
    v->addWidget(new QLabel(I18n::tr("tfc_sens_note"), &dlg));

    auto *box = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    v->addWidget(box);
    dlg.exec();
}

// ── 製造誤差モンテカルロ ────────────────────────────────────────────────────
void ThinFilmTab::runMonteCarlo()
{
    const optics::StackAtLambda fn = makeStackFn();
    const std::vector<optics::TargetBand> tb = targetBands();

    optics::ToleranceOptions o;
    o.trials = 1000;
    bool sOk = false;
    const double sigmaPct = m_thickErr->text().trimmed().toDouble(&sOk);
    o.sigmaRel = (sOk && sigmaPct >= 0.0) ? sigmaPct / 100.0 : 0.005;
    o.systematic = m_systematic->isChecked();

    // 1000 試行 × 数十波長で数百 ms 程度 (43 層で実測 ~0.2 s)。
    // 進行中であることを砂時計カーソルで示す。
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const optics::ToleranceResult r = optics::monteCarlo(fn, tb, aoiDeg(), o);
    QApplication::restoreOverrideCursor();

    if (!r.valid) {
        m_yieldBadge->setText(I18n::tr("tfc_yield_fail"));
        m_yieldBadge->setStyleSheet(badgeCss("err"));
        return;
    }
    m_yieldBadge->setText(I18n::tr("tfc_yield_fmt")
                              .arg(QString::number(r.yield * 100.0, 'f', 1))
                              .arg(r.passed)
                              .arg(r.trials));
    m_yieldBadge->setStyleSheet(badgeCss(r.yield >= 0.9 ? "ok"
                                       : r.yield >= 0.5 ? "warn" : "err"));
}
