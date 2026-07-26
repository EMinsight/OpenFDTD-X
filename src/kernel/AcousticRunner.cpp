// AcousticRunner.cpp
#include "AcousticRunner.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>

using namespace ofd;

AcousticRunner::AcousticRunner(QObject *parent) : QObject(parent) {}
AcousticRunner::~AcousticRunner() { if (m_proc) m_proc->kill(); }

bool AcousticRunner::isRunning() const {
    return m_proc && m_proc->state() != QProcess::NotRunning;
}

// HOME / kernel/ / PATH 探索で使う既定バイナリ名。実ソルバーの正式名は
// 未確定 (ADR-0007 未決事項) のため暫定 — CI・開発では
// $OFDX_ACOUSTIC_SOLVER または cfg.executable の直接指定が優先される。
static QString solverBaseName(AcousticBackend backend) {
    return (backend == AcousticBackend::ExternalGeometric)
               ? QStringLiteral("ofdx_acoustic_geom")
               : QStringLiteral("ofdx_acoustic_fdtd");
}

QString AcousticRunner::resolveSolver(const AcousticRunConfig &cfg)
{
    // ⓪ 明示指定 (`.ofdx` solver.executable / UI 入力) は探索より優先。
    //    指定が実在しない場合は PATH 等へフォールバックしない (誤った
    //    バイナリを黙って使わないため)。
    if (!cfg.executable.isEmpty())
        return QFileInfo::exists(cfg.executable) ? cfg.executable : QString();

    // ① $OFDX_ACOUSTIC_SOLVER: 絶対パス直接指定 (CI/開発オーバーライド)
    const QString envSolver = qEnvironmentVariable("OFDX_ACOUSTIC_SOLVER");
    if (!envSolver.isEmpty())
        return QFileInfo::exists(envSolver) ? envSolver : QString();

    QString base = solverBaseName(cfg.backend);
#ifdef Q_OS_WIN
    base += ".exe";
#endif
    // ② $OPENFDTD_ACOUSTICS_HOME 配下 → ③ アプリ実行ディレクトリ kernel/
    const QString dirs[] = {
        qEnvironmentVariable("OPENFDTD_ACOUSTICS_HOME"),
        QCoreApplication::applicationDirPath() + "/kernel",
        QCoreApplication::applicationDirPath(),
    };
    for (const QString &d : dirs) {
        if (d.isEmpty()) continue;
        const QString full = QDir(d).absoluteFilePath(base);
        if (QFileInfo::exists(full)) return full;
    }
    // ④ PATH (実在確認込み — 見つからなければ空を返す)
    return QStandardPaths::findExecutable(base);
}

void AcousticRunner::fail(const QString &reason)
{
    emit logLine("error: " + reason);
    emit finished(false);
}

void AcousticRunner::start(const AcousticRunConfig &cfg)
{
    if (isRunning()) return;
    m_cfg = cfg;

    // 外部プロセスを起動する backend は ExternalFDTD / ExternalGeometric のみ
    if (m_cfg.backend != AcousticBackend::ExternalFDTD &&
        m_cfg.backend != AcousticBackend::ExternalGeometric) {
        fail(QStringLiteral("backend does not launch an external solver "
                            "(only ExternalFDTD / ExternalGeometric do)"));
        return;
    }

    if (m_cfg.workingDir.isEmpty()) {
        m_cfg.workingDir =
            QStandardPaths::writableLocation(QStandardPaths::TempLocation)
            + "/openfdtd-x-acoustics";
    }
    QDir().mkpath(m_cfg.workingDir);

    const QString solver = resolveSolver(m_cfg);
    if (solver.isEmpty()) {
        fail(QStringLiteral("acoustic solver not found (searched: explicit "
                            "executable, $OFDX_ACOUSTIC_SOLVER, "
                            "$OPENFDTD_ACOUSTICS_HOME, app dir kernel/, "
                            "PATH)"));
        return;
    }

    QString program;
    QStringList args;
    if (m_cfg.processes > 1) {
        program = "mpiexec";
        args << "-n" << QString::number(m_cfg.processes) << solver;
    } else {
        program = solver;
    }
    args << QDir(m_cfg.workingDir).absolutePath();
    if (!m_cfg.inputFile.isEmpty()) args << m_cfg.inputFile;

    m_proc = new QProcess(this);
    m_proc->setWorkingDirectory(m_cfg.workingDir);
    m_proc->setProcessChannelMode(QProcess::MergedChannels);

    auto env = QProcessEnvironment::systemEnvironment();
    env.insert("OMP_NUM_THREADS", QString::number(m_cfg.threads));
    m_proc->setProcessEnvironment(env);

    connect(m_proc, &QProcess::readyRead, this, &AcousticRunner::onReadyRead);
    connect(m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &AcousticRunner::onFinished);
    connect(m_proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        emit logLine("error: " + m_proc->errorString()
                     + " (" + m_proc->program() + ")");
        // FailedToStart では QProcess::finished が来ないため自前で閉じる
        if (m_proc->state() == QProcess::NotRunning) {
            m_proc->deleteLater();
            m_proc = nullptr;
            emit finished(false);
        }
    });

    emit logLine(QStringLiteral("$ cd %1").arg(m_cfg.workingDir));
    emit logLine(QStringLiteral("$ %1 %2").arg(program, args.join(' ')));
    m_proc->start(program, args);
    emit started();
}

void AcousticRunner::stop()
{
    if (!isRunning()) return;
    m_proc->terminate();
    if (!m_proc->waitForFinished(2000))
        m_proc->kill();
}

void AcousticRunner::onReadyRead()
{
    if (!m_proc) return;
    // 進捗行: "progress <step>/<total>" (ADR-0007 — モックが参照実装)
    static const QRegularExpression progressRe(
        "^progress\\s+(\\d+)\\s*/\\s*(\\d+)$");
    while (m_proc->canReadLine()) {
        const QString line = QString::fromUtf8(m_proc->readLine())
                                 .remove('\r').trimmed();
        if (line.isEmpty()) continue;
        emit logLine(line);
        const auto m = progressRe.match(line);
        if (m.hasMatch())
            emit progress(m.captured(1).toInt(),
                          qMax(1, m.captured(2).toInt()));
    }
}

void AcousticRunner::onFinished(int exitCode, QProcess::ExitStatus status)
{
    const bool ok = (status == QProcess::NormalExit && exitCode == 0);
    m_proc->deleteLater();
    m_proc = nullptr;

    if (!ok) {
        emit logLine(QStringLiteral("=== failed (exit %1) ===").arg(exitCode));
        emit finished(false);
        return;
    }

    // 出力契約の検証 (ADR-0007 Decision 4)。違反時は部分出力を採用しない。
    const QDir wd(m_cfg.workingDir);
    const QString rirName = m_cfg.outputRirFile.isEmpty()
        ? QStringLiteral("rir.wav") : m_cfg.outputRirFile;
    const QString rirPath = wd.absoluteFilePath(rirName);
    if (!QFileInfo::exists(rirPath)) {
        fail(QStringLiteral("contract violation: solver exited 0 but %1 "
                            "was not produced in %2")
                 .arg(rirName, m_cfg.workingDir));
        return;
    }
    const QString metaPath = wd.absoluteFilePath(QStringLiteral("metadata.json"));
    if (!QFileInfo::exists(metaPath)) {
        fail(QStringLiteral("contract violation: metadata.json was not "
                            "produced in %1").arg(m_cfg.workingDir));
        return;
    }

    emit rirReady(rirPath);
    emit logLine(QStringLiteral("=== normal end ==="));
    emit finished(true);
}
