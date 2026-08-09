// PhotonicsSolversTab.cpp
#include "PhotonicsSolversTab.h"
#include "TabHelpers.h"
#include "../core/Project.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"
#include "../Theme.h"

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <cmath>

using namespace ofd;

// ── タブ固有語彙 (psol_) — file-local 登録 ──────────────────────────────────
namespace {
const bool s_i18n = [] {
    using ofd::I18n;
    I18n::reg("psol_title", "光学ソルバ選択", "Photonics solver");
    I18n::reg("psol_hint",
        "光解析は4種のソルバを問題に応じて使い分けます。\n"
        "同じプロジェクト形状を異なるソルバで検証可能 (Cross-validation)。",
        "Photonic analysis uses four solvers depending on the problem.\n"
        "The same project geometry can be verified with different solvers "
        "(cross-validation).");
    I18n::reg("psol_selected", "選択中", "Selected");
    I18n::reg("psol_best", "得意", "Best for");
    I18n::reg("psol_weak", "弱点", "Weakness");
    I18n::reg("psol_cost", "計算量", "Cost");
    I18n::reg("psol_examples", "適用例", "Examples");

    // カード本文 (mock の methods[] をそのまま)
    I18n::reg("psol_fdtd_full", "Finite-Difference Time-Domain",
                                "Finite-Difference Time-Domain");
    I18n::reg("psol_fdtd_best", "広帯域・任意形状・分散材料・パルス応答",
        "broadband, arbitrary shapes, dispersive materials, pulse response");
    I18n::reg("psol_fdtd_weak", "高Qは長時間, 周期構造は非効率",
        "high-Q takes long, periodic structures inefficient");
    I18n::reg("psol_fdtd_cost", "中-高 (10⁶~10⁸ セル)", "medium-high (10⁶~10⁸ cells)");
    I18n::reg("psol_fdtd_ex", "BPF, リング共振器, メタサーフェス, 光導波路, PhC",
        "BPF, ring resonators, metasurfaces, waveguides, PhC");

    I18n::reg("psol_rcwa_full", "Rigorous Coupled-Wave Analysis",
                                "Rigorous Coupled-Wave Analysis");
    I18n::reg("psol_rcwa_best", "周期格子の効率計算・薄膜・回折次数",
        "efficiency of periodic gratings, thin films, diffraction orders");
    I18n::reg("psol_rcwa_weak", "非周期不可・有限サイズ近似",
        "no aperiodic structures, finite size only approximated");
    I18n::reg("psol_rcwa_cost", "低 (層別固有値分解)",
        "low (per-layer eigen decomposition)");
    I18n::reg("psol_rcwa_ex", "DBR, 透過/反射格子, 1D/2D メタサーフェス, 太陽電池薄膜",
        "DBR, transmission/reflection gratings, 1D/2D metasurfaces, PV thin films");

    I18n::reg("psol_bpm_full", "Beam Propagation Method", "Beam Propagation Method");
    I18n::reg("psol_bpm_best", "パラメシ近似で導波路を秒単位で解く",
        "solves waveguides in seconds with the paraxial approximation");
    I18n::reg("psol_bpm_weak", "大角度散乱・後方反射不可",
        "no wide-angle scattering or back reflection");
    I18n::reg("psol_bpm_cost", "非常に低い (1方向マーチ)",
        "very low (single-direction march)");
    I18n::reg("psol_bpm_ex", "スプリッタ, 長距離導波路, MZI 設計, AWG",
        "splitters, long waveguides, MZI design, AWG");

    I18n::reg("psol_fmm_full", "Fourier Modal Method (S-Matrix)",
                               "Fourier Modal Method (S-Matrix)");
    I18n::reg("psol_fmm_best", "層状周期構造、安定なS行列カスケード",
        "layered periodic structures, stable S-matrix cascade");
    I18n::reg("psol_fmm_weak", "非周期/離散的境界に弱い",
        "weak for aperiodic / discrete boundaries");
    I18n::reg("psol_fmm_cost", "低-中 (周波数毎に1回)", "low-medium (once per frequency)");
    I18n::reg("psol_fmm_ex", "多層メタサーフェス, Bragg反射器, グレーティングカプラ",
        "multilayer metasurfaces, Bragg reflectors, grating couplers");

    // ── FDTD 設定 ──
    I18n::reg("psol_fdtd_sec", "FDTD 設定", "FDTD parameters");
    I18n::reg("psol_mesh_acc", "メッシュ精度", "Mesh accuracy");
    I18n::reg("psol_simtime_fs", "シミュレーション時間 (fs)", "Simulation time (fs)");
    I18n::reg("psol_shutoff", "自動シャットオフ", "Auto shutoff");
    I18n::reg("psol_shutoff_lv", "レベル ≤", "level ≤");
    I18n::reg("psol_subpixel", "サブピクセル平均", "Subpixel averaging");
    I18n::reg("psol_conf_mesh", "共形メッシュ", "Conformal mesh");
    I18n::reg("psol_stair", "階段", "Staircase");
    I18n::reg("psol_bc", "境界条件", "Boundary conditions");
    I18n::reg("psol_bc_hint", "「境界面」タブで詳細設定",
                              "detailed settings in the \"Per-face BC\" tab");

    // ── RCWA 設定 ──
    I18n::reg("psol_rcwa_sec", "RCWA 設定", "Rigorous Coupled-Wave");
    I18n::reg("psol_rcwa_hint",
        "周期構造を層毎に Fourier 展開し、各層内で固有値分解 → S行列で連結。\n"
        "形状は周期セル単位で記述。",
        "Fourier-expand the periodic structure layer by layer, eigen-decompose "
        "inside each layer, then cascade the layers with an S-matrix.\n"
        "Geometry is described per period cell.");
    I18n::reg("psol_period", "格子周期 (Λx, Λy)", "Grating period (Λx, Λy)");
    I18n::reg("psol_orders", "Fourier 次数 (Nx, Ny)", "Fourier orders (Nx, Ny)");
    I18n::reg("psol_harm_fmt", "(計 %1×%2 = %3 ハーモニクス)",
                               "(%1×%2 = %3 harmonics total)");
    I18n::reg("psol_trunc", "Truncation 法", "Truncation");
    I18n::reg("psol_trunc_rect", "矩形 (Nx×Ny)", "Rectangular (Nx×Ny)");
    I18n::reg("psol_trunc_circ", "円形 (推奨)", "Circular (recommended)");
    I18n::reg("psol_trunc_para", "放物線", "Parabolic");
    I18n::reg("psol_slices", "層分割", "Layer slicing");
    I18n::reg("psol_slices_hint", "段 (連続変化を階段近似)",
                                  "steps (staircase approximation of a graded profile)");
    I18n::reg("psol_incidence", "入射波 (θ, φ, ψ)", "Incidence (θ, φ, ψ)");
    I18n::reg("psol_incidence_hint", "° (角度, 偏波方位)",
                                     "° (angles, polarisation azimuth)");
    I18n::reg("psol_lambda_range", "波長範囲", "Wavelength range");
    I18n::reg("psol_points", "点", "points");
    I18n::reg("psol_tetm", "TE/TM 分離出力", "Separate TE/TM output");
    I18n::reg("psol_rphase", "複素反射率位相", "Complex reflectance phase");
    I18n::reg("psol_output", "出力", "Output");
    I18n::reg("psol_out_zero", "0次回折効率 (R₀, T₀)",
                               "0th-order diffraction efficiency (R₀, T₀)");
    I18n::reg("psol_out_high", "高次回折 (±1, ±2, …)",
                               "Higher orders (±1, ±2, …)");
    I18n::reg("psol_out_abs", "吸収率 A(λ)", "Absorptance A(λ)");
    I18n::reg("psol_out_field_l", "場分布 E(x,y,z,λ)", "Field distribution E(x,y,z,λ)");
    I18n::reg("psol_eta_rcwa", "推定実行時間: ~3秒/波長 (目安の例)",
                               "Estimated runtime: ~3 s per wavelength "
                               "(illustrative example)");

    // ── BPM 設定 ──
    I18n::reg("psol_bpm_sec", "BPM 設定", "Beam Propagation Method");
    I18n::reg("psol_bpm_hint",
        "パラメシ近似 (∂²/∂z² 無視) で導波路を z 方向に1ステップずつマーチ。\n"
        "広角BPM (Wide-Angle BPM) で大きな曲がりにも対応可能。",
        "March along z step by step with the paraxial approximation "
        "(∂²/∂z² dropped).\n"
        "Wide-Angle BPM handles large bends as well.");
    I18n::reg("psol_algo", "アルゴリズム", "Algorithm");
    I18n::reg("psol_prop_dir", "伝搬方向", "Propagation direction");
    I18n::reg("psol_dz", "ステップ Δz", "Step Δz");
    I18n::reg("psol_dz_hint", "(自動推定: λ/(4n_eff))", "(auto estimate: λ/(4n_eff))");
    I18n::reg("psol_prop_len", "伝搬距離", "Propagation length");
    I18n::reg("psol_nref", "参照屈折率 n_ref", "Reference index n_ref");
    I18n::reg("psol_bpm_tbc", "TBC (Transparent)", "TBC (Transparent)");
    I18n::reg("psol_in_mode", "入射モード", "Input mode");
    I18n::reg("psol_gauss", "ガウシアン", "Gaussian");
    I18n::reg("psol_bidir", "双方向BPM (後方散乱含む)",
                            "Bidirectional BPM (includes back scattering)");
    I18n::reg("psol_vector", "セミベクトル / フルベクトル", "Semi-vector / full-vector");
    I18n::reg("psol_out_field", "場分布 |E(x,y,z)|", "Field distribution |E(x,y,z)|");
    I18n::reg("psol_out_poi", "モード重なり積分 (POI)", "Mode overlap integral (POI)");
    I18n::reg("psol_out_loss", "伝搬損失 [dB/cm]", "Propagation loss [dB/cm]");
    I18n::reg("psol_eta_bpm", "推定実行時間: ~5秒 (FDTDの ~1/100) (目安の例)",
                              "Estimated runtime: ~5 s (~1/100 of FDTD) "
                              "(illustrative example)");

    // ── FMM 設定 ──
    I18n::reg("psol_fmm_sec", "FMM (S-Matrix) 設定", "Fourier Modal Method");
    I18n::reg("psol_fmm_hint",
        "RCWAの数値安定版。S行列カスケードで層数が多くても安定。\n"
        "メタサーフェス・多層DBR・偏光素子に最適。",
        "The numerically stable variant of RCWA: the S-matrix cascade stays "
        "stable for many layers.\n"
        "Ideal for metasurfaces, multilayer DBRs and polarisation elements.");
    I18n::reg("psol_fmm_harm", "Fourier ハーモニクス (M, N)",
                               "Fourier harmonics (M, N)");
    I18n::reg("psol_fmm_total_fmt", "(計 %1)", "(%1 total)");
    I18n::reg("psol_smatrix", "S行列カスケード", "S-matrix cascade");
    I18n::reg("psol_redheffer", "Redheffer 方式", "Redheffer star product");
    I18n::reg("psol_num_stable", "(数値安定)", "(numerically stable)");
    I18n::reg("psol_li", "Li の因数化 (NFFF)", "Li factorisation (NFFF)");
    I18n::reg("psol_li_hint", "金属構造で重要", "important for metallic structures");
    I18n::reg("psol_layers", "層構造", "Layer stack");
    I18n::reg("psol_col_mat", "材質", "Material");
    I18n::reg("psol_col_thick", "厚さ [nm]", "Thickness [nm]");
    I18n::reg("psol_col_pattern", "パターン", "Pattern");
    I18n::reg("psol_air", "空気", "Air");
    I18n::reg("psol_uniform", "一様", "Uniform");
    I18n::reg("psol_pillar", "柱周期 (φ140nm)", "Pillar array (φ140 nm)");
    // 層構造テーブル = 光学タブの RCWA 層スタック (.ofdx) のビュー
    I18n::reg("psol_layers_note",
              "層構造は光学タブ「RCWA 層スタック」の内容です (.ofdx に保存、"
              "RCWA/FMM 実行時に orcwa へ渡されます)。編集は光学タブで行います。",
              "The layer stack mirrors the RCWA layer stack from the Optical tab "
              "(stored in .ofdx and handed to orcwa when RCWA/FMM runs). Edit it "
              "in the Optical tab.");
    I18n::reg("psol_layers_empty",
              "層が未定義です — 光学タブの「RCWA 層スタック」で追加してください",
              "No layers defined — add them in \"RCWA layer stack\" on the "
              "Optical tab");
    I18n::reg("psol_semi_inf", "半無限", "Semi-infinite");
    I18n::reg("psol_grating_fmt", "格子 (fill=%1, εr %2 / %3)",
              "Grating (fill=%1, εr %2 / %3)");
    I18n::reg("psol_uniform_fmt", "一様 (εr %1)", "Uniform (εr %1)");
    I18n::reg("psol_layer_invalid", "不正な層", "Invalid layer");
    // ハイブリッド表は手法の対応関係を示す静的な一覧 (実行連携は未実装)
    I18n::reg("psol_hy_note",
              "この表は「どのスケールをどの手法で解くか」の静的な対応表です。"
              "領域を分割して複数ソルバを連携実行する機能は未実装で、"
              "実行されるのは上で選択した 1 ソルバだけです。",
              "This table is a static map of which method suits which scale. "
              "Splitting a model across several solvers and running them "
              "together is not implemented — only the single solver selected "
              "above is run.");
    I18n::reg("psol_sweep", "掃引", "Sweep");
    I18n::reg("psol_sweep_lam", "波長 λ", "Wavelength λ");
    I18n::reg("psol_sweep_theta", "入射角 θ", "Incidence angle θ");
    I18n::reg("psol_sweep_psi", "偏光角 ψ", "Polarisation angle ψ");
    I18n::reg("psol_out_rt", "複素 R/T (位相付)", "Complex R/T (with phase)");
    I18n::reg("psol_out_stokes", "Stokes パラメータ", "Stokes parameters");
    I18n::reg("psol_out_band", "バンド構造 ω(k)", "Band structure ω(k)");
    I18n::reg("psol_out_cross", "場分布断面", "Field cross-section");

    // ── クロスバリデーション / ハイブリッド ──
    I18n::reg("psol_cross_sec", "クロスバリデーション", "Cross-validation");
    I18n::reg("psol_cross_hint",
        "同じ問題を複数ソルバで解いて結果を相互検証 (Lumerical style)",
        "Solve the same problem with several solvers and cross-check the results "
        "(Lumerical style)");
    I18n::reg("psol_run_fdtd", "FDTD で実行", "Run with FDTD");
    I18n::reg("psol_rerun_rcwa", "RCWA で再実行", "Re-run with RCWA");
    I18n::reg("psol_rerun_bpm", "BPM で再実行", "Re-run with BPM");
    I18n::reg("psol_rerun_fmm", "FMM で再実行", "Re-run with FMM");
    I18n::reg("psol_run_all", "🔍 全ソルバで実行して比較",
                              "🔍 Run all solvers and compare");
    I18n::reg("psol_hybrid_sec", "ハイブリッド解析", "Hybrid analysis");
    I18n::reg("psol_hybrid_hint", "スケール毎に最適ソルバを使い分け",
                                  "Use the best solver for each scale");
    I18n::reg("psol_col_region", "領域", "Region");
    I18n::reg("psol_col_solver", "ソルバ", "Solver");
    I18n::reg("psol_col_role", "役割", "Role");
    I18n::reg("psol_hy_cell", "メタレンズ単位胞 (周期)", "Metalens unit cell (periodic)");
    I18n::reg("psol_hy_cell_role", "位相応答取得", "Extract phase response");
    I18n::reg("psol_hy_lens", "レンズ全体 (~mm)", "Whole lens (~mm)");
    I18n::reg("psol_hy_lens_role", "大スケール伝搬", "Large-scale propagation");
    I18n::reg("psol_hy_wg", "導波路接続部", "Waveguide interconnect");
    I18n::reg("psol_hy_wg_role", "モード進化", "Mode evolution");
    I18n::reg("psol_hy_res", "共振器 / 微細構造", "Resonator / fine structure");
    I18n::reg("psol_hy_res_role", "完全波動解析", "Full-wave analysis");
    I18n::reg("psol_uw_rerun", "再実行のチェック群",
              "the re-run check boxes");
    I18n::reg("psol_uw_fdtd", "FDTD ページの入力 (シミュレーション時間・シャットオフ・サブピクセル・共形メッシュ等)",
              "the FDTD page inputs (simulation time, shutoff, subpixel averaging, conformal mesh, …)");
    I18n::reg("psol_uw_rcwa", "Truncation 法・入射角 (θ, φ, ψ)・TE/TM・出力のチェック",
              "the truncation scheme, the incidence angles (theta, phi, psi), TE/TM and the output check boxes");
    I18n::reg("psol_uw_rcwa_ok", "周期・次数・層分割・波長範囲 (.ofdx へ保存されます)",
              "the period, orders, layer slicing and wavelength range (saved to .ofdx)");
    I18n::reg("psol_uw_bpm", "伝搬方向・伝搬距離・境界条件・双方向 / ベクトル / 出力のチェック",
              "the propagation direction and distance, the boundary condition and the bidirectional / vector / output check boxes");
    I18n::reg("psol_uw_bpm_ok", "アルゴリズム・Δz・n_ref・入射モード",
              "the algorithm, dz, n_ref and the launch mode");
    return true;
}();

const char *kAccOpt = "#B83280";   // --acc-opt (光ドメインのアクセント)

// badge 相当の QLabel (kind: "plain" | "ok" | "acc")
QLabel *makeBadge(const QString &text, const char *kind, QWidget *parent)
{
    auto *b = new QLabel(text, parent);
    QString css = "border-radius:3px; padding:1px 6px; font-size:11px;";
    if (qstrcmp(kind, "ok") == 0)       css += "background:#DFF6DD; color:#0F7B0F;";
    else if (qstrcmp(kind, "acc") == 0) css += "background:#F7E3EF; color:#B83280;";
    else                                css += "background:palette(midlight);";
    b->setStyleSheet(css);
    return b;
}

QLabel *makeMuted(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setStyleSheet("color:gray; font-size:11px;");
    l->setWordWrap(true);
    return l;
}

// <Seg> 相当 (少数選択肢の排他セグメント) — QComboBox で再現
QComboBox *makeSeg(const QStringList &items, int current, QWidget *parent)
{
    auto *c = new QComboBox(parent);
    c->addItems(items);
    c->setCurrentIndex(current);
    return c;
}

QLineEdit *makeNum(const QString &def, int width, QWidget *parent)
{
    auto *e = new QLineEdit(def, parent);
    e->setMaximumWidth(width);
    return e;
}

// mock の <Row> 相当: チェックボックスを横並びに
QHBoxLayout *checkRow(const QStringList &labels, const QVector<bool> &checked,
                      QWidget *parent)
{
    auto *h = new QHBoxLayout();
    h->setSpacing(8);
    for (int i = 0; i < labels.size(); ++i) {
        auto *c = new QCheckBox(labels[i], parent);
        c->setChecked(i < checked.size() && checked[i]);
        h->addWidget(c);
    }
    h->addStretch(1);
    return h;
}

QString harmText(int nx, int ny)
{
    return I18n::tr("psol_harm_fmt").arg(nx).arg(ny).arg(nx * ny);
}

// カード本文 (得意/弱点/計算量/適用例)
QString cardBody(const char *best, const char *weak, const char *cost,
                 const char *ex)
{
    return QStringLiteral("<b>%1:</b> %2<br><b>%3:</b> %4<br>"
                          "<b>%5:</b> %6<br><b>%7:</b> "
                          "<span style=\"color:gray;\">%8</span>")
        .arg(I18n::tr("psol_best"), I18n::tr(best),
             I18n::tr("psol_weak"), I18n::tr(weak),
             I18n::tr("psol_cost"), I18n::tr(cost),
             I18n::tr("psol_examples"), I18n::tr(ex));
}
} // namespace

