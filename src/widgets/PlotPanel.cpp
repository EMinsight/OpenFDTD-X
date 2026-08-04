// PlotPanel.cpp
#include "PlotPanel.h"
#include "../core/Project.h"
#include "../I18n.h"

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
    ofd::I18n::reg("ppb_mode_far", "放射パターン", "Pattern");
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
    m_btnFreq = modeBtn("ppb_mode_freq");
    m_btnFar  = modeBtn("ppb_mode_far");
    btnRow->addWidget(m_btnWave);
    btnRow->addWidget(m_btnConv);
    btnRow->addWidget(m_btnFreq);
    btnRow->addWidget(m_btnFar);
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
    connect(m_btnFar, &QToolButton::clicked, this, &PlotPanel::showFarPattern);
    connect(m_csvBtn, &QToolButton::clicked, this, &PlotPanel::saveCsvDialog);
    connect(m_pngBtn, &QToolButton::clicked, this, &PlotPanel::savePngDialog);
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

void PlotPanel::showFarPattern()
{
    if (hasFarPattern()) setMode(Pattern);
    else updateModeButtons();
}

void PlotPanel::updateModeButtons()
{
    m_btnWave->setChecked(m_mode == Waveform);
    m_btnConv->setChecked(m_mode == Convergence);
    m_btnFreq->setChecked(m_mode == FreqChar);
    m_btnFar->setChecked(m_mode == Pattern);
    // 結果系モードはデータが届くまで無効 (未実装ではなく「まだ結果が無い」)
    m_btnFreq->setEnabled(hasFreqChar());
    m_btnFreq->setToolTip(hasFreqChar() ? QString()
                                        : I18n::tr("ppb_freq_none_tip"));
    m_btnFar->setEnabled(hasFarPattern());
    m_btnFar->setToolTip(hasFarPattern() ? QString()
                                         : I18n::tr("ppb_far_none_tip"));
}

void PlotPanel::setRunResults(const QVector<FeedSweep> &sweeps,
                              const QVector<FarPattern> &patterns)
{
    m_sweeps = sweeps;
    m_patterns = patterns;
    // 新しい結果が届いたら周波数特性を前面に (無ければパターン)
    if (hasFreqChar()) m_mode = FreqChar;
    else if (hasFarPattern()) m_mode = Pattern;
    updateModeButtons();
    update();
}

void PlotPanel::clearRunResults()
{
    m_sweeps.clear();
    m_patterns.clear();
    if (m_mode == FreqChar || m_mode == Pattern)
        m_mode = Convergence;    // 実行中は収束を見せる
    updateModeButtons();
    update();
}

void PlotPanel::saveCsvDialog()
{
    const char *suggest =
        m_mode == FreqChar ? "feed_response.csv" :
        m_mode == Pattern ? "far_pattern.csv" : "convergence.csv";
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
    m_csvBtn->setVisible(false);
    m_pngBtn->setVisible(false);
    grab().save(path);
    m_csvBtn->setVisible(true);
    m_pngBtn->setVisible(true);
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
    } else if (m_mode == Pattern && !m_patterns.isEmpty()) {
        out << "plane,frequency_Hz,deg,Eabs_dB\n";
        for (const FarPattern &pat : m_patterns)
            for (int i = 0; i < pat.deg.size(); ++i)
                out << pat.plane << ',' << pat.freqHz << ',' << pat.deg[i]
                    << ',' << pat.eAbsDb[i] << '\n';
    } else {
        out << "step,Eavg,Havg\n";
        for (int i = 0; i < m_steps.size(); ++i)
            out << m_steps[i] << ',' << m_eAvg[i] << ',' << m_hAvg[i] << '\n';
    }
    return true;
}

// ── 給電点特性 (Rin / Xin / Ref[dB]) ────────────────────────────────────────
void PlotPanel::paintFreqChar(QPainter &p, const QRectF &plot,
                              const QColor &accent)
{
    p.drawText(QPointF(plot.left(), 18), I18n::tr("pp_freqchar"));
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

// ── 遠方界パターン (面ごとの E-abs[dB]) ─────────────────────────────────────
void PlotPanel::paintFarPattern(QPainter &p, const QRectF &plot,
                                const QColor &accent)
{
    p.drawText(QPointF(plot.left(), 18), I18n::tr("pp_farpattern"));
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

void PlotPanel::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), palette().base());

    const QRectF plot(56, 28, width() - 76, height() - 64);
    const QColor accent(accentColor(m_domain));

    p.setPen(QPen(palette().mid().color(), 1));
    p.drawRect(plot);

    // grid
    p.setPen(QPen(palette().midlight().color(), 1, Qt::DotLine));
    for (int i = 1; i < 10; ++i) {
        const double x = plot.left() + plot.width() * i / 10.0;
        p.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
    }
    for (int i = 1; i < 5; ++i) {
        const double y = plot.top() + plot.height() * i / 5.0;
        p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
    }

    p.setPen(palette().text().color());

    if (m_mode == FreqChar && hasFreqChar()) {
        paintFreqChar(p, plot, accent);
        return;
    }
    if (m_mode == Pattern && hasFarPattern()) {
        paintFarPattern(p, plot, accent);
        return;
    }

    if (m_mode == Waveform) {
        p.drawText(QPointF(plot.left(), 18), I18n::tr("pp_waveform"));

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
    } else {
        p.drawText(QPointF(plot.left(), 18), I18n::tr("pp_convergence"));
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

        p.setPen(accent);
        p.drawText(QPointF(plot.right() - 110, plot.top() + 16), "⟨E⟩");
        p.setPen(QColor("#888888"));
        p.drawText(QPointF(plot.right() - 70, plot.top() + 16), "⟨H⟩");
        p.setPen(palette().text().color());
        p.drawText(QPointF(plot.left(), plot.bottom() + 16),
                   QStringLiteral("step: 0 … %1").arg(smax));
    }
}
