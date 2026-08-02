// ThinFilmTab.cpp
#include "ThinFilmTab.h"
#include "../core/Project.h"
#include "../widgets/MiniPlot.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"
#include "../Theme.h"

#include <QButtonGroup>
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
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include <cmath>

using namespace ofd;

// ── タブ専用語彙 (file-local 登録, 接頭辞 tfc_) ──────────────────────────────
namespace {
const bool s_i18n = [] {
    using ofd::I18n;
    // 上段
    I18n::reg("tfc_title", "薄膜多層膜設計 / Multilayer coating design",
              "Multilayer coating design");
    I18n::reg("tfc_hint",
              "等価アドミッタンス法 (Abeles行列) で高速計算、必要に応じFDTD/RCWAで検証。"
              "Essential Macleod / OptiLayer / TFCalc 相当の設計環境。",
              "Fast evaluation with the equivalent-admittance (Abeles matrix) method, "
              "verified with FDTD/RCWA when needed. An Essential Macleod / OptiLayer / "
              "TFCalc class design environment.");
    I18n::reg("tfc_preset", "プリセット", "Preset");
    I18n::reg("tfc_layers_n", "%1 層", "%1 layers");
    I18n::reg("tfc_target_fmt", "目標: %1", "Target: %1");

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
    I18n::reg("tfc_sub_bk7",      "N-BK7 (n=1.5168)", "N-BK7 (n=1.5168)");
    I18n::reg("tfc_sub_sio2",     "溶融石英 SiO₂", "Fused silica SiO₂");
    I18n::reg("tfc_sub_si",       "Si (赤外)", "Si (infrared)");
    I18n::reg("tfc_sub_sapphire", "サファイア", "Sapphire");
    I18n::reg("tfc_sub_pc",       "ポリカーボネート", "Polycarbonate");
    I18n::reg("tfc_incident", "入射媒質: 空気 (n=1)", "Incident medium: air (n=1)");
    I18n::reg("tfc_c_mat",   "材料", "Material");
    I18n::reg("tfc_c_n",     "n @550nm", "n @550nm");
    I18n::reg("tfc_c_dphys", "物理膜厚 [nm]", "Physical thickness [nm]");
    I18n::reg("tfc_c_qwot",  "光学膜厚 (QWOT)", "Optical thickness (QWOT)");
    I18n::reg("tfc_c_role",  "役割", "Role");
    I18n::reg("tfc_r_outer", "最外層 (低屈折率)", "Outermost layer (low index)");
    I18n::reg("tfc_r_high",  "高屈折率", "High index");
    I18n::reg("tfc_r_match", "整合層", "Matching layer");
    I18n::reg("tfc_r_sub",   "基板側", "Substrate side");
    I18n::reg("tfc_layer_add", "＋ 層を追加 / (H L)^N 周期記法で入力…",
              "+ Add a layer / enter (H L)^N periodic notation…");
    I18n::reg("tfc_periodic", "周期記法", "Periodic notation");
    I18n::reg("tfc_expand", "展開", "Expand");
    I18n::reg("tfc_dispersion", "材料の分散 (n,k λ依存) を使用",
              "Use material dispersion (λ-dependent n, k)");
    I18n::reg("tfc_absorption", "吸収 (k) を考慮", "Include absorption (k)");
    I18n::reg("tfc_matsource", "材料データ: ガラスカタログ/材料Explorer と共有",
              "Material data shared with the glass catalog / material explorer");

    // 分光特性
    I18n::reg("tfc_spec_sec", "分光特性 / Spectral response", "Spectral response");
    I18n::reg("tfc_aoi", "入射角", "Angle of incidence");
    I18n::reg("tfc_aoi_unit", "° · ", "° · ");
    I18n::reg("tfc_angle_sweep", "角度スイープ 0-60°", "Angle sweep 0-60°");
    I18n::reg("tfc_split_sp", "s/p 偏光を分離", "Separate s / p polarization");
    I18n::reg("tfc_lam_range", "波長範囲", "Wavelength range");
    I18n::reg("tfc_y_r", "R [%]", "R [%]");
    I18n::reg("tfc_y_t", "T [%]", "T [%]");
    I18n::reg("tfc_c_metric", "指標", "Metric");
    I18n::reg("tfc_c_value",  "値", "Value");
    I18n::reg("tfc_c_target", "目標", "Target");
    I18n::reg("tfc_c_judge",  "判定", "Verdict");
    I18n::reg("tfc_met", "達成", "Met");
    I18n::reg("tfc_m_ravg",  "平均反射率 (450-650nm)",
              "Average reflectance (450-650 nm)");
    I18n::reg("tfc_m_rmax",  "最大反射率", "Peak reflectance");
    I18n::reg("tfc_m_od",    "光学濃度 (阻止域)", "Optical density (blocking band)");
    I18n::reg("tfc_m_shift", "色シフト (Δu'v' @45°)", "Color shift (Δu'v' @45°)");
    I18n::reg("tfc_m_gdr",   "群遅延リップル", "Group-delay ripple");
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
    I18n::reg("tfc_c_lamrange", "λ範囲", "λ range");
    I18n::reg("tfc_c_quantity", "量", "Quantity");
    I18n::reg("tfc_c_tol",      "許容", "Tolerance");
    I18n::reg("tfc_c_weight",   "重み", "Weight");
    I18n::reg("tfc_run_opt", "▶ 最適化実行", "▶ Run optimization");
    I18n::reg("tfc_merit", "Merit = 0.0184 (42反復で収束)",
              "Merit = 0.0184 (converged in 42 iterations)");

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
    I18n::reg("tfc_yield", "歩留まり 92.4%", "Yield 92.4%");
    I18n::reg("tfc_sensitive", "最も敏感な層: #2 (ZrO₂) — 感度 0.42 %R/nm",
              "Most sensitive layer: #2 (ZrO₂) — sensitivity 0.42 %R/nm");
    I18n::reg("tfc_btn_recipe", "📄 成膜レシピ書出 (装置向け)",
              "📄 Export deposition recipe (for the coater)");
    I18n::reg("tfc_btn_sens", "📊 感度解析", "📊 Sensitivity analysis");
    return true;
}();

// ── モックの PRESETS マップ (順序・値をそのまま) ─────────────────────────────
struct Preset { const char *key; const char *nameKey; int layers; const char *targetKey; };
const Preset kPresets[6] = {
    { "ar",   "tfc_p_ar",    4, "tfc_t_ar"   },
    { "dbr",  "tfc_p_dbr",  25, "tfc_t_dbr"  },
    { "bpf",  "tfc_p_bpf",  42, "tfc_t_bpf"  },
    { "dich", "tfc_p_dich", 18, "tfc_t_dich" },
    { "heat", "tfc_p_heat",  9, "tfc_t_heat" },
    { "pol",  "tfc_p_pol",  21, "tfc_t_pol"  },
};

// 層スタック表 (モックの <tbody> をそのまま)
struct LayerRow { const char *num, *mat, *n, *d, *qwot, *roleKey; };
const LayerRow kLayers[4] = {
    { "1", "MgF₂",  "1.384", "99.3",  "0.250", "tfc_r_outer" },
    { "2", "ZrO₂",  "2.050", "134.1", "0.500", "tfc_r_high"  },
    { "3", "Al₂O₃", "1.660", "41.4",  "0.125", "tfc_r_match" },
    { "4", "ZrO₂",  "2.050", "67.1",  "0.250", "tfc_r_sub"   },
};

// 指標判定表 (モックの数値をそのまま)
struct MetricRow { const char *itemKey, *value, *target; };
const MetricRow kSpecRows[5] = {
    { "tfc_m_ravg",  "0.32 %",  "< 0.5"   },
    { "tfc_m_rmax",  "0.71 %",  "< 1.0"   },
    { "tfc_m_od",    "OD 4.2",  "> 4.0"   },
    { "tfc_m_shift", "0.008",   "< 0.010" },
    { "tfc_m_gdr",   "±3.2 ps", "±5"      },
};

// 最適化ターゲット表 (モックの <tbody> をそのまま)
struct TargetRow { const char *range, *quantity, *goal, *tol, *weight; };
const TargetRow kTargets[2] = {
    { "450-650nm", "R", "0%",  "0.5%", "1.0" },
    { "700-750nm", "R", "<2%", "2%",   "0.3" },
};

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

// 表セル内バッジ (左寄せ)
QWidget *badgeCell(const QString &text, const char *kind)
{
    auto *w = new QWidget;
    auto *h = new QHBoxLayout(w);
    h->setContentsMargins(4, 2, 4, 2);
    h->addWidget(makeBadge(text, kind, w));
    h->addStretch(1);
    return w;
}

QLabel *hintLabel(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setTextFormat(Qt::PlainText);
    l->setWordWrap(true);
    return l;
}

QLineEdit *numEdit(const char *value, int width, QWidget *parent)
{
    auto *e = new QLineEdit(QString::fromUtf8(value), parent);
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
} // namespace

// ── ThinFilmTab ─────────────────────────────────────────────────────────────
ThinFilmTab::ThinFilmTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project),
      m_preset(nullptr), m_layerBadge(nullptr), m_targetLabel(nullptr),
      m_tabs(nullptr),
      m_substrate(nullptr), m_layerTable(nullptr), m_periodic(nullptr),
      m_useDispersion(nullptr), m_useAbsorption(nullptr),
      m_aoi(nullptr), m_angleSweep(nullptr), m_splitSP(nullptr),
      m_lamMin(nullptr), m_lamMax(nullptr), m_specPlot(nullptr),
      m_specTable(nullptr),
      m_method(nullptr), m_varThickness(nullptr), m_varCount(nullptr),
      m_varMaterial(nullptr), m_targetTable(nullptr),
      m_deposition(nullptr), m_thickErr(nullptr), m_systematic(nullptr),
      m_correlated(nullptr), m_monitoring(nullptr), m_yieldBadge(nullptr),
      m_sensitiveLabel(nullptr)
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