// ── SolverCard — mock のクリック選択カード ──────────────────────────────────
SolverCard::SolverCard(int id, const QString &name, const QString &full,
                       const QString &bodyHtml, QWidget *parent)
    : QFrame(parent), m_id(id)
{
    setObjectName("psolCard");
    setCursor(Qt::PointingHandCursor);

    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(12, 10, 12, 10);
    v->setSpacing(4);

    auto *head = new QHBoxLayout();
    head->setSpacing(8);
    auto *nameL = new QLabel(name, this);
    nameL->setStyleSheet(QStringLiteral("font-size:14px; font-weight:700; color:%1;")
                             .arg(QString::fromUtf8(kAccOpt)));
    head->addWidget(nameL);
    head->addWidget(makeMuted(full, this));
    head->addStretch(1);
    m_badge = makeBadge(I18n::tr("psol_selected"), "acc", this);
    m_badge->setVisible(false);
    head->addWidget(m_badge);
    v->addLayout(head);

    auto *bodyL = new QLabel(bodyHtml, this);
    bodyL->setTextFormat(Qt::RichText);
    bodyL->setWordWrap(true);
    bodyL->setStyleSheet("font-size:11px;");
    v->addWidget(bodyL);

    setSelected(false);
}

void SolverCard::setSelected(bool on)
{
    m_badge->setVisible(on);
    const QString border = on ? QString::fromUtf8(kAccOpt)
                              : QStringLiteral("palette(mid)");
    const QString bg = on ? QStringLiteral("background:palette(midlight);")
                          : QString();
    setStyleSheet(QStringLiteral("#psolCard { border:2px solid %1;"
                                 " border-radius:4px; %2 }")
                      .arg(border, bg));
}

