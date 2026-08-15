// MaterialExplorerTab.cpp
#include "MaterialExplorerTab.h"

#include "../widgets/RefractiveIndexDialog.h"
#include "../core/GlassCatalog.h"
#include "../core/Project.h"
#include "../optics/DispersionFit.h"
#include "../optics/MaterialDispersion.h"
#include "../widgets/MiniPlot.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"
#include "TabHelpers.h"

#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QCheckBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <cmath>

using namespace ofd;

// ── file-local i18n vocabulary (mex_) ───────────────────────────────────────
namespace {
const bool s_i18n = [] {
    ofd::I18n::reg("mex_section", "マテリアルエクスプローラ (Ansys Lumerical 相当)",
                   "Material Explorer (Ansys Lumerical equivalent)");
    ofd::I18n::reg("mex_hint",
        "金属・半導体・誘電体・2D材料のデータベース。誘電体 6 種 (SiO2/Si3N4/"
        "Al2O3/Si/TiO2/PMMA) と光学ガラスは公刊 Sellmeier 係数の実分散を内蔵し、"
        "分散モデルのフィットと診断 (残差・因果律・受動性) を実計算できます。"
        "それ以外は例示表示のみでフィットできません。実測の n,k テーブル "
        "(CSV / TSV) は「n,k 取込」で読み込めば、内蔵材と同じようにフィット・"
        "診断・物性値への追加ができます。",
        "Database of metals, semiconductors, dielectrics and 2D materials. "
        "Six dielectrics (SiO2/Si3N4/Al2O3/Si/TiO2/PMMA) and optical glasses "
        "carry real published Sellmeier dispersion and can be fitted and "
        "diagnosed (residual, causality, passivity) for real. The other entries "
        "are illustrative only and cannot be fitted. A measured n,k table "
        "(CSV / TSV) can be brought in with \"Import n,k\" and is then fitted, "
        "diagnosed and added just like the built-in materials."
        "implemented).");
    ofd::I18n::reg("mex_prev_real",
        "実 Sellmeier 分散 (公刊係数) — 有効範囲 %1 のみ描画。この範囲では k≈0",
        "Real Sellmeier dispersion (published coefficients) — drawn only "
        "within the valid range %1, where k≈0");
    ofd::I18n::reg("mex_prev_glass",
        "ガラスカタログの Sellmeier 実曲線 (k≈0)",
        "Real Sellmeier curve from the glass catalog (k≈0)");
    ofd::I18n::reg("mex_prev_fake",
        "⚠ 例示曲線 — この材料の実分散データは未内蔵 (追加ボタンは無効)",
        "⚠ Illustrative curve — no real dispersion data built in for this "
        "material (Add is disabled)");
    ofd::I18n::reg("mex_add_na_tip",
        "実分散データが無い材料は物性値に追加できません (誤った εr を"
        "書き込まないため)。実測値があれば「n,k 取込」で読み込んでください",
        "Materials without real dispersion data cannot be added (to avoid "
        "writing an incorrect εr). If you have measured data, bring it in with "
        "\"Import n,k\"");
    ofd::I18n::reg("mex_db_section",  "データベース", "Database");
    ofd::I18n::reg("mex_search_ph",   "🔎 材料を検索…", "🔎 Search materials…");
    ofd::I18n::reg("mex_import_nk",   "📁 n,k 取込", "📁 Import n,k");
    ofd::I18n::reg("mex_riinfo_tip",
              "公開の屈折率データベース (CC0 1.0) から n,k を取り込みます。"
              "押したときだけ通信します",
              "Import n,k from the public refractive index database (CC0 1.0). "
              "It only connects when you press a button");
    ofd::I18n::reg("mex_riinfo_ok", "%1 を取り込みました (%2 点)",
              "Imported %1 (%2 points)");
    ofd::I18n::reg("mex_why_temp",
              "温度依存の材料が .ofd にありません — 材料は "
              "material = type epsr esgm amur msgm だけで温度の概念が無く、"
              "表を作っても渡す先がありません",
              "temperature-dependent materials do not exist in .ofd — a "
              "material is only material = type epsr esgm amur msgm, with no "
              "notion of temperature, so a table would have nowhere to go");
    ofd::I18n::reg("mex_nk_unit",     "波長単位", "Wavelength unit");
    ofd::I18n::reg("mex_nk_auto",     "自動", "Auto");
    ofd::I18n::reg("mex_nk_dialog",   "実測 n,k テーブルを開く",
                   "Open a measured n,k table");
    ofd::I18n::reg("mex_nk_filter",
                   "n,k テーブル (*.csv *.txt *.tsv *.dat);;すべて (*)",
                   "n,k tables (*.csv *.txt *.tsv *.dat);;All files (*)");
    ofd::I18n::reg("mex_nk_fail",
                   "取込に失敗しました: %1 (数値の行が 2 行以上、"
                   "「波長, n」または「波長, n, k」の並びで必要です)",
                   "Import failed: %1 (at least two numeric rows are needed, "
                   "laid out as \"wavelength, n\" or \"wavelength, n, k\")");
    // 単位は推測することがあるので、**どう決めたかを必ず書く**
    ofd::I18n::reg("mex_nk_ok_head",
                   "%1 を取り込みました: %2 点、%3 〜 %4 nm。",
                   "Imported %1: %2 points, %3 to %4 nm.");
    ofd::I18n::reg("mex_nk_unit_hdr",
                   " 波長の単位はヘッダの表記から %1 と読みました。",
                   " The wavelength unit was read as %1 from the header.");
    ofd::I18n::reg("mex_nk_unit_guess",
                   " 波長の単位はヘッダに書かれていないため、値の桁から %1 と"
                   "解釈しました。違う場合は「波長単位」で指定して読み直して"
                   "ください。",
                   " The header does not name a wavelength unit, so %1 was "
                   "inferred from the magnitude of the values. If that is "
                   "wrong, pick the unit in \"Wavelength unit\" and import "
                   "again.");
    ofd::I18n::reg("mex_nk_unit_user",
                   " 波長の単位は指定どおり %1 として読みました。",
                   " The wavelength unit was taken as %1 as you specified.");
    ofd::I18n::reg("mex_nk_nok",
                   " k の列が無いので消衰係数は「データ無し」として扱います "
                   "(0 とは書きません)。",
                   " There is no k column, so the extinction coefficient is "
                   "treated as missing data (it is not recorded as 0).");
    ofd::I18n::reg("mex_nk_skipped",
                   " 読めなかった行が %1 行あり、読み飛ばしました。",
                   " %1 row(s) could not be read and were skipped.");
    ofd::I18n::reg("mex_nk_dup",
                   " 波長が重複した点が %1 点あり、最初の値を残しました。",
                   " %1 point(s) had a duplicate wavelength; the first value "
                   "was kept.");
    ofd::I18n::reg("mex_nk_group", "取込 / Imported", "Imported");
    ofd::I18n::reg("mex_prev_import",
                   "取り込んだ実測値です (%1)。曲線は測った点の線形補間で、"
                   "範囲外は外挿しません。",
                   "This is the measured data you imported (%1). The curve is "
                   "a linear interpolation of the measured points and is not "
                   "extrapolated beyond them.");
    ofd::I18n::reg("mex_nk_model", "実測", "Measured");
    ofd::I18n::reg("mex_riinfo",      "🌐 refractiveindex.info",
                                      "🌐 refractiveindex.info");
    ofd::I18n::reg("mex_selected",    "選択中: %1", "Selected: %1");
    ofd::I18n::reg("mex_db_model",    "DB モデル", "DB model");
    ofd::I18n::reg("mex_range_fmt",   "有効範囲 %1", "Valid range %1");
    ofd::I18n::reg("mex_fit_model",   "フィットモデル", "Fit model");
    ofd::I18n::reg("mex_model_sampled", "Sampled (補間)", "Sampled (interp.)");
    ofd::I18n::reg("mex_fit_range",   "フィット範囲", "Fit range");
    ofd::I18n::reg("mex_ncoef",       "係数の数 (max)", "Coefficients (max)");
    ofd::I18n::reg("mex_rms_tol",     "許容 RMS 誤差", "RMS error tolerance");
    ofd::I18n::reg("mex_iters",       "改善反復", "Improvement iterations");
    ofd::I18n::reg("mex_eps_inf",     "ε∞", "ε∞");
    ofd::I18n::reg("mex_wp",          "プラズマ周波数 ωp", "Plasma frequency ωp");
    ofd::I18n::reg("mex_gamma",       "衝突周波数 γ", "Collision frequency γ");
    ofd::I18n::reg("mex_w0",          "共鳴 ω0", "Resonance ω0");
    ofd::I18n::reg("mex_run_fit",     "▶ フィット実行", "▶ Run fit");
    ofd::I18n::reg("mex_badge_rms_na", "RMS誤差 未計算", "RMS error not computed");
    ofd::I18n::reg("mex_badge_rms",   "RMS誤差 %1", "RMS error %1");
    ofd::I18n::reg("mex_badge_causal_na", "因果律 未評価",
                                          "Causality not evaluated");
    ofd::I18n::reg("mex_badge_causal","因果律 OK", "Causality OK");
    ofd::I18n::reg("mex_badge_causal_ng", "因果律 違反あり",
                                          "Causality violated");
    ofd::I18n::reg("mex_badge_causal_skip", "因果律 評価対象外",
                                            "Causality out of scope");
    ofd::I18n::reg("mex_fit_section", "n, k フィット結果", "Fit quality");
    ofd::I18n::reg("mex_n_real",      "屈折率 n (実部)", "Refractive index n (real part)");
    ofd::I18n::reg("mex_k_imag",      "消衰係数 k (虚部)", "Extinction coefficient k (imag part)");
    ofd::I18n::reg("mex_fit_note",
        "── 実線は参照データ (公刊 Sellmeier 係数)。「▶ フィット実行」後は"
        "フィットモデルの曲線を重ねて描きます (フィット範囲のみ)。"
        "実分散データを持たない材料は例示曲線のみでフィットできません。",
        "── the solid curve is the reference data (published Sellmeier "
        "coefficients). After “▶ Run fit” the fitted model is overlaid over the "
        "fit range. Entries without real dispersion data show only an "
        "illustrative curve and cannot be fitted.");
    // ── フィット結果・診断 (実計算) ──
    ofd::I18n::reg("mex_fit_src",
        "参照データ: %1 の公刊 Sellmeier 係数を %2〜%3 nm で %4 点サンプル",
        "Reference data: published Sellmeier coefficients of %1, sampled at %4 "
        "points over %2-%3 nm");
    // 取り込んだ実測データは出所も点数も違う — 同じ文言を使い回すと
    // 「公刊 Sellmeier を 64 点サンプル」と嘘になる (ヘッドレス描画で発見)
    ofd::I18n::reg("mex_fit_src_import",
        "参照データ: 取り込んだ実測値 %1 の %2〜%3 nm にある %4 点 "
        "(測った点をそのまま使い、等間隔に引き直していません)",
        "Reference data: %4 measured point(s) of the imported file %1 between "
        "%2 and %3 nm (the measured points are used as they are, not "
        "resampled on a uniform grid)");
    ofd::I18n::reg("mex_fit_done",
        "フィット完了: 極 %1 個 · RMS 誤差 (n) = %2 · 最大誤差 %3",
        "Fit done: %1 pole(s), RMS error (n) = %2, max error %3");
    ofd::I18n::reg("mex_fit_interp",
        "Sampled (補間) はフィットではないため残差は定義上 0 です "
        "(参照データをそのまま使います)。",
        "Sampled (interpolation) is not a fit, so the residual is zero by "
        "definition (the reference data is used as is).");
    ofd::I18n::reg("mex_fit_nodata",
        "この材料は実分散データを内蔵していないためフィットできません "
        "(実測値があれば「n,k 取込」で読み込めばフィットできます)。",
        "This entry has no built-in real dispersion data, so it cannot be "
        "fitted (import a measured table with \"Import n,k\" to fit it).");
    ofd::I18n::reg("mex_fit_badrange",
        "フィット範囲 %1〜%2 nm が参照データの有効範囲 %3〜%4 nm と重なりません。",
        "The fit range %1-%2 nm does not overlap the validity range of the "
        "reference data (%3-%4 nm).");
    ofd::I18n::reg("mex_fit_failed",
        "最小二乗が解けませんでした (範囲・係数の数を見直してください)。",
        "The least-squares fit did not solve (check the range and the number "
        "of coefficients).");
    ofd::I18n::reg("mex_fit_clamped",
        "  ※ 指定範囲は有効範囲へ丸めました (%1〜%2 nm)。",
        "  (the requested range was clamped to the validity range: %1-%2 nm)");
    ofd::I18n::reg("mex_param_note",
        "▸ ε∞ / ωp / γ / ω0 はフィット結果の表示欄です (入力ではありません)。"
        "無損失の極モデルを当てるため γ は 0 になります。",
        "▸ ε∞ / ωp / γ / ω0 are read-outs of the fit, not inputs. The pole "
        "models fitted here are lossless, so γ is zero.");
    ofd::I18n::reg("mex_dash", "—", "—");
    ofd::I18n::reg("mex_diag_section","モデル診断", "Diagnostics");
    ofd::I18n::reg("mex_diag_item",   "項目", "Item");
    ofd::I18n::reg("mex_diag_value",  "値", "Value");
    ofd::I18n::reg("mex_diag_verdict","判定", "Verdict");
    ofd::I18n::reg("mex_diag_rms_n",  "RMS 誤差 (n)", "RMS error (n)");
    ofd::I18n::reg("mex_diag_rms_k",  "RMS 誤差 (k)", "RMS error (k)");
    ofd::I18n::reg("mex_diag_kk",     "因果律 (Kramers-Kronig)",
                                      "Causality (Kramers-Kronig)");
    ofd::I18n::reg("mex_diag_passive","受動性 (Im ε ≥ 0)", "Passivity (Im ε ≥ 0)");
    ofd::I18n::reg("mex_diag_stab",   "FDTD安定性", "FDTD stability");
    ofd::I18n::reg("mex_satisfied",   "満足", "satisfied");
    ofd::I18n::reg("mex_violated",    "違反", "violated");
    ofd::I18n::reg("mex_notcalc",     "未計算", "not computed");
    ofd::I18n::reg("mex_outofscope",  "評価対象外", "out of scope");
    ofd::I18n::reg("mex_cond_stable", "条件付安定", "conditionally stable");
    ofd::I18n::reg("mex_good",        "良好", "good");
    ofd::I18n::reg("mex_improve",     "要改善", "needs improvement");
    ofd::I18n::reg("mex_dt_limit",    "Δt制約あり", "Δt constrained");
    // 診断表の値欄 (すべて実計算 / 実データからの判定)
    ofd::I18n::reg("mex_v_rms_n",     "%1 (最大 %2)", "%1 (max %2)");
    ofd::I18n::reg("mex_v_nofit",     "フィット未実行", "fit not run");
    ofd::I18n::reg("mex_v_nok",
        "k データ無し (Sellmeier 係数は実部のみ)",
        "no k data (Sellmeier coefficients give the real part only)");
    ofd::I18n::reg("mex_v_kk_ok",
        "透明域の必要条件 dε/dω ≥ 0 を全 %1 区間で満足",
        "the transparency-window requirement dε/dω ≥ 0 holds on all %1 "
        "intervals");
    ofd::I18n::reg("mex_v_kk_ng",     "%1/%2 区間で dε/dω < 0",
                                      "dε/dω < 0 on %1 of %2 intervals");
    ofd::I18n::reg("mex_v_kk_na",
        "実 n,k データが無いため数値評価できません",
        "cannot be evaluated numerically without real n,k data");
    ofd::I18n::reg("mex_v_kk_absorb",
        "吸収域を含むため必要条件では判定できません (KK 積分が必要)",
        "the data covers an absorbing region, so the simple requirement does "
        "not apply (a KK integral is needed)");
    ofd::I18n::reg("mex_v_passive",   "ε∞ = %1 · 最小 Δε = %2",
                                      "ε∞ = %1, min Δε = %2");
    ofd::I18n::reg("mex_v_passive_drude", "ε∞ = %1 · λp = %2 µm",
                                          "ε∞ = %1, λp = %2 µm");
    ofd::I18n::reg("mex_v_nmin",      "フィット範囲で n_min = %1",
                                      "n_min = %1 over the fit range");
    ofd::I18n::reg("mex_diag_hint",
        "▸ 判定基準の出典は src/optics/DispersionFit.h に記載。RMS 誤差が"
        "許容値を超えるときは係数の数を増やすか、フィット範囲を狭めてください。"
        "n_min < 1 の帯域があるモデルは真空基準の Courant 条件では不足します。",
        "▸ Sources for the criteria are listed in src/optics/DispersionFit.h. "
        "If the RMS error exceeds the tolerance, add coefficients or narrow the "
        "fit range. A model with n_min < 1 needs a stricter Courant limit than "
        "the vacuum one.");
    ofd::I18n::reg("mex_apply_section","材料の利用", "Apply");
    ofd::I18n::reg("mex_add_material", "この材料を物性値リストに追加",
                                       "Add this material to the material list");
    ofd::I18n::reg("mex_temp_table",   "温度依存テーブル", "Temperature-dependent table");
    ofd::I18n::reg("mex_aniso",        "異方性テンソル ε[ij]", "Anisotropic tensor ε[ij]");
    ofd::I18n::reg("mex_nonlinear",    "非線形 χ⁽²⁾/χ⁽³⁾ 付与", "Nonlinear χ⁽²⁾/χ⁽³⁾");
    ofd::I18n::reg("mex_gain",         "利得媒質 (4準位)", "Gain medium (4-level)");
    ofd::I18n::reg("mex_magnetic",     "磁性 (μr≠1)", "Magnetic (μr≠1)");
    I18n::reg("mex_uw_flags", "非線形・利得・磁性のチェック",
              "the nonlinear / gain / magnetic check boxes");
    return true;
}();

// 内蔵データベース (Lumerical の material database グルーピングを踏襲, mock 同値)
struct DbItem { const char *id, *name, *model, *range; };
struct DbGroup { const char *grp; std::initializer_list<DbItem> items; };
const DbGroup kDb[] = {
    { "金属 / Metals", {
        { "Au_JC",   "Au (Johnson & Christy)", "Multi-coefficient", "0.2–2 μm" },
        { "Au_Palik","Au (Palik)",             "Sampled 3D",        "0.1–10 μm" },
        { "Ag_JC",   "Ag (Johnson & Christy)", "Multi-coefficient", "0.2–2 μm" },
        { "Al",      "Al (Palik)",             "Multi-coefficient", "0.1–10 μm" },
        { "Cu",      "Cu (CRC)",               "Drude+Lorentz",     "0.2–5 μm" },
        { "W",       "W (Tungsten)",           "Sampled",           "0.3–25 μm" },
    } },
    { "半導体 / Semiconductors", {
        // Si は Salzberg-Villa の Sellmeier を内蔵 (赤外の透明域)
        { "Si",      "Si (Salzberg-Villa)",    "Sellmeier",         "1.36–11 μm" },
        { "Si_pala", "Si (Palik)",             "Sampled",           "0.1–333 μm" },
        { "GaAs",    "GaAs (Palik)",           "Multi-coefficient", "0.2–15 μm" },
        { "Ge",      "Ge (CRC)",               "Sampled",           "0.2–14 μm" },
        { "InP",     "InP",                    "Sellmeier",         "0.95–10 μm" },
    } },
    { "誘電体 / Dielectrics", {
        { "SiO2",    "SiO2 (Malitson)",        "Sellmeier",         "0.21–3.71 μm" },
        { "Si3N4",   "Si3N4 (Luke)",           "Sellmeier",         "0.31–5.5 μm" },
        { "Al2O3",   "Al2O3 (Malitson)",       "Sellmeier",         "0.2–5 μm" },
        { "TiO2",    "TiO2 (DeVore)",          "Sellmeier",         "0.43–1.5 μm" },
        { "PMMA",    "PMMA (Sultanova)",       "Sellmeier",         "0.44–1.05 μm" },
    } },
    { "2D材料 / 2D & emerging", {
        { "graphene","Graphene (surface cond.)","Kubo",             "THz–IR" },
        { "hBN",     "h-BN",                   "Sampled",           "vis–IR" },
        { "VO2",     "VO2 (phase change)",     "Sampled (T依存)",   "vis–IR" },
        { "ITO",     "ITO (epsilon-near-zero)","Drude",             "vis–IR" },
    } },
};

// ── 実分散データ ────────────────────────────────────────────────────────────
// 公刊 Sellmeier 係数のテーブルは src/optics/MaterialDispersion へ抽出済み
// (Qt 非依存の共有モジュール。モードソルバ FDE 等からも同じ値を使う)。
// ここでは kDb の id で共有テーブルを引くだけ。
// 有効範囲外は評価しない (範囲外へ外挿した「それらしい値」を出さない)。
const optics::MaterialInfo *realNk(const QString &id)
{
    return optics::findMaterial(id.toUtf8().constData());
}

// 光学ガラスの参照範囲 [µm]。GlassCatalog は銘柄ごとの有効範囲を持たないので、
// ツリーに表示しているのと同じ可視〜近赤外 (0.4–1.6 µm) を使う。
const double kGlassLoUm = 0.4;
const double kGlassHiUm = 1.6;

// フィットの参照点数 (最小二乗の点数。極 6 個でも十分に優決定)
const int kFitSamples = 64;

QLabel *makeBadge(const QString &text, const char *color, QWidget *parent);

QLabel *makeBadge(const QString &text, const char *color, QWidget *parent)
{
    auto *b = new QLabel(text, parent);
    b->setStyleSheet(QStringLiteral(
        "border:1px solid %1; color:%1; border-radius:3px;"
        "padding:1px 6px; font-size:11px;").arg(QString::fromUtf8(color)));
    return b;
}
} // namespace

