// VerificationTab.cpp
#include "VerificationTab.h"
#include "TabHelpers.h"

#include <limits>
#include "../core/FdtdVerification.h"
#include "../core/Project.h"
#include "../kernel/Runner.h"
#include "../widgets/MiniPlot.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"

#include <QColor>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QShowEvent>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <string>

using namespace ofd;

// ── タブ専用語彙 (file-local 登録, 接頭辞 ver_) ─────────────────────────────
namespace {
const bool s_i18n = [] {
    using ofd::I18n;
    I18n::reg("ver_title", "精度検証", "Result Verification");
    I18n::reg("ver_hint",
              "ソルバ結果の信頼性を複数の観点からチェックする画面。設定から"
              "決まる量は実計算し、実行が要る量はソルバーの実行ログから表示"
              "します。",
              "Screen for checking solver result reliability from several "
              "angles. Quantities determined by the settings are computed "
              "here; quantities that need a run are read from the solver log.");
    // 表示区分の明示 (CLAUDE.md 絶対規則 5 — 未実装を動作済みに見せない)
    I18n::reg("ver_scope_note",
              "表示の区分 — 【実計算】メッシュ解像度の計画値・境界の設計反射率・"
              "自動診断 (いずれも現在のプロジェクト設定から計算) / 【実データ】"
              "収束履歴 (ソルバーの実行ログを読む) / 【未計算】各解像度での結果"
              "比較・反射率の実測値 (ソルバー実行または未実装の機能が必要 — "
              "該当欄は「—」のままです)",
              "What is shown — [computed] mesh resolution plan, boundary design "
              "reflection, auto-diagnostics (all derived from the current "
              "project settings); [measured] convergence history (read from the "
              "solver log); [not computed] per-resolution result comparison and "
              "measured reflection (these need a run or an unimplemented "
              "feature and stay as \"—\")");

    I18n::reg("ver_mesh_title", "① メッシュ解像度", "① Mesh resolution");
    I18n::reg("ver_mesh_hint",
              "メッシュ精度を段階的に上げて結果の収束を確認する。セル数・λ/Δx・"
              "推定メモリは現在のメッシュから実計算した計画値。",
              "Raise mesh accuracy stepwise and confirm the result converges. "
              "Cell count, λ/Δx and estimated memory are computed from the "
              "current mesh.");
    I18n::reg("ver_mesh_qty", "チェックする量", "Quantity to check");
    I18n::reg("ver_mesh_stop", "収束テストを中止", "Stop the convergence test");
    I18n::reg("ver_mesh_running", "収束テスト実行中 %1 / %2 — メッシュ %3",
              "Convergence test running %1 / %2 — mesh %3");
    I18n::reg("ver_mesh_run_failed",
              "収束テストを開始できませんでした (ログを確認してください)",
              "Could not start the convergence test (see the log)");
    I18n::reg("ver_mesh_run_partial",
              "収束テストの一部の解像度が失敗しました",
              "Some resolutions failed during the convergence test");
    I18n::reg("ver_mesh_failed", "実行失敗", "run failed");
    I18n::reg("ver_mesh_noqty", "給電点表なし", "no feed table");
    I18n::reg("ver_mesh_noqty_all",
              "チェック量を取得できた解像度が 2 つ未満です "
              "(波源と frequency1 が要ります)",
              "Fewer than two resolutions yielded the quantity "
              "(a feed and frequency1 are required)");
    I18n::reg("ver_mesh_ref", "基準 (最細)", "reference (finest)");
    I18n::reg("ver_mesh_conv_fmt",
              "最細解像度との差が ±%3 dB 以内: %1 / %2 解像度",
              "Within +/-%3 dB of the finest resolution: %1 of %2");
    I18n::reg("ver_h_mesh", "メッシュ (倍率)", "Mesh (factor)");
    I18n::reg("ver_h_cells", "セル数", "Cells");
    I18n::reg("ver_h_res", "λ/Δx", "λ/Δx");
    I18n::reg("ver_h_mem", "推定メモリ", "Est. memory");
    I18n::reg("ver_h_result", "結果", "Result");
    I18n::reg("ver_h_err", "誤差 vs 最高", "Error vs finest");
    I18n::reg("ver_mesh_cur", "現在", "current");
    I18n::reg("ver_dash", "—", "—");
    I18n::reg("ver_mesh_note",
              "セル数・λ/Δx・推定メモリは現在のメッシュから実計算した計画値です"
              " (メモリは %1 byte/セルの見積り。λ/Δx は最も粗い方向の Δx で"
              "評価)。「結果」「誤差」列は各解像度でソルバーを実行しないと"
              "埋まりません — 自動収束テストは未実装です。",
              "Cell count, λ/Δx and estimated memory are computed from the "
              "current mesh (memory assumes %1 byte/cell; λ/Δx uses the "
              "coarsest Δx). The Result and Error columns stay empty until the "
              "solver is run at each resolution — the automatic convergence "
              "test is not implemented.");
    I18n::reg("ver_mesh_res_fmt", "現在のメッシュ: λ/Δx = %1 (Δx_max = %2, λ = %3 @ %4)",
              "Current mesh: λ/Δx = %1 (Δx_max = %2, λ = %3 at %4)");
    I18n::reg("ver_mesh_res_na",
              "λ/Δx 未計算 — 解析周波数1 (frequency1) が無く波長が決まりません",
              "λ/Δx not computed — frequency1 is absent so the wavelength is unknown");
    I18n::reg("ver_mesh_run", "▶ 自動収束テスト実行", "▶ Run auto-convergence test");

    I18n::reg("ver_pml_title", "② 境界吸収 (設計反射率)",
              "② Boundary absorption (design reflection)");
    I18n::reg("ver_pml_hint",
              "吸収境界の反射がどれだけ小さい設計になっているかを、設定値から"
              "理論式で確認する",
              "Check how small the absorbing-boundary reflection is by design, "
              "from the settings and the published formula");
    I18n::reg("ver_h_angle", "入射角 θ", "Incidence θ");
    I18n::reg("ver_h_design", "設計反射率 (理論)", "Design reflection (theory)");
    I18n::reg("ver_h_meas", "実測", "Measured");
    I18n::reg("ver_meas_na", "— (未測定)", "— (not measured)");
    I18n::reg("ver_pml_design_note",
              "設計値は連続体近似の理論式 R(θ) = R₀^cosθ (Berenger 1994 式(26) / "
              "Taflove & Hagness 3rd ed. §7.7) に現在の PML 設定 (層数 %1, "
              "次数 m = %2, R₀ = %3) を当てはめた値です。離散化による数値反射"
              "(層数不足・急峻なグレーディング) は含まないため、実際の反射は"
              "これより大きくなります。実測にはソルバー側の反射測定が必要で"
              "未実装です。",
              "The design value applies the continuum formula R(θ) = R₀^cosθ "
              "(Berenger 1994 eq. 26 / Taflove & Hagness 3rd ed. §7.7) to the "
              "current PML settings (%1 layers, order m = %2, R₀ = %3). It "
              "EXCLUDES discretization reflection (too few layers, steep "
              "grading), so the real reflection is larger. Measuring it needs "
              "solver-side instrumentation, which is not implemented.");
    I18n::reg("ver_mur_design_note",
              "境界条件が 1 次 Mur (abc = 0) です。設計値は Engquist–Majda の 1 次"
              "近似が残す反射 R(θ) = (1 − cosθ)/(1 + cosθ) (Mur 1981) の理論値で、"
              "垂直入射では 0、斜入射で急増します。実測は未実装です。",
              "The boundary is first-order Mur (abc = 0). The design value is "
              "the residual reflection of the Engquist–Majda first-order "
              "approximation, R(θ) = (1 − cosθ)/(1 + cosθ) (Mur 1981): zero at "
              "normal incidence, rising steeply with angle. Measurement is not "
              "implemented.");
    // 対策ボタン (配線済み — Project の実設定を変更する)。層数は現在値から表示
    I18n::reg("ver_pml_btn1_fmt", "PML層数を%1→%2に増加",
              "Increase PML layers %1→%2");
    I18n::reg("ver_pml_btn1_tip",
              "PML層数を増やして境界反射を低減します (プロジェクト設定を実際に"
              "変更。境界条件が Mur の場合は PML へ切り替えます)",
              "Increase the PML layer count to reduce boundary reflection "
              "(actually modifies the project settings; switches a Mur "
              "boundary to PML)");
    I18n::reg("ver_pml_btn2", "境界余裕を増加 (+λ/4)", "Increase boundary margin (+λ/4)");
    I18n::reg("ver_pml_btn2_tip",
              "解析周波数1の中心 %1 の λ/4 = %2 をメッシュ各軸の両端に追加します"
              " (プロジェクト設定を実際に変更。周期境界の軸は除外)",
              "Extend the mesh on both ends of each axis by λ/4 = %2 at %1 "
              "(center of frequency1). Actually modifies the project settings; "
              "periodic-boundary axes are excluded");
    I18n::reg("ver_pml_margin_na",
              "メッシュが未定義/不正、または解析周波数1が無いため使用できません",
              "Unavailable: the mesh is undefined/invalid or frequency1 is absent");

    I18n::reg("ver_time_title", "③ 収束履歴 (実行ログ)",
              "③ Convergence history (run log)");
    I18n::reg("ver_time_hint",
              "ソルバーの実行ログを読み、自動シャットオフ前に振幅が十分減衰して"
              "いるか確認する",
              "Read the solver run log and confirm the amplitude decayed "
              "sufficiently before auto-shutoff");
    I18n::reg("ver_time_ok_fmt",
              "収束: 平均|E| = %1, 平均|H| = %2 ≤ 判定値 %3 (step %4)",
              "Converged: mean|E| = %1, mean|H| = %2 ≤ threshold %3 (step %4)");
    I18n::reg("ver_time_warn_fmt",
              "未収束: 平均|E| = %1, 平均|H| = %2 > 判定値 %3 (step %4)",
              "Not converged: mean|E| = %1, mean|H| = %2 > threshold %3 (step %4)");
    I18n::reg("ver_time_none_fmt",
              "未実行 — ソルバーを実行すると、作業ディレクトリのログ (%1) から"
              "収束履歴をここに描画します",
              "Not run — once the solver runs, the convergence history is drawn "
              "here from the log (%1) in the working directory");
    I18n::reg("ver_time_reload", "⟳ 実行ログを再読込", "⟳ Reload run log");
    I18n::reg("ver_time_src_fmt", "読込元: %1 (%2 点)", "Source: %1 (%2 points)");
    I18n::reg("ver_time_x", "step", "step");
    I18n::reg("ver_time_y", "log10 (平均振幅)", "log10 (mean amplitude)");
    I18n::reg("ver_time_thresh", "収束判定値", "convergence threshold");
    I18n::reg("ver_time_note", "▸ 高Q構造では時間を延長してください",
              "▸ Extend the simulation time for high-Q structures");

    I18n::reg("ver_cross_title", "④ 周波数領域比較", "④ Cross-validation");
    I18n::reg("ver_cross_hint", "同じ問題をFEM/RCWA等の異なるソルバで解いて結果を比較",
              "Solve the same problem with a different solver (FEM/RCWA…) and compare");
    I18n::reg("ver_cross_solver", "比較ソルバ", "Comparison solver");
    I18n::reg("ver_cross_run", "▶ クロスバリデーション実行", "▶ Run cross-validation");
    // ドメイン別の比較ソルバ名 (選択のみ。実行連携は未実装)
    I18n::reg("ver_cross_ism", "ISM (鏡像法)", "ISM (image source)");
    I18n::reg("ver_cross_ray", "レイトレース", "Ray tracing");
    I18n::reg("ver_cross_pe", "PE (放物型方程式)", "PE (parabolic equation)");

    I18n::reg("ver_diag_title", "自動診断", "Auto-diagnostics");
    I18n::reg("ver_h_item", "項目", "Item");
    I18n::reg("ver_h_verdict", "判定", "Verdict");
    I18n::reg("ver_h_note", "備考", "Note");
    // 判定バッジ
    I18n::reg("ver_v_ok", "OK", "OK");
    I18n::reg("ver_v_warn", "注意", "Check");
    I18n::reg("ver_v_ng", "NG", "NG");
    I18n::reg("ver_v_na", "未判定", "Undetermined");
    I18n::reg("ver_v_auto", "自動", "Auto");
    // 診断項目名
    I18n::reg("ver_d_res", "λ/Δx ≥ 10", "λ/Δx ≥ 10");
    I18n::reg("ver_d_cfl", "CFL 安定条件 (S ≤ 1)", "CFL stability (S ≤ 1)");
    I18n::reg("ver_d_abc", "吸収境界の設定", "Absorbing boundary setting");
    I18n::reg("ver_d_conv", "収束 (シャットオフ) 到達", "Shutoff reached");
    I18n::reg("ver_d_monin", "観測点が解析領域内", "Monitors inside the domain");
    I18n::reg("ver_d_sep", "波源と観測点の距離 > λ", "Source–monitor distance > λ");
    I18n::reg("ver_d_margin", "形状と境界の余裕 ≥ λ/4", "Structure–boundary margin ≥ λ/4");
    // 診断備考
    I18n::reg("ver_n_res_fmt", "λ/Δx = %1 (Δx_max = %2, λ = %3)",
              "λ/Δx = %1 (Δx_max = %2, λ = %3)");
    I18n::reg("ver_n_res_na", "解析周波数1が無く波長が決まりません",
              "frequency1 is absent so the wavelength is unknown");
    I18n::reg("ver_n_cfl_fmt", "S = %1 (Δt = %2, 伝搬速度 %3 m/s を仮定)",
              "S = %1 (Δt = %2, assuming %3 m/s)");
    I18n::reg("ver_n_cfl_auto",
              "Δt 自動 (dt = 0) — カーネルが安定条件を満たす Δt を決めるため"
              "GUI では検算していません",
              "Δt is automatic (dt = 0) — the kernel picks a stable Δt, so the "
              "GUI does not verify it");
    I18n::reg("ver_n_abc_pml_fmt", "PML %1 層, m = %2, R₀ = %3 (垂直入射 設計 %4)",
              "PML %1 layers, m = %2, R₀ = %3 (design %4 at normal incidence)");
    I18n::reg("ver_n_abc_mur", "1 次 Mur (abc = 0) — 斜入射で反射が大きい",
              "First-order Mur (abc = 0) — large reflection at oblique incidence");
    I18n::reg("ver_n_conv_fmt", "最終 step %1: 平均|E| = %2, 平均|H| = %3 (判定値 %4)",
              "Last step %1: mean|E| = %2, mean|H| = %3 (threshold %4)");
    I18n::reg("ver_n_conv_none", "未実行 — 実行ログ (%1) が見つかりません",
              "Not run — no run log (%1) found");
    I18n::reg("ver_n_mon_ok", "観測点 %1 点すべてが解析領域内",
              "All %1 monitor points are inside the domain");
    I18n::reg("ver_n_mon_out", "観測点 %1 点中 %2 点が解析領域外",
              "%2 of %1 monitor points are outside the domain");
    I18n::reg("ver_n_mon_none", "観測点 (point) がありません",
              "No monitor points (point) defined");
    I18n::reg("ver_n_sep_fmt", "最小距離 %1 = %2 λ", "Minimum distance %1 = %2 λ");
    I18n::reg("ver_n_sep_none", "給電点または観測点がありません",
              "No feed or no monitor point");
    I18n::reg("ver_n_sep_pw", "平面波励振のため給電点との距離は対象外",
              "Plane-wave excitation — feed distance does not apply");
    I18n::reg("ver_n_margin_fmt", "形状から境界までの最小余裕 %1 = %2 λ",
              "Minimum structure-to-boundary margin %1 = %2 λ");
    I18n::reg("ver_n_margin_none", "形状 (geometry) がありません",
              "No geometry defined");
    I18n::reg("ver_n_margin_shape",
              "外接直方体を一意に決められない形状 (角錐台/円錐台) があるため未判定",
              "Undetermined: some shapes (frusta) have no unambiguous bounding box");
    I18n::reg("ver_diag_note",
              "判定はいずれも現在のプロジェクト設定からの実計算です。閾値の根拠: "
              "安定条件 S ≤ 1 と 1 波長あたり 10〜20 セルは Taflove & Hagness "
              "3rd ed. §4.5 / §4.7、近傍界の境界 λ/2π は Balanis 4th ed. §2.2.4。"
              "境界余裕 λ/4 は規格ではなく実務上の目安です。波長・CFL の評価には"
              "伝搬速度 %1 m/s を仮定しています (%2)。実行結果が要る行は未実行の"
              "とき「未判定」と表示します。",
              "Every verdict is computed from the current project settings. "
              "Thresholds: S ≤ 1 and 10–20 cells per wavelength follow Taflove "
              "& Hagness 3rd ed. §4.5/§4.7; the λ/2π near-field boundary "
              "follows Balanis 4th ed. §2.2.4. The λ/4 margin is a practical "
              "rule of thumb, not a standard. Wavelength and CFL assume a "
              "propagation speed of %1 m/s (%2). Rows that need a run show "
              "\"Undetermined\" until one exists.");
    I18n::reg("ver_speed_em", "真空中の光速", "speed of light in vacuum");
    I18n::reg("ver_speed_air", "空気中の音速 20℃", "speed of sound in air at 20℃");
    I18n::reg("ver_speed_sea", "海水中の代表音速", "typical speed of sound in sea water");
    I18n::reg("ver_uw_cross", "比較ソルバの選択",
              "the cross-check solver selection");
    return true;
}();

// ── バッジ ─────────────────────────────────────────────────────────────────
void styleBadge(QLabel *b, const char *kind)
{
    QString css = "border-radius:3px; padding:1px 6px; font-size:11px;";
    if (qstrcmp(kind, "ok") == 0)        css += "background:#DFF6DD; color:#0F7B0F;";
    else if (qstrcmp(kind, "warn") == 0) css += "background:#FFF4CE; color:#9D5D00;";
    else if (qstrcmp(kind, "err") == 0)  css += "background:#FDE7E9; color:#A4262C;";
    else if (qstrcmp(kind, "acc") == 0)  css += "background:#DEECF9; color:#0078D4;";
    else                                  css += "background:palette(midlight);";
    b->setStyleSheet(css);
}

QLabel *makeBadge(const QString &text, const char *kind, QWidget *parent)
{
    auto *b = new QLabel(text, parent);
    b->setWordWrap(true);
    styleBadge(b, kind);
    return b;
}

// 表セル内バッジ (左寄せ)。生成した QLabel を outLabel へ返す
QWidget *badgeCell(QLabel **outLabel)
{
    auto *w = new QWidget;
    auto *h = new QHBoxLayout(w);
    h->setContentsMargins(4, 2, 4, 2);
    auto *lb = new QLabel(w);
    styleBadge(lb, "");
    h->addWidget(lb);
    h->addStretch(1);
    if (outLabel) *outLabel = lb;
    return w;
}

// 判定 → バッジ文言と色
const char *verdictKind(verify::Verdict v)
{
    switch (v) {
    case verify::Verdict::Ok:   return "ok";
    case verify::Verdict::Warn: return "warn";
    case verify::Verdict::Ng:   return "err";
    default:                    return "";
    }
}

QString verdictText(verify::Verdict v)
{
    switch (v) {
    case verify::Verdict::Ok:   return I18n::tr("ver_v_ok");
    case verify::Verdict::Warn: return I18n::tr("ver_v_warn");
    case verify::Verdict::Ng:   return I18n::tr("ver_v_ng");
    default:                    return I18n::tr("ver_v_na");
    }
}

QLabel *hintLabel(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setWordWrap(true);
    return l;
}

QLabel *noteLabel(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setWordWrap(true);
    l->setStyleSheet("font-size:11px; color:palette(mid);");
    return l;
}

// ドメイン別「チェックする量」(mock の三項演算子をそのまま転記)
const char *meshQuantity(ofd::Domain d)
{
    switch (d) {
        case ofd::Domain::Optical:    return "T_drop @ 1550nm";
        case ofd::Domain::Acoustic:   return "RT60 @ 1kHz";
        case ofd::Domain::Underwater: return "TL @ 50km";
        default:                      return "S11 @ 2.45GHz";
    }
}

// ── λ/4 境界余裕の計算 (MeshTab.cpp の λ/N 評価と同じ定義) ──────────────────
// 波長評価に使うドメイン別の伝搬速度 [m/s]。MeshTab の file-local ヘルパーと
// 同値 (数行のためここも file-local に留める)。水中は PML セクション自体を
// 隠すので到達しないが、既定 (光速) のままにしておく。
double domainWaveSpeed(ofd::Domain d)
{
    switch (d) {
    case ofd::Domain::Acoustic:   return 343.0;         // 空気中の音速 (20℃)
    case ofd::Domain::Underwater: return 1500.0;        // 海水中の代表音速
    default:                      return 2.99792458e8;  // EM / 光: 真空光速
    }
}

// 伝搬速度の根拠を説明する i18n キー (画面に仮定を明示するため)
QString waveSpeedNoteKey(ofd::Domain d)
{
    switch (d) {
    case ofd::Domain::Acoustic:   return I18n::tr("ver_speed_air");
    case ofd::Domain::Underwater: return I18n::tr("ver_speed_sea");
    default:                      return I18n::tr("ver_speed_em");
    }
}

// 解析周波数1の中心 [Hz] (MeshTab の λ/N 評価と同じ)。無ければ 0
double centerFreq1(const ofd::GeneralOpts &g)
{
    return g.hasF1 ? 0.5 * (g.f1min + g.f1max) : 0.0;
}

// 周波数の簡易表示 (MeshTab::formatFreq と同形式)
QString formatFreq(double f)
{
    if (f >= 1e9) return QStringLiteral("%1 GHz").arg(f / 1e9, 0, 'g', 3);
    if (f >= 1e6) return QStringLiteral("%1 MHz").arg(f / 1e6, 0, 'g', 3);
    if (f >= 1e3) return QStringLiteral("%1 kHz").arg(f / 1e3, 0, 'g', 3);
    return QStringLiteral("%1 Hz").arg(f, 0, 'g', 3);
}

// 長さの簡易表示 (単位を自動で選ぶ)
QString formatLength(double m)
{
    const double a = std::fabs(m);
    if (!(a > 0) || !std::isfinite(m)) return QStringLiteral("—");
    if (a < 1e-6) return QStringLiteral("%1 nm").arg(m * 1e9, 0, 'g', 3);
    if (a < 1e-3) return QStringLiteral("%1 µm").arg(m * 1e6, 0, 'g', 3);
    if (a < 1.0)  return QStringLiteral("%1 mm").arg(m * 1e3, 0, 'g', 3);
    return QStringLiteral("%1 m").arg(m, 0, 'g', 3);
}

QString formatMemory(double mb)
{
    if (!(mb > 0) || !std::isfinite(mb)) return QStringLiteral("—");
    if (mb >= 1024.0) return QStringLiteral("%1 GB").arg(mb / 1024.0, 0, 'f', 2);
    return QStringLiteral("%1 MB").arg(mb, 0, 'f', 1);
}

QString formatCells(long long n)
{
    // C ロケールは既定で桁区切りを省くので、明示的に有効化して読みやすくする
    QLocale loc = QLocale::c();
    loc.setNumberOptions(loc.numberOptions() & ~QLocale::OmitGroupSeparator);
    return loc.toString(static_cast<qlonglong>(n));
}

// 解析周波数1の中心に対応する波長 [m]。周波数が無ければ 0 (= 未計算)
double wavelength(const ofd::Project &p)
{
    const double fc = centerFreq1(p.general());
    return (fc > 0) ? domainWaveSpeed(p.activeDomain()) / fc : 0.0;
}

// λ/4 マージン [m]。周波数が無く計算できないときは 0
double quarterWaveMargin(const ofd::Project &p)
{
    const double lambda = wavelength(p);
    return (lambda > 0) ? 0.25 * lambda : 0.0;
}

// 軸 a (0=x 1=y 2=z) が +λ/4 拡張の対象か。
// 周期境界 (PBC) の軸は端を延ばすと周期そのものが変わってしまうため除外する
bool axisExtendable(const ofd::Project &p, int a)
{
    const ofd::GeneralOpts &g = p.general();
    const bool pbc = (a == 0) ? g.pbcX : (a == 1) ? g.pbcY : g.pbcZ;
    return !pbc;
}

// +λ/4 拡張が今すぐ安全に実行できるか (対象軸が 1 本以上あり、全て妥当)
bool canAddMargin(const ofd::Project &p)
{
    if (!(quarterWaveMargin(p) > 0)) return false;
    bool any = false;
    for (int a = 0; a < 3; ++a) {
        if (!axisExtendable(p, a)) continue;
        if (!p.mesh(a).isValid()) return false;
        any = true;
    }
    return any;
}

// Project のメッシュ → 検証モジュールの格子仕様
verify::Grid gridFromProject(const ofd::Project &p)
{
    verify::Grid g;
    for (int a = 0; a < 3; ++a) {
        const ofd::MeshAxis &ax = p.mesh(a);
        if (!ax.isValid()) continue;
        double dmin = 1e308, dmax = 0.0;
        for (int i = 0; i < ax.divs.size(); ++i) {
            const double d = (ax.nodes[i + 1] - ax.nodes[i]) / ax.divs[i];
            dmin = std::min(dmin, d);
            dmax = std::max(dmax, d);
        }
        if (!(dmax > 0)) continue;
        g.axis[a].cells   = ax.totalCells();
        g.axis[a].dxMin_m = (dmin < 1e308) ? dmin : 0.0;
        g.axis[a].dxMax_m = dmax;
    }
    return g;
}

// 形状群の外接直方体。角錐台/円錐台 (41..53) は g[4..7] が寸法で、
// 断面の広がりを一意に決められないため false を返す (誤った余裕を出さない)。
bool geometryBounds(const ofd::Project &p, double lo[3], double hi[3])
{
    bool any = false;
    for (int a = 0; a < 3; ++a) { lo[a] = 1e308; hi[a] = -1e308; }
    for (const ofd::Geometry &g : p.geometries()) {
        const int s = g.shape;
        const bool bounded = (s == 1 || s == 2 || s == 11 || s == 12 ||
                              s == 13 || s == 31 || s == 32 || s == 33);
        if (!bounded) return false;
        for (int a = 0; a < 3; ++a) {
            int idx[3] = { 0, 0, 0 };
            const int n = ofd::Geometry::coordIndices(s, a, idx);
            for (int i = 0; i < n; ++i) {
                lo[a] = std::min(lo[a], g.g[idx[i]]);
                hi[a] = std::max(hi[a], g.g[idx[i]]);
            }
        }
        any = true;
    }
    return any;
}

// ソルバー実行ログのファイル名 (カーネル別)。水中 (bellhopcxx) は収束履歴を
// 出さないので空文字列
QString runLogName(ofd::Kernel k)
{
    switch (k) {
    case ofd::Kernel::FDTD: return QStringLiteral("ofd.log");
    case ofd::Kernel::RCWA: return QStringLiteral("orcwa.log");
    case ofd::Kernel::BPM:  return QStringLiteral("obpm.log");
    default:                return QString();
    }
}

// 表示する解像度レベル (現在のメッシュに対する 1 軸あたり分割数の倍率)
const double kRefineFactors[5] = { 0.5, 0.707106781, 1.0, 1.414213562, 2.0 };
// 収束したとみなす差 [dB] (最細解像度の値との差がこれ以下)
const double kConvergedTol_dB = 0.5;
// ② で設計反射率を出す入射角 [deg]
const double kAngles[5] = { 0.0, 30.0, 45.0, 60.0, 80.0 };
// 実行ログの読み込み上限 [byte] (GUI スレッドを止めないための保険)
constexpr qint64 kMaxLogBytes = 16 * 1024 * 1024;
} // namespace

