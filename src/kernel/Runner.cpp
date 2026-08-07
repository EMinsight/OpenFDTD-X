// Runner.cpp
#include "Runner.h"
#include "../io/BellhopIO.h"
#include "../core/Project.h"
#include "../io/OfdIO.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>

using namespace ofd;

Runner::Runner(QObject *parent) : QObject(parent) {}
Runner::~Runner() { if (m_proc) m_proc->kill(); }

bool Runner::isRunning() const {
    return m_proc && m_proc->state() != QProcess::NotRunning;
}

static QString kernelPrefix(Kernel k) {
    switch (k) {
        case Kernel::FDTD:    return "ofd";
        case Kernel::RCWA:    return "orcwa";
        case Kernel::BPM:     return "obpm";
        case Kernel::Bellhop: return "bellhopcxx";
        // 回路パラメータ抽出 (別リポジトリの姉妹ソルバ)
        case Kernel::PEEC:    return "peec";
        case Kernel::FEM:     return "ofe";
    }
    return "ofd";
}

QString Runner::solverBinary(const RunConfig &cfg) {
    QString base = kernelPrefix(cfg.kernel);
    if (cfg.kernel == Kernel::PEEC || cfg.kernel == Kernel::FEM) {
        // 回路抽出ソルバは CPU 単一実装 (MPI/GPU 変種なし)
        return resolveBinary(cfg, base);
    }
    if (cfg.kernel == Kernel::Bellhop) {
        // bellhopcuda のバイナリは bellhopcxx (CPU, 内部マルチスレッド) と
        // bellhopcuda (GPU) の 2 系統。MPI 変種は存在しないため CPU に倒す。
        if (cfg.engine == Engine::GPU || cfg.engine == Engine::GPU_MPI)
            base = "bellhopcuda";
        return resolveBinary(cfg, base);
    }
    switch (cfg.engine) {
        case Engine::CPU:     break;
        case Engine::CPU_MPI: base += "_mpi";      break;
        case Engine::GPU:     base += "_cuda";     break;
        case Engine::GPU_MPI: base += "_cuda_mpi"; break;
    }
    return resolveBinary(cfg, base);
}

QString Runner::postBinary(const RunConfig &cfg) {
    return resolveBinary(cfg, kernelPrefix(cfg.kernel) + "_post");
}

// カーネルの場所を指す環境変数名 (探索とエラーメッセージで共用)
const char *Runner::homeVarFor(Kernel k) {
    switch (k) {
        case Kernel::PEEC:    return "OPENPEEC_HOME";
        case Kernel::FEM:     return "OPENFEM_HOME";
        case Kernel::RCWA:    return "OPENRCWA_HOME";
        case Kernel::BPM:     return "OPENBPM_HOME";
        case Kernel::Bellhop: return "BELLHOPCUDA_HOME";
        case Kernel::FDTD:    break;
    }
    return "OPENFDTD_HOME";
}

// GUI で設定したカーネルディレクトリの永続化。環境変数が届かない起動経路
// (macOS の Finder / Dock 起動など) でもカーネルの場所を指定できるように、
// openfdtd_x / openuwa で共有する専用スコープに保存する。
QString Runner::kernelDirSetting(Kernel k)
{
    QSettings s(QSettings::UserScope,
                QStringLiteral("OpenFDTD"), QStringLiteral("Kernels"));
    return s.value(QLatin1String(homeVarFor(k))).toString();
}

void Runner::setKernelDirSetting(Kernel k, const QString &dir)
{
    QSettings s(QSettings::UserScope,
                QStringLiteral("OpenFDTD"), QStringLiteral("Kernels"));
    if (dir.isEmpty()) s.remove(QLatin1String(homeVarFor(k)));
    else               s.setValue(QLatin1String(homeVarFor(k)), dir);
}