// ── MaterialExplorerTab ─────────────────────────────────────────────────────
MaterialExplorerTab::MaterialExplorerTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // 見出し + 説明
    auto *sTop = new SectionBox(I18n::tr("mex_section"), body);
    auto *hint = new QLabel(I18n::tr("mex_hint"), sTop);
    hint->setWordWrap(true);
    sTop->vbox()->addWidget(hint);
    v->addWidget(sTop);

    // 左: DBツリー / 右: フィットパネル
    auto *mid = new QHBoxLayout();
    mid->setSpacing(14);

    auto *left = new QWidget(body);
    left->setFixedWidth(280);
    auto *lv = new QVBoxLayout(left);
    lv->setContentsMargins(0, 0, 0, 0);
    auto *sDb = new SectionBox(I18n::tr("mex_db_section"), left);
    m_search = new QLineEdit(sDb);
    m_search->setPlaceholderText(I18n::tr("mex_search_ph"));
    sDb->vbox()->addWidget(m_search);
    m_tree = new QTreeWidget(sDb);
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(false);
    m_tree->setMaximumHeight(320);
    sDb->vbox()->addWidget(m_tree);
    auto *dbBtns = new QHBoxLayout();
    auto *impNk = new QPushButton(I18n::tr("mex_import_nk"), sDb);
    auto *riBtn = new QPushButton(I18n::tr("mex_riinfo"), sDb);
    connect(impNk, &QPushButton::clicked, this,
            &MaterialExplorerTab::importNk);
    // 実装済み: 公開データベース (CC0) から n,k を取り込む。通信は
    // ダイアログの中で利用者が押したときだけ行う (io/RefractiveIndexDb)
    riBtn->setToolTip(I18n::tr("mex_riinfo_tip"));
    connect(riBtn, &QPushButton::clicked, this,
            &MaterialExplorerTab::importFromRiInfo);
    dbBtns->addWidget(impNk);
    dbBtns->addWidget(riBtn);
    dbBtns->addStretch(1);
    sDb->vbox()->addLayout(dbBtns);
    // 波長の単位。既定は「自動」(ヘッダの単位語 → 無ければ値の桁) で、
    // 推測になったときは画面にそう書く。ここで明示すれば推測しない。
    // ボタンと同じ行に置くと横幅が足りずラベルが切れるので行を分ける。
    auto *unitRow = new QHBoxLayout();
    unitRow->addWidget(new QLabel(I18n::tr("mex_nk_unit"), sDb));
    m_nkUnit = new QComboBox(sDb);
    m_nkUnit->addItems({ I18n::tr("mex_nk_auto"), QStringLiteral("nm"),
                         QStringLiteral("um"), QStringLiteral("m") });
    m_nkUnit->setMaximumWidth(90);
    unitRow->addWidget(m_nkUnit);
    unitRow->addStretch(1);
    sDb->vbox()->addLayout(unitRow);
    lv->addWidget(sDb);
    lv->addStretch(1);
    mid->addWidget(left);

    auto *right = new QVBoxLayout();
    right->setSpacing(8);

    // 選択中材料 + フィット設定
    m_selSection = new SectionBox(QString(), body);
    auto *modelRow = new QHBoxLayout();
    m_modelBadge = makeBadge(QString(), "#B83280", m_selSection);
    m_rangeLabel = new QLabel(m_selSection);
    m_rangeLabel->setStyleSheet("color:gray; font-size:11px;");
    modelRow->addWidget(m_modelBadge);
    modelRow->addWidget(m_rangeLabel);
    modelRow->addStretch(1);
    m_selSection->form()->addRow(I18n::tr("mex_db_model"), modelRow);

    m_fitModel = new QComboBox(m_selSection);
    m_fitModel->addItem("Multi-coefficient");
    m_fitModel->addItem("Drude");
    m_fitModel->addItem("Lorentz");
    m_fitModel->addItem(I18n::tr("mex_model_sampled"));
    m_selSection->form()->addRow(I18n::tr("mex_fit_model"), m_fitModel);

    auto *rangeRow = new QHBoxLayout();
    m_fitMin = new QLineEdit("1500", m_selSection);
    m_fitMin->setMaximumWidth(60);
    m_fitMax = new QLineEdit("1600", m_selSection);
    m_fitMax->setMaximumWidth(60);
    rangeRow->addWidget(m_fitMin);
    rangeRow->addWidget(new QLabel(QString::fromUtf8("〜"), m_selSection));
    rangeRow->addWidget(m_fitMax);
    rangeRow->addWidget(new QLabel("nm", m_selSection));
    rangeRow->addStretch(1);
    m_selSection->form()->addRow(I18n::tr("mex_fit_range"), rangeRow);

    // フィットモデル別パラメータ (mock の条件分岐を QStackedWidget で再現)
    m_modelStack = new QStackedWidget(m_selSection);
    {   // [0] Multi-coefficient
        auto *page = new QWidget(m_modelStack);
        auto *f = new QFormLayout(page);
        f->setContentsMargins(0, 0, 0, 0);
        m_nCoef  = new QLineEdit("6", page);   m_nCoef->setMaximumWidth(60);
        m_rmsTol = new QLineEdit("0.1", page); m_rmsTol->setMaximumWidth(60);
        m_iters  = new QLineEdit("10", page);  m_iters->setMaximumWidth(60);
        f->addRow(I18n::tr("mex_ncoef"), m_nCoef);
        f->addRow(I18n::tr("mex_rms_tol"), m_rmsTol);
        f->addRow(I18n::tr("mex_iters"), m_iters);
        m_modelStack->addWidget(page);
    }
    {   // [1] Drude
        auto *page = new QWidget(m_modelStack);
        auto *f = new QFormLayout(page);
        f->setContentsMargins(0, 0, 0, 0);
        m_epsInfD = new QLineEdit("1.0", page); m_epsInfD->setMaximumWidth(60);
        m_wpD    = new QLineEdit("1.37e16", page); m_wpD->setMaximumWidth(100);
        m_gammaD = new QLineEdit("1.07e14", page); m_gammaD->setMaximumWidth(100);
        f->addRow(I18n::tr("mex_eps_inf"), m_epsInfD);
        auto *wpRow = new QHBoxLayout();
        wpRow->addWidget(m_wpD);
        wpRow->addWidget(new QLabel("rad/s", page));
        wpRow->addStretch(1);
        f->addRow(I18n::tr("mex_wp"), wpRow);
        auto *gRow = new QHBoxLayout();
        gRow->addWidget(m_gammaD);
        gRow->addWidget(new QLabel("rad/s", page));
        gRow->addStretch(1);
        f->addRow(I18n::tr("mex_gamma"), gRow);
        m_modelStack->addWidget(page);
    }
    {   // [2] Lorentz (Drude と同じ + 共鳴 ω0)
        auto *page = new QWidget(m_modelStack);
        auto *f = new QFormLayout(page);
        f->setContentsMargins(0, 0, 0, 0);
        m_epsInfL = new QLineEdit("1.0", page); m_epsInfL->setMaximumWidth(60);
        m_wpL    = new QLineEdit("1.37e16", page); m_wpL->setMaximumWidth(100);
        m_gammaL = new QLineEdit("1.07e14", page); m_gammaL->setMaximumWidth(100);
        m_w0L    = new QLineEdit("0.0", page); m_w0L->setMaximumWidth(100);
        f->addRow(I18n::tr("mex_eps_inf"), m_epsInfL);
        auto *wpRow = new QHBoxLayout();
        wpRow->addWidget(m_wpL);
        wpRow->addWidget(new QLabel("rad/s", page));
        wpRow->addStretch(1);
        f->addRow(I18n::tr("mex_wp"), wpRow);
        auto *gRow = new QHBoxLayout();
        gRow->addWidget(m_gammaL);
        gRow->addWidget(new QLabel("rad/s", page));
        gRow->addStretch(1);
        f->addRow(I18n::tr("mex_gamma"), gRow);
        f->addRow(I18n::tr("mex_w0"), m_w0L);
        m_modelStack->addWidget(page);
    }
    m_modelStack->addWidget(new QWidget(m_modelStack));   // [3] Sampled (補間)
    m_selSection->form()->addRow(m_modelStack);

    // ε∞ / ωp / γ / ω0 はフィット結果の表示欄 (入力ではない)
    for (QLineEdit *e : { m_epsInfD, m_wpD, m_gammaD,
                          m_epsInfL, m_wpL, m_gammaL, m_w0L }) {
        e->setReadOnly(true);
        e->setText(I18n::tr("mex_dash"));
    }
    auto *paramNote = new QLabel(I18n::tr("mex_param_note"), m_selSection);
    paramNote->setWordWrap(true);
    paramNote->setStyleSheet("font-size:11px; color:palette(mid);");
    m_selSection->form()->addRow(paramNote);

    auto *fitRow = new QHBoxLayout();
    m_fitBtn = new QPushButton(I18n::tr("mex_run_fit"), m_selSection);
    m_fitBtn->setProperty("primary", true);
    fitRow->addWidget(m_fitBtn);
    fitRow->addStretch(1);
    m_badgeRms = makeBadge(I18n::tr("mex_badge_rms_na"), "#808080", m_selSection);
    m_badgeCausal = makeBadge(I18n::tr("mex_badge_causal_na"), "#808080",
                              m_selSection);
    fitRow->addWidget(m_badgeRms);
    fitRow->addWidget(m_badgeCausal);
    m_selSection->form()->addRow(fitRow);
    // フィットの参照データ・結果・エラーを常に文章で示す
    m_fitStatus = new QLabel(m_selSection);
    m_fitStatus->setWordWrap(true);
    m_fitStatus->setStyleSheet("font-size:11px;");
    m_selSection->form()->addRow(m_fitStatus);
    right->addWidget(m_selSection);

    // n, k フィット結果プロット
    auto *sFit = new SectionBox(I18n::tr("mex_fit_section"), body);
    sFit->vbox()->addWidget(new QLabel(I18n::tr("mex_n_real"), sFit));
    m_plotN = new MiniPlot(sFit);
    m_plotN->setLabels(QString::fromUtf8("λ [nm]"), "n");
    m_plotN->setMinimumHeight(90);
    sFit->vbox()->addWidget(m_plotN);
    sFit->vbox()->addWidget(new QLabel(I18n::tr("mex_k_imag"), sFit));
    m_plotK = new MiniPlot(sFit);
    m_plotK->setLabels(QString::fromUtf8("λ [nm]"), "k");
    m_plotK->setMinimumHeight(90);
    sFit->vbox()->addWidget(m_plotK);
    m_previewNote = new QLabel(sFit);
    m_previewNote->setWordWrap(true);
    m_previewNote->setStyleSheet("font-size:11px;");
    sFit->vbox()->addWidget(m_previewNote);
    auto *fitNote = new QLabel(I18n::tr("mex_fit_note"), sFit);
    fitNote->setWordWrap(true);
    sFit->vbox()->addWidget(fitNote);
    right->addWidget(sFit);
    right->addStretch(1);
    mid->addLayout(right, 1);
    v->addLayout(mid);

    // モデル診断 (フィット結果と参照データから実計算 — showFit が埋める)
    auto *sDiag = new SectionBox(I18n::tr("mex_diag_section"), body);
    m_diag = new QTableWidget(5, 3, sDiag);
    m_diag->setHorizontalHeaderLabels({ I18n::tr("mex_diag_item"),
        I18n::tr("mex_diag_value"), I18n::tr("mex_diag_verdict") });
    m_diag->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_diag->verticalHeader()->setVisible(false);
    m_diag->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_diag->setMinimumHeight(170);
    const char *kDiagItem[5] = { "mex_diag_rms_n", "mex_diag_rms_k",
                                 "mex_diag_kk", "mex_diag_passive",
                                 "mex_diag_stab" };
    for (int r = 0; r < 5; ++r) {
        m_diag->setItem(r, 0, new QTableWidgetItem(I18n::tr(kDiagItem[r])));
        m_diag->setItem(r, 1, new QTableWidgetItem(QString()));
        m_diag->setItem(r, 2, new QTableWidgetItem(QString()));
    }
    sDiag->vbox()->addWidget(m_diag);
    auto *diagHint = new QLabel(I18n::tr("mex_diag_hint"), sDiag);
    diagHint->setWordWrap(true);
    sDiag->vbox()->addWidget(diagHint);
    v->addWidget(sDiag);

    // 材料の利用
    auto *sApply = new SectionBox(I18n::tr("mex_apply_section"), body);
    auto *applyRow = new QHBoxLayout();
    m_addBtn = new QPushButton(I18n::tr("mex_add_material"), sApply);
    m_addBtn->setProperty("primary", true);
    applyRow->addWidget(m_addBtn);
    auto *tempBtn  = new QPushButton(I18n::tr("mex_temp_table"), sApply);
    auto *anisoBtn = new QPushButton(I18n::tr("mex_aniso"), sApply);
    // kData (データが同梱されていない) では浅い — データがあっても渡す先が無い。
    // .ofd の材料は material = type epsr esgm amur msgm だけで温度の概念が無い
    tabhelp::markNotImplemented(tempBtn, I18n::tr("mex_why_temp"));
    tabhelp::markNotImplemented(anisoBtn, I18n::tr(tabhelp::notimpl::kModel));   // 異方性テンソルは未配線
    applyRow->addWidget(tempBtn);
    applyRow->addWidget(anisoBtn);
    applyRow->addStretch(1);
    sApply->vbox()->addLayout(applyRow);
    auto *checkRow = new QHBoxLayout();
    checkRow->addWidget(new QCheckBox(I18n::tr("mex_nonlinear"), sApply));
    checkRow->addWidget(new QCheckBox(I18n::tr("mex_gain"), sApply));
    checkRow->addWidget(new QCheckBox(I18n::tr("mex_magnetic"), sApply));
    checkRow->addStretch(1);
    sApply->vbox()->addLayout(checkRow);
    // 非線形/利得/磁性チェックはどこにも読まれない
    sApply->vbox()->addWidget(tabhelp::unwiredNote(sApply, I18n::tr("mex_uw_flags")));
    v->addWidget(sApply);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    buildDatabase();

    connect(m_search, &QLineEdit::textChanged,
            this, &MaterialExplorerTab::filterTree);
    connect(m_tree, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem *cur, QTreeWidgetItem *) {
                if (!cur) return;
                const QVariant idx = cur->data(0, Qt::UserRole);
                if (idx.isValid()) showEntry(idx.toInt());
            });
    connect(m_fitModel, &QComboBox::currentIndexChanged,
            m_modelStack, &QStackedWidget::setCurrentIndex);
    // モデル・範囲・係数の設定を変えたら前回のフィット結果は無効 (入力と
    // 食い違う数値を残さない)
    connect(m_fitModel, &QComboBox::currentIndexChanged,
            this, [this](int) { clearFit(); });
    for (QLineEdit *e : { m_fitMin, m_fitMax, m_nCoef, m_rmsTol, m_iters })
        connect(e, &QLineEdit::textChanged, this, [this](const QString &) {
            clearFit();
        });
    connect(m_fitBtn, &QPushButton::clicked, this, &MaterialExplorerTab::runFit);
    connect(m_addBtn, &QPushButton::clicked,
            this, &MaterialExplorerTab::addToMaterials);

    showEntry(0);   // 既定選択: Au (Johnson & Christy)
}

