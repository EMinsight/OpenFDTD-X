// PlotPanel.h — 2D result/preview plot (QPainter, no QtCharts dependency).
//
// Four data sources, all honest:
//   - source waveform preview: gaussian pulse computed from the project's
//     Δt/Tw settings (what the kernel will inject)
//   - convergence history: "<step> <Eavg> <Havg>" lines parsed live from the
//     running kernel's stdout (Runner::logLine → addConvergencePoint)
//   - feed frequency response: the "feed #N" impedance table parsed from the
//     finished run's <kernel>.log (io/KernelResultReader)
//   - far-field pattern: far1d.log parsed from the finished run
// モード切替は左上のボタン列。実行結果系のモードはデータが届いた実行後に
// だけ有効になる (残存ファイルの再表示をしない — 呼び出し側で mtime ゲート)。
#pragma once
#include <QWidget>
#include <QVector>
#include <QPointF>
#include "../core/Domain.h"
#include "../io/KernelResultReader.h"

class QToolButton;

namespace ofd {

class Project;

class PlotPanel : public QWidget {
    Q_OBJECT
public:
    explicit PlotPanel(Project *project, QWidget *parent = nullptr);

    void setDomain(Domain d) { m_domain = d; update(); }

    // 実行結果の反映 (onRunnerFinished から)。空ならそのモードは無効のまま
    void setRunResults(const QVector<FeedSweep> &sweeps,
                       const QVector<FarPattern> &patterns);
    void clearRunResults();
    bool hasFreqChar() const { return !m_sweeps.isEmpty(); }
    bool hasFarPattern() const { return !m_patterns.isEmpty(); }

public slots:
    void showWaveform();
    void showConvergence();
    void showFreqChar();
    void showFarPattern();
    void clearConvergence();
    void addConvergencePoint(int step, double e, double h);
    bool exportCsv(const QString &path) const;

    // Live convergence history (for HDF5 export etc.)
    const QVector<int>    &steps() const { return m_steps; }
    const QVector<double> &eAvg()  const { return m_eAvg; }
    const QVector<double> &hAvg()  const { return m_hAvg; }
    bool hasConvergence() const { return !m_steps.isEmpty(); }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    // Pattern は構造体 FarPattern との名前衝突を避けた列挙名
    enum Mode { Waveform, Convergence, FreqChar, Pattern };

    void setMode(Mode m);
    void updateModeButtons();
    void saveCsvDialog();   // 右上 CSV ボタン → exportCsv
    void savePngDialog();   // 右上 PNG ボタン → grab() (ボタンは一時非表示)
    void paintFreqChar(QPainter &p, const QRectF &plot, const QColor &accent);
    void paintFarPattern(QPainter &p, const QRectF &plot,
                         const QColor &accent);

    Project *m_project;
    Domain   m_domain = Domain::EM;
    Mode     m_mode = Waveform;

    QToolButton *m_csvBtn = nullptr;
    QToolButton *m_pngBtn = nullptr;
    QToolButton *m_btnWave = nullptr, *m_btnConv = nullptr,
                *m_btnFreq = nullptr, *m_btnFar = nullptr;

    QVector<int>    m_steps;
    QVector<double> m_eAvg, m_hAvg;
    QVector<FeedSweep>  m_sweeps;
    QVector<FarPattern> m_patterns;
};

} // namespace ofd
