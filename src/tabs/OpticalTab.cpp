// OpticalTab.cpp
#include "OpticalTab.h"
#include "../core/Project.h"
#include "../io/ActivationCurve.h"
#include "../io/Touchstone.h"
#include "../kernel/Runner.h"
#include "../widgets/MiniPlot.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"
#include "../Theme.h"
#include "TabHelpers.h"

#include <QBrush>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTextStream>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <complex>
#include <initializer_list>
#include <utility>

using namespace ofd;

// ── Raycast / 光学系 / ハイブリッド セクション専用キー (optray_) ─────────────
// 既存の光タブ語彙 (opt_*) は I18n.cpp にあるので、モック追加分だけ file-local
// に登録する。reg は既存キー優先なので衝突しても安全。
namespace {
const bool s_i18nOptRay = [] {
    using ofd::I18n;
    // Raycast 設定 / Geometric Optics
    I18n::reg("optray_section", "Raycast 設定 / Geometric Optics",
              "Raycast settings / Geometric optics");
    I18n::reg("optray_num_rays", "レイ数", "# rays");
    I18n::reg("optray_rays_unit", "本", "rays");
    I18n::reg("optray_max_bounces", "最大反射回数", "Max bounces");
    I18n::reg("optray_min_energy", "最小エネルギー [dB]", "Min energy [dB]");
    I18n::reg("optray_sampling", "サンプリング", "Sampling");
    I18n::reg("optray_uniform", "一様", "Uniform");
    I18n::reg("optray_jittered", "ジッタ", "Jittered");
    I18n::reg("optray_qmc", "準モンテカルロ (QMC)", "Quasi-Monte Carlo");
    // mock i18n の ray_importance (サンプリング方式の4番目)
    I18n::reg("optray_importance", "重要度サンプリング", "Importance sampling");
    I18n::reg("optray_refl_model", "反射モデル", "Reflection model");
    I18n::reg("optray_specular", "鏡面反射", "Specular");
    I18n::reg("optray_diffuse", "拡散反射 (Lambert)", "Diffuse (Lambert)");
    // mock i18n の ray_diff_ord (拡散反射を何段まで追跡するか)
    I18n::reg("optray_diff_ord", "拡散次数", "Diffuse order");
    I18n::reg("optray_physics", "物理追跡", "Physical tracking");
    I18n::reg("optray_polarized", "偏波追跡", "Polarization tracking");
    I18n::reg("optray_dispersion", "分散追跡 (Sellmeier)",
              "Dispersion tracking");
    I18n::reg("optray_fresnel", "フレネル係数", "Fresnel coefficients");
    I18n::reg("optray_raydiag", "光線図出力", "Ray diagram output");
    I18n::reg("optray_viz_rays", "可視化レイ数", "Visualized rays");
    I18n::reg("optray_gpu", "GPU加速 (未実装)", "GPU accelerated (not implemented)");
    I18n::reg("optray_gpu_hint", "OptiX / Embree 経由 (レイトレーサ未実装)",
              "via OptiX / Embree (ray tracer not implemented)");
    // 光学系定義 / Optical system
    I18n::reg("optray_sys_section", "光学系定義 / Optical system",
              "Optical system definition");
    I18n::reg("optray_col_type", "面タイプ", "Surface type");
    I18n::reg("optray_col_thick", "厚さ", "Thickness");
    I18n::reg("optray_col_mat", "材質", "Material");
    I18n::reg("optray_col_stop", "絞り", "Stop");
    I18n::reg("optray_sph", "球面", "Sphere");
    I18n::reg("optray_stop", "絞り", "Stop");
    I18n::reg("optray_asph", "非球面", "Asphere");
    I18n::reg("optray_seidel", "収差解析 (Seidel)",
              "Aberration analysis (Seidel)");
    I18n::reg("optray_spot", "スポットダイアグラム", "Spot diagram");
    I18n::reg("optray_mtf", "MTF", "MTF");
    I18n::reg("optray_ray_aberr", "光線収差図", "Ray aberration plot");
    // ハイブリッド連携 / FDTD↔Ray bridge
    I18n::reg("optray_hyb_section", "ハイブリッド連携 / FDTD↔Ray bridge",
              "Hybrid coupling / FDTD↔Ray bridge");
    I18n::reg("optray_hyb_region", "FDTD領域", "FDTD region");
    I18n::reg("optray_hyb_region_badge",
              "[-2,2] μm × [-2,2] μm × [-2,2] μm",
              "[-2,2] μm × [-2,2] μm × [-2,2] μm");
    I18n::reg("optray_hyb_bconv", "境界モード変換", "Boundary mode conversion");
    I18n::reg("optray_hyb_modedec", "モード分解", "Mode decomposition");
    I18n::reg("optray_hyb_gauss", "ガウシアンビーム", "Gaussian beam");
    I18n::reg("optray_hyb_prop", "伝搬モデル", "Propagation model");
    I18n::reg("optray_hyb_hint",
              "微細構造はFDTDで精密解析、遠方伝搬はRayで高速化",
              "Fine structures are solved by FDTD; far-field propagation is "
              "accelerated by ray tracing");
    I18n::reg("optm_uw_method",
              "ハイブリッド (波動 + 幾何) の連携そのもの",
              "the wave + geometrical hybrid coupling itself");
    I18n::reg("optm_uw_method_ok",
              "解法の選択 — 幾何光学を選ぶと下の波動ソルバーの設定を無効化し、"
              "順次光線追跡 (レンズエディタタブ) へ誘導します",
              "the choice of method — picking geometrical optics disables the "
              "wave-solver settings below and points at the sequential ray "
              "trace in the lens editor tab");
    I18n::reg("optm_geo_wave_off",
              "幾何光学ではこの下の波動ソルバー (FDTD / RCWA / BPM / FMM) は"
              "使いません。順次光線追跡はレンズエディタタブの「解析プロット」"
              "で行います (外部カーネルは起動しません)。",
              "Geometrical optics does not use the wave solvers below (FDTD / "
              "RCWA / BPM / FMM). Sequential ray tracing is done under "
              "\"Analyses\" in the lens editor tab; no external kernel is "
              "launched.");
    I18n::reg("optm_surf_shared",
              "面データはレンズエディタタブの面テーブルそのものです "
              "(ここは表示のみ。編集するとプロジェクトに保存されます)。",
              "The surface data is the lens editor tab's own table (shown "
              "read-only here; edits there are saved with the project).");
    I18n::reg("optm_uw_ray",
              "この節のレイトレース設定 (拡散反射・重要度サンプリング・偏光を"
              "扱う非順次レイトレーサが未実装のため)。順次光線追跡 "
              "(スポットダイアグラム・光線収差図) はレンズエディタタブに"
              "あります",
              "the ray-trace settings in this section (the non-sequential ray "
              "tracer that handles diffuse reflection, importance sampling and "
              "polarisation is not implemented). Sequential ray tracing — spot "
              "diagrams and ray-aberration curves — is in the lens editor tab");
    I18n::reg("optm_uw_surf",
              "この節の面データ (固定の設計例) と MTF のチェック "
              "(MTF は回折を含む計算が要ります)",
              "the surface data in this section (a fixed worked example) and the "
              "MTF check box (MTF needs a diffraction calculation)");
    I18n::reg("optm_seidel_where",
              "収差解析 (Seidel) は 3 次収差の実計算がレンズエディタタブに"
              "あります — 面テーブル・入射瞳径・視野をそこで編集すると、"
              "球面収差・コマ・非点・像面湾曲・歪曲が面ごとに出ます。"
              "スポット径と光線収差図も同じタブの「解析プロット」から"
              "実光線追跡で計算します。",
              "The Seidel (third-order) aberrations are computed for real in the "
              "lens editor tab — edit the surface table, the entrance pupil and "
              "the field there and you get spherical, coma, astigmatism, field "
              "curvature and distortion per surface. The spot size and the ray "
              "aberration curves come from a real ray trace in the same tab, "
              "under \"Analyses\".");
    I18n::reg("optm_uw_hybrid", "波動 / 幾何のハイブリッド連携の設定",
              "the wave / geometrical hybrid settings");
    I18n::reg("optm_uw_out", "この節の設定",
              "the settings in this section");
    return true;
}();

// ── 光解析モード別セクション / 分散モデル 専用キー (optm_) ──────────────────
// mock の各モードセクション (BPF 追加行 / Ring 追加行 / WG / MZI / メタサーフェス
// / フォトニック結晶 / NF→FF / S パラメータ / 分散モデル) の語彙。
// セクション見出しは既存の opt_* (モード名) をそのまま使う。
const bool s_i18nOptModes = [] {
    using ofd::I18n;
    // ── mock i18n テーブルにあって C++ 側に無かった語彙 (キー名もモックのまま) ──
    // opt_nf_ff は I18n.cpp の opt_nf2ff (近傍界→遠方界変換) と同義。モックの
    // 表記に合わせるため、こちらを見出し・モード名に使う。
    I18n::reg("opt_nf_ff", "近接場/遠方場変換", "Near-to-far field");
    I18n::reg("opt_raycast", "Raycast (幾何光学)", "Raycast (geometric)");
    I18n::reg("opt_hybrid", "ハイブリッド FDTD+Ray", "Hybrid FDTD+Ray");
    I18n::reg("opt_target_band", "目標通過帯域", "Target passband");
    I18n::reg("opt_port_a", "入力ポート", "Input port");
    I18n::reg("opt_port_b", "出力ポート", "Output port");
    // 解法 (波動 / 幾何光学 / ハイブリッド) の行 — mock の Seg + ヒント文
    I18n::reg("optm_geo_method", "解法 (波動/幾何)", "Method (wave/geometric)");
    I18n::reg("optm_geo_fdtd", "FDTD (波動)", "FDTD (wave)");
    I18n::reg("optm_geo_hint_fdtd",
              "全波動FDTD — 小スケール構造、共振・回折・分散すべて正確",
              "Full-wave FDTD — small-scale structures; resonance, diffraction "
              "and dispersion are all accurate");
    I18n::reg("optm_geo_hint_ray",
              "幾何光学レイトレース — 大規模光学系・遠方・カメラに最適",
              "Geometric ray tracing — best for large optical systems, far "
              "field and cameras");
    I18n::reg("optm_geo_hint_hybrid",
              "ローカル領域はFDTD、伝搬域はRay — マルチスケール解析",
              "FDTD in the local region, rays in the propagation region — "
              "multi-scale analysis");
    // BPF 追加行
    I18n::reg("optm_bpf_il", "挿入損失 / IL", "Insertion loss / IL");
    I18n::reg("optm_bpf_stop", "阻止域減衰", "Stopband rejection");
    I18n::reg("optm_bpf_spectrum", "透過スペクトル (設計目標)",
              "Transmission spectrum (design target)");
    I18n::reg("optm_bpf_target_note",
              "目標帯域・Q・挿入損失・阻止域減衰から計算した設計目標カーブ"
              "です (ソルバーの計算結果ではありません)",
              "Design-target curve computed from the target band, Q, insertion "
              "loss and stopband rejection — not a solver result");
    // Ring 追加行
    I18n::reg("optm_fsr", "FSR (Free Spectral Range)", "FSR");
    I18n::reg("optm_finesse", "フィネス", "Finesse");
    I18n::reg("optm_q_factor", "Q値", "Q-factor");
    I18n::reg("optm_thru_port", "スルーポート出力", "Thru port output");
    I18n::reg("optm_drop_port", "ドロップポート出力", "Drop port output");
    // 導波路モード解析 (opt_wg)
    I18n::reg("optm_mode_te", "TEモード", "TE mode");
    I18n::reg("optm_mode_tm", "TMモード", "TM mode");
    I18n::reg("optm_neff", "実効屈折率 neff", "Effective index neff");
    I18n::reg("optm_loss", "損失 [dB/cm]", "Loss [dB/cm]");
    // MZI
    I18n::reg("optm_mzi_dl", "アーム長差 ΔL", "Arm length difference ΔL");
    I18n::reg("optm_mzi_shifter", "位相シフタ", "Phase shifter");
    I18n::reg("optm_mzi_thermo", "熱光学", "Thermo-optic");
    I18n::reg("optm_mzi_eo", "電気光学", "Electro-optic");
    // メタサーフェス
    I18n::reg("optm_meta_period", "格子周期", "Lattice period");
    I18n::reg("optm_meta_nano", "ナノ構造", "Nanostructure");
    I18n::reg("optm_meta_pillar", "柱 / Pillar", "Pillar");
    I18n::reg("optm_meta_hole", "穴 / Hole", "Hole");
    I18n::reg("optm_meta_cross", "クロス / Cross", "Cross");
    I18n::reg("optm_meta_phase", "位相設計", "Phase design");
    I18n::reg("optm_meta_flat", "等位相波面 (flat lens)",
              "Flat phase front (flat lens)");
    I18n::reg("optm_meta_steer", "偏向 / Beam steering", "Beam steering");
    I18n::reg("optm_meta_oam", "渦光 / OAM", "Vortex beam / OAM");
    // フォトニック結晶
    I18n::reg("optm_phc_lattice", "格子タイプ", "Lattice type");
    I18n::reg("optm_phc_tri", "三角格子", "Triangular");
    I18n::reg("optm_phc_sq", "正方格子", "Square");
    I18n::reg("optm_phc_honey", "ハニカム", "Honeycomb");
    I18n::reg("optm_phc_a", "格子定数 a", "Lattice constant a");
    I18n::reg("optm_phc_ra", "穴半径 r/a", "Hole radius r/a");
    I18n::reg("optm_phc_band", "バンド構造解析", "Band structure");
    I18n::reg("optm_phc_defect", "欠陥モード", "Defect mode");
    // 近傍界→遠方界変換
    I18n::reg("optm_nf_surface", "変換面", "Transform surface");
    I18n::reg("optm_nf_box", "直方体", "Box");
    I18n::reg("optm_nf_sphere", "球面", "Sphere");
    I18n::reg("optm_nf_dist", "観測距離", "Observation distance");
    // S パラメータ
    I18n::reg("optm_sp_ports", "ポート数", "# ports");
    I18n::reg("optm_sp_s11", "S11 (反射)", "S11 (reflection)");
    I18n::reg("optm_sp_s21", "S21 (透過)", "S21 (transmission)");
    I18n::reg("optm_sp_phase", "位相情報を含む", "Include phase");
    I18n::reg("optm_sp_gd", "群遅延", "Group delay");
    I18n::reg("optm_sp_export", "Touchstone (.s2p) 出力",
              "Export Touchstone (.s2p)");
    // S パラメータ出力 (カーネルの test.snp → Touchstone / CSV)
    I18n::reg("optsp_title", "S パラメータ出力", "Export S-parameters");
    I18n::reg("optsp_none",
              "S パラメータの出力が作業ディレクトリに見つかりません。\n"
              "観測点 (point) と plotspara を有効にしてソルバーを実行してください。\n"
              "作業ディレクトリ: %1",
              "No S-parameter output found in the working directory.\n"
              "Enable observation points (point) and plotspara, then run the "
              "solver.\nWorking directory: %1");
    I18n::reg("optsp_read_fail", "%1 を読めませんでした: %2",
              "Could not read %1: %2");
    I18n::reg("optsp_port_range",
              "ポート番号 (入力 %1 / 出力 %2) がファイルのポート数 %3 を超えています。",
              "Port numbers (in %1 / out %2) exceed the %3 ports in the file.");
    I18n::reg("optsp_col1_only",
              "カーネル出力 (%1) はポート 1 を励振した第 1 列 (S_n1) だけです。"
              "入力ポートに 1 以外を指定した結果は計算されていません。",
              "The kernel output (%1) contains only the first column (S_n1) "
              "from exciting port 1. Nothing is available for an input port "
              "other than 1.");
    I18n::reg("optsp_nosel", "S11 / S21 のどちらも選択されていません。",
              "Neither S11 nor S21 is selected.");
    I18n::reg("optsp_write_fail", "書き出しに失敗しました: %1",
              "Write failed: %1");
    I18n::reg("optsp_skip_2port",
              "2 ポート Touchstone (.s2p) は書けません — S12 / S22 が"
              "計算されていないためです (カーネルはポート 1 のみ励振)。"
              "欠けた要素を仮定で埋めることはしません。",
              "A 2-port Touchstone (.s2p) cannot be written: S12 / S22 were "
              "never computed (the kernel excites port 1 only). Missing "
              "entries are not filled in by assumption.");
    I18n::reg("optsp_done", "出力しました。\n\n入力: %1\n%2周波数: %3 点 (%4 〜 %5 Hz)",
              "Exported.\n\nSource: %1\n%2Frequencies: %3 points (%4 – %5 Hz)");
    I18n::reg("optsp_out_ts", "Touchstone: %1\n", "Touchstone: %1\n");
    I18n::reg("optsp_out_csv", "CSV: %1\n", "CSV: %1\n");
    I18n::reg("optsp_filter_ts", "Touchstone (*.s1p *.s2p *.snp)",
              "Touchstone (*.s1p *.s2p *.snp)");
    I18n::reg("optsp_filter_csv", "CSV (*.csv)", "CSV (*.csv)");
    // 分散モデル
    I18n::reg("optm_disp_section", "分散モデル", "Dispersion model");
    // 固定表示の算出値が「設計例」であることの明記 (絶対規則 5)
    I18n::reg("optm_design_example", "(設計例)", "(design example)");
    return true;
}();

// mock の <span className="badge"> 相当 (色は最小限)
QLabel *makeBadge(const QString &text, bool accent, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    QString ss = "border-radius:8px; padding:1px 7px; font-size:11px;";
    if (accent)
        ss += "background:#B83280; color:white; border:1px solid transparent;";
    else
        ss += "border:1px solid palette(mid);";
    l->setStyleSheet(ss);
    return l;
}

// mock の <span className="muted text-sm"> 相当
QLabel *mutedLabel(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setWordWrap(true);
    l->setStyleSheet("color:#7A7A7A; font-size:11px;");
    return l;
}

// mock の <span className="mono"> 相当 (算出値の読取専用表示)
QLabel *monoLabel(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    QFont f = l->font();
    f.setStyleHint(QFont::Monospace);
    // 実在するファミリのみ指定 (Theme が環境ごとに解決済み)
    if (const QString mf = Theme::monoFontFamily(); !mf.isEmpty())
        f.setFamily(mf);
    l->setFont(f);
    l->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return l;
}

QCheckBox *makeCheck(const QString &text, bool on, QWidget *parent)
{
    auto *c = new QCheckBox(text, parent);
    c->setChecked(on);
    return c;
}

// mock の <input className="q-num w-sm|w-md"> 相当 (ローカル state のみの数値入力)
QLineEdit *numEdit(const QString &value, QWidget *parent, int width = 100)
{
    auto *e = new QLineEdit(value, parent);
    e->setMaximumWidth(width);
    return e;
}

// mock の <Row> 内に複数要素が並ぶケース (左寄せ + 余白)
QHBoxLayout *hrow(std::initializer_list<QWidget *> ws)
{
    auto *h = new QHBoxLayout();
    h->setContentsMargins(0, 0, 0, 0);
    for (QWidget *w : ws)
        h->addWidget(w);
    h->addStretch(1);
    return h;
}

// 光学系定義テーブルの行データ (mock の <tbody> そのまま)
QTableWidgetItem *alignedItem(const QString &text, Qt::Alignment a)
{
    auto *it = new QTableWidgetItem(text);
    it->setTextAlignment(a | Qt::AlignVCenter);
    return it;
}
} // namespace

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

    // ── 解法: 波動 / 幾何光学 / ハイブリッド (mock の <Seg> 相当) ────────────
    // OpticalSolver enum (FDTD/RCWA/BPM/FMM) は Runner のカーネル選択と .ofdx に
    // 直結するため増やせない。mock の Raycast / ハイブリッドはここで UI 専用の
    // ローカル state として持ち、ヒント文だけ mock どおり切り替える。
    m_geoMethod = new QComboBox(ss);
    m_geoMethod->addItem(I18n::tr("optm_geo_fdtd"));      // 0 = 全波動FDTD
    m_geoMethod->addItem(I18n::tr("opt_raycast"));        // 1 = 幾何光学
    m_geoMethod->addItem(I18n::tr("opt_hybrid"));         // 2 = FDTD+Ray
    ss->form()->addRow(I18n::tr("optm_geo_method"), m_geoMethod);
    m_geoHint = mutedLabel(I18n::tr("optm_geo_hint_fdtd"), ss);
    ss->vbox()->addWidget(m_geoHint);
    // 解法 (波動/幾何) の選択はローカル state のみで計算へは渡らない
    ss->vbox()->addWidget(tabhelp::unwiredNote(ss, I18n::tr("optm_uw_method"),
                                               I18n::tr("optm_uw_method_ok")));

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

        // ── 層スタック (orcwa の rcwalayer キーに 1:1 対応) ──
        auto *stack = new SectionBox(I18n::tr("opt_rcwa_stack"), page);
        m_rcwaStack = new QTableWidget(0, 4, stack);
        m_rcwaStack->setHorizontalHeaderLabels(
            { I18n::tr("opt_rcwa_eps1"), I18n::tr("opt_rcwa_eps2"),
              I18n::tr("opt_rcwa_fill"), I18n::tr("opt_rcwa_thick") });
        m_rcwaStack->horizontalHeader()->setSectionResizeMode(
            QHeaderView::Stretch);
        m_rcwaStack->verticalHeader()->setVisible(true);
        m_rcwaStack->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_rcwaStack->setMinimumHeight(140);
        stack->vbox()->addWidget(m_rcwaStack);
        auto *btns = new QWidget(stack);
        auto *bh = new QHBoxLayout(btns);
        bh->setContentsMargins(0, 0, 0, 0);
        m_rcwaAdd = new QPushButton(I18n::tr("opt_rcwa_add"), btns);
        m_rcwaDel = new QPushButton(I18n::tr("opt_rcwa_del"), btns);
        bh->addWidget(m_rcwaAdd);
        bh->addWidget(m_rcwaDel);
        bh->addStretch(1);
        stack->vbox()->addWidget(btns);
        auto *stackHint = new QLabel(I18n::tr("opt_rcwa_stack_hint"), stack);
        stackHint->setWordWrap(true);
        stack->vbox()->addWidget(stackHint);
        m_rcwaWarn = new QLabel(stack);
        m_rcwaWarn->setWordWrap(true);
        m_rcwaWarn->setStyleSheet("color: #C42B1C;");
        m_rcwaWarn->setVisible(false);
        stack->vbox()->addWidget(m_rcwaWarn);
        page->vbox()->addWidget(stack);

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
    // [3] FMM — Fourier Modal Method は RCWA と同一手法の別名で、
    // 実行カーネルも OpenRCWA (orcwa) を共用する (Runner::kernelForProject)。
    // 周期・層スタックは RCWA ページの設定を共用し、調和次数だけ本ページの
    // 値 (fmmHarmonics) を使う。
    {
        auto *page = new SectionBox(I18n::tr("opt_fmm_section"), m_solverStack);
        auto *hint = new QLabel(I18n::tr("opt_fmm_kernel_hint"), page);
        hint->setWordWrap(true);
        page->vbox()->addWidget(hint);
        m_fmmHarmonics = new QSpinBox(page);
        m_fmmHarmonics->setRange(1, 201);
        m_fmmLi = new QCheckBox(page);
        // Li 規則は orcwa 側に対応する入力キーが無い (カーネルの実装に従う)
        // ため、切替が効かないことを明示する (絶対規則 5: 未実装を動作済みと
        // 表示しない)。
        m_fmmLi->setToolTip(I18n::tr("opt_fmm_li_tip"));
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
    // モックの表記 (近接場/遠方場変換) に合わせる。enum は OpticalMode::NF2FF。
    m_mode->addItem(I18n::tr("opt_nf_ff"));
    m_mode->addItem(I18n::tr("opt_spara"));
    sm->vbox()->addWidget(m_mode);
    v->addWidget(sm);

    // BPF spec (mode = BPF)
    auto *sb = new SectionBox(I18n::tr("opt_bpf_section"), body);
    m_secBpf = sb;
    m_bpfMin = new QLineEdit(sb); m_bpfMin->setMaximumWidth(100);
    m_bpfMax = new QLineEdit(sb); m_bpfMax->setMaximumWidth(100);
    m_bpfQ = new QLineEdit(sb); m_bpfQ->setMaximumWidth(100);
    // mock: <Row label={opt_target_band}> [min] ~ [max] nm — 1行にまとめる
    sb->form()->addRow(I18n::tr("opt_target_band"),
                       hrow({ m_bpfMin, mutedLabel("~", sb), m_bpfMax,
                              mutedLabel("nm", sb) }));
    sb->form()->addRow(I18n::tr("opt_q"), m_bpfQ);
    // 挿入損失 / 阻止域減衰 — 設計目標値 (.ofdx "bpf" il_db / stop_db へ保存)
    m_bpfIL = numEdit("0.5", sb);
    sb->form()->addRow(I18n::tr("optm_bpf_il"),
                       hrow({ m_bpfIL, mutedLabel("dB", sb) }));
    m_bpfStop = numEdit("40", sb);
    sb->form()->addRow(I18n::tr("optm_bpf_stop"),
                       hrow({ m_bpfStop, mutedLabel("dB", sb) }));
    // 設計目標の透過スペクトル — 目標帯域・Q・IL・阻止域から updateBpfPlot()
    // が再計算する (設計目標カーブの表示なので物理的に正当。計算結果ではない
    // ことを注記で明示する)。
    m_bpfPlot = new MiniPlot(sb);
    m_bpfPlot->setLabels("λ [nm]", "T [dB]");
    m_bpfPlot->setMinimumHeight(90);
    sb->vbox()->addWidget(new QLabel(I18n::tr("optm_bpf_spectrum"), sb));
    sb->vbox()->addWidget(m_bpfPlot);
    sb->vbox()->addWidget(mutedLabel(I18n::tr("optm_bpf_target_note"), sb));
    v->addWidget(sb);

    // Ring spec (mode = Ring)
    auto *sr = new SectionBox(I18n::tr("opt_ring_section"), body);
    m_secRing = sr;
    m_ringR = new QLineEdit(sr); m_ringR->setMaximumWidth(100);
    m_ringGap = new QLineEdit(sr); m_ringGap->setMaximumWidth(100);
    sr->form()->addRow(I18n::tr("opt_radius"), m_ringR);
    sr->form()->addRow(I18n::tr("opt_gap"), m_ringGap);
    // 算出値表示 (mock の <span className="mono">, R=5μm / gap=200nm の設計例)
    // — 計算結果ではなく固定の設計例であることを明記する (絶対規則 5)
    sr->form()->addRow(I18n::tr("optm_fsr"),
                       hrow({ monoLabel("~16.5 nm", sr),
                              mutedLabel("@λ=1550nm", sr),
                              mutedLabel(I18n::tr("optm_design_example"), sr) }));
    sr->form()->addRow(I18n::tr("optm_finesse"),
                       hrow({ monoLabel("~85", sr),
                              mutedLabel(I18n::tr("optm_design_example"), sr) }));
    sr->form()->addRow(I18n::tr("optm_q_factor"),
                       hrow({ monoLabel("~80,000", sr),
                              mutedLabel(I18n::tr("optm_design_example"), sr) }));
    // スルー/ドロップポートの出力選択 (.ofdx "ring" thru_port / drop_port)
    m_ringThru = makeCheck(I18n::tr("optm_thru_port"), true, sr);
    m_ringDrop = makeCheck(I18n::tr("optm_drop_port"), true, sr);
    sr->vbox()->addLayout(hrow({ m_ringThru, m_ringDrop }));
    v->addWidget(sr);

    // ── 導波路モード解析 / Waveguide mode (mode = Waveguide) ────────────────
    auto *swg = new SectionBox(I18n::tr("opt_wg"), body);
    m_secWg = swg;
    m_wgTe0 = makeCheck("TE0", true, swg);
    m_wgTe1 = makeCheck("TE1", false, swg);
    swg->form()->addRow(I18n::tr("optm_mode_te"), hrow({ m_wgTe0, m_wgTe1 }));
    m_wgTm0 = makeCheck("TM0", false, swg);
    m_wgTm1 = makeCheck("TM1", false, swg);
    swg->form()->addRow(I18n::tr("optm_mode_tm"), hrow({ m_wgTm0, m_wgTm1 }));
    swg->form()->addRow(
        I18n::tr("optm_neff"),
        hrow({ monoLabel("2.412 (TE0) / 1.873 (TM0)", swg),
               mutedLabel(I18n::tr("optm_design_example"), swg) }));
    m_wgLoss = numEdit("0.3", swg);
    swg->form()->addRow(I18n::tr("optm_loss"), m_wgLoss);
    // モード選択・損失は .ofdx "waveguide" へ保存 (カーネル連携は未実装)
    v->addWidget(swg);

    // ── MZI 干渉計 (mode = MZI) ────────────────────────────────────────────
    auto *smz = new SectionBox(I18n::tr("opt_mzi"), body);
    m_secMzi = smz;
    m_mziDeltaL = numEdit("50.0", smz, 120);
    smz->form()->addRow(I18n::tr("optm_mzi_dl"),
                        hrow({ m_mziDeltaL, mutedLabel("μm", smz) }));
    smz->form()->addRow(I18n::tr("optm_fsr"),
                        hrow({ monoLabel("~9.6 nm", smz),
                               mutedLabel(I18n::tr("optm_design_example"), smz) }));
    m_mziThermo  = makeCheck(I18n::tr("optm_mzi_thermo"), true, smz);
    m_mziElectro = makeCheck(I18n::tr("optm_mzi_eo"), false, smz);
    smz->form()->addRow(I18n::tr("optm_mzi_shifter"),
                        hrow({ m_mziThermo, m_mziElectro }));
    // ΔL・位相シフタは .ofdx "mzi" へ保存
    v->addWidget(smz);

    // ── メタサーフェス (mode = Metasurface) ────────────────────────────────
    auto *sms = new SectionBox(I18n::tr("opt_meta"), body);
    m_secMeta = sms;
    m_metaPeriod = numEdit("400", sms);
    sms->form()->addRow(I18n::tr("optm_meta_period"),
                        hrow({ m_metaPeriod, mutedLabel("nm", sms) }));
    m_metaShape = new QComboBox(sms);
    m_metaShape->addItem(I18n::tr("optm_meta_pillar"));
    m_metaShape->addItem(I18n::tr("optm_meta_hole"));
    m_metaShape->addItem(I18n::tr("optm_meta_cross"));
    sms->form()->addRow(I18n::tr("optm_meta_nano"), m_metaShape);
    m_metaPhase = new QComboBox(sms);
    m_metaPhase->addItem(I18n::tr("optm_meta_flat"));
    m_metaPhase->addItem(I18n::tr("optm_meta_steer"));
    m_metaPhase->addItem(I18n::tr("optm_meta_oam"));
    sms->form()->addRow(I18n::tr("optm_meta_phase"), m_metaPhase);
    // 周期・形状・位相設計は .ofdx "metasurface" へ保存
    v->addWidget(sms);

    // ── フォトニック結晶 (mode = PhC) ──────────────────────────────────────
    auto *sph = new SectionBox(I18n::tr("opt_phc"), body);
    m_secPhc = sph;
    m_phcLattice = new QComboBox(sph);
    m_phcLattice->addItem(I18n::tr("optm_phc_tri"));
    m_phcLattice->addItem(I18n::tr("optm_phc_sq"));
    m_phcLattice->addItem(I18n::tr("optm_phc_honey"));
    sph->form()->addRow(I18n::tr("optm_phc_lattice"), m_phcLattice);
    m_phcA = numEdit("430", sph);
    sph->form()->addRow(I18n::tr("optm_phc_a"),
                        hrow({ m_phcA, mutedLabel("nm", sph) }));
    m_phcRa = numEdit("0.30", sph);
    sph->form()->addRow(I18n::tr("optm_phc_ra"), m_phcRa);
    m_phcBand   = makeCheck(I18n::tr("optm_phc_band"), true, sph);
    m_phcDefect = makeCheck(I18n::tr("optm_phc_defect"), false, sph);
    sph->vbox()->addLayout(hrow({ m_phcBand, m_phcDefect }));
    // 格子・a・r/a・解析チェックは .ofdx "phc" へ保存
    v->addWidget(sph);

    // ── 近接場/遠方場変換 (mode = NF2FF) ──────────────────────────────────
    auto *snf = new SectionBox(I18n::tr("opt_nf_ff"), body);
    m_secNfff = snf;
    m_nfffSurface = new QComboBox(snf);
    m_nfffSurface->addItem(I18n::tr("optm_nf_box"));
    m_nfffSurface->addItem(I18n::tr("optm_nf_sphere"));
    m_nfffSurface->setCurrentIndex(0);           // mock 既定 = 直方体
    snf->form()->addRow(I18n::tr("optm_nf_surface"), m_nfffSurface);
    m_nfffDistance = numEdit("1.0e3", snf, 120);
    snf->form()->addRow(I18n::tr("optm_nf_dist"),
                        hrow({ m_nfffDistance, mutedLabel("λ", snf) }));
    // 変換面・観測距離は .ofdx "nf2ff" へ保存 (ofd_post 連携は未実装)
    v->addWidget(snf);

    // ── S パラメータ抽出 (mode = SParam) ──────────────────────────────────
    auto *ssp = new SectionBox(I18n::tr("opt_spara"), body);
    m_secSparam = ssp;
    m_spPorts = new QSpinBox(ssp);
    m_spPorts->setRange(1, 64);
    m_spPorts->setValue(2);
    m_spPorts->setMaximumWidth(100);
    ssp->form()->addRow(I18n::tr("optm_sp_ports"), m_spPorts);
    // 入力/出力ポート — S21 を取る対象ポート対 (mock 既定のポート数 2 に合わせ
    // 1→2)。ポート数・チェック群とともに .ofdx "sparam" へ保存する。
    m_spPortIn = new QSpinBox(ssp);
    m_spPortIn->setRange(1, 64);
    m_spPortIn->setValue(1);
    m_spPortIn->setMaximumWidth(100);
    ssp->form()->addRow(I18n::tr("opt_port_a"), m_spPortIn);
    m_spPortOut = new QSpinBox(ssp);
    m_spPortOut->setRange(1, 64);
    m_spPortOut->setValue(2);
    m_spPortOut->setMaximumWidth(100);
    ssp->form()->addRow(I18n::tr("opt_port_b"), m_spPortOut);
    m_spPortIn->setMaximum(m_spPorts->value());
    m_spPortOut->setMaximum(m_spPorts->value());
    m_spS11 = makeCheck(I18n::tr("optm_sp_s11"), true, ssp);
    m_spS21 = makeCheck(I18n::tr("optm_sp_s21"), true, ssp);
    ssp->vbox()->addLayout(hrow({ m_spS11, m_spS21 }));
    m_spPhase       = makeCheck(I18n::tr("optm_sp_phase"), true, ssp);
    m_spGroupDelay  = makeCheck(I18n::tr("optm_sp_gd"), false, ssp);
    ssp->vbox()->addLayout(hrow({ m_spPhase, m_spGroupDelay }));
    m_spExport = new QPushButton(I18n::tr("optm_sp_export"), ssp);
    connect(m_spExport, &QPushButton::clicked, this, &OpticalTab::exportSparam);
    ssp->vbox()->addLayout(hrow({ m_spExport }));
    v->addWidget(ssp);

    // ── Raycast 設定 / Geometric Optics ────────────────────────────────────
    // 幾何光学レイトレース (大規模光学系・遠方・カメラ) のトレース設定。
    // Project に対応フィールドが無いのでローカル state のみ (モック既定値)。
    auto *sray = new SectionBox(I18n::tr("optray_section"), body);
    m_rayCount = new QSpinBox(sray);
    m_rayCount->setRange(1, 1000000000);
    m_rayCount->setGroupSeparatorShown(true);
    m_rayCount->setValue(1000000);
    m_rayCount->setMaximumWidth(140);
    sray->form()->addRow(
        I18n::tr("optray_num_rays"),
        hrow({ m_rayCount, new QLabel(I18n::tr("optray_rays_unit"), sray) }));
    m_rayBounces = new QSpinBox(sray);
    m_rayBounces->setRange(1, 10000);
    m_rayBounces->setValue(12);
    m_rayBounces->setMaximumWidth(100);
    sray->form()->addRow(I18n::tr("optray_max_bounces"), m_rayBounces);
    m_rayMinEnergy = new QLineEdit("-60", sray);
    m_rayMinEnergy->setMaximumWidth(100);
    sray->form()->addRow(I18n::tr("optray_min_energy"),
                         hrow({ m_rayMinEnergy, new QLabel("dB", sray) }));
    m_raySampling = new QComboBox(sray);
    m_raySampling->addItem(I18n::tr("optray_uniform"));
    m_raySampling->addItem(I18n::tr("optray_jittered"));
    m_raySampling->addItem(I18n::tr("optray_qmc"));
    m_raySampling->addItem(I18n::tr("optray_importance"));
    m_raySampling->setCurrentIndex(2);           // mock 既定 = QMC
    sray->form()->addRow(I18n::tr("optray_sampling"), m_raySampling);
    m_raySpecular = makeCheck(I18n::tr("optray_specular"), true, sray);
    m_rayDiffuse  = makeCheck(I18n::tr("optray_diffuse"), true, sray);
    sray->form()->addRow(I18n::tr("optray_refl_model"),
                         hrow({ m_raySpecular, m_rayDiffuse }));
    // 拡散次数 — 拡散反射を何段まで追跡するか (mock はウィジェット無しの語彙のみ)
    m_rayDiffOrder = new QSpinBox(sray);
    m_rayDiffOrder->setRange(0, 32);
    m_rayDiffOrder->setValue(2);
    m_rayDiffOrder->setMaximumWidth(100);
    sray->form()->addRow(I18n::tr("optray_diff_ord"), m_rayDiffOrder);
    m_rayPolarized  = makeCheck(I18n::tr("optray_polarized"), false, sray);
    m_rayDispersion = makeCheck(I18n::tr("optray_dispersion"), true, sray);
    m_rayFresnel    = makeCheck(I18n::tr("optray_fresnel"), true, sray);
    sray->form()->addRow(
        I18n::tr("optray_physics"),
        hrow({ m_rayPolarized, m_rayDispersion, m_rayFresnel }));
    m_rayVizEnable = makeCheck(I18n::tr("optray_viz_rays"), false, sray);
    m_rayVizCount = new QSpinBox(sray);
    m_rayVizCount->setRange(1, 1000000);
    m_rayVizCount->setValue(500);
    m_rayVizCount->setMaximumWidth(100);
    sray->form()->addRow(
        I18n::tr("optray_raydiag"),
        hrow({ m_rayVizEnable, m_rayVizCount,
               new QLabel(I18n::tr("optray_rays_unit"), sray) }));
    auto *gpuRow = new QHBoxLayout();
    gpuRow->setContentsMargins(0, 0, 0, 0);
    gpuRow->addWidget(makeBadge(I18n::tr("optray_gpu"), true, sray));
    gpuRow->addWidget(mutedLabel(I18n::tr("optray_gpu_hint"), sray));
    gpuRow->addStretch(1);
    sray->vbox()->addLayout(gpuRow);
    // レイトレーサ本体が未実装のため、この節はローカル state のみ
    sray->vbox()->addWidget(tabhelp::unwiredNote(sray, I18n::tr("optm_uw_ray")));
    v->addWidget(sray);

    // ── 光学系定義 / Optical system ────────────────────────────────────────
    // レンズ面データ (mock の q-table) + 収差解析オプション。
    auto *ssys = new SectionBox(I18n::tr("optray_sys_section"), body);
    m_optSysTable = new QTableWidget(0, 6, ssys);
    m_optSysTable->setHorizontalHeaderLabels(
        { "#", I18n::tr("optray_col_type"), "R [mm]",
          I18n::tr("optray_col_thick"), I18n::tr("optray_col_mat"),
          I18n::tr("optray_col_stop") });
    m_optSysTable->horizontalHeader()->setSectionResizeMode(
        QHeaderView::Stretch);
    m_optSysTable->verticalHeader()->setVisible(false);
    m_optSysTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ssys->vbox()->addWidget(m_optSysTable);
    refreshOpticalSystem();      // 面データはレンズエディタと同じモデル
    m_optSeidel   = makeCheck(I18n::tr("optray_seidel"), true, ssys);
    m_optSpot     = makeCheck(I18n::tr("optray_spot"), true, ssys);
    m_optMtf      = makeCheck(I18n::tr("optray_mtf"), false, ssys);
    m_optRayAberr = makeCheck(I18n::tr("optray_ray_aberr"), false, ssys);
    ssys->vbox()->addLayout(
        hrow({ m_optSeidel, m_optSpot, m_optMtf, m_optRayAberr }));
    // 面データはレンズエディタタブと同じモデル (.ofdx に保存される)。
    // 実計算 (3 次収差・スポット・光線収差図) もそちらにあるので誘導する。
    ssys->vbox()->addWidget(mutedLabel(I18n::tr("optm_surf_shared"), ssys));
    ssys->vbox()->addWidget(mutedLabel(I18n::tr("optm_seidel_where"), ssys));
    ssys->vbox()->addWidget(tabhelp::unwiredNote(ssys, I18n::tr("optm_uw_surf")));
    v->addWidget(ssys);

    // ── ハイブリッド連携 / FDTD↔Ray bridge ─────────────────────────────────
    auto *shyb = new SectionBox(I18n::tr("optray_hyb_section"), body);
    shyb->form()->addRow(
        I18n::tr("optray_hyb_region"),
        hrow({ makeBadge(I18n::tr("optray_hyb_region_badge"), false, shyb) }));
    m_hybModeDecomp = makeCheck(I18n::tr("optray_hyb_modedec"), true, shyb);
    m_hybGaussian   = makeCheck(I18n::tr("optray_hyb_gauss"), false, shyb);
    shyb->form()->addRow(I18n::tr("optray_hyb_bconv"),
                         hrow({ m_hybModeDecomp, m_hybGaussian }));
    m_hybPropModel = new QComboBox(shyb);
    m_hybPropModel->addItems({ "Geometric", "BPM", "Physical Optics" });
    m_hybPropModel->setCurrentIndex(0);          // mock 既定 = Geometric
    shyb->form()->addRow(I18n::tr("optray_hyb_prop"), m_hybPropModel);
    shyb->vbox()->addWidget(mutedLabel(I18n::tr("optray_hyb_hint"), shyb));
    // ハイブリッド連携は未実装 — この節はローカル state のみ
    shyb->vbox()->addWidget(tabhelp::unwiredNote(shyb, I18n::tr("optm_uw_hybrid")));
    v->addWidget(shyb);

    // ── 分散モデル / Dispersion model (mock 末尾の <Section>) ───────────────
    // Drude / Lorentz / Sellmeier。材料タブの分散モデルとは独立の解析既定値で、
    // OpticalOpts に対応フィールドが無いためローカル state (mock 既定 = Lorentz)。
    auto *sdisp = new SectionBox(I18n::tr("optm_disp_section"), body);
    m_dispModel = new QComboBox(sdisp);
    m_dispModel->addItems({ "Drude", "Lorentz", "Sellmeier" });
    m_dispModel->setCurrentIndex(1);             // mock 既定 = Lorentz
    sdisp->vbox()->addWidget(m_dispModel);
    // この節はローカル state のみ
    sdisp->vbox()->addWidget(tabhelp::unwiredNote(sdisp, I18n::tr("optm_uw_out")));
    v->addWidget(sdisp);

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
    // モード切替でモード別セクションの表示を切り替える (mock の条件付き描画)
    connect(m_mode, &QComboBox::currentIndexChanged,
            this, &OpticalTab::updateModeSections);
    connect(m_bpmAlgo, &QComboBox::currentIndexChanged, this, applyCb);
    connect(m_bpmInput, &QComboBox::currentIndexChanged, this, applyCb);
    connect(m_psScale, &QComboBox::currentIndexChanged, this, applyCb);
    // モード別セクションのコンボ (メタサーフェス / PhC / NF2FF) も永続化対象
    for (auto *c : { m_metaShape, m_metaPhase, m_phcLattice, m_nfffSurface })
        connect(c, &QComboBox::currentIndexChanged, this, applyCb);
    for (auto *e : { m_lambdaMin, m_lambdaMax, m_rcwaPx, m_rcwaPy, m_bpmDz,
                     m_bpmN0, m_tpaBeta, m_psPmin, m_psPmax,
                     m_bpfMin, m_bpfMax, m_bpfQ, m_ringR, m_ringGap,
                     m_bpfIL, m_bpfStop, m_wgLoss, m_mziDeltaL,
                     m_metaPeriod, m_phcA, m_phcRa, m_nfffDistance })
        connect(e, &QLineEdit::editingFinished, this, applyCb);
    for (auto *s : { m_lambdaDiv, m_rcwaNx, m_rcwaNy, m_rcwaLayers,
                     m_fmmHarmonics, m_tpaMatId, m_psPoints,
                     m_spPorts, m_spPortIn, m_spPortOut })
        connect(s, &QSpinBox::valueChanged, this, applyCb);
    connect(m_fmmLi, &QCheckBox::toggled, this, applyCb);
    connect(m_tpaEnable, &QCheckBox::toggled, this, applyCb);
    connect(m_psEnable, &QCheckBox::toggled, this, applyCb);
    // モード別セクションのチェック群 (Ring / 導波路 / MZI / PhC / S パラメータ)
    for (auto *c : { m_ringThru, m_ringDrop, m_wgTe0, m_wgTe1, m_wgTm0,
                     m_wgTm1, m_mziThermo, m_mziElectro, m_phcBand,
                     m_phcDefect, m_spS11, m_spS21, m_spPhase,
                     m_spGroupDelay })
        connect(c, &QCheckBox::toggled, this, applyCb);

    // 解法 (波動 / 幾何光学 / ハイブリッド): UI 専用なので apply() は呼ばず、
    // mock と同じくヒント文だけ差し替える。
    connect(m_geoMethod, &QComboBox::currentIndexChanged, this, [this](int i) {
        const char *k = (i == 1) ? "optm_geo_hint_ray"
                      : (i == 2) ? "optm_geo_hint_hybrid"
                                 : "optm_geo_hint_fdtd";
        m_geoHint->setText(I18n::tr(k));
        updateGeoMethodView();
    });
    updateGeoMethodView();
    // ポート数を変えたら入力/出力ポート番号の上限も追従させる
    connect(m_spPorts, &QSpinBox::valueChanged, this, [this](int n) {
        m_spPortIn->setMaximum(std::max(1, n));
        m_spPortOut->setMaximum(std::max(1, n));
    });

    // ── RCWA 層スタックの編集 ──
    connect(m_rcwaStack, &QTableWidget::cellChanged, this, applyCb);
    connect(m_rcwaAdd, &QPushButton::clicked, this, [this] {
        // 直前の層をコピーして追加する (無ければ既定の空気層)。
        QVector<RcwaLayer> &ls = m_p->optical().rcwaLayerList;
        ls.push_back(ls.isEmpty() ? RcwaLayer{} : ls.last());
        refreshRcwaTable();
    refreshOpticalSystem();
        apply();
    });
    connect(m_rcwaDel, &QPushButton::clicked, this, [this] {
        const int row = m_rcwaStack->currentRow();
        QVector<RcwaLayer> &ls = m_p->optical().rcwaLayerList;
        if (row < 0 || row >= ls.size()) return;
        ls.removeAt(row);
        refreshRcwaTable();
        apply();
    });

    connect(project, &Project::loaded, this, &OpticalTab::refresh);
    refresh();
    // refresh() でモード index が変わらなかった場合も初期表示を確定させる
    updateModeSections();
}

