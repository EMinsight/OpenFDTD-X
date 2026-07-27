// SourceTab.cpp
#include "SourceTab.h"
#include "../core/Project.h"
#include "../widgets/MiniPlot.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <cmath>

using namespace ofd;

// ── タブ固有の翻訳キー (sox_) — file-local 登録 (既存 so_ は I18n.cpp) ───────
namespace {
const bool s_i18n = [] {
    using ofd::I18n;
    // 波源の種類 / Source type
    I18n::reg("sox_src_type", "波源の種類 / Source type", "Source type");
    I18n::reg("sox_feed", "給電点", "Feed");
    I18n::reg("sox_plane", "平面波入射", "Plane wave");
    // 給電点表の位相列 (mock: src_phase)。.ofd feed 行の 6 番目 = 位相 [deg]
    // (Feed::delay) と同じ値なので、見出しをモックの語に合わせる。
    I18n::reg("sox_phase", "位相 [°]", "Phase [°]");
    // 波形 / Waveform
    I18n::reg("sox_waveform", "波形", "Waveform");
    I18n::reg("sox_pulse", "ガウシアンパルス", "Gaussian pulse");
    I18n::reg("sox_cw", "正弦波", "Continuous wave");
    I18n::reg("sox_ricker", "リッカー", "Ricker");
    I18n::reg("sox_sweep", "周波数掃引", "Frequency sweep");
    I18n::reg("sox_pulse_width", "パルス幅 [s]", "Pulse width [s]");
    I18n::reg("sox_f0", "中心周波数 [GHz]", "Center frequency [GHz]");
    I18n::reg("sox_peak_time", "ピーク時間 [s]", "Peak time [s]");
    I18n::reg("sox_fmin", "fmin [GHz]", "fmin [GHz]");
    I18n::reg("sox_fmax", "fmax [GHz]", "fmax [GHz]");
    I18n::reg("sox_sweep_time", "掃引時間 [s]", "Sweep time [s]");
    I18n::reg("sox_preview", "プレビュー", "Preview");
    I18n::reg("sox_wave_hint",
              "この欄は波形プレビュー用のローカル設定です。"
              "実際のパルス幅・周波数は「全般」タブ (pulsewidth / frequency) が保持します。",
              "Local preview settings only. The actual pulsewidth and frequency "
              "live on the General tab (pulsewidth / frequency).");
    I18n::reg("sox_wave_t", "t", "t");
    I18n::reg("sox_wave_amp", "amp", "amp");
    // 平面波の振幅 (mock: src_amp) — .ofd に対応キーが無いローカル設定
    I18n::reg("sox_amp", "振幅", "Amplitude");
    I18n::reg("sox_amp_hint",
              "振幅は .ofd の planewave 行に無いため保存されません "
              "(給電点の電圧が振幅に相当)。",
              "Amplitude has no counterpart on the .ofd planewave line, so it "
              "is not saved (the feed voltage plays that role).");
    return true;
}();

// mock の <Row label>…<input>…</Row> 相当。表示切替できるよう 1 行 = 1 QWidget。
QWidget *makeParamRow(QWidget *parent, const QString &label,
                      const QString &value, int width = 110)
{
    auto *w = new QWidget(parent);
    auto *h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(6);
    auto *l = new QLabel(label, w);
    l->setMinimumWidth(120);
    auto *e = new QLineEdit(value, w);
    e->setMaximumWidth(width);
    h->addWidget(l);
    h->addWidget(e);
    h->addStretch(1);
    return w;
}
} // namespace

