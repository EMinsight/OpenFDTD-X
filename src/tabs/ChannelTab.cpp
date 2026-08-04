// ChannelTab.cpp
#include "ChannelTab.h"
#include "TabHelpers.h"
#include "../core/Project.h"
#include "../em/RadioPropagation.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"

#include <cmath>

#include <QButtonGroup>
#include <QCheckBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <QVBoxLayout>

using namespace ofd;

// ── タブ固有語彙 (chn_) — file-local 登録 ───────────────────────────────────
namespace {
const bool s_i18n = [] {
    using ofd::I18n;
    // タブ名 (MainWindow 配線用)
    I18n::reg("t_channel", "📡 電波伝搬", "📡 Propagation");

    // 概要
    I18n::reg("chn_title", "電波伝搬・チャネル解析 / Propagation & channel",
              "Propagation & channel");
    I18n::reg("chn_hint",
              "屋内・市街地の電波カバレッジとチャネル特性。FDTDは近傍・小規模、"
              "レイトレースは広域を担当。",
              "Indoor and urban radio coverage plus channel characteristics. FDTD "
              "handles the near field and small scales, ray tracing the wide area.");
    I18n::reg("chn_env_indoor", "屋内 (オフィス/工場)", "Indoor (office / factory)");
    I18n::reg("chn_env_urban", "市街地", "Urban");
    I18n::reg("chn_env_vehicle", "車内・車車間", "In-vehicle / V2V");
    I18n::reg("chn_env_tunnel", "トンネル/地下", "Tunnel / underground");
    I18n::reg("chn_band", "周波数帯", "Frequency band");
    I18n::reg("chn_band_sub1", "< 1GHz", "< 1 GHz");
    I18n::reg("chn_band_sub6", "Sub-6 (3.5G)", "Sub-6 (3.5 G)");
    I18n::reg("chn_band_mmw", "ミリ波 (28/39G)", "mmWave (28 / 39 G)");
    I18n::reg("chn_band_thz", "サブTHz (100G+, 6G)", "Sub-THz (100 G+, 6G)");
    I18n::reg("chn_method", "解析手法", "Method");
    I18n::reg("chn_method_rt", "レイトレース (広域)", "Ray tracing (wide area)");
    I18n::reg("chn_method_fdtd", "FDTD (詳細・回折)",
              "FDTD (detailed, diffraction)");
    I18n::reg("chn_method_hybrid", "ハイブリッド", "Hybrid");

    // 環境モデル
    I18n::reg("chn_envm_section", "環境モデル / Environment", "Environment");
    I18n::reg("chn_layout", "間取り/地形", "Floor plan / terrain");
    I18n::reg("chn_browse", "📁 参照…", "📁 Browse…");
    I18n::reg("chn_envm_hint",
              "▸ 対応: STL のみ (IFC/BIM・OpenStreetMap・DXF は未実装)",
              "▸ Supported: STL only (IFC/BIM, OpenStreetMap and DXF are not "
              "implemented yet)");
    I18n::reg("chn_stl_filter", "STL (*.stl);;すべてのファイル (*)",
              "STL (*.stl);;All files (*)");
    I18n::reg("chn_material", "材料", "Materials");
    I18n::reg("chn_mat_db", "コンクリート/石膏ボード/ガラスの透過損失DB",
              "Transmission-loss database for concrete / plasterboard / glass");
    I18n::reg("chn_mat_scatter", "家具・人体の散乱",
              "Scattering from furniture and people");

    // 送受信
    I18n::reg("chn_txrx_section", "送受信 / TX-RX", "TX-RX");
    I18n::reg("chn_ap", "基地局/AP", "Base stations / APs");
    I18n::reg("chn_ap_unit", "台 · ", "units · ");
    I18n::reg("chn_mimo", "MIMO 4×4", "MIMO 4×4");
    I18n::reg("chn_beamforming", "ビームフォーミング", "Beamforming");
    I18n::reg("chn_rx", "受信点", "Receive points");
    I18n::reg("chn_rx_grid", "格子 (カバレッジマップ)", "Grid (coverage map)");
    I18n::reg("chn_rx_route", "経路 (移動体)", "Route (mobile)");
    I18n::reg("chn_rx_points", "指定点", "Specified points");

    // ── リンク条件 (計算入力) ──
    I18n::reg("chn_link_section", "リンク条件 / Link budget inputs",
              "Link budget inputs");
    I18n::reg("chn_link_hint",
              "▸ 下のチャネル特性は、この条件を見通し内 (LOS) の伝搬モデルに"
              "入れて実計算します。周波数は選択した周波数帯の代表値を入れて"
              "ありますが、直接編集できます。",
              "▸ The channel metrics below are computed from these inputs with "
              "line-of-sight propagation models. The frequency is prefilled "
              "with a representative value for the selected band and can be "
              "edited directly.");
    I18n::reg("chn_freq", "中心周波数", "Centre frequency");
    I18n::reg("chn_dist", "送受信距離 d", "TX-RX distance d");
    I18n::reg("chn_htx", "送信アンテナ高", "TX antenna height");
    I18n::reg("chn_hrx", "受信アンテナ高", "RX antenna height");
    I18n::reg("chn_eirp", "送信 EIRP", "TX EIRP");
    I18n::reg("chn_grx", "受信アンテナ利得", "RX antenna gain");
    I18n::reg("chn_bw", "帯域幅", "Bandwidth");
    I18n::reg("chn_nf", "受信機雑音指数 NF", "RX noise figure NF");
    I18n::reg("chn_refl", "大地反射係数 |Γ|", "Ground reflection |Γ|");
    I18n::reg("chn_bad_input",
              "⚠ 入力に数値でない値、または範囲外の値があります — 計算できません",
              "⚠ Some inputs are not numbers or are out of range — cannot "
              "compute");

    // チャネル特性
    I18n::reg("chn_metrics_section", "チャネル特性 / Channel metrics",
              "Channel metrics");
    I18n::reg("chn_col_metric", "指標", "Metric");
    I18n::reg("chn_col_value", "値", "Value");
    I18n::reg("chn_col_note", "備考", "Notes");
    I18n::reg("chn_notcalc", "未計算", "not computed");
    I18n::reg("chn_m_fspl", "自由空間損失 (Friis)", "Free-space loss (Friis)");
    I18n::reg("chn_m_fspl_note", "L = 20log10(4πd/λ), λ = %1 · ITU-R P.525",
              "L = 20log10(4πd/λ), λ = %1 · ITU-R P.525");
    I18n::reg("chn_m_2ray", "経路損失 (2波モデル)",
              "Path loss (two-ray model)");
    I18n::reg("chn_m_2ray_note",
              "直接波 + 大地反射 (Γ = −%1) の干渉。建物透過・散乱は含まない",
              "interference of the direct and ground-reflected rays "
              "(Γ = −%1); no building penetration or scattering");
    I18n::reg("chn_m_bp", "ブレークポイント距離", "Breakpoint distance");
    I18n::reg("chn_m_bp_note", "d_bp = 4·h_t·h_r/λ — これより遠方は n ≈ 4",
              "d_bp = 4·h_t·h_r/λ — beyond this the exponent tends to 4");
    I18n::reg("chn_m_rx", "受信電力", "Received power");
    I18n::reg("chn_m_rx_note", "EIRP − 経路損失(2波) + 受信利得",
              "EIRP − two-ray path loss + RX gain");
    I18n::reg("chn_m_ds", "遅延スプレッド (RMS)", "Delay spread (RMS)");
    I18n::reg("chn_m_ds_note",
              "多重波の分布が要るためレイトレース / FDTD の実行が必要 "
              "(2波だけの遅延差は上の「遅延差 τ (2波)」の行)",
              "needs the multipath distribution, i.e. a ray-tracing or FDTD "
              "run (the two-ray-only delay difference is in the “excess delay "
              "τ” row above)");
    I18n::reg("chn_m_tau", "遅延差 τ (2波)", "Excess delay τ (two-ray)");
    I18n::reg("chn_m_tau_note", "τ = (d_ref − d_los)/c",
              "τ = (d_ref − d_los)/c");
    I18n::reg("chn_m_as", "角度スプレッド (方位)", "Angular spread (azimuth)");
    I18n::reg("chn_m_as_note",
              "到来角分布が要るためレイトレース / FDTD の実行が必要",
              "needs the angle-of-arrival distribution, i.e. a ray-tracing or "
              "FDTD run");
    I18n::reg("chn_m_k", "K-factor (2波モデル)", "K-factor (two-ray)");
    I18n::reg("chn_m_k_note", "直接波/反射波の電力比 = (d_ref/(d_los·|Γ|))²",
              "direct-to-reflected power ratio = (d_ref/(d_los·|Γ|))²");
    I18n::reg("chn_m_n", "経路損失指数 n", "Path-loss exponent n");
    I18n::reg("chn_m_n_note", "2波モデルの d〜2d 局所勾配 (自由空間 n = 2)",
              "local slope of the two-ray model between d and 2d "
              "(free space n = 2)");
    I18n::reg("chn_m_noise", "雑音電力 / SNR", "Noise power / SNR");
    I18n::reg("chn_m_noise_note", "N = kT0B + NF (T0 = 290 K), SNR = Prx − N",
              "N = kT0B + NF (T0 = 290 K), SNR = Prx − N");
    I18n::reg("chn_m_cap", "チャネル容量 (SISO Shannon)",
              "Channel capacity (SISO Shannon)");
    I18n::reg("chn_m_cap_note",
              "C = B·log2(1+SNR)。MIMO 4×4 の多重利得はチャネル行列が要るため"
              "未計算",
              "C = B·log2(1+SNR). The 4×4 MIMO multiplexing gain needs the "
              "channel matrix and is not computed");
    I18n::reg("chn_btn_heat", "🗺 カバレッジヒートマップ", "🗺 Coverage heat map");
    I18n::reg("chn_btn_pdp", "📊 電力遅延プロファイル",
              "📊 Power-delay profile");
    I18n::reg("chn_btn_h5", "💾 チャネル係数 (.h5) 書出",
              "💾 Export channel coefficients (.h5)");
    I18n::reg("chn_metrics_hint",
              "▸ 3GPP TR 38.901 形式のチャネルモデル係数の書出は未実装です。",
              "▸ Export as 3GPP TR 38.901 channel-model coefficients is not "
              "implemented yet.");
    I18n::reg("chn_model_note",
              "▸ 値は「平面大地・完全反射の 2 波モデル (見通し内)」と Friis の"
              "自由空間損失による実計算です (式と出典は src/em/RadioPropagation.h)。"
              "建物・壁の透過損失、家具や人体の散乱、多重反射は含みません — "
              "それらを含む指標 (RMS 遅延スプレッド・角度スプレッド) は"
              "レイトレース / FDTD の実行が必要で「未計算」と表示します。"
              "上の環境モデル・解析手法の選択は計算に反映されません。",
              "▸ The values are computed with the flat-earth perfectly-"
              "reflecting two-ray model (line of sight) and the Friis "
              "free-space loss (formulas and sources in "
              "src/em/RadioPropagation.h). Building or wall penetration, "
              "scattering from furniture and people and higher-order "
              "reflections are not included — metrics that need them (RMS "
              "delay spread, angular spread) require a ray-tracing or FDTD run "
              "and are shown as “not computed”. The environment and method "
              "selections above do not enter the calculation.");
    return true;
}();

// ── 環境モデルごとの既定ファイル名 (モックの三項演算子をそのまま転記) ───────
const char kEnvFileIndoor[] = "office_floor3.ifc";
const char kEnvFileOther[]  = "city_shibuya.osm";

// ── 小物ヘルパー (mock の muted / q-table / Seg 相当) ───────────────────────
QLabel *makeHint(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setWordWrap(true);
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

// mock の <Row label> 単発版。SectionBox::form() は 1 枚しか持てないため、
// hint を間に挟むセクションではこれで 1 行ずつ vbox に積む。
QFormLayout *formRow(const QString &label, QLayout *field)
{
    auto *f = new QFormLayout();
    f->setContentsMargins(0, 0, 0, 0);
    f->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    f->setLabelAlignment(Qt::AlignLeft);
    f->setHorizontalSpacing(8);
    f->setVerticalSpacing(4);
    f->addRow(label, field);
    return f;
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

// チャネル特性表の行 (値は fillMetricsTable が実計算で埋める)。
// note が計算値を含む行は書式引数を持つので、行ごとに fillMetricsTable で作る。
enum MetricRow {
    RowFspl = 0, RowTwoRay, RowBreak, RowRx, RowN, RowK, RowTau,
    RowDelaySpread, RowAngleSpread, RowNoise, RowCapacity, RowCount
};
const char *kMetricNameKey[RowCount] = {
    "chn_m_fspl", "chn_m_2ray", "chn_m_bp", "chn_m_rx", "chn_m_n", "chn_m_k",
    "chn_m_tau", "chn_m_ds", "chn_m_as", "chn_m_noise", "chn_m_cap",
};

// 選択した周波数帯の代表中心周波数 [GHz] (モックの帯域区分に対応)。
// 帯域を切り替えたときの既定値であって、利用者が直接編集できる。
double bandCenterGHz(int band)
{
    switch (band) {
    case 0:  return 0.9;      // < 1 GHz
    case 1:  return 3.5;      // Sub-6
    case 2:  return 28.0;     // ミリ波
    default: return 140.0;    // サブ THz
    }
}

// 距離の書式 (m / km)
QString fmtDistance(double m)
{
    if (!(m > 0.0) || !std::isfinite(m)) return QStringLiteral("—");
    if (m >= 1000.0) return QStringLiteral("%1 km").arg(m / 1000.0, 0, 'f', 2);
    return QStringLiteral("%1 m").arg(m, 0, 'f', 1);
}

// 波長の書式 (mm / m)
QString fmtWavelength(double m)
{
    if (!(m > 0.0) || !std::isfinite(m)) return QStringLiteral("—");
    if (m < 1.0) return QStringLiteral("%1 mm").arg(m * 1000.0, 0, 'f', 2);
    return QStringLiteral("%1 m").arg(m, 0, 'f', 3);
}
} // namespace

// ── ChannelTab ──────────────────────────────────────────────────────────────
ChannelTab::ChannelTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // ── 電波伝搬・チャネル解析 (概要 + 環境 + 周波数帯 + 手法) ──────────────
    auto *st = new SectionBox(I18n::tr("chn_title"), body);
    st->vbox()->addWidget(makeHint(I18n::tr("chn_hint"), st));
    st->vbox()->addWidget(segRow(st, &m_env, { I18n::tr("chn_env_indoor"),
                                               I18n::tr("chn_env_urban"),
                                               I18n::tr("chn_env_vehicle"),
                                               I18n::tr("chn_env_tunnel") }, 0));
    st->form()->addRow(I18n::tr("chn_band"),
                       segRow(st, &m_band, { I18n::tr("chn_band_sub1"),
                                             I18n::tr("chn_band_sub6"),
                                             I18n::tr("chn_band_mmw"),
                                             I18n::tr("chn_band_thz") }, 1));
    st->form()->addRow(I18n::tr("chn_method"),
                       segRow(st, &m_method, { I18n::tr("chn_method_rt"),
                                               I18n::tr("chn_method_fdtd"),
                                               I18n::tr("chn_method_hybrid") }, 2));
    v->addWidget(st);

    // ── 環境モデル / Environment ────────────────────────────────────────────
    // モックは Row(間取り) → hint → Row(材料) の順なので、
    // SectionBox::form() を使わずに QFormLayout を 2 枚に分けて順序を保つ。
    auto *se = new SectionBox(I18n::tr("chn_envm_section"), body);
    m_envFile = new QLineEdit(QString::fromUtf8(kEnvFileIndoor), se);
    auto *fr = new QHBoxLayout();
    fr->addWidget(m_envFile, 1);
    // 「📁 参照…」のみ実配線 (選択パスを欄へ反映する。読込・解析は未実装)
    auto *browseBtn = new QPushButton(I18n::tr("chn_browse"), se);
    connect(browseBtn, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, I18n::tr("chn_layout"), m_envFile->text(),
            I18n::tr("chn_stl_filter"));
        if (!path.isEmpty()) m_envFile->setText(path);
    });
    fr->addWidget(browseBtn);
    se->vbox()->addLayout(formRow(I18n::tr("chn_layout"), fr));

    se->vbox()->addWidget(makeHint(I18n::tr("chn_envm_hint"), se));

    m_matDb      = makeCheck(I18n::tr("chn_mat_db"), true, se);
    m_matScatter = makeCheck(I18n::tr("chn_mat_scatter"), false, se);
    auto *mr = new QHBoxLayout();
    mr->addWidget(m_matDb);
    mr->addWidget(m_matScatter);
    mr->addStretch(1);
    se->vbox()->addLayout(formRow(I18n::tr("chn_material"), mr));
    // 環境モデルのフォームはどこにも読まれていない (未実装)
    se->vbox()->addWidget(tabhelp::unwiredNote(se));
    v->addWidget(se);

    // ── 送受信 / TX-RX ──────────────────────────────────────────────────────
    auto *sx = new SectionBox(I18n::tr("chn_txrx_section"), body);
    m_apCount     = numEdit("4", sx);
    m_mimo        = makeCheck(I18n::tr("chn_mimo"), true, sx);
    m_beamforming = makeCheck(I18n::tr("chn_beamforming"), true, sx);
    auto *ar = new QHBoxLayout();
    ar->addWidget(m_apCount);
    ar->addWidget(new QLabel(I18n::tr("chn_ap_unit"), sx));
    ar->addWidget(m_mimo);
    ar->addWidget(m_beamforming);
    ar->addStretch(1);
    sx->form()->addRow(I18n::tr("chn_ap"), ar);

    sx->form()->addRow(I18n::tr("chn_rx"),
                       segRow(sx, &m_rxKind, { I18n::tr("chn_rx_grid"),
                                               I18n::tr("chn_rx_route"),
                                               I18n::tr("chn_rx_points") }, 0));
    // 送受信フォームはどこにも読まれていない (未実装)
    sx->vbox()->addWidget(tabhelp::unwiredNote(sx));
    v->addWidget(sx);

    // ── リンク条件 / Link budget inputs (チャネル特性の計算入力) ────────────
    auto *sl = new SectionBox(I18n::tr("chn_link_section"), body);
    sl->vbox()->addWidget(makeHint(I18n::tr("chn_link_hint"), sl));
    m_freqGHz = numEdit(QString::number(bandCenterGHz(1)), sl);
    m_dist    = numEdit(QStringLiteral("100"), sl);
    m_hTx     = numEdit(QStringLiteral("10"), sl);
    m_hRx     = numEdit(QStringLiteral("1.5"), sl);
    m_eirp    = numEdit(QStringLiteral("30"), sl);
    m_gRx     = numEdit(QStringLiteral("0"), sl);
    m_bw      = numEdit(QStringLiteral("100"), sl);
    m_nf      = numEdit(QStringLiteral("7"), sl);
    m_refl    = numEdit(QStringLiteral("1.0"), sl);
    const struct { const char *key; QLineEdit *edit; const char *unit; }
    kLinkRows[] = {
        { "chn_freq", m_freqGHz, "GHz"  },
        { "chn_dist", m_dist,    "m"    },
        { "chn_htx",  m_hTx,     "m"    },
        { "chn_hrx",  m_hRx,     "m"    },
        { "chn_eirp", m_eirp,    "dBm"  },
        { "chn_grx",  m_gRx,     "dBi"  },
        { "chn_bw",   m_bw,      "MHz"  },
        { "chn_nf",   m_nf,      "dB"   },
        { "chn_refl", m_refl,    ""     },
    };
    for (const auto &r : kLinkRows) {
        auto *row = new QHBoxLayout();
        row->addWidget(r.edit);
        if (*r.unit) row->addWidget(new QLabel(QString::fromUtf8(r.unit), sl));
        row->addStretch(1);
        sl->form()->addRow(I18n::tr(r.key), row);
        connect(r.edit, &QLineEdit::textChanged,
                this, &ChannelTab::recompute);
    }
    m_inputError = new QLabel(sl);
    m_inputError->setWordWrap(true);
    m_inputError->setStyleSheet("color:#C0392B; font-size:11px;");
    m_inputError->setVisible(false);
    sl->vbox()->addWidget(m_inputError);
    v->addWidget(sl);

    // ── チャネル特性 / Channel metrics — 上のリンク条件からの実計算 ─────────
    auto *sm = new SectionBox(I18n::tr("chn_metrics_section"), body);
    m_metrics = makeTable({ I18n::tr("chn_col_metric"), I18n::tr("chn_col_value"),
                            I18n::tr("chn_col_note") }, RowCount, sm,
                          26 * RowCount + 30);   // 全行を折り返さず見せる
    sm->vbox()->addWidget(m_metrics);
    sm->vbox()->addWidget(makeHint(I18n::tr("chn_model_note"), sm));

    // ヒートマップ / PDP / 書出のボタンはいずれも未配線 (絶対規則 5)
    auto *bb = new QHBoxLayout();
    auto *heatBtn = new QPushButton(I18n::tr("chn_btn_heat"), sm);
    auto *pdpBtn  = new QPushButton(I18n::tr("chn_btn_pdp"), sm);
    auto *h5Btn   = new QPushButton(I18n::tr("chn_btn_h5"), sm);
    for (QPushButton *b : { heatBtn, pdpBtn, h5Btn }) {
        tabhelp::markNotImplemented(b);
        bb->addWidget(b);
    }
    bb->addStretch(1);
    sm->vbox()->addLayout(bb);
    sm->vbox()->addWidget(makeHint(I18n::tr("chn_metrics_hint"), sm));
    v->addWidget(sm);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    connect(m_env, &QButtonGroup::idClicked, this, [this](int id) {
        // ローカル state のみ (Project に対応フィールド無し → 永続化しない)
        if (m_updating) return;
        m_envIdx = id;
        onEnvChanged();
    });
    // 周波数帯を切り替えたら中心周波数の既定値を入れ直す (編集は自由)
    connect(m_band, &QButtonGroup::idClicked, this, [this](int id) {
        if (m_updating) return;
        m_freqGHz->setText(QString::number(bandCenterGHz(id)));
    });
    connect(project, &Project::loaded, this, &ChannelTab::refresh);
    refresh();
}

