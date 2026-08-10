// PlotPanel.cpp
#include "PlotPanel.h"
#include "../core/Project.h"
#include "../em/Reflection.h"
#include "../I18n.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QTextStream>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

using namespace ofd;

namespace {
const bool s_i18n = [] {
    ofd::I18n::reg("ppb_csv_tip", "表示中のデータを CSV 保存",
                   "Save the displayed data as CSV");
    ofd::I18n::reg("ppb_png_tip", "プロットを PNG 保存",
                   "Save the plot as PNG");
    ofd::I18n::reg("ppb_mode_wave", "波形", "Waveform");
    ofd::I18n::reg("ppb_mode_conv", "収束", "Convergence");
    ofd::I18n::reg("ppb_mode_freq", "周波数特性", "Frequency");
    ofd::I18n::reg("ppb_mode_smith", "スミスチャート", "Smith chart");
    ofd::I18n::reg("ppb_mode_far", "放射パターン", "Pattern");
    ofd::I18n::reg("ppb_mode_post", "ポスト表", "Post tables");
    ofd::I18n::reg("ppb_post_none_tip",
        "ポスト処理のテキスト表がまだありません "
        "(ポスト(1)/(2) の項目を有効にして実行すると feed.log / point.log / "
        "far0d.log / near1d.log が出ます)",
        "No post-processing tables yet (enable items on Post-Proc (1)/(2) and "
        "run to produce feed.log / point.log / far0d.log / near1d.log)");
    ofd::I18n::reg("ppb_post_logy", "対数 Y 軸", "Log Y axis");
    ofd::I18n::reg("pp_post_decimated",
                   "※ %1 行を %2 点へ等間隔に間引いて読み込みました",
                   "* decimated on load: %1 rows -> %2 points (uniform)");
    ofd::I18n::reg("pp_post_title", "ポスト処理の表 (%1)",
                   "Post-processing table (%1)");
    ofd::I18n::reg("pp_post_hint",
        "ev2d / ev3d を使わず、ofd_post が出したテキスト表をそのまま描いています。"
        "列の意味はカーネルの出力どおりです。",
        "Drawn directly from the text tables written by ofd_post - no ev2d or "
        "ev3d involved. The columns are exactly as the kernel wrote them.");
    ofd::I18n::reg("ppb_smith_none_tip",
        "計算を実行すると <kernel>.log の給電点表から Γ = (Z−Z0)/(Z+Z0) を"
        "描きます",
        "Run the solver to plot Γ = (Z−Z0)/(Z+Z0) from the feed table in "
        "<kernel>.log");
    ofd::I18n::reg("pp_smith",
                   "スミスチャート / S11 (実行結果 <kernel>.log)",
                   "Smith chart / S11 (run result <kernel>.log)");
    // 1 給電点 = 1 ポートなので S11 = Γ。多ポートの S21 等はカーネルが
    // 出さない (ofd は給電点ごとの Zin しか書かない) — 誤解を招かないよう明示
    ofd::I18n::reg("pp_smith_note",
        "※ 給電点 1 個 = 1 ポートのため S11 = Γ です。S21 等の伝達項は "
        "<kernel>.log に含まれません",
        "* One feed = one port, so S11 = Γ. Transfer terms such as S21 are "
        "not present in <kernel>.log");
    ofd::I18n::reg("pp_smith_range", "f: %1 … %2 Hz  (%3 点)",
                   "f: %1 … %2 Hz  (%3 points)");
    ofd::I18n::reg("pp_smith_best", "最良整合 f = %1 Hz",
                   "Best match at f = %1 Hz");
    ofd::I18n::reg("pp_smith_perfect", "−∞ (完全整合)",
                   "−∞ (perfect match)");
    ofd::I18n::reg("pp_smith_total", "∞ (全反射)", "∞ (total reflection)");
    ofd::I18n::reg("pp_smith_marks", "○ = f 最小 / ● = f 最大",
                   "○ = lowest f / ● = highest f");
    ofd::I18n::reg("ppb_freq_none_tip",
        "計算を実行すると <kernel>.log の給電点表がここに表示されます",
        "Run the solver to show the feed table from <kernel>.log here");
    ofd::I18n::reg("ppb_far_none_tip",
        "ポスト処理 (plotfar1d) を実行すると far1d.log がここに表示されます",
        "Run post processing (plotfar1d) to show far1d.log here");
    ofd::I18n::reg("pp_freqchar", "給電点特性 (実行結果 <kernel>.log)",
                   "Feed-point response (run result <kernel>.log)");
    ofd::I18n::reg("pp_farpattern", "遠方界パターン (実行結果 far1d.log)",
                   "Far-field pattern (run result far1d.log)");
    // 室内音響: ソルバ励振はガウシアンパルス (インパルス応答の計測)。
    // 音源リストの WAV はソルバへは入らず、可聴化で RIR と畳み込まれる —
    // 「スピーカーなら音声ファイルの波形になるのでは」という誤解への注記
    ofd::I18n::reg("ppb_wave_ac_note",
        "※ ソルバ励振はガウシアンパルス (インパルス応答の計測)。音源リストの"
        "音声ファイルは可聴化タブで RIR と畳み込まれます",
        "* The solver is excited with a Gaussian pulse (impulse-response "
        "measurement). Source-list audio files are convolved with the RIR "
        "in the Auralization tab");
    // 室内音響の収束表示: 実行カーネルは ofd (電磁 FDTD) の波動アナロジーで、
    // ⟨p⟩/⟨v⟩ は定量的な音響量ではない (ADR-0004 — 絶対規則 5)
    ofd::I18n::reg("ppb_conv_ac_note",
        "※ 波動アナロジー (電磁 FDTD) — 定量的な音響量ではありません",
        "* Wave analogy (electromagnetic FDTD) — not quantitative "
        "acoustic quantities");
    return true;
}();
} // namespace

