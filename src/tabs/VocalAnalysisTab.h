// VocalAnalysisTab.h — 歌声分析タブ (フェーズ3)。
//
// 単一・無伴奏・モノフォニック歌唱の WAV を C++14 音響コア (VocalAnalyzer)
// で物理量として分析する。声種は YIN の F0 探索範囲プリセットの選択にのみ
// 使用し、診断的な結論 (声区判定・巧拙・適性など) は表示しない (ADR-0006)。
//   ① 入力     — 歌唱 WAV (voicePath 再利用) / 声種 / F0 範囲上書き /
//                 校正状態の表示 (編集は実測RIR分析タブ)
//   ② 実行     — QtAcousticAdapter::analyzeVocalFile で同期分析
//   ③ 結果     — 指標表 (F0 統計 / ビブラート / HNR / スペクトル重心 /
//                 歌手フォルマント比 / 帯域エネルギー / Leq・ピーク)。
//                 無効値は「算出不可 (理由)」、SPL は未校正時必ず算出不可
//   ④ プロット — F0 軌跡 (無声区間は欠落) + LTAS
//   ⑤ 出力     — CSV / JSON 保存 (AcousticResultModel 経由)
// 設定は .ofdx の acoustic/opera_analysis (voice_file / voice_type / vocal)
// に永続化される。
#pragma once
#include <QScrollArea>

#include "../acoustics/core/VocalAnalyzer.h"

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

namespace ofd {

class Project;
class MiniPlot;

class VocalAnalysisTab : public QScrollArea {
    Q_OBJECT
public:
    explicit VocalAnalysisTab(Project *project, QWidget *parent = nullptr);

private slots:
    void refresh();        // model → widgets
    void apply();          // widgets → model
    void browseVoice();
    void runAnalysis();
    void exportCsv();
    void exportJson();

private:
    void showResult(const acoustics::VocalAnalysisResult &result);
    void clearResult(const QString &statusText);

    Project *m_p;
    bool     m_updating = false;

    // ① 入力
    QLineEdit      *m_voicePath = nullptr;
    QComboBox      *m_voiceType = nullptr;
    QDoubleSpinBox *m_f0Min = nullptr;
    QDoubleSpinBox *m_f0Max = nullptr;
    QLabel         *m_calibInfo = nullptr;   // 表示のみ (編集は実測RIR分析タブ)

    // ② 実行
    QPushButton *m_runBtn = nullptr;
    QLabel      *m_status = nullptr;

    // ③ 結果
    QTableWidget *m_metricTable = nullptr;
    QLabel       *m_warnings = nullptr;

    // ④ プロット
    MiniPlot *m_f0Plot = nullptr;
    MiniPlot *m_ltasPlot = nullptr;

    // ⑤ 出力
    QPushButton *m_csvBtn = nullptr;
    QPushButton *m_jsonBtn = nullptr;

    // 直近の分析結果 (出力用)
    bool m_hasResult = false;
    acoustics::VocalAnalysisResult m_result;
};

} // namespace ofd