// プリセット → 層数バッジ / 目標 / スペクトル
void ThinFilmTab::presetChanged(int index)
{
    index = qBound(0, index, 5);
    m_layerBadge->setText(I18n::tr("tfc_layers_n").arg(kPresets[index].layers));
    m_targetLabel->setText(
        I18n::tr("tfc_target_fmt").arg(I18n::tr(kPresets[index].targetKey)));
    updateSpecPlot();
}

// ── 層構成 / Layer stack ────────────────────────────────────────────────────
QWidget *ThinFilmTab::buildStackPage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setSpacing(8);

    auto *s = new SectionBox(I18n::tr("tfc_stack_sec"), page);

    // 基板 (defaultValue="bk7" → 先頭)
    auto *subRow = new QHBoxLayout();
    m_substrate = new QComboBox(s);
    m_substrate->addItem(I18n::tr("tfc_sub_bk7"));
    m_substrate->addItem(I18n::tr("tfc_sub_sio2"));
    m_substrate->addItem(I18n::tr("tfc_sub_si"));
    m_substrate->addItem(I18n::tr("tfc_sub_sapphire"));
    m_substrate->addItem(I18n::tr("tfc_sub_pc"));
    subRow->addWidget(m_substrate);
    subRow->addWidget(new QLabel(I18n::tr("tfc_incident"), s));
    subRow->addStretch(1);
    s->form()->addRow(I18n::tr("tfc_substrate"), subRow);

    // 層スタック表 (4層 + 「層を追加」行)
    m_layerTable = new QTableWidget(5, 7, s);
    m_layerTable->setHorizontalHeaderLabels({ QString(), "#", I18n::tr("tfc_c_mat"),
                                              I18n::tr("tfc_c_n"),
                                              I18n::tr("tfc_c_dphys"),
                                              I18n::tr("tfc_c_qwot"),
                                              I18n::tr("tfc_c_role") });
    m_layerTable->verticalHeader()->setVisible(false);
    m_layerTable->verticalHeader()->setDefaultSectionSize(24);
    m_layerTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_layerTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_layerTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_layerTable->setMaximumHeight(170);

    for (int r = 0; r < 4; ++r) {
        const LayerRow &L = kLayers[r];
        auto *sel = new QTableWidgetItem();
        sel->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        sel->setCheckState(Qt::Checked);            // defaultChecked
        m_layerTable->setItem(r, 0, sel);
        m_layerTable->setItem(r, 1, numItem(QString::fromUtf8(L.num)));
        m_layerTable->setItem(r, 2, textItem(QString::fromUtf8(L.mat)));
        m_layerTable->setItem(r, 3, numItem(QString::fromUtf8(L.n)));
        // 物理膜厚だけ編集可 (モックの <input className="cell-input">)
        auto *d = new QTableWidgetItem(QString::fromUtf8(L.d));
        d->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_layerTable->setItem(r, 4, d);
        m_layerTable->setItem(r, 5, numItem(QString::fromUtf8(L.qwot)));
        m_layerTable->setItem(r, 6, textItem(I18n::tr(L.roleKey)));
    }
    // 追加行 (チェック無し + 6列結合のイタリック行)
    auto *addSel = new QTableWidgetItem();
    addSel->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
    addSel->setCheckState(Qt::Unchecked);
    m_layerTable->setItem(4, 0, addSel);
    auto *addIt = textItem(I18n::tr("tfc_layer_add"));
    QFont italic = addIt->font();
    italic.setItalic(true);
    addIt->setFont(italic);
    m_layerTable->setItem(4, 1, addIt);
    m_layerTable->setSpan(4, 1, 1, 6);
    s->vbox()->addWidget(m_layerTable);

    // 表より後ろの <Row> は独立フォームへ (順序をモックどおりに保つ)
    QFormLayout *f2 = appendForm(s);

    // 周期記法
    auto *perRow = new QHBoxLayout();
    m_periodic = numEdit("Air | (H L)^12 H | Sub    H=TiO2 L=SiO2 @ 1550nm", 0, s);
    perRow->addWidget(m_periodic, 1);
    perRow->addWidget(new QPushButton(I18n::tr("tfc_expand"), s));
    f2->addRow(I18n::tr("tfc_periodic"), perRow);

    // 材料オプション
    auto *optRow = new QHBoxLayout();
    m_useDispersion = makeCheck(I18n::tr("tfc_dispersion"), true, s);
    m_useAbsorption = makeCheck(I18n::tr("tfc_absorption"), true, s);
    optRow->addWidget(m_useDispersion);
    optRow->addWidget(m_useAbsorption);
    optRow->addWidget(new QLabel(I18n::tr("tfc_matsource"), s));
    optRow->addStretch(1);
    f2->addRow(optRow);
    v->addWidget(s);

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
    m_aoi = numEdit("0", 70, s);
    aoiRow->addWidget(m_aoi);
    aoiRow->addWidget(new QLabel(I18n::tr("tfc_aoi_unit"), s));
    m_angleSweep = makeCheck(I18n::tr("tfc_angle_sweep"), false, s);
    m_splitSP    = makeCheck(I18n::tr("tfc_split_sp"),    true,  s);
    aoiRow->addWidget(m_angleSweep);
    aoiRow->addWidget(m_splitSP);
    aoiRow->addStretch(1);
    s->form()->addRow(I18n::tr("tfc_aoi"), aoiRow);

    auto *lamRow = new QHBoxLayout();
    m_lamMin = numEdit("400", 70, s);
    m_lamMax = numEdit("800", 70, s);
    lamRow->addWidget(m_lamMin);
    lamRow->addWidget(new QLabel("〜", s));
    lamRow->addWidget(m_lamMax);
    lamRow->addWidget(new QLabel("nm", s));
    lamRow->addStretch(1);
    s->form()->addRow(I18n::tr("tfc_lam_range"), lamRow);

    // MiniPlot (プリセット依存 — updateSpecPlot() で生成)
    m_specPlot = new MiniPlot(s);
    m_specPlot->setMinimumSize(360, 140);
    s->vbox()->addWidget(m_specPlot);

    // 指標判定表
    m_specTable = new QTableWidget(5, 4, s);
    m_specTable->setHorizontalHeaderLabels({ I18n::tr("tfc_c_metric"),
                                             I18n::tr("tfc_c_value"),
                                             I18n::tr("tfc_c_target"),
                                             I18n::tr("tfc_c_judge") });
    m_specTable->verticalHeader()->setVisible(false);
    m_specTable->verticalHeader()->setDefaultSectionSize(26);
    m_specTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_specTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_specTable->setMaximumHeight(190);
    for (int r = 0; r < 5; ++r) {
        const MetricRow &m = kSpecRows[r];
        m_specTable->setItem(r, 0, textItem(I18n::tr(m.itemKey)));
        m_specTable->setItem(r, 1, numItem(QString::fromUtf8(m.value)));
        m_specTable->setItem(r, 2, numItem(QString::fromUtf8(m.target)));
        m_specTable->setCellWidget(r, 3, badgeCell(I18n::tr("tfc_met"), "ok"));
    }
    s->vbox()->addWidget(m_specTable);

    auto *btnRow = new QHBoxLayout();
    btnRow->addWidget(new QPushButton(I18n::tr("tfc_btn_rta"), s));
    btnRow->addWidget(new QPushButton(I18n::tr("tfc_btn_map"), s));
    btnRow->addWidget(new QPushButton(I18n::tr("tfc_btn_field"), s));
    btnRow->addWidget(new QPushButton(I18n::tr("tfc_btn_fdtd"), s));
    btnRow->addStretch(1);
    s->vbox()->addLayout(btnRow);
    v->addWidget(s);

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
                      1, s);                     // 既定 "needle"
    s->form()->addRow(I18n::tr("tfc_method"), mRow);

    auto *vRow = new QHBoxLayout();
    m_varThickness = makeCheck(I18n::tr("tfc_v_thick"), true,  s);
    m_varCount     = makeCheck(I18n::tr("tfc_v_count"), true,  s);
    m_varMaterial  = makeCheck(I18n::tr("tfc_v_mat"),   false, s);
    vRow->addWidget(m_varThickness);
    vRow->addWidget(m_varCount);
    vRow->addWidget(m_varMaterial);
    vRow->addStretch(1);
    s->form()->addRow(I18n::tr("tfc_vars"), vRow);

    // ターゲット表
    m_targetTable = new QTableWidget(2, 5, s);
    m_targetTable->setHorizontalHeaderLabels({ I18n::tr("tfc_c_lamrange"),
                                               I18n::tr("tfc_c_quantity"),
                                               I18n::tr("tfc_c_target"),
                                               I18n::tr("tfc_c_tol"),
                                               I18n::tr("tfc_c_weight") });
    m_targetTable->verticalHeader()->setVisible(false);
    m_targetTable->verticalHeader()->setDefaultSectionSize(24);
    m_targetTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_targetTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_targetTable->setMaximumHeight(100);
    for (int r = 0; r < 2; ++r) {
        const TargetRow &t = kTargets[r];
        m_targetTable->setItem(r, 0, monoItem(QString::fromUtf8(t.range)));
        m_targetTable->setItem(r, 1, textItem(QString::fromUtf8(t.quantity)));
        m_targetTable->setItem(r, 2, numItem(QString::fromUtf8(t.goal)));
        m_targetTable->setItem(r, 3, numItem(QString::fromUtf8(t.tol)));
        m_targetTable->setItem(r, 4, numItem(QString::fromUtf8(t.weight)));
    }
    s->form()->addRow(I18n::tr("tfc_targets"), m_targetTable);

    auto *runRow = new QHBoxLayout();
    auto *runBtn = new QPushButton(I18n::tr("tfc_run_opt"), s);
    runBtn->setDefault(true);                    // q-btn primary
    runRow->addWidget(runBtn);
    runRow->addWidget(new QLabel(I18n::tr("tfc_merit"), s));
    runRow->addStretch(1);
    s->vbox()->addLayout(runRow);
    v->addWidget(s);

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
    s->form()->addRow(I18n::tr("tfc_depo"), dRow);

    auto *eRow = new QHBoxLayout();
    m_thickErr = numEdit("0.5", 70, s);
    eRow->addWidget(m_thickErr);
    eRow->addWidget(new QLabel(I18n::tr("tfc_thickerr_unit"), s));
    eRow->addStretch(1);
    s->form()->addRow(I18n::tr("tfc_thickerr"), eRow);

    auto *cRow = new QHBoxLayout();
    m_systematic = makeCheck(I18n::tr("tfc_systematic"), true,  s);
    m_correlated = makeCheck(I18n::tr("tfc_correlated"), false, s);
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
    s->form()->addRow(I18n::tr("tfc_monitor"), monRow);

    auto *mcRow = new QHBoxLayout();
    auto *mcBtn = new QPushButton(I18n::tr("tfc_run_mc"), s);
    mcBtn->setDefault(true);                     // q-btn primary
    mcRow->addWidget(mcBtn);
    mcRow->addStretch(1);
    s->vbox()->addLayout(mcRow);

    auto *yRow = new QHBoxLayout();
    m_yieldBadge = makeBadge(I18n::tr("tfc_yield"), "ok", s);
    yRow->addWidget(m_yieldBadge);
    m_sensitiveLabel = hintLabel(I18n::tr("tfc_sensitive"), s);
    yRow->addWidget(m_sensitiveLabel);
    yRow->addStretch(1);
    s->vbox()->addLayout(yRow);

    auto *btnRow = new QHBoxLayout();
    btnRow->addWidget(new QPushButton(I18n::tr("tfc_btn_recipe"), s));
    btnRow->addWidget(new QPushButton(I18n::tr("tfc_btn_sens"), s));
    btnRow->addStretch(1);
    s->vbox()->addLayout(btnRow);
    v->addWidget(s);

    v->addStretch(1);
    return page;
}

// モック: lam = 400 + i*6.7 (60点)
//   preset==="ar" → y = 0.4 + ((lam-550)/230)^4 * 3.2   yLabel "R [%]"
//   それ以外      → y = |cos((lam-550)/60)| * 95        yLabel "T [%]"
void ThinFilmTab::updateSpecPlot()
{
    if (!m_specPlot) return;
    const bool ar = (m_preset->currentIndex() == 0);      // kPresets[0] == "ar"

    MiniSeries sp;
    sp.color = QColor("#0078D4");                         // var(--acc)
    for (int i = 0; i < 60; ++i) {
        const double lam = 400.0 + i * 6.7;
        const double y = ar
            ? 0.4 + std::pow((lam - 550.0) / 230.0, 4) * 3.2
            : std::fabs(std::cos((lam - 550.0) / 60.0)) * 95.0;
        sp.pts.push_back({ lam, y });
    }
    m_specPlot->setSeries({ sp });
    m_specPlot->setLabels("λ [nm]", I18n::tr(ar ? "tfc_y_r" : "tfc_y_t"));
}