// ── VerificationTab ─────────────────────────────────────────────────────────
VerificationTab::VerificationTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // 精度検証
    auto *sTop = new SectionBox(I18n::tr("ver_title"), body);
    sTop->vbox()->addWidget(hintLabel(I18n::tr("ver_hint"), sTop));
    // 何が実計算で何が未計算かをタブ先頭で宣言する
    auto *scopeNote = hintLabel(I18n::tr("ver_scope_note"), sTop);
    scopeNote->setStyleSheet(
        "background:#DEECF9; color:#204E7A; border-radius:3px; "
        "padding:4px 8px; font-size:11px;");
    sTop->vbox()->addWidget(scopeNote);
    v->addWidget(sTop);

    // ① メッシュ解像度 (計画値は実計算、結果列は未実行)
    auto *sMesh = new SectionBox(I18n::tr("ver_mesh_title"), body);
    sMesh->vbox()->addWidget(hintLabel(I18n::tr("ver_mesh_hint"), sMesh));
    m_qtyBox = new QComboBox(sMesh);
    sMesh->form()->addRow(I18n::tr("ver_mesh_qty"), m_qtyBox);
    // 「チェックする量」は下の自動収束テストが読む (ドメインごとに固定)。
    // 電磁では給電点表の反射係数 Ref[dB] を frequency1 の中心で拾う。

    m_meshTbl = new QTableWidget(5, 6, sMesh);
    m_meshTbl->setHorizontalHeaderLabels({
        I18n::tr("ver_h_mesh"), I18n::tr("ver_h_cells"), I18n::tr("ver_h_res"),
        I18n::tr("ver_h_mem"), I18n::tr("ver_h_result"), I18n::tr("ver_h_err") });
    for (int r = 0; r < 5; ++r)
        for (int c = 0; c < 6; ++c)
            m_meshTbl->setItem(r, c, new QTableWidgetItem());
    m_meshTbl->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_meshTbl->verticalHeader()->setVisible(false);
    m_meshTbl->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_meshTbl->setMinimumHeight(170);
    sMesh->vbox()->addWidget(m_meshTbl);

    auto *meshRow = new QHBoxLayout();
    m_meshStatus = makeBadge(QString(), "", sMesh);
    meshRow->addWidget(m_meshStatus);
    m_meshRunBtn = new QPushButton(I18n::tr("ver_mesh_run"), sMesh);
    meshRow->addWidget(m_meshRunBtn);
    connect(m_meshRunBtn, &QPushButton::clicked, this,
            &VerificationTab::startMeshConvergence);
    meshRow->addStretch(1);
    sMesh->vbox()->addLayout(meshRow);
    m_meshNote = noteLabel(I18n::tr("ver_mesh_note")
                               .arg(verify::kBytesPerCell, 0, 'g', 3), sMesh);
    sMesh->vbox()->addWidget(m_meshNote);
    v->addWidget(sMesh);

    // ② 境界吸収の設計反射率 (FDTD 系のみ — 水中ドメインでは refreshDomain が隠す)
    auto *sPml = new SectionBox(I18n::tr("ver_pml_title"), body);
    m_pmlSection = sPml;
    sPml->vbox()->addWidget(hintLabel(I18n::tr("ver_pml_hint"), sPml));
    m_pmlTbl = new QTableWidget(5, 3, sPml);
    m_pmlTbl->setHorizontalHeaderLabels({
        I18n::tr("ver_h_angle"), I18n::tr("ver_h_design"), I18n::tr("ver_h_meas") });
    for (int r = 0; r < 5; ++r)
        for (int c = 0; c < 3; ++c)
            m_pmlTbl->setItem(r, c, new QTableWidgetItem());
    m_pmlTbl->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_pmlTbl->verticalHeader()->setVisible(false);
    m_pmlTbl->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_pmlTbl->setMinimumHeight(170);
    sPml->vbox()->addWidget(m_pmlTbl);
    auto *pmlRow = new QHBoxLayout();
    // 対策 2 ボタンは配線済み: Project の実設定 (PML 層数 / メッシュ端) を
    // 変更する。文言・有効状態は refreshPmlButtons() が現状に合わせる
    m_pmlBtn1 = new QPushButton(sPml);
    m_pmlBtn2 = new QPushButton(I18n::tr("ver_pml_btn2"), sPml);
    connect(m_pmlBtn1, &QPushButton::clicked,
            this, &VerificationTab::increasePmlLayers);
    connect(m_pmlBtn2, &QPushButton::clicked,
            this, &VerificationTab::addBoundaryMargin);
    pmlRow->addWidget(m_pmlBtn1);
    pmlRow->addWidget(m_pmlBtn2);
    pmlRow->addStretch(1);
    sPml->vbox()->addLayout(pmlRow);
    m_pmlNote = noteLabel(QString(), sPml);
    sPml->vbox()->addWidget(m_pmlNote);
    v->addWidget(sPml);

    // ③ 収束履歴 (実行ログ) — 時間領域ソルバのみ
    auto *sTime = new SectionBox(I18n::tr("ver_time_title"), body);
    m_timeSection = sTime;
    sTime->vbox()->addWidget(hintLabel(I18n::tr("ver_time_hint"), sTime));
    m_energyPlot = new MiniPlot(sTime);
    m_energyPlot->setLabels(I18n::tr("ver_time_x"), I18n::tr("ver_time_y"));
    m_energyPlot->setMinimumHeight(120);
    sTime->vbox()->addWidget(m_energyPlot);
    auto *timeRow = new QHBoxLayout();
    m_timeBadge = makeBadge(QString(), "", sTime);
    timeRow->addWidget(m_timeBadge, 1);
    auto *reloadBtn = new QPushButton(I18n::tr("ver_time_reload"), sTime);
    connect(reloadBtn, &QPushButton::clicked, this, &VerificationTab::reloadRunLog);
    timeRow->addWidget(reloadBtn);
    sTime->vbox()->addLayout(timeRow);
    m_timeSource = noteLabel(QString(), sTime);
    sTime->vbox()->addWidget(m_timeSource);
    sTime->vbox()->addWidget(hintLabel(I18n::tr("ver_time_note"), sTime));
    v->addWidget(sTime);

    // ④ クロスバリデーション (実行連携は未実装)
    auto *sCross = new SectionBox(I18n::tr("ver_cross_title"), body);
    sCross->vbox()->addWidget(hintLabel(I18n::tr("ver_cross_hint"), sCross));
    // 項目はドメイン別 (refreshDomain が入れる)
    m_crossBox = new QComboBox(sCross);
    sCross->form()->addRow(I18n::tr("ver_cross_solver"), m_crossBox);
    // 比較ソルバの選択はどこにも読まれない
    sCross->form()->addRow(tabhelp::unwiredNote(sCross, I18n::tr("ver_uw_cross")));
    auto *crossRow = new QHBoxLayout();
    auto *crossRunBtn = new QPushButton(I18n::tr("ver_cross_run"), sCross);
    tabhelp::markNotImplemented(crossRunBtn);   // クロスバリデーションは未実装
    crossRow->addWidget(crossRunBtn);
    crossRow->addStretch(1);
    sCross->vbox()->addLayout(crossRow);
    v->addWidget(sCross);

    // 自動診断 (プロジェクト設定からの実判定 + 実行ログ由来の 1 行)
    auto *sDiag = new SectionBox(I18n::tr("ver_diag_title"), body);
    m_diag = new QTableWidget(DiagRowCount, 3, sDiag);
    m_diag->setHorizontalHeaderLabels({
        I18n::tr("ver_h_item"), I18n::tr("ver_h_verdict"), I18n::tr("ver_h_note") });
    const char *kDiagKeys[DiagRowCount] = {
        "ver_d_res", "ver_d_cfl", "ver_d_abc", "ver_d_conv",
        "ver_d_monin", "ver_d_sep", "ver_d_margin" };
    for (int r = 0; r < DiagRowCount; ++r) {
        m_diag->setItem(r, 0, new QTableWidgetItem(I18n::tr(kDiagKeys[r])));
        m_diag->setCellWidget(r, 1, badgeCell(&m_diagBadge[r]));
        m_diag->setItem(r, 2, new QTableWidgetItem());
    }
    m_diag->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_diag->verticalHeader()->setVisible(false);
    m_diag->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_diag->setMinimumHeight(230);
    sDiag->vbox()->addWidget(m_diag);
    m_diagNote = noteLabel(QString(), sDiag);
    sDiag->vbox()->addWidget(m_diagNote);
    v->addWidget(sDiag);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    connect(project, &Project::domainChanged, this, &VerificationTab::refreshDomain);
    // 他タブでの編集 (メッシュ・周波数・境界条件・形状) を表示へ追従させる。
    // ここで行うのは算術だけ (ファイル読み込みは reloadRunLog に限る)。
    connect(project, &Project::changed, this, &VerificationTab::refreshComputed);
    connect(project, &Project::loaded, this, &VerificationTab::reloadRunLog);
    refreshDomain();
    reloadRunLog();
}