QString Runner::resolveBinary(const RunConfig &cfg, const QString &name) {
    QString base = name;
#ifdef Q_OS_WIN
    base += ".exe";
#endif
    const QString dirs[] = {
        cfg.binaryDir,
        kernelDirSetting(cfg.kernel),
        qEnvironmentVariable(homeVarFor(cfg.kernel)),
        QCoreApplication::applicationDirPath() + "/kernel",
        QCoreApplication::applicationDirPath(),
    };
    for (const QString &d : dirs) {
        if (d.isEmpty()) continue;
        // ディレクトリ直下と bin/ の両方を探す — README は
        // OPENFDTD_HOME=/path/to/OpenFDTD (リポジトリルート) を案内しており、
        // 各カーネルのビルドはバイナリを bin/ に置くため。
        const QString full = QDir(d).absoluteFilePath(base);
        if (QFileInfo::exists(full)) return full;
        const QString inBin = QDir(d).absoluteFilePath("bin/" + base);
        if (QFileInfo::exists(inBin)) return inBin;
    }
    return base;   // let PATH resolve it
}

QString Runner::resolvedSolverPath(const RunConfig &cfg)
{
    const QString bin = solverBinary(cfg);
    if (QFileInfo::exists(bin)) return bin;
    // ディレクトリ探索で見つからず素の名前が返った場合は PATH を確認
    if (!bin.contains(QLatin1Char('/'))) {
        const QString onPath = QStandardPaths::findExecutable(bin);
        if (!onPath.isEmpty()) return onPath;
    }
    return QString();
}

Kernel Runner::kernelForProject(const Project &project)
{
    if (project.activeDomain() == Domain::Optical) {
        switch (project.optical().solver) {
            case OpticalSolver::RCWA: return Kernel::RCWA;
            case OpticalSolver::BPM:  return Kernel::BPM;
            // FMM は RCWA と同一手法 (Fourier Modal Method) の別名 —
            // OpenRCWA (orcwa) をカーネルとして実行する。
            case OpticalSolver::FMM:  return Kernel::RCWA;
            default:                  return Kernel::FDTD;
        }
    }
    // 水中音響は bellhopcxx (bellhopcuda リポジトリ) を起動する
    if (project.activeDomain() == Domain::Underwater)
        return Kernel::Bellhop;
    return Kernel::FDTD;
}

QString Runner::resolveWorkingDir(const Project *project, const RunConfig &cfg)
{
    if (!cfg.workingDir.isEmpty()) return cfg.workingDir;
    if (!project) return QString();
    return project->filePath().isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::TempLocation)
          + "/openfdtd-x"
        : QFileInfo(project->filePath()).path();
}

bool Runner::producesActivationCurve(const Project &project,
                                     const RunConfig &cfg)
{
    // activation_curve.csv は obpm (BPM ソルバー) が powersweep 指定時に
    // 書く。ポスト処理のみの実行 (obpm_post) は新しい CSV を作らない。
    if (cfg.kernel != Kernel::BPM) return false;
    if (cfg.mode == RunMode::Post) return false;
    if (project.activeDomain() != Domain::Optical) return false;
    if (project.optical().solver != OpticalSolver::BPM) return false;
    return project.optical().powerSweepEnabled;
}

