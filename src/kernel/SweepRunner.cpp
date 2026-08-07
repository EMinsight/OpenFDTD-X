// SweepRunner.cpp
#include "SweepRunner.h"
#include "../core/Project.h"
#include "../io/OfdIO.h"
#include "../core/MeshAxis.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

#include <cmath>

using namespace ofd;

SweepRunner::SweepRunner(QObject *parent) : QObject(parent)
{
    m_runner = new Runner(this);
    connect(m_runner, &Runner::logLine, this, &SweepRunner::logLine);
    connect(m_runner, &Runner::finished, this, &SweepRunner::onPointFinished);
}

QVector<double> SweepRunner::plan(const SweepConfig &cfg)
{
    QVector<double> v;
    // 明示指定があればそれが正 (収束テストの不等間隔な倍率列など)。
    // 1 点しか無い列はスイープとして成立しないので空にする。
    if (!cfg.values.isEmpty())
        return cfg.values.size() >= 2 ? cfg.values : v;
    // 1 点は通常実行と同じ。範囲が 0 幅なら全点が同じ値になり意味が無い。
    if (cfg.points < 2 || cfg.from == cfg.to) return v;
    v.reserve(cfg.points);
    for (int i = 0; i < cfg.points; ++i)
        v.push_back(cfg.from + (cfg.to - cfg.from) * i / double(cfg.points - 1));
    return v;
}

void SweepRunner::applyPoint(Project &p, SweepKind kind, double value)
{
    if (kind == SweepKind::MeshRefine) {
        // 各区間の分割数を倍率で丸める。1 未満にはしない (0 分割は不正な
        // メッシュになり、カーネルが読めない)。倍率が小さいと分割数が
        // 1 で飽和する区間が出るが、それは「これ以上粗くできない」という
        // 事実で、こちらで区間を統合したりはしない (形状が変わるため)。
        for (int a = 0; a < 3; ++a) {
            MeshAxis &m = p.mesh(a);
            for (int i = 0; i < m.divs.size(); ++i)
                m.divs[i] = qMax(1, int(std::lround(m.divs[i] * value)));
        }
        return;
    }
    PlaneWave &pw = p.planewave();
    // スイープは平面波入射の解析。無効のままでは .ofd に planewave 行が
    // 出ず、全点が同じ (平面波なしの) 計算になってしまう。
    pw.enabled = true;
    if (kind == SweepKind::PlaneWaveTheta) pw.theta = value;
    else                                   pw.phi   = value;
}

QString SweepRunner::pointDirName(int index)
{
    return QStringLiteral("sweep_%1").arg(index, 3, 10, QLatin1Char('0'));
}

QString SweepRunner::pointLabel(SweepKind kind, double value)
{
    if (kind == SweepKind::MeshRefine)
        return QStringLiteral("×%1").arg(value, 0, 'f', 3);
    return QStringLiteral("%1 = %2°")
        .arg(kind == SweepKind::PlaneWaveTheta ? QStringLiteral("θ")
                                               : QStringLiteral("φ"))
        .arg(value, 0, 'g', 6);
}

bool SweepRunner::refDbNear(const QVector<FeedSweep> &feeds, double freqHz,
                            double *refDb)
{
    const FeedSweepPoint *best = nullptr;
    double bestDf = 0.0;
    for (const FeedSweep &f : feeds) {
        for (const FeedSweepPoint &pt : f.points) {
            const double df = std::fabs(pt.freqHz - freqHz);
            if (!best || df < bestDf) { best = &pt; bestDf = df; }
        }
    }
    if (!best) return false;
    if (refDb) *refDb = best->refDb;
    return true;
}

SweepResult SweepRunner::collect(const QString &dir, Kernel kernel,
                                 SweepKind kind, double value, bool ok)
{
    SweepResult r;
    r.value = value;
    r.label = pointLabel(kind, value);
    r.dir = dir;
    r.ok = ok;
    if (!ok) return r;

    const QDir d(dir);
    const QString logName =
        kernel == Kernel::RCWA ? QStringLiteral("orcwa.log") :
        kernel == Kernel::BPM  ? QStringLiteral("obpm.log")
                               : QStringLiteral("ofd.log");
    r.feeds = KernelResultReader::readFeedSweeps(d.filePath(logName));
    r.patterns = KernelResultReader::readFar1d(d.filePath(
        QStringLiteral("far1d.log")));

    // 代表値 = 全パターンを通じた E-abs の最大 [dB]
    for (const FarPattern &fp : r.patterns) {
        for (const double v : fp.eAbsDb) {
            if (!std::isfinite(v)) continue;
            if (!r.hasPeak || v > r.peakEAbs_dB) {
                r.peakEAbs_dB = v;
                r.hasPeak = true;
            }
        }
    }
    return r;
}