// ── モード別セクションの表示切替 ────────────────────────────────────────────
// mock は {mode === "bpf" && <Section…>} のように選択モードのセクションだけを
// 描画する。Raycast / 光学系定義 / ハイブリッド連携 / 分散モデルは mock でも
// モードに依らず出る場合があるので常時表示のままとする。
void OpticalTab::updateModeSections()
{
    const int m = m_mode->currentIndex();
    m_secBpf->setVisible(m == int(OpticalMode::BPF));
    m_secWg->setVisible(m == int(OpticalMode::Waveguide));
    m_secRing->setVisible(m == int(OpticalMode::Ring));
    m_secMzi->setVisible(m == int(OpticalMode::MZI));
    m_secMeta->setVisible(m == int(OpticalMode::Metasurface));
    m_secPhc->setVisible(m == int(OpticalMode::PhC));
    m_secNfff->setVisible(m == int(OpticalMode::NF2FF));
    m_secSparam->setVisible(m == int(OpticalMode::SParam));
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
    const QStringList rcwaWarns = applyRcwaTable();
    m_rcwaWarn->setText(rcwaWarns.join('\n'));
    m_rcwaWarn->setVisible(!rcwaWarns.isEmpty());
    o.bpmAlgorithm = m_bpmAlgo->currentIndex();
    o.bpmDz = m_bpmDz->text().toDouble();
    o.bpmRefIndex = m_bpmN0->text().toDouble();
    o.bpmInputMode = m_bpmInput->currentIndex();
    o.fmmHarmonics = m_fmmHarmonics->value();
    o.fmmLiRules = m_fmmLi->isChecked();
    o.bpfBandMin = m_bpfMin->text().toDouble();
    o.bpfBandMax = m_bpfMax->text().toDouble();
    o.bpfQ = m_bpfQ->text().toDouble();
    o.bpfIL_dB = m_bpfIL->text().toDouble();
    o.bpfStop_dB = m_bpfStop->text().toDouble();
    o.ringRadius_um = m_ringR->text().toDouble();
    o.ringGap_nm = m_ringGap->text().toDouble();
    o.ringThruPort = m_ringThru->isChecked();
    o.ringDropPort = m_ringDrop->isChecked();

    // ── 光解析モード別設定 (.ofdx へ保存。カーネル入力 .ofd は不変) ──
    o.wgTE0 = m_wgTe0->isChecked();
    o.wgTE1 = m_wgTe1->isChecked();
    o.wgTM0 = m_wgTm0->isChecked();
    o.wgTM1 = m_wgTm1->isChecked();
    o.wgLoss_dBcm = m_wgLoss->text().toDouble();
    o.mziDeltaL_um = m_mziDeltaL->text().toDouble();
    o.mziThermo = m_mziThermo->isChecked();
    o.mziElectro = m_mziElectro->isChecked();
    o.metaPeriod_nm = m_metaPeriod->text().toDouble();
    o.metaShape = m_metaShape->currentIndex();
    o.metaPhase = m_metaPhase->currentIndex();
    o.phcLattice = m_phcLattice->currentIndex();
    o.phcA_nm = m_phcA->text().toDouble();
    o.phcRoverA = m_phcRa->text().toDouble();
    o.phcBand = m_phcBand->isChecked();
    o.phcDefect = m_phcDefect->isChecked();
    o.nfffSurface = m_nfffSurface->currentIndex();
    o.nfffDistance_lambda = m_nfffDistance->text().toDouble();
    o.spPorts = m_spPorts->value();
    o.spPortIn = m_spPortIn->value();
    o.spPortOut = m_spPortOut->value();
    o.spS11 = m_spS11->isChecked();
    o.spS21 = m_spS21->isChecked();
    o.spPhase = m_spPhase->isChecked();
    o.spGroupDelay = m_spGroupDelay->isChecked();

    // 目標帯域 / Q / IL / 阻止域が変わったので設計目標カーブを描き直す
    updateBpfPlot();

    // ── 非線形 (TPA) / ONN 活性化 — バリデーション付き ──
    // 不正値はモデルに書き込まず警告を表示する (β>0, 0<Pmin≤Pmax, 点数≥1)。
    // さらに、不正なまま有効フラグを立てておくと「警告が出ているのに前回値
    // (既定 424) でカーネルが走る」状態になるため、有効フラグを落とす。
    o.tpaMaterialId = m_tpaMatId->value();
    o.psPoints = m_psPoints->value();          // QSpinBox が ≥1 を保証
    o.psLog = (m_psScale->currentIndex() == 0);
    QStringList warns;
    const double beta = m_tpaBeta->text().toDouble();
    const bool betaOk = isValidTpaBeta(beta);
    if (betaOk)
        o.tpaBeta_cmGW = beta;
    else if (m_tpaEnable->isChecked())
        warns << I18n::tr("opt_tpa_warn_beta")
              << I18n::tr("opt_tpa_warn_disabled");
    o.tpaEnabled = m_tpaEnable->isChecked() && betaOk;

    const double pmin = m_psPmin->text().toDouble();
    const double pmax = m_psPmax->text().toDouble();
    const bool rangeOk = isValidPowerSweepRange(pmin, pmax);
    if (rangeOk) {
        o.psPmin_W = pmin;
        o.psPmax_W = pmax;
    } else if (m_psEnable->isChecked()) {
        warns << I18n::tr("opt_ps_warn_range")
              << I18n::tr("opt_ps_warn_disabled");
    }
    o.powerSweepEnabled = m_psEnable->isChecked() && rangeOk;
    m_tpaWarn->setText(warns.join('\n'));
    m_tpaWarn->setVisible(!warns.isEmpty());
    updateTpaWidgetState();

    m_p->touch();
}

