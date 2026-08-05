// SarTab.cpp
#include "SarTab.h"
#include "TabHelpers.h"
#include "../core/Project.h"
#include "../em/SarMetrics.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"
#include "../Theme.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QColor>
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
#include <QVector>

using namespace ofd;

// ── タブ固有語彙 (sar_) — file-local 登録 ───────────────────────────────────
namespace {
const bool s_i18n = [] {
    using ofd::I18n;
    // タブ名 (MainWindow 配線用)
    I18n::reg("t_sar", "🧠 SAR / 生体", "🧠 SAR / bio");

    // 概要
    I18n::reg("sar_title", "SAR / 生体電磁 解析 (IEC 62704 / IEEE 1528)",
              "SAR / bio-electromagnetics (IEC 62704 / IEEE 1528)");
    I18n::reg("sar_hint",
              "人体内の比吸収率 (SAR) を算出し、電波防護指針への適合を評価。"
              "OpenFDTD本家の代表的用途。\n"
              "(SAR 計算は未実装 — 画面は設計モック)",
              "Computes the specific absorption rate (SAR) inside the human body "
              "and checks it against the RF exposure guidelines — a flagship use "
              "of the original OpenFDTD.\n"
              "(SAR computation is not implemented — this screen is a design "
              "mock.)");
    I18n::reg("sar_model", "人体モデル", "Human model");
    I18n::reg("sar_model_head", "SAM頭部ファントム", "SAM head phantom");
    I18n::reg("sar_model_flat", "平板ファントム", "Flat phantom");
    I18n::reg("sar_model_voxel", "数値人体モデル (voxel)",
              "Numerical human model (voxel)");

    // 数値人体モデル (voxel)
    I18n::reg("sar_voxel_model", "モデル", "Model");
    I18n::reg("sar_vx_taro", "TARO (成人男性 2mm, NICT)",
              "TARO (adult male, 2 mm, NICT)");
    I18n::reg("sar_vx_hanako", "HANAKO (成人女性 2mm, NICT)",
              "HANAKO (adult female, 2 mm, NICT)");
    I18n::reg("sar_vx_duke", "Duke / Ella (Virtual Family, IT'IS)",
              "Duke / Ella (Virtual Family, IT'IS)");
    I18n::reg("sar_vx_child", "子供モデル (3歳/7歳)",
              "Child models (3 / 7 years old)");
    I18n::reg("sar_vx_icrp", "ICRP 標準ファントム", "ICRP reference phantom");
    I18n::reg("sar_tissues", "組織数", "Tissue count");
    I18n::reg("sar_tissues_val", "51 組織", "51 tissues");
    I18n::reg("sar_gabriel", "周波数依存の誘電率 (Gabriel モデル)",
              "Frequency-dependent permittivity (Gabriel model)");

    // SAM頭部ファントム
    I18n::reg("sar_liquid", "液剤", "Tissue-equivalent liquid");
    I18n::reg("sar_liq_head", "頭部液剤 (HSL)", "Head simulating liquid (HSL)");
    I18n::reg("sar_liq_body", "体部液剤 (BSL)", "Body simulating liquid (BSL)");
    I18n::reg("sar_gap", "離隔距離", "Separation distance");
    I18n::reg("sar_gap_unit", "mm (Touch / Tilt 15°)", "mm (Touch / Tilt 15°)");

    // 曝露源
    I18n::reg("sar_src_section", "曝露源 / Exposure source", "Exposure source");
    I18n::reg("sar_device", "機器", "Device");
    I18n::reg("sar_dev_phone", "スマートフォン (内蔵アンテナ)",
              "Smartphone (internal antenna)");
    I18n::reg("sar_dev_wpt", "ワイヤレス給電コイル (WPT)",
              "Wireless power transfer coil (WPT)");
    I18n::reg("sar_dev_bs", "基地局アンテナ (遠方界)",
              "Base-station antenna (far field)");
    I18n::reg("sar_dev_mri", "MRI RFコイル", "MRI RF coil");
    I18n::reg("sar_dev_wear", "ウェアラブル/イヤホン", "Wearable / earbuds");
    I18n::reg("sar_freq", "周波数", "Frequency");
    I18n::reg("sar_multiband", "複数バンド一括 (700M/1.9G/2.4G/3.5G/28G)",
              "All bands at once (700 M / 1.9 G / 2.4 G / 3.5 G / 28 G)");
    I18n::reg("sar_tx_power", "送信電力", "Transmit power");
    I18n::reg("sar_tx_power_unit", "dBm (最大送信条件)",
              "dBm (maximum transmit condition)");

    // 評価指標
    I18n::reg("sar_metrics_section", "評価指標 / Metrics", "Metrics");
    I18n::reg("sar_col_metric", "指標", "Metric");
    I18n::reg("sar_col_value", "算出値", "Computed");
    I18n::reg("sar_col_limit", "指針値", "Guideline");
    I18n::reg("sar_col_std", "規格", "Standard");
    I18n::reg("sar_col_verdict", "判定", "Verdict");
    I18n::reg("sar_m_10g", "ピーク空間平均 SAR (10g)",
              "Peak spatial-average SAR (10 g)");
    I18n::reg("sar_m_1g", "ピーク空間平均 SAR (1g)",
              "Peak spatial-average SAR (1 g)");
    I18n::reg("sar_m_wb", "全身平均 SAR", "Whole-body average SAR");
    I18n::reg("sar_m_pd", "入射電力密度 (>6GHz)",
              "Incident power density (>6 GHz)");
    I18n::reg("sar_m_temp", "温度上昇 (BioHeat連成)",
              "Temperature rise (BioHeat coupling)");
    I18n::reg("sar_std_icnirp_gp", "ICNIRP 一般環境",
              "ICNIRP general public");
    I18n::reg("sar_std_fcc", "FCC / IEEE C95.1", "FCC / IEEE C95.1");
    I18n::reg("sar_std_fcc47", "FCC 47 CFR", "FCC 47 CFR");
    I18n::reg("sar_std_ieee2019", "IEEE C95.1-2019", "IEEE C95.1-2019");
    I18n::reg("sar_std_icnirp", "ICNIRP", "ICNIRP");
    I18n::reg("sar_std_icnirp2020", "ICNIRP 2020", "ICNIRP 2020");
    I18n::reg("sar_std_ieee_th", "IEEE 熱指標", "IEEE thermal metric");
    I18n::reg("sar_ok", "適合", "Compliant");
    I18n::reg("sar_ng", "不適合", "Non-compliant");
    I18n::reg("sar_na", "対象外", "Not applicable");
    I18n::reg("sar_uneval", "未評価", "Not evaluated");
    // 曝露区分 (指針値は区分で変わる)
    I18n::reg("sar_category", "曝露区分", "Exposure category");
    I18n::reg("sar_cat_gp", "一般環境 (uncontrolled)",
              "General public (uncontrolled)");
    I18n::reg("sar_cat_occ", "職業 (controlled)", "Occupational (controlled)");
    I18n::reg("sar_m_temp_basis", "局所温度上昇 (健康影響の根拠値)",
              "Local temperature rise (adverse-effect basis)");
    // 点 SAR 換算 (定義式による実計算)
    I18n::reg("sar_point_section", "点 SAR 換算 / Point SAR conversion",
              "Point SAR conversion");
    I18n::reg("sar_pt_hint",
              "定義式 SAR = σ|E|²/(2ρ) (|E| は振幅) をその場で評価します。"
              "IEEE Std C95.1-2019 §3 / IEC 62704-1:2017 の定義。",
              "Evaluates the definition SAR = σ|E|²/(2ρ) (|E| = amplitude) "
              "directly. Definition per IEEE Std C95.1-2019 §3 / "
              "IEC 62704-1:2017.");
    I18n::reg("sar_pt_sigma", "導電率 σ", "Conductivity σ");
    I18n::reg("sar_pt_rho", "組織密度 ρ", "Tissue density ρ");
    I18n::reg("sar_pt_efield", "電界 |E| (実効値)", "Electric field |E| (rms)");
    I18n::reg("sar_pt_cp", "比熱 c_p", "Specific heat c_p");
    I18n::reg("sar_pt_time", "曝露時間", "Exposure time");
    I18n::reg("sar_pt_result", "算出値", "Computed");
    I18n::reg("sar_pt_caveat",
              "⚠ 点 SAR は 1 g / 10 g 空間平均 SAR とは別量で、下表の指針値との"
              "適合判定には使えません (空間平均には電界分布全体が必要)。\n"
              "温度上昇は熱伝導・血流灌流を無視した断熱上限 (ΔT = SAR·t/c_p)。",
              "⚠ Point SAR is a different quantity from the 1 g / 10 g "
              "spatial-average SAR and cannot be compared with the limits in "
              "the table below (spatial averaging needs the full field "
              "distribution).\nThe temperature rise is the adiabatic upper "
              "bound ΔT = SAR·t/c_p (conduction and perfusion neglected).");
    I18n::reg("sar_pt_bad", "⚠ 入力が不正です (σ ≥ 0, ρ > 0, |E| ≥ 0)",
              "⚠ Invalid input (σ ≥ 0, ρ > 0, |E| ≥ 0)");
    I18n::reg("sar_bioheat", "Pennes BioHeat 方程式で温度上昇を連成計算",
              "Co-simulate the temperature rise with the Pennes BioHeat equation");
    I18n::reg("sar_uncertainty", "不確かさ評価 (IEC 62704-1 準拠)",
              "Uncertainty assessment (per IEC 62704-1)");
    I18n::reg("sar_zoning", "ゾーニング/位置スキャン (最大SAR探索)",
              "Zoning / position scan (search for the maximum SAR)");

    // 出力
    I18n::reg("sar_out_section", "出力 / Output", "Output");
    I18n::reg("sar_btn_dist", "🗺 SAR分布 (断面/3D)",
              "🗺 SAR distribution (slice / 3D)");
    I18n::reg("sar_btn_anim", "🎬 H5アニメで可視化",
              "🎬 Visualize as an H5 animation");
    I18n::reg("sar_btn_report", "📄 適合宣言用レポート (IEC 62704)",
              "📄 Declaration-of-conformity report (IEC 62704)");

    // 安全規制に関わる表のため強い注記を付ける (絶対規則 5)。
    // 算出値は持たない (未計算) ので「サンプル値」ではなく状態を説明する。
    I18n::reg("sar_status_warn",
              "⚠ 算出値は未計算です — SAR 分布の計算は未実装のため、"
              "「算出値」「判定」は空欄のままです (適合宣言に使用不可)。",
              "⚠ No computed values — the SAR distribution is not implemented, "
              "so the “Computed” and “Verdict” columns stay empty (must not be "
              "used for a declaration of conformity).");
    I18n::reg("sar_limits_note",
              "▸ 「指針値」欄は規格の基本制限そのもの (周波数と曝露区分から選択)。"
              "算出値を埋めるには ofd カーネルによる SAR 分布計算と "
              "IEC 62704-1 の空間平均処理が必要です (いずれも未実装)。",
              "▸ The “Guideline” column holds the basic restrictions of the "
              "standards themselves (selected by frequency and exposure "
              "category). Filling the computed column requires an SAR "
              "distribution from the ofd kernel plus the IEC 62704-1 spatial "
              "averaging — neither is implemented yet.");
    I18n::reg("sar_col_avg", "平均化", "Averaging");
    return true;
}();

// ── 小物ヘルパー (mock の badge / muted / q-table / Seg 相当) ───────────────
const char kMuted[] = "#888888";    // badge (色指定なし)

QLabel *makeHint(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setWordWrap(true);
    return l;
}

QLabel *makeMono(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    QFont f = l->font();
    // 実在するファミリのみ指定 (Theme が環境ごとに解決済み)
    if (const QString mf = Theme::monoFontFamily(); !mf.isEmpty())
        f.setFamily(mf);
    f.setStyleHint(QFont::Monospace);
    l->setFont(f);
    return l;
}

QCheckBox *makeCheck(const QString &text, bool on, QWidget *parent)
{
    auto *c = new QCheckBox(text, parent);
    c->setChecked(on);
    return c;
}

QLineEdit *numEdit(const QString &text, QWidget *parent, int w = 80)
{
    auto *e = new QLineEdit(text, parent);
    e->setMaximumWidth(w);
    return e;
}

QHBoxLayout *unitRow(QWidget *w, const QString &unit, QWidget *parent)
{
    auto *h = new QHBoxLayout();
    h->addWidget(w);
    h->addWidget(new QLabel(unit, parent));
    h->addStretch(1);
    return h;
}

// mock の <Row> 相当: チェックボックスを横並びに
QHBoxLayout *checkRow(const QVector<QCheckBox *> &boxes)
{
    auto *h = new QHBoxLayout();
    h->setSpacing(8);
    for (auto *b : boxes)
        h->addWidget(b);
    h->addStretch(1);
    return h;
}

// <Seg> 相当: 排他 checkable QPushButton 行を 1 ウィジェットに畳む
QWidget *segRow(QWidget *parent, QButtonGroup **out, const QStringList &labels,
                int current)
{
    auto *w = new QWidget(parent);
    auto *h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(1);
    auto *grp = new QButtonGroup(w);
    grp->setExclusive(true);
    for (int i = 0; i < labels.size(); ++i) {
        auto *b = new QPushButton(labels[i], w);
        b->setCheckable(true);
        b->setStyleSheet("padding:2px 10px;");
        grp->addButton(b, i);
        h->addWidget(b);
    }
    if (auto *b = grp->button(current)) b->setChecked(true);
    h->addStretch(1);
    if (out) *out = grp;
    return w;
}

QTableWidgetItem *textItem(const QString &s) { return new QTableWidgetItem(s); }

QTableWidgetItem *numItem(const QString &s)
{
    auto *it = new QTableWidgetItem(s);
    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return it;
}

// 判定セル (mock の <span className="badge ok"> / 無色 badge)
QTableWidgetItem *badgeItem(const QString &s, const char *color)
{
    auto *it = new QTableWidgetItem(s);
    it->setForeground(QColor(color));
    QFont f = it->font();
    f.setBold(true);
    it->setFont(f);
    return it;
}

QTableWidget *makeTable(const QStringList &headers, int rows, QWidget *parent,
                        int minH)
{
    auto *t = new QTableWidget(rows, headers.size(), parent);
    t->setHorizontalHeaderLabels(headers);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    t->horizontalHeader()->setStretchLastSection(true);
    t->verticalHeader()->setVisible(false);
    t->verticalHeader()->setDefaultSectionSize(24);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setMinimumHeight(minH);
    return t;
}

// 評価指標表の行定義。
// 「指針値」は規格そのもの (src/em/SarMetrics) から周波数・曝露区分に応じて
// 引く実データ。「算出値」「判定」は SAR 分布の計算が未実装なので空欄のまま
// (固定のサンプル値を出さない — CLAUDE.md 絶対規則 5・6)。
struct MetricRow {
    const char    *nameKey;
    em::Standard   standard;
    em::Metric     metric;
    const char    *stdKey;
};
const MetricRow kMetrics[5] = {
    { "sar_m_10g",        em::Standard::Icnirp2020,
      em::Metric::LocalSar10g,          "sar_std_icnirp2020" },
    { "sar_m_1g",         em::Standard::Fcc47Cfr,
      em::Metric::LocalSar1g,           "sar_std_fcc47"      },
    { "sar_m_wb",         em::Standard::Icnirp2020,
      em::Metric::WholeBodySar,         "sar_std_icnirp2020" },
    { "sar_m_pd",         em::Standard::Icnirp2020,
      em::Metric::IncidentPowerDensity, "sar_std_icnirp2020" },
    { "sar_m_temp_basis", em::Standard::IeeeC95_1_2019,
      em::Metric::LocalTemperatureRise, "sar_std_ieee2019"   },
};

// 平均化の説明 ("10 g / 6 min")
QString averagingText(const em::ExposureLimit &l)
{
    QStringList parts;
    if (l.averagingMass_g > 0.0)
        parts << QString::number(l.averagingMass_g, 'g', 3) + " g";
    if (l.averagingTime_s > 0.0)
        parts << QString::number(l.averagingTime_s / 60.0, 'g', 3) + " min";
    return parts.isEmpty() ? QString::fromUtf8("—") : parts.join(" / ");
}
} // namespace

