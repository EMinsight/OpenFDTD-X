// ToleranceTab.cpp
#include "ToleranceTab.h"
#include "TabHelpers.h"
#include "../core/Project.h"
#include "../core/ToleranceStats.h"
#include "../widgets/MiniPlot.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <cmath>

using namespace ofd;

// ── タブ専用語彙 (file-local 登録, 接頭辞 tol_) ─────────────────────────────
namespace {
const bool s_i18n = [] {
    using ofd::I18n;
    I18n::reg("tol_title_fmt", "製造ばらつき・歩留まり解析 (%1)",
              "Tolerance & Yield (%1)");
    // 何が実計算で何が未計算かを冒頭で宣言する (CLAUDE.md 絶対規則 5)
    I18n::reg("tol_hint",
              "製造誤差/環境変動をモンテカルロでサンプリングし、性能分布と"
              "歩留まりを評価する画面。\n"
              "【実計算】入力ばらつきそのものの確率分布 (下の表の分布・中心・σ "
              "から解析的に計算。表を編集すると即座に反映されます)\n"
              "【未計算】性能 (FoM) の分布と歩留まり — 各サンプルについて"
              "ソルバーを走らせる必要があり、モンテカルロは未実装です。",
              "Samples manufacturing and environmental variation by Monte Carlo "
              "and evaluates the performance distribution and yield.\n"
              "[computed] the probability distribution of the input variation "
              "itself (analytic, from the distribution / center / σ in the "
              "table below — edits apply immediately)\n"
              "[not computed] the performance (FoM) distribution and the yield: "
              "these need a solver run per sample and Monte Carlo is not "
              "implemented.");

    I18n::reg("tol_sources", "ばらつき要因", "Variation sources");
    I18n::reg("tol_c_item", "項目", "Item");
    I18n::reg("tol_c_dist", "分布", "Distribution");
    I18n::reg("tol_c_center", "中心", "Center");
    I18n::reg("tol_c_sigma", "σ / 半幅", "σ / half-width");
    I18n::reg("tol_c_unit", "単位", "Unit");
    I18n::reg("tol_src_note",
              "中心と σ / 半幅は編集できます (数値のみ。不正な入力は元の値へ"
              "戻します)。σ は正規分布では標準偏差、一様分布では半幅 a、"
              "レイリー分布では尺度パラメータです。この表は下の分布表示に"
              "使われますが、プロジェクトへの保存とソルバー連携は未実装です。",
              "Center and σ / half-width are editable (numbers only; invalid "
              "input reverts). σ is the standard deviation for a normal "
              "distribution, the half-width a for a uniform one, and the scale "
              "parameter for a Rayleigh one. The table feeds the distribution "
              "plot below; saving it to the project and using it in the solver "
              "are not implemented.");

    // 分布名
    I18n::reg("tol_d_normal", "正規", "Normal");
    I18n::reg("tol_d_uniform", "一様", "Uniform");
    I18n::reg("tol_d_rayleigh", "レイリー", "Rayleigh");
    I18n::reg("tol_d_discrete", "離散", "Discrete");

    // ばらつき要因の項目名 (ドメイン別)
    I18n::reg("tol_i_patch", "パッチ寸法", "Patch dimension");
    I18n::reg("tol_i_epsr", "基板誘電率 εr", "Substrate εr");
    I18n::reg("tol_i_subth", "基板厚さ", "Substrate thickness");
    I18n::reg("tol_i_feed", "給電位置", "Feed position");
    I18n::reg("tol_i_solder", "はんだ位置", "Solder position");
    I18n::reg("tol_i_temp", "温度変動", "Temperature variation");
    I18n::reg("tol_i_wgw", "導波路幅", "Waveguide width");
    I18n::reg("tol_i_wgt", "導波路厚さ", "Waveguide thickness");
    I18n::reg("tol_i_rough", "側壁ラフネス", "Sidewall roughness");
    I18n::reg("tol_i_gap", "結合間隙 (gap)", "Coupling gap");
    I18n::reg("tol_i_index", "屈折率 n", "Refractive index n");
    I18n::reg("tol_i_alpha", "吸音率 α", "Absorption coefficient α");
    I18n::reg("tol_i_wall", "壁面位置", "Wall position");
    I18n::reg("tol_i_occ", "客席占有率", "Audience occupancy");
    I18n::reg("tol_i_room", "室温", "Room temperature");
    I18n::reg("tol_i_humid", "湿度", "Humidity");
    I18n::reg("tol_i_ssp", "音速プロファイル", "Sound-speed profile");
    I18n::reg("tol_i_bottom", "底質特性", "Bottom type");
    I18n::reg("tol_i_water", "水温変動", "Water temperature");
    I18n::reg("tol_i_salt", "塩分", "Salinity");
    I18n::reg("tol_i_wave", "波高", "Wave height");
    I18n::reg("tol_u_none", "—", "—");
    I18n::reg("tol_u_bottom", "砂/泥/岩", "sand/mud/rock");
    I18n::reg("tol_u_nmrms", "nm RMS", "nm RMS");

    I18n::reg("tol_mc", "モンテカルロ設定", "Monte Carlo settings");
    I18n::reg("tol_samples", "サンプル数", "Samples");
    I18n::reg("tol_method", "サンプリング法", "Sampling method");
    I18n::reg("tol_random", "完全ランダム", "Pure random");
    I18n::reg("tol_lhs", "Latin Hypercube", "Latin Hypercube");
    I18n::reg("tol_sobol", "Sobol系列", "Sobol sequence");

    I18n::reg("tol_criteria", "合格条件", "Pass criteria");
    I18n::reg("tol_results", "入力ばらつきの分布 / 結果",
              "Input variation distribution / results");
    // モンテカルロは未実装なので「完了」も歩留まりも偽装しない
    I18n::reg("tol_lastrun", "最終ラン: 未実行", "Last run: not run yet");
    I18n::reg("tol_yield_na", "歩留まり: 未計算 (モンテカルロ未実装)",
              "Yield: not computed (Monte Carlo not implemented)");
    I18n::reg("tol_var", "表示する変数", "Variable to plot");
    I18n::reg("tol_var_none", "(連続分布の有効な変数がありません)",
              "(no enabled variable with a continuous distribution)");
    I18n::reg("tol_3sigma_fmt",
              "3σ 相当の被覆区間 (P = %1%): %2 ~ %3 %4  /  標準偏差 σ = %5 %4, "
              "平均 = %6 %4",
              "Coverage interval equivalent to ±3σ (P = %1%): %2 to %3 %4  /  "
              "standard deviation σ = %5 %4, mean = %6 %4");
    I18n::reg("tol_3sigma_na", "3σ 区間: 連続分布の変数を選択してください",
              "±3σ interval: select a variable with a continuous distribution");
    I18n::reg("tol_density", "確率密度", "probability density");
    I18n::reg("tol_res_note",
              "上のグラフは選択した「入力変数」の確率密度 (解析形) です。"
              "被覆確率は正規分布の ±3σ (99.73%) に揃えてあり、一様分布・"
              "レイリー分布でも同じ確率を含む区間を示します。"
              "性能 (FoM) の分布・歩留まり・感度は表示していません — "
              "モンテカルロ (各サンプルでソルバーを実行) が未実装のためです。",
              "The plot above is the analytic probability density of the "
              "selected INPUT variable. The coverage probability is matched to "
              "±3σ of a normal distribution (99.73%), so uniform and Rayleigh "
              "variables show the interval holding the same probability. The "
              "performance (FoM) distribution, yield and sensitivity are NOT "
              "shown because Monte Carlo (a solver run per sample) is not "
              "implemented.");
    I18n::reg("tol_report", "📤 統計レポート (PDF)", "📤 Statistics report (PDF)");
    I18n::reg("tol_sensitivity", "📊 Sensitivity 解析", "📊 Sensitivity analysis");
    I18n::reg("tol_robust", "🎯 ロバスト最適化", "🎯 Robust optimization");
    return true;
}();

using tolstat::Dist;

// ドメイン別の既定のばらつき要因 (mock の sources[domain] を数値パラメータへ
// 整理したもの)。中心は「公称値からの偏差 0」を既定とし、絶対値で語るのが
// 自然な湿度だけ中心 50 %RH にしてある。
struct SourceDef {
    bool        ck;
    const char *nameKey;
    Dist        dist;
    double      center;
    double      spread;
    const char *unit;      // 直接表示する短い単位 (nullptr = i18n キー使用)
    const char *unitKey;
};

const SourceDef kEmSrc[] = {
    { true,  "tol_i_patch",  Dist::Normal,  0.0, 0.05,  "mm", nullptr },
    { true,  "tol_i_epsr",   Dist::Normal,  0.0, 0.05,  nullptr, "tol_u_none" },
    { true,  "tol_i_subth",  Dist::Normal,  0.0, 0.025, "mm", nullptr },
    { true,  "tol_i_feed",   Dist::Normal,  0.0, 0.1,   "mm", nullptr },
    { false, "tol_i_solder", Dist::Uniform, 0.0, 0.1,   "mm", nullptr },
    { false, "tol_i_temp",   Dist::Normal,  0.0, 10.0,  "K",  nullptr },
};
const SourceDef kOptSrc[] = {
    { true,  "tol_i_wgw",   Dist::Normal,   0.0, 5.0,   "nm", nullptr },
    { true,  "tol_i_wgt",   Dist::Normal,   0.0, 3.0,   "nm", nullptr },
    { true,  "tol_i_rough", Dist::Rayleigh, 0.0, 2.5,   nullptr, "tol_u_nmrms" },
    { true,  "tol_i_gap",   Dist::Normal,   0.0, 10.0,  "nm", nullptr },
    { false, "tol_i_index", Dist::Normal,   0.0, 0.001, nullptr, "tol_u_none" },
    { true,  "tol_i_temp",  Dist::Normal,   0.0, 5.0,   "K",  nullptr },
};
const SourceDef kAcSrc[] = {
    { true,  "tol_i_alpha", Dist::Normal,  0.0,  0.05, nullptr, "tol_u_none" },
    { true,  "tol_i_wall",  Dist::Normal,  0.0,  0.1,  "m",   nullptr },
    { true,  "tol_i_occ",   Dist::Uniform, 0.0,  20.0, "%",   nullptr },
    { false, "tol_i_room",  Dist::Normal,  0.0,  5.0,  "K",   nullptr },
    { false, "tol_i_humid", Dist::Uniform, 50.0, 20.0, "%RH", nullptr },
};
const SourceDef kUwSrc[] = {
    { true,  "tol_i_ssp",    Dist::Normal,   0.0, 5.0,  "m/s", nullptr },
    { true,  "tol_i_bottom", Dist::Discrete, 0.0, 0.0,  nullptr, "tol_u_bottom" },
    { true,  "tol_i_water",  Dist::Normal,   0.0, 2.0,  "K",   nullptr },
    { false, "tol_i_salt",   Dist::Normal,   0.0, 0.5,  "psu", nullptr },
    { false, "tol_i_wave",   Dist::Rayleigh, 0.0, 1.5,  "m",   nullptr },
};

// mock の criteria[domain] (合格条件。判定はソルバー結果が要るため未配線)
struct Criteria { const char *goal, *val, *at, *unit; };
const Criteria kEmCrit  = { "S11 ≤",    "-10",     "@ 2.45GHz",      "dB" };
const Criteria kOptCrit = { "透過率 ≥", "0.7",     "@ 1550 nm",      "—"  };
const Criteria kAcCrit  = { "RT60 (1kHz)", "1.0~1.8", "全座席",      "s"  };
const Criteria kUwCrit  = { "TL ≤",     "90",      "@ 50km, 3.5kHz", "dB" };

QString distName(Dist d)
{
    switch (d) {
    case Dist::Normal:   return I18n::tr("tol_d_normal");
    case Dist::Uniform:  return I18n::tr("tol_d_uniform");
    case Dist::Rayleigh: return I18n::tr("tol_d_rayleigh");
    case Dist::Discrete: break;
    }
    return I18n::tr("tol_d_discrete");
}

QLabel *makeBadge(const QString &text, const char *kind, QWidget *parent)
{
    auto *b = new QLabel(text, parent);
    b->setWordWrap(true);
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

QLabel *noteLabel(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setWordWrap(true);
    l->setStyleSheet("font-size:11px; color:palette(mid);");
    return l;
}

// 表示用の数値整形 (桁数が極端にならないように有効数字で出す)
QString num(double v) { return QString::number(v, 'g', 4); }

// ばらつき要因の表の列
enum SrcCol { ColCheck = 0, ColName, ColDist, ColCenter, ColSpread, ColUnit,
              SrcColCount };
} // namespace

// ── ToleranceTab ────────────────────────────────────────────────────────────
ToleranceTab::ToleranceTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // ── 製造ばらつき・歩留まり解析 (説明) ──────────────────────────────────
    m_titleSec = new SectionBox(QString(), body);
    m_titleSec->vbox()->addWidget(hintLabel(I18n::tr("tol_hint"), m_titleSec));
    v->addWidget(m_titleSec);

