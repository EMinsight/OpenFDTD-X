// RirAnalyzer.h — 室内インパルス応答 (RIR) 分析の統合パイプライン。
// Qt 非依存 / C++14。
//
// 処理順: 前処理 (DC 除去 / 非有限値検出 / クリッピング検出 / 入力長・
// 動的範囲検査) → 直接音検出 → 帯域分割 → Schroeder 減衰 → ISO 3382-1
// 指標 → 反射音検出。
#pragma once
#include <cstddef>
#include <string>
#include <vector>

#include "AcousticError.h"
#include "AcousticMetrics.h"
#include "AnalysisQuality.h"
#include "ArrayView.h"
#include "AudioBuffer.h"
#include "BandFilter.h"
#include "DirectSoundDetector.h"
#include "ReflectionDetector.h"

namespace ofd {
namespace acoustics {

// 校正状態。Uncalibrated / Relative では絶対 SPL 系の指標を valid にしない。
enum class CalibrationState {
    Absolute,     // フルスケール→SPL の換算が既知 (calibrationOffsetDb)
    Relative,     // 相対レベルのみ有効
    Uncalibrated  // 未校正
};

struct RirAnalyzerConfig {
    CalibrationState calibration; // 既定 Uncalibrated
    double calibrationOffsetDb;   // Absolute 時: dBFS → dB SPL のオフセット
    BandSet bandSet;              // 既定 Compat6
    bool zeroPhaseFiltering;      // 帯域分割をゼロ位相で行う (既定 true)
    bool removeDc;                // DC (平均値) 除去 (既定 true)
    double minDurationSeconds;    // 入力長の下限 (既定 0.05 s)
    double minDynamicRangeDb;     // これ未満はエラー (既定 10 dB)
    double warnDynamicRangeDb;    // これ未満は警告 (既定 30 dB)
    double clipThreshold;         // クリッピング判定振幅 (既定 0.999)
    int clipRunLength;            // 連続数 (既定 3)
    DirectSoundConfig directSound;
    MetricsOptions metrics;
    ReflectionDetectorConfig reflections;

    RirAnalyzerConfig()
        : calibration(CalibrationState::Uncalibrated), calibrationOffsetDb(0.0),
          bandSet(BandSet::Compat6), zeroPhaseFiltering(true), removeDc(true),
          minDurationSeconds(0.05), minDynamicRangeDb(10.0),
          warnDynamicRangeDb(30.0), clipThreshold(0.999), clipRunLength(3),
          directSound(), metrics(), reflections() {}
};

// 前処理の結果情報
struct PreprocessInfo {
    double dcOffset;             // 除去した DC 成分
    bool dcRemoved;
    std::size_t nonFiniteCount;  // NaN / Inf の個数 (>0 ならエラー)
    bool clippingDetected;       // |x| > clipThreshold が clipRunLength 連続
    int clippedRunCount;         // クリッピング連続区間の数
    double noiseFloorDb;         // 末尾区間ノイズフロア [dBFS]
    double peakDb;               // ピークレベル [dBFS]
    double dynamicRangeDb;       // peakDb - noiseFloorDb
    std::size_t sampleCount;
    double durationSeconds;

    PreprocessInfo()
        : dcOffset(0.0), dcRemoved(false), nonFiniteCount(0),
          clippingDetected(false), clippedRunCount(0), noiseFloorDb(-300.0),
          peakDb(-300.0), dynamicRangeDb(0.0), sampleCount(0),
          durationSeconds(0.0) {}
};

// 帯域ごとの指標
struct BandMetricsResult {
    Band band;
    bool filterOk;             // フィルタ設計に成功したか
    std::string filterWarning; // 失敗理由 (成功時は空)
    AcousticMetricsSet metrics;

    BandMetricsResult() : band(), filterOk(false), filterWarning(), metrics() {}
};

struct RirAnalysisResult {
    PreprocessInfo preprocess;
    DirectSoundResult directSound;
    std::vector<BandMetricsResult> bands;
    std::vector<ReflectionEvent> reflections;
    ReflectionTimeSummary reflectionSummary;
    MetricValue absoluteSplDb; // ピーク絶対 SPL。Absolute 校正時のみ valid
    AnalysisQuality overallQuality;
    std::vector<std::string> warnings;

    RirAnalysisResult()
        : preprocess(), directSound(), bands(), reflections(),
          reflectionSummary(), absoluteSplDb(),
          overallQuality(AnalysisQuality::Invalid), warnings() {}
};

class RirAnalyzer {
public:
    explicit RirAnalyzer(const RirAnalyzerConfig &config = RirAnalyzerConfig());

    AcousticResult<RirAnalysisResult> analyze(ArrayView<const double> rir,
                                              double sampleRateHz) const;
    // AudioBuffer の指定チャンネルを分析する
    AcousticResult<RirAnalysisResult> analyze(const AudioBuffer &buffer,
                                              std::size_t channel = 0) const;

    const RirAnalyzerConfig &config() const { return m_config; }

private:
    RirAnalyzerConfig m_config;
};

} // namespace acoustics
} // namespace ofd