void SolverCard::mousePressEvent(QMouseEvent *e)
{
    emit picked(m_id);
    QFrame::mousePressEvent(e);
}

// ── PhotonicsSolversTab ─────────────────────────────────────────────────────
PhotonicsSolversTab::PhotonicsSolversTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // ── 光学ソルバ選択 (2列カードグリッド) ─────────────────────────────────
    auto *sSel = new SectionBox(I18n::tr("psol_title"), body);
    auto *hint = new QLabel(I18n::tr("psol_hint"), sSel);
    hint->setWordWrap(true);
    sSel->vbox()->addWidget(hint);

    auto *cardGrid = new QGridLayout();
    cardGrid->setSpacing(8);
    struct MethodDef { const char *name, *full, *best, *weak, *cost, *ex; };
    static const MethodDef kMethods[4] = {
        { "FDTD", "psol_fdtd_full", "psol_fdtd_best", "psol_fdtd_weak",
          "psol_fdtd_cost", "psol_fdtd_ex" },
        { "RCWA", "psol_rcwa_full", "psol_rcwa_best", "psol_rcwa_weak",
          "psol_rcwa_cost", "psol_rcwa_ex" },
        { "BPM",  "psol_bpm_full",  "psol_bpm_best",  "psol_bpm_weak",
          "psol_bpm_cost",  "psol_bpm_ex" },
        { "FMM",  "psol_fmm_full",  "psol_fmm_best",  "psol_fmm_weak",
          "psol_fmm_cost",  "psol_fmm_ex" },
    };
    for (int i = 0; i < 4; ++i) {
        const MethodDef &m = kMethods[i];
        m_cards[i] = new SolverCard(i, QString::fromUtf8(m.name),
                                    I18n::tr(m.full),
                                    cardBody(m.best, m.weak, m.cost, m.ex),
                                    sSel);
        connect(m_cards[i], &SolverCard::picked,
                this, &PhotonicsSolversTab::setMethod);
        cardGrid->addWidget(m_cards[i], i / 2, i % 2);
    }
    sSel->vbox()->addLayout(cardGrid);
    v->addWidget(sSel);

    // ── ソルバ別設定 (mock の条件分岐を QStackedWidget で再現) ─────────────
    m_stack = new QStackedWidget(body);
    m_stack->addWidget(buildFdtdPage());
    m_stack->addWidget(buildRcwaPage());
    m_stack->addWidget(buildBpmPage());
    m_stack->addWidget(buildFmmPage());
    v->addWidget(m_stack);

    // ── クロスバリデーション ───────────────────────────────────────────────
    auto *sCross = new SectionBox(I18n::tr("psol_cross_sec"), body);
    sCross->vbox()->addWidget(makeMuted(I18n::tr("psol_cross_hint"), sCross));
    sCross->vbox()->addLayout(checkRow({ I18n::tr("psol_run_fdtd"),
                                         I18n::tr("psol_rerun_rcwa"),
                                         I18n::tr("psol_rerun_bpm"),
                                         I18n::tr("psol_rerun_fmm") },
                                       { true, false, false, false }, sCross));
    auto *runRow = new QHBoxLayout();
    auto *runAll = new QPushButton(I18n::tr("psol_run_all"), sCross);
    runAll->setProperty("primary", true);
    tabhelp::markNotImplemented(runAll);   // 一括実行は未配線
    runRow->addWidget(runAll);
    runRow->addStretch(1);
    sCross->vbox()->addLayout(runRow);
    // 再実行チェック群はどこにも読まれない (未実装の明示 — 絶対規則 5)
    sCross->vbox()->addWidget(tabhelp::unwiredNote(sCross, I18n::tr("psol_uw_rerun")));
    v->addWidget(sCross);

    // ── ハイブリッド解析 ───────────────────────────────────────────────────
    auto *sHy = new SectionBox(I18n::tr("psol_hybrid_sec"), body);
    sHy->vbox()->addWidget(makeMuted(I18n::tr("psol_hybrid_hint"), sHy));
    auto *hy = new QTableWidget(4, 3, sHy);
    hy->setHorizontalHeaderLabels({ I18n::tr("psol_col_region"),
                                    I18n::tr("psol_col_solver"),
                                    I18n::tr("psol_col_role") });
    hy->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    hy->verticalHeader()->setVisible(false);
    hy->setEditTriggers(QAbstractItemView::NoEditTriggers);
    hy->setMinimumHeight(150);
    const struct { QString region, solver, role; bool acc; } kHybrid[4] = {
        { I18n::tr("psol_hy_cell"), "RCWA/FMM",     I18n::tr("psol_hy_cell_role"), true  },
        { I18n::tr("psol_hy_lens"), "Ray tracing",  I18n::tr("psol_hy_lens_role"), false },
        { I18n::tr("psol_hy_wg"),   "BPM",          I18n::tr("psol_hy_wg_role"),   false },
        { I18n::tr("psol_hy_res"),  "FDTD",         I18n::tr("psol_hy_res_role"),  true  },
    };
    for (int r = 0; r < 4; ++r) {
        hy->setItem(r, 0, new QTableWidgetItem(kHybrid[r].region));
        auto *sv = new QTableWidgetItem(kHybrid[r].solver);
        if (kHybrid[r].acc) {                 // badge acc 相当
            sv->setForeground(QColor(kAccOpt));
            QFont f = sv->font();
            f.setBold(true);
            sv->setFont(f);
        }
        hy->setItem(r, 1, sv);
        hy->setItem(r, 2, new QTableWidgetItem(kHybrid[r].role));
    }
    sHy->vbox()->addWidget(hy);
    // 表は手法の対応関係 (静的な一覧)。連携実行が未実装であることを明示する
    sHy->vbox()->addWidget(makeMuted(I18n::tr("psol_hy_note"), sHy));
    v->addWidget(sHy);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    // ── 配線 ────────────────────────────────────────────────────────────────
    auto applyCb = [this] { apply(); };
    connect(m_meshAcc, &QSlider::valueChanged, this, [this](int val) {
        m_meshAccVal->setText(QString::number(val));
    });
    connect(m_nx, &QSpinBox::valueChanged, this, [this, applyCb] {
        m_harmLabel->setText(harmText(m_nx->value(), m_ny->value()));
        applyCb();
    });
    connect(m_ny, &QSpinBox::valueChanged, this, [this, applyCb] {
        m_harmLabel->setText(harmText(m_nx->value(), m_ny->value()));
        applyCb();
    });
    connect(m_fmmM, &QSpinBox::valueChanged, this, [this, applyCb] {
        m_fmmTotal->setText(I18n::tr("psol_fmm_total_fmt")
                                .arg(m_fmmM->value() * m_fmmN->value()));
        applyCb();
    });
    connect(m_fmmN, &QSpinBox::valueChanged, this, [this, applyCb] {
        m_fmmTotal->setText(I18n::tr("psol_fmm_total_fmt")
                                .arg(m_fmmM->value() * m_fmmN->value()));
        applyCb();
    });
    for (auto *s : { m_slices, m_lamPts })
        connect(s, &QSpinBox::valueChanged, this, applyCb);
    for (auto *e : { m_px, m_py, m_lamMin, m_lamMax,
                     m_bpmDz, m_bpmNref })
        connect(e, &QLineEdit::editingFinished, this, applyCb);
    for (auto *c : { m_bpmAlgo, m_bpmInput })
        connect(c, &QComboBox::currentIndexChanged, this, applyCb);
    connect(m_fmmLi, &QCheckBox::toggled, this, applyCb);

    connect(project, &Project::loaded, this, &PhotonicsSolversTab::refresh);
    // 光学タブで層スタックを編集したら層構造テーブルも追従させる
    connect(project, &Project::changed, this,
            [this] { rebuildLayerTable(); });
    refresh();
}