    // ── ばらつき要因 / Variation sources ───────────────────────────────────
    auto *sSrc = new SectionBox(I18n::tr("tol_sources"), body);
    m_sources = new QTableWidget(0, SrcColCount, sSrc);
    m_sources->setHorizontalHeaderLabels({ QString(), I18n::tr("tol_c_item"),
        I18n::tr("tol_c_dist"), I18n::tr("tol_c_center"),
        I18n::tr("tol_c_sigma"), I18n::tr("tol_c_unit") });
    m_sources->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_sources->horizontalHeader()->setSectionResizeMode(ColCheck, QHeaderView::Fixed);
    m_sources->horizontalHeader()->resizeSection(ColCheck, 24);
    m_sources->verticalHeader()->setVisible(false);
    m_sources->setMinimumHeight(200);
    connect(m_sources, &QTableWidget::itemChanged,
            this, &ToleranceTab::onSourceEdited);
    sSrc->vbox()->addWidget(m_sources);
    // 表は分布表示に使われる。保存・ソルバー連携が無いことは注記で明示する
    sSrc->vbox()->addWidget(noteLabel(I18n::tr("tol_src_note"), sSrc));
    v->addWidget(sSrc);

    // ── モンテカルロ設定 ───────────────────────────────────────────────────
    auto *sMc = new SectionBox(I18n::tr("tol_mc"), body);
    m_samples = new QLineEdit("1000", sMc);
    m_samples->setMaximumWidth(80);
    sMc->form()->addRow(I18n::tr("tol_samples"), m_samples);
    m_sampling = new QComboBox(sMc);
    m_sampling->addItem(I18n::tr("tol_random"));
    m_sampling->addItem(I18n::tr("tol_lhs"));
    m_sampling->addItem(I18n::tr("tol_sobol"));
    m_sampling->setCurrentIndex(1);          // 既定 "lhs"
    sMc->form()->addRow(I18n::tr("tol_method"), m_sampling);
    // サンプル数・サンプリング法はモンテカルロ未実装のためどこにも読まれない
    sMc->form()->addRow(tabhelp::unwiredNote(sMc));
    v->addWidget(sMc);

