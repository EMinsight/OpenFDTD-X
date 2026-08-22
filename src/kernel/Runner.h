// Runner.h — drives the OpenFDTD-family CLI kernels as subprocesses.
//
// 本家のパイプライン:
//   solver:  ofd [-n <thread>] [-out <outfile>] <datafile>   → ofd.out + ofd.log
//   post:    ofd_post [-n <thread>] [-html] <datafile>       → ev.ev2/ev.ev3 or *.htm
//   MPI:     mpiexec -n <process> ofd_mpi [-p x y z] [-n <thread>] <datafile>
//   CUDA:    ofd_cuda / ofd_cuda_mpi
//
// 姉妹ソルバー (光ドメインの RCWA / BPM):
//   OpenRCWA: orcwa / orcwa_mpi / orcwa_cuda + orcwa_post
//   OpenBPM:  obpm  / obpm_mpi  / obpm_cuda  + obpm_post
//
// バイナリ探索順: cfg.binaryDir → $OPENFDTD_HOME (orcwa は $OPENRCWA_HOME,
// obpm は $OPENBPM_HOME) → アプリ実行ディレクトリ → PATH。
#pragma once
#include <QObject>
#include <QProcess>
#include <QString>

namespace ofd {

class Project;

enum class Engine  { CPU, CPU_MPI, GPU, GPU_MPI };
enum class Kernel  { FDTD, RCWA, BPM, Bellhop, PEEC, FEM }; // solver family
enum class RunMode { Solver, Post, Both };

struct RunConfig {
    Engine  engine    = Engine::CPU;
    Kernel  kernel    = Kernel::FDTD;
    RunMode mode      = RunMode::Both;
    int     threads   = 4;       // OpenMP threads
    int     processes = 2;       // MPI processes
    int     device    = 0;       // GPU device no. (CUDA_VISIBLE_DEVICES)
    bool    evHtml    = false;   // post: -html → ev2d.htm / ev3d.htm
    QString workingDir;          // where .ofd lives — outputs land here too
    QString binaryDir;           // explicit kernel location (optional)
};

class Runner : public QObject {
    Q_OBJECT
public:
    explicit Runner(QObject *parent = nullptr);
    ~Runner() override;

    bool isRunning() const;
    QString workingDir() const { return m_cfg.workingDir; }
    const RunConfig &config() const { return m_cfg; }

    // Serialize project → .ofd/.ofdx in the working dir, then launch.
    void start(Project *project, const RunConfig &cfg = {});
    void stop();

    static QString solverBinary(const RunConfig &cfg);
    static QString postBinary(const RunConfig &cfg);
    // 探索順: binaryDir → $<KERNEL>_HOME → <app dir>/kernel → <app dir> → PATH。
    // 各ディレクトリは直下と bin/ の両方を見る (HOME にリポジトリルートを
    // 指定できるように — 各カーネルのビルドはバイナリを bin/ に置く)。
    static QString resolveBinary(const RunConfig &cfg, const QString &name);
    // カーネルの場所を指す環境変数名 (OPENFDTD_HOME 等)
    static const char *homeVarFor(Kernel k);

    // GUI で設定したカーネルディレクトリ (QSettings "OpenFDTD/Kernels"、
    // openfdtd_x / openuwa 共有)。環境変数が届かない Finder / Dock 起動でも
    // 効く。空文字列で設定削除。探索順は binaryDir → この設定 → 環境変数 →
    // <app dir>/kernel → <app dir> → PATH。
    static QString kernelDirSetting(Kernel k);
    static void    setKernelDirSetting(Kernel k, const QString &dir);

    // 実際に起動できるソルバーバイナリの絶対パス (PATH 解決込み)。
    // 見つからなければ空文字列。未検出の事前警告表示 (ステータスバー /
    // カーネルパス設定ダイアログ) が使う。
    static QString resolvedSolverPath(const RunConfig &cfg);

    // ── 実行環境の可用性 ────────────────────────────────────────────────
    // 「選べるのに実行できない」を防ぐための実機検出。GUI はこれを見て
    // エンジンの選択肢を無効化し、理由を出す (絶対規則 5)。
    // MPI: mpiexec (または mpirun) が見つかり (findMpiLauncher)、かつ
    //      <kernel>_mpi のバイナリが解決できること。両方そろって初めて実行できる。
    // CUDA: <kernel>_cuda のバイナリが解決できること (GPU 実機の有無は
    //      起動してみるまで分からないので、ここでは判定しない)。
    struct Availability {
        bool    mpi = false;       // mpiexec/mpirun + _mpi バイナリ
        bool    cuda = false;      // _cuda バイナリ
        QString mpiLauncher;       // 見つかった mpiexec のパス (空 = 無し)
        QString mpiReason;         // 使えない理由 (利用者向け)
        QString cudaReason;
    };
    static Availability checkAvailability(Kernel kernel);

