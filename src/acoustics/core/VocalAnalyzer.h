// VocalAnalyzer.h — 単一歌唱音声 (無伴奏・モノフォニック) の分析。
// Qt 非依存 / C++14。
//
// 算出内容:
//   F0 軌跡        : YIN 法 (de Cheveigné & Kawahara 2002)
//                    差分関数 → CMNDF → 絶対閾値 → 放物線補間
//   F0 統計        : 有声フレームの中央値 / 平均 / 範囲、
//                    ピッチ安定性 = 中央値からの偏差の RMS [cent]
//   ビブラート     : rate [Hz] / depth [cent, 片振幅]
//   LTAS           : Welch 法 (Hann 窓, 50% オーバーラップ, パワー平均)
//   スペクトル重心 : パワー重み付き平均周波数 [Hz]
//   HNR            : 自己相関法 (Boersma 1993:
//                    HNR = 10·log10(r(T0) / (1 - r(T0))) のフレーム平均) [dB]
//   倍音レベル     : F0 中央値の H1..H8 近傍 (±3%) の LTAS ピーク (H1 相対 dB)
//   帯域エネルギー : 全帯域 / 0-2k / 2.0-2.5k / 2.5-3.15k / 3.15-4.0k / 2.0-4.0k
//   歌手フォルマント指標 : 2-4 kHz / 0-2 kHz のエネルギー比 [dB]
//                    (Sundberg 1974 の singer's formant 概念に基づく LTAS 比率)
//   RMS / ピーク [dBFS]、Leq (全長)
//
// 注意: VoiceType (声種) は YIN の F0 探索範囲プリセットの選択にのみ使用する。
// 声種から医学的・教育的な結論 (声区の診断、発声の巧拙、適性判定など) を
// 導く機能は意図的に実装しない。本モジュールは物理量の測定のみを行う。
//
// SPL 系の絶対値は CalibrationState::Absolute かつ校正オフセット指定時のみ
// valid とし、それ以外は dBFS の相対値のみを返す (自動正規化はしない)。
#pragma once
#include <cstddef>
#include <string>
#include <vector>

#include "AcousticError.h"
#include "AnalysisQuality.h"
#include "ArrayView.h"
#include "AudioBuffer.h"
#include "BandFilter.h"
#include "RirAnalyzer.h" // CalibrationState

namespace ofd {
namespace acoustics {

// 声種 (F0 探索範囲プリセットの選択にのみ使用。診断等には用いない)
enum class VoiceType {
    Soprano,      // 220–1400 Hz
    MezzoSoprano, // 180–1000 Hz
    Contralto,    // 150–800 Hz
    Tenor,        // 120–700 Hz
    Baritone,     //  90–500 Hz
    Bass,         //  70–400 Hz
    Unknown       //  60–1500 Hz
};

struct F0SearchRange {
    double minHz;
    double maxHz;

    F0SearchRange() : minHz(0.0), maxHz(0.0) {}
    F0SearchRange(double lo, double hi) : minHz(lo), maxHz(hi) {}
};

// 声種 → F0 探索範囲プリセット
F0SearchRange f0SearchRangeFor(VoiceType type);

struct VocalAnalyzerConfig {
    VoiceType voiceType;        // F0 探索範囲プリセット (既定 Unknown)
    double f0MinHz;             // > 0 ならプリセットを上書き
    double f0MaxHz;             // > 0 ならプリセットを上書き
    double yinThreshold;        // CMNDF 絶対閾値 (既定 0.15)
    double frameSeconds;        // <= 0 で自動: max(0.040, 2 / f0Min) (既定 0)
    double hopSeconds;          // フレームホップ (既定 0.010)
    double noiseGateDbfs;       // フレーム RMS がこれ未満なら無声 (既定 -70)

    double vibratoMinHz;            // ビブラート探索帯域下限 (既定 3 Hz)
    double vibratoMaxHz;            // 同上限 (既定 9 Hz)
    double vibratoDetrendSeconds;   // 移動平均デトレンド長 (既定 0.25 s)
    double vibratoMinSegmentSeconds;// 分析に必要な最短有声区間 (既定 1.0 s)
    double vibratoMinDepthCents;    // これ未満の深さは無効扱い (既定 5 cent)
    double vibratoMinPeakRatio;     // ピーク / 帯域中央値パワー比 (既定 4)

    std::size_t ltasFftLength;  // LTAS の FFT 長 (2 の冪、既定 4096)

    CalibrationState calibration; // 既定 Uncalibrated
    double calibrationOffsetDb;   // Absolute 時: dBFS → dB SPL のオフセット