SourceTab::SourceTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    m_warning = new QLabel(I18n::tr("so_exclusive"), body);
    m_warning->setStyleSheet("color: #c25400; font-weight: 600;");
    m_warning->setVisible(false);
    v->addWidget(m_warning);

    // 波源の種類 / Source type (mock: <Radio name="src">)
    // 給電点と平面波は本家仕様で排他なので、選んだ側のセクションだけを見せる。
    auto *st = new SectionBox(I18n::tr("sox_src_type"), body);
    m_srcFeed  = new QRadioButton(I18n::tr("sox_feed"), st);
    m_srcPlane = new QRadioButton(I18n::tr("sox_plane"), st);
    m_srcFeed->setChecked(true);
    auto *strow = new QHBoxLayout();
    strow->addWidget(m_srcFeed);
    strow->addWidget(m_srcPlane);
    strow->addStretch(1);
    st->vbox()->addLayout(strow);
    v->addWidget(st);

    // feeds
    auto *sf = new SectionBox(I18n::tr("so_feeds"), body);
    m_feedSection = sf;
    m_feeds = new QTableWidget(0, 7, sf);
    m_feeds->setHorizontalHeaderLabels({
        I18n::tr("ma_dir"), "X [m]", "Y [m]", "Z [m]",
        I18n::tr("so_volt"), I18n::tr("sox_phase"), I18n::tr("so_z0") });
    // 旧見出し「遅延 [deg]」は同じ量 (feed 行の delay) — tooltip で補足
    if (auto *h = m_feeds->horizontalHeaderItem(5))
        h->setToolTip(I18n::tr("so_delay"));
    m_feeds->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_feeds->verticalHeader()->setDefaultSectionSize(24);
    m_feeds->setMinimumHeight(110);
    sf->vbox()->addWidget(m_feeds);

    auto *frow = new QHBoxLayout();
    auto *fadd = new QPushButton(I18n::tr("so_add_feed"), sf);
    auto *fdel = new QPushButton(I18n::tr("so_del_feed"), sf);
    frow->addWidget(fadd);
    frow->addWidget(fdel);
    frow->addStretch(1);
    sf->vbox()->addLayout(frow);
    v->addWidget(sf);

    // plane wave
    auto *sp = new SectionBox(I18n::tr("so_planewave"), body);
    m_pwSection = sp;
    m_pwEnable = new QCheckBox(I18n::tr("so_pw_enable"), sp);
    m_pwTheta = new QLineEdit(sp); m_pwTheta->setMaximumWidth(90);
    m_pwPhi   = new QLineEdit(sp); m_pwPhi->setMaximumWidth(90);
    m_pwPol = new QComboBox(sp);
    m_pwPol->addItem(I18n::tr("so_pol_v"));
    m_pwPol->addItem(I18n::tr("so_pol_h"));
    // 振幅 (mock: <Row label={t("src_amp")}> defaultValue="1.0") — ローカル状態
    m_pwAmp = new QLineEdit("1.0", sp); m_pwAmp->setMaximumWidth(90);
    sp->form()->addRow(m_pwEnable);
    sp->form()->addRow(I18n::tr("so_theta"), m_pwTheta);
    sp->form()->addRow(I18n::tr("so_phi"), m_pwPhi);
    sp->form()->addRow(I18n::tr("so_pol"), m_pwPol);
    sp->form()->addRow(I18n::tr("sox_amp"), m_pwAmp);
    auto *pwHint = new QLabel(I18n::tr("sox_amp_hint"), sp);
    pwHint->setWordWrap(true);
    pwHint->setStyleSheet("color:#888888; font-size:11px;");  // mock: muted text-sm
    sp->vbox()->addWidget(pwHint);
    v->addWidget(sp);

    // 波形 / Waveform (mock: <Seg> + 種類別パラメータ + プレビュー)
    auto *sw = new SectionBox(I18n::tr("sox_waveform"), body);
    auto *wrow = new QHBoxLayout();
    m_waveGroup = new QButtonGroup(this);
    m_waveGroup->setExclusive(true);
    static const char *kWave[4] = { "sox_pulse", "sox_cw", "sox_ricker",
                                    "sox_sweep" };
    for (int i = 0; i < 4; ++i) {
        auto *b = new QPushButton(I18n::tr(kWave[i]), sw);
        b->setCheckable(true);
        b->setStyleSheet("padding:2px 10px;");
        m_waveGroup->addButton(b, i);
        wrow->addWidget(b);
    }
    m_waveGroup->button(0)->setChecked(true);
    wrow->addStretch(1);
    sw->vbox()->addLayout(wrow);

    m_rowPulseWidth = makeParamRow(sw, I18n::tr("sox_pulse_width"), "5.08e-10");
    m_rowF0         = makeParamRow(sw, I18n::tr("sox_f0"), "2.500");
    m_rowPeak       = makeParamRow(sw, I18n::tr("sox_peak_time"), "1.5e-9");
    m_rowFmin       = makeParamRow(sw, I18n::tr("sox_fmin"), "2.0", 90);
    m_rowFmax       = makeParamRow(sw, I18n::tr("sox_fmax"), "3.0", 90);
    m_rowSweep      = makeParamRow(sw, I18n::tr("sox_sweep_time"), "2.0e-9");
    for (QWidget *r : { m_rowPulseWidth, m_rowF0, m_rowPeak,
                        m_rowFmin, m_rowFmax, m_rowSweep })
        sw->vbox()->addWidget(r);

    auto *wprev = new QHBoxLayout();
    wprev->addWidget(new QLabel(I18n::tr("sox_preview"), sw));
    m_wavePlot = new MiniPlot(sw);
    m_wavePlot->setLabels(I18n::tr("sox_wave_t"), I18n::tr("sox_wave_amp"));
    m_wavePlot->setYRange(-1.1, 1.1);
    m_wavePlot->setMinimumHeight(110);
    wprev->addWidget(m_wavePlot, 1);
    sw->vbox()->addLayout(wprev);

    auto *whint = new QLabel(I18n::tr("sox_wave_hint"), sw);
    whint->setWordWrap(true);
    sw->vbox()->addWidget(whint);
    v->addWidget(sw);

    // observation points
    // mock の観測点表には名前列 (t("mat_name")) がある。Probe に名前フィールドは
    // 無いので、post 処理のラベルと同じ probe_N を読み取り専用で見せる。
    auto *so = new SectionBox(I18n::tr("so_points"), body);
    m_points = new QTableWidget(0, 6, so);
    m_points->setHorizontalHeaderLabels({
        I18n::tr("ma_dir"), "X [m]", "Y [m]", "Z [m]", I18n::tr("so_prop"),
        I18n::tr("ma_name") });
    m_points->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_points->verticalHeader()->setDefaultSectionSize(24);
    m_points->setMinimumHeight(110);
    so->vbox()->addWidget(m_points);

    auto *prow = new QHBoxLayout();
    auto *padd = new QPushButton(I18n::tr("so_add_point"), so);
    auto *pdel = new QPushButton(I18n::tr("so_del_point"), so);
    prow->addWidget(padd);
    prow->addWidget(pdel);
    prow->addStretch(1);
    so->vbox()->addLayout(prow);
    v->addWidget(so);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    connect(fadd, &QPushButton::clicked, this, [this] {
        m_p->feeds().push_back(Feed{});
        refresh();
        m_p->touch();
    });
    connect(fdel, &QPushButton::clicked, this, [this] {
        const int r = m_feeds->currentRow();
        if (r >= 0 && r < m_p->feeds().size()) {
            m_p->feeds().removeAt(r);
            refresh();
            m_p->touch();
        }
    });
    connect(m_feeds, &QTableWidget::cellChanged, this, [this] {
        if (m_updating) return;
        applyFeeds();
        m_p->touch();
    });

    auto applyPw = [this] {
        if (m_updating) return;
        PlaneWave &pw = m_p->planewave();
        pw.enabled = m_pwEnable->isChecked();
        pw.theta = m_pwTheta->text().toDouble();
        pw.phi   = m_pwPhi->text().toDouble();
        pw.pol   = m_pwPol->currentIndex() + 1;
        updateExclusiveWarning();
        updateSourceType();
        m_p->touch();
    };
    connect(m_pwEnable, &QCheckBox::toggled, this, applyPw);
    connect(m_pwTheta, &QLineEdit::editingFinished, this, applyPw);
    connect(m_pwPhi, &QLineEdit::editingFinished, this, applyPw);
    connect(m_pwPol, &QComboBox::currentIndexChanged, this, applyPw);

    connect(padd, &QPushButton::clicked, this, [this] {
        Probe pr;
        if (m_p->probes().isEmpty()) pr.propagation = "+X";
        m_p->probes().push_back(pr);
        refresh();
        m_p->touch();
    });
    connect(pdel, &QPushButton::clicked, this, [this] {
        const int r = m_points->currentRow();
        if (r >= 0 && r < m_p->probes().size()) {
            m_p->probes().removeAt(r);
            refresh();
            m_p->touch();
        }
    });
    connect(m_points, &QTableWidget::cellChanged, this, [this] {
        if (m_updating) return;
        applyPoints();
        m_p->touch();
    });

    // 波源の種類 / 波形 はローカル状態 (モデルには書かない)
    for (auto *rb : { m_srcFeed, m_srcPlane })
        connect(rb, &QRadioButton::toggled, this, [this] { updateSourceType(); });
    connect(m_waveGroup, &QButtonGroup::idClicked, this, [this](int id) {
        m_wave = id;
        updateWaveform();
    });

    connect(project, &Project::loaded, this, &SourceTab::refresh);
    refresh();
    updateWaveform();
}