// タブを開くたびに実行ログを見に行く (実行後に開けば結果が反映される)
void VerificationTab::showEvent(QShowEvent *e)
{
    QScrollArea::showEvent(e);
    reloadRunLog();
}

// ② 対策ボタン 1: PML 層数を増加 (既に 12 以上なら +4)。
// Mur (abc=0) のまま層数だけ増やしても効果が無いので PML へ切り替える
void VerificationTab::increasePmlLayers()
{
    GeneralOpts &g = m_p->general();
    g.abc = 1;                                    // PML を使用
    g.pmlL = (g.pmlL >= 12) ? g.pmlL + 4 : 12;
    m_p->touch();   // changed() → refreshComputed() で表示も追従
}

// ② 対策ボタン 2: メッシュ各軸の両端を λ/4 (解析周波数1の中心) だけ拡張する。
// 追加区間の分割数は端の区間のセル幅を保つように選ぶ (最低 1 分割)。
// PBC の軸は周期が変わってしまうため対象外 (axisExtendable)
void VerificationTab::addBoundaryMargin()
{
    if (!canAddMargin(*m_p)) return;   // 途中で失敗して半端に拡張しない
    const double margin = quarterWaveMargin(*m_p);
    // 端のセル幅を保つ分割数。周波数とメッシュスケールが極端に不整合でも
    // int を溢れさせない (上限 1e6 — その場合は総セル数の統計表示で気付ける)
    const auto ndiv = [](double ratio) {
        return int(qBound(1.0, std::floor(ratio + 0.5), 1e6));
    };
    for (int a = 0; a < 3; ++a) {
        if (!axisExtendable(*m_p, a)) continue;
        MeshAxis &ax = m_p->mesh(a);
        const int n = ax.nodes.size();
        const double dLo = (ax.nodes[1] - ax.nodes[0]) / ax.divs.first();
        const double dHi = (ax.nodes[n - 1] - ax.nodes[n - 2]) / ax.divs.last();
        ax.nodes.prepend(ax.nodes.first() - margin);
        ax.divs.prepend(ndiv(margin / dLo));
        ax.nodes.append(ax.nodes.last() + margin);
        ax.divs.append(ndiv(margin / dHi));
    }
    m_p->touch();
}