// ── SarTab ──────────────────────────────────────────────────────────────────
SarTab::SarTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // ── SAR / 生体電磁 解析 (概要 + 人体モデル + 条件付き行) ────────────────
    auto *sm = new SectionBox(I18n::tr("sar_title"), body);
    sm->vbox()->addWidget(makeHint(I18n::tr("sar_hint"), sm));
    sm->form()->addRow(I18n::tr("sar_model"),
                       segRow(sm, &m_model, { I18n::tr("sar_model_head"),
                                              I18n::tr("sar_model_flat"),
                                              I18n::tr("sar_model_voxel") }, 0));
    // モックの {model === …} 条件行を 1 枚のペインに畳んで出し入れする
    m_voxelPane = buildVoxelPane(sm);
    m_headPane  = buildHeadPane(sm);
    sm->form()->addRow(m_voxelPane);
    sm->form()->addRow(m_headPane);
    v->addWidget(sm);

    // ── 曝露源 / Exposure source ────────────────────────────────────────────
    auto *ss = new SectionBox(I18n::tr("sar_src_section"), body);
    m_device = new QComboBox(ss);
    m_device->addItems({ I18n::tr("sar_dev_phone"), I18n::tr("sar_dev_wpt"),
                         I18n::tr("sar_dev_bs"), I18n::tr("sar_dev_mri"),
                         I18n::tr("sar_dev_wear") });
    ss->form()->addRow(I18n::tr("sar_device"), m_device);

    m_freq      = numEdit("1950", ss);
    m_multiBand = makeCheck(I18n::tr("sar_multiband"), false, ss);
    auto *fr = new QHBoxLayout();
    fr->addWidget(m_freq);
    fr->addWidget(new QLabel("MHz", ss));
    fr->addWidget(m_multiBand);
    fr->addStretch(1);
    ss->form()->addRow(I18n::tr("sar_freq"), fr);

    m_txPower = numEdit("24", ss);
    ss->form()->addRow(I18n::tr("sar_tx_power"),
                       unitRow(m_txPower, I18n::tr("sar_tx_power_unit"), ss));
    // 曝露区分 — 指針値の選択に実際に使う (機器/送信電力はまだ未使用)
    m_category = new QComboBox(ss);
    m_category->addItems({ I18n::tr("sar_cat_gp"), I18n::tr("sar_cat_occ") });
    ss->form()->addRow(I18n::tr("sar_category"), m_category);
    // 機器・送信電力はどこにも読まれない (未実装の明示 — 絶対規則 5)
    ss->vbox()->addWidget(tabhelp::unwiredNote(ss));
    v->addWidget(ss);

    // ── 点 SAR 換算 (定義式による実計算) ────────────────────────────────────
    v->addWidget(buildPointSarSection(body));

    // ── 評価指標 / Metrics ──────────────────────────────────────────────────
    auto *sme = new SectionBox(I18n::tr("sar_metrics_section"), body);
    // 算出値は持たない (SAR 分布の計算が未実装)。固定のサンプル値を出さず、
    // 空欄 +「未評価」とし、何が足りないかを明示する (絶対規則 5・6)。
    auto *statusWarn = new QLabel(I18n::tr("sar_status_warn"), sme);
    statusWarn->setWordWrap(true);
    statusWarn->setStyleSheet("font-weight:600; color:#B91C1C;");
    sme->vbox()->addWidget(statusWarn);
    m_metrics = makeTable({ I18n::tr("sar_col_metric"), I18n::tr("sar_col_value"),
                            I18n::tr("sar_col_limit"), I18n::tr("sar_col_avg"),
                            I18n::tr("sar_col_std"),
                            I18n::tr("sar_col_verdict") }, 5, sme, 165);
    sme->vbox()->addWidget(m_metrics);
    auto *limitsNote = new QLabel(I18n::tr("sar_limits_note"), sme);
    limitsNote->setWordWrap(true);
    limitsNote->setStyleSheet("font-size:11px; color:palette(mid);");
    sme->vbox()->addWidget(limitsNote);
    fillMetricsTable();
    m_bioHeat     = makeCheck(I18n::tr("sar_bioheat"), true, sme);
    m_uncertainty = makeCheck(I18n::tr("sar_uncertainty"), false, sme);
    m_zoning      = makeCheck(I18n::tr("sar_zoning"), true, sme);
    sme->vbox()->addLayout(checkRow({ m_bioHeat }));
    sme->vbox()->addLayout(checkRow({ m_uncertainty, m_zoning }));
    // BioHeat/不確かさ/ゾーニングのチェックはどこにも読まれない (絶対規則 5)
    sme->vbox()->addWidget(tabhelp::unwiredNote(sme));
    v->addWidget(sme);

    // ── 出力 / Output ───────────────────────────────────────────────────────
    auto *so = new SectionBox(I18n::tr("sar_out_section"), body);
    auto *ob = new QHBoxLayout();
    // 出力 3 ボタンは未配線 — 押せる形で放置しない (絶対規則 5)
    auto *distBtn   = new QPushButton(I18n::tr("sar_btn_dist"), so);
    auto *animBtn   = new QPushButton(I18n::tr("sar_btn_anim"), so);
    auto *reportBtn = new QPushButton(I18n::tr("sar_btn_report"), so);
    for (auto *b : { distBtn, animBtn, reportBtn })
        tabhelp::markNotImplemented(b);
    ob->addWidget(distBtn);
    ob->addWidget(animBtn);
    ob->addWidget(reportBtn);
    ob->addStretch(1);
    so->vbox()->addLayout(ob);
    v->addWidget(so);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    connect(m_model, &QButtonGroup::idClicked, this, [this](int id) {
        // ローカル state のみ (Project に対応フィールド無し → 永続化しない)
        if (m_updating) return;
        m_modelIdx = id;
        onModelChanged();
    });
    // 周波数・曝露区分は指針値の選択に効く → 変更のたびに表を作り直す
    connect(m_freq, &QLineEdit::textChanged, this,
            [this](const QString &) { fillMetricsTable(); });
    connect(m_category, &QComboBox::currentIndexChanged, this,
            [this](int) { fillMetricsTable(); });
    connect(project, &Project::loaded, this, &SarTab::refresh);
    refresh();
}