// 波源の種類 → 該当セクションのみ表示。ただしデータが入っている側は
// 隠さない (排他警告と併せて、取り違えで設定を見失わないようにする)。
void SourceTab::updateSourceType()
{
    if (!m_feedSection || !m_pwSection) return;
    const bool feedSel = m_srcFeed->isChecked();
    m_feedSection->setVisible(feedSel || !m_p->feeds().isEmpty());
    m_pwSection->setVisible(!feedSel || m_p->planewave().enabled);
}

// 波形の種類に応じてパラメータ行を出し入れし、プレビュー波形を作り直す。
// 式は mock (tabs.jsx SourceTab の MiniPlot data) をそのまま移した。
void SourceTab::updateWaveform()
{
    if (!m_wavePlot) return;
    m_rowPulseWidth->setVisible(m_wave == 0);
    m_rowF0->setVisible(m_wave == 0 || m_wave == 1 || m_wave == 2);
    m_rowPeak->setVisible(m_wave == 2);
    m_rowFmin->setVisible(m_wave == 3);
    m_rowFmax->setVisible(m_wave == 3);
    m_rowSweep->setVisible(m_wave == 3);

    MiniSeries s;
    s.color = QColor("#0078D4");
    for (int i = 0; i < 80; ++i) {
        const double x = i / 79.0;
        double y = 0;
        if (m_wave == 0) {                       // ガウシアンパルス
            y = std::exp(-std::pow((x - 0.3) * 7.0, 2))
                * std::sin(2 * M_PI * 4.0 * x);
        } else if (m_wave == 1) {                // 正弦波 (立ち上がり付き)
            y = x < 0.1 ? 0.0
                        : std::sin(2 * M_PI * 5.0 * x)
                              * (1.0 - std::exp(-(x - 0.1) * 20.0));
        } else if (m_wave == 2) {                // リッカー
            const double tau = (x - 0.4) * 8.0;
            const double pi2t2 = M_PI * M_PI * tau * tau;
            y = (1.0 - 2.0 * pi2t2) * std::exp(-pi2t2);
        } else {                                 // 周波数掃引
            y = std::sin(2 * M_PI * (3.0 + x * 5.0) * x);
        }
        s.pts.push_back({ x, y });
    }
    m_wavePlot->setSeries({ s });
}