    VocalAnalyzerConfig()
        : voiceType(VoiceType::Unknown), f0MinHz(0.0), f0MaxHz(0.0),
          yinThreshold(0.15), frameSeconds(0.0), hopSeconds(0.010),
          noiseGateDbfs(-70.0), vibratoMinHz(3.0), vibratoMaxHz(9.0),
          vibratoDetrendSeconds(0.25), vibratoMinSegmentSeconds(1.0),
          vibratoMinDepthCents(5.0), vibratoMinPeakRatio(4.0),
          ltasFftLength(4096), calibration(CalibrationState::Uncalibrated),
          calibrationOffsetDb(0.0) {}
};

// F0 軌跡の 1 フレーム
struct F0Frame {
    double timeSeconds; // フレーム中心時刻 [s]
    double f0Hz;        // 無声時は 0
    bool voiced;
    double cmndfMin;    // CMNDF 最小値 (有声性の目安)
    double rmsDbfs;     // フレーム RMS [dBFS]

    F0Frame()
        : timeSeconds(0.0), f0Hz(0.0), voiced(false), cmndfMin(1.0),
          rmsDbfs(-300.0) {}
};

struct VibratoResult {
    MetricValue rateHz;     // ビブラート速度 [Hz]
    MetricValue depthCents; // 深さ [cent, 片振幅]
    bool valid;             // 両指標が有効か
    std::string warning;    // 無効・品質低下の理由
    std::size_t segmentStartFrame; // 分析に使った有声区間 (f0Track 上の添字)
    std::size_t segmentFrameCount;

    VibratoResult()
        : rateHz(), depthCents(), valid(false), warning(),
          segmentStartFrame(0), segmentFrameCount(0) {}
};

// 長時間平均スペクトル (LTAS)。levelsDb は正弦波実効値基準:
// フルスケール正弦波 (振幅 1.0) のビン付近が約 -3 dB になるスケール。
struct LtasResult {
    std::vector<double> frequenciesHz;
    std::vector<double> levelsDb;
    std::size_t frameCount; // 平均に使ったフレーム数
    bool valid;
    std::string warning;

    LtasResult()
        : frequenciesHz(), levelsDb(), frameCount(0), valid(false), warning() {}
};

// 帯域エネルギー (LTAS パワー積算による RMS dB)
struct BandEnergyValue {
    Band band;
    MetricValue levelDb;

    BandEnergyValue() : band(), levelDb() {}
};

struct VocalAnalysisResult {
    // F0 軌跡と実際に使ったパラメータ
    std::vector<F0Frame> f0Track;
    double frameSeconds;    // 実際のフレーム長 [s]
    double hopSeconds;      // 実際のホップ [s]
    double f0SearchMinHz;   // 実際の探索範囲
    double f0SearchMaxHz;

    std::size_t totalFrameCount;
    std::size_t voicedFrameCount;
    double voicedRatio;

    // F0 統計 (有声フレームのみ。有声フレームなしなら invalid)
    MetricValue f0MedianHz;
    MetricValue f0MeanHz;
    MetricValue f0MinHz;
    MetricValue f0MaxHz;
    MetricValue pitchStabilityCents; // 中央値からの偏差の RMS [cent]

    VibratoResult vibrato;
    LtasResult ltas;
    MetricValue spectralCentroidHz;
    MetricValue hnrDb;
    // H1..H8 (添字 0..7)。値は H1 相対 [dB] (H1 = 0 dB)
    std::vector<MetricValue> harmonicLevelsDb;
    std::vector<BandEnergyValue> bandEnergies;
    MetricValue singerFormantRatioDb; // 10·log10(P(2-4k) / P(0-2k)) [dB]

    // レベル (dBFS は常に valid、SPL は Absolute 校正時のみ valid)
    MetricValue peakDbfs;
    MetricValue rmsDbfs;
    MetricValue leqDbfs;
    MetricValue leqSplDb;
    MetricValue peakSplDb;

    AnalysisQuality overallQuality;
    std::vector<std::string> warnings;

    VocalAnalysisResult()
        : f0Track(), frameSeconds(0.0), hopSeconds(0.0), f0SearchMinHz(0.0),
          f0SearchMaxHz(0.0), totalFrameCount(0), voicedFrameCount(0),
          voicedRatio(0.0), f0MedianHz(), f0MeanHz(), f0MinHz(), f0MaxHz(),
          pitchStabilityCents(), vibrato(), ltas(), spectralCentroidHz(),
          hnrDb(), harmonicLevelsDb(), bandEnergies(), singerFormantRatioDb(),
          peakDbfs(), rmsDbfs(), leqDbfs(), leqSplDb(), peakSplDb(),
          overallQuality(AnalysisQuality::Invalid), warnings() {}
};

class VocalAnalyzer {
public:
    explicit VocalAnalyzer(
        const VocalAnalyzerConfig &config = VocalAnalyzerConfig());

    AcousticResult<VocalAnalysisResult> analyze(ArrayView<const double> x,
                                                double sampleRateHz) const;
    // AudioBuffer の指定チャンネルを分析する
    AcousticResult<VocalAnalysisResult> analyze(const AudioBuffer &buffer,
                                                std::size_t channel = 0) const;

    const VocalAnalyzerConfig &config() const { return m_config; }

private:
    VocalAnalyzerConfig m_config;
};

} // namespace acoustics
} // namespace ofd