// ── 点 SAR 換算 (SAR = σ|E|²/(2ρ)) ─────────────────────────────────────────
QWidget *SarTab::buildPointSarSection(QWidget *parent)
{
    auto *s = new SectionBox(I18n::tr("sar_point_section"), parent);
    s->vbox()->addWidget(makeHint(I18n::tr("sar_pt_hint"), s));

    // 既定値は 1950 MHz の筋肉 (IT'IS Tissue Properties Database V4.1)
    m_ptSigma = numEdit("1.45", s);
    s->form()->addRow(I18n::tr("sar_pt_sigma"), unitRow(m_ptSigma, "S/m", s));
    m_ptRho = numEdit("1090", s);
    s->form()->addRow(I18n::tr("sar_pt_rho"),
                      unitRow(m_ptRho, QString::fromUtf8("kg/m³"), s));
    m_ptField = numEdit("61.4", s);
    s->form()->addRow(I18n::tr("sar_pt_efield"),
                      unitRow(m_ptField, "V/m (rms)", s));
    m_ptCp = numEdit("3421", s);
    s->form()->addRow(I18n::tr("sar_pt_cp"),
                      unitRow(m_ptCp, QString::fromUtf8("J/(kg·K)"), s));
    m_ptTime = numEdit("360", s);
    s->form()->addRow(I18n::tr("sar_pt_time"), unitRow(m_ptTime, "s", s));

    m_ptResult = makeMono(QString::fromUtf8("—"), s);
    m_ptResult->setTextInteractionFlags(Qt::TextSelectableByMouse);
    s->form()->addRow(I18n::tr("sar_pt_result"), m_ptResult);

    auto *caveat = makeHint(I18n::tr("sar_pt_caveat"), s);
    caveat->setStyleSheet("color:#B45309;");
    s->vbox()->addWidget(caveat);

    for (QLineEdit *e : { m_ptSigma, m_ptRho, m_ptField, m_ptCp, m_ptTime })
        connect(e, &QLineEdit::textChanged, this, &SarTab::updatePointSar);
    updatePointSar();
    return s;
}