PlotPanel::PlotPanel(Project *project, QWidget *parent)
    : QWidget(parent), m_project(project)
{
    setObjectName("PlotPanel");
    setMinimumSize(320, 200);
    connect(project, &Project::changed, this, qOverload<>(&QWidget::update));

    // 左上のモード切替 + 右上の CSV / PNG 保存ボタン
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 2, 6, 0);
    auto *btnRow = new QHBoxLayout();
    btnRow->addSpacing(6);
    auto modeBtn = [this](const char *key) {
        auto *b = new QToolButton(this);
        b->setText(I18n::tr(key));
        b->setCheckable(true);
        b->setAutoRaise(true);
        return b;
    };
    m_btnWave = modeBtn("ppb_mode_wave");
    m_btnConv = modeBtn("ppb_mode_conv");
    m_btnFreq  = modeBtn("ppb_mode_freq");
    m_btnSmith = modeBtn("ppb_mode_smith");
    m_btnFar   = modeBtn("ppb_mode_far");
    m_btnPost  = modeBtn("ppb_mode_post");
    btnRow->addWidget(m_btnWave);
    btnRow->addWidget(m_btnConv);
    btnRow->addWidget(m_btnFreq);
    btnRow->addWidget(m_btnSmith);
    btnRow->addWidget(m_btnFar);
    btnRow->addWidget(m_btnPost);
    // ポスト表モードの表選択と対数軸 (このモードのときだけ出す)
    m_tableSel = new QComboBox(this);
    m_tableSel->setMinimumWidth(200);
    m_tableSel->setVisible(false);
    m_logY = new QCheckBox(I18n::tr("ppb_post_logy"), this);
    m_logY->setVisible(false);
    btnRow->addWidget(m_tableSel);
    btnRow->addWidget(m_logY);
    btnRow->addStretch(1);
    m_csvBtn = new QToolButton(this);
    m_csvBtn->setText(QStringLiteral("CSV"));
    m_csvBtn->setToolTip(I18n::tr("ppb_csv_tip"));
    m_pngBtn = new QToolButton(this);
    m_pngBtn->setText(QStringLiteral("PNG"));
    m_pngBtn->setToolTip(I18n::tr("ppb_png_tip"));
    btnRow->addWidget(m_csvBtn);
    btnRow->addWidget(m_pngBtn);
    outer->addLayout(btnRow);
    outer->addStretch(1);

    connect(m_btnWave, &QToolButton::clicked, this, &PlotPanel::showWaveform);
    connect(m_btnConv, &QToolButton::clicked, this,
            &PlotPanel::showConvergence);
    connect(m_btnFreq, &QToolButton::clicked, this, &PlotPanel::showFreqChar);
    connect(m_btnSmith, &QToolButton::clicked, this, &PlotPanel::showSmith);
    connect(m_btnFar, &QToolButton::clicked, this, &PlotPanel::showFarPattern);
    connect(m_btnPost, &QToolButton::clicked, this, &PlotPanel::showPostTable);
    connect(m_tableSel, &QComboBox::currentIndexChanged, this,
            [this] { update(); });
    connect(m_logY, &QCheckBox::toggled, this, [this] { update(); });
    connect(m_csvBtn, &QToolButton::clicked, this, &PlotPanel::saveCsvDialog);
    connect(m_pngBtn, &QToolButton::clicked, this, &PlotPanel::savePngDialog);
    updateModeButtons();

    // ドメイン切替でモードボタンの出し分けを更新 (初回は下で直接反映)
    connect(project, &Project::domainChanged, this,
            [this] { setDomain(m_project->activeDomain()); });
    setDomain(project->activeDomain());
}

void PlotPanel::setDomain(Domain d)
{
    m_domain = d;
    updateDomainVisibility();
    update();
}

// 現在のドメインで意味を持つモードか (ドメイン監査の結果に基づく出し分け)
bool PlotPanel::modeAllowed(Mode m) const
{
    switch (m) {
    case Waveform:
        // ガウシアン励振の時間波形 — BELLHOP (水中音響) は周波数領域で
        // 時間波形励振が無い
        return m_domain != Domain::Underwater;
    case FreqChar:
    case Smith:
        // 給電点 Rin/Xin/Ref と、そこから作る Γ — EM 専用
        return m_domain == Domain::EM;
    case Pattern:
        // far1d.log の放射パターン — 音響/水中には無い
        return m_domain == Domain::EM || m_domain == Domain::Optical;
    case PostLog:
        // ofd_post のテキスト表 — 出るかどうかは結果次第でドメインを問わない
        return true;
    case Convergence:
    default:
        return true;    // 収束履歴は全ドメイン共通
    }
}

// ドメインで意味を持たないモードのボタンを隠す (削除はしない)。
// 結果が無い間の無効化は従来どおり updateModeButtons() が行う。
void PlotPanel::updateDomainVisibility()
{
    m_btnWave->setVisible(modeAllowed(Waveform));
    m_btnFreq->setVisible(modeAllowed(FreqChar));
    m_btnSmith->setVisible(modeAllowed(Smith));
    m_btnFar->setVisible(modeAllowed(Pattern));
    m_btnPost->setVisible(modeAllowed(PostLog));
    // 隠したモードが選択中だった場合は表示可能なモードへフォールバック
    if (!modeAllowed(m_mode))
        m_mode = modeAllowed(Waveform) ? Waveform : Convergence;
    updateModeButtons();
}