// ── RCWA 層スタック ─────────────────────────────────────────────────────────
// テーブルの内容をモデルへ書き戻し、不正な行を赤字にする。戻り値は警告文。
// 値はそのままモデルへ入れる (UI とモデルを乖離させない)。不正な層が 1 つ
// でもあれば OfdIO 側の isValidRcwaStack() ゲートが RCWA 行の書き出しを丸ごと
// 止めるので、不正な設定が orcwa へ渡ることはない。
QStringList OpticalTab::applyRcwaTable()
{
    QVector<RcwaLayer> layers;
    QStringList warns;
    for (int r = 0; r < m_rcwaStack->rowCount(); ++r) {
        auto cellVal = [this, r](int c) {
            QTableWidgetItem *it = m_rcwaStack->item(r, c);
            return it ? it->text().trimmed().toDouble() : 0.0;
        };
        RcwaLayer l;
        l.eps1 = cellVal(0);
        l.eps2 = cellVal(1);
        l.fill = cellVal(2);
        l.thickness_nm = cellVal(3);
        const bool ok = isValidRcwaLayer(l);
        if (!ok)
            warns << I18n::tr("opt_rcwa_warn_layer").arg(r + 1);
        for (int c = 0; c < 4; ++c) {
            if (QTableWidgetItem *it = m_rcwaStack->item(r, c))
                it->setForeground(ok ? QBrush() : QBrush(QColor("#C42B1C")));
        }
        layers.push_back(l);
    }
    if (!warns.isEmpty())
        warns << I18n::tr("opt_rcwa_warn_skip");
    else if (layers.isEmpty() &&
             m_p->optical().solver == OpticalSolver::RCWA)
        warns << I18n::tr("opt_rcwa_warn_empty");
    m_p->optical().rcwaLayerList = layers;
    return warns;
}