void SarTab::updatePointSar()
{
    bool ok[5] = { false, false, false, false, false };
    const double sigma = m_ptSigma->text().trimmed().toDouble(&ok[0]);
    const double rho   = m_ptRho->text().trimmed().toDouble(&ok[1]);
    const double erms  = m_ptField->text().trimmed().toDouble(&ok[2]);
    const double cp    = m_ptCp->text().trimmed().toDouble(&ok[3]);
    const double t     = m_ptTime->text().trimmed().toDouble(&ok[4]);
    for (bool b : ok) if (!b) {
        m_ptResult->setText(I18n::tr("sar_pt_bad"));
        return;
    }
    if (sigma < 0.0 || rho <= 0.0 || erms < 0.0) {
        m_ptResult->setText(I18n::tr("sar_pt_bad"));
        return;
    }
    const double sar = em::sarFromRmsField(sigma, erms, rho);
    const double pd  = em::planeWavePowerDensityFromRms(erms);
    const double dT  = em::adiabaticTemperatureRise(sar, t, cp);
    m_ptResult->setText(
        QString::fromUtf8("SAR = %1 W/kg    S = %2 W/m²    ΔT ≤ %3 K")
            .arg(QString::number(sar, 'g', 4))
            .arg(QString::number(pd, 'g', 4))
            .arg(QString::number(dT, 'g', 3)));
}