void PlotPanel::setMode(Mode m)
{
    m_mode = m;
    updateModeButtons();
    update();
}

void PlotPanel::showWaveform()    { setMode(Waveform); }
void PlotPanel::showConvergence() { setMode(Convergence); }

void PlotPanel::showFreqChar()
{
    if (hasFreqChar()) setMode(FreqChar);
    else updateModeButtons();
}

void PlotPanel::showSmith()
{
    if (hasFreqChar()) setMode(Smith);
    else updateModeButtons();
}

void PlotPanel::showFarPattern()
{
    if (hasFarPattern()) setMode(Pattern);
    else updateModeButtons();
}

void PlotPanel::showPostTable()
{
    if (hasPostTables()) setMode(PostLog);
    else updateModeButtons();
}

void PlotPanel::updateModeButtons()
{
    m_btnWave->setChecked(m_mode == Waveform);
    m_btnConv->setChecked(m_mode == Convergence);
    m_btnFreq->setChecked(m_mode == FreqChar);
    m_btnSmith->setChecked(m_mode == Smith);
    m_btnFar->setChecked(m_mode == Pattern);
    m_btnPost->setChecked(m_mode == PostLog);
    // 結果系モードはデータが届くまで無効 (未実装ではなく「まだ結果が無い」)
    m_btnFreq->setEnabled(hasFreqChar());
    m_btnFreq->setToolTip(hasFreqChar() ? QString()
                                        : I18n::tr("ppb_freq_none_tip"));
    m_btnSmith->setEnabled(hasFreqChar());   // 素データは給電点表と同じ
    m_btnSmith->setToolTip(hasFreqChar() ? QString()
                                         : I18n::tr("ppb_smith_none_tip"));
    m_btnFar->setEnabled(hasFarPattern());
    m_btnFar->setToolTip(hasFarPattern() ? QString()
                                         : I18n::tr("ppb_far_none_tip"));
    m_btnPost->setEnabled(hasPostTables());
    m_btnPost->setToolTip(hasPostTables() ? QString()
                                          : I18n::tr("ppb_post_none_tip"));
    // 表選択と対数軸はポスト表モードのときだけ意味がある
    const bool post = (m_mode == PostLog) && hasPostTables();
    m_tableSel->setVisible(post);
    m_logY->setVisible(post);
}

void PlotPanel::setRunResults(const QVector<FeedSweep> &sweeps,
                              const QVector<FarPattern> &patterns)
{
    m_sweeps = sweeps;
    m_patterns = patterns;
    // 新しい結果が届いたら周波数特性を前面に (無ければパターン)。
    // ただし現在のドメインで非表示のモードには切り替えない
    if (hasFreqChar() && modeAllowed(FreqChar)) m_mode = FreqChar;
    else if (hasFarPattern() && modeAllowed(Pattern)) m_mode = Pattern;
    updateModeButtons();
    update();
}

// ofd_post のテキスト表を受け取る。ev.ev2 の有無とは無関係 — こちらは
// 「作図が無くても結果が見える」ための経路なので、表があれば必ず出す。
void PlotPanel::setPostTables(const QVector<PostTable> &tables)
{
    m_tables = tables;
    m_tableSel->blockSignals(true);
    m_tableSel->clear();
    for (const PostTable &t : m_tables) {
        QString label = t.sourceFile;
        if (!t.title.isEmpty())
            label += QStringLiteral(" — ") + t.title;
        m_tableSel->addItem(label);
    }
    m_tableSel->setCurrentIndex(m_tables.isEmpty() ? -1 : 0);
    m_tableSel->blockSignals(false);
    updateModeButtons();
    update();
}

void PlotPanel::clearRunResults()
{
    m_sweeps.clear();
    m_patterns.clear();
    setPostTables(QVector<PostTable>());
    if (m_mode == FreqChar || m_mode == Smith || m_mode == Pattern
        || m_mode == PostLog)
        m_mode = Convergence;    // 実行中は収束を見せる
    updateModeButtons();
    update();
}

void PlotPanel::saveCsvDialog()
{
    const char *suggest =
        m_mode == FreqChar ? "feed_response.csv" :
        m_mode == Smith ? "reflection.csv" :
        m_mode == Pattern ? "far_pattern.csv" :
        m_mode == PostLog ? "post_table.csv" : "convergence.csv";
    const QString path = QFileDialog::getSaveFileName(
        this, I18n::tr("ppb_csv_tip"), QString::fromLatin1(suggest),
        "CSV (*.csv)");
    if (!path.isEmpty()) exportCsv(path);
}

void PlotPanel::savePngDialog()
{
    const QString path = QFileDialog::getSaveFileName(
        this, I18n::tr("ppb_png_tip"), "plot.png", "PNG (*.png)");
    if (path.isEmpty()) return;
    // ボタンを写し込まないため一時的に隠して grab する
    // (モードボタンはドメインで出し分けているので元の可視状態へ戻す)
    QToolButton *btns[] = { m_csvBtn, m_pngBtn, m_btnWave, m_btnConv,
                            m_btnFreq, m_btnSmith, m_btnFar, m_btnPost };
    const int n = int(sizeof(btns) / sizeof(btns[0]));
    QVector<bool> vis(n);
    for (int i = 0; i < n; ++i) {
        vis[i] = btns[i]->isVisible();
        btns[i]->setVisible(false);
    }
    grab().save(path);
    for (int i = 0; i < n; ++i) btns[i]->setVisible(vis[i]);
}