// ── FDTD 設定ページ ─────────────────────────────────────────────────────────
QWidget *PhotonicsSolversTab::buildFdtdPage()
{
    auto *s = new SectionBox(I18n::tr("psol_fdtd_sec"), m_stack);

    auto *accRow = new QHBoxLayout();
    m_meshAcc = new QSlider(Qt::Horizontal, s);
    m_meshAcc->setRange(1, 8);
    m_meshAcc->setValue(4);                  // mock 既定 4
    m_meshAcc->setTickPosition(QSlider::TicksBelow);
    m_meshAcc->setTickInterval(1);
    accRow->addWidget(m_meshAcc, 1);
    m_meshAccVal = new QLabel("4", s);
    m_meshAccVal->setMinimumWidth(24);
    m_meshAccVal->setAlignment(Qt::AlignCenter);
    m_meshAccVal->setStyleSheet(Theme::monoQss());
    accRow->addWidget(m_meshAccVal);
    s->form()->addRow(I18n::tr("psol_mesh_acc"), accRow);

    m_simTime = makeNum("1000", 100, s);
    s->form()->addRow(I18n::tr("psol_simtime_fs"), m_simTime);

    auto *shutRow = new QHBoxLayout();
    m_shutOn = new QCheckBox("ON", s);
    m_shutOn->setChecked(true);
    m_shutLevel = makeNum("1e-5", 70, s);
    shutRow->addWidget(m_shutOn);
    shutRow->addWidget(makeMuted(I18n::tr("psol_shutoff_lv"), s));
    shutRow->addWidget(m_shutLevel);
    shutRow->addStretch(1);
    s->form()->addRow(I18n::tr("psol_shutoff"), shutRow);

    m_subpixel = new QCheckBox("ON", s);
    m_subpixel->setChecked(true);
    s->form()->addRow(I18n::tr("psol_subpixel"), m_subpixel);

    m_confMesh = makeSeg({ I18n::tr("psol_stair"), "Conformal", "Yu-Mittra" }, 1, s);
    s->form()->addRow(I18n::tr("psol_conf_mesh"), m_confMesh);

    auto *bcRow = new QHBoxLayout();
    bcRow->addWidget(makeBadge("PML", "plain", s));
    bcRow->addWidget(makeMuted(I18n::tr("psol_bc_hint"), s));
    bcRow->addStretch(1);
    s->form()->addRow(I18n::tr("psol_bc"), bcRow);

    // FDTD ページの入力 (シミュ時間/シャットオフ/サブピクセル/共形メッシュ等)
    // は apply() が読まない (未実装の明示 — 絶対規則 5)
    s->vbox()->addWidget(tabhelp::unwiredNote(s, I18n::tr("psol_uw_fdtd")));
    return s;
}