void SourceTab::applyFeeds()
{
    auto &feeds = m_p->feeds();
    for (int r = 0; r < m_feeds->rowCount() && r < feeds.size(); ++r) {
        Feed &f = feeds[r];
        auto cell = [this, r](int c) {
            auto *it = m_feeds->item(r, c);
            return it ? it->text() : QString();
        };
        if (auto *cb = qobject_cast<QComboBox *>(m_feeds->cellWidget(r, 0)))
            f.dir = "XYZ"[cb->currentIndex()];
        f.x = cell(1).toDouble();
        f.y = cell(2).toDouble();
        f.z = cell(3).toDouble();
        f.volt  = cell(4).toDouble();
        f.delay = cell(5).toDouble();
        f.z0    = cell(6).toDouble();
    }
    updateExclusiveWarning();
}

void SourceTab::applyPoints()
{
    auto &probes = m_p->probes();
    for (int r = 0; r < m_points->rowCount() && r < probes.size(); ++r) {
        Probe &pr = probes[r];
        auto cell = [this, r](int c) {
            auto *it = m_points->item(r, c);
            return it ? it->text() : QString();
        };
        if (auto *cb = qobject_cast<QComboBox *>(m_points->cellWidget(r, 0)))
            pr.dir = "XYZ"[cb->currentIndex()];
        pr.x = cell(1).toDouble();
        pr.y = cell(2).toDouble();
        pr.z = cell(3).toDouble();
        if (r == 0) pr.propagation = cell(4);
    }
}