    // ── 合格条件 / Pass criteria ───────────────────────────────────────────
    auto *sCrit = new SectionBox(I18n::tr("tol_criteria"), body);
    auto *critRow = new QHBoxLayout();
    m_goal = new QLabel(sCrit);
    m_goalVal = new QLineEdit(sCrit);
    m_goalVal->setMaximumWidth(100);
    m_goalUnit = new QLabel(sCrit);
    m_goalAt = new QLabel(sCrit);
    critRow->addWidget(m_goal);
    critRow->addWidget(m_goalVal);
    critRow->addWidget(m_goalUnit);
    critRow->addWidget(m_goalAt);
    critRow->addStretch(1);
    sCrit->vbox()->addLayout(critRow);
    // 合格判定はソルバー結果が要るためどこにも読まれない
    sCrit->vbox()->addWidget(tabhelp::unwiredNote(sCrit));
    v->addWidget(sCrit);

    // ── 入力ばらつきの分布 / 結果 ──────────────────────────────────────────
    auto *sRes = new SectionBox(I18n::tr("tol_results"), body);
    sRes->vbox()->addWidget(hintLabel(I18n::tr("tol_lastrun"), sRes));
    m_varBox = new QComboBox(sRes);
    sRes->form()->addRow(I18n::tr("tol_var"), m_varBox);
    connect(m_varBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ToleranceTab::updateDistribution);

