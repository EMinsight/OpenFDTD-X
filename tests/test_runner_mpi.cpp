// test_runner_mpi.cpp — GUI の kernel/Runner を MPI 構成で実際に走らせる統合テスト。
//
// selftest 側は Runner の純関数 (引数の組み立て・ランチャ探索・可用性判定) を
// プロセスを起動せずに検証している。本テストはその先 — **Runner が実際に
// mpiexec を起動し、カーネルが完走し、結果が CPU 実行と一致する**ところまでを
// 見る。ここが通らないと「GUI から MPI エンジンを選ぶと動く」と言えない。
//
// 実カーネルが要るので、他の統合テストと同じく環境変数ゲート:
//   OFDX_OFD_BIN=<...>/bin/ofd.exe   (同じディレクトリの ofd_mpi / ofd_cuda_mpi を使う)
// 未設定・MPI 未導入なら skip して 0 を返す (CI を赤くしない)。
//
// 実行: ofdx_test_runner_mpi <tests/data のパス>
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QTemporaryDir>
#include <QTimer>

#include <cstdio>

#include "core/Project.h"
#include "io/KernelResultReader.h"
#include "kernel/Runner.h"

using namespace ofd;

static int g_checks = 0;
static int g_failures = 0;

static void check(bool cond, const char *what)
{
    ++g_checks;
    if (!cond) {
        ++g_failures;
        std::fprintf(stderr, "FAIL: %s\n", what);
    }
}

namespace {

struct RunObserver {
    bool        startedSeen = false;
    bool        finishedSeen = false;
    bool        finishedOk = false;
    bool        timedOut = false;
    QStringList log;
    int         progressCount = 0;
    int         lastTotal = -1;