void PlotPanel::clearConvergence()
{
    m_steps.clear(); m_eAvg.clear(); m_hAvg.clear();
    update();
}

void PlotPanel::addConvergencePoint(int step, double e, double h)
{
    m_steps.push_back(step);
    m_eAvg.push_back(e);
    m_hAvg.push_back(h);
    if (m_mode == Convergence) update();
}

bool PlotPanel::exportCsv(const QString &path) const
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QTextStream out(&f);
    if (m_mode == FreqChar && !m_sweeps.isEmpty()) {
        out << "feed,frequency_Hz,Rin_ohm,Xin_ohm,Ref_dB,VSWR\n";
        for (const FeedSweep &s : m_sweeps)
            for (const FeedSweepPoint &pt : s.points)
                out << s.feedIndex << ',' << pt.freqHz << ',' << pt.rin << ','
                    << pt.xin << ',' << pt.refDb << ',' << pt.vswr << '\n';
    } else if (m_mode == Smith && !m_sweeps.isEmpty()) {
        // Γ は Rin/Xin/Z0 から求めた値 (em/Reflection)。∞ になる列
        // (完全整合の S11[dB]、全反射の VSWR) は空欄にする — 丸めた有限値を
        // 書くと「整合していないのに整合して見える」ため
        out << "feed,frequency_Hz,Rin_ohm,Xin_ohm,Z0_ohm,"
               "Gamma_re,Gamma_im,Gamma_abs,Gamma_deg,S11_dB,VSWR\n";
        for (const FeedSweep &s : m_sweeps) {
            for (const FeedSweepPoint &pt : s.points) {
                const em::Reflection r =
                    em::reflectionFromZ(pt.rin, pt.xin, s.z0);
                if (!r.valid) continue;
                out << s.feedIndex << ',' << pt.freqHz << ',' << pt.rin << ','
                    << pt.xin << ',' << s.z0 << ',' << r.gammaRe << ','
                    << r.gammaIm << ',' << r.magnitude << ',' << r.phaseDeg
                    << ',';
                if (std::isfinite(r.s11Db)) out << r.s11Db;
                out << ',';
                if (std::isfinite(r.vswr)) out << r.vswr;
                out << '\n';
            }
        }
    } else if (m_mode == Pattern && !m_patterns.isEmpty()) {
        out << "plane,frequency_Hz,deg,Eabs_dB\n";
        for (const FarPattern &pat : m_patterns)
            for (int i = 0; i < pat.deg.size(); ++i)
                out << pat.plane << ',' << pat.freqHz << ',' << pat.deg[i]
                    << ',' << pat.eAbsDb[i] << '\n';
    } else if (m_mode == PostLog && !m_tables.isEmpty()) {
        // 表示中の表をそのまま出す。列名はカーネルの出力どおり
        const int i = qBound(0, m_tableSel->currentIndex(),
                             int(m_tables.size()) - 1);
        const PostTable &t = m_tables[i];
        out << t.xName;
        for (const QString &n : t.yNames) out << ',' << n;
        out << '\n';
        for (int r = 0; r < t.x.size(); ++r) {
            out << t.x[r];
            for (const QVector<double> &c : t.y) out << ',' << c[r];
            out << '\n';
        }
    } else {
        // 音響/水中は電磁界 (E/H) ではなく音圧/粒子速度 (p/v) の平均
        const bool acoustic = (m_domain == Domain::Acoustic
                               || m_domain == Domain::Underwater);
        out << (acoustic ? "step,pavg,vavg\n" : "step,Eavg,Havg\n");
        for (int i = 0; i < m_steps.size(); ++i)
            out << m_steps[i] << ',' << m_eAvg[i] << ',' << m_hAvg[i] << '\n';
    }
    return true;
}

// ── 給電点特性 (Rin / Xin / Ref[dB]) ────────────────────────────────────────
void PlotPanel::paintFreqChar(QPainter &p, const QRectF &plot,
                              const QColor &accent)
{
    p.drawText(QPointF(plot.left(), plot.top() - 8), I18n::tr("pp_freqchar"));
    const FeedSweep &s = m_sweeps.first();
    const QVector<FeedSweepPoint> &pts = s.points;
    if (pts.isEmpty()) return;

    double fmin = pts.first().freqHz, fmax = pts.last().freqHz;
    if (fmax <= fmin) fmax = fmin + 1;
    double zmin = 0, zmax = -1e300;
    double rmin = 0, rmax = -1e300;
    for (const FeedSweepPoint &pt : pts) {
        zmin = std::min({ zmin, pt.rin, pt.xin });
        zmax = std::max({ zmax, pt.rin, pt.xin });
        rmin = std::min(rmin, pt.refDb);
        rmax = std::max(rmax, pt.refDb);
    }
    if (zmax <= zmin) zmax = zmin + 1;
    if (rmax <= rmin) rmax = rmin + 1;

    auto xAt = [&](double f) {
        return plot.left() + plot.width() * (f - fmin) / (fmax - fmin);
    };
    auto series = [&](auto get, double lo, double hi, const QPen &pen) {
        QPainterPath path;
        for (int i = 0; i < pts.size(); ++i) {
            const double y = plot.bottom()
                - plot.height() * (get(pts[i]) - lo) / (hi - lo) * 0.92;
            const double x = xAt(pts[i].freqHz);
            if (i == 0) path.moveTo(x, y); else path.lineTo(x, y);
        }
        p.setPen(pen);
        p.drawPath(path);
    };
    series([](const FeedSweepPoint &q) { return q.rin; }, zmin, zmax,
           QPen(accent, 2));
    series([](const FeedSweepPoint &q) { return q.xin; }, zmin, zmax,
           QPen(accent, 2, Qt::DashLine));
    series([](const FeedSweepPoint &q) { return q.refDb; }, rmin, rmax,
           QPen(QColor("#888888"), 2));

    p.setPen(accent);
    p.drawText(QPointF(plot.right() - 190, plot.top() + 16), "Rin");
    p.drawText(QPointF(plot.right() - 150, plot.top() + 16), "Xin(--)");
    p.setPen(QColor("#888888"));
    p.drawText(QPointF(plot.right() - 80, plot.top() + 16), "Ref[dB]");
    p.setPen(palette().text().color());
    p.drawText(QPointF(plot.left(), plot.bottom() + 16),
        QStringLiteral("f: %1 … %2 Hz   Z: %3 … %4 Ω   Ref: %5 … %6 dB"
                       "   (feed #%7, Z0=%8Ω)")
            .arg(QString::number(fmin, 'g', 4),
                 QString::number(fmax, 'g', 4),
                 QString::number(zmin, 'g', 3),
                 QString::number(zmax, 'g', 3),
                 QString::number(rmin, 'g', 3),
                 QString::number(rmax, 'g', 3))
            .arg(s.feedIndex)
            .arg(s.z0));
}