void Runner::start(Project *project, const RunConfig &cfg)
{
    if (isRunning() || !project) return;
    m_cfg = cfg;

    // 相対パスで開いたプロジェクト (例: CLI 引数 tests/data/dipole.ofd) では
    // resolveWorkingDir が相対 "tests/data" を返す。そのまま使うと子プロセスの
    // 作業ディレクトリと入力パスの両方が相対になり、カーネル側から見た入力が
    // "tests/data/tests/data/dipole.ofd" に二重解決されて見つからない。
    // ここで絶対化して m_ofdPath 以降を全て絶対パスにする。
    m_cfg.workingDir = QDir(resolveWorkingDir(project, m_cfg)).absolutePath();
    QDir().mkpath(m_cfg.workingDir);

    const QString baseName = project->filePath().isEmpty()
        ? QStringLiteral("project")
        : QFileInfo(project->filePath()).completeBaseName();

    // 水中音響 (bellhopcxx): 入力は .ofd ではなく BELLHOP の .env。
    // ポスト段は存在しない (結果は .prt / .shd に直接出る)。
    if (m_cfg.kernel == Kernel::Bellhop) {
        if (m_cfg.mode == RunMode::Post) {
            emit logLine(QStringLiteral(
                "bellhopcxx: post-only mode is not applicable "
                "(results are written directly to .prt / .shd)"));
            emit finished(false);
            return;
        }
        m_ofdPath = QDir(m_cfg.workingDir).filePath(baseName + ".env");
        QFile env(m_ofdPath);
        if (!env.open(QIODevice::WriteOnly | QIODevice::Text)) {
            emit logLine("error: cannot write .env: " + env.errorString());
            emit finished(false);
            return;
        }
        env.write(BellhopIO::envText(*project).toUtf8());
        env.close();
        // 海底地形 (BTYFIL)。断面が無ければ書かず、前回実行の残骸も消す
        // (残っていると .env 側が 'A~' でなくても紛らわしいうえ、次回
        //  地形を外した実行で古い地形を拾う事故になる)。
        const QString btyPath =
            QDir(m_cfg.workingDir).filePath(baseName + ".bty");
        const QString bty = BellhopIO::btyText(*project);
        if (bty.isEmpty()) {
            QFile::remove(btyPath);
        } else {
            QFile f(btyPath);
            if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                emit logLine("error: cannot write .bty: " + f.errorString());
                emit finished(false);
                return;
            }
            f.write(bty.toUtf8());
            f.close();
            emit logLine(QStringLiteral("bathymetry: %1 (%2 points)")
                             .arg(QFileInfo(btyPath).fileName())
                             .arg(bty.split('\n').value(1)));
        }
        m_totalSteps = 1;
        m_postPending = false;
        launch(true);
        return;
    }

    m_ofdPath = QDir(m_cfg.workingDir).filePath(baseName + ".ofd");

    QString err;
    if (!project->save(m_ofdPath, &err)) {
        emit logLine("error: cannot write .ofd: " + err);
        emit finished(false);
        return;
    }

    m_totalSteps = qMax(1, project->general().maxiter);
    m_postPending = (m_cfg.mode == RunMode::Both);
    launch(m_cfg.mode != RunMode::Post);
}