// ── RCWA 設定ページ ─────────────────────────────────────────────────────────
QWidget *PhotonicsSolversTab::buildRcwaPage()
{
    auto *s = new SectionBox(I18n::tr("psol_rcwa_sec"), m_stack);
    auto *hint = new QLabel(I18n::tr("psol_rcwa_hint"), s);
    hint->setWordWrap(true);
    s->form()->addRow(hint);

    auto *perRow = new QHBoxLayout();
    m_px = makeNum("600", 90, s);
    m_py = makeNum("600", 90, s);
    perRow->addWidget(m_px);
    perRow->addWidget(new QLabel("×", s));
    perRow->addWidget(m_py);
    perRow->addWidget(makeMuted("nm", s));
    perRow->addStretch(1);
    s->form()->addRow(I18n::tr("psol_period"), perRow);

    auto *ordRow = new QHBoxLayout();
    m_nx = new QSpinBox(s); m_nx->setRange(1, 101); m_nx->setValue(11);
    m_ny = new QSpinBox(s); m_ny->setRange(1, 101); m_ny->setValue(11);
    m_harmLabel = makeMuted(harmText(11, 11), s);
    ordRow->addWidget(m_nx);
    ordRow->addWidget(new QLabel("×", s));
    ordRow->addWidget(m_ny);
    ordRow->addWidget(m_harmLabel);
    ordRow->addStretch(1);
    s->form()->addRow(I18n::tr("psol_orders"), ordRow);

    m_trunc = makeSeg({ I18n::tr("psol_trunc_rect"), I18n::tr("psol_trunc_circ"),
                        I18n::tr("psol_trunc_para") }, 1, s);
    s->form()->addRow(I18n::tr("psol_trunc"), m_trunc);

    auto *slRow = new QHBoxLayout();
    m_slices = new QSpinBox(s);
    m_slices->setRange(1, 1000);
    m_slices->setValue(20);
    slRow->addWidget(m_slices);
    slRow->addWidget(makeMuted(I18n::tr("psol_slices_hint"), s));
    slRow->addStretch(1);
    s->form()->addRow(I18n::tr("psol_slices"), slRow);

    auto *incRow = new QHBoxLayout();
    m_incTheta = makeNum("0", 60, s);
    m_incPhi   = makeNum("0", 60, s);
    m_incPsi   = makeNum("0", 60, s);
    incRow->addWidget(m_incTheta);
    incRow->addWidget(m_incPhi);
    incRow->addWidget(m_incPsi);
    incRow->addWidget(makeMuted(I18n::tr("psol_incidence_hint"), s));
    incRow->addStretch(1);
    s->form()->addRow(I18n::tr("psol_incidence"), incRow);

    auto *lamRow = new QHBoxLayout();
    m_lamMin = makeNum("400", 90, s);
    m_lamMax = makeNum("800", 90, s);
    m_lamPts = new QSpinBox(s);
    m_lamPts->setRange(2, 100000);
    m_lamPts->setValue(201);
    lamRow->addWidget(m_lamMin);
    lamRow->addWidget(new QLabel(QString::fromUtf8("〜"), s));
    lamRow->addWidget(m_lamMax);
    lamRow->addWidget(new QLabel("nm", s));
    lamRow->addWidget(new QLabel("(", s));
    lamRow->addWidget(m_lamPts);
    lamRow->addWidget(new QLabel(I18n::tr("psol_points") + ")", s));
    lamRow->addStretch(1);
    s->form()->addRow(I18n::tr("psol_lambda_range"), lamRow);

    s->form()->addRow(checkRow({ I18n::tr("psol_tetm"), I18n::tr("psol_rphase") },
                               { true, true }, s));
    s->form()->addRow(I18n::tr("psol_output"),
                      checkRow({ I18n::tr("psol_out_zero"), I18n::tr("psol_out_high"),
                                 I18n::tr("psol_out_abs"), I18n::tr("psol_out_field_l") },
                               { true, true, false, false }, s));

    auto *etaRow = new QHBoxLayout();
    etaRow->addWidget(makeBadge(I18n::tr("psol_eta_rcwa"), "ok", s));
    etaRow->addStretch(1);
    s->form()->addRow(etaRow);
    // Truncation 法・入射角 (θ, φ, ψ)・TE/TM/出力チェックは apply() が読まない
    // (周期/次数/層分割/波長範囲のみ .ofdx へ反映 — 絶対規則 5)
    s->vbox()->addWidget(tabhelp::unwiredNote(s, I18n::tr("psol_uw_rcwa"), I18n::tr("psol_uw_rcwa_ok")));
    return s;
}

