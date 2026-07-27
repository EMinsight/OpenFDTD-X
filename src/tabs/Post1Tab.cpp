// Post1Tab.cpp
#include "Post1Tab.h"
#include "../core/Project.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>

using namespace ofd;

// ── タブ固有の翻訳キー (p1x_) — file-local 登録 (既存 p1_ は I18n.cpp) ────────
namespace {
const bool s_i18n = [] {
    using ofd::I18n;
    // 時間特性(2D) セクション (mock: pp_time_2d) と、その 3 つのチェック
    // (pp_conv / pp_feed_wave / pp_obs_wave)。.ofd 側は
    // plotiter / plotfeed / plotpoint で、キー名はツールチップに出す。
    I18n::reg("p1x_time_2d", "時間特性(2D)", "Time domain (2D)");
    I18n::reg("p1x_conv", "収束状況", "Convergence");
    I18n::reg("p1x_feed_wave", "給電点波形・スペクトル",
              "Feed waveform & spectrum");
    I18n::reg("p1x_obs_wave", "観測点波形・スペクトル",
              "Probe waveform & spectrum");
    // 周波数特性(2D) の自動スケール (mock: pp_auto_scale)
    I18n::reg("p1x_auto_scale", "自動スケール", "Auto-scale");
    I18n::reg("p1x_auto_hint", "→ OFF時に最小/最大/分割数を指定",
              "→ set min / max / div when off");
    return true;
}();
} // namespace

Post1Tab::Post1Tab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // ── 時間特性(2D) (mock: <Section title={t("pp_time_2d")}>) ───────────────
    // 収束状況 = plotiter (反復回数に対する残差)、給電点/観測点は波形+スペクトル。
    // mock はこのセクションを時間領域の 3 つに限っているので、スミスチャート・
    // 整合損・周波数目盛分割は下の周波数特性セクションへ置く。
    auto *sw = new SectionBox(I18n::tr("p1x_time_2d"), body);
    m_iter  = new QCheckBox(I18n::tr("p1x_conv"), sw);
    m_feed  = new QCheckBox(I18n::tr("p1x_feed_wave"), sw);
    m_point = new QCheckBox(I18n::tr("p1x_obs_wave"), sw);
    m_iter->setToolTip("plotiter");
    m_feed->setToolTip("plotfeed");
    m_point->setToolTip("plotpoint");
    for (auto *c : { m_iter, m_feed, m_point })
        sw->vbox()->addWidget(c);
    v->addWidget(sw);

    // ── 周波数特性(2D) — mock の並び順:
    //    スミスチャート → 入力インピーダンス…結合係数 → 自動スケール → 分割数
    auto *sf = new SectionBox(I18n::tr("p1_freq_section"), body);
    m_smith = new QCheckBox(I18n::tr("p1_smith"), sf);
    sf->vbox()->addWidget(m_smith);

    PostOpts &po = m_p->post();
    addFreqRow(body, sf, I18n::tr("p1_zin"),      &po.zin);
    addFreqRow(body, sf, I18n::tr("p1_yin"),      &po.yin);
    addFreqRow(body, sf, I18n::tr("p1_ref"),      &po.ref);
    addFreqRow(body, sf, I18n::tr("p1_spara"),    &po.spara);
    addFreqRow(body, sf, I18n::tr("p1_coupling"), &po.coupling);

    m_matching = new QCheckBox(I18n::tr("p1_matching"), sf);
    sf->vbox()->addWidget(m_matching);

    // 自動スケール (mock: <Check pp_auto_scale checked> + muted ヒント)
    auto *asrow = new QHBoxLayout();
    m_autoScale = new QCheckBox(I18n::tr("p1x_auto_scale"), sf);
    auto *ashint = new QLabel(I18n::tr("p1x_auto_hint"), sf);
    ashint->setWordWrap(true);
    ashint->setStyleSheet("color:#888888; font-size:11px;");  // mock: muted text-sm
    asrow->addWidget(m_autoScale);
    asrow->addWidget(ashint, 1);
    sf->vbox()->addLayout(asrow);

    // 周波数目盛分割 (mock: <Row label={t("pp_freq_div")}>)
    auto *fr = new QHBoxLayout();
    fr->addWidget(new QLabel(I18n::tr("p1_freqdiv"), sf));
    m_freqdiv = new QSpinBox(sf);
    m_freqdiv->setRange(1, 1000);
    fr->addWidget(m_freqdiv);
    fr->addStretch(1);
    sf->vbox()->addLayout(fr);
    v->addWidget(sf);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    for (auto *c : { m_iter, m_feed, m_point, m_smith, m_matching })
        connect(c, &QCheckBox::toggled, this, [this] { apply(); });
    connect(m_freqdiv, &QSpinBox::valueChanged, this, [this] { apply(); });

    // 自動スケール ON → 全行の「スケール指定」を外す / OFF → 全行に付ける
    connect(m_autoScale, &QCheckBox::toggled, this, [this](bool on) {
        if (m_updating) return;
        m_updating = true;
        for (FreqRow &r : m_rows) r.userScale->setChecked(!on);
        m_updating = false;
        apply();
    });

    connect(project, &Project::loaded, this, &Post1Tab::refresh);
    refresh();
}