    auto *yRow = new QHBoxLayout();
    // 歩留まりは未計算 (モンテカルロ未実装) — 数値を出さない
    m_yield = makeBadge(I18n::tr("tol_yield_na"), "", sRes);
    m_yield->setWordWrap(false);   // バッジは 1 行で内容幅に合わせる
    yRow->addWidget(m_yield);
    yRow->addStretch(1);
    sRes->vbox()->addLayout(yRow);
    m_sigma3 = new QLabel(sRes);
    m_sigma3->setWordWrap(true);
    sRes->vbox()->addWidget(m_sigma3);

    m_hist = new MiniPlot(sRes);
    m_hist->setMinimumHeight(120);
    sRes->vbox()->addWidget(m_hist);
    // 何が実計算で何が未計算かを直下に明示する
    sRes->vbox()->addWidget(noteLabel(I18n::tr("tol_res_note"), sRes));

    auto *btnRow = new QHBoxLayout();
    // 3 ボタンとも未配線 → 無効化 + 「未実装」ツールチップ
    auto *reportBtn = new QPushButton(I18n::tr("tol_report"), sRes);
    auto *sensBtn   = new QPushButton(I18n::tr("tol_sensitivity"), sRes);
    auto *robustBtn = new QPushButton(I18n::tr("tol_robust"), sRes);
    for (QPushButton *b : { reportBtn, sensBtn, robustBtn }) {
        tabhelp::markNotImplemented(b);
        btnRow->addWidget(b);
    }
    btnRow->addStretch(1);
    sRes->vbox()->addLayout(btnRow);
    v->addWidget(sRes);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    connect(project, &Project::domainChanged, this, &ToleranceTab::rebuildDomain);
    rebuildDomain();
}