// 内蔵DB + 光学ガラスカタログ先頭12銘柄でツリーを構築
void MaterialExplorerTab::buildDatabase()
{
    m_entries.clear();
    m_tree->clear();

    auto addGroup = [this](const QString &grp) {
        auto *g = new QTreeWidgetItem(m_tree);
        g->setText(0, QStringLiteral("📂 ") + grp);
        g->setFlags(Qt::ItemIsEnabled);
        QFont f = g->font(0);
        f.setBold(true);
        g->setFont(0, f);
        return g;
    };
    auto addItem = [this](QTreeWidgetItem *g, const Entry &e) {
        auto *it = new QTreeWidgetItem(g);
        it->setText(0, QStringLiteral("🔹 ") + e.name);
        it->setData(0, Qt::UserRole, int(m_entries.size()));
        m_entries.push_back(e);
    };

    for (const DbGroup &grp : kDb) {
        auto *g = addGroup(QString::fromUtf8(grp.grp));
        for (const DbItem &it : grp.items)
            addItem(g, { QString::fromUtf8(it.id), QString::fromUtf8(it.name),
                         QString::fromUtf8(it.model), QString::fromUtf8(it.range),
                         -1 });
    }
    // 光学ガラス (GlassCatalog 先頭12銘柄 — Sellmeier 実曲線でプレビュー)
    auto *gGlass = addGroup(QString::fromUtf8("光学ガラス / Optical glass"));
    const auto &glasses = GlassCatalog::all();
    for (int i = 0; i < glasses.size() && i < 12; ++i) {
        const Glass &g = glasses[i];
        addItem(gGlass, { QStringLiteral("glass_%1_%2").arg(g.maker, g.name),
                          QStringLiteral("%1 (%2)").arg(g.name, g.maker),
                          QStringLiteral("Sellmeier"),
                          QString::fromUtf8("0.4–1.6 μm"), i });
    }
    // 取り込んだ実測テーブル (あれば末尾に別グループで置く)。**内蔵データと
    // 同じ木に混ぜても、区分は「実測」と出して区別が付くようにする**。
    if (!m_imports.isEmpty()) {
        auto *gImp = addGroup(I18n::tr("mex_nk_group"));
        for (int i = 0; i < m_imports.size(); ++i) {
            const NkTable &t = m_imports[i];
            Entry e;
            e.id = QStringLiteral("import_%1").arg(i);
            e.name = t.error.isEmpty() ? m_importNames.value(i)
                                       : m_importNames.value(i);
            e.model = I18n::tr("mex_nk_model");
            e.range = QStringLiteral("%1–%2 nm")
                          .arg(t.minLambda_um() * 1000.0, 0, 'f', 0)
                          .arg(t.maxLambda_um() * 1000.0, 0, 'f', 0);
            e.importIndex = i;
            addItem(gImp, e);
        }
    }
    m_tree->expandAll();
    if (m_tree->topLevelItemCount() > 0
        && m_tree->topLevelItem(0)->childCount() > 0)
        m_tree->setCurrentItem(m_tree->topLevelItem(0)->child(0));
}

