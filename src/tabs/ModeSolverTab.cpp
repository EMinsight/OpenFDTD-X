// ModeSolverTab.cpp
#include "ModeSolverTab.h"
#include "../optics/BendWaveguide.h"
#include "../optics/FdeModeSolver.h"
#include "../optics/MaterialDispersion.h"
#include "../widgets/FieldHeatmap.h"
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
#include <QStringList>
#include <QTableWidget>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <vector>

using namespace ofd;
using namespace ofd::tabhelp;

// ── タブ固有語彙 (mds_) — file-local 登録 ───────────────────────────────────
namespace {
const bool s_i18n = [] {
    I18n::reg("mds_sec_solver", "導波路モードソルバ FDE / Waveguide mode solver",
              "Waveguide mode solver (FDE)");
    I18n::reg("mds_hint",
        "断面2Dの固有モード解析。neff / ng / 閉込め係数 Γ / 実効断面積・分散指標・"
        "プロセスコーナーを内蔵 FDE ソルバで実計算します。",
        "2D cross-section eigenmode analysis: neff / ng / confinement Γ / "
        "effective area, dispersion metrics and process corners are computed "
        "by the built-in FDE solver.");
    I18n::reg("mds_solver_note",
        "屈折率は公刊 Sellmeier 係数による n(λ,T) — 材料・波長・温度が計算に反映"
        "されます。実屈折率のみを扱うため伝搬損失 (散乱・吸収) と曲げの放射損失"
        "は対象外です (曲げは接続のモード不整合損とカウスティック位置のみ算出)。",
        "Refractive indices come from published Sellmeier data as n(λ,T), so "
        "material, wavelength and temperature all enter the computation. Only "
        "real indices are handled, so propagation loss (scattering/absorption) "
        "and bend radiation loss are out of scope (for bends only the junction "
        "mismatch loss and the caustic position are computed).");
    I18n::reg("mds_shape", "断面形状", "Cross-section");
    I18n::reg("mds_shape_strip", "ストリップ", "Strip");
    I18n::reg("mds_shape_rib", "リブ (スラブ付)", "Rib (with slab)");
    I18n::reg("mds_core", "コア寸法", "Core size");
    I18n::reg("mds_w", "幅", "Width");
    I18n::reg("mds_h", "高", "Height");
    I18n::reg("mds_slab", "スラブ", "Slab");
    I18n::reg("mds_mat", "材料", "Materials");
    I18n::reg("mds_mat_core_l", "コア", "Core");
    I18n::reg("mds_mat_clad_l", "クラッド", "Cladding");
    I18n::reg("mds_mat_sub_l", "基板", "Substrate");
    I18n::reg("mds_mat_note",
              "n(λ) は材料エクスプローラと同じ公刊 Sellmeier 係数テーブル",
              "n(λ) uses the same published Sellmeier table as the material "
              "explorer");
    I18n::reg("mds_n_preview",
              "n(λ=%1nm, T=%2°C) → コア %3 / クラッド %4 / 基板 %5",
              "n(λ=%1 nm, T=%2 °C) → core %3 / cladding %4 / substrate %5");
    I18n::reg("mds_n_notemp", "  ※ dn/dT 未定義のため温度補正なし: %1",
              "  (no dn/dT published → no temperature correction: %1)");
    I18n::reg("mds_err_range",
              "λ = %1 nm は %2 の有効範囲 (%3〜%4 µm) の外です — 計算できません",
              "λ = %1 nm is outside the %2 validity range (%3–%4 µm) — cannot "
              "compute");
    I18n::reg("mds_err_rib", "スラブ厚はコア高さ未満にしてください",
              "The slab must be thinner than the core height");
    I18n::reg("mds_wl_pol", "波長・偏波", "Wavelength / polarization");
    I18n::reg("mds_temp", "温度", "Temperature");
    I18n::reg("mds_run", "▶ モード計算", "▶ Compute modes");
    I18n::reg("mds_status_idle",
              "未計算 — 「▶ モード計算」で断面 FDE 解析を実行します",
              "Not computed — press “▶ Compute modes” to run the FDE analysis");
    I18n::reg("mds_status_run", "計算中… (%1/%2)", "Computing… (%1/%2)");
    I18n::reg("mds_status_done",
              "完了: 導波モード %1 本 / 格子 %2 nm (%3 秒)",
              "Done: %1 guided mode(s), grid %2 nm (%3 s)");
    I18n::reg("mds_status_none",
              "導波モードなし — コアがクラッド/基板より高屈折率か、断面が"
              "カットオフ以下でないか確認してください",
              "No guided mode — check that the core index exceeds the cladding "
              "and substrate and that the cross-section is above cut-off");
    I18n::reg("mds_status_sweep", "掃引完了 (%1 秒)", "Sweep finished (%1 s)");
    I18n::reg("mds_status_corner", "コーナー計算完了 (%1 秒)",
              "Corner analysis finished (%1 s)");
    I18n::reg("mds_sec_modes", "固有モード / Eigenmodes", "Eigenmodes");
    I18n::reg("mds_col_mode", "モード", "Mode");
    I18n::reg("mds_col_ng", "ng (群)", "ng (group)");
    I18n::reg("mds_col_guided", "導波", "Guided");
    I18n::reg("mds_col_loss", "伝搬損失 [dB/cm]", "Loss [dB/cm]");
    I18n::reg("mds_col_gamma", "閉込め係数 Γ", "Confinement Γ");
    I18n::reg("mds_col_aeff", "実効断面積 [µm²]", "Effective area [µm²]");
    I18n::reg("mds_guided", "導波", "Guided");
    I18n::reg("mds_loss_note",
        "▸ 伝搬損失は実屈折率のみの FDE では求まりません (側壁散乱・材料吸収は"
        "未対応) — 列は「—」のままです。",
        "▸ Propagation loss cannot be obtained from a real-index FDE solver "
        "(sidewall scattering and material absorption are not modelled) — the "
        "column stays “—”.");
    I18n::reg("mds_single_ok",
              "✓ シングルモード条件 満足 — 導波モードは基本モードの 1 本のみ",
              "✓ Single-mode condition met — only the fundamental mode is "
              "guided");
    I18n::reg("mds_single_ng",
              "⚠ マルチモード — 導波モードが %1 本 (幅を狭めると高次モードが"
              "カットオフします)",
              "⚠ Multi-mode — %1 guided modes (narrowing the core cuts off the "
              "higher-order ones)");
    I18n::reg("mds_grid_note",
        "▸ 格子 %1 × %2 nm の有限差分による離散化誤差を含みます (neff の絶対値で "
        "1e-3 程度。掃引・コーナーで使う差分量はこれより 1〜2 桁小さい)。",
        "▸ Values carry the discretisation error of the %1 × %2 nm finite-"
        "difference grid (~1e-3 on the absolute neff; the differences used by "
        "the sweep and the corner analysis are one to two orders smaller).");
    I18n::reg("mds_show_field", "🗺 モード分布 |E|² 表示", "🗺 Show |E|² mode profile");
    I18n::reg("mds_hide_field", "🗺 モード分布を隠す", "🗺 Hide |E|² mode profile");
    I18n::reg("mds_field_cap",
        "%1: neff = %2 / Γ = %3 / Aeff = %4 µm²  —  表示窓 %5 × %6 µm "
        "(横軸 x・縦軸 y、格子 %7 × %8 nm)。表の行を選ぶと切り替わります。",
        "%1: neff = %2 / Γ = %3 / Aeff = %4 µm² — window %5 × %6 µm "
        "(x horizontal, y vertical; grid %7 × %8 nm). Select a table row to "
        "switch modes.");
    I18n::reg("mds_to_source", "→ モード波源に設定", "→ Set as mode source");
    I18n::reg("mds_to_monitor", "→ モード展開モニターに登録",
              "→ Register as expansion monitor");
    I18n::reg("mds_to_schematic", "→ Schematic コンパクトモデル生成",
              "→ Generate Schematic compact model");
    I18n::reg("mds_handoff_note",
        "▸ 上の 3 つの受け渡しは受け側モデル (モード波源・モード展開モニター・"
        "Schematic) が未実装のため無効です。",
        "▸ The three hand-off buttons are disabled: the receiving models (mode "
        "source, expansion monitor, schematic) are not implemented yet.");
    I18n::reg("mds_sec_disp", "分散解析 / Dispersion", "Dispersion");
    I18n::reg("mds_sweep", "掃引", "Sweep");
    I18n::reg("mds_sweep_lambda", "波長 λ₀±3%", "Wavelength λ₀±3%");
    I18n::reg("mds_sweep_width", "幅 w₀±100nm", "Width w₀±100 nm");
    I18n::reg("mds_sweep_temp", "温度 -40〜85°C", "Temperature -40 to 85 °C");
    I18n::reg("mds_sweep_run", "▶ 掃引実行", "▶ Run sweep");
    I18n::reg("mds_sweep_note",
        "掃引カーブと下表の 4 指標 (D / Δn / dneff/dT / dneff/dw) を差分ソルブで"
        "実計算します。",
        "The curve and the four metrics below (D / Δn / dneff/dT / dneff/dw) "
        "are all computed from finite-difference solves.");
    I18n::reg("mds_col_metric", "指標", "Metric");
    I18n::reg("mds_col_value", "値", "Value");
    I18n::reg("mds_col_use", "用途", "Use");
    I18n::reg("mds_notcalc", "未計算", "not computed");
    I18n::reg("mds_d1", "群速度分散 D", "Group-velocity dispersion D");
    I18n::reg("mds_d1_use", "高速変調の波形歪み評価",
              "Waveform distortion at high modulation rates");
    I18n::reg("mds_d2", "複屈折 Δn (TE-TM)", "Birefringence Δn (TE-TM)");
    I18n::reg("mds_d2_use", "偏波依存性", "Polarization dependence");
    I18n::reg("mds_d3", "dneff/dT", "dneff/dT");
    I18n::reg("mds_d3_use", "熱チューニング設計 (ヒーター)",
              "Thermal tuning design (heaters)");
    I18n::reg("mds_d4", "dneff/dw (感度)", "dneff/dw (sensitivity)");
    I18n::reg("mds_d4_use", "製造ばらつき→コーナー解析へ",
              "Fab variation → corner analysis");
    I18n::reg("mds_disp_nodndt",
        "▸ dneff/dT: 選択した材料に公刊 dn/dT が無いため未算出 (温度掃引も"
        "同じ理由で省略)。",
        "▸ dneff/dT: none of the selected materials has a published dn/dT, so "
        "it is not computed (the temperature sweep is skipped for the same "
        "reason).");
    I18n::reg("mds_disp_skipped",
              "▸ 掃引点のうち %1 点は材料の λ 有効範囲外または非導波のため除外。",
              "▸ %1 sweep point(s) were dropped (outside the material λ range "
              "or not guided).");
    I18n::reg("mds_sec_bend", "曲げ導波路 / Bent waveguide", "Bent waveguide");
    I18n::reg("mds_col_radius", "曲げ半径", "Bend radius");
    I18n::reg("mds_col_rad_loss", "放射損失 [dB/90°]", "Radiation loss [dB/90°]");
    I18n::reg("mds_col_mismatch", "モード不整合 [dB/接続]",
              "Mode mismatch [dB/junction]");
    I18n::reg("mds_col_caustic", "放射カウスティック x_c [µm]",
              "Radiation caustic x_c [µm]");
    I18n::reg("mds_bend_hint",
        "共形変換 n_eq(x) = n(x)·(1+x/R) (Heiblum-Harris 1975) で曲げ導波路を"
        "等価直線導波路に置き換え、その断面を FDE で解いて直線モードとの"
        "重なり積分から「直線⇄曲げ接続のモード不整合損」を実計算します。"
        "カウスティック位置 x_c = R·(neff/n_clad − 1) も同じ変換から求めます。",
        "The bend is replaced by an equivalent straight guide through the "
        "conformal transformation n_eq(x) = n(x)·(1+x/R) (Heiblum-Harris "
        "1975). Solving that cross-section with the FDE and overlapping it "
        "with the straight mode gives the straight-to-bend mode mismatch "
        "loss. The caustic position x_c = R·(neff/n_clad − 1) comes from the "
        "same transformation.");
    I18n::reg("mds_bend_run", "▶ 曲げ計算", "▶ Compute bends");
    I18n::reg("mds_bend_idle",
        "▸ 未計算 — 「▶ 曲げ計算」で直線モード + 4 半径の等価断面を解きます "
        "(計 5 回のソルブ)。",
        "▸ Not computed — press “▶ Compute bends” to solve the straight mode "
        "plus the equivalent cross-sections of four radii (five solves).");
    I18n::reg("mds_bend_status", "曲げ計算完了 (%1 秒)",
              "Bend analysis finished (%1 s)");
    I18n::reg("mds_bend_note",
        "▸ 放射損失そのものは求まりません: 等価屈折率は外周側で単調に増えるため"
        "曲げモードは本質的に漏れモード (複素 neff) で、実対称・Dirichlet 窓の"
        "FDE では虚部が出ないためです (透過境界での漏れモード解析が別途必要)。"
        "列は「未計算」のままにします。",
        "▸ The radiation loss itself is not obtained: the equivalent index "
        "grows monotonically outwards, so the bend mode is inherently leaky "
        "(complex neff) and a real-symmetric FDE with Dirichlet walls cannot "
        "produce the imaginary part (a leaky-mode solve with transparent "
        "boundaries is required). That column stays “not computed”.");
    I18n::reg("mds_bend_ratio",
        "▸ 窓端での |x|/R は最小半径の行で %1 (半径が大きいほど小さい) — "
        "共形変換の 1 次近似は |x| ≪ R が前提なので、0.1 を超える行は"
        "近似誤差を含む目安として読んでください。",
        "▸ |x|/R at the window edge is %1 for the smallest radius (it shrinks "
        "as R grows). The first-order conformal transformation assumes "
        "|x| ≪ R, so rows above 0.1 carry approximation error and should be "
        "read as indicative.");
    I18n::reg("mds_bend_nomode",
        "この半径では等価断面の導波モードが求まりませんでした (窓外へ漏れる)",
        "no guided mode of the equivalent cross-section at this radius (it "
        "leaks out of the window)");
    I18n::reg("mds_euler",
        "▸ 一定曲率の曲げのみを扱います。オイラー曲線 (クロソイド) は曲率が"
        "連続なので接続部の不整合損を下げられますが、その形状は本タブでは"
        "解いていません。",
        "▸ Only constant-curvature bends are handled. Euler (clothoid) bends "
        "have continuous curvature and lower junction mismatch loss, but that "
        "shape is not solved in this tab.");
    I18n::reg("mds_sec_corner", "プロセスコーナー / Corner analysis",
              "Corner analysis");
    I18n::reg("mds_corner_hint",
        "幅±10nm・高さ±5nm の4コーナーを実際に解いて neff/ng を比較し、"
        "リング共振の波長シフト Δλ = λ·Δneff/ng を求めます。",
        "Solves the four ±10 nm width / ±5 nm height corners, compares neff/ng "
        "and reports the ring resonance shift Δλ = λ·Δneff/ng.");
    I18n::reg("mds_corner_run", "▶ コーナー計算", "▶ Run corner analysis");
    I18n::reg("mds_col_corner", "コーナー", "Corner");
    I18n::reg("mds_col_dlambda", "Δλ共振 = λ·Δneff/ng", "Δλ resonance = λ·Δneff/ng");
    I18n::reg("mds_corner_nom", "公称", "Nominal");
    I18n::reg("mds_corner_pp", "幅+10 / 高+5", "W+10 / H+5");
    I18n::reg("mds_corner_pm", "幅+10 / 高-5", "W+10 / H-5");
    I18n::reg("mds_corner_mp", "幅-10 / 高+5", "W-10 / H+5");
    I18n::reg("mds_corner_mm", "幅-10 / 高-5", "W-10 / H-5");
    I18n::reg("mds_corner_idle",
        "▸ 未計算 — 「▶ コーナー計算」で公称 + 4 コーナーを実際に解きます "
        "(1 コーナーあたり ng 用の λ 差分を含む 3 回のソルブ)。",
        "▸ Not computed — press “▶ Run corner analysis” to solve the nominal "
        "point plus the four corners (three solves each, including the λ "
        "difference for ng).");
    I18n::reg("mds_corner_spread",
        "▸ コーナー間のリング共振波長ずれは最大 %1 nm — 熱チューナ要否の定量根拠。",
        "▸ The ring resonance spreads by up to %1 nm across the corners — this "
        "quantifies whether a thermal tuner is mandatory.");
    return true;
}();

namespace opt = ofd::optics;
using ofd::mds::Setup;

const double kC0 = 2.99792458e8;    // 真空中の光速 [m/s]

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

// ── 計算ヘルパー (Qt ウィジェットに触らない — ワーカースレッドから呼ぶ) ─────

// 格子間隔と窓余白の自動決定。コア断面を最低 22 分割し、総格子点数が 5 万点を
// 超えないよう粗くする (GUI の応答性確保。実測 0.7〜2 秒/ソルブ)。
void chooseGrid(double w_um, double h_um, double &dx_um, double &margin)
{
    margin = std::min(1.6, std::max(0.5, 0.8 / std::max(w_um, h_um)));
    double dx = std::min(w_um, h_um) / 22.0;
    dx = std::max(0.005, std::min(0.030, dx));
    for (int i = 0; i < 12; ++i) {
        const double wx = w_um + 2.0 * std::max(margin * w_um, 0.6);
        const double wy = h_um + 2.0 * std::max(margin * h_um, 0.6);
        if ((wx / dx) * (wy / dx) <= 50000.0) break;
        dx *= 1.25;
    }
    dx_um = dx;
}

// 指定条件で n(λ,T) を引き直し断面を組んで解く。
// 材料が λ の有効範囲外なら空を返す (外挿した「それらしい値」を出さない)。
std::vector<opt::ModeResult> solveAt(const Setup &s, double w_um, double h_um,
                                     double lam_um, double temp_C, bool te,
                                     int nModes, opt::CrossSection *csOut = nullptr)
{
    double nc = 0.0, ncl = 0.0, nsub = 0.0;
    bool ta = false;
    if (!opt::refractiveIndexAt(s.coreId.c_str(), lam_um, temp_C, nc, ta)) return {};
    if (!opt::refractiveIndexAt(s.cladId.c_str(), lam_um, temp_C, ncl, ta)) return {};
    if (!opt::refractiveIndexAt(s.subId.c_str(),  lam_um, temp_C, nsub, ta)) return {};
    opt::CrossSection cs = opt::makeRectangularCore(
        w_um, h_um, s.slab_um, nc, ncl, nsub, s.dx_um, s.marginRatio);
    if (cs.nx <= 0) return {};
    opt::SolveOptions o;
    o.pol = te ? opt::Polarization::SemiVecTE : opt::Polarization::SemiVecTM;
    o.modes = nModes;
    std::vector<opt::ModeResult> r = opt::solveModes(cs, lam_um, o);
    if (csOut) *csOut = cs;
    return r;
}

// 基本モードの neff だけ (差分計算用)。求まらなければ NaN
double neff0(const Setup &s, double w_um, double h_um, double lam_um,
             double temp_C, bool te, std::atomic<int> *prog)
{
    const std::vector<opt::ModeResult> r =
        solveAt(s, w_um, h_um, lam_um, temp_C, te, 1);
    if (prog) ++(*prog);
    return r.empty() ? std::numeric_limits<double>::quiet_NaN() : r[0].neff;
}

// 選択した 3 材料のいずれかに公刊 dn/dT があるか
bool anyDnDt(const Setup &s)
{
    for (const std::string *id : { &s.coreId, &s.cladId, &s.subId }) {
        const opt::MaterialInfo *mi = opt::findMaterial(id->c_str());
        if (mi && mi->hasDnDt) return true;
    }
    return false;
}

// ng = neff − λ·dneff/dλ (λ(1±dl) の中心差分)
bool groupIndex(double n0, double nlo, double nhi, double lam_um, double dl,
                double &ng)
{
    if (!std::isfinite(n0) || !std::isfinite(nlo) || !std::isfinite(nhi))
        return false;
    ng = n0 - lam_um * (nhi - nlo) / (2.0 * lam_um * dl);
    return true;
}

// D = −(λ/c)·d²neff/dλ² を ps/(nm·km) で
bool dispersionD(double n0, double nlo, double nhi, double lam_um, double dl,
                 double &D)
{
    if (!std::isfinite(n0) || !std::isfinite(nlo) || !std::isfinite(nhi))
        return false;
    const double h_um = lam_um * dl;
    const double d2_per_um2 = (nhi - 2.0 * n0 + nlo) / (h_um * h_um);
    // 1/µm² → 1/m² (1e12)、s/m² → ps/(nm·km) (1e6)
    D = -(lam_um * 1e-6 / kC0) * (d2_per_um2 * 1e12) * 1e6;
    return true;
}

// |E|² を表示用に上下反転 (FieldHeatmap は先頭行を上端に描く。
// 断面は iy=0 が −y 側なので、そのままだと基板が上に来る)
QVector<double> flipRows(const std::vector<double> &v, int nx, int ny)
{
    QVector<double> out;
    if (nx <= 0 || ny <= 0 ||
        v.size() < static_cast<std::size_t>(nx) * ny) return out;
    out.resize(nx * ny);
    for (int iy = 0; iy < ny; ++iy)
        for (int ix = 0; ix < nx; ++ix)
            out[(ny - 1 - iy) * nx + ix] = v[static_cast<std::size_t>(iy) * nx + ix];
    return out;
}

double nowSec()
{
    return std::chrono::duration<double>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

// ── ワーカー本体 ────────────────────────────────────────────────────────────
struct ModeJob {
    QVector<ofd::mds::ModeRow> rows;
    double secs = 0.0;
    double dx_nm = 0.0;
};

// λ0 と λ0(1±1%) の 3 回のソルブでモード表 (neff/Γ/Aeff/ng/|E|²) を作る
void computeModes(const Setup &s, ModeJob &job, std::atomic<int> *prog)
{
    const double t0 = nowSec();
    const double dl = 0.01;
    opt::CrossSection cs;
    const std::vector<opt::ModeResult> base =
        solveAt(s, s.w_um, s.h_um, s.lambda_um, s.temp_C, s.te, 4, &cs);
    ++(*prog);
    std::vector<opt::ModeResult> lo, hi;
    if (!base.empty()) {
        const int nm = static_cast<int>(base.size());
        lo = solveAt(s, s.w_um, s.h_um, s.lambda_um * (1.0 - dl), s.temp_C, s.te, nm);
        ++(*prog);
        hi = solveAt(s, s.w_um, s.h_um, s.lambda_um * (1.0 + dl), s.temp_C, s.te, nm);
        ++(*prog);
    } else {
        prog->store(3);
    }
    for (std::size_t i = 0; i < base.size(); ++i) {
        ofd::mds::ModeRow row;
        row.name = QStringLiteral("%1%2")
                       .arg(s.te ? "TE" : "TM").arg(static_cast<int>(i));
        row.neff = base[i].neff;
        row.gamma = base[i].gamma;
        row.aeff_um2 = base[i].aeff_um2;
        if (i < lo.size() && i < hi.size())
            row.hasNg = groupIndex(base[i].neff, lo[i].neff, hi[i].neff,
                                   s.lambda_um, dl, row.ng);
        row.nx = cs.nx;
        row.ny = cs.ny;
        row.dx_um = cs.dx_um;
        row.dy_um = cs.dy_um;
        row.intensity = flipRows(base[i].intensity, cs.nx, cs.ny);
        job.rows.append(row);
    }
    job.dx_nm = cs.dx_um * 1000.0;
    job.secs = nowSec() - t0;
}

struct SweepJob {
    ofd::mds::Dispersion d;
    int    skipped = 0;
    bool   noDnDt = false;
    double secs = 0.0;
};

// 掃引の刻み数 (GUI の進捗表示と歩調を合わせるためここで一元管理)
const int kSweepPts = 9;

int sweepSteps(const Setup &s, int kind)
{
    const bool dndt = anyDnDt(s);
    int n = 3 + 1 + 2;                      // D(3) + 複屈折(1) + dneff/dw(2)
    if (dndt) n += 2;                       // dneff/dT
    if (!(kind == 2 && !dndt)) n += kSweepPts;
    return n;
}

// kind: 0 = 波長 / 1 = 幅 / 2 = 温度
void computeSweep(const Setup &s, int kind, SweepJob &job, std::atomic<int> *prog)
{
    const double t0 = nowSec();
    const double dl = 0.01;
    job.noDnDt = !anyDnDt(s);

    // ── 掃引カーブ ──
    // 温度掃引は dn/dT が無いと定義上まったく動かないので実行しない
    if (!(kind == 2 && job.noDnDt)) {
        for (int i = 0; i < kSweepPts; ++i) {
            const double t = (i - (kSweepPts - 1) / 2.0)
                             / ((kSweepPts - 1) / 2.0);   // −1..+1
            double lam = s.lambda_um, w = s.w_um, T = s.temp_C, x = 0.0;
            if (kind == 0) {
                lam = s.lambda_um * (1.0 + 0.03 * t);
                x = lam * 1000.0;
            } else if (kind == 1) {
                w = std::max(0.05, s.w_um + 0.1 * t);
                x = w * 1000.0;
            } else {
                T = 22.5 + 62.5 * t;                      // −40〜85 °C
                x = T;
            }
            const double n = neff0(s, w, s.h_um, lam, T, s.te, prog);
            if (std::isfinite(n)) job.d.curve.append(QPointF(x, n));
            else                  ++job.skipped;
        }
    }

    // ── 指標 ──
    const double n0  = neff0(s, s.w_um, s.h_um, s.lambda_um, s.temp_C, s.te, prog);
    const double nlo = neff0(s, s.w_um, s.h_um, s.lambda_um * (1.0 - dl),
                             s.temp_C, s.te, prog);
    const double nhi = neff0(s, s.w_um, s.h_um, s.lambda_um * (1.0 + dl),
                             s.temp_C, s.te, prog);
    job.d.hasD = dispersionD(n0, nlo, nhi, s.lambda_um, dl, job.d.D_ps_nm_km);

    const double nOther = neff0(s, s.w_um, s.h_um, s.lambda_um, s.temp_C,
                                !s.te, prog);
    if (std::isfinite(n0) && std::isfinite(nOther)) {
        job.d.biref = s.te ? (n0 - nOther) : (nOther - n0);
        job.d.hasBiref = true;
    }

    if (!job.noDnDt) {
        const double dT = 10.0;
        const double nTm = neff0(s, s.w_um, s.h_um, s.lambda_um,
                                 s.temp_C - dT, s.te, prog);
        const double nTp = neff0(s, s.w_um, s.h_um, s.lambda_um,
                                 s.temp_C + dT, s.te, prog);
        if (std::isfinite(nTm) && std::isfinite(nTp)) {
            job.d.dneff_dT = (nTp - nTm) / (2.0 * dT);
            job.d.hasDnDt = true;
        }
    }

    const double dw = 0.010;   // ±10 nm
    const double nwm = neff0(s, std::max(0.05, s.w_um - dw), s.h_um,
                             s.lambda_um, s.temp_C, s.te, prog);
    const double nwp = neff0(s, s.w_um + dw, s.h_um, s.lambda_um,
                             s.temp_C, s.te, prog);
    if (std::isfinite(nwm) && std::isfinite(nwp)) {
        job.d.dneff_dw = (nwp - nwm) / (2.0 * dw * 1000.0);   // [1/nm]
        job.d.hasDnDw = true;
    }
    job.secs = nowSec() - t0;
}

struct CornerJob {
    QVector<ofd::mds::CornerRow> rows;
    double secs = 0.0;
};

// 公称 + 4 コーナー。各点で λ 差分 (±1%) から ng も出す
const struct { const char *key; double dw_um, dh_um; } kCorners[] = {
    { "mds_corner_nom",  0.000,  0.000 },
    { "mds_corner_pp",   0.010,  0.005 },
    { "mds_corner_pm",   0.010, -0.005 },
    { "mds_corner_mp",  -0.010,  0.005 },
    { "mds_corner_mm",  -0.010, -0.005 },
};
const int kNCorners = static_cast<int>(sizeof(kCorners) / sizeof(kCorners[0]));

void computeCorners(const Setup &s, CornerJob &job, std::atomic<int> *prog)
{
    const double t0 = nowSec();
    const double dl = 0.01;
    for (int i = 0; i < kNCorners; ++i) {
        const double w = std::max(0.05, s.w_um + kCorners[i].dw_um);
        const double h = std::max(0.02, s.h_um + kCorners[i].dh_um);
        ofd::mds::CornerRow row;
        const double n0 = neff0(s, w, h, s.lambda_um, s.temp_C, s.te, prog);
        if (std::isfinite(n0)) {
            row.ok = true;
            row.neff = n0;
            const double nlo = neff0(s, w, h, s.lambda_um * (1.0 - dl),
                                     s.temp_C, s.te, prog);
            const double nhi = neff0(s, w, h, s.lambda_um * (1.0 + dl),
                                     s.temp_C, s.te, prog);
            row.hasNg = groupIndex(n0, nlo, nhi, s.lambda_um, dl, row.ng);
        } else {
            prog->fetch_add(2);
        }
        job.rows.append(row);
    }
    // Δλ = λ·Δneff/ng (共振次数一定の下での共振波長シフト)
    if (!job.rows.isEmpty() && job.rows[0].ok) {
        const double nNom = job.rows[0].neff;
        for (ofd::mds::CornerRow &r : job.rows) {
            if (!r.ok || !r.hasNg || r.ng == 0.0) continue;
            r.dlambda_nm = s.lambda_um * 1000.0 * (r.neff - nNom) / r.ng;
            r.hasDl = true;
        }
    }
    job.secs = nowSec() - t0;
}

// ── 曲げ導波路 (共形変換 + 重なり積分) ──────────────────────────────────────
struct BendJob {
    QVector<ofd::mds::BendRow> rows;
    bool   haveStraight = false;
    double ratio = 0.0;        // 窓端での |x|/R (最小半径での値 = 最悪値)
    double secs = 0.0;
};

// 評価する曲げ半径 [µm] (シリコンフォトニクスで実用される範囲)
const double kBendRadii[] = { 3.0, 5.0, 10.0, 20.0 };
const int kNBend = static_cast<int>(sizeof(kBendRadii) / sizeof(kBendRadii[0]));

// 直線モードを 1 回、各半径の等価断面を 1 回ずつ解く (計 1 + kNBend 回)
void computeBend(const Setup &s, BendJob &job, std::atomic<int> *prog)
{
    const double t0 = nowSec();
    double nClad = 0.0;
    bool ta = false;
    opt::refractiveIndexAt(s.cladId.c_str(), s.lambda_um, s.temp_C, nClad, ta);

    opt::CrossSection cs;
    const std::vector<opt::ModeResult> straight =
        solveAt(s, s.w_um, s.h_um, s.lambda_um, s.temp_C, s.te, 1, &cs);
    ++(*prog);
    if (straight.empty() || cs.nx <= 0) {
        prog->store(1 + kNBend);
        job.secs = nowSec() - t0;
        return;
    }
    job.haveStraight = true;

    opt::SolveOptions o;
    o.pol = s.te ? opt::Polarization::SemiVecTE : opt::Polarization::SemiVecTM;
    o.modes = 1;
    for (int i = 0; i < kNBend; ++i) {
        ofd::mds::BendRow row;
        row.R_um = kBendRadii[i];
        row.ratio = opt::conformalRatio(cs, row.R_um);
        job.ratio = std::max(job.ratio, row.ratio);
        // 放射カウスティック x_c = R(neff/n_clad − 1) は直線モードの neff から
        row.caustic_um = opt::radiationCaustic(row.R_um, straight[0].neff, nClad);
        row.hasCaustic = (row.caustic_um > 0.0);

        const opt::CrossSection bent = opt::bendEquivalent(cs, row.R_um);
        const std::vector<opt::ModeResult> bm = opt::solveModes(bent, s.lambda_um, o);
        ++(*prog);
        if (!bm.empty()) {
            const double eta = opt::overlapEfficiency(straight[0].field,
                                                      bm[0].field);
            if (eta > 0.0) {
                row.ok = true;
                row.neffBend = bm[0].neff;
                row.mismatchDb = opt::mismatchLossDb(eta);
            }
        }
        job.rows.append(row);
    }
    job.secs = nowSec() - t0;
}

} // namespace

// ── construction ────────────────────────────────────────────────────────────
ModeSolverTab::ModeSolverTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    m_progress = std::make_shared<std::atomic<int>>(0);

    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // ── ソルバ設定 ──────────────────────────────────────────────────────────
    auto *s1 = new SectionBox(I18n::tr("mds_sec_solver"), body);
    auto *hint = new QLabel(I18n::tr("mds_hint"), s1);
    hint->setWordWrap(true);
    s1->vbox()->addWidget(hint);
    auto *solverNote = new QLabel(I18n::tr("mds_solver_note"), s1);
    solverNote->setWordWrap(true);
    solverNote->setStyleSheet("color:palette(mid); font-size:11px;");
    s1->vbox()->addWidget(solverNote);

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