// ── スミスチャート (Γ = (Z−Z0)/(Z+Z0) の軌跡) ──────────────────────────────
// 素データは給電点特性と同じ <kernel>.log の給電点表。Γ・S11・VSWR の式と
// 目盛円の幾何は src/em/Reflection (Qt 非依存・selftest 済み) にある。
void PlotPanel::paintSmith(QPainter &p, const QRectF &plot,
                           const QColor &accent)
{
    p.drawText(QPointF(plot.left(), plot.top() - 8), I18n::tr("pp_smith"));
    const FeedSweep &s = m_sweeps.first();
    const QVector<FeedSweepPoint> &pts = s.points;
    if (pts.isEmpty()) return;

    // 右側は読み取り値の欄に使い、左側の正方形にチャートを描く
    const qreal readout = qMin<qreal>(260.0, plot.width() * 0.42);
    const qreal side = qMin(plot.width() - readout - 12.0, plot.height()) - 8.0;
    if (side < 40.0) return;                  // 小さすぎるときは描かない
    const QPointF c(plot.left() + 6.0 + side / 2.0,
                    plot.top() + (plot.height() - side) / 2.0 + side / 2.0);
    const qreal R = side / 2.0;
    auto at = [&](double gre, double gim) {   // Γ → 画面座標 (虚部は上向き)
        return QPointF(c.x() + gre * R, c.y() - gim * R);
    };
    const QRectF unit(c.x() - R, c.y() - R, 2 * R, 2 * R);

    // 目盛 (単位円の内側だけ) — 円弧のはみ出しはクリップで落とす
    QPainterPath clip;
    clip.addEllipse(unit);
    p.save();
    p.setClipPath(clip);
    p.setPen(QPen(palette().midlight().color(), 1));
    for (double r : { 0.2, 0.5, 1.0, 2.0, 5.0 }) {
        const em::SmithCircle g = em::constantResistanceCircle(r);
        if (!g.valid) continue;
        p.drawEllipse(QPointF(c.x() + g.cx * R, c.y() - g.cy * R),
                      g.radius * R, g.radius * R);
    }
    for (double x : { 0.2, 0.5, 1.0, 2.0, 5.0 }) {
        for (double sgn : { 1.0, -1.0 }) {
            const em::SmithCircle g = em::constantReactanceCircle(sgn * x);
            if (!g.valid) continue;
            p.drawEllipse(QPointF(c.x() + g.cx * R, c.y() - g.cy * R),
                          g.radius * R, g.radius * R);
        }
    }
    p.drawLine(at(-1.0, 0.0), at(1.0, 0.0));  // 実軸 (x = 0)
    p.restore();

    // 単位円 (|Γ| = 1) と中心 (整合点 Z = Z0)
    p.setPen(QPen(palette().mid().color(), 1));
    p.drawEllipse(unit);
    p.drawLine(QPointF(c.x() - 3, c.y()), QPointF(c.x() + 3, c.y()));
    p.drawLine(QPointF(c.x(), c.y() - 3), QPointF(c.x(), c.y() + 3));

    // Γ の軌跡 (周波数の昇順)。最良整合点 (|Γ| 最小) を控えておく
    QPainterPath locus;
    bool started = false;
    int best = -1;
    double bestMag = 1e300;
    QPointF firstPt, lastPt;
    for (int i = 0; i < pts.size(); ++i) {
        const em::Reflection r = em::reflectionFromZ(pts[i].rin, pts[i].xin,
                                                     s.z0);
        if (!r.valid) continue;
        const QPointF q = at(r.gammaRe, r.gammaIm);
        if (!started) { locus.moveTo(q); firstPt = q; started = true; }
        else locus.lineTo(q);
        lastPt = q;
        if (r.magnitude < bestMag) { bestMag = r.magnitude; best = i; }
    }
    if (!started) return;
    p.setPen(QPen(accent, 2));
    p.drawPath(locus);
    // 始点 (○) と終点 (●) — 掃引の向きが分かるように区別する
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(firstPt, 4.0, 4.0);
    p.setBrush(accent);
    p.drawEllipse(lastPt, 3.5, 3.5);
    p.setBrush(Qt::NoBrush);

    // 読み取り値
    const qreal tx = plot.right() - readout + 6.0;
    qreal ty = plot.top() + 14.0;
    auto line = [&](const QString &t) {
        p.drawText(QPointF(tx, ty), t);
        ty += 16.0;
    };
    p.setPen(palette().text().color());
    line(QStringLiteral("feed #%1   Z0 = %2 Ω").arg(s.feedIndex)
             .arg(QString::number(s.z0, 'g', 4)));
    line(I18n::tr("pp_smith_range")
             .arg(QString::number(pts.first().freqHz, 'g', 4),
                  QString::number(pts.last().freqHz, 'g', 4))
             .arg(pts.size()));
    ty += 4.0;
    if (best >= 0) {
        const em::Reflection r =
            em::reflectionFromZ(pts[best].rin, pts[best].xin, s.z0);
        p.setPen(accent);
        line(I18n::tr("pp_smith_best")
                 .arg(QString::number(pts[best].freqHz, 'g', 5)));
        p.setPen(palette().text().color());
        line(QStringLiteral("  Z = %1 %2 j%3 Ω")
                 .arg(QString::number(pts[best].rin, 'f', 2),
                      pts[best].xin < 0 ? QStringLiteral("−")
                                        : QStringLiteral("+"),
                      QString::number(std::fabs(pts[best].xin), 'f', 2)));
        line(QStringLiteral("  |Γ| = %1  ∠%2°")
                 .arg(QString::number(r.magnitude, 'f', 4),
                      QString::number(r.phaseDeg, 'f', 1)));
        // ∞ は数値にせずそのまま出す (丸めた有限値を出さない)
        line(QStringLiteral("  S11 = %1 dB")
                 .arg(std::isfinite(r.s11Db)
                          ? QString::number(r.s11Db, 'f', 2)
                          : I18n::tr("pp_smith_perfect")));
        line(QStringLiteral("  VSWR = %1")
                 .arg(std::isfinite(r.vswr) ? QString::number(r.vswr, 'f', 3)
                                            : I18n::tr("pp_smith_total")));
    }
    ty += 4.0;
    p.setPen(palette().mid().color());
    line(I18n::tr("pp_smith_marks"));

    // 1 給電点 = 1 ポートである旨 (S21 が無いことの説明)
    QFont f = p.font();
    const QFont keep = f;
    f.setPointSizeF(qMax(6.0, f.pointSizeF() - 1.0));
    p.setFont(f);
    p.setPen(palette().mid().color());
    p.drawText(QRectF(plot.left(), plot.bottom() + 2, plot.width(), 30),
               Qt::AlignLeft | Qt::TextWordWrap, I18n::tr("pp_smith_note"));
    p.setFont(keep);
}