// ② 対策ボタンの表示更新: 層数は現在値→目標値で表示し、+λ/4 は実行できる
// ときだけ有効化してツールチップに実際の追加量を示す
void VerificationTab::refreshPmlButtons()
{
    const GeneralOpts &g = m_p->general();
    const int target = (g.pmlL >= 12) ? g.pmlL + 4 : 12;
    m_pmlBtn1->setText(I18n::tr("ver_pml_btn1_fmt").arg(g.pmlL).arg(target));
    m_pmlBtn1->setToolTip(I18n::tr("ver_pml_btn1_tip"));

    const bool ok = canAddMargin(*m_p);
    m_pmlBtn2->setEnabled(ok);
    m_pmlBtn2->setToolTip(ok
        ? I18n::tr("ver_pml_btn2_tip")
              .arg(formatFreq(centerFreq1(g)))
              .arg(QStringLiteral("%1 m")
                       .arg(quarterWaveMargin(*m_p), 0, 'g', 3))
        : I18n::tr("ver_pml_margin_na"));
}

// ── ① メッシュ解像度の計画値 ───────────────────────────────────────────────
void VerificationTab::updateMeshTable()
{
    const double lambda = wavelength(*m_p);
    const verify::Grid grid = gridFromProject(*m_p);
    const std::vector<double> factors(std::begin(kRefineFactors),
                                      std::end(kRefineFactors));
    const std::vector<verify::MeshLevel> levels =
        verify::meshConvergenceLevels(grid, lambda, factors);

    const QString dash = I18n::tr("ver_dash");
    const int rows = int(levels.size());
    m_meshTbl->setRowCount(std::max(rows, 1));
    if (levels.empty()) {
        // メッシュが不正 — 数字を作らない
        for (int c = 0; c < 6; ++c) {
            if (!m_meshTbl->item(0, c))
                m_meshTbl->setItem(0, c, new QTableWidgetItem());
            m_meshTbl->item(0, c)->setText(dash);
        }
        m_meshStatus->setText(I18n::tr("ver_mesh_res_na"));
        styleBadge(m_meshStatus, "warn");
        return;
    }

    for (int r = 0; r < rows; ++r) {
        const verify::MeshLevel &lv = levels[size_t(r)];
        const bool cur = std::fabs(lv.refine - 1.0) < 1e-9;
        for (int c = 0; c < 6; ++c)
            if (!m_meshTbl->item(r, c))
                m_meshTbl->setItem(r, c, new QTableWidgetItem());

        QString label = QStringLiteral("×%1").arg(lv.refine, 0, 'f', 2);
        if (cur) label += QStringLiteral(" (%1)").arg(I18n::tr("ver_mesh_cur"));
        m_meshTbl->item(r, 0)->setText(label);
        m_meshTbl->item(r, 1)->setText(formatCells(lv.cells));
        m_meshTbl->item(r, 2)->setText(lv.lambdaOverDx > 0
            ? QStringLiteral("%1").arg(lv.lambdaOverDx, 0, 'f', 1) : dash);
        m_meshTbl->item(r, 3)->setText(formatMemory(lv.memoryMB));
        // 結果・誤差はソルバーを各解像度で走らせないと得られない (未実行)
        for (int c = 4; c < 6; ++c) {
            m_meshTbl->item(r, c)->setText(dash);
            m_meshTbl->item(r, c)->setForeground(QColor(128, 128, 128));
        }
        const QColor bg = cur ? QColor(0, 120, 212, 36) : QColor(0, 0, 0, 0);
        for (int c = 0; c < 6; ++c)
            m_meshTbl->item(r, c)->setBackground(bg);
    }

    // 現在メッシュの λ/Δx をバッジで要約する
    const verify::MeshLevel *cur = nullptr;
    for (const verify::MeshLevel &lv : levels)
        if (std::fabs(lv.refine - 1.0) < 1e-9) cur = &lv;
    if (cur && cur->lambdaOverDx > 0) {
        m_meshStatus->setText(I18n::tr("ver_mesh_res_fmt")
            .arg(cur->lambdaOverDx, 0, 'f', 1)
            .arg(formatLength(cur->dxMax_m))
            .arg(formatLength(lambda))
            .arg(formatFreq(centerFreq1(m_p->general()))));
        styleBadge(m_meshStatus, verdictKind(
            verify::resolutionVerdict(cur->lambdaOverDx)));
    } else {
        m_meshStatus->setText(I18n::tr("ver_mesh_res_na"));
        styleBadge(m_meshStatus, "warn");
    }
}