    // 材料 (コア / クラッド / 基板) — MaterialDispersion の内蔵テーブルから
    auto *mat = new QHBoxLayout();
    const std::vector<opt::MaterialInfo> &mats = opt::materials();
    auto *matCoreLbl = new QLabel(I18n::tr("mds_mat_core_l"), s1);
    auto *matCladLbl = new QLabel(I18n::tr("mds_mat_clad_l"), s1);
    auto *matSubLbl  = new QLabel(I18n::tr("mds_mat_sub_l"), s1);
    m_matCore = new QComboBox(s1);
    m_matClad = new QComboBox(s1);
    m_matSub  = new QComboBox(s1);
    for (QComboBox *c : { m_matCore, m_matClad, m_matSub })
        for (const opt::MaterialInfo &mi : mats)
            c->addItem(QString::fromUtf8(mi.label), QString::fromUtf8(mi.id));
    const auto selectId = [](QComboBox *c, const char *id) {
        const int i = c->findData(QString::fromUtf8(id));
        if (i >= 0) c->setCurrentIndex(i);
    };
    selectId(m_matCore, "Si");        // 既定は SOI ストリップ (Si / SiO2)
    selectId(m_matClad, "SiO2");
    selectId(m_matSub,  "SiO2");
    mat->addWidget(matCoreLbl);  mat->addWidget(m_matCore);
    mat->addWidget(matCladLbl);  mat->addWidget(m_matClad);
    mat->addWidget(matSubLbl);   mat->addWidget(m_matSub);
    mat->addStretch(1);
    s1->form()->addRow(I18n::tr("mds_mat"), mat);
    auto *matNote = new QLabel(I18n::tr("mds_mat_note"), s1);
    matNote->setWordWrap(true);
    matNote->setStyleSheet("color:palette(mid); font-size:11px;");
    s1->vbox()->addWidget(matNote);

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