void ChannelTab::refresh()
{
    m_updating = true;
    if (auto *b = m_env->button(m_envIdx)) b->setChecked(true);
    m_updating = false;
    onEnvChanged();
    recompute();
}

// mock: defaultValue={env==="indoor" ? "office_floor3.ifc" : "city_shibuya.osm"}
void ChannelTab::onEnvChanged()
{
    m_envFile->setText(QString::fromUtf8(m_envIdx == 0 ? kEnvFileIndoor
                                                       : kEnvFileOther));
}

// リンク条件 → チャネル特性 (見通し内の伝搬モデルによる実計算)。
// 計算できない指標 (多重波が要るもの・入力不正) は「未計算」+ 理由を出す。
void ChannelTab::recompute()
{
    namespace prop = ofd::em::propagation;
    const QString nc = I18n::tr("chn_notcalc");

    auto value = [](QLineEdit *e, bool &ok) {
        const double v = e->text().trimmed().toDouble(&ok);
        if (!std::isfinite(v)) ok = false;
        return v;
    };
    bool ok = true, all = true;
    const double fGHz = value(m_freqGHz, ok); all = all && ok;
    const double d    = value(m_dist, ok);    all = all && ok;
    const double ht   = value(m_hTx, ok);     all = all && ok;
    const double hr   = value(m_hRx, ok);     all = all && ok;
    const double eirp = value(m_eirp, ok);    all = all && ok;
    const double grx  = value(m_gRx, ok);     all = all && ok;
    const double bwM  = value(m_bw, ok);      all = all && ok;
    const double nf   = value(m_nf, ok);      all = all && ok;
    const double refl = value(m_refl, ok);    all = all && ok;
    // 物理的に意味のある範囲か (負の距離・負の高さ・|Γ| > 1 は受け付けない)
    const bool valid = all && fGHz > 0.0 && d > 0.0 && ht >= 0.0 && hr >= 0.0
                       && bwM > 0.0 && refl >= 0.0 && refl <= 1.0;
    m_inputError->setText(I18n::tr("chn_bad_input"));
    m_inputError->setVisible(!valid);

    // 行の見出しは常に埋める (値だけ差し替える)
    for (int r = 0; r < RowCount; ++r)
        m_metrics->setItem(r, 0, textItem(I18n::tr(kMetricNameKey[r])));

    auto setRow = [this](int r, const QString &val, const QString &note) {
        m_metrics->setItem(r, 1, numItem(val));
        m_metrics->setItem(r, 2, textItem(note));
    };

    if (!valid) {
        for (int r = 0; r < RowCount; ++r)
            setRow(r, nc, I18n::tr("chn_bad_input"));
        return;
    }

    const double f = fGHz * 1e9;
    const double lam = prop::wavelength(f);
    const double fspl = prop::freeSpacePathLossDb(d, f);
    const double l2ray = prop::twoRayPathLossDb(d, ht, hr, f, refl);
    const double dbp = prop::breakpointDistance(ht, hr, f);
    const double prx = prop::receivedPowerDbm(eirp, l2ray, grx);
    const double n = prop::pathLossExponent(
        l2ray, d, prop::twoRayPathLossDb(2.0 * d, ht, hr, f, refl), 2.0 * d);
    const double kdb = prop::twoRayKFactorDb(d, ht, hr, refl);
    const double tau = prop::twoRayExcessDelay(d, ht, hr);
    const double bw = bwM * 1e6;
    const double noise = prop::thermalNoiseDbm(bw, nf);
    const double snr = prx - noise;
    const double cap = prop::shannonCapacity(bw, snr);

    setRow(RowFspl, QStringLiteral("%1 dB").arg(fspl, 0, 'f', 2),
           I18n::tr("chn_m_fspl_note").arg(fmtWavelength(lam)));
    setRow(RowTwoRay, QStringLiteral("%1 dB").arg(l2ray, 0, 'f', 2),
           I18n::tr("chn_m_2ray_note").arg(refl, 0, 'f', 2));
    setRow(RowBreak, fmtDistance(dbp), I18n::tr("chn_m_bp_note"));
    setRow(RowRx, QStringLiteral("%1 dBm").arg(prx, 0, 'f', 2),
           I18n::tr("chn_m_rx_note"));
    setRow(RowN, QString::number(n, 'f', 2), I18n::tr("chn_m_n_note"));
    setRow(RowK, QStringLiteral("%1 dB").arg(kdb, 0, 'f', 2),
           I18n::tr("chn_m_k_note"));
    setRow(RowTau, QStringLiteral("%1 ns").arg(tau * 1e9, 0, 'f', 2),
           I18n::tr("chn_m_tau_note"));
    // 多重波の統計が要る指標は実行しないと出せない (偽の値を出さない)
    setRow(RowDelaySpread, nc, I18n::tr("chn_m_ds_note"));
    setRow(RowAngleSpread, nc, I18n::tr("chn_m_as_note"));
    setRow(RowNoise, QStringLiteral("%1 dBm / SNR %2 dB")
                         .arg(noise, 0, 'f', 2).arg(snr, 0, 'f', 2),
           I18n::tr("chn_m_noise_note"));
    setRow(RowCapacity, QStringLiteral("%1 Mbps").arg(cap / 1e6, 0, 'f', 1),
           I18n::tr("chn_m_cap_note"));
}
