// AcousticSolverTab.h — 🔌 音響ソルバ連携 (元 mock: opera-analysis.jsx
// AcousticSolverTab)。
//
// 外部音響ソルバー (ADR-0007 の出力契約) を AcousticRunner (QProcess 疎結合)
// で起動する画面。バックエンド 5 値と実行設定は .ofdx の
// acoustic/opera_analysis/solver に永続化する。契約検証済みの rir.wav は
// operaAcoustic().rirPath へ反映して RIR 分析パイプラインに渡す。
#pragma once
#include <QScrollArea>

#include "../kernel/AcousticRunner.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QWidget;

namespace ofd {

class Project;
class SectionBox;

class AcousticSolverTab : public QScrollArea {
    Q_OBJECT
public:
    explicit AcousticSolverTab(Project *project, QWidget *parent = nullptr);

    void apply();     // widgets → model (+ touch)
    void refresh();   // model → widgets (m_updating ガード付き)

signals:
    // 契約検証済みの rir.wav を operaAcoustic().rirPath へ書き込んだ直後に
    // 発行する。実測RIR分析タブは Project::loaded にしか繋がっておらず、
    // 実行後にタブを開いても WAV 欄が空のままになるため、MainWindow が
    // この単発イベントを RirAnalysisTab へ橋渡しする (タブ間の直接依存を
    // 作らない)。引数は設定した WAV のパス。
    void rirAssigned(const QString &path);

private:
    void updateResolution();   // 探索順どおりの解決結果をライブ表示
    // 実行前の入力準備: 現在のプロジェクトを作業ディレクトリへ .ofd + .ofdx
    // で書き出し、その .ofd の絶対パスを返す (失敗時は空 + err に理由)。
    // 併せて前回実行の契約ファイルを消す (残骸を今回の結果として拾わない)。
    // subDir が非空なら作業ディレクトリの下にその名前で掘る
    // (ハイブリッド実行で FDTD と幾何音響の契約ファイルを混ぜないため)。
    QString prepareRunInput(QString *workingDir, QString *err,
                            const QString &subDir = QString());
    void startSolver();
    // ハイブリッド実行: FDTD → 幾何音響 → 合成 → 可聴化へ設定 まで通す。
    // 2 つのソルバーを順に起動する (m_hybridPhase が段を持つ)。
    void startHybridRun();
    // ハイブリッドの 1 段だけ実行する。phase: 1 = 低域 (一括の第 1 段) /
    // 2 = 高域 (一括の第 2 段) / 3 = 低域のみ / 4 = 高域のみ。
    // 3・4 は結果を欄に入れて終わる (合成はユーザーの「▶ 合成する」)。
    void startHybridStage(int phase);
    // ハイブリッド実行の段を進める (ソルバー 1 本の終了ごとに呼ぶ)
    void advanceHybridRun(bool ok);
    void stopSolver();
    // ハイブリッド RIR 合成 (低域 FDTD + 高域 幾何音響)。ファイル 2 本を
    // acoustics::buildHybridRir へ渡し、結果を WAV へ書き出す。
    void buildHybrid();
    // 入力の揃い具合に応じて合成ボタンの有効/無効と理由 (ツールチップ) を更新
    void updateHybridUi();

    Project        *m_p;
    AcousticRunner *m_runner = nullptr;
    bool            m_updating = false;

    QComboBox    *m_backend = nullptr;
    QWidget      *m_extGroup = nullptr;    // 外部プロセス設定 (FDTD/幾何のみ表示)
    QLineEdit    *m_execPath = nullptr;
    QSpinBox     *m_threads = nullptr, *m_processes = nullptr;
    QPushButton  *m_btnRun = nullptr, *m_btnStop = nullptr;
    QLabel       *m_resolved = nullptr;    // 解決されたバイナリ or 未検出
    QLabel       *m_status = nullptr;
    QProgressBar *m_progress = nullptr;
    QPlainTextEdit *m_log = nullptr;

    // ── ハイブリッド RIR 合成 (セッション限りの入力欄 — .ofdx へは
    //    保存しない。シリアライズ出力を 1 バイトも変えないため) ────────────
    QLineEdit *m_hyLow = nullptr;    // 低域 RIR (FDTD)
    QLineEdit *m_hyHigh = nullptr;   // 高域 RIR (幾何音響)
    QLineEdit *m_hyOut = nullptr;    // 出力 RIR
    QLineEdit *m_hyCross = nullptr;  // クロスオーバー [Hz] (空 = 自動)
    QPushButton *m_hyRun = nullptr;
    QPushButton *m_hyAssign = nullptr;
    QLabel *m_hyResult = nullptr;
    QString m_hyLastOut;             // 直近の合成結果 (可聴化へ渡す用)
    QPushButton *m_hyRunAll = nullptr;  // 2 ソルバー実行 → 合成まで一括
    QPushButton *m_hyRunLow = nullptr;  // 低域 (FDTD) だけ実行
    QPushButton *m_hyRunHigh = nullptr; // 高域 (幾何音響) だけ実行
    // ハイブリッド実行の段: 0 = 実行していない / 1 = FDTD (一括) /
    // 2 = 幾何音響 (一括) / 3 = 低域のみ / 4 = 高域のみ。
    // 段が進行中は rirReady を可聴化へ流さない (中間 RIR を最終結果として
    // 扱わないため — 最後の合成結果だけを設定する)。
    int m_hybridPhase = 0;
};

} // namespace ofd
