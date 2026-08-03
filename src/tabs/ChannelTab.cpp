// ChannelTab.cpp
#include "ChannelTab.h"
#include "TabHelpers.h"
#include "../core/Project.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"

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

    // チャネル特性
    I18n::reg("chn_metrics_section", "チャネル特性 / Channel metrics",
              "Channel metrics");
    I18n::reg("chn_col_metric", "指標", "Metric");
    I18n::reg("chn_col_value", "値", "Value");
    I18n::reg("chn_col_note", "備考", "Notes");
    I18n::reg("chn_m_rx", "受信電力 (中央値)", "Received power (median)");
    I18n::reg("chn_m_rx_note", "カバレッジ 96.2% (>-90dBm)",
              "Coverage 96.2% (>-90 dBm)");
    I18n::reg("chn_m_ds", "遅延スプレッド (RMS)", "Delay spread (RMS)");
    I18n::reg("chn_m_ds_note", "OFDM シンボル設計に使用",
              "Used for OFDM symbol design");
    I18n::reg("chn_m_as", "角度スプレッド (方位)", "Angular spread (azimuth)");
    I18n::reg("chn_m_as_note", "MIMO ランク推定", "MIMO rank estimation");
    I18n::reg("chn_m_k", "K-factor (ライシアン)", "K-factor (Rician)");
    I18n::reg("chn_m_k_note", "見通し成分の強さ",
              "Strength of the line-of-sight component");
    I18n::reg("chn_m_n", "経路損失指数 n", "Path-loss exponent n");
    I18n::reg("chn_m_n_note", "自由空間 n=2", "Free space n = 2");
    I18n::reg("chn_m_cap", "チャネル容量", "Channel capacity");
    I18n::reg("chn_m_cap_note", "100MHz帯域, 4×4 MIMO",
              "100 MHz bandwidth, 4×4 MIMO");
    I18n::reg("chn_btn_heat", "🗺 カバレッジヒートマップ", "🗺 Coverage heat map");
    I18n::reg("chn_btn_pdp", "📊 電力遅延プロファイル",
              "📊 Power-delay profile");
    I18n::reg("chn_btn_h5", "💾 チャネル係数 (.h5) 書出",
              "💾 Export channel coefficients (.h5)");
    I18n::reg("chn_metrics_hint",
              "▸ 3GPP TR 38.901 形式のチャネルモデル係数の書出は未実装です。",
              "▸ Export as 3GPP TR 38.901 channel-model coefficients is not "
              "implemented yet.");
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

// チャネル特性表 (モックの <tbody> をそのまま。&gt; は > に戻す)
struct ChRow { const char *nameKey, *value, *noteKey; };
const ChRow kMetrics[6] = {
    { "chn_m_rx",  "-68.4 dBm",  "chn_m_rx_note"  },
    { "chn_m_ds",  "42 ns",      "chn_m_ds_note"  },
    { "chn_m_as",  "28°",        "chn_m_as_note"  },
    { "chn_m_k",   "6.8 dB",     "chn_m_k_note"   },
    { "chn_m_n",   "2.9",        "chn_m_n_note"   },
    { "chn_m_cap", "412 Mbps",   "chn_m_cap_note" },
};
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

    // ── チャネル特性 / Channel metrics — モック由来の固定サンプル値 ─────────
    auto *sm = new SectionBox(I18n::tr("chn_metrics_section"), body);
    sm->vbox()->addWidget(tabhelp::sampleNote(sm));
    m_metrics = makeTable({ I18n::tr("chn_col_metric"), I18n::tr("chn_col_value"),
                            I18n::tr("chn_col_note") }, 6, sm, 190);
    sm->vbox()->addWidget(m_metrics);
    fillMetricsTable();

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
    connect(project, &Project::loaded, this, &ChannelTab::refresh);
    refresh();
}

void ChannelTab::refresh()
{
    m_updating = true;
    if (auto *b = m_env->button(m_envIdx)) b->setChecked(true);
    m_updating = false;
    onEnvChanged();
}

// mock: defaultValue={env==="indoor" ? "office_floor3.ifc" : "city_shibuya.osm"}
void ChannelTab::onEnvChanged()
{
    m_envFile->setText(QString::fromUtf8(m_envIdx == 0 ? kEnvFileIndoor
                                                       : kEnvFileOther));
}

void ChannelTab::fillMetricsTable()
{
    for (int r = 0; r < 6; ++r) {
        const ChRow &row = kMetrics[r];
        m_metrics->setItem(r, 0, textItem(I18n::tr(row.nameKey)));
        m_metrics->setItem(r, 1, numItem(QString::fromUtf8(row.value)));
        m_metrics->setItem(r, 2, textItem(I18n::tr(row.noteKey)));
    }
}