// ── BPM 設定ページ ──────────────────────────────────────────────────────────
QWidget *PhotonicsSolversTab::buildBpmPage()
{
    auto *s = new SectionBox(I18n::tr("psol_bpm_sec"), m_stack);
    auto *hint = new QLabel(I18n::tr("psol_bpm_hint"), s);
    hint->setWordWrap(true);
    s->form()->addRow(hint);

    m_bpmAlgo = makeSeg({ "FFT-BPM", "FDM-BPM (Crank-Nicolson)",
                          QString::fromUtf8("Wide-Angle BPM (Padé)") }, 0, s);
    s->form()->addRow(I18n::tr("psol_algo"), m_bpmAlgo);

    m_bpmDir = makeSeg({ "+X", "+Y", "+Z" }, 2, s);
    s->form()->addRow(I18n::tr("psol_prop_dir"), m_bpmDir);

    auto *dzRow = new QHBoxLayout();
    m_bpmDz = makeNum("0.5", 90, s);
    dzRow->addWidget(m_bpmDz);
    dzRow->addWidget(makeMuted(QString::fromUtf8("μm"), s));
    dzRow->addWidget(makeMuted(I18n::tr("psol_dz_hint"), s));
    dzRow->addStretch(1);
    s->form()->addRow(I18n::tr("psol_dz"), dzRow);

    auto *lenRow = new QHBoxLayout();
    m_bpmLen = makeNum("1000", 90, s);
    lenRow->addWidget(m_bpmLen);
    lenRow->addWidget(makeMuted(QString::fromUtf8("μm"), s));
    lenRow->addStretch(1);
    s->form()->addRow(I18n::tr("psol_prop_len"), lenRow);

    m_bpmNref = makeNum("3.45", 60, s);
    s->form()->addRow(I18n::tr("psol_nref"), m_bpmNref);

    m_bpmBc = makeSeg({ "ABC", I18n::tr("psol_bpm_tbc"), "PML" }, 1, s);
    s->form()->addRow(I18n::tr("psol_bc"), m_bpmBc);

    m_bpmInput = makeSeg({ QString::fromUtf8("TE₀"), QString::fromUtf8("TE₁"),
                           QString::fromUtf8("TM₀"), I18n::tr("psol_gauss") }, 0, s);
    s->form()->addRow(I18n::tr("psol_in_mode"), m_bpmInput);

    s->form()->addRow(checkRow({ I18n::tr("psol_bidir") }, { false }, s));
    s->form()->addRow(checkRow({ I18n::tr("psol_vector") }, { true }, s));
    s->form()->addRow(I18n::tr("psol_output"),
                      checkRow({ I18n::tr("psol_out_field"), I18n::tr("psol_out_poi"),
                                 I18n::tr("psol_out_loss") },
                               { true, true, true }, s));

    auto *etaRow = new QHBoxLayout();
    etaRow->addWidget(makeBadge(I18n::tr("psol_eta_bpm"), "ok", s));
    etaRow->addStretch(1);
    s->form()->addRow(etaRow);
    // 伝搬方向・伝搬距離・境界条件・双方向/ベクトル/出力チェックは
    // apply() が読まない (アルゴリズム/Δz/n_ref/入射モードのみ反映 — 絶対規則 5)
    s->vbox()->addWidget(tabhelp::unwiredNote(s, I18n::tr("psol_uw_bpm"), I18n::tr("psol_uw_bpm_ok")));
    return s;
}