// ── 遠方界パターン (面ごとの E-abs[dB]) ─────────────────────────────────────
void PlotPanel::paintFarPattern(QPainter &p, const QRectF &plot,
                                const QColor &accent)
{
    p.drawText(QPointF(plot.left(), plot.top() - 8),
               I18n::tr("pp_farpattern"));
    double dbMax = -1e300;
    for (const FarPattern &pat : m_patterns)
        for (double v : pat.eAbsDb) dbMax = std::max(dbMax, v);
    if (dbMax < -1e299) return;
    // -240 dB のヌル床で潰れないよう表示レンジは最大から 60 dB
    const double dbMin = dbMax - 60.0;

    const QColor colors[3] = { accent, QColor("#C08030"), QColor("#3C8CD0") };
    int ci = 0;
    double legendX = plot.right() - 240;
    for (const FarPattern &pat : m_patterns) {
        const QColor c = colors[ci % 3];
        QPainterPath path;
        for (int i = 0; i < pat.deg.size(); ++i) {
            const double x = plot.left()
                + plot.width() * pat.deg[i] / 360.0;
            const double v =
                std::max(dbMin, std::min(dbMax, pat.eAbsDb[i]));
            const double y = plot.bottom()
                - plot.height() * (v - dbMin) / (dbMax - dbMin) * 0.92;
            if (i == 0) path.moveTo(x, y); else path.lineTo(x, y);
        }
        p.setPen(QPen(c, 2));
        p.drawPath(path);
        p.drawText(QPointF(legendX, plot.top() + 16), pat.plane);
        legendX += 80;
        ++ci;
    }
    p.setPen(palette().text().color());
    p.drawText(QPointF(plot.left(), plot.bottom() + 16),
        QStringLiteral("deg: 0 … 360   E-abs: %1 … %2 dB   f=%3 Hz")
            .arg(QString::number(dbMin, 'f', 0),
                 QString::number(dbMax, 'f', 1),
                 QString::number(m_patterns.first().freqHz, 'g', 4)));
}