void SarTab::refresh()
{
    m_updating = true;
    if (auto *b = m_model->button(m_modelIdx)) b->setChecked(true);
    m_updating = false;
    onModelChanged();
}

void SarTab::onModelChanged()
{
    // mock: {model === "voxel" && …} / {model === "head" && …}
    // 平板ファントム (1) はどちらの追加行も出さない
    m_headPane->setVisible(m_modelIdx == 0);
    m_voxelPane->setVisible(m_modelIdx == 2);
}

// ── 数値人体モデル (voxel) の追加行 ─────────────────────────────────────────
QWidget *SarTab::buildVoxelPane(QWidget *parent)
{
    auto *pane = new QWidget(parent);
    auto *fl = new QFormLayout(pane);
    fl->setContentsMargins(0, 0, 0, 0);
    fl->setHorizontalSpacing(8);
    fl->setVerticalSpacing(4);

    m_voxelModel = new QComboBox(pane);
    m_voxelModel->addItems({ I18n::tr("sar_vx_taro"), I18n::tr("sar_vx_hanako"),
                             I18n::tr("sar_vx_duke"), I18n::tr("sar_vx_child"),
                             I18n::tr("sar_vx_icrp") });
    fl->addRow(I18n::tr("sar_voxel_model"), m_voxelModel);

    m_tissueCount = makeMono(I18n::tr("sar_tissues_val"), pane);
    m_gabriel     = makeCheck(I18n::tr("sar_gabriel"), true, pane);
    auto *th = new QHBoxLayout();
    th->addWidget(m_tissueCount);
    th->addWidget(m_gabriel);
    th->addStretch(1);
    fl->addRow(I18n::tr("sar_tissues"), th);
    return pane;
}

