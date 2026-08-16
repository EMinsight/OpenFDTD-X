// RirAnalysisTab.h — 実測 RIR 分析タブ (最小版)。
//
// 実測した室内インパルス応答 (WAV) を C++14 音響コア (RirAnalyzer) で
// ISO 3382-1 分析する。既存の RoomAcousticsTab (統計推定) や、将来の
// FDTD シミュレーション RIR 分析とは別系統。
//   ① 入力     — RIR WAV / チャンネル / 校正状態 / 校正オフセット (Absolute
//                 のときのみ有効) / 直接音方式 / 帯域モード / ノイズ補正 /
//                 最小動的範囲 (OperaAcousticSettings と双方向)
//   ② 実行     — QtAcousticAdapter 経由で同期分析
//   ③ 結果     — 指標表 (EDT/T20/T30/C50/C80/D50/Ts × 帯域) + 警告リスト
//   ④ プロット — 波形 + Schroeder 減衰カーブ + 初期反射一覧表
//   ⑤ 出力     — CSV / JSON 保存 (AcousticResultModel 経由)
// 設定は .ofdx の acoustic/opera_analysis に永続化される。
#pragma once
#include <QScrollArea>
#include <vector>

#include "../acoustics/core/RirAnalyzer.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

namespace ofd {

class Project;
class MiniPlot;

class RirAnalysisTab : public QScrollArea {
    Q_OBJECT
public:
    explicit RirAnalysisTab(Project *project, QWidget *parent = nullptr);

    // 直近の分析結果への読み取り専用アクセス (一括レポート出力が使う)。
    // 未実行なら hasResult() == false — result() の中身は意味を持たない。
    bool hasResult() const { return m_hasResult; }
    const acoustics::RirAnalysisResult &result() const { return m_result; }

    // 音響ソルバ連携タブが契約検証済みの rir.wav を
    // operaAcoustic().rirPath へ設定したときに MainWindow が呼ぶ
    // (AcousticSolverTab::rirAssigned の中継先)。WAV 欄を最新化し、
    // 次に「▶ 分析」を押せばよいことをヒントで示す。
    void applySolverRir(const QString &path);

private slots:
    void refresh();        // model → widgets
    void apply();          // widgets → model
    void browseRir();
    void browseStrengthRef();   // G の基準録音 (自由音場 IR) を選ぶ
    void runAnalysis();
    void exportCsv();
    void exportJson();

private:
    void showResult(const acoustics::RirAnalysisResult &result,
                    const std::vector<double> &samples, double sampleRateHz);
    void clearResult(const QString &statusText);
    void updateCalibOffsetEnabled();   // Absolute 時のみオフセット欄を有効化
    void updateStrengthRefEnabled();   // G 基準の入力欄をモードに応じて有効化

    Project *m_p;
    bool     m_updating = false;

    // ① 入力
    QLineEdit      *m_rirPath = nullptr;
    QLabel         *m_solverHint = nullptr;   // ソルバ実行結果を設定した旨
    QComboBox      *m_channel = nullptr;
    QComboBox      *m_calibration = nullptr;
    QDoubleSpinBox *m_calibOffset = nullptr;
    QLabel         *m_calibOffsetLabel = nullptr;
    QComboBox      *m_directMethod = nullptr;
    QComboBox      *m_bandMode = nullptr;
    QCheckBox      *m_noiseCorr = nullptr;
    QDoubleSpinBox *m_minDr = nullptr;
    // G (音の強さ) の基準
    QComboBox      *m_gRefMode = nullptr;
    QLineEdit      *m_gRefFile = nullptr;
    QPushButton    *m_gRefBrowse = nullptr;
    QLabel         *m_gRefFileLabel = nullptr;
    QDoubleSpinBox *m_gRefLevel = nullptr;
    QLabel         *m_gRefLevelLabel = nullptr;
    QDoubleSpinBox *m_gRefDistance = nullptr;
    QLabel         *m_gRefDistanceLabel = nullptr;

    // ② 実行
    QPushButton *m_runBtn = nullptr;
    QLabel      *m_status = nullptr;

    // ③ 結果
    QTableWidget *m_metricTable = nullptr;
    QLabel       *m_warnings = nullptr;
    QLabel       *m_stiLabel = nullptr;
    QLabel       *m_strengthLabel = nullptr;

    // ④ プロット + 反射一覧
    MiniPlot     *m_wavePlot = nullptr;
    MiniPlot     *m_decayPlot = nullptr;
    QTableWidget *m_reflTable = nullptr;

    // ⑤ 出力
    QPushButton *m_csvBtn = nullptr;
    QPushButton *m_jsonBtn = nullptr;

    // 直近の分析結果 (出力用)
    bool m_hasResult = false;
    acoustics::RirAnalysisResult m_result;
};

} // namespace ofd