// 実測 n,k テーブル (CSV / TSV / 空白区切り) を読んでデータベースへ足す。
// **波長の単位をどう決めたかを必ず画面に書く** (推測を黙って通さない)。
// refractiveindex.info からの取り込み。ダイアログが n,k 表まで作るので、
// ここから先は手元の CSV を読んだときとまったく同じ経路を通す。
void MaterialExplorerTab::importFromRiInfo()
{
    RefractiveIndexDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;
    const NkTable t = dlg.table();
    if (!t.ok) return;

    m_imports.push_back(t);
    m_importNames.push_back(dlg.name());
    buildDatabase();

    if (m_fitStatus) {
        m_fitStatus->setStyleSheet("font-size:11px; color:#555;");
        m_fitStatus->setText(I18n::tr("mex_riinfo_ok")
                                 .arg(dlg.name()).arg(t.rows));
    }
}

void MaterialExplorerTab::importNk()
{
    const QString path = QFileDialog::getOpenFileName(
        this, I18n::tr("mex_nk_dialog"), QString(), I18n::tr("mex_nk_filter"));
    if (path.isEmpty()) return;

    QString unit;                       // 空 = 自動 (ヘッダ → 桁)
    if (m_nkUnit && m_nkUnit->currentIndex() > 0)
        unit = m_nkUnit->currentText();
    const NkTable t = readNkCsv(path, unit);

    if (!t.ok) {
        m_fitStatus->setText(I18n::tr("mex_nk_fail").arg(t.error));
        m_fitStatus->setStyleSheet("font-size:11px; color:#C0392B;");
        return;
    }

    const QString name = QFileInfo(path).fileName();
    m_imports.push_back(t);
    m_importNames.push_back(name);
    buildDatabase();

    // 取り込んだものを選び、読み取り結果を説明する
    const int target = m_entries.size() - 1;
    for (int g = 0; g < m_tree->topLevelItemCount(); ++g) {
        QTreeWidgetItem *grp = m_tree->topLevelItem(g);
        for (int c = 0; c < grp->childCount(); ++c) {
            if (grp->child(c)->data(0, Qt::UserRole).toInt() == target) {
                m_tree->setCurrentItem(grp->child(c));
                showEntry(target);
            }
        }
    }

    QString note = I18n::tr("mex_nk_ok_head")
                       .arg(name)
                       .arg(int(t.points.size()))
                       .arg(QString::number(t.minLambda_um() * 1000.0, 'f', 1),
                            QString::number(t.maxLambda_um() * 1000.0, 'f', 1));
    if (!unit.isEmpty())          note += I18n::tr("mex_nk_unit_user").arg(t.unit);
    else if (t.unitFromHeader)    note += I18n::tr("mex_nk_unit_hdr").arg(t.unit);
    else                          note += I18n::tr("mex_nk_unit_guess").arg(t.unit);
    if (!t.hasK)          note += I18n::tr("mex_nk_nok");
    if (t.skipped > 0)    note += I18n::tr("mex_nk_skipped").arg(t.skipped);
    if (t.duplicates > 0) note += I18n::tr("mex_nk_dup").arg(t.duplicates);
    m_fitStatus->setText(note);
    m_fitStatus->setStyleSheet("font-size:11px; color:#2E7D32;");
}