// ── SAM頭部ファントムの追加行 ──────────────────────────────────────────────
QWidget *SarTab::buildHeadPane(QWidget *parent)
{
    auto *pane = new QWidget(parent);
    auto *fl = new QFormLayout(pane);
    fl->setContentsMargins(0, 0, 0, 0);
    fl->setHorizontalSpacing(8);
    fl->setVerticalSpacing(4);

    fl->addRow(I18n::tr("sar_liquid"),
               segRow(pane, &m_liquid, { I18n::tr("sar_liq_head"),
                                         I18n::tr("sar_liq_body") }, 0));
    m_gap = numEdit("5", pane);
    fl->addRow(I18n::tr("sar_gap"),
               unitRow(m_gap, I18n::tr("sar_gap_unit"), pane));
    return pane;
}

// 指針値は規格 (src/em/SarMetrics) から周波数・曝露区分に応じて引く。
// 算出値は無いので空欄 + 「未評価」/「対象外」とする (絶対規則 5)。
void SarTab::fillMetricsTable()
{
    bool okF = false;
    const double fMHz = m_freq->text().trimmed().toDouble(&okF);
    const double f_Hz = (okF && fMHz > 0.0) ? fMHz * 1.0e6 : -1.0;
    const em::Category cat = (m_category->currentIndex() == 1)
                                 ? em::Category::Occupational
                                 : em::Category::GeneralPublic;
    const QString catText = I18n::tr(m_category->currentIndex() == 1
                                         ? "sar_cat_occ" : "sar_cat_gp");
    const QString dash = QString::fromUtf8("—");

    for (int r = 0; r < 5; ++r) {
        const MetricRow &row = kMetrics[r];
        const em::ExposureLimit lim =
            em::exposureLimit(row.standard, cat, row.metric,
                              f_Hz > 0.0 ? f_Hz : 0.0);

        m_metrics->setItem(r, 0, textItem(I18n::tr(row.nameKey)));
        // 算出値: SAR 分布が無いので常に未計算
        m_metrics->setItem(r, 1, numItem(dash));
        // 指針値 (規格値)
        const QString limitText =
            lim.defined ? QString::number(lim.value, 'g', 3) + " "
                              + QString::fromUtf8(lim.unit)
                        : dash;
        m_metrics->setItem(r, 2, numItem(limitText));
        m_metrics->setItem(r, 3, textItem(lim.defined ? averagingText(lim)
                                                      : dash));
        m_metrics->setItem(r, 4,
                           textItem(I18n::tr(row.stdKey) + " / " + catText));
        // 判定: 未計算なので「未評価」。周波数が適用範囲外なら「対象外」
        const em::Verdict v = em::evaluate(lim, 0.0, /*hasValue=*/false);
        const bool na = (v == em::Verdict::NotApplicable);
        m_metrics->setItem(r, 5,
                           badgeItem(I18n::tr(na ? "sar_na" : "sar_uneval"),
                                     kMuted));

        // 出典条項と適用周波数範囲をツールチップに (表示欄は狭いため)
        if (lim.defined) {
            const QString tip =
                QString::fromUtf8("%1\n%2 – %3 MHz")
                    .arg(QString::fromUtf8(lim.reference))
                    .arg(QString::number(lim.fmin_Hz / 1.0e6, 'g', 4))
                    .arg(QString::number(lim.fmax_Hz / 1.0e6, 'g', 4));
            for (int c = 0; c < 6; ++c)
                if (auto *it = m_metrics->item(r, c)) it->setToolTip(tip);
        }
    }
}