void OpticalTab::refreshRcwaTable()
{
    const bool wasUpdating = m_updating;
    m_updating = true;                 // cellChanged → apply() の再入を防ぐ
    const QVector<RcwaLayer> &ls = m_p->optical().rcwaLayerList;
    m_rcwaStack->setRowCount(ls.size());
    for (int r = 0; r < ls.size(); ++r) {
        const double v[4] = { ls[r].eps1, ls[r].eps2, ls[r].fill,
                              ls[r].thickness_nm };
        const bool ok = isValidRcwaLayer(ls[r]);
        for (int c = 0; c < 4; ++c) {
            auto *it = new QTableWidgetItem(QString::number(v[c], 'g', 8));
            it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            if (!ok) it->setForeground(QBrush(QColor("#C42B1C")));
            m_rcwaStack->setItem(r, c, it);
        }
    }
    m_updating = wasUpdating;
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
    refreshRcwaTable();
    refreshOpticalSystem();
    m_rcwaWarn->setVisible(false);
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
    m_bpfIL->setText(QString::number(o.bpfIL_dB, 'g', 8));
    m_bpfStop->setText(QString::number(o.bpfStop_dB, 'g', 8));
    m_ringR->setText(QString::number(o.ringRadius_um, 'g', 8));
    m_ringGap->setText(QString::number(o.ringGap_nm, 'g', 8));
    m_ringThru->setChecked(o.ringThruPort);
    m_ringDrop->setChecked(o.ringDropPort);

    // ── 光解析モード別設定 (.ofdx から復元) ──
    m_wgTe0->setChecked(o.wgTE0);
    m_wgTe1->setChecked(o.wgTE1);
    m_wgTm0->setChecked(o.wgTM0);
    m_wgTm1->setChecked(o.wgTM1);
    m_wgLoss->setText(QString::number(o.wgLoss_dBcm, 'g', 8));
    m_mziDeltaL->setText(QString::number(o.mziDeltaL_um, 'g', 8));
    m_mziThermo->setChecked(o.mziThermo);
    m_mziElectro->setChecked(o.mziElectro);
    m_metaPeriod->setText(QString::number(o.metaPeriod_nm, 'g', 8));
    m_metaShape->setCurrentIndex(o.metaShape);
    m_metaPhase->setCurrentIndex(o.metaPhase);
    m_phcLattice->setCurrentIndex(o.phcLattice);
    m_phcA->setText(QString::number(o.phcA_nm, 'g', 8));
    m_phcRa->setText(QString::number(o.phcRoverA, 'g', 8));
    m_phcBand->setChecked(o.phcBand);
    m_phcDefect->setChecked(o.phcDefect);
    m_nfffSurface->setCurrentIndex(o.nfffSurface);
    m_nfffDistance->setText(QString::number(o.nfffDistance_lambda, 'g', 8));
    // ポート数を先に入れて上限を確定させてから対象ポート対を入れる
    m_spPorts->setValue(o.spPorts);
    m_spPortIn->setValue(o.spPortIn);
    m_spPortOut->setValue(o.spPortOut);
    m_spS11->setChecked(o.spS11);
    m_spS21->setChecked(o.spS21);
    m_spPhase->setChecked(o.spPhase);
    m_spGroupDelay->setChecked(o.spGroupDelay);

    updateBpfPlot();
    m_updating = false;
}