// ── ofd_post のテキスト表をそのまま描く (ev2d / ev3d を介さない経路) ────────
//
// 列の意味はカーネルが決めたものなので、GUI は解釈を足さずに列名を凡例に
// 出すだけにする。単位の違う列 (V[V] と I[A]、E-abs[dB] と degree など) が
// 同じ表に並ぶので、**縦軸は列ごとに自分の最小最大へ正規化**して重ねる。
// 共通の縦軸に押し込むと桁の小さい列が潰れて「出ていない」ように見える。
// 各列のレンジは凡例に数値で書くので、読み取りに必要な情報は失われない。
void PlotPanel::paintPostTable(QPainter &p, const QRectF &plot)
{
    const int idx = qBound(0, m_tableSel->currentIndex(),
                           int(m_tables.size()) - 1);
    const PostTable &t = m_tables[idx];
    if (!t.isValid()) return;

    QString head = I18n::tr("pp_post_title").arg(t.sourceFile);
    if (!t.title.isEmpty()) head += QStringLiteral("  ") + t.title;
    if (!t.fixed.isEmpty()) head += QStringLiteral("  [") + t.fixed
                                  + QStringLiteral("]");
    p.drawText(QPointF(plot.left(), plot.top() - 8), head);

    double xmin = t.x.first(), xmax = t.x.first();
    for (double v : t.x) { xmin = std::min(xmin, v); xmax = std::max(xmax, v); }
    if (xmax <= xmin) xmax = xmin + 1.0;

    const bool logY = m_logY->isChecked();
    const QColor colors[6] = { QColor("#C42B1C"), QColor("#0078D4"),
                               QColor("#2E8B57"), QColor("#C08030"),
                               QColor("#8A2BE2"), QColor("#008080") };
    qreal legendY = plot.top() + 16;
    for (int c = 0; c < t.y.size(); ++c) {
        const QVector<double> &col = t.y[c];
        // 対数軸は正の値だけ (0 や負を含む列は線形のまま — 落とさない)
        bool allPositive = true;
        for (double v : col) if (v <= 0) { allPositive = false; break; }
        const bool lg = logY && allPositive;

        double lo = 1e300, hi = -1e300;
        for (double v : col) {
            const double w = lg ? std::log10(v) : v;
            lo = std::min(lo, w); hi = std::max(hi, w);
        }
        if (hi <= lo) hi = lo + 1.0;

        const QColor cc = colors[c % 6];
        p.setPen(QPen(cc, 2));
        auto toY = [&](double v) {
            const double w = lg ? std::log10(v) : v;
            return plot.bottom() - plot.height() * (w - lo) / (hi - lo) * 0.92;
        };
        if (col.size() > int(plot.width())) {
            // 点数が横方向の画素数より多い: 1 画素ごとの最小/最大を縦線で描く。
            // 単純に間引くと振動波形の山谷が消えて「小さくなった」ように
            // 見えるので、包絡線として正しく残す
            const int px = qMax(1, int(plot.width()));
            int i = 0;
            for (int b = 0; b < px; ++b) {
                const int end = int(qint64(col.size()) * (b + 1) / px);
                if (end <= i) continue;
                double vlo = col[i], vhi = col[i];
                for (int k = i; k < end; ++k) {
                    vlo = std::min(vlo, col[k]);
                    vhi = std::max(vhi, col[k]);
                }
                const double x = plot.left() + plot.width() * b / double(px);
                p.drawLine(QPointF(x, toY(vlo)), QPointF(x, toY(vhi)));
                i = end;
            }
        } else {
            QPainterPath path;
            for (int i = 0; i < col.size(); ++i) {
                const double x = plot.left()
                    + plot.width() * (t.x[i] - xmin) / (xmax - xmin);
                if (i == 0) path.moveTo(x, toY(col[i]));
                else        path.lineTo(x, toY(col[i]));
            }
            p.drawPath(path);
        }
        p.drawText(QPointF(plot.right() - 250, legendY),
                   QStringLiteral("%1: %2 … %3%4")
                       .arg(t.yNames.value(c))
                       .arg(QString::number(lg ? std::pow(10.0, lo) : lo,
                                            'g', 4))
                       .arg(QString::number(lg ? std::pow(10.0, hi) : hi,
                                            'g', 4))
                       .arg(lg ? QStringLiteral(" (log)") : QString()));
        legendY += 15;
    }

    p.setPen(palette().text().color());
    QString axis = QStringLiteral("%1: %2 … %3   (%4 点)")
                       .arg(t.xName,
                            QString::number(xmin, 'g', 4),
                            QString::number(xmax, 'g', 4))
                       .arg(t.x.size());
    // 読み込み時に間引いたなら黙って隠さない (元の行数を必ず出す)
    if (t.decimated())
        axis += QStringLiteral("  ") + I18n::tr("pp_post_decimated")
                                           .arg(t.totalRows).arg(t.x.size());
    p.drawText(QPointF(plot.left(), plot.bottom() + 16), axis);
    p.drawText(QPointF(plot.left(), plot.bottom() + 32),
               I18n::tr("pp_post_hint"));
}

