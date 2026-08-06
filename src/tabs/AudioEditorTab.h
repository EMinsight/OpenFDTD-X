// AudioEditorTab.h — 🎚 音響編集・解析 (DAW 風波形/スペクトル編集)
// (元 mock: audio-editor.jsx + audio-editor-ext.jsx)。
//
// 読込 (WAV) / 信号生成 / 編集 / エフェクト / 解析 / WAV 書出を本体内で実行
// する。DSP は src/audio/AudioEditEngine (Qt 非依存) に分離し、このタブは
// UI と undo スタック (12 段) のみを持つ。
// アプリ内リアルタイム再生・録音は未対応 (既存方針 — AuralizationTab 参照):
// 再生は書き出した WAV を OS のプレーヤーで開く。
// プロジェクトモデルとは独立の作業ツール (編集状態は保存しない)。
#pragma once
#include <QScrollArea>
#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

#include "../audio/AudioEditEngine.h"

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QTableWidget;

namespace ofd {

class AudioWaveformView;
class MiniPlot;
class Project;

class AudioEditorTab : public QScrollArea {
    Q_OBJECT
public:
    explicit AudioEditorTab(Project *project, QWidget *parent = nullptr);

    // タブ規約 (widgets→model / model→widgets)。このタブはモデル非結合の
    // 作業ツールのため、どちらも何もしない
    void apply() {}
    void refresh() {}

private:
    // 「使い方 / 代表的な手順」表をドメインに合わせて作り直す
    void rebuildHowTo();
    QTableWidget *m_howTable = nullptr;
    bool m_howToIsUnderwater = false;

    // 現在バッファを置き換え、直前の状態を undo スタックへ積む (最大 12 段)
    void pushBuffer(audioedit::AudioBuffer next, const QString &status);
    void undoLast();
    // 選択範囲 [a, z)。未選択は (0, 全長)
    std::pair<std::size_t, std::size_t> range() const;
    bool hasBuf() const { return m_buf.sampleCount() > 0; }

    void setStatus(const QString &s);
    void updateInfo();          // ch/fs/長さ/選択の表示と各ボタンの有効化
    // 重い処理 (リバーブ/ピッチ/ストレッチ/NR) を QThread で非同期実行する
    // (gui.md: 秒単位の処理を GUI スレッドで同期実行しない)。op は
    // 呼び出し時にバッファをコピーで捕捉した純関数であること。
    void runHeavy(const std::function<audioedit::AudioBuffer()> &op,
                  const QString &doneStatus);
    void loadWav();
    void exportWav();
    void playViaSystemPlayer();
    void generateSignal();
    void runAnalysis();      // 計算は QThread、結果表示は showAnalysis
    void showAnalysis(const audioedit::LevelMetrics &m,
                      const std::vector<audioedit::SpectrumPoint> &spec,
                      const audioedit::LoudnessMetrics &loud,
                      const std::vector<audioedit::OctaveBand> &bands);
    audioedit::WindowKind currentWindow() const;

    Project *m_p;

    // 編集状態
    audioedit::AudioBuffer              m_buf;
    std::vector<audioedit::AudioBuffer> m_undo;
    std::vector<double>                 m_noiseProfile;
    bool                                m_busy = false;  // 非同期処理中

    // ヘッダ部
    AudioWaveformView *m_view = nullptr;
    QComboBox   *m_viewMode = nullptr;
    QLabel      *m_info = nullptr;
    QLabel      *m_status = nullptr;
    QPushButton *m_btnPlay = nullptr, *m_btnUndo = nullptr,
                *m_btnExport = nullptr, *m_btnClearSel = nullptr;

    // 信号生成
    QComboBox      *m_genKind = nullptr;
    QDoubleSpinBox *m_genF1 = nullptr, *m_genF2 = nullptr,
                   *m_genDur = nullptr, *m_genAmp = nullptr;
    QLabel         *m_genHint = nullptr;

    // エフェクトのパラメータ
    QDoubleSpinBox *m_eqF = nullptr, *m_eqQ = nullptr, *m_eqG = nullptr;
    QDoubleSpinBox *m_thr = nullptr, *m_ratio = nullptr;
    QDoubleSpinBox *m_dly = nullptr, *m_fb = nullptr, *m_mix = nullptr;
    QDoubleSpinBox *m_revRT = nullptr, *m_revMix = nullptr;
    QDoubleSpinBox *m_rate = nullptr, *m_semi = nullptr, *m_stretch = nullptr;
    QDoubleSpinBox *m_nrDb = nullptr;
    QPushButton    *m_nrLearn = nullptr, *m_nrApply = nullptr;

    // 解析
    QComboBox    *m_winCombo = nullptr;
    QLabel       *m_winInfo = nullptr;
    QTableWidget *m_metricsTable = nullptr;
    MiniPlot     *m_spectrumPlot = nullptr;
    QLabel       *m_spectrumNote = nullptr;
    QTableWidget *m_loudTable = nullptr;
    QTableWidget *m_bandTable = nullptr;

    // バッファ有効時にだけ押せるボタン群 (updateInfo() で一括切替)
    std::vector<QPushButton *> m_needBuf;
    // 選択範囲があるときだけ押せるボタン群
    std::vector<QPushButton *> m_needSel;
};

} // namespace ofd