void ToleranceTab::rebuildDomain()
{
    const Domain d = m_p->activeDomain();
    m_titleSec->setTitle(I18n::tr("tol_title_fmt").arg(domainKey(d).toUpper()));

    // ── ばらつき要因 (ドメイン別の既定値から作り直す) ──────────────────────
    const SourceDef *defs = kEmSrc;
    int n = int(sizeof(kEmSrc) / sizeof(kEmSrc[0]));
    switch (d) {
        case Domain::Optical:
            defs = kOptSrc; n = int(sizeof(kOptSrc) / sizeof(kOptSrc[0])); break;
        case Domain::Acoustic:
            defs = kAcSrc;  n = int(sizeof(kAcSrc)  / sizeof(kAcSrc[0]));  break;
        case Domain::Underwater:
            defs = kUwSrc;  n = int(sizeof(kUwSrc)  / sizeof(kUwSrc[0]));  break;
        default:
            defs = kEmSrc;  n = int(sizeof(kEmSrc)  / sizeof(kEmSrc[0]));  break;
    }
    m_vars.clear();
    m_vars.reserve(n);
    for (int i = 0; i < n; ++i) {
        VarRow r;
        r.enabled      = defs[i].ck;
        r.name         = I18n::tr(defs[i].nameKey);
        r.unit         = defs[i].unit ? QString::fromUtf8(defs[i].unit)
                                      : I18n::tr(defs[i].unitKey);
        r.var.dist     = defs[i].dist;
        r.var.center   = defs[i].center;
        r.var.spread   = defs[i].spread;
        m_vars.append(r);
    }
    fillSourceTable();

    // ── 合格条件 ───────────────────────────────────────────────────────────
    const Criteria &c = (d == Domain::Optical)    ? kOptCrit
                      : (d == Domain::Acoustic)   ? kAcCrit
                      : (d == Domain::Underwater) ? kUwCrit
                                                  : kEmCrit;
    m_goal->setText(QString::fromUtf8(c.goal));
    m_goalVal->setText(QString::fromUtf8(c.val));
    m_goalUnit->setText(QString::fromUtf8(c.unit));
    m_goalAt->setText(QString::fromUtf8(c.at));

    refreshVarChoices();
    updateDistribution();
}

// m_vars → 表 (再入防止のため m_updating で囲む)
void ToleranceTab::fillSourceTable()
{
    m_updating = true;
    m_sources->setRowCount(m_vars.size());
    for (int r = 0; r < m_vars.size(); ++r) {
        const VarRow &row = m_vars[r];
        // 離散変数は連続分布のパラメータを持たない (数値欄は編集させない)
        const bool cont = (row.var.dist != tolstat::Dist::Discrete);

        auto *ck = new QTableWidgetItem;
        ck->setCheckState(row.enabled ? Qt::Checked : Qt::Unchecked);
        ck->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        m_sources->setItem(r, ColCheck, ck);

        auto *name = new QTableWidgetItem(row.name);
        name->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        m_sources->setItem(r, ColName, name);

        auto *dist = new QTableWidgetItem(distName(row.var.dist));
        dist->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        m_sources->setItem(r, ColDist, dist);

        // 離散変数は連続分布のパラメータを持たない → 数値欄は "—" で編集不可
        const QString dash = I18n::tr("tol_u_none");
        auto *center = new QTableWidgetItem(cont ? num(row.var.center) : dash);
        center->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        center->setFlags(cont ? (Qt::ItemIsEnabled | Qt::ItemIsSelectable |
                                 Qt::ItemIsEditable)
                              : (Qt::ItemIsEnabled | Qt::ItemIsSelectable));
        m_sources->setItem(r, ColCenter, center);

        auto *spread = new QTableWidgetItem(cont ? num(row.var.spread) : dash);
        spread->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        spread->setFlags(center->flags());
        m_sources->setItem(r, ColSpread, spread);

        auto *unit = new QTableWidgetItem(row.unit);
        unit->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        m_sources->setItem(r, ColUnit, unit);
    }
    m_updating = false;
}

