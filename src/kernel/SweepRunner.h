// SweepRunner.h — 1 つのパラメータを振って同じカーネルを N 回まわす。
//
// なぜ GUI 側なのか:
//   OpenFDTD のカーネルは 1 回の実行につき `planewave = θ φ pol` を 1 組しか
//   受け付けない (sol/input_data.c)。したがって「入射角スイープ」は
//   カーネルの改修ではなく **同じ入力の θ 違いを N 回走らせる** ことで実現
//   する。ここはその司会進行だけを持ち、実行そのものは Runner に委ねる。
//
// 動作:
//   1. plan() が振る値の列を作る (純関数 — selftest が直接検証する)
//   2. 各点で base プロジェクトを複製 → applyPoint() で値を差し替え
//      → <baseDir>/sweep_NNN/ へ .ofd を書き出して Runner を起動
//   3. 1 点終わるごとに <kernel>.log / far1d.log を読んで結果を集める
//   4. 全点終了で finished(true)
//
// 直列実行なのは、カーネルが OpenMP でコアを使い切るため (同時実行すると
// 1 点あたりが遅くなるだけで総時間は縮まない)。
#pragma once
#include <QObject>
#include <QString>
#include <QVector>

#include "Runner.h"
#include "../io/KernelResultReader.h"

namespace ofd {

class Project;

// 何を振るか
enum class SweepKind {
    PlaneWaveTheta,   // 平面波の入射角 θ [deg]
    PlaneWavePhi,     // 同 φ [deg]
};

struct SweepConfig {
    SweepKind kind = SweepKind::PlaneWaveTheta;
    double    from = 0.0;      // 振る範囲 [deg]
    double    to   = 180.0;
    int       points = 37;     // >= 2
    RunConfig run;             // 各実行の設定 (engine / threads / kernel)
    QString   baseDir;         // 実行用の親ディレクトリ (空 = Runner の既定)
};

// 1 点の結果
struct SweepResult {
    double  value = 0.0;       // 振った値 [deg]
    QString label;             // "θ = 30°" 等
    QString dir;               // その点の作業ディレクトリ
    bool    ok = false;        // カーネルが正常終了したか
    // 遠方界 (far1d.log)。RCS はここから読む
    QVector<FarPattern> patterns;
    // 給電点表 (<kernel>.log)
    QVector<FeedSweep>  feeds;
    // 代表値: 全パターンを通じた E-abs の最大 [dB]。パターンが無ければ NaN。
    // 「スイープの 1 点を 1 行で見る」ための要約であって、RCS そのものでは
    // ない (RCS の絶対値化は未実装 — 絶対規則 5)。
    double  peakEAbs_dB = 0.0;
    bool    hasPeak = false;
};

class SweepRunner : public QObject {
    Q_OBJECT
public:
    explicit SweepRunner(QObject *parent = nullptr);

    // ── 純関数 (selftest が直接叩く) ────────────────────────────────────
    // 振る値の列。points < 2 または from == to なら空を返す
    // (1 点スイープは通常実行と同じなので、スイープとしては成立しない)。
    static QVector<double> plan(const SweepConfig &cfg);
    // 1 点ぶんの値をプロジェクトへ当てる。平面波が無効なら有効化する
    // (スイープは平面波入射の解析なので、無効のままでは意味が無い)。
    static void applyPoint(Project &p, SweepKind kind, double value);
    // 点 i の作業ディレクトリ名 (sweep_000, sweep_001, …)
    static QString pointDirName(int index);
    // 表示ラベル ("θ = 30°")
    static QString pointLabel(SweepKind kind, double value);
    // その点の出力から結果を組み立てる (実行後のディレクトリを読むだけ)
    static SweepResult collect(const QString &dir, Kernel kernel,
                               SweepKind kind, double value, bool ok);
    // 結果表 → CSV テキスト (先頭行はヘッダ)
    static QString toCsv(const QVector<SweepResult> &results);

    // ── 実行 ────────────────────────────────────────────────────────────
    bool isRunning() const { return m_running; }
    const QVector<SweepResult> &results() const { return m_results; }
    // base を複製して N 回まわす。base は変更しない。
    // 開始できなければ false (点数不足 / 既に実行中)。
    bool start(const Project &base, const SweepConfig &cfg);
    void stop();

signals:
    void logLine(const QString &line);
    void pointStarted(int index, int total, const QString &label);
    void pointFinished(int index, const SweepResult &result);
    void finished(bool ok);      // ok = 全点が正常終了

private:
    void launchNext();
    void onPointFinished(bool ok);

    Runner              *m_runner = nullptr;
    SweepConfig          m_cfg;
    QVector<double>      m_values;
    QVector<SweepResult> m_results;
    Project             *m_work = nullptr;   // 実行用の複製 (所有)
    int                  m_index = -1;
    bool                 m_running = false;
    bool                 m_allOk = true;
};

} // namespace ofd