    // 現在の入力での屈折率 (材料・λ・T が計算へ入っていることを画面で示す)
    m_indexLabel = new QLabel(s1);
    m_indexLabel->setWordWrap(true);
    s1->vbox()->addWidget(m_indexLabel);

    auto *runRow = new QHBoxLayout();
    m_btnRun = new QPushButton(I18n::tr("mds_run"), s1);
    runRow->addWidget(m_btnRun);
    m_status = new QLabel(I18n::tr("mds_status_idle"), s1);
    m_status->setWordWrap(true);
    runRow->addWidget(m_status, 1);
    s1->vbox()->addLayout(runRow);
    v->addWidget(s1);

    // ── 固有モード ──────────────────────────────────────────────────────────
    auto *s2 = new SectionBox(I18n::tr("mds_sec_modes"), body);
    m_modeTable = makeTable(s2, { I18n::tr("mds_col_mode"),
        QStringLiteral("neff"), I18n::tr("mds_col_ng"),
        I18n::tr("mds_col_guided"), I18n::tr("mds_col_gamma"),
        I18n::tr("mds_col_aeff"), I18n::tr("mds_col_loss") });
    m_modeTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_modeTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    s2->vbox()->addWidget(m_modeTable);
    m_singleModeBadge = new QLabel(s2);
    m_singleModeBadge->setWordWrap(true);
    s2->vbox()->addWidget(m_singleModeBadge);
    m_gridNote = new QLabel(s2);
    m_gridNote->setWordWrap(true);
    m_gridNote->setStyleSheet("color:palette(mid); font-size:11px;");
    s2->vbox()->addWidget(m_gridNote);
    auto *lossNote = new QLabel(I18n::tr("mds_loss_note"), s2);
    lossNote->setWordWrap(true);
    lossNote->setStyleSheet("color:palette(mid); font-size:11px;");
    s2->vbox()->addWidget(lossNote);