// 検索フィルタ: 材料名にマッチしない項目を隠す
void MaterialExplorerTab::filterTree(const QString &query)
{
    const QString q = query.trimmed();
    for (int g = 0; g < m_tree->topLevelItemCount(); ++g) {
        QTreeWidgetItem *grp = m_tree->topLevelItem(g);
        int visible = 0;
        for (int c = 0; c < grp->childCount(); ++c) {
            QTreeWidgetItem *it = grp->child(c);
            const int idx = it->data(0, Qt::UserRole).toInt();
            const bool hit = q.isEmpty()
                || m_entries[idx].name.contains(q, Qt::CaseInsensitive);
            it->setHidden(!hit);
            if (hit) ++visible;
        }
        grp->setHidden(!q.isEmpty() && visible == 0);
    }
}

// n 曲線: 光学ガラスと実分散内蔵材は実 Sellmeier、その他は例示式 — λ は nm。
// 実分散は有効範囲へクランプして評価する (範囲外へ外挿しない)
double MaterialExplorerTab::previewN(const Entry &e, double lambda_nm) const
{
    if (e.glassIndex >= 0 && e.glassIndex < GlassCatalog::all().size())
        return GlassCatalog::all()[e.glassIndex].n(lambda_nm / 1000.0);
    if (e.importIndex >= 0 && e.importIndex < m_imports.size()) {
        // 実測点の線形補間。範囲外は端の値で留める (外挿して作らない)
        const std::vector<optics::NkSample> &pt = m_imports[e.importIndex].points;
        if (pt.empty()) return 0.0;
        const double um = lambda_nm / 1000.0;
        if (um <= pt.front().lambda_um) return pt.front().n;
        if (um >= pt.back().lambda_um)  return pt.back().n;
        for (std::size_t i = 1; i < pt.size(); ++i) {
            if (um <= pt[i].lambda_um) {
                const double d = pt[i].lambda_um - pt[i - 1].lambda_um;
                const double f = (d > 0.0) ? (um - pt[i - 1].lambda_um) / d : 0.0;
                return pt[i - 1].n + f * (pt[i].n - pt[i - 1].n);
            }
        }
        return pt.back().n;
    }
    if (const optics::MaterialInfo *r = realNk(e.id)) {
        const double um = qBound(r->lmin_um, lambda_nm / 1000.0, r->lmax_um);
        double n = 0.0;
        if (optics::refractiveIndex(r->id, um, n)) return n;
        return 0.0;
    }
    return 0.2 + 2.5 * std::exp(-(lambda_nm - 1200.0) / 900.0)
         + std::sin(lambda_nm / 200.0) * 0.05;
}