// ── BPF 設計目標の透過スペクトル ────────────────────────────────────────────
// mock の固定 Butterworth カーブを、ユーザーの設計目標値から再計算して描く:
//   T(λ) = T_IL / (1 + ((λ−λ0)/w)^8)          — 4次 Butterworth 型 (mock と同型)
//   λ0 = (min+max)/2, w = (max−min)/2 — 帯域端 (bpfBandMin/Max) が
//   −3 dB (×T_IL) になる。帯域幅が退化しているときは共振器 1 段の
//   FWHM = λ0/Q を代わりに使う。下限は阻止域減衰でクランプ (設計目標の floor)。
// 表示するのはあくまで「設計目標」であり計算結果ではない (注記ラベルで明示)。
void OpticalTab::updateBpfPlot()
{
    const OpticalOpts &o = m_p->optical();
    double lo = o.bpfBandMin, hi = o.bpfBandMax;
    if (hi < lo) std::swap(lo, hi);
    const double l0 = 0.5 * (lo + hi);          // 中心波長 [nm]
    if (l0 <= 0) {                              // 帯域が無意味 → 描画しない
        m_bpfPlot->setSeries({});
        return;
    }
    double w = 0.5 * (hi - lo);                 // 帯域半幅 [nm] (−3dB 点)
    if (w <= 0)                                 // 退化時: FWHM = λ0/Q
        w = 0.5 * l0 / std::max(o.bpfQ, 1.0);
    const double il_dB   = std::max(o.bpfIL_dB, 0.0);
    const double stop_dB = std::max(o.bpfStop_dB, 0.0);
    const double tPass  = std::pow(10.0, -il_dB / 10.0);
    const double tFloor = tPass * std::pow(10.0, -stop_dB / 10.0);
    MiniSeries s;
    s.color = QColor("#B83280");                // mock: var(--acc-opt)
    const int n = 101;
    for (int i = 0; i < n; ++i) {
        const double x = l0 + w * (10.0 * i / (n - 1) - 5.0);   // λ0 ± 5w
        const double t = tPass / (1.0 + std::pow((x - l0) / w, 8.0));
        s.pts.push_back({ x, 10.0 * std::log10(std::max(t, tFloor)) });
    }
    m_bpfPlot->setYRange(-(il_dB + stop_dB) - 5.0, 2.0);
    m_bpfPlot->setSeries(QVector<MiniSeries>{ s });
}