    auto *btnRow = new QHBoxLayout();
    m_btnField = new QPushButton(I18n::tr("mds_show_field"), s2);
    m_btnField->setEnabled(false);
    btnRow->addWidget(m_btnField);
    for (const char *key : { "mds_to_source", "mds_to_monitor",
                             "mds_to_schematic" }) {
        auto *b = new QPushButton(I18n::tr(key), s2);
        markNotImplemented(b);   // 受け側モデルが未実装
        btnRow->addWidget(b);
    }
    btnRow->addStretch(1);
    s2->vbox()->addLayout(btnRow);
    auto *handoff = new QLabel(I18n::tr("mds_handoff_note"), s2);
    handoff->setWordWrap(true);
    handoff->setStyleSheet("font-size:11px; color:#B8860B;");
    s2->vbox()->addWidget(handoff);

    m_field = new FieldHeatmap(s2);
    m_field->setVisible(false);
    s2->vbox()->addWidget(m_field);
    m_fieldCaption = new QLabel(s2);
    m_fieldCaption->setWordWrap(true);
    m_fieldCaption->setVisible(false);
    m_fieldCaption->setStyleSheet("font-size:11px;");
    s2->vbox()->addWidget(m_fieldCaption);
    v->addWidget(s2);

    // ── 分散解析 ────────────────────────────────────────────────────────────
    auto *s3 = new SectionBox(I18n::tr("mds_sec_disp"), body);
    auto *swRow = new QHBoxLayout();
    m_sweepSel = new QComboBox(s3);
    m_sweepSel->addItem(I18n::tr("mds_sweep_lambda"));
    m_sweepSel->addItem(I18n::tr("mds_sweep_width"));
    m_sweepSel->addItem(I18n::tr("mds_sweep_temp"));
    swRow->addWidget(new QLabel(I18n::tr("mds_sweep"), s3));
    swRow->addWidget(m_sweepSel);
    m_btnSweep = new QPushButton(I18n::tr("mds_sweep_run"), s3);
    swRow->addWidget(m_btnSweep);
    swRow->addStretch(1);
    s3->vbox()->addLayout(swRow);
    auto *sweepNote = new QLabel(I18n::tr("mds_sweep_note"), s3);
    sweepNote->setWordWrap(true);
    sweepNote->setStyleSheet("color:palette(mid); font-size:11px;");
    s3->vbox()->addWidget(sweepNote);
    m_dispPlot = new MiniPlot(s3);
    m_dispPlot->setLabels(QStringLiteral("λ [nm]"), QStringLiteral("neff"));
    m_dispPlot->setMinimumHeight(120);
    s3->vbox()->addWidget(m_dispPlot);
    m_dispTable = makeTable(s3, { I18n::tr("mds_col_metric"),
        I18n::tr("mds_col_value"), I18n::tr("mds_col_use") });
    const char *kDisp[][2] = {
        { "mds_d1", "mds_d1_use" },
        { "mds_d2", "mds_d2_use" },
        { "mds_d3", "mds_d3_use" },
        { "mds_d4", "mds_d4_use" },
    };
    for (const auto &row : kDisp) {
        const int r = m_dispTable->rowCount();
        m_dispTable->insertRow(r);
        m_dispTable->setItem(r, 0, roItem(I18n::tr(row[0])));
        m_dispTable->setItem(r, 1, roItem(I18n::tr("mds_notcalc")));
        m_dispTable->setItem(r, 2, roItem(I18n::tr(row[1])));
    }
    fitTable(m_dispTable);
    s3->vbox()->addWidget(m_dispTable);
    m_dispNote = new QLabel(s3);
    m_dispNote->setWordWrap(true);
    m_dispNote->setStyleSheet("font-size:11px; color:#B8860B;");
    m_dispNote->setVisible(false);
    s3->vbox()->addWidget(m_dispNote);
    v->addWidget(s3);