void MaterialExplorerTab::showEntry(int index)
{
    if (index < 0 || index >= m_entries.size()) return;
    m_sel = index;
    const Entry &e = m_entries[index];

    m_selSection->setTitle(I18n::tr("mex_selected").arg(e.name));
    m_modelBadge->setText(e.model);
    m_rangeLabel->setText(I18n::tr("mex_range_fmt").arg(e.range));

    // 実分散内蔵材は有効範囲のみ、その他は 400–1580 nm を 60 点で描く
    const optics::MaterialInfo *r = realNk(e.id);
    const bool imported = (e.importIndex >= 0
                           && e.importIndex < m_imports.size());
    double l0 = 400.0, l1 = 1580.0;
    if (r) {
        l0 = std::max(l0, r->lmin_um * 1000.0);
        l1 = std::min(l1, r->lmax_um * 1000.0);
        if (l1 <= l0) {              // 表示帯域と重ならない (Si の赤外域など)
            l0 = r->lmin_um * 1000.0;
            l1 = std::min(r->lmax_um, r->lmin_um * 4.0) * 1000.0;
        }
    } else if (imported) {
        // 取込データは測った範囲だけを描く (外挿した曲線を見せない)
        l0 = m_imports[e.importIndex].minLambda_um() * 1000.0;
        l1 = m_imports[e.importIndex].maxLambda_um() * 1000.0;
    }
    // 取り込んだ実測値は**いちばん実データ**なので、内蔵の実分散材と同じ
    // 扱いにする (ここを落とすとフィットも追加も押せなくなる)
    const bool realCurve = (r != nullptr) || (e.glassIndex >= 0) || imported;
    MiniSeries sn, sk;
    sn.color = QColor("#B83280");
    sk.color = QColor("#F59E0B");
    for (int i = 0; i < 60; ++i) {
        const double l = l0 + (l1 - l0) * i / 59.0;
        sn.pts.push_back({ l, previewN(e, l) });
        // 実 Sellmeier の有効範囲 (透明域) では k≈0
        const double k = realCurve ? 0.0
            : 1.5 + (l - 400.0) / 1600.0 * 8.0 + std::cos(l / 300.0) * 0.1;
        sk.pts.push_back({ l, k });
    }
    if (imported && m_imports[e.importIndex].hasK) {
        // k 列があるなら測った点をそのまま描く。**無いときは描かない**
        // (0 の直線を引くと「吸収ゼロを測った」ことになる)
        sk.pts.clear();
        for (const optics::NkSample &sm : m_imports[e.importIndex].points)
            if (sm.k >= 0.0)
                sk.pts.push_back({ sm.lambda_um * 1000.0, sm.k });
    } else if (imported) {
        sk.pts.clear();
    }
    m_refSeries = sn;              // フィット曲線の重ね描き用に保持
    m_plotN->setSeries({ sn });
    m_plotK->setSeries({ sk });

    // 実データの有無を明示し、無い材料は追加を無効化する (誤った εr を
    // プロジェクトへ書き込まない — 絶対規則 5/6 の流儀)
    if (imported)
        m_previewNote->setText(I18n::tr("mex_prev_import").arg(e.range));
    else if (r)
        m_previewNote->setText(I18n::tr("mex_prev_real").arg(e.range));
    else if (e.glassIndex >= 0)
        m_previewNote->setText(I18n::tr("mex_prev_glass"));
    else
        m_previewNote->setText(I18n::tr("mex_prev_fake"));
    m_addBtn->setEnabled(realCurve);
    m_addBtn->setToolTip(realCurve ? QString()
                                   : I18n::tr("mex_add_na_tip"));
    // フィット範囲も選んだ材料の有効範囲へ合わせる。**前の材料の範囲を
    // 残すと、選び直した直後に「範囲が重ならない」で必ず失敗する**
    // (取込データを選んだときにヘッドレス描画で見つけた)。
    double refLo = 0.0, refHi = 0.0;
    if (referenceRange(e, refLo, refHi)) {
        m_fitMin->setText(QString::number(refLo, 'f', 0));
        m_fitMax->setText(QString::number(refHi, 'f', 0));
    }
    // 材料を変えたら前回のフィット結果は捨てる (別材料の残差を残さない)
    m_fitBtn->setEnabled(realCurve);
    m_fitBtn->setToolTip(realCurve ? QString() : I18n::tr("mex_fit_nodata"));
    clearFit();
}