void PlotPanel::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), palette().base());

    // モードボタン行 (子ウィジェット) の下から描く — タイトル文字が
    // ボタンと重ならないよう、ボタン行の実高さぶんプロットを下げる
    const int hdr = m_btnConv->geometry().bottom() + 6;
    const QRectF plot(56, hdr + 22, width() - 76, height() - hdr - 58);
    const qreal titleY = plot.top() - 8;
    const QColor accent(accentColor(m_domain));

    p.setPen(QPen(palette().mid().color(), 1));
    p.drawRect(plot);

    // grid — スミスチャートは自前の円目盛を持つので直交格子は描かない
    const bool smith = (m_mode == Smith && hasFreqChar());
    if (!smith) {
        p.setPen(QPen(palette().midlight().color(), 1, Qt::DotLine));
        for (int i = 1; i < 10; ++i) {
            const double x = plot.left() + plot.width() * i / 10.0;
            p.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        }
        for (int i = 1; i < 5; ++i) {
            const double y = plot.top() + plot.height() * i / 5.0;
            p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        }
    }

    p.setPen(palette().text().color());

    if (m_mode == FreqChar && hasFreqChar()) {
        paintFreqChar(p, plot, accent);
        return;
    }
    if (smith) {
        paintSmith(p, plot, accent);
        return;
    }
    if (m_mode == Pattern && hasFarPattern()) {
        paintFarPattern(p, plot, accent);
        return;
    }
    if (m_mode == PostLog && hasPostTables()) {
        paintPostTable(p, plot);
        return;
    }

    if (m_mode == Waveform) {
        p.drawText(QPointF(plot.left(), titleY), I18n::tr("pp_waveform"));

        // gaussian pulse exactly as the kernel computes it:
        //   v(t) = exp(-((t - 4σ)/σ)²·π) 系の正規化パルス。
        //   Tw (pulsewidth) 未指定時はカーネル既定 (周波数帯域から自動)。
        const GeneralOpts &g = m_project->general();
        double dt = g.dt > 0 ? g.dt : m_project->courantDt();
        if (dt <= 0) dt = 1e-12;
        const double tw = g.tw > 0 ? g.tw
                          : (g.f1max > 0 ? 1.27 / g.f1max : 100 * dt);
        const int N = qMax(64, qMin(2048, int(4 * tw / dt)));

        QPainterPath path;
        for (int i = 0; i <= N; ++i) {
            const double t = 4.0 * tw * i / N;
            const double arg = (t - 2.0 * tw) / (tw / 2.0);
            const double v = std::exp(-arg * arg);
            const double x = plot.left() + plot.width() * i / double(N);
            const double y = plot.bottom() - plot.height() * v * 0.92;
            if (i == 0) path.moveTo(x, y); else path.lineTo(x, y);
        }
        p.setPen(QPen(accent, 2));
        p.drawPath(path);

        p.setPen(palette().text().color());
        p.drawText(QPointF(plot.left(), plot.bottom() + 16),
                   QStringLiteral("t: 0 … %1 s   (Δt=%2 s, Tw=%3 s)")
                       .arg(QString::number(4 * tw, 'g', 3),
                            QString::number(dt, 'g', 3),
                            QString::number(tw, 'g', 3)));

        // 室内音響では「スピーカー = 音声ファイルの波形」ではないことを明示
        if (m_domain == Domain::Acoustic) {
            QFont f = p.font();
            const QFont keep = f;
            // QSS 適用時はポイントではなくピクセル指定のことがある
            if (f.pointSizeF() > 0)
                f.setPointSizeF(f.pointSizeF() * 0.85);
            else if (f.pixelSize() > 0)
                f.setPixelSize(qMax(1, int(f.pixelSize() * 0.85)));
            p.setFont(f);
            QRectF noteRect(plot.left() + 8, plot.top() + 6,
                            plot.width() - 16, 60);
            p.drawText(noteRect, Qt::TextWordWrap,
                       I18n::tr("ppb_wave_ac_note"));
            p.setFont(keep);
        }
    } else {
        p.drawText(QPointF(plot.left(), titleY), I18n::tr("pp_convergence"));

        // 室内音響: ⟨p⟩/⟨v⟩ は ofd (電磁 FDTD) の波動アナロジー表示で、
        // 定量的な音響量ではないことを明示する (ADR-0004 — 絶対規則 5)。
        // データ有無に関わらず描く (実行前でも何が得られるかを示す)。
        if (m_domain == Domain::Acoustic) {
            QFont f = p.font();
            const QFont keep = f;
            // QSS 適用時はポイントではなくピクセル指定のことがある
            if (f.pointSizeF() > 0)
                f.setPointSizeF(f.pointSizeF() * 0.85);
            else if (f.pixelSize() > 0)
                f.setPixelSize(qMax(1, int(f.pixelSize() * 0.85)));
            p.setFont(f);
            QRectF noteRect(plot.left() + 8, plot.top() + 6,
                            plot.width() - 16, 60);
            p.drawText(noteRect, Qt::TextWordWrap,
                       I18n::tr("ppb_conv_ac_note"));
            p.setFont(keep);
        }
        if (m_steps.isEmpty()) {
            p.drawText(plot, Qt::AlignCenter,
                       QStringLiteral("no data — run the solver"));
            return;
        }

        double vmax = 1e-300;
        for (double v : m_eAvg) vmax = std::max(vmax, v);
        for (double v : m_hAvg) vmax = std::max(vmax, v);
        const int smax = qMax(1, m_steps.last());

        auto drawSeries = [&](const QVector<double> &v, const QColor &c) {
            QPainterPath path;
            for (int i = 0; i < v.size(); ++i) {
                const double x = plot.left() + plot.width() * m_steps[i] / double(smax);
                const double y = plot.bottom() - plot.height() * (v[i] / vmax) * 0.92;
                if (i == 0) path.moveTo(x, y); else path.lineTo(x, y);
            }
            p.setPen(QPen(c, 2));
            p.drawPath(path);
        };
        drawSeries(m_eAvg, accent);
        drawSeries(m_hAvg, QColor("#888888"));

        // 凡例: 音響/水中は音圧/粒子速度 (p/v)、それ以外は電磁界 (E/H)
        const bool acoustic = (m_domain == Domain::Acoustic
                               || m_domain == Domain::Underwater);
        p.setPen(accent);
        p.drawText(QPointF(plot.right() - 110, plot.top() + 16),
                   acoustic ? QStringLiteral("⟨p⟩") : QStringLiteral("⟨E⟩"));
        p.setPen(QColor("#888888"));
        p.drawText(QPointF(plot.right() - 70, plot.top() + 16),
                   acoustic ? QStringLiteral("⟨v⟩") : QStringLiteral("⟨H⟩"));
        p.setPen(palette().text().color());
        p.drawText(QPointF(plot.left(), plot.bottom() + 16),
                   QStringLiteral("step: 0 … %1").arg(smax));
    }
}