    // ── 曲げ導波路 (共形変換 + 重なり積分による実計算) ──────────────────────
    auto *s4 = new SectionBox(I18n::tr("mds_sec_bend"), body);
    auto *bendHint = new QLabel(I18n::tr("mds_bend_hint"), s4);
    bendHint->setWordWrap(true);
    s4->vbox()->addWidget(bendHint);
    auto *bendRow = new QHBoxLayout();
    m_btnBend = new QPushButton(I18n::tr("mds_bend_run"), s4);
    bendRow->addWidget(m_btnBend);
    bendRow->addStretch(1);
    s4->vbox()->addLayout(bendRow);
    m_bendTable = makeTable(s4, { I18n::tr("mds_col_radius"),
        I18n::tr("mds_col_mismatch"), I18n::tr("mds_col_caustic"),
        I18n::tr("mds_col_rad_loss") });
    s4->vbox()->addWidget(m_bendTable);
    m_bendNote = new QLabel(I18n::tr("mds_bend_idle"), s4);
    m_bendNote->setWordWrap(true);
    s4->vbox()->addWidget(m_bendNote);
    // 放射損失が求まらない理由は常に表示する (「未計算」の根拠)
    auto *bendLimit = new QLabel(I18n::tr("mds_bend_note"), s4);
    bendLimit->setWordWrap(true);
    bendLimit->setStyleSheet("font-size:11px; color:#B8860B;");
    s4->vbox()->addWidget(bendLimit);
    auto *euler = new QLabel(I18n::tr("mds_euler"), s4);
    euler->setWordWrap(true);
    euler->setStyleSheet("font-size:11px; color:palette(mid);");
    s4->vbox()->addWidget(euler);
    v->addWidget(s4);