// ── 分散モデルのフィット ────────────────────────────────────────────────────

// 参照データの有効範囲 [nm]。実分散を持たない材料は false。
bool MaterialExplorerTab::referenceRange(const Entry &e, double &lo_nm,
                                         double &hi_nm) const
{
    if (e.glassIndex >= 0 && e.glassIndex < GlassCatalog::all().size()) {
        // Sellmeier 係数を持たない銘柄 (nd/vd だけの取込分) は公刊分散が
        // 無いのでフィット対象にしない (近似式を「実データ」と呼ばない)
        if (!GlassCatalog::all()[e.glassIndex].hasSellmeier()) return false;
        lo_nm = kGlassLoUm * 1000.0;
        hi_nm = kGlassHiUm * 1000.0;
        return true;
    }
    if (e.importIndex >= 0 && e.importIndex < m_imports.size()) {
        const NkTable &t = m_imports[e.importIndex];
        if (!t.ok) return false;
        lo_nm = t.minLambda_um() * 1000.0;
        hi_nm = t.maxLambda_um() * 1000.0;
        return true;
    }
    if (const optics::MaterialInfo *r = realNk(e.id)) {
        lo_nm = r->lmin_um * 1000.0;
        hi_nm = r->lmax_um * 1000.0;
        return true;
    }
    return false;
}

void MaterialExplorerTab::setBadge(QLabel *badge, const QString &text,
                                   const char *color)
{
    badge->setText(text);
    badge->setStyleSheet(QStringLiteral(
        "border:1px solid %1; color:%1; border-radius:3px;"
        "padding:1px 6px; font-size:11px;").arg(QString::fromUtf8(color)));
}

// フィット結果を捨てて「未計算」表示へ戻す
void MaterialExplorerTab::clearFit()
{
    m_hasFit = false;
    m_fit = optics::FitReport();
    setBadge(m_badgeRms, I18n::tr("mex_badge_rms_na"), "#808080");
    setBadge(m_badgeCausal, I18n::tr("mex_badge_causal_na"), "#808080");
    for (QLineEdit *e : { m_epsInfD, m_wpD, m_gammaD,
                          m_epsInfL, m_wpL, m_gammaL, m_w0L })
        e->setText(I18n::tr("mex_dash"));
    m_fitStatus->setStyleSheet("font-size:11px;");   // 前回のエラー色を戻す
    if (m_sel >= 0 && m_sel < m_entries.size()) {
        double lo = 0.0, hi = 0.0;
        m_fitStatus->setText(referenceRange(m_entries[m_sel], lo, hi)
                                 ? QString()
                                 : I18n::tr("mex_fit_nodata"));
    } else {
        m_fitStatus->clear();
    }
    showFit();
}

void MaterialExplorerTab::runFit()
{
    if (m_sel < 0 || m_sel >= m_entries.size()) return;
    const Entry &e = m_entries[m_sel];
    double refLo = 0.0, refHi = 0.0;
    if (!referenceRange(e, refLo, refHi)) {
        clearFit();
        m_fitStatus->setText(I18n::tr("mex_fit_nodata"));
        m_fitStatus->setStyleSheet("font-size:11px; color:#B8860B;");
        return;
    }

    // フィット範囲: 入力値を参照データの有効範囲へ丸める (外挿しない)
    bool okLo = false, okHi = false;
    double lo = m_fitMin->text().trimmed().toDouble(&okLo);
    double hi = m_fitMax->text().trimmed().toDouble(&okHi);
    if (!okLo || !okHi || !(hi > lo)) { lo = refLo; hi = refHi; }
    const double clampedLo = std::max(lo, refLo);
    const double clampedHi = std::min(hi, refHi);
    if (!(clampedHi > clampedLo)) {
        clearFit();
        m_fitStatus->setText(I18n::tr("mex_fit_badrange")
                                 .arg(lo, 0, 'f', 0).arg(hi, 0, 'f', 0)
                                 .arg(refLo, 0, 'f', 0).arg(refHi, 0, 'f', 0));
        m_fitStatus->setStyleSheet("font-size:11px; color:#C0392B;");
        return;
    }

    std::vector<optics::NkSample> samples;
    if (e.importIndex >= 0 && e.importIndex < m_imports.size()) {
        // 取り込んだ実測データは**測った点そのもの**を使う。等間隔に
        // 引き直すと補間した値を「実測」としてフィットすることになる。
        for (const optics::NkSample &sm : m_imports[e.importIndex].points) {
            const double nm = sm.lambda_um * 1000.0;
            if (nm >= clampedLo - 1e-9 && nm <= clampedHi + 1e-9)
                samples.push_back(sm);
        }
    } else {
        // 参照データ (公刊 Sellmeier) をサンプルする。k は「データ無し」(負値)
        samples.reserve(kFitSamples);
        for (int i = 0; i < kFitSamples; ++i) {
            const double nm = clampedLo
                            + (clampedHi - clampedLo) * i / (kFitSamples - 1);
            optics::NkSample s;
            s.lambda_um = nm / 1000.0;
            s.n = previewN(e, nm);
            s.k = -1.0;                   // 内蔵データは実部のみ
            samples.push_back(s);
        }
    }

    optics::FitOptions o;
    switch (m_fitModel->currentIndex()) {
    case 1:  o.model = optics::FitModel::Drude;   break;
    case 2:  o.model = optics::FitModel::Lorentz; break;
    case 3:  o.model = optics::FitModel::Sampled; break;
    default: o.model = optics::FitModel::MultiPole; break;
    }
    bool ok = false;
    const int nc = m_nCoef->text().trimmed().toInt(&ok);
    o.maxPoles = (ok && nc >= 1) ? std::min(nc, 12) : 6;
    const double tol = m_rmsTol->text().trimmed().toDouble(&ok);
    o.rmsTol = (ok && tol > 0.0) ? tol : 0.1;
    const int it = m_iters->text().trimmed().toInt(&ok);
    o.iterations = (ok && it >= 0) ? std::min(it, 100) : 10;

    m_fit = optics::fitDispersion(samples, o);
    m_hasFit = (m_fit.status == optics::FitStatus::Ok);
    m_fitLo_nm = clampedLo;
    m_fitHi_nm = clampedHi;

    const bool fromImport = (e.importIndex >= 0
                             && e.importIndex < m_imports.size());
    QString msg = I18n::tr(fromImport ? "mex_fit_src_import" : "mex_fit_src")
                      .arg(e.name)
                      .arg(clampedLo, 0, 'f', 0)
                      .arg(clampedHi, 0, 'f', 0)
                      .arg(int(samples.size()));   // 実際に使った点数
    if (clampedLo > lo + 0.5 || clampedHi < hi - 0.5)
        msg += I18n::tr("mex_fit_clamped")
                   .arg(clampedLo, 0, 'f', 0).arg(clampedHi, 0, 'f', 0);
    if (!m_hasFit) {
        msg += QStringLiteral("\n") + I18n::tr("mex_fit_failed");
        m_fitStatus->setStyleSheet("font-size:11px; color:#C0392B;");
    } else if (m_fit.interpolation) {
        msg += QStringLiteral("\n") + I18n::tr("mex_fit_interp");
        m_fitStatus->setStyleSheet("font-size:11px;");
    } else {
        msg += QStringLiteral("\n") + I18n::tr("mex_fit_done")
                   .arg(m_fit.poles)
                   .arg(m_fit.rmsN, 0, 'e', 2)
                   .arg(m_fit.maxErrN, 0, 'e', 2);
        m_fitStatus->setStyleSheet("font-size:11px;");
    }
    m_fitStatus->setText(msg);
    showFit();
}

