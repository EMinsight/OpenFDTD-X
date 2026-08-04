// MaterialExplorerTab.cpp
#include "MaterialExplorerTab.h"
#include "../core/GlassCatalog.h"
#include "../core/Project.h"
#include "../widgets/MiniPlot.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"
#include "TabHelpers.h"

#include <QComboBox>
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
        "Al2O3/Si/TiO2/PMMA) と光学ガラスは公刊 Sellmeier 係数の実分散を内蔵。"
        "それ以外は例示表示のみ (n,k 取込・分散フィットは未実装)。",
        "Database of metals, semiconductors, dielectrics and 2D materials. "
        "Six dielectrics (SiO2/Si3N4/Al2O3/Si/TiO2/PMMA) and optical glasses "
        "carry real published Sellmeier dispersion; the rest are illustrative "
        "only (n,k import and dispersion fitting are not implemented).");
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
        "書き込まないため)。n,k 取込は未実装です",
        "Materials without real dispersion data cannot be added (to avoid "
        "writing an incorrect εr). n,k import is not implemented");
    ofd::I18n::reg("mex_db_section",  "データベース", "Database");
    ofd::I18n::reg("mex_search_ph",   "🔎 材料を検索…", "🔎 Search materials…");
    ofd::I18n::reg("mex_import_nk",   "📁 n,k 取込", "📁 Import n,k");
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
    ofd::I18n::reg("mex_badge_rms",   "RMS誤差 0.043", "RMS error 0.043");
    ofd::I18n::reg("mex_badge_causal","因果律 OK", "Causality OK");
    ofd::I18n::reg("mex_fit_section", "n, k フィット結果", "Fit quality");
    ofd::I18n::reg("mex_n_real",      "屈折率 n (実部)", "Refractive index n (real part)");
    ofd::I18n::reg("mex_k_imag",      "消衰係数 k (虚部)", "Extinction coefficient k (imag part)");
    ofd::I18n::reg("mex_fit_note",
        "── 表示は合成 1 系列のみ (光学ガラスは Sellmeier 実曲線、"
        "カタログ材以外は例示曲線)。実測点・フィット曲線の重ね描きは未実装。",
        "── single synthetic curve only (optical glass uses real Sellmeier curves; "
        "other entries are illustrative). Measured points / fitted-curve overlay not implemented.");
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
    ofd::I18n::reg("mex_cond_stable", "条件付安定", "conditionally stable");
    ofd::I18n::reg("mex_good",        "良好", "good");
    ofd::I18n::reg("mex_dt_limit",    "Δt制約あり", "Δt constrained");
    ofd::I18n::reg("mex_diag_hint",
        "▸ 不安定な場合は係数を減らすか、フィット範囲を狭めてください。",
        "▸ If unstable, reduce the number of coefficients or narrow the fit range.");
    ofd::I18n::reg("mex_apply_section","材料の利用", "Apply");
    ofd::I18n::reg("mex_add_material", "この材料を物性値リストに追加",
                                       "Add this material to the material list");
    ofd::I18n::reg("mex_temp_table",   "温度依存テーブル", "Temperature-dependent table");
    ofd::I18n::reg("mex_aniso",        "異方性テンソル ε[ij]", "Anisotropic tensor ε[ij]");
    ofd::I18n::reg("mex_nonlinear",    "非線形 χ⁽²⁾/χ⁽³⁾ 付与", "Nonlinear χ⁽²⁾/χ⁽³⁾");
    ofd::I18n::reg("mex_gain",         "利得媒質 (4準位)", "Gain medium (4-level)");
    ofd::I18n::reg("mex_magnetic",     "磁性 (μr≠1)", "Magnetic (μr≠1)");
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