// ── ① 自動収束テスト (各解像度で実際に走らせる) ────────────────────────────
// 表の「結果」「誤差」列はソルバーを各解像度で走らせないと埋まらない。
// kernel/SweepRunner に倍率の列を渡し、1 点ずつ順に走らせて埋める。
// 誤差は **最も細かい解像度を基準** にした差 (収束の見方はこれが標準)。
void VerificationTab::startMeshConvergence()
{
    if (m_meshSweep && m_meshSweep->isRunning()) {   // 実行中の押下は中止
        m_meshSweep->stop();
        return;
    }
    // 水中 (BELLHOP) は FDTD のメッシュ概念が無いのでこの画面自体を出さない
    if (m_p->activeDomain() == Domain::Underwater) return;

    if (!m_meshSweep) {
        m_meshSweep = new SweepRunner(this);
        connect(m_meshSweep, &SweepRunner::logLine, this,
                &VerificationTab::sweepLog);
        connect(m_meshSweep, &SweepRunner::pointStarted, this,
                [this](int i, int n, const QString &label) {
            m_meshStatus->setText(
                I18n::tr("ver_mesh_running").arg(i + 1).arg(n).arg(label));
            styleBadge(m_meshStatus, "warn");
        });
        connect(m_meshSweep, &SweepRunner::pointFinished, this,
                [this](int i, const SweepResult &r) { fillMeshResult(i, r); });
        connect(m_meshSweep, &SweepRunner::finished, this,
                &VerificationTab::finishMeshConvergence);
    }

    SweepConfig cfg;
    cfg.kind = SweepKind::MeshRefine;
    cfg.values.clear();
    for (const double f : kRefineFactors) cfg.values.push_back(f);
    cfg.run = m_runCfg;
    cfg.run.mode = RunMode::Both;      // 給電点表はソルバー段が書くが、
                                       // ポストまで通して他の図も残す
    cfg.run.kernel = Runner::kernelForProject(*m_p);

    m_meshValues.clear();
    // QVector::assign は Qt 6.6+ — CI Linux の下限は 6.4.2 なので fill を使う
    m_meshQty.fill(std::numeric_limits<double>::quiet_NaN(),
                   int(std::size(kRefineFactors)));
    // 走らせる前に結果列を「実行中」に戻す (前回の値を残さない)
    const QString dash = I18n::tr("ver_dash");
    for (int r = 0; r < m_meshTbl->rowCount(); ++r)
        for (int c = 4; c < 6; ++c)
            if (m_meshTbl->item(r, c)) m_meshTbl->item(r, c)->setText(dash);

    if (!m_meshSweep->start(*m_p, cfg)) {
        m_meshStatus->setText(I18n::tr("ver_mesh_run_failed"));
        styleBadge(m_meshStatus, "err");
        return;
    }
    m_meshRunBtn->setText(I18n::tr("ver_mesh_stop"));
}