    // ── プロセスコーナー ────────────────────────────────────────────────────
    auto *s5 = new SectionBox(I18n::tr("mds_sec_corner"), body);
    auto *cornerHint = new QLabel(I18n::tr("mds_corner_hint"), s5);
    cornerHint->setWordWrap(true);
    s5->vbox()->addWidget(cornerHint);
    auto *cornerRow = new QHBoxLayout();
    m_btnCorner = new QPushButton(I18n::tr("mds_corner_run"), s5);
    cornerRow->addWidget(m_btnCorner);
    cornerRow->addStretch(1);
    s5->vbox()->addLayout(cornerRow);
    m_cornerTable = makeTable(s5, { I18n::tr("mds_col_corner"),
        QStringLiteral("neff"), I18n::tr("mds_col_ng"),
        I18n::tr("mds_col_dlambda") });
    s5->vbox()->addWidget(m_cornerTable);
    m_cornerNote = new QLabel(I18n::tr("mds_corner_idle"), s5);
    m_cornerNote->setWordWrap(true);
    s5->vbox()->addWidget(m_cornerNote);
    v->addWidget(s5);
    v->addStretch(1);

    // 計算中に無効化する入力群
    m_inputs = { m_shape, m_width, m_height, m_slab, m_matCore, m_matClad,
                 m_matSub, m_lambda, m_temp, m_pol, m_sweepSel };

    // ── 接続 ────────────────────────────────────────────────────────────────
    connect(m_shape, &QComboBox::currentIndexChanged, this, [this](int i) {
        m_slab->setEnabled(i == 1);
        m_slab->setValue(i == 1 ? 90 : 0);
        updateIndexLabel();
    });
    const auto onSpin = qOverload<double>(&QDoubleSpinBox::valueChanged);
    connect(m_width,  onSpin, this, [this](double) { updateIndexLabel(); });
    connect(m_height, onSpin, this, [this](double) { updateIndexLabel(); });
    connect(m_slab,   onSpin, this, [this](double) { updateIndexLabel(); });
    connect(m_lambda, onSpin, this, [this](double) { updateIndexLabel(); });
    connect(m_temp,   onSpin, this, [this](double) { updateIndexLabel(); });
    for (QComboBox *c : { m_matCore, m_matClad, m_matSub, m_pol })
        connect(c, &QComboBox::currentIndexChanged, this,
                [this](int) { updateIndexLabel(); });

    connect(m_btnRun,    &QPushButton::clicked, this, &ModeSolverTab::runModes);
    connect(m_btnSweep,  &QPushButton::clicked, this, &ModeSolverTab::runSweep);
    connect(m_btnCorner, &QPushButton::clicked, this, &ModeSolverTab::runCorners);
    connect(m_btnBend,   &QPushButton::clicked, this, &ModeSolverTab::runBend);
    connect(m_btnField, &QPushButton::clicked, this, [this] {
        const bool show = !m_field->isVisible();
        m_field->setVisible(show);
        m_fieldCaption->setVisible(show);
        m_btnField->setText(I18n::tr(show ? "mds_hide_field" : "mds_show_field"));
        updateFieldView();
    });
    connect(m_modeTable, &QTableWidget::itemSelectionChanged, this,
            &ModeSolverTab::updateFieldView);

    // 進捗ポーリング (ワーカーからウィジェットへ触らない)
    m_poll = new QTimer(this);
    m_poll->setInterval(150);
    connect(m_poll, &QTimer::timeout, this, [this] {
        if (!m_busy) return;
        m_status->setText(m_busyLabel.arg(m_progress->load()).arg(m_steps));
    });

    updateIndexLabel();
    setWidget(body);
    setWidgetResizable(true);
}

// ── 入力 → 計算条件 ─────────────────────────────────────────────────────────
bool ModeSolverTab::buildSetup(mds::Setup &s, QString &err) const
{
    s.w_um      = m_width->value() / 1000.0;
    s.h_um      = m_height->value() / 1000.0;
    s.slab_um   = (m_shape->currentIndex() == 1) ? m_slab->value() / 1000.0 : 0.0;
    s.lambda_um = m_lambda->value() / 1000.0;
    s.temp_C    = m_temp->value();
    s.te        = (m_pol->currentIndex() == 0);
    s.coreId    = m_matCore->currentData().toString().toStdString();
    s.cladId    = m_matClad->currentData().toString().toStdString();
    s.subId     = m_matSub->currentData().toString().toStdString();

    if (s.slab_um > 0.0 && s.slab_um >= s.h_um) {
        err = I18n::tr("mds_err_rib");
        return false;
    }
    // 材料の λ 有効範囲 (外挿した「それらしい値」は出さない)
    for (const std::string *id : { &s.coreId, &s.cladId, &s.subId }) {
        double n = 0.0;
        bool ta = false;
        if (opt::refractiveIndexAt(id->c_str(), s.lambda_um, s.temp_C, n, ta))
            continue;
        const opt::MaterialInfo *mi = opt::findMaterial(id->c_str());
        err = I18n::tr("mds_err_range")
                  .arg(m_lambda->value(), 0, 'f', 0)
                  .arg(mi ? QString::fromUtf8(mi->label)
                          : QString::fromStdString(*id))
                  .arg(mi ? mi->lmin_um : 0.0, 0, 'f', 2)
                  .arg(mi ? mi->lmax_um : 0.0, 0, 'f', 2);
        return false;
    }
    chooseGrid(s.w_um, s.h_um, s.dx_um, s.marginRatio);
    return true;
}