// ── ONN 活性化カーブ結果表示 ────────────────────────────────────────────────
void OpticalTab::showActivationResult(const QString &workdir, double aeff_m2,
                                      double beta_cmGW, double length_m)
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
    // β と L は実行開始時のスナップショット (引数) を使う — 表示時点の
    // ライブ値を使うと、実行中に UI を編集した場合に実測 CSV と対応しない
    // カーブが重なる。
    if (aeff_m2 > 0 && length_m > 0 && beta_cmGW > 0) {
        MiniSeries at, ap;
        at.color = ap.color = QColor("#C42B1C");
        at.dashed = ap.dashed = true;
        at.label = ap.label = I18n::tr("opt_onn_analytic");
        for (const ActivationPoint &a : pts) {
            const double T = ActivationCurve::analyticTransmission(
                a.pin, beta_cmGW, aeff_m2, length_m);
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

// ── S パラメータの書き出し (Touchstone / CSV) ────────────────────────────────
// カーネル (ofd / orcwa) は plotspara + 観測点があるとき作業ディレクトリへ
// test.snp を書く。ただしこれは Touchstone 準拠ではなく **ポート 1 を励振した
// 第 1 列 (S_n1)** だけを持つ (sol/outputSpara.c)。したがって:
//   - S11 と S_n1 は実測値として扱える → CSV (dB / 位相 / 群遅延) を出す
//   - 2 ポート Touchstone (.s2p) は S12 / S22 が無いので **書かない**
//     (相反性を仮定して埋めれば「計算していない値」を出力することになる)
// 準拠した全行列のファイル (他ツール製 / 将来のカーネル) が置かれていれば、
// 選択したポート対の部分行列をそのまま .s1p / .s2p として書き出す。
void OpticalTab::exportSparam()
{
    const QString title = I18n::tr("optsp_title");
    const QString wd = Runner::resolveWorkingDir(m_p, RunConfig{});

    QString src;
    if (!wd.isEmpty()) {
        const QStringList hits = QDir(wd).entryList(
            { "*.snp", "*.s?p", "*.s??p" }, QDir::Files, QDir::Time);
        if (!hits.isEmpty()) src = QDir(wd).filePath(hits.first());
    }
    if (src.isEmpty()) {
        QMessageBox::information(this, title,
                                 I18n::tr("optsp_none").arg(wd));
        return;
    }

    TouchstoneData d;
    QString err;
    if (!Touchstone::read(src, &d, &err, m_spPorts->value())) {
        QMessageBox::warning(this, title,
            I18n::tr("optsp_read_fail").arg(QFileInfo(src).fileName(), err));
        return;
    }

    const int pin  = m_spPortIn->value();
    const int pout = m_spPortOut->value();
    if (pin > d.ports || pout > d.ports) {
        QMessageBox::warning(this, title,
            I18n::tr("optsp_port_range")
                .arg(pin).arg(pout).arg(d.ports));
        return;
    }

    // 使える要素 — カーネル出力では第 1 列だけが計算済み
    const bool haveS11 = d.isKnown(pin, pin);
    const bool haveS21 = (pout != pin) && d.isKnown(pout, pin);
    if (!haveS11 && !haveS21) {
        QMessageBox::warning(this, title,
            I18n::tr("optsp_col1_only").arg(QFileInfo(src).fileName()));
        return;
    }
    const bool wantS11 = m_spS11->isChecked() && haveS11;
    const bool wantS21 = m_spS21->isChecked() && haveS21;
    if (!wantS11 && !wantS21) {
        QMessageBox::warning(this, title, I18n::tr("optsp_nosel"));
        return;
    }

    // Touchstone に出せるのは、選んだポート対の全要素が計算済みのときだけ
    QVector<int> sel;
    sel.push_back(pin);
    if (pout != pin) sel.push_back(pout);
    const TouchstoneData sub = Touchstone::subset(d, sel);
    const bool canTouchstone = !sub.isEmpty();

    const QString base = m_p->general().title.isEmpty()
                             ? QStringLiteral("sparam")
                             : m_p->general().title;
    const QString suggested =
        canTouchstone ? base + (sub.ports == 1 ? ".s1p" : ".s2p")
                      : base + "_sparam.csv";
    const QString dst = QFileDialog::getSaveFileName(
        this, title, suggested,
        canTouchstone ? I18n::tr("optsp_filter_ts") + ";;"
                            + I18n::tr("optsp_filter_csv")
                      : I18n::tr("optsp_filter_csv"));
    if (dst.isEmpty()) return;

    QString written;
    const bool dstIsCsv = dst.endsWith(".csv", Qt::CaseInsensitive);
    if (canTouchstone && !dstIsCsv) {
        if (!Touchstone::writeSnp(dst, sub, &err)) {
            QMessageBox::warning(this, title,
                                 I18n::tr("optsp_write_fail").arg(err));
            return;
        }
        written += I18n::tr("optsp_out_ts").arg(dst);
    }

    // CSV — dB (常に) / 位相 (m_spPhase) / 群遅延 (m_spGroupDelay)。
    // Touchstone を書いたときは同じ場所に <base>_sparam.csv を並べる。
    const QString csvPath =
        (dstIsCsv || !canTouchstone)
            ? (dst.endsWith(".csv", Qt::CaseInsensitive) ? dst : dst + ".csv")
            : QDir(QFileInfo(dst).path())
                  .filePath(QFileInfo(dst).completeBaseName() + "_sparam.csv");
    QFile cf(csvPath);
    if (!cf.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, title,
                             I18n::tr("optsp_write_fail").arg(cf.errorString()));
        return;
    }
    {
        const bool wantPhase = m_spPhase->isChecked();
        const bool wantGd    = m_spGroupDelay->isChecked();
        struct Entry {
            QString name;
            QVector<std::complex<double>> s;
            QVector<double> phi, tau;
        };
        QVector<Entry> ent;
        auto addEntry = [&](const QString &name, int row, int col) {
            Entry e;
            e.name = name;
            e.s = d.series(row, col);
            e.phi = Touchstone::unwrapPhaseRad(e.s);
            e.tau = Touchstone::groupDelaySec(d.freqHz, e.s);
            ent.push_back(e);
        };
        if (wantS11) addEntry(QStringLiteral("S%1%1").arg(pin), pin, pin);
        if (wantS21) addEntry(QStringLiteral("S%1%2").arg(pout).arg(pin),
                              pout, pin);

        QTextStream out(&cf);
        out << "frequency[Hz]";
        for (const Entry &e : ent) {
            out << ',' << e.name << "[dB]";
            if (wantPhase) out << ',' << e.name << "[deg]";
            if (wantGd)    out << ',' << e.name << "_groupdelay[ps]";
        }
        out << '\n';
        for (int i = 0; i < d.freqHz.size(); ++i) {
            out << QString::number(d.freqHz[i], 'e', 9);
            for (const Entry &e : ent) {
                const double mag = std::abs(e.s[i]);
                out << ',' << QString::number(
                    20.0 * std::log10(std::max(mag, 1e-30)), 'f', 6);
                if (wantPhase)
                    out << ',' << QString::number(
                        e.phi[i] * 180.0 / 3.14159265358979323846, 'f', 6);
                if (wantGd)
                    out << ',' << QString::number(e.tau[i] * 1e12, 'f', 6);
            }
            out << '\n';
        }
    }
    cf.close();
    written += I18n::tr("optsp_out_csv").arg(csvPath);

    if (!canTouchstone) written += I18n::tr("optsp_skip_2port") + "\n";

    QMessageBox::information(
        this, title,
        I18n::tr("optsp_done")
            .arg(QFileInfo(src).fileName(), written)
            .arg(d.freqHz.size())
            .arg(QString::number(d.freqHz.first(), 'g', 6),
                 QString::number(d.freqHz.last(), 'g', 6)));
}

// ── 光学系定義テーブル (レンズエディタと同じ面データの表示) ────────────────
// 出典は Project::optical().lensSurfaces。空なら既定の設計例 (Cooke triplet)
// を出す — レンズエディタの初期表示と同じものになる。
void OpticalTab::refreshOpticalSystem()
{
    if (!m_optSysTable) return;
    const QVector<LensSurfaceRow> &src = m_p->optical().lensSurfaces;
    const QVector<LensSurfaceRow> rows =
        src.isEmpty() ? defaultLensSurfaces() : src;
    m_optSysTable->setRowCount(rows.size());
    m_optSysTable->setMinimumHeight(rows.size() * 30 + 42);
    for (int r = 0; r < rows.size(); ++r) {
        const LensSurfaceRow &s = rows[r];
        m_optSysTable->setItem(r, 0,
                               alignedItem(QString::number(r + 1),
                                           Qt::AlignRight));
        m_optSysTable->setItem(r, 1, new QTableWidgetItem(s.type));
        m_optSysTable->setItem(r, 2, alignedItem(s.R, Qt::AlignRight));
        m_optSysTable->setItem(r, 3, alignedItem(s.thick, Qt::AlignRight));
        m_optSysTable->setItem(r, 4, new QTableWidgetItem(s.glass));
        m_optSysTable->setItem(
            r, 5, alignedItem(s.type == QStringLiteral("STO")
                                  ? QString::fromUtf8("●")
                                  : QString::fromUtf8("—"),
                              Qt::AlignHCenter));
        // 無効行はグレーで出す (行を隠すとテーブルと面テーブルが食い違う)
        if (!s.enabled)
            for (int c = 0; c < 6; ++c)
                if (auto *it = m_optSysTable->item(r, c))
                    it->setForeground(QBrush(QColor(0x88, 0x88, 0x88)));
    }
}

// 解法 (波動 / 幾何 / ハイブリッド) → 波動ソルバー設定の有効・無効。
// 幾何光学では外部カーネルを起動しないので、波動側の設定を触れなくして
// 理由を出す (設定できるのに効かない状態を作らない)。
void OpticalTab::updateGeoMethodView()
{
    if (!m_geoMethod || !m_solver || !m_solverStack) return;
    const bool wave = (m_geoMethod->currentIndex() != 1);
    m_solver->setEnabled(wave);
    m_solverStack->setEnabled(wave);
    const QString why = wave ? QString() : I18n::tr("optm_geo_wave_off");
    m_solver->setToolTip(why);
    m_solverStack->setToolTip(why);
}