// 1 点ぶんの結果を表へ入れる。取れなかった量は数字を作らず理由を出す。
void VerificationTab::fillMeshResult(int row, const SweepResult &r)
{
    m_meshValues.push_back(r.value);
    if (row < 0 || row >= m_meshTbl->rowCount()) return;
    for (int c = 4; c < 6; ++c)
        if (!m_meshTbl->item(row, c))
            m_meshTbl->setItem(row, c, new QTableWidgetItem());

    double qty = std::numeric_limits<double>::quiet_NaN();
    if (!r.ok) {
        m_meshTbl->item(row, 4)->setText(I18n::tr("ver_mesh_failed"));
    } else if (SweepRunner::refDbNear(r.feeds, centerFreq1(m_p->general()),
                                      &qty)) {
        m_meshTbl->item(row, 4)->setText(
            QStringLiteral("%1 dB").arg(qty, 0, 'f', 3));
    } else {
        // 給電点が無い / frequency1 が無いと表が出ない — 事実をそのまま出す
        m_meshTbl->item(row, 4)->setText(I18n::tr("ver_mesh_noqty"));
    }
    if (row < m_meshQty.size()) m_meshQty[row] = qty;
    m_meshTbl->item(row, 4)->setForeground(QColor(0, 0, 0));
    m_meshTbl->item(row, 5)->setText(I18n::tr("ver_dash"));
}