QString SweepRunner::toCsv(const QVector<SweepResult> &results)
{
    // 代表値が無い点も行として残す (「走ったが遠方界が無い」ことが分かる)
    // 列名は「振った値」— 角度とは限らない (収束テストは倍率)
    QString s = QStringLiteral("value,label,status,peak_eabs_db,dir\n");
    for (const SweepResult &r : results) {
        s += QStringLiteral("%1,%2,%3,%4,%5\n")
                 .arg(r.value, 0, 'g', 10)
                 .arg(r.label,
                      r.ok ? QStringLiteral("ok") : QStringLiteral("failed"),
                      r.hasPeak ? QString::number(r.peakEAbs_dB, 'g', 10)
                                : QString(),
                      QFileInfo(r.dir).fileName());
    }
    return s;
}

bool SweepRunner::start(const Project &base, const SweepConfig &cfg)
{
    if (m_running) return false;
    m_cfg = cfg;
    m_values = plan(cfg);
    if (m_values.isEmpty()) {
        emit logLine(QStringLiteral(
            "sweep: need at least 2 points over a non-zero range"));
        return false;
    }

    // 親ディレクトリ。指定が無ければ通常実行と同じ場所の下に sweep/ を掘る。
    QString root = cfg.baseDir;
    if (root.isEmpty()) {
        const QString wd = Runner::resolveWorkingDir(&base, cfg.run);
        if (wd.isEmpty()) {
            emit logLine(QStringLiteral("sweep: no working directory"));
            return false;
        }
        root = QDir(wd).filePath(QStringLiteral("sweep"));
    }
    root = QDir(root).absolutePath();
    if (!QDir().mkpath(root)) {
        emit logLine(QStringLiteral("sweep: cannot create %1").arg(root));
        return false;
    }

    // 走らせる元を 1 度だけ書き出す。各点はこれを読み直してから 1 値だけ
    // 差し替える (前の点の変更が積み上がらない)。base 自身は変更しない
    // ので const 参照のまま OfdIO へ渡す。
    const QString baseName = base.filePath().isEmpty()
        ? QStringLiteral("project")
        : QFileInfo(base.filePath()).completeBaseName();
    const QString baseDir = QDir(root).filePath(QStringLiteral("base"));
    QDir().mkpath(baseDir);
    const QString basePath = QDir(baseDir).filePath(baseName + ".ofd");
    QString err;
    if (!OfdIO::save(basePath, base, &err)) {
        emit logLine(QStringLiteral("sweep: cannot write %1: %2")
                         .arg(basePath, err));
        return false;
    }
    OfdxIO::save(QDir(baseDir).filePath(baseName + ".ofdx"), base, nullptr);

    m_cfg.baseDir = root;
    m_results.clear();
    m_results.reserve(m_values.size());
    m_index = -1;
    m_allOk = true;
    m_running = true;
    delete m_work;
    m_work = new Project(this);
    emit logLine(QStringLiteral("sweep: %1 points, %2 → %3, under %4")
                     .arg(m_values.size())
                     .arg(pointLabel(m_cfg.kind, m_values.first()),
                          pointLabel(m_cfg.kind, m_values.last()), root));
    launchNext();
    return true;
}

void SweepRunner::launchNext()
{
    ++m_index;
    if (m_index >= m_values.size()) {
        m_running = false;
        emit finished(m_allOk);
        return;
    }

    const double value = m_values[m_index];
    const QString label = pointLabel(m_cfg.kind, value);
    emit pointStarted(m_index, m_values.size(), label);

    // 元を読み直してから 1 値だけ差し替える
    const QString baseDir = QDir(m_cfg.baseDir).filePath(
        QStringLiteral("base"));
    const QStringList ofds =
        QDir(baseDir).entryList({ QStringLiteral("*.ofd") }, QDir::Files);
    QString err;
    if (ofds.isEmpty()
        || !m_work->load(QDir(baseDir).filePath(ofds.first()), &err)) {
        emit logLine(QStringLiteral("sweep: cannot reload the base: %1")
                         .arg(err));
        m_allOk = false;
        m_running = false;
        emit finished(false);
        return;
    }
    applyPoint(*m_work, m_cfg.kind, value);

    RunConfig rc = m_cfg.run;
    rc.workingDir = QDir(m_cfg.baseDir).filePath(pointDirName(m_index));
    QDir().mkpath(rc.workingDir);
    m_runner->start(m_work, rc);
}

void SweepRunner::onPointFinished(bool ok)
{
    if (!m_running) return;   // stop() 後の取りこぼしを無視する
    if (!ok) m_allOk = false;

    const SweepResult r = collect(m_runner->workingDir(), m_cfg.run.kernel,
                                  m_cfg.kind, m_values[m_index], ok);
    m_results.push_back(r);
    emit pointFinished(m_index, r);
    launchNext();
}

void SweepRunner::stop()
{
    if (!m_running) return;
    m_running = false;
    m_allOk = false;
    m_runner->stop();
    emit finished(false);
}