// ── FMM (S-Matrix) 設定ページ ───────────────────────────────────────────────
QWidget *PhotonicsSolversTab::buildFmmPage()
{
    auto *s = new SectionBox(I18n::tr("psol_fmm_sec"), m_stack);
    auto *hint = new QLabel(I18n::tr("psol_fmm_hint"), s);
    hint->setWordWrap(true);
    s->form()->addRow(hint);

    auto *harmRow = new QHBoxLayout();
    m_fmmM = new QSpinBox(s); m_fmmM->setRange(1, 201); m_fmmM->setValue(13);
    m_fmmN = new QSpinBox(s); m_fmmN->setRange(1, 201); m_fmmN->setValue(13);
    m_fmmTotal = makeMuted(I18n::tr("psol_fmm_total_fmt").arg(13 * 13), s);
    harmRow->addWidget(m_fmmM);
    harmRow->addWidget(new QLabel("×", s));
    harmRow->addWidget(m_fmmN);
    harmRow->addWidget(m_fmmTotal);
    harmRow->addStretch(1);
    s->form()->addRow(I18n::tr("psol_fmm_harm"), harmRow);

    auto *casRow = new QHBoxLayout();
    auto *redheffer = new QCheckBox(I18n::tr("psol_redheffer"), s);
    redheffer->setChecked(true);
    casRow->addWidget(redheffer);
    casRow->addWidget(makeMuted(I18n::tr("psol_num_stable"), s));
    casRow->addStretch(1);
    s->form()->addRow(I18n::tr("psol_smatrix"), casRow);

    auto *liRow = new QHBoxLayout();
    m_fmmLi = new QCheckBox("ON", s);
    m_fmmLi->setChecked(true);
    liRow->addWidget(m_fmmLi);
    liRow->addWidget(makeMuted(I18n::tr("psol_li_hint"), s));
    liRow->addStretch(1);
    s->form()->addRow(I18n::tr("psol_li"), liRow);

    // 層構造 = 光学タブで編集する RCWA 層スタック (.ofdx) のビュー
    m_layerTable = new QTableWidget(0, 4, s);
    m_layerTable->setHorizontalHeaderLabels({ "#", I18n::tr("psol_col_mat"),
                                              I18n::tr("psol_col_thick"),
                                              I18n::tr("psol_col_pattern") });
    m_layerTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_layerTable->verticalHeader()->setVisible(false);
    m_layerTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_layerTable->setMinimumHeight(175);
    s->form()->addRow(I18n::tr("psol_layers"), m_layerTable);
    s->form()->addRow(makeMuted(I18n::tr("psol_layers_note"), s));
    rebuildLayerTable();

    s->form()->addRow(I18n::tr("psol_sweep"),
                      checkRow({ I18n::tr("psol_sweep_lam"),
                                 I18n::tr("psol_sweep_theta"),
                                 I18n::tr("psol_sweep_psi") },
                               { true, false, false }, s));
    s->form()->addRow(I18n::tr("psol_output"),
                      checkRow({ I18n::tr("psol_out_rt"), I18n::tr("psol_out_stokes"),
                                 I18n::tr("psol_out_band"), I18n::tr("psol_out_cross") },
                               { true, false, false, true }, s));
    return s;
}