// 入力が変わったら屈折率プレビューを更新し、既存の結果を破棄する
// (入力と食い違う古い数値を残さない)
void ModeSolverTab::updateIndexLabel()
{
    clearResults();
    mds::Setup s;
    QString err;
    const bool ok = buildSetup(s, err);
    m_btnRun->setEnabled(ok && !m_busy);
    m_btnSweep->setEnabled(ok && !m_busy);
    m_btnCorner->setEnabled(ok && !m_busy);
    m_btnBend->setEnabled(ok && !m_busy);
    if (!ok) {
        m_indexLabel->setText(err);
        m_indexLabel->setStyleSheet("color:#C62828; font-size:11px;");
        return;
    }
    double nc = 0.0, ncl = 0.0, nsub = 0.0;
    bool t1 = false, t2 = false, t3 = false;
    opt::refractiveIndexAt(s.coreId.c_str(), s.lambda_um, s.temp_C, nc, t1);
    opt::refractiveIndexAt(s.cladId.c_str(), s.lambda_um, s.temp_C, ncl, t2);
    opt::refractiveIndexAt(s.subId.c_str(),  s.lambda_um, s.temp_C, nsub, t3);
    QString text = I18n::tr("mds_n_preview")
                       .arg(m_lambda->value(), 0, 'f', 0)
                       .arg(m_temp->value(), 0, 'f', 0)
                       .arg(nc, 0, 'f', 4).arg(ncl, 0, 'f', 4)
                       .arg(nsub, 0, 'f', 4);
    QStringList missing;
    for (const std::string *id : { &s.coreId, &s.cladId, &s.subId }) {
        const opt::MaterialInfo *mi = opt::findMaterial(id->c_str());
        const QString name = QString::fromStdString(*id);
        if (mi && !mi->hasDnDt && !missing.contains(name)) missing << name;
    }
    if (!missing.isEmpty())
        text += I18n::tr("mds_n_notemp").arg(missing.join(QStringLiteral(", ")));
    m_indexLabel->setText(text);
    m_indexLabel->setStyleSheet("color:palette(mid); font-size:11px;");
}

void ModeSolverTab::clearResults()
{
    m_modes.clear();
    m_disp = mds::Dispersion();
    m_corners.clear();
    m_modeTable->setRowCount(0);
    fitTable(m_modeTable);
    m_singleModeBadge->clear();
    m_gridNote->clear();
    m_field->setVisible(false);
    m_fieldCaption->setVisible(false);
    m_fieldCaption->clear();
    m_btnField->setEnabled(false);
    m_btnField->setText(I18n::tr("mds_show_field"));
    m_dispPlot->setSeries({});
    showDispersion();
    m_cornerTable->setRowCount(0);
    fitTable(m_cornerTable);
    m_cornerNote->setText(I18n::tr("mds_corner_idle"));
    m_bends.clear();
    m_bendTable->setRowCount(0);
    fitTable(m_bendTable);
    m_bendNote->setText(I18n::tr("mds_bend_idle"));
    m_status->setText(I18n::tr("mds_status_idle"));
    m_status->setStyleSheet(QString());
}

void ModeSolverTab::setBusy(bool busy, int totalSteps, const QString &status)
{
    m_busy = busy;
    if (busy) {
        m_progress->store(0);
        m_steps = std::max(1, totalSteps);
        m_busyLabel = status;
        m_status->setText(m_busyLabel.arg(0).arg(m_steps));
        m_status->setStyleSheet(QString());
        m_poll->start();
    } else {
        m_poll->stop();
    }
    m_btnRun->setEnabled(!busy);
    m_btnSweep->setEnabled(!busy);
    m_btnCorner->setEnabled(!busy);
    m_btnBend->setEnabled(!busy);
    for (QWidget *w : m_inputs) w->setEnabled(!busy);
    if (!busy) m_slab->setEnabled(m_shape->currentIndex() == 1);
}

// ── モード計算 ──────────────────────────────────────────────────────────────
void ModeSolverTab::runModes()
{
    if (m_busy) return;
    mds::Setup s;
    QString err;
    if (!buildSetup(s, err)) return;
    // モード側の結果だけ捨てる (掃引・コーナーは同じ入力の結果なので残す。
    // 入力が変わったときは updateIndexLabel が全部捨てる)
    m_modes.clear();
    m_modeTable->setRowCount(0);
    fitTable(m_modeTable);
    m_singleModeBadge->clear();
    m_gridNote->clear();
    m_field->setVisible(false);
    m_fieldCaption->setVisible(false);
    m_btnField->setEnabled(false);
    m_btnField->setText(I18n::tr("mds_show_field"));
    setBusy(true, 3, I18n::tr("mds_status_run"));

    auto out = std::make_shared<ModeJob>();
    auto prog = m_progress;
    QThread *th = QThread::create([s, out, prog] {
        computeModes(s, *out, prog.get());
    });
    connect(th, &QThread::finished, this, [this, th, out] {
        th->deleteLater();
        setBusy(false, 0, QString());
        m_modes = out->rows;
        showModes();
        if (m_modes.isEmpty()) {
            m_status->setText(I18n::tr("mds_status_none"));
            m_status->setStyleSheet("color:#B58900;");
        } else {
            m_status->setText(I18n::tr("mds_status_done")
                                  .arg(m_modes.size())
                                  .arg(out->dx_nm, 0, 'f', 1)
                                  .arg(out->secs, 0, 'f', 1));
        }
    });
    th->start();
}

void ModeSolverTab::showModes()
{
    m_modeTable->setRowCount(0);
    for (const mds::ModeRow &m : m_modes) {
        const int r = m_modeTable->rowCount();
        m_modeTable->insertRow(r);
        m_modeTable->setItem(r, 0, roItem(m.name));
        m_modeTable->setItem(r, 1, roItem(QString::number(m.neff, 'f', 4)));
        m_modeTable->setItem(r, 2, roItem(m.hasNg
            ? QString::number(m.ng, 'f', 3) : QStringLiteral("—")));
        // solveModes は導波モードのみを返す (非導波は打ち切られる)
        m_modeTable->setItem(r, 3, roItem(I18n::tr("mds_guided")));
        m_modeTable->setItem(r, 4, roItem(QString::number(m.gamma, 'f', 3)));
        m_modeTable->setItem(r, 5, roItem(QString::number(m.aeff_um2, 'f', 4)));
        // 伝搬損失は本ソルバでは求まらない (mds_loss_note で明示)
        m_modeTable->setItem(r, 6, roItem(QStringLiteral("—")));
    }
    fitTable(m_modeTable);
    m_btnField->setEnabled(!m_modes.isEmpty());
    if (m_modes.isEmpty()) {
        m_singleModeBadge->clear();
        m_gridNote->clear();
        return;
    }
    m_gridNote->setText(I18n::tr("mds_grid_note")
                            .arg(m_modes[0].dx_um * 1000.0, 0, 'f', 1)
                            .arg(m_modes[0].dy_um * 1000.0, 0, 'f', 1));
    m_modeTable->selectRow(0);
    m_singleModeBadge->setText(m_modes.size() <= 1
        ? I18n::tr("mds_single_ok")
        : I18n::tr("mds_single_ng").arg(m_modes.size()));
    updateFieldView();
}

void ModeSolverTab::updateFieldView()
{
    if (!m_field->isVisible()) return;
    const int r = m_modeTable->currentRow();
    if (r < 0 || r >= m_modes.size()) return;
    const mds::ModeRow &m = m_modes[r];
    if (m.intensity.isEmpty()) return;
    m_field->setData(m.intensity, m.nx, m.ny);
    m_field->setTitle(QStringLiteral("%1  |E|²  (neff = %2)")
                          .arg(m.name).arg(m.neff, 0, 'f', 4));
    m_fieldCaption->setText(I18n::tr("mds_field_cap")
        .arg(m.name)
        .arg(m.neff, 0, 'f', 4)
        .arg(m.gamma, 0, 'f', 3)
        .arg(m.aeff_um2, 0, 'f', 4)
        .arg(m.nx * m.dx_um, 0, 'f', 2)
        .arg(m.ny * m.dy_um, 0, 'f', 2)
        .arg(m.dx_um * 1000.0, 0, 'f', 1)
        .arg(m.dy_um * 1000.0, 0, 'f', 1));
}