// ── 実分散データ (公刊 Sellmeier 係数 — 出典を明記) ─────────────────────────
// n² = A + Σ Bi·λ²/(λ² − Ci) + Σ Di/(λ² − Ei)   (λ は μm, Ci/Ei は μm²)
// 有効範囲外は評価しない (範囲外へ外挿した「それらしい値」を出さない)。
struct RealDispersion {
    const char *id;       // kDb の id と一致
    double A;
    double B[3], C[3];    // Sellmeier 項
    double D, E;          // 加算極 (DeVore 型)。D=0 なら未使用
    double lmin_um, lmax_um;
};
const RealDispersion kRealNk[] = {
    // SiO2 — Malitson (1965)
    { "SiO2", 1.0, { 0.6961663, 0.4079426, 0.8974794 },
      { 0.004679148, 0.013512063, 97.934003 }, 0, 0, 0.21, 3.71 },
    // Si3N4 — Luke (2015)
    { "Si3N4", 1.0, { 3.0249, 40314.0, 0.0 },
      { 0.018317068, 1537208.2, 1.0 }, 0, 0, 0.31, 5.5 },
    // Al2O3 (サファイア常光) — Malitson (1962)
    { "Al2O3", 1.0, { 1.4313493, 0.65054713, 5.3414021 },
      { 0.005279925, 0.014238264, 325.01783 }, 0, 0, 0.2, 5.0 },
    // Si — Salzberg & Villa (1957), 赤外の透明域
    { "Si", 1.0, { 10.6684293, 0.0030434748, 1.54133408 },
      { 0.090912190, 1.2876602, 1218816.0 }, 0, 0, 1.36, 11.0 },
    // TiO2 (ルチル常光) — DeVore (1951): n² = 5.913 + 0.2441/(λ²−0.0803)
    { "TiO2", 5.913, { 0, 0, 0 }, { 1, 1, 1 }, 0.2441, 0.0803, 0.43, 1.5 },
    // PMMA — Sultanova ら (2009)
    { "PMMA", 1.0, { 1.1819, 0, 0 }, { 0.011313, 1.0, 1.0 }, 0, 0,
      0.44, 1.05 },
};

const RealDispersion *realNk(const QString &id)
{
    for (const RealDispersion &r : kRealNk)
        if (id == QLatin1String(r.id)) return &r;
    return nullptr;
}

