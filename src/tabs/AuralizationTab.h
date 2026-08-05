// AuralizationTab.h — 可聴化タブ (フェーズ4)。
//
// ドライ (無響/近接) 歌唱 WAV と実測 RIR WAV を C++14 音響コア
// (ConvolutionEngine) で畳み込み、ウェット WAV (float32) を書き出す。
//   ① 入力   — ドライ WAV / RIR WAV (実測RIR分析タブの rirPath を共用) /
//               出力先 / ゲインモード (そのまま / 推奨ゲイン適用)。
//               自動正規化は行わない。ドライ WAV は「🎵 音源リストから」で
//               音源リスト (.ofdx acoustic.sources[].signal) の信号を
//               取り込める (1 音源 = 1 ドライ音源。ミックスは未実装)
//   ② 実行   — QtAcousticAdapter::convolveFiles で畳み込み + WAV 書き出し
//               (QThread::create + busy ガードで非同期 — gui.md)
//   ③ 結果   — outputPeak / suggestedGainDb / クリップ数。fs 不一致は
//               RIR をドライ側 fs へリサンプリングして続行し、変換した旨を
//               結果に明示する (core/Resampler — 負債 #12 解消)。fs 自体が
//               不正で変換できない場合のみエラー理由を表示する
//   ④ A/B    — ドライ / ウェット波形の MiniPlot 並置。アプリ内再生は
//               未対応 (書き出した WAV を外部プレイヤーで比較する) と明示
//   ⑤ 一括   — 複数受音点の一括可聴化。受音点リスト (AcousticOpts::receivers)
//               の各行に受音点ごとの RIR WAV (ReceiverRow::rirFile, .ofdx
//               追加キー) を割り当て、有効かつ RIR 指定済みの行を順に畳み込んで
//               <ドライ名>_<受音点名>.wav を出力先フォルダへ書き出す。
//               行ごとに QThread で非同期・行間で中断可能。受聴位置が違えば
//               RIR は異なるため「全受音点へ同じ RIR」の導線は置かない。
//               「📂 フォルダから自動割当」でフォルダ直下の *.wav を
//               受音点名との対応規則 (core/RirAutoAssign — 完全一致 /
//               rir_ 接頭・_rir 接尾 / 唯一の rir.wav) で各行へ一括割当できる
//               (既定では未設定の行のみ。曖昧な行は割り当てず理由を表示)
// 設定は .ofdx の acoustic/opera_analysis/auralization (+ 受音点ごとの RIR は
// acoustic.receivers[].rir_file) に永続化される。
#pragma once
#include <QHash>
#include <QScrollArea>
#include <QStringList>
#include <QVector>
#include "../core/Project.h"   // AcousticSourceRow (音源リストの行)

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

namespace ofd {

class Project;
class MiniPlot;

class AuralizationTab : public QScrollArea {
    Q_OBJECT
public:
    explicit AuralizationTab(Project *project, QWidget *parent = nullptr);

    // 一括レンダリングの出力ファイル名 (受音点ごと、拡張子 .wav 込み)。
    // 命名規則 <ドライ名>_<受音点名>.wav — 空名は P<行番号>、ファイル名に
    // 使えない文字と空白は '_'、重複は _2, _3… を付けて一意化する。
    // 決定的な純関数 (ヘッドレス検証からも参照する)。
    static QStringList batchOutputNames(const QString &dryPath,
                                        const QStringList &receiverNames);

    // ⑤ フォルダ直下の *.wav を受音点へ自動割当 (core/RirAutoAssign の規則)。
    // 「未設定の行のみ」チェックの状態に従い、割当結果をモデル
    // (ReceiverRow::rirFile) へ書いて Project::touch() する。戻り値は
    // 割り当てた行数。ダイアログを開かないのでヘッドレス検証からも呼べる。
    int autoAssignFromDir(const QString &dirPath);

    // ① ドライ音源を音源リスト (.ofdx acoustic.sources) から取り込む導線の
    //    純ロジック。ofdx_selftest は GUI_SOURCES をリンクしないため、
    //    ヘッダ内 inline 定義の static メソッドにしてある
    //    (AcousticSourceTab::syncFeedsFromSources と同じ流儀)。

    // ドライ音源として選べる行 = 有効 (enabled) かつ信号が非空の行の添字。
    // ファイルの実在は見ない (存在しないパスは UI 側が印を付けて示す)。
    static QVector<int> drySourceCandidates(
        const QVector<AcousticSourceRow> &sources)
    {
        QVector<int> idx;
        for (int i = 0; i < sources.size(); ++i) {
            if (!sources[i].enabled) continue;
            if (sources[i].signal.trimmed().isEmpty()) continue;
            idx.push_back(i);
        }
        return idx;
    }