// フィット結果 → バッジ・パラメータ欄・診断表・重ね描き
void MaterialExplorerTab::showFit()
{
    const QString nc = I18n::tr("mex_notcalc");
    const QString oos = I18n::tr("mex_outofscope");
    const optics::FitReport &f = m_fit;
    const bool fitted = m_hasFit && !f.interpolation;

    // ── バッジ ──
    if (fitted) {
        bool okTol = true;
        const double tol = m_rmsTol->text().trimmed().toDouble(&okTol);
        const bool within = okTol && tol > 0.0 && f.rmsN <= tol;
        setBadge(m_badgeRms,
                 I18n::tr("mex_badge_rms").arg(f.rmsN, 0, 'e', 2),
                 within ? "#2E8B57" : "#B45309");
    } else if (m_hasFit && f.interpolation) {
        setBadge(m_badgeRms, I18n::tr("mex_badge_rms").arg(0.0, 0, 'f', 3),
                 "#2E8B57");
    } else {
        setBadge(m_badgeRms, I18n::tr("mex_badge_rms_na"), "#808080");
    }
    if (!m_hasFit)
        setBadge(m_badgeCausal, I18n::tr("mex_badge_causal_na"), "#808080");
    else if (!f.causalityEvaluable)
        setBadge(m_badgeCausal, I18n::tr("mex_badge_causal_skip"), "#808080");
    else if (f.causalityViolations == 0)
        setBadge(m_badgeCausal, I18n::tr("mex_badge_causal"), "#2E8B57");
    else
        setBadge(m_badgeCausal, I18n::tr("mex_badge_causal_ng"), "#C0392B");

    // ── モデルパラメータ欄 (フィット結果の表示) ──
    if (fitted) {
        const double c0 = 2.99792458e8;
        const double pi = 3.14159265358979323846;
        // ω = 2πc/λ (λ は µm → m)
        auto omega = [c0, pi](double lam_um) {
            return (lam_um > 0.0) ? 2.0 * pi * c0 / (lam_um * 1e-6) : 0.0;
        };
        if (f.model == optics::FitModel::Drude && !f.lambda0_um.empty()) {
            m_epsInfD->setText(QString::number(f.epsInf, 'f', 4));
            m_wpD->setText(QString::number(omega(f.lambda0_um[0]), 'e', 3));
            m_gammaD->setText(QStringLiteral("0"));
        } else if (f.model == optics::FitModel::Lorentz
                   && !f.lambda0_um.empty() && !f.deltaEps.empty()) {
            const double w0 = omega(f.lambda0_um[0]);
            m_epsInfL->setText(QString::number(f.epsInf, 'f', 4));
            // Lorentz 振動子 ε = ε∞ + Δε·ω0²/(ω0²−ω²) の等価プラズマ周波数
            m_wpL->setText(QString::number(std::sqrt(std::max(0.0, f.deltaEps[0]))
                                               * w0, 'e', 3));
            m_gammaL->setText(QStringLiteral("0"));
            m_w0L->setText(QString::number(w0, 'e', 3));
        }
    }

    // ── 診断表 ──
    struct Row { QString value, verdict; int level; };   // 0=良 1=注意 2=不可
    Row rows[5];
    // ① n の RMS 残差
    if (fitted) {
        bool okTol = true;
        const double tol = m_rmsTol->text().trimmed().toDouble(&okTol);
        const bool within = okTol && tol > 0.0 && f.rmsN <= tol;
        rows[0] = { I18n::tr("mex_v_rms_n")
                        .arg(QString::number(f.rmsN, 'e', 3),
                             QString::number(f.maxErrN, 'e', 3)),
                    I18n::tr(within ? "mex_good" : "mex_improve"),
                    within ? 0 : 1 };
    } else if (m_hasFit && f.interpolation) {
        rows[0] = { QString::number(0.0, 'f', 3), I18n::tr("mex_good"), 0 };
    } else {
        rows[0] = { I18n::tr("mex_v_nofit"), nc, 1 };
    }
    // ② k の RMS 残差 — 内蔵データは実部のみなので評価対象外
    if (m_hasFit && f.hasK)
        rows[1] = { QString::number(f.rmsK, 'e', 3),
                    I18n::tr(f.rmsK <= 0.1 ? "mex_good" : "mex_improve"),
                    f.rmsK <= 0.1 ? 0 : 1 };
    else if (m_hasFit)
        rows[1] = { I18n::tr("mex_v_nok"), oos, 1 };
    else
        rows[1] = { I18n::tr("mex_v_nofit"), nc, 1 };
    // ③ 因果律 (透明域の必要条件 dε/dω ≥ 0)
    if (!m_hasFit)
        rows[2] = { I18n::tr("mex_v_kk_na"), oos, 1 };
    else if (!f.causalityEvaluable)
        rows[2] = { I18n::tr("mex_v_kk_absorb"), oos, 1 };
    else if (f.causalityViolations == 0)
        rows[2] = { I18n::tr("mex_v_kk_ok").arg(f.causalityChecks),
                    I18n::tr("mex_satisfied"), 0 };
    else
        rows[2] = { I18n::tr("mex_v_kk_ng").arg(f.causalityViolations)
                        .arg(f.causalityChecks),
                    I18n::tr("mex_violated"), 2 };
    // ④ 受動性 (ε∞ ≥ 1 かつ Δε_p ≥ 0)
    if (fitted) {
        QString v;
        if (f.model == optics::FitModel::Drude && !f.lambda0_um.empty()) {
            v = I18n::tr("mex_v_passive_drude")
                    .arg(f.epsInf, 0, 'f', 3).arg(f.lambda0_um[0], 0, 'f', 4);
        } else {
            double minDe = f.deltaEps.empty() ? 0.0 : f.deltaEps[0];
            for (double d : f.deltaEps) minDe = std::min(minDe, d);
            v = I18n::tr("mex_v_passive")
                    .arg(f.epsInf, 0, 'f', 3).arg(minDe, 0, 'f', 4);
        }
        rows[3] = { v, I18n::tr(f.passivityOk ? "mex_satisfied" : "mex_violated"),
                    f.passivityOk ? 0 : 2 };
    } else if (m_hasFit && f.interpolation) {
        rows[3] = { I18n::tr("mex_v_nok"), oos, 1 };
    } else {
        rows[3] = { I18n::tr("mex_v_nofit"), nc, 1 };
    }
    // ⑤ FDTD 安定性 (n < 1 の帯域があると真空基準の Courant 条件では不足)
    if (fitted) {
        const bool ok = (f.nMin >= 1.0);
        rows[4] = { I18n::tr("mex_v_nmin").arg(f.nMin, 0, 'f', 4),
                    I18n::tr(ok ? "mex_good" : "mex_dt_limit"), ok ? 0 : 1 };
    } else {
        rows[4] = { I18n::tr("mex_v_nofit"), nc, 1 };
    }

    for (int r = 0; r < 5; ++r) {
        m_diag->item(r, 1)->setText(rows[r].value);
        m_diag->item(r, 2)->setText(rows[r].verdict);
        m_diag->item(r, 2)->setForeground(QColor(
            rows[r].level == 0 ? "#2E8B57"
                               : (rows[r].level == 1 ? "#B45309" : "#C0392B")));
    }

    // ── n プロットへフィット曲線を重ねる (参照データ + フィット結果) ──
    if (m_sel >= 0 && m_sel < m_entries.size()) {
        QVector<MiniSeries> series;
        series.push_back(m_refSeries);
        if (fitted && m_fitHi_nm > m_fitLo_nm) {
            MiniSeries fitS;
            fitS.color = QColor("#0078D4");
            for (int i = 0; i < 60; ++i) {
                const double nm = m_fitLo_nm
                    + (m_fitHi_nm - m_fitLo_nm) * i / 59.0;
                const double n = optics::modelIndex(m_fit, nm / 1000.0);
                if (n > 0.0) fitS.pts.push_back({ nm, n });
            }
            if (!fitS.pts.isEmpty()) series.push_back(fitS);
        }
        m_plotN->setSeries(series);
    }
}

// 選択材料を Project::materials() へ (εr = n(λc)²  — GlassCatalogTab と同方針)
void MaterialExplorerTab::addToMaterials()
{
    if (m_sel < 0 || m_sel >= m_entries.size()) return;
    const Entry &e = m_entries[m_sel];
    // 実分散データの無い材料は追加しない (例示曲線から誤った εr を
    // プロジェクトへ書き込まない)。UI 側でもボタンを無効化している
    if (e.glassIndex < 0 && !realNk(e.id)) return;

    const OpticalOpts &o = m_p->optical();
    const double lc_nm = (o.lambdaMin + o.lambdaMax) / 2.0;
    const double n = previewN(e, lc_nm);

    Material m;
    m.type = 1;
    m.epsr = n * n;
    m.name = QStringLiteral("%1 (n=%2 @%3nm)")
        .arg(e.name, QString::number(n, 'f', 4))
        .arg(qRound(lc_nm));
    m_p->materials().push_back(m);
    emit m_p->materialsEdited();
    m_p->touch();
}