    // MPI ランチャ (mpiexec / mpirun) の探索。見つからなければ空。探索順:
    //   1. GUI 設定 (mpiLauncherSetting — 実行ファイルの絶対パス)
    //   2. PATH
    //   3. $MSMPI_BIN (MS-MPI のインストーラが設定する環境変数)
    //   4. Windows の標準インストール先 C:/Program Files/Microsoft MPI/Bin
    //   5. カーネルの探索ディレクトリ (cfg.binaryDir → カーネルパス設定 →
    //      $<KERNEL>_HOME → <app dir>/kernel → <app dir>、各直下と bin/)
    //      — カーネルと一緒に mpiexec を配置した構成 (管理者権限なしで
    //      MS-MPI を展開した PC など) のため
    // Windows の MS-MPI は SMPD サービス無しでも単一ノードなら mpiexec が
    // 動く (ローカルに smpd を起動する) ので、サービスの有無は判定しない。
    static QString findMpiLauncher(const RunConfig &cfg = RunConfig());
    // GUI で設定した mpiexec のパス (QSettings "OpenFDTD/Kernels"、
    // カーネルパス設定ダイアログが書く)。空文字列で設定削除。
    static QString mpiLauncherSetting();
    static void    setMpiLauncherSetting(const QString &path);

    // カーネルへ渡す引数 (mpiexec の分は含まない)。本家 CLI の規約:
    //   ofd / ofd_mpi         : [-n <thread>] <datafile>   (OpenMP スレッド数)
    //   ofd_cuda / _cuda_mpi  : [-gpu|-cpu] [-device <n>] <datafile> — -n は無い。
    //                           渡すと未知の引数として入力ファイル名に取り違え
    //                           られる (最後の引数で上書きされ実害は無いが、
    //                           規約外なので GPU 系には渡さない)。GPU の選択は
    //                           環境変数 CUDA_VISIBLE_DEVICES で行う (launch)。
    //   bellhopcxx            : <FILEROOT> (拡張子を除いたケース名のみ)
    //   *_post                : [-n <thread>] [-html] <datafile>
    // 純関数なので selftest で検証する (プロセスを起動しない)。
    static QStringList solverArguments(const RunConfig &cfg,
                                       const QString &inputPath);
    static QStringList postArguments(const RunConfig &cfg,
                                     const QString &inputPath);

    // ── エンジン × ソルバー設定の「仕様上できない」組合せ ──────────────────
    // バイナリの有無 (checkAvailability) とは独立に、その変種が計算できない
    // 理由を返す。空文字列 = 問題なし。実機で確認した事実に基づく:
    //   光 RCWA / FMM (rcwalayer を出力する有効な層スタック):
    //     RCWA コアは orcwa (CPU) だけに結線されている (sol/rcwa_bridge.cpp)。
    //     orcwa_mpi / orcwa_cuda / orcwa_cuda_mpi は FDTD 専用で、rcwalayer
    //     入力ではメッシュ 0 のまま走ろうとして落ちる。
    //   光 BPM: obpm_mpi / obpm_cuda_mpi は FDTD 専用 (BPM 未対応)。
    //     obpm_cuda は BPM に対応している (cuda/solve_bpm.cu)。
    //   それ以外 (電磁 / 光 FDTD / 水中音響): 変種間に仕様差は無い
    //     (水中音響の MPI 版不在は checkAvailability が扱う)。
    // MainWindow はこれを見てエンジンの選択肢を無効化し、実行直前にも
    // 再確認する (絶対規則 5: 走らせて初めて失敗する組合せを選ばせない)。
    static QString engineUnsupportedReason(const Project &project, Engine engine);

    // アクティブドメインとソルバー設定から実行カーネルを決める
    // (MainWindow の実行設定と selftest で共用)。
    //   光: RCWA → orcwa / BPM → obpm / FMM → orcwa (RCWA と同一手法の
    //       別名のため専用カーネルは作らない) / それ以外 → ofd
    //   他ドメイン: ofd
    static Kernel kernelForProject(const Project &project);

    // ソルバー実行ログのファイル名 (カーネル別)。どのバイナリがどのログを
    // 書くかは Runner の知識なのでここに置く (タブごとに書かない)。
    // bellhopcxx は収束履歴を出さないので空文字列。
    static QString runLogName(Kernel k);

    // start() が使う作業ディレクトリを、起動前に同じ規則で求める
    // (実行前に前回の出力を掃除したい呼び出し側のため)。start() 自身も使う。
    static QString resolveWorkingDir(const Project *project,
                                     const RunConfig &cfg);

    // この実行が ONN 活性化カーブ (activation_curve.csv) を新たに生成し
    // うるか。obpm をソルバー段で起動し、かつパワースイープが有効な実行
    // だけが true。結果表示を「その実行が生成したもの」に限るために使う。
    static bool producesActivationCurve(const Project &project,
                                        const RunConfig &cfg);

signals:
    void started();
    void logLine(const QString &line);     // forwarded stdout/stderr
    void progress(int step, int total);    // parsed from "%7d %f %f" lines
    void finished(bool ok);

private slots:
    void onReadyRead();
    void onFinished(int exitCode, QProcess::ExitStatus status);

private:
    void launch(bool solverPhase);

    QProcess  *m_proc = nullptr;
    RunConfig  m_cfg;
    QString    m_ofdPath;
    bool       m_postPending = false;   // Both mode: post runs after solver
    int        m_totalSteps = 1000;
};

} // namespace ofd