// ── 分散掃引 ────────────────────────────────────────────────────────────────
void ModeSolverTab::runSweep()
{
    if (m_busy) return;
    mds::Setup s;
    QString err;
    if (!buildSetup(s, err)) return;
    const int kind = m_sweepSel->currentIndex();
    setBusy(true, sweepSteps(s, kind), I18n::tr("mds_status_run"));

    auto out = std::make_shared<SweepJob>();
    auto prog = m_progress;
    QThread *th = QThread::create([s, kind, out, prog] {
        computeSweep(s, kind, *out, prog.get());
    });
    connect(th, &QThread::finished, this, [this, th, out, kind] {
        th->deleteLater();
        setBusy(false, 0, QString());
        m_disp = out->d;
        m_disp.xLabel = (kind == 0) ? QStringLiteral("λ [nm]")
                      : (kind == 1) ? QStringLiteral("w [nm]")
                                    : QStringLiteral("T [°C]");
        QStringList notes;
        if (out->noDnDt) notes << I18n::tr("mds_disp_nodndt");
        if (out->skipped > 0)
            notes << I18n::tr("mds_disp_skipped").arg(out->skipped);
        m_disp.note = notes.join(QStringLiteral("\n"));
        showDispersion();
        m_status->setText(I18n::tr("mds_status_sweep").arg(out->secs, 0, 'f', 1));
    });
    th->start();
}

void ModeSolverTab::showDispersion()
{
    m_dispPlot->setLabels(m_disp.xLabel.isEmpty() ? QStringLiteral("λ [nm]")
                                                  : m_disp.xLabel,
                          QStringLiteral("neff"));
    MiniSeries series;
    series.pts = m_disp.curve;
    series.markers = true;
    m_dispPlot->setSeries(m_disp.curve.isEmpty() ? QVector<MiniSeries>()
                                                 : QVector<MiniSeries>{ series });
    const QString nc = I18n::tr("mds_notcalc");
    const QString vals[4] = {
        m_disp.hasD ? QStringLiteral("%1 ps/nm/km")
                          .arg(m_disp.D_ps_nm_km, 0, 'f', 0) : nc,
        m_disp.hasBiref ? QString::number(m_disp.biref, 'f', 4) : nc,
        m_disp.hasDnDt ? QStringLiteral("%1 /K")
                             .arg(m_disp.dneff_dT, 0, 'e', 3) : nc,
        m_disp.hasDnDw ? QStringLiteral("%1 /nm")
                             .arg(m_disp.dneff_dw, 0, 'e', 3) : nc,
    };
    for (int r = 0; r < 4 && r < m_dispTable->rowCount(); ++r)
        m_dispTable->setItem(r, 1, roItem(vals[r]));
    fitTable(m_dispTable);
    // 未算出の理由・除外点は必ず画面に出す (黙って空欄にしない)
    m_dispNote->setText(m_disp.note);
    m_dispNote->setVisible(!m_disp.note.isEmpty());
}

// ── プロセスコーナー ────────────────────────────────────────────────────────
void ModeSolverTab::runCorners()
{
    if (m_busy) return;
    mds::Setup s;
    QString err;
    if (!buildSetup(s, err)) return;
    setBusy(true, kNCorners * 3, I18n::tr("mds_status_run"));

    auto out = std::make_shared<CornerJob>();
    auto prog = m_progress;
    QThread *th = QThread::create([s, out, prog] {
        computeCorners(s, *out, prog.get());
    });
    connect(th, &QThread::finished, this, [this, th, out] {
        th->deleteLater();
        setBusy(false, 0, QString());
        m_corners = out->rows;
        showCorners();
        m_status->setText(I18n::tr("mds_status_corner").arg(out->secs, 0, 'f', 1));
    });
    th->start();
}

void ModeSolverTab::showCorners()
{
    m_cornerTable->setRowCount(0);
    double dlMin = 0.0, dlMax = 0.0;
    bool anyDl = false;
    for (int i = 0; i < m_corners.size(); ++i) {
        const mds::CornerRow &c = m_corners[i];
        const int r = m_cornerTable->rowCount();
        m_cornerTable->insertRow(r);
        m_cornerTable->setItem(r, 0, roItem(
            I18n::tr(i < kNCorners ? kCorners[i].key : "mds_corner_nom")));
        m_cornerTable->setItem(r, 1, roItem(c.ok
            ? QString::number(c.neff, 'f', 4) : QStringLiteral("—")));
        m_cornerTable->setItem(r, 2, roItem(c.hasNg
            ? QString::number(c.ng, 'f', 3) : QStringLiteral("—")));
        m_cornerTable->setItem(r, 3, roItem(c.hasDl
            ? QStringLiteral("%1 nm").arg(c.dlambda_nm, 0, 'f', 2)
            : QStringLiteral("—")));
        if (!c.hasDl) continue;
        if (!anyDl) { dlMin = dlMax = c.dlambda_nm; anyDl = true; }
        dlMin = std::min(dlMin, c.dlambda_nm);
        dlMax = std::max(dlMax, c.dlambda_nm);
    }
    fitTable(m_cornerTable);
    m_cornerNote->setText(anyDl
        ? I18n::tr("mds_corner_spread").arg(dlMax - dlMin, 0, 'f', 2)
        : I18n::tr("mds_corner_idle"));
}

// ── 曲げ導波路 ──────────────────────────────────────────────────────────────
void ModeSolverTab::runBend()
{
    if (m_busy) return;
    mds::Setup s;
    QString err;
    if (!buildSetup(s, err)) return;
    m_bends.clear();
    m_bendTable->setRowCount(0);
    fitTable(m_bendTable);
    setBusy(true, 1 + kNBend, I18n::tr("mds_status_run"));

    auto out = std::make_shared<BendJob>();
    auto prog = m_progress;
    QThread *th = QThread::create([s, out, prog] {
        computeBend(s, *out, prog.get());
    });
    connect(th, &QThread::finished, this, [this, th, out] {
        th->deleteLater();
        setBusy(false, 0, QString());
        m_bends = out->rows;
        m_bendRatio = out->ratio;
        showBend();
        if (out->haveStraight) {
            m_status->setText(I18n::tr("mds_bend_status")
                                  .arg(out->secs, 0, 'f', 1));
        } else {
            // 直線モードが無ければ曲げの比較対象が無い (行を作らない)
            m_status->setText(I18n::tr("mds_status_none"));
            m_status->setStyleSheet("color:#B58900;");
            m_bendNote->setText(I18n::tr("mds_status_none"));
        }
    });
    th->start();
}

void ModeSolverTab::showBend()
{
    m_bendTable->setRowCount(0);
    const QString dash = QStringLiteral("—");
    for (const mds::BendRow &b : m_bends) {
        const int r = m_bendTable->rowCount();
        m_bendTable->insertRow(r);
        m_bendTable->setItem(r, 0, roItem(QStringLiteral("%1 µm")
                                              .arg(b.R_um, 0, 'f', 0)));
        m_bendTable->setItem(r, 1, roItem(b.ok
            ? QString::number(b.mismatchDb, 'f', 4)
            : I18n::tr("mds_bend_nomode")));
        m_bendTable->setItem(r, 2, roItem(b.hasCaustic
            ? QString::number(b.caustic_um, 'f', 3) : dash));
        // 放射損失は漏れモード解析が要るので常に未計算 (mds_bend_note で明示)
        m_bendTable->setItem(r, 3, roItem(I18n::tr("mds_notcalc")));
    }
    fitTable(m_bendTable);
    if (m_bends.isEmpty())
        m_bendNote->setText(I18n::tr("mds_bend_idle"));
    else
        m_bendNote->setText(I18n::tr("mds_bend_ratio")
                                .arg(m_bendRatio, 0, 'f', 3));
}