void SourceTab::updateExclusiveWarning()
{
    m_warning->setVisible(!m_p->feeds().isEmpty()
                          && m_p->planewave().enabled);
}

void SourceTab::refresh()
{
    m_updating = true;

    const auto &feeds = m_p->feeds();
    m_feeds->setRowCount(feeds.size());
    for (int r = 0; r < feeds.size(); ++r) {
        const Feed &f = feeds[r];
        auto *dir = new QComboBox(m_feeds);
        dir->addItems({ "X", "Y", "Z" });
        dir->setCurrentIndex(f.dir == 'X' ? 0 : f.dir == 'Y' ? 1 : 2);
        connect(dir, &QComboBox::currentIndexChanged, this, [this] {
            if (m_updating) return;
            applyFeeds();
            m_p->touch();
        });
        m_feeds->setCellWidget(r, 0, dir);
        const double vals[6] = { f.x, f.y, f.z, f.volt, f.delay, f.z0 };
        for (int c = 0; c < 6; ++c)
            m_feeds->setItem(r, 1 + c, new QTableWidgetItem(
                QString::number(vals[c], 'g', 8)));
    }

    const PlaneWave &pw = m_p->planewave();
    m_pwEnable->setChecked(pw.enabled);
    m_pwTheta->setText(QString::number(pw.theta, 'g', 8));
    m_pwPhi->setText(QString::number(pw.phi, 'g', 8));
    m_pwPol->setCurrentIndex(pw.pol == 2 ? 1 : 0);

    // 波源の種類ラジオはモデルから復元する (書き戻しはしない)
    if (pw.enabled && m_p->feeds().isEmpty()) m_srcPlane->setChecked(true);
    else                                      m_srcFeed->setChecked(true);

    const auto &probes = m_p->probes();
    m_points->setRowCount(probes.size());
    for (int r = 0; r < probes.size(); ++r) {
        const Probe &pr = probes[r];
        auto *dir = new QComboBox(m_points);
        dir->addItems({ "X", "Y", "Z" });
        dir->setCurrentIndex(pr.dir == 'X' ? 0 : pr.dir == 'Y' ? 1 : 2);
        connect(dir, &QComboBox::currentIndexChanged, this, [this] {
            if (m_updating) return;
            applyPoints();
            m_p->touch();
        });
        m_points->setCellWidget(r, 0, dir);
        const double vals[3] = { pr.x, pr.y, pr.z };
        for (int c = 0; c < 3; ++c)
            m_points->setItem(r, 1 + c, new QTableWidgetItem(
                QString::number(vals[c], 'g', 8)));
        auto *prop = new QTableWidgetItem(r == 0 ? pr.propagation : QString());
        if (r != 0) prop->setFlags(prop->flags() & ~Qt::ItemIsEditable);
        m_points->setItem(r, 4, prop);
        auto *name = new QTableWidgetItem(QStringLiteral("probe_%1").arg(r + 1));
        name->setFlags(name->flags() & ~Qt::ItemIsEditable);
        m_points->setItem(r, 5, name);
    }

    updateExclusiveWarning();
    updateSourceType();
    m_updating = false;
}
