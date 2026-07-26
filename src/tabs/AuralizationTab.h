// AuralizationTab.h — 可聴化タブ (フェーズ4)。
//
// ドライ (無響/近接) 歌唱 WAV と実測 RIR WAV を C++14 音響コア
// (ConvolutionEngine) で畳み込み、ウェット WAV (float32) を書き出す。
//   ① 入力   — ドライ WAV / RIR WAV (実測RIR分析タブの rirPath を共用) /
//               出力先 / ゲインモード (そのまま / 推奨ゲイン適用)。
//               自動正規化は行わない
//   ② 実行   — QtAcousticAdapter::convolveFiles で同期畳み込み + WAV 書き出し
//   ③ 結果   — outputPeak / suggestedGainDb / クリップ数。fs 不一致は
//               リサンプリングせずエラー理由を表示する
//   ④ A/B    — ドライ / ウェット波形の MiniPlot 並置。アプリ内再生は
//               未対応 (書き出した WAV を外部プレイヤーで比較する) と明示
// 設定は .ofdx の acoustic/opera_analysis/auralization に永続化される。
#pragma once
#include <QScrollArea>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;

namespace ofd {

class Project;
class MiniPlot;

class AuralizationTab : public QScrollArea {
    Q_OBJECT
public:
    explicit AuralizationTab(Project *project, QWidget *parent = nullptr);

private slots:
    void refresh();        // model → widgets
    void apply();          // widgets → model
    void browseDry();
    void browseRir();
    void browseOutput();
    void runConvolution();

private:
    void clearResult(const QString &statusText);

    Project *m_p;
    bool     m_updating = false;

    // ① 入力
    QLineEdit *m_dryPath = nullptr;
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
};

} // namespace ofd
