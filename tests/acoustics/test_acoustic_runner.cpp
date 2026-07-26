// test_acoustic_runner.cpp — AcousticRunner × モックソルバーの統合テスト。
// C++17 / Qt6 Core (Qt 依存テストのため音響コアの C++14 縛りの対象外)。
//
// 使い方: ofdx_test_acoustic_runner <mock_acoustic_solver のパス>
// (CTest は $<TARGET_FILE:ofdx_mock_acoustic_solver> を渡す)
//
// 検証経路 (ADR-0007): AcousticRunner 起動 → stdout "progress a/b" 解析 →
// 出力契約検証 → rirReady(rir.wav) → WavReader → RirAnalyzer で
// T30 ≈ 0.8 s (モックの公称 RT)。イベントループ待ちは QSignalSpy 相当を
// QEventLoop + タイムアウトで手書きする。
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTimer>

#include <cstdio>
#include <vector>

#include "../../src/acoustics/core/RirAnalyzer.h"
#include "../../src/acoustics/io/WavReader.h"
#include "../../src/kernel/AcousticRunner.h"
#include "test_common.h"

using namespace ofd;
using namespace ofd::acoustics;

namespace {

// QSignalSpy 相当: シグナルを記録し、finished まで (または timeout まで)
// イベントループを回す。
struct RunObserver {
    bool startedSeen = false;
    bool finishedSeen = false;
    bool finishedOk = false;
    bool timedOut = false;
    QString rirPath;
    QStringList log;
    int lastStep = -1;
    int lastTotal = -1;
    int progressCount = 0;
};

RunObserver runAndWait(const AcousticRunConfig &cfg, int timeoutMs = 30000) {
    AcousticRunner runner;
    RunObserver obs;
    QEventLoop loop;

    QObject::connect(&runner, &AcousticRunner::started,
                     [&]() { obs.startedSeen = true; });
    QObject::connect(&runner, &AcousticRunner::logLine,
                     [&](const QString &line) { obs.log << line; });
    QObject::connect(&runner, &AcousticRunner::progress,
                     [&](int step, int total) {
                         obs.lastStep = step;
                         obs.lastTotal = total;
                         ++obs.progressCount;
                     });
    QObject::connect(&runner, &AcousticRunner::rirReady,
                     [&](const QString &path) { obs.rirPath = path; });
    QObject::connect(&runner, &AcousticRunner::finished, [&](bool ok) {
        obs.finishedSeen = true;
        obs.finishedOk = ok;
        loop.quit();
    });

    runner.start(cfg);
    if (!obs.finishedSeen) { // 失敗経路では start() 内で同期発火する
        QTimer timer;
        timer.setSingleShot(true);
        QObject::connect(&timer, &QTimer::timeout, [&]() {
            obs.timedOut = true;
            loop.quit();
        });
        timer.start(timeoutMs);
        loop.exec();
    }
    return obs;
}

} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    if (argc < 2) {
        std::printf("usage: %s <mock_acoustic_solver>\n", argv[0]);
        return 2;
    }
    const QString mockPath = QFileInfo(QString::fromLocal8Bit(argv[1]))
                                 .absoluteFilePath();
    CHECK(QFileInfo::exists(mockPath));

    // 環境からの干渉を防ぐ (探索順①のオーバーライドを既定で無効化)
    qunsetenv("OFDX_ACOUSTIC_SOLVER");
    qunsetenv("OPENFDTD_ACOUSTICS_HOME");

    // ── 正常系: モックを起動 → 契約検証 → RIR 分析 (T30 ≈ 0.8 s) ──
    {
        QTemporaryDir tmp;
        CHECK(tmp.isValid());

        AcousticRunConfig cfg;
        cfg.backend = AcousticBackend::ExternalFDTD;
        cfg.executable = mockPath;
        cfg.workingDir = tmp.path();
        cfg.threads = 2;

        const RunObserver obs = runAndWait(cfg);
        CHECK(!obs.timedOut);
        CHECK(obs.startedSeen);
        CHECK(obs.finishedSeen);
        CHECK(obs.finishedOk);

        // 進捗: "progress a/b" が (a,b) にパースされ 10/10 で終わる
        CHECK(obs.progressCount >= 1);
        CHECK(obs.lastStep == 10);
        CHECK(obs.lastTotal == 10);

        // rirReady のパスに rir.wav が実在する
        CHECK(!obs.rirPath.isEmpty());
        CHECK(QFileInfo::exists(obs.rirPath));
        CHECK(QFileInfo(obs.rirPath).fileName() == QStringLiteral("rir.wav"));

        // 出力契約の残りのファイルも作業ディレクトリに揃っている
        const QDir wd(tmp.path());
        CHECK(QFileInfo::exists(wd.absoluteFilePath("metadata.json")));
        CHECK(QFileInfo::exists(wd.absoluteFilePath("metrics.json")));
        CHECK(QFileInfo::exists(wd.absoluteFilePath("solver.log")));

        // WavReader → RirAnalyzer (実測 RIR と同一パイプライン)
        AcousticResult<AudioBuffer> wav =
            readWavFile(obs.rirPath.toStdString());
        CHECK(wav.success());
        if (wav.success()) {
            const AudioBuffer &buf = wav.value();
            CHECK(buf.sampleRateHz == 48000.0);
            CHECK(buf.channelCount() == 1);
            CHECK(buf.sampleCount() > 0);

            RirAnalyzerConfig acfg;
            acfg.bandSet = BandSet::FullBandOnly;
            RirAnalyzer analyzer(acfg);
            AcousticResult<RirAnalysisResult> r = analyzer.analyze(buf, 0);
            CHECK(r.success());
            if (r.success()) {
                const RirAnalysisResult &res = r.value();
                CHECK(res.directSound.found);
                CHECK(res.bands.size() == 1);
                if (res.bands.size() == 1) {
                    const MetricValue &t30 = res.bands[0].metrics.t30;
                    CHECK(t30.valid);
                    if (t30.valid) {
                        // モックの公称 RT = 0.8 s → T30 は 0.8 ± 0.2 s
                        CHECK_NEAR(t30.value, 0.8, 0.2);
                        std::printf("  mock RIR: T30 = %.4f s "
                                    "(nominal 0.8 s)\n", t30.value);
                    }
                }
            }
        }
    }

    // ── 異常系: 実行ファイルが解決できない → finished(false) + 理由ログ ──
    {
        QTemporaryDir tmp;
        AcousticRunConfig cfg;
        cfg.backend = AcousticBackend::ExternalFDTD;
        cfg.executable = tmp.path() + "/no_such_solver";
        cfg.workingDir = tmp.path();

        const RunObserver obs = runAndWait(cfg, 5000);
        CHECK(obs.finishedSeen);
        CHECK(!obs.finishedOk);
        CHECK(obs.rirPath.isEmpty());
        CHECK(!obs.log.filter(QStringLiteral("not found")).isEmpty());
    }

    // ── 異常系: 外部プロセスを起動しない backend → finished(false) ──
    {
        QTemporaryDir tmp;
        AcousticRunConfig cfg;
        cfg.backend = AcousticBackend::MeasuredRir;
        cfg.executable = mockPath;
        cfg.workingDir = tmp.path();

        const RunObserver obs = runAndWait(cfg, 5000);
        CHECK(obs.finishedSeen);
        CHECK(!obs.finishedOk);
        CHECK(!obs.startedSeen);
    }

    // ── 解決順①: $OFDX_ACOUSTIC_SOLVER によるオーバーライド ──
    {
        QTemporaryDir tmp;
        qputenv("OFDX_ACOUSTIC_SOLVER", mockPath.toLocal8Bit());

        AcousticRunConfig cfg;
        cfg.backend = AcousticBackend::ExternalGeometric;
        cfg.executable.clear(); // 明示指定なし → 環境変数で解決される
        cfg.workingDir = tmp.path();

        const RunObserver obs = runAndWait(cfg);
        qunsetenv("OFDX_ACOUSTIC_SOLVER");
        CHECK(!obs.timedOut);
        CHECK(obs.finishedSeen);
        CHECK(obs.finishedOk);
        CHECK(!obs.rirPath.isEmpty());
    }

    return testutil::summary("test_acoustic_runner");
}