void Runner::launch(bool solverPhase)
{
    QString program;
    QStringList args;

    if (solverPhase && m_cfg.kernel == Kernel::Bellhop) {
        // bellhopcxx <FILEROOT> — 引数は拡張子を除いたケース名のみ。
        // 作業ディレクトリで実行するので相対ベース名を渡す。
        program = solverBinary(m_cfg);
        args << QFileInfo(m_ofdPath).completeBaseName();
    } else if (solverPhase) {
        const QString bin = solverBinary(m_cfg);
        if (m_cfg.engine == Engine::CPU_MPI || m_cfg.engine == Engine::GPU_MPI) {
            program = "mpiexec";
            args << "-n" << QString::number(m_cfg.processes) << bin;
        } else {
            program = bin;
        }
        args << "-n" << QString::number(m_cfg.threads);
        args << m_ofdPath;
    } else {
        program = postBinary(m_cfg);
        args << "-n" << QString::number(m_cfg.threads);
        if (m_cfg.evHtml) args << "-html";
        args << m_ofdPath;
    }

    m_proc = new QProcess(this);
    m_proc->setWorkingDirectory(m_cfg.workingDir);
    m_proc->setProcessChannelMode(QProcess::MergedChannels);

    auto env = QProcessEnvironment::systemEnvironment();
    env.insert("OMP_NUM_THREADS", QString::number(m_cfg.threads));
    // GPU カーネル (ofd_cuda 等) 用のデバイス指定
    if (m_cfg.engine == Engine::GPU || m_cfg.engine == Engine::GPU_MPI)
        env.insert("CUDA_VISIBLE_DEVICES", QString::number(m_cfg.device));
    m_proc->setProcessEnvironment(env);

    connect(m_proc, &QProcess::readyRead, this, &Runner::onReadyRead);
    connect(m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &Runner::onFinished);
    connect(m_proc, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError e) {
        emit logLine("error: " + m_proc->errorString()
                     + " (" + m_proc->program() + ")");
        // FailedToStart だけは QProcess::finished が発火しないので
        // ここで完了扱いにしないと isRunning でないのに UI が実行状態のまま
        // 固まる (カーネル未インストール環境で必ず踏む経路)。
        // Crashed 等は finished も来るため二重発火させない。
        if (e == QProcess::FailedToStart) {
            m_proc->deleteLater();
            m_proc = nullptr;
            m_postPending = false;
            // どこを探して見つからなかったのかを示す (カーネル未導入の
            // 環境で最初に踏むエラーなので、次の一手が分かる文言にする)。
            // 各探索元は「実際の設定値」を出す — 「設定したのに効かない」と
            // 「未設定」をユーザーがコンソールだけで切り分けられるように。
            const char *homeVar = homeVarFor(m_cfg.kernel);
            const auto orUnset = [](const QString &v) {
                return v.isEmpty() ? QStringLiteral("(unset)") : v;
            };
            emit logLine(QStringLiteral(
                "hint: searched binaryDir=%1, kernel-path setting=%2, "
                "$%3=%4 (and their bin/), <app dir>/kernel, <app dir>, PATH")
                .arg(orUnset(m_cfg.binaryDir),
                     orUnset(kernelDirSetting(m_cfg.kernel)),
                     QLatin1String(homeVar),
                     orUnset(qEnvironmentVariable(homeVar))));
            emit logLine(QStringLiteral(
                "hint: build the solver kernel, then set its folder in "
                "ツール > カーネルパスの設定… (or set $%1 to the repository "
                "root — bin/ is searched too). See README 'カーネル'")
                .arg(QLatin1String(homeVar)));
            emit logLine(QStringLiteral("=== failed (kernel not found) ==="));
            emit finished(false);
        }
    });

    emit logLine(QStringLiteral("$ cd %1").arg(m_cfg.workingDir));
    emit logLine(QStringLiteral("$ %1 %2").arg(program, args.join(' ')));
    m_proc->start(program, args);
    emit started();
}

void Runner::stop()
{
    m_postPending = false;
    if (!isRunning()) return;
    m_proc->terminate();
    if (!m_proc->waitForFinished(2000))
        m_proc->kill();
}

void Runner::onReadyRead()
{
    if (!m_proc) return;
    // solver iteration lines: "%7d %.6f %.6f" (sol/solve.c)
    static const QRegularExpression stepRe(
        "^\\s*(\\d+)\\s+([-+0-9.eE]+)\\s+([-+0-9.eE]+)\\s*$");
    while (m_proc->canReadLine()) {
        const QString line = QString::fromUtf8(m_proc->readLine())
                                 .remove('\r').trimmed();
        if (line.isEmpty()) continue;
        emit logLine(line);
        const auto m = stepRe.match(line);
        if (m.hasMatch())
            emit progress(m.captured(1).toInt(), m_totalSteps);
    }
}

void Runner::onFinished(int exitCode, QProcess::ExitStatus status)
{
    const bool ok = (status == QProcess::NormalExit && exitCode == 0);
    m_proc->deleteLater();
    m_proc = nullptr;

    if (ok && m_postPending) {
        m_postPending = false;
        emit logLine("=== solver done, running post ===");
        launch(false);
        return;
    }
    m_postPending = false;
    emit logLine(ok ? "=== normal end ==="
                    : QStringLiteral("=== failed (exit %1) ===").arg(exitCode));
    emit finished(ok);
}