double realN(const RealDispersion &r, double lambda_um)
{
    const double l2 = lambda_um * lambda_um;
    double n2 = r.A;
    for (int i = 0; i < 3; ++i)
        if (r.B[i] != 0.0) n2 += r.B[i] * l2 / (l2 - r.C[i]);
    if (r.D != 0.0) n2 += r.D / (l2 - r.E);
    return (n2 > 0.0) ? std::sqrt(n2) : 0.0;
}

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
    tabhelp::markNotImplemented(impNk);   // n,k 取込は未配線
    tabhelp::markNotImplemented(riBtn);   // refractiveindex.info 連携は未配線
    dbBtns->addWidget(impNk);
    dbBtns->addWidget(riBtn);
    dbBtns->addStretch(1);
    sDb->vbox()->addLayout(dbBtns);
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

    auto *fitRow = new QHBoxLayout();
    auto *fitBtn = new QPushButton(I18n::tr("mex_run_fit"), m_selSection);
    fitBtn->setProperty("primary", true);
    tabhelp::markNotImplemented(fitBtn);   // フィット計算は未実装
    fitRow->addWidget(fitBtn);
    fitRow->addStretch(1);
    fitRow->addWidget(makeBadge(I18n::tr("mex_badge_rms"), "#2E8B57", m_selSection));
    fitRow->addWidget(makeBadge(I18n::tr("mex_badge_causal"), "#2E8B57", m_selSection));
    m_selSection->form()->addRow(fitRow);
    // バッジ (RMS誤差 / 因果律) は固定のサンプル値
    m_selSection->form()->addRow(tabhelp::sampleNote(m_selSection));
    // フィット設定フォーム (モデル/範囲/係数) はどこにも反映されない
    m_selSection->form()->addRow(tabhelp::unwiredNote(m_selSection));
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

    // モデル診断
    auto *sDiag = new SectionBox(I18n::tr("mex_diag_section"), body);
    auto *diag = new QTableWidget(5, 3, sDiag);
    diag->setHorizontalHeaderLabels({ I18n::tr("mex_diag_item"),
        I18n::tr("mex_diag_value"), I18n::tr("mex_diag_verdict") });
    diag->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    diag->verticalHeader()->setVisible(false);
    diag->setEditTriggers(QAbstractItemView::NoEditTriggers);
    diag->setMinimumHeight(170);
    const struct { QString item, value, verdict; bool warn; } kDiag[5] = {
        { I18n::tr("mex_diag_rms_n"),   "0.043",                    I18n::tr("mex_good"),     false },
        { I18n::tr("mex_diag_rms_k"),   "0.061",                    I18n::tr("mex_good"),     false },
        { I18n::tr("mex_diag_kk"),      I18n::tr("mex_satisfied"),  "OK",                     false },
        { I18n::tr("mex_diag_passive"), I18n::tr("mex_satisfied"),  "OK",                     false },
        { I18n::tr("mex_diag_stab"),    I18n::tr("mex_cond_stable"),I18n::tr("mex_dt_limit"), true },
    };
    for (int r = 0; r < 5; ++r) {
        diag->setItem(r, 0, new QTableWidgetItem(kDiag[r].item));
        diag->setItem(r, 1, new QTableWidgetItem(kDiag[r].value));
        auto *ver = new QTableWidgetItem(kDiag[r].verdict);
        ver->setForeground(QColor(kDiag[r].warn ? "#B45309" : "#2E8B57"));
        diag->setItem(r, 2, ver);
    }
    sDiag->vbox()->addWidget(diag);
    // 診断表は固定のサンプル値 (実行結果ではない)
    sDiag->vbox()->addWidget(tabhelp::sampleNote(sDiag));
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
    tabhelp::markNotImplemented(tempBtn);    // 温度依存テーブルは未配線
    tabhelp::markNotImplemented(anisoBtn);   // 異方性テンソルは未配線
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
    sApply->vbox()->addWidget(tabhelp::unwiredNote(sApply));
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
    m_tree->expandAll();
    if (m_tree->topLevelItemCount() > 0
        && m_tree->topLevelItem(0)->childCount() > 0)
        m_tree->setCurrentItem(m_tree->topLevelItem(0)->child(0));
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
    if (const RealDispersion *r = realNk(e.id)) {
        const double um = qBound(r->lmin_um, lambda_nm / 1000.0, r->lmax_um);
        return realN(*r, um);
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
    const RealDispersion *r = realNk(e.id);
    double l0 = 400.0, l1 = 1580.0;
    if (r) {
        l0 = std::max(l0, r->lmin_um * 1000.0);
        l1 = std::min(l1, r->lmax_um * 1000.0);
        if (l1 <= l0) {              // 表示帯域と重ならない (Si の赤外域など)
            l0 = r->lmin_um * 1000.0;
            l1 = std::min(r->lmax_um, r->lmin_um * 4.0) * 1000.0;
        }
    }
    const bool realCurve = (r != nullptr) || (e.glassIndex >= 0);
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
    m_plotN->setSeries({ sn });
    m_plotK->setSeries({ sk });

    // 実データの有無を明示し、無い材料は追加を無効化する (誤った εr を
    // プロジェクトへ書き込まない — 絶対規則 5/6 の流儀)
    if (r)
        m_previewNote->setText(I18n::tr("mex_prev_real").arg(e.range));
    else if (e.glassIndex >= 0)
        m_previewNote->setText(I18n::tr("mex_prev_glass"));
    else
        m_previewNote->setText(I18n::tr("mex_prev_fake"));
    m_addBtn->setEnabled(realCurve);
    m_addBtn->setToolTip(realCurve ? QString()
                                   : I18n::tr("mex_add_na_tip"));
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