void VerificationTab::finishMeshConvergence(bool ok)
{
    m_meshRunBtn->setText(I18n::tr("ver_mesh_run"));

    // 誤差列 = 最も細かい解像度 (倍率最大) の値との差 [dB]。
    // 基準が取れていなければ誤差は出さない (差の意味が無い)。
    int refRow = -1;
    double refVal = 0.0, refFactor = 0.0;
    for (int i = 0; i < m_meshQty.size() && i < m_meshValues.size(); ++i) {
        if (!std::isfinite(m_meshQty[i])) continue;
        if (refRow < 0 || m_meshValues[i] > refFactor) {
            refRow = i;
            refFactor = m_meshValues[i];
            refVal = m_meshQty[i];
        }
    }
    int converged = 0, usable = 0;
    if (refRow >= 0) {
        for (int i = 0; i < m_meshQty.size(); ++i) {
            if (i >= m_meshTbl->rowCount() || !m_meshTbl->item(i, 5)) continue;
            if (!std::isfinite(m_meshQty[i])) continue;
            ++usable;
            const double d = m_meshQty[i] - refVal;
            m_meshTbl->item(i, 5)->setText(
                (i == refRow) ? I18n::tr("ver_mesh_ref")
                              : QStringLiteral("%1%2 dB")
                                    .arg(d >= 0 ? QStringLiteral("+")
                                                : QString())
                                    .arg(d, 0, 'f', 3));
            if (std::fabs(d) <= kConvergedTol_dB) ++converged;
        }
    }

    if (!ok) {
        m_meshStatus->setText(I18n::tr("ver_mesh_run_partial"));
        styleBadge(m_meshStatus, "warn");
    } else if (usable < 2) {
        m_meshStatus->setText(I18n::tr("ver_mesh_noqty_all"));
        styleBadge(m_meshStatus, "warn");
    } else {
        m_meshStatus->setText(I18n::tr("ver_mesh_conv_fmt")
                                  .arg(converged).arg(usable)
                                  .arg(kConvergedTol_dB, 0, 'g', 2));
        styleBadge(m_meshStatus, converged == usable ? "ok" : "warn");
    }
}

// ── ② 吸収境界の設計反射率 ─────────────────────────────────────────────────
void VerificationTab::updateBoundaryTable()
{
    const GeneralOpts &g = m_p->general();
    const bool pml = (g.abc == 1);

    for (int r = 0; r < 5; ++r) {
        const double th = kAngles[r];
        const double ratio = pml ? verify::pmlDesignReflection(g.pmlR0, th)
                                 : verify::murDesignReflection(th);
        const double db = verify::toDb(ratio);
        for (int c = 0; c < 3; ++c)
            if (!m_pmlTbl->item(r, c))
                m_pmlTbl->setItem(r, c, new QTableWidgetItem());
        m_pmlTbl->item(r, 0)->setText(QStringLiteral("%1°").arg(th, 0, 'f', 0));
        // 反射が実質ゼロ (垂直入射の Mur) はダイナミックレンジ外として示す
        m_pmlTbl->item(r, 1)->setText(db <= -300.0
            ? QStringLiteral("< -300 dB")
            : QStringLiteral("%1 dB").arg(db, 0, 'f', 1));
        m_pmlTbl->item(r, 2)->setText(I18n::tr("ver_meas_na"));
        m_pmlTbl->item(r, 2)->setForeground(QColor(128, 128, 128));
    }

    m_pmlNote->setText(pml
        ? I18n::tr("ver_pml_design_note")
              .arg(g.pmlL)
              .arg(g.pmlM, 0, 'g', 3)
              .arg(g.pmlR0, 0, 'g', 3)
        : I18n::tr("ver_mur_design_note"));
}

// ── ③ 収束履歴 (実行ログ) ──────────────────────────────────────────────────
void VerificationTab::reloadRunLog()
{
    m_history.clear();
    m_logPath.clear();
    m_logName = runLogName(Runner::kernelForProject(*m_p));

    if (!m_logName.isEmpty()) {
        RunConfig cfg;   // 既定 = プロジェクトのあるディレクトリ
        const QString dir = Runner::resolveWorkingDir(m_p, cfg);
        if (!dir.isEmpty()) {
            const QString path = QDir(dir).filePath(m_logName);
            QFile f(path);
            if (f.exists() && f.open(QIODevice::ReadOnly)) {
                // 巨大ログで GUI を止めないよう読み込み量を上限で切る
                const QByteArray raw = f.read(kMaxLogBytes);
                m_history = verify::parseConvergenceLog(
                    std::string(raw.constData(), size_t(raw.size())));
                if (!m_history.empty()) m_logPath = path;
            }
        }
    }
    updateEnergyPlot();
    updateDiagnostics();
}

void VerificationTab::updateEnergyPlot()
{
    const double thr = m_p->general().converg;

    if (m_history.empty()) {
        // 空のプロットは軸だけが無意味な値で残る — グラフごと隠して
        // 「まだ何も無い」ことを文言だけで伝える
        m_energyPlot->setSeries({});
        m_energyPlot->clearYRange();
        m_energyPlot->setVisible(false);
        m_timeBadge->setText(I18n::tr("ver_time_none_fmt")
            .arg(m_logName.isEmpty() ? I18n::tr("ver_dash") : m_logName));
        styleBadge(m_timeBadge, "");
        m_timeSource->clear();
        return;
    }

    MiniSeries se, sh, st;
    se.color = QColor("#0078D4");
    se.label = "log10 |E|";
    sh.color = QColor("#2E8B57");
    sh.label = "log10 |H|";
    st.color = QColor("#9D5D00");
    st.dashed = true;
    st.label = I18n::tr("ver_time_thresh");

    // 対数表示。0 や負値 (ログの丸め) は下限でクリップする
    const auto lg = [](double v) { return std::log10(std::max(v, 1e-30)); };
    for (const verify::ConvergencePoint &p : m_history) {
        se.pts.append(QPointF(double(p.step), lg(p.e)));
        sh.pts.append(QPointF(double(p.step), lg(p.h)));
    }
    if (thr > 0) {
        st.pts.append(QPointF(double(m_history.front().step), lg(thr)));
        st.pts.append(QPointF(double(m_history.back().step), lg(thr)));
    }

    QVector<MiniSeries> series{ se, sh };
    if (!st.pts.isEmpty()) series.append(st);
    m_energyPlot->setSeries(series);
    m_energyPlot->clearYRange();
    m_energyPlot->setVisible(true);

    const verify::ConvergencePoint &last = m_history.back();
    const verify::Verdict v = verify::convergenceVerdict(m_history, thr);
    m_timeBadge->setText((v == verify::Verdict::Ok ? I18n::tr("ver_time_ok_fmt")
                                                   : I18n::tr("ver_time_warn_fmt"))
        .arg(last.e, 0, 'g', 3).arg(last.h, 0, 'g', 3)
        .arg(thr, 0, 'g', 3).arg(QString::number(qlonglong(last.step))));
    styleBadge(m_timeBadge, verdictKind(v));
    m_timeSource->setText(I18n::tr("ver_time_src_fmt")
        .arg(m_logPath).arg(int(m_history.size())));
}

