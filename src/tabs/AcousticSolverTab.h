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

private:
    void updateResolution();   // 探索順どおりの解決結果をライブ表示
    void startSolver();
    void stopSolver();

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
};

} // namespace ofd