    // Runner がログに出す起動コマンド行 ("$ <program> <args>")。
    // 期待どおりの実行ファイル・引数で起動したかはこれで見る。
    QString commandLine() const
    {
        for (const QString &l : log)
            if (l.startsWith(QLatin1String("$ ")) && !l.contains(QLatin1String("$ cd ")))
                return l;
        return QString();
    }
};

RunObserver runAndWait(Project *project, const RunConfig &cfg, int timeoutMs)
{
    Runner runner;
    RunObserver obs;
    QEventLoop loop;

    QObject::connect(&runner, &Runner::started, [&] { obs.startedSeen = true; });
    QObject::connect(&runner, &Runner::logLine,
                     [&](const QString &line) { obs.log << line; });
    QObject::connect(&runner, &Runner::progress, [&](int, int total) {
        ++obs.progressCount;
        obs.lastTotal = total;
    });
    QObject::connect(&runner, &Runner::finished, [&](bool ok) {
        obs.finishedSeen = true;
        obs.finishedOk = ok;
        loop.quit();
    });

    runner.start(project, cfg);
    if (!obs.finishedSeen) {   // 失敗経路は start() 内で同期発火しうる
        QTimer timer;
        timer.setSingleShot(true);
        QObject::connect(&timer, &QTimer::timeout, [&] {
            obs.timedOut = true;
            runner.stop();
            loop.quit();
        });
        timer.start(timeoutMs);
        loop.exec();
    }
    return obs;
}

// ofd.log の給電点表 (21 周波数 × Rin/Xin) を読む
QVector<FeedSweepPoint> feedPoints(const QString &workDir)
{
    const QVector<FeedSweep> s =
        KernelResultReader::readFeedSweeps(QDir(workDir).filePath("ofd.log"));
    return s.isEmpty() ? QVector<FeedSweepPoint>() : s.first().points;
}

QString logText(const QString &workDir)
{
    QFile f(QDir(workDir).filePath("ofd.log"));
    return f.open(QIODevice::ReadOnly | QIODevice::Text)
               ? QString::fromUtf8(f.readAll()) : QString();
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    const QString sampleDir = (argc > 1)
        ? QString::fromLocal8Bit(argv[1]) : QStringLiteral("tests/data");
    const QString sample = QDir(sampleDir).filePath(QStringLiteral("dipole.ofd"));

    const QString ofdBin = qEnvironmentVariable("OFDX_OFD_BIN");
    if (ofdBin.isEmpty() || !QFileInfo::exists(ofdBin)) {
        std::printf("  (runner mpi test skipped: set OFDX_OFD_BIN to run)\n");
        return 0;
    }
    if (!QFileInfo::exists(sample)) {
        std::printf("  (runner mpi test skipped: %s not found)\n",
                    sample.toUtf8().constData());
        return 0;
    }
    const QString binDir = QFileInfo(ofdBin).absolutePath();

    // MPI が実際に使えるか (mpiexec + ofd_mpi)。片方でも欠ければ skip。
    RunConfig probe;
    probe.kernel = Kernel::FDTD;
    probe.binaryDir = binDir;
    probe.engine = Engine::CPU_MPI;
    const QString launcher = Runner::findMpiLauncher(probe);
    const QString mpiBin = Runner::resolvedSolverPath(probe);
    if (launcher.isEmpty() || mpiBin.isEmpty()) {
        std::printf("  (runner mpi test skipped: launcher=%s mpi binary=%s)\n",
                    launcher.isEmpty() ? "none" : "ok",
                    mpiBin.isEmpty() ? "none" : "ok");
        return 0;
    }
    std::printf("  runner mpi: launcher=%s\n", launcher.toUtf8().constData());

    QTemporaryDir cpuDir, mpiDir;
    check(cpuDir.isValid() && mpiDir.isValid(), "mpirun: temp dirs");
    if (!cpuDir.isValid() || !mpiDir.isValid()) return 1;

    // ── 基準: CPU 実行 ────────────────────────────────────────────────────
    Project cpuProject;
    QString err;
    check(cpuProject.load(sample, &err), "mpirun: sample loaded (cpu)");
    RunConfig cpu;
    cpu.kernel = Kernel::FDTD;
    cpu.engine = Engine::CPU;
    cpu.mode = RunMode::Solver;
    cpu.threads = 2;
    cpu.binaryDir = binDir;
    cpu.workingDir = cpuDir.path();
    const RunObserver cpuRun = runAndWait(&cpuProject, cpu, 300000);
    check(!cpuRun.timedOut && cpuRun.finishedOk, "mpirun: cpu baseline finished");
    check(logText(cpuDir.path()).contains(QLatin1String("normal end")),
          "mpirun: cpu baseline reports normal end");

    // ── 本題: CPU+MPI 実行 ────────────────────────────────────────────────
    Project mpiProject;
    check(mpiProject.load(sample, &err), "mpirun: sample loaded (mpi)");
    RunConfig mpi;
    mpi.kernel = Kernel::FDTD;
    mpi.engine = Engine::CPU_MPI;
    mpi.mode = RunMode::Solver;
    mpi.threads = 2;
    mpi.processes = 2;
    mpi.binaryDir = binDir;
    mpi.workingDir = mpiDir.path();
    const RunObserver mpiRun = runAndWait(&mpiProject, mpi, 300000);

    check(mpiRun.startedSeen, "mpirun: started() emitted");
    check(!mpiRun.timedOut, "mpirun: finished within the timeout (no hang)");
    check(mpiRun.finishedOk, "mpirun: finished(true)");

    // 起動コマンド: mpiexec -n <processes> <...ofd_mpi> -n <threads> <input>
    const QString cmd = mpiRun.commandLine();
    check(cmd.contains(QLatin1String("mpiexec")) || cmd.contains(QLatin1String("mpirun")),
          "mpirun: launched through the MPI launcher");
    check(cmd.contains(QLatin1String("-n 2")), "mpirun: process count passed to mpiexec");
    check(cmd.contains(QLatin1String("ofd_mpi")), "mpirun: the _mpi binary was launched");
    // CPU+MPI ではカーネルにも OpenMP スレッド数を渡す (GPU 系とは違う)
    check(cmd.count(QLatin1String("-n ")) >= 2,
          "mpirun: -n reaches both mpiexec and the kernel");

    // 進捗シグナル (ofd.log の反復行から解析される)
    check(mpiRun.progressCount > 0, "mpirun: progress() emitted");
    check(mpiRun.lastTotal == mpiProject.general().maxiter,
          "mpirun: progress total is the iteration count");

    // カーネル側のログ: 2 プロセスで走ったこと + 完走
    const QString mpiLog = logText(mpiDir.path());
    check(mpiLog.contains(QLatin1String("normal end")), "mpirun: kernel reports normal end");
    check(mpiLog.contains(QLatin1String("=2")) && mpiLog.contains(QLatin1String("process=")),
          "mpirun: kernel ran with 2 processes");

    // ── 数値の一致 (MPI は CPU と一致するのが本家の保証) ──────────────────
    const QVector<FeedSweepPoint> cpuPts = feedPoints(cpuDir.path());
    const QVector<FeedSweepPoint> mpiPts = feedPoints(mpiDir.path());
    check(!cpuPts.isEmpty() && cpuPts.size() == mpiPts.size(),
          "mpirun: both runs produced the same number of frequency points");
    if (!cpuPts.isEmpty() && cpuPts.size() == mpiPts.size()) {
        bool same = true;
        for (int i = 0; i < cpuPts.size(); ++i) {
            // ofd.log は小数 3 桁なので、表示精度で一致すれば十分
            if (cpuPts[i].freqHz != mpiPts[i].freqHz
                || cpuPts[i].rin != mpiPts[i].rin
                || cpuPts[i].xin != mpiPts[i].xin
                || cpuPts[i].vswr != mpiPts[i].vswr) { same = false; break; }
        }
        check(same, "mpirun: MPI impedance table matches the CPU run exactly");
    }

    // ── GPU+MPI (あれば): 起動できること + CUDA 版に -n を渡さないこと ────
    RunConfig gpuProbe;
    gpuProbe.kernel = Kernel::FDTD;
    gpuProbe.binaryDir = binDir;
    gpuProbe.engine = Engine::GPU_MPI;
    if (!Runner::resolvedSolverPath(gpuProbe).isEmpty()) {
        QTemporaryDir gpuDir;
        Project gpuProject;
        gpuProject.load(sample, &err);
        RunConfig gpu = gpuProbe;
        gpu.mode = RunMode::Solver;
        gpu.threads = 2;
        gpu.processes = 2;
        gpu.workingDir = gpuDir.path();
        const RunObserver gpuRun = runAndWait(&gpuProject, gpu, 300000);
        const QString gcmd = gpuRun.commandLine();
        check(gcmd.contains(QLatin1String("ofd_cuda_mpi")),
              "mpirun: gpu+mpi launched the _cuda_mpi binary");
        // mpiexec の -n <process> だけが残り、カーネルへの -n は無い
        check(gcmd.count(QLatin1String("-n ")) == 1,
              "mpirun: the CUDA kernel gets no -n (it does not accept one)");
        check(!gpuRun.timedOut && gpuRun.finishedOk, "mpirun: gpu+mpi finished");
        check(logText(gpuDir.path()).contains(QLatin1String("normal end")),
              "mpirun: gpu+mpi kernel reports normal end");
    } else {
        std::printf("  (gpu+mpi part skipped: ofd_cuda_mpi not found)\n");
    }

    std::printf("runner mpi: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