// ── 自動診断 (すべてプロジェクト設定 or 実行ログからの実判定) ───────────────
void VerificationTab::updateDiagnostics()
{
    const GeneralOpts &g = m_p->general();
    const Domain d = m_p->activeDomain();
    const double speed = domainWaveSpeed(d);
    const double lambda = wavelength(*m_p);
    const verify::Grid grid = gridFromProject(*m_p);

    const auto setRow = [&](int row, verify::Verdict v, const QString &note,
                            const QString &badgeOverride = QString()) {
        if (row < 0 || row >= DiagRowCount) return;
        if (m_diagBadge[row]) {
            m_diagBadge[row]->setText(badgeOverride.isEmpty() ? verdictText(v)
                                                              : badgeOverride);
            styleBadge(m_diagBadge[row],
                       badgeOverride.isEmpty() ? verdictKind(v) : "acc");
        }
        if (auto *it = m_diag->item(row, 2)) it->setText(note);
    };

    // 行0: λ/Δx (分解能)
    double dxMax = 0.0;
    for (int a = 0; a < 3; ++a)
        if (grid.axis[a].cells > 0) dxMax = std::max(dxMax, grid.axis[a].dxMax_m);
    if (lambda > 0 && dxMax > 0) {
        const double res = lambda / dxMax;
        setRow(DiagResolution, verify::resolutionVerdict(res),
               I18n::tr("ver_n_res_fmt").arg(res, 0, 'f', 1)
                   .arg(formatLength(dxMax)).arg(formatLength(lambda)));
    } else {
        setRow(DiagResolution, verify::Verdict::Unknown, I18n::tr("ver_n_res_na"));
    }

    // 行1: CFL 安定条件。dt = 0 はカーネルが安定 Δt を自動決定する
    // (OpenFDTD setup())。GUI 側で検算していないので「自動」と明示し、
    // OK を捏造しない。
    if (g.dt > 0) {
        double dxMin[3] = { 0, 0, 0 };
        bool okGrid = true;
        for (int a = 0; a < 3; ++a) {
            dxMin[a] = grid.axis[a].dxMin_m;
            if (!(dxMin[a] > 0)) okGrid = false;
        }
        const double S = okGrid ? verify::courantNumber(g.dt, speed, dxMin) : 0.0;
        setRow(DiagCourant, verify::courantVerdict(S),
               S > 0 ? I18n::tr("ver_n_cfl_fmt").arg(S, 0, 'f', 3)
                           .arg(QStringLiteral("%1 s").arg(g.dt, 0, 'g', 3))
                           .arg(speed, 0, 'g', 4)
                     : I18n::tr("ver_n_res_na"));
    } else {
        setRow(DiagCourant, verify::Verdict::Unknown, I18n::tr("ver_n_cfl_auto"),
               I18n::tr("ver_v_auto"));
    }

    // 行2: 吸収境界の設定
    const bool pml = (g.abc == 1);
    if (pml) {
        const double db0 = verify::toDb(verify::pmlDesignReflection(g.pmlR0, 0.0));
        setRow(DiagBoundary, verify::absorbingBoundaryVerdict(true, g.pmlL),
               I18n::tr("ver_n_abc_pml_fmt").arg(g.pmlL)
                   .arg(g.pmlM, 0, 'g', 3).arg(g.pmlR0, 0, 'g', 3)
                   .arg(QStringLiteral("%1 dB").arg(db0, 0, 'f', 1)));
    } else {
        setRow(DiagBoundary, verify::absorbingBoundaryVerdict(false, g.pmlL),
               I18n::tr("ver_n_abc_mur"));
    }

    // 行3: 収束 (シャットオフ) 到達 — 実行ログが無ければ未判定
    if (m_history.empty()) {
        setRow(DiagConverged, verify::Verdict::Unknown,
               I18n::tr("ver_n_conv_none")
                   .arg(m_logName.isEmpty() ? I18n::tr("ver_dash") : m_logName));
    } else {
        const verify::ConvergencePoint &last = m_history.back();
        setRow(DiagConverged, verify::convergenceVerdict(m_history, g.converg),
               I18n::tr("ver_n_conv_fmt")
                   .arg(QString::number(qlonglong(last.step)))
                   .arg(last.e, 0, 'g', 3).arg(last.h, 0, 'g', 3)
                   .arg(g.converg, 0, 'g', 3));
    }

    // 解析領域 (メッシュの外形)
    double meshLo[3], meshHi[3];
    bool meshOk = true;
    for (int a = 0; a < 3; ++a) {
        meshLo[a] = m_p->mesh(a).min();
        meshHi[a] = m_p->mesh(a).max();
        if (!(meshHi[a] > meshLo[a])) meshOk = false;
    }

    // 行4: 観測点が解析領域内か
    const QVector<Probe> &probes = m_p->probes();
    if (probes.isEmpty() || !meshOk) {
        setRow(DiagMonitorInside, verify::Verdict::Unknown,
               I18n::tr("ver_n_mon_none"));
    } else {
        int outside = 0;
        for (const Probe &pr : probes) {
            const double pt[3] = { pr.x, pr.y, pr.z };
            for (int a = 0; a < 3; ++a)
                if (pt[a] < meshLo[a] || pt[a] > meshHi[a]) { ++outside; break; }
        }
        setRow(DiagMonitorInside,
               outside == 0 ? verify::Verdict::Ok : verify::Verdict::Ng,
               outside == 0
                   ? I18n::tr("ver_n_mon_ok").arg(probes.size())
                   : I18n::tr("ver_n_mon_out").arg(probes.size()).arg(outside));
    }

    // 行5: 波源 (給電点) と観測点の距離
    const QVector<Feed> &feeds = m_p->feeds();
    if (feeds.isEmpty() && m_p->planewave().enabled) {
        setRow(DiagSeparation, verify::Verdict::Unknown, I18n::tr("ver_n_sep_pw"));
    } else if (feeds.isEmpty() || probes.isEmpty() || !(lambda > 0)) {
        setRow(DiagSeparation, verify::Verdict::Unknown, I18n::tr("ver_n_sep_none"));
    } else {
        double dmin = 1e308;
        for (const Feed &f : feeds)
            for (const Probe &pr : probes) {
                const double dx = f.x - pr.x, dy = f.y - pr.y, dz = f.z - pr.z;
                dmin = std::min(dmin, std::sqrt(dx * dx + dy * dy + dz * dz));
            }
        const double rel = dmin / lambda;
        setRow(DiagSeparation, verify::separationVerdict(rel),
               I18n::tr("ver_n_sep_fmt").arg(formatLength(dmin))
                   .arg(rel, 0, 'f', 2));
    }

    // 行6: 形状と境界の余裕
    double geoLo[3] = { 0, 0, 0 }, geoHi[3] = { 0, 0, 0 };
    if (m_p->geometries().isEmpty()) {
        setRow(DiagMargin, verify::Verdict::Unknown, I18n::tr("ver_n_margin_none"));
    } else if (!meshOk || !(lambda > 0)) {
        setRow(DiagMargin, verify::Verdict::Unknown, I18n::tr("ver_n_res_na"));
    } else if (!geometryBounds(*m_p, geoLo, geoHi)) {
        setRow(DiagMargin, verify::Verdict::Unknown, I18n::tr("ver_n_margin_shape"));
    } else {
        double margin = 1e308;
        for (int a = 0; a < 3; ++a) {
            margin = std::min(margin, geoLo[a] - meshLo[a]);
            margin = std::min(margin, meshHi[a] - geoHi[a]);
        }
        const double rel = margin / lambda;
        setRow(DiagMargin, verify::marginVerdict(rel),
               I18n::tr("ver_n_margin_fmt").arg(formatLength(margin))
                   .arg(rel, 0, 'f', 2));
    }

    m_diagNote->setText(I18n::tr("ver_diag_note")
        .arg(speed, 0, 'g', 4).arg(waveSpeedNoteKey(d)));
}

// 設定から決まる表示をまとめて更新する (ファイル読み込みは含まない)
void VerificationTab::refreshComputed()
{
    updateMeshTable();
    updateBoundaryTable();
    updateDiagnostics();
    refreshPmlButtons();
}

void VerificationTab::refreshDomain()
{
    const Domain d = m_p->activeDomain();
    const bool uw = (d == Domain::Underwater);
    m_qtyBox->clear();
    m_qtyBox->addItem(QString::fromUtf8(meshQuantity(d)));

    // ② 境界吸収 / ③ 収束履歴は FDTD 系 (時間領域 + PML) 前提の画面。
    // 水中ドメイン (BELLHOP レイトレース) には対応概念が無い → セクションごと隠す
    m_pmlSection->setVisible(!uw);
    m_timeSection->setVisible(!uw);

    // ④ 比較ソルバ: ドメインごとの妥当な候補だけを見せる (選択のみ・未配線)
    m_crossBox->clear();
    switch (d) {
        case Domain::Optical:
            m_crossBox->addItems({ "RCWA", "STACK", "tidy3d (Cloud)" });
            break;
        case Domain::Acoustic:
            m_crossBox->addItems({ I18n::tr("ver_cross_ism"),
                                   I18n::tr("ver_cross_ray") });
            break;
        case Domain::Underwater:
            m_crossBox->addItems({ I18n::tr("ver_cross_pe"), "BEM" });
            break;
        default:   // EM
            m_crossBox->addItem("FEM (Frequency)");
            break;
    }

    // 自動診断: CFL・吸収境界・収束履歴・境界余裕は FDTD 系の項目で
    // 水中 (レイトレース) では無意味 → 行を隠す
    m_diag->setRowHidden(DiagCourant, uw);
    m_diag->setRowHidden(DiagBoundary, uw);
    m_diag->setRowHidden(DiagConverged, uw);
    m_diag->setRowHidden(DiagMargin, uw);

    // ドメインで伝搬速度 (波長) が変わるため数値も更新する
    refreshComputed();
}