void Post1Tab::addFreqRow(QWidget *, SectionBox *s, const QString &label,
                          FreqPlot *target)
{
    FreqRow row;
    row.target = target;
    row.enabled = new QCheckBox(label, s);
    row.userScale = new QCheckBox(I18n::tr("p1_user_scale"), s);
    row.min = new QLineEdit(s); row.min->setMaximumWidth(80);
    row.max = new QLineEdit(s); row.max->setMaximumWidth(80);
    row.div = new QSpinBox(s);  row.div->setRange(1, 1000);

    auto *h = new QHBoxLayout();
    h->addWidget(row.enabled, 1);
    h->addWidget(row.userScale);
    h->addWidget(new QLabel(I18n::tr("p1_min"), s));
    h->addWidget(row.min);
    h->addWidget(new QLabel(I18n::tr("p1_max"), s));
    h->addWidget(row.max);
    h->addWidget(new QLabel(I18n::tr("p1_div"), s));
    h->addWidget(row.div);
    s->vbox()->addLayout(h);

    connect(row.enabled, &QCheckBox::toggled, this, [this] { apply(); });
    connect(row.userScale, &QCheckBox::toggled, this, [this] { apply(); });
    connect(row.min, &QLineEdit::editingFinished, this, [this] { apply(); });
    connect(row.max, &QLineEdit::editingFinished, this, [this] { apply(); });
    connect(row.div, &QSpinBox::valueChanged, this, [this] { apply(); });

    m_rows.push_back(row);
}

void Post1Tab::apply()
{
    if (m_updating) return;
    PostOpts &po = m_p->post();
    po.plotiter  = m_iter->isChecked();
    po.plotfeed  = m_feed->isChecked();
    po.plotpoint = m_point->isChecked();
    po.plotsmith = m_smith->isChecked();
    po.matchingloss = m_matching->isChecked();
    po.freqdiv = m_freqdiv->value();
    for (FreqRow &r : m_rows) {
        r.target->enabled = r.enabled->isChecked();
        r.target->userScale = r.userScale->isChecked();
        r.target->min = r.min->text().toDouble();
        r.target->max = r.max->text().toDouble();
        r.target->div = r.div->value();
    }
    syncAutoScale();
    m_p->touch();
}

// 1 行でも「スケール指定」が付いていれば自動スケールは OFF 表示。
void Post1Tab::syncAutoScale()
{
    if (!m_autoScale) return;
    bool anyUser = false;
    for (const FreqRow &r : m_rows)
        if (r.userScale->isChecked()) anyUser = true;
    const bool guard = m_updating;
    m_updating = true;                   // マスターの toggled を空回りさせる
    m_autoScale->setChecked(!anyUser);
    m_updating = guard;
}

void Post1Tab::refresh()
{
    m_updating = true;
    const PostOpts &po = m_p->post();
    m_iter->setChecked(po.plotiter);
    m_feed->setChecked(po.plotfeed);
    m_point->setChecked(po.plotpoint);
    m_smith->setChecked(po.plotsmith);
    m_matching->setChecked(po.matchingloss);
    m_freqdiv->setValue(po.freqdiv);
    for (FreqRow &r : m_rows) {
        r.enabled->setChecked(r.target->enabled);
        r.userScale->setChecked(r.target->userScale);
        r.min->setText(QString::number(r.target->min, 'g', 6));
        r.max->setText(QString::number(r.target->max, 'g', 6));
        r.div->setValue(r.target->div);
    }
    syncAutoScale();
    m_updating = false;
}
