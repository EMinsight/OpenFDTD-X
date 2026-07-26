// AcousticRunner.h — drives the external acoustic solver as a subprocess.
//
// 既存 Runner (ofd/orcwa/obpm) とは分離した音響専用ランナー (ADR-0007
// Decision 1)。連携は QProcess 疎結合:
//   solver <working_dir> [<input_file>]
// ソルバーは作業ディレクトリに出力契約ファイル一式を書き出す:
//   metadata.json (必須) / rir.wav (必須) / metrics.json (任意) /
//   solver.log (必須)  — docs/adr/0007-acoustic-solver-contract.md
// 進捗は stdout の "progress a/b" 行を解析する。
//
// バイナリ探索順 (ADR-0007 Decision 3): cfg.executable (明示指定、最優先)
// → $OFDX_ACOUSTIC_SOLVER (絶対パス直接指定 — CI/開発オーバーライド)
// → $OPENFDTD_ACOUSTICS_HOME 配下 → アプリ実行ディレクトリ kernel/
// → PATH。見つからなければ起動せず finished(false)。
#pragma once
#include <QObject>
#include <QProcess>
#include <QString>

namespace ofd {

// RIR の取得元 5 値 (ADR-0007 Decision 2)。`.ofdx` に
// opera_analysis.solver.backend (int, 同順) で永続化されるため、
// 値の並び替え・挿入は禁止 (追加は末尾のみ)。
// 外部プロセスを起動するのは ExternalFDTD / ExternalGeometric のみ。
enum class AcousticBackend {
    None,               // RIR 取得なし (統計推定のみ)
    MeasuredRir,        // 実測 RIR (RirAnalysisTab の従来経路)
    Statistical,        // 統計モデルからの合成 RIR
    ExternalFDTD,       // 外部音響 FDTD ソルバー
    ExternalGeometric   // 外部幾何音響 (レイトレース系) ソルバー
};

struct AcousticRunConfig {
    AcousticBackend backend = AcousticBackend::ExternalFDTD;
    QString executable;      // ソルバーバイナリの明示パス (空 = 自動解決)
    QString workingDir;      // 出力契約ファイルの置き場 (空 = 一時ディレクトリ)
    QString inputFile;       // ソルバーへ渡す入力ファイル (任意)
    QString outputRirFile;   // 期待する RIR ファイル名 (空 = "rir.wav")
    int     threads   = 4;   // OpenMP threads (OMP_NUM_THREADS)
    int     processes = 1;   // >1 で mpiexec -n <processes> 経由で起動
};

class AcousticRunner : public QObject {
    Q_OBJECT
public:
    explicit AcousticRunner(QObject *parent = nullptr);
    ~AcousticRunner() override;

    bool isRunning() const;
    QString workingDir() const { return m_cfg.workingDir; }

    // ソルバーを解決して起動する。失敗は logLine + finished(false) で通知。
    void start(const AcousticRunConfig &cfg);
    void stop();

    // 探索順どおりに実在するバイナリの絶対パスを返す。見つからなければ空。
    static QString resolveSolver(const AcousticRunConfig &cfg);

signals:
    void started();
    void logLine(const QString &line);     // forwarded stdout/stderr
    void progress(int step, int total);    // parsed from "progress a/b" lines
    void rirReady(const QString &path);    // 契約検証後の rir.wav 絶対パス
    void finished(bool ok);

private slots:
    void onReadyRead();
    void onFinished(int exitCode, QProcess::ExitStatus status);

private:
    void fail(const QString &reason);      // logLine + finished(false)

    QProcess         *m_proc = nullptr;
    AcousticRunConfig m_cfg;
};

} // namespace ofd