// 中心 / σ の編集とチェック状態の変更を取り込む。
// 不正な入力は表示を元の値へ戻す (UI と内部状態を乖離させない)。
void ToleranceTab::onSourceEdited(QTableWidgetItem *item)
{
    if (m_updating || !item) return;
    const int r = item->row();
    if (r < 0 || r >= m_vars.size()) return;
    VarRow &row = m_vars[r];

    if (item->column() == ColCheck) {
        row.enabled = (item->checkState() == Qt::Checked);
        refreshVarChoices();
        updateDistribution();
        return;
    }
    if (item->column() != ColCenter && item->column() != ColSpread) return;

    bool ok = false;
    const double val = item->text().trimmed().toDouble(&ok);
    const bool isSpread = (item->column() == ColSpread);
    // σ / 半幅は正の有限値のみ (0 以下は分布が定義できない)
    if (ok && std::isfinite(val) && (!isSpread || val > 0.0)) {
        if (isSpread) row.var.spread = val;
        else          row.var.center = val;
    }
    // 受理・不受理いずれの場合も、内部状態の値で表示を書き戻す
    m_updating = true;
    item->setText(num(isSpread ? row.var.spread : row.var.center));
    m_updating = false;
    updateDistribution();
}

// 有効かつ連続分布の変数だけを選択コンボへ入れる
void ToleranceTab::refreshVarChoices()
{
    const QString prev = m_varBox->currentText();
    m_updating = true;
    m_varBox->clear();
    for (int i = 0; i < m_vars.size(); ++i) {
        const VarRow &row = m_vars[i];
        if (!row.enabled || !tolstat::isContinuous(row.var)) continue;
        m_varBox->addItem(QStringLiteral("%1 [%2]").arg(row.name, row.unit), i);
    }
    m_updating = false;
    const int idx = m_varBox->findText(prev);
    if (idx >= 0) m_varBox->setCurrentIndex(idx);
}

// 選択中の変数の確率密度と被覆区間を計算して表示する (実計算)
void ToleranceTab::updateDistribution()
{
    if (m_updating) return;

    const int idx = m_varBox->currentIndex();
    const int vi = (idx >= 0) ? m_varBox->itemData(idx).toInt() : -1;
    if (vi < 0 || vi >= m_vars.size() ||
        !tolstat::isContinuous(m_vars[vi].var)) {
        // 描くものが無いときは空の軸を残さずグラフごと隠す
        m_hist->setSeries({});
        m_hist->setLabels(I18n::tr("tol_var_none"), I18n::tr("tol_density"));
        m_hist->setVisible(false);
        m_sigma3->setText(I18n::tr("tol_3sigma_na"));
        return;
    }

    const VarRow &row = m_vars[vi];
    const std::vector<tolstat::Point> curve = tolstat::pdfCurve(row.var);
    MiniSeries s;
    s.color = QColor("#0078D4");
    s.pts.reserve(int(curve.size()));
    for (const tolstat::Point &p : curve) s.pts.append(QPointF(p.x, p.y));
    m_hist->setSeries({ s });
    m_hist->setLabels(QStringLiteral("%1 [%2]").arg(row.name, row.unit),
                      I18n::tr("tol_density"));
    m_hist->setVisible(true);

    // 被覆確率は正規分布の ±3σ に合わせる (分布の形が違っても比較できる)
    const double k = 3.0;
    const tolstat::Interval iv = tolstat::coverageInterval(row.var, k);
    m_sigma3->setText(I18n::tr("tol_3sigma_fmt")
        .arg(100.0 * tolstat::normalCoverage(k), 0, 'f', 2)
        .arg(num(iv.lo)).arg(num(iv.hi)).arg(row.unit)
        .arg(num(tolstat::stdDev(row.var)))
        .arg(num(tolstat::mean(row.var))));
}