    // 音源 1 行の信号を可聴化のドライ音源 (auralizationDryFile) に設定する。
    // **1 音源 = 1 ドライ音源** で、複数音源のミックスは行わない (未実装)。
    // 添字が範囲外 / 無効行 / 信号が空なら何も書かずに false を返す。
    // Project::touch() は呼び出し側で行う。
    static bool setDryFromSource(Project &p, int index)
    {
        const QVector<AcousticSourceRow> &src = p.acoustic().sources;
        if (index < 0 || index >= src.size()) return false;
        if (!src[index].enabled) return false;
        const QString sig = src[index].signal.trimmed();
        if (sig.isEmpty()) return false;
        p.operaAcoustic().auralizationDryFile = sig;
        p.operaAcoustic().enabled = true;   // 可聴化を使う意思表示 (browseDry と同じ)
        return true;
    }

private slots:
    void refresh();        // model → widgets
    void apply();          // widgets → model
    void browseDry();
    void chooseDryFromSource();   // ① 🎵 音源リストからドライ音源を取り込む
    void browseRir();
    void browseOutput();
    void runConvolution();
    // ⑤ 複数受音点の一括可聴化
    void browseBatchOutDir();
    void autoAssignRirs();     // 📂 フォルダから自動割当 (ディレクトリ選択)
    void runBatch();
    void cancelBatch();

private:
    void clearResult(const QString &statusText);
    // ⑤ 一括レンダリングの内部処理
    void rebuildBatchTable();              // model → 受音点表 (状態列は保持)
    void startBatchJob(int jobIdx);        // m_batchJobs[jobIdx] を非同期実行
    void finishBatch(int nextIdx);         // 完了/中断の後始末 (nextIdx 以降は未実行)
    void setBatchRowStatus(int row, const QString &text,
                           const QString &tooltip = QString());
    QString batchOutputDir() const;        // 出力先 (空なら既定の解決)
    QString autoAssignDefaultDir() const;  // 自動割当ダイアログの初期フォルダ
    void updateBusyUi();                   // 単発/一括の実行ボタン排他

    Project *m_p;
    bool     m_updating = false;
    bool     m_runBusy = false;    // 畳み込み実行中 (再入防止 busy ガード)

    // ① 入力
    QLineEdit   *m_dryPath = nullptr;
    QPushButton *m_dryFromSrcBtn = nullptr;  // 🎵 音源リストから (信号を取り込む)
    QLineEdit *m_rirPath = nullptr;      // OperaAcousticSettings::rirPath 共用
    QLineEdit *m_outPath = nullptr;
    QComboBox *m_gainMode = nullptr;     // 0=そのまま 1=推奨ゲイン適用

    // ② 実行
    QPushButton *m_runBtn = nullptr;
    QLabel      *m_status = nullptr;

    // ③ 結果
    QLabel *m_peakLabel = nullptr;
    QLabel *m_gainLabel = nullptr;
    QLabel *m_clipLabel = nullptr;
    QLabel *m_warnings = nullptr;

    // ④ A/B 波形
    MiniPlot *m_dryPlot = nullptr;
    MiniPlot *m_wetPlot = nullptr;

    // ⑤ 複数受音点の一括可聴化
    struct BatchJob {
        int     row;       // receivers のインデックス (= 表の行)
        QString rirPath;   // その受音点の RIR WAV
        QString outPath;   // 書き出し先 (出力先フォルダ + 命名規則)
    };
    QTableWidget *m_batchTable  = nullptr;
    QPushButton  *m_autoAssignBtn = nullptr;  // 📂 フォルダから自動割当
    QCheckBox    *m_autoOnlyUnset = nullptr;  // 未設定の行のみ (既定 ON)
    QLineEdit    *m_batchOutDir = nullptr;
    QPushButton  *m_batchRunBtn = nullptr;
    QPushButton  *m_batchCancelBtn = nullptr;
    QLabel       *m_batchStatus = nullptr;
    QVector<BatchJob> m_batchJobs;
    bool m_batchBusy   = false;   // 一括実行中 (単発実行と排他)
    bool m_batchCancel = false;   // 中断要求 (実行中の行の完了後に停止)
    int  m_batchDone   = 0;       // 書き出し済み行数 (今回の一括実行)
    // 行 → 直近の結果 (状態文字列 / ツールチップ / 出力パス)。
    // rebuildBatchTable が表を作り直しても結果表示と試聴ボタンを保つ
    QHash<int, QString> m_batchRowText;
    QHash<int, QString> m_batchRowTip;
    QHash<int, QString> m_batchOutFiles;
};

} // namespace ofd
