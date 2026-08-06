// AcousticRunner.cpp
#include "AcousticRunner.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSettings>
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
// バックエンド → 実行ファイル名の候補 (先頭が本命)。
// 幾何音響は OpenAcoustics の実装名が `ofdx_acoustic_ga`。旧称
// `ofdx_acoustic_geom` は該当バイナリが存在した実績が無いが、既にその名前で
// 配置している環境を壊さないよう候補として後ろに残す。
static QStringList solverBaseNames(AcousticBackend backend) {
    return (backend == AcousticBackend::ExternalGeometric)
               ? QStringList{ QStringLiteral("ofdx_acoustic_ga"),
                              QStringLiteral("ofdx_acoustic_geom") }
               : QStringList{ QStringLiteral("ofdx_acoustic_fdtd") };
}

// 外部音響ソルバーの既定パス (グローバル設定)。他カーネルの
// Runner::kernelDirSetting と同じ QSettings 領域に置く。
QString AcousticRunner::solverPathSetting()
{
    QSettings s(QSettings::UserScope,
                QStringLiteral("OpenFDTD"), QStringLiteral("Kernels"));
    return s.value(QStringLiteral("OFDX_ACOUSTIC_SOLVER")).toString();
}

void AcousticRunner::setSolverPathSetting(const QString &path)
{
    QSettings s(QSettings::UserScope,
                QStringLiteral("OpenFDTD"), QStringLiteral("Kernels"));
    if (path.isEmpty()) s.remove(QStringLiteral("OFDX_ACOUSTIC_SOLVER"));
    else                s.setValue(QStringLiteral("OFDX_ACOUSTIC_SOLVER"), path);
}

QString AcousticRunner::resolveSolver(const AcousticRunConfig &cfg)
{
    // ⓪ 明示指定 (`.ofdx` solver.executable / UI 入力) は探索より優先。
    //    指定が実在しない場合は PATH 等へフォールバックしない (誤った
    //    バイナリを黙って使わないため)。
    if (!cfg.executable.isEmpty())
        return QFileInfo::exists(cfg.executable) ? cfg.executable : QString();

    // ①' GUI の「カーネルパスの設定」で指定した既定パス。
    //     他のカーネル (Runner: binaryDir → GUI 設定 → 環境変数 → PATH) と
    //     並びを揃える。既定は空なので、未設定なら従来の探索と完全に同じ。
    const QString uiSolver = solverPathSetting();
    if (!uiSolver.isEmpty())
        return QFileInfo::exists(uiSolver) ? uiSolver : QString();

    // ① $OFDX_ACOUSTIC_SOLVER: 絶対パス直接指定 (CI/開発オーバーライド)
    const QString envSolver = qEnvironmentVariable("OFDX_ACOUSTIC_SOLVER");
    if (!envSolver.isEmpty())
        return QFileInfo::exists(envSolver) ? envSolver : QString();

    return resolveSolverByName(cfg.backend);
}

QString AcousticRunner::resolveSolverByName(AcousticBackend backend)
{
    // ② $OPENFDTD_ACOUSTICS_HOME 配下 → ③ アプリ実行ディレクトリ kernel/
    const QString dirs[] = {
        qEnvironmentVariable("OPENFDTD_ACOUSTICS_HOME"),
        QCoreApplication::applicationDirPath() + "/kernel",
        QCoreApplication::applicationDirPath(),
    };
    const QStringList bases = solverBaseNames(backend);
    for (const QString &b : bases) {
        QString base = b;
#ifdef Q_OS_WIN
        base += ".exe";
#endif
        for (const QString &d : dirs) {
            if (d.isEmpty()) continue;
            const QString full = QDir(d).absoluteFilePath(base);
            if (QFileInfo::exists(full)) return full;
        }
        // ④ PATH (実在確認込み)
        const QString onPath = QStandardPaths::findExecutable(base);
        if (!onPath.isEmpty()) return onPath;
    }
    return QString();
}

QString AcousticRunner::resolveSolverForHybrid(AcousticBackend backend)
{
    // ハイブリッド実行は 2 つのソルバーを続けて起動するので、バックエンドを
    // 区別しない上書き ($OFDX_ACOUSTIC_SOLVER / カーネルパス設定 /
    // solver.executable) をそのまま使うと**両方に同じバイナリ**を渡して
    // しまう。ファイル名がそのバックエンドの候補と一致するときだけ上書きを
    // 採用し、それ以外は名前による探索へ落とす。
    const QStringList bases = solverBaseNames(backend);
    const auto matches = [&bases](const QString &path) {
        if (path.isEmpty() || !QFileInfo::exists(path)) return false;
        const QString stem = QFileInfo(path).completeBaseName();
        for (const QString &b : bases)
            if (stem.compare(b, Qt::CaseInsensitive) == 0) return true;
        return false;
    };
    const QString overrides[] = { solverPathSetting(),
                                  qEnvironmentVariable("OFDX_ACOUSTIC_SOLVER") };
    for (const QString &o : overrides)
        if (matches(o)) return o;
    // 名前が一致しない上書き (= もう一方のソルバーを指している) でも、
    // **その隣**は探す。カーネルパス設定も $OFDX_ACOUSTIC_SOLVER も 1 個
    // しか持てないので、「2 本を同じ場所に置いて片方を指定した」が最も
    // 自然な設定になるため (どちらを指定してももう一方が見つかる)。
    for (const QString &o : overrides) {
        if (o.isEmpty() || !QFileInfo::exists(o)) continue;
        const QDir dir(QFileInfo(o).absolutePath());
        for (const QString &b : bases) {
            QString base = b;
#ifdef Q_OS_WIN
            base += ".exe";
#endif
            const QString full = dir.absoluteFilePath(base);
            if (QFileInfo::exists(full)) return full;
        }
    }
    return resolveSolverByName(backend);
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