// ── 層構造テーブル (OpticalOpts::rcwaLayerList のビュー) ────────────────────
// 材質は誘電率から屈折率 n = √εr を出して示す (RCWA 層は材質名を持たない)。
// 先頭・末尾の層は半無限層として厚みがカーネルで無視される (Project.h)。
void PhotonicsSolversTab::rebuildLayerTable()
{
    if (!m_layerTable) return;
    const QVector<RcwaLayer> &ls = m_p->optical().rcwaLayerList;
    m_layerTable->clearContents();
    m_layerTable->clearSpans();   // 前回の結合セルを解除
    if (ls.isEmpty()) {
        m_layerTable->setRowCount(1);
        m_layerTable->setItem(0, 0,
            new QTableWidgetItem(I18n::tr("psol_layers_empty")));
        m_layerTable->setSpan(0, 0, 1, 4);
        return;
    }
    m_layerTable->setRowCount(ls.size());
    for (int r = 0; r < ls.size(); ++r) {
        const RcwaLayer &l = ls[r];
        auto *num = new QTableWidgetItem(QString::number(r + 1));
        num->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_layerTable->setItem(r, 0, num);

        const bool valid = isValidRcwaLayer(l);
        const bool grating = (l.eps1 != l.eps2 && l.fill > 0.0 && l.fill < 1.0);
        // 材質: 一様なら n、格子なら 2 材質の n を併記
        auto nOf = [](double eps) {
            return QString::number(eps > 0 ? std::sqrt(eps) : 0.0, 'f', 3);
        };
        const QString mat = grating
            ? QStringLiteral("n = %1 / %2").arg(nOf(l.eps1), nOf(l.eps2))
            : QStringLiteral("n = %1").arg(nOf(l.eps1));
        m_layerTable->setItem(r, 1, new QTableWidgetItem(
            valid ? mat : I18n::tr("psol_layer_invalid")));

        // 厚み: 半無限層 (先頭/末尾) はカーネルが厚みを無視する
        const bool semiInf = (r == 0 || r == ls.size() - 1);
        auto *th = new QTableWidgetItem(
            semiInf ? I18n::tr("psol_semi_inf")
                    : QString::number(l.thickness_nm, 'g', 6));
        th->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_layerTable->setItem(r, 2, th);

        m_layerTable->setItem(r, 3, new QTableWidgetItem(
            grating ? I18n::tr("psol_grating_fmt")
                          .arg(QString::number(l.fill, 'g', 3),
                               QString::number(l.eps1, 'g', 4),
                               QString::number(l.eps2, 'g', 4))
                    : I18n::tr("psol_uniform_fmt")
                          .arg(QString::number(l.eps1, 'g', 4))));
    }
}

// 選択ソルバ切替 (カードクリック / refresh から)
void PhotonicsSolversTab::setMethod(int id)
{
    if (id < 0 || id > 3) return;
    m_method = id;
    for (int i = 0; i < 4; ++i)
        m_cards[i]->setSelected(i == id);
    m_stack->setCurrentIndex(id);
    apply();
}

// OpticalOpts と共有するパラメータのみ永続化 (それ以外はローカル状態)
void PhotonicsSolversTab::apply()
{
    if (m_updating) return;
    OpticalOpts &o = m_p->optical();
    o.solver = OpticalSolver(m_method);
    // RCWA
    o.rcwaNx = m_nx->value();
    o.rcwaNy = m_ny->value();
    o.rcwaPeriodX = m_px->text().toDouble();
    o.rcwaPeriodY = m_py->text().toDouble();
    o.rcwaLayers  = m_slices->value();
    o.lambdaMin = m_lamMin->text().toDouble();
    o.lambdaMax = m_lamMax->text().toDouble();
    o.lambdaDiv = m_lamPts->value();
    // BPM (Δz は μm 入力 → .ofdx は nm)
    o.bpmAlgorithm = m_bpmAlgo->currentIndex();
    o.bpmInputMode = m_bpmInput->currentIndex();
    o.bpmDz        = m_bpmDz->text().toDouble() * 1000.0;
    o.bpmRefIndex  = m_bpmNref->text().toDouble();
    // FMM (M のみ .ofdx の fmmHarmonics に対応、N はローカル)
    o.fmmHarmonics = m_fmmM->value();
    o.fmmLiRules   = m_fmmLi->isChecked();
    m_p->touch();
}

void PhotonicsSolversTab::refresh()
{
    m_updating = true;
    const OpticalOpts &o = m_p->optical();
    m_nx->setValue(o.rcwaNx);
    m_ny->setValue(o.rcwaNy);
    m_px->setText(QString::number(o.rcwaPeriodX, 'g', 8));
    m_py->setText(QString::number(o.rcwaPeriodY, 'g', 8));
    m_slices->setValue(o.rcwaLayers);
    m_lamMin->setText(QString::number(o.lambdaMin, 'g', 8));
    m_lamMax->setText(QString::number(o.lambdaMax, 'g', 8));
    m_lamPts->setValue(o.lambdaDiv);
    m_bpmAlgo->setCurrentIndex(o.bpmAlgorithm);
    m_bpmInput->setCurrentIndex(o.bpmInputMode);
    m_bpmDz->setText(QString::number(o.bpmDz / 1000.0, 'g', 8));
    m_bpmNref->setText(QString::number(o.bpmRefIndex, 'g', 8));
    m_fmmM->setValue(o.fmmHarmonics);
    m_fmmLi->setChecked(o.fmmLiRules);
    m_harmLabel->setText(harmText(m_nx->value(), m_ny->value()));
    m_fmmTotal->setText(I18n::tr("psol_fmm_total_fmt")
                            .arg(m_fmmM->value() * m_fmmN->value()));
    rebuildLayerTable();
    setMethod(int(o.solver));      // apply() は m_updating で抑止される
    m_updating = false;
}
