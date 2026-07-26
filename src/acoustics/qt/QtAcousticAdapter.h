// QtAcousticAdapter.h — Qt モデル (OperaAcousticSettings) と C++14 音響コア
// (src/acoustics/core, src/acoustics/io) を橋渡しする static 関数群。
//
// 方針:
//   - コアの結果型 (RirAnalysisResult / SchroederResult / AudioBuffer) は
//     Qt 型に包まず「そのまま」返す。QString 化は AcousticResultModel が担う。
//   - QString パス → std::string 変換と、チャンネル選択 (L/R/平均モノ) の
//     適用はここで行う。
#pragma once
#include <QString>
#include <vector>

#include "../core/ConvolutionEngine.h"
#include "../core/RirAnalyzer.h"
#include "../core/SchroederDecay.h"
#include "../core/VocalAnalyzer.h"
#include "../io/WavReader.h"
#include "../../core/Project.h"

namespace ofd {

class QtAcousticAdapter {
public:
    // WAV 読み込み (QString → std::string 変換のみ、正規化なし)
    static acoustics::AcousticResult<acoustics::AudioBuffer>
    readWav(const QString &path);

    // チャンネル選択: channelMode 0=L 1=R 2=平均モノ。
    // 指定チャンネルが無い場合は先頭チャンネルにフォールバックする。
    static std::vector<double>
    selectChannel(const acoustics::AudioBuffer &buffer, int channelMode);

    // OperaAcousticSettings → コアの RirAnalyzerConfig 変換
    static acoustics::RirAnalyzerConfig
    toAnalyzerConfig(const OperaAcousticSettings &settings);

    // 選択済み 1ch 信号を分析する
    static acoustics::AcousticResult<acoustics::RirAnalysisResult>
    analyze(const std::vector<double> &samples, double sampleRateHz,
            const OperaAcousticSettings &settings);

    // 便宜関数: settings.rirPath を読み込み → チャンネル選択 → 分析。
    // outSamples / outSampleRate が非 null なら分析に使った信号を返す
    // (波形・減衰カーブのプロット用)。
    static acoustics::AcousticResult<acoustics::RirAnalysisResult>
    analyzeFile(const OperaAcousticSettings &settings,
                std::vector<double> *outSamples = nullptr,
                double *outSampleRate = nullptr);

    // 広帯域 Schroeder 減衰カーブ (プロット用)
    static acoustics::SchroederResult
    decayCurve(const std::vector<double> &samples, double sampleRateHz,
               const OperaAcousticSettings &settings);

    // ── 歌声分析 (フェーズ3) ────────────────────────────────────────────────
    // OperaAcousticSettings → コアの VocalAnalyzerConfig 変換
    // (voiceType / calibrationState / vocalF0MinHz / vocalF0MaxHz)。
    // 校正オフセット (dBFS→SPL) は未導入のため 0 のまま。
    static acoustics::VocalAnalyzerConfig
    toVocalConfig(const OperaAcousticSettings &settings);

    // 歌唱 WAV を読み込み → チャンネル選択 → VocalAnalyzer で分析する
    static acoustics::AcousticResult<acoustics::VocalAnalysisResult>
    analyzeVocalFile(const QString &path, const OperaAcousticSettings &settings);

    // ── 可聴化 (フェーズ4) ──────────────────────────────────────────────────
    // dry WAV × rir WAV を畳み込み、outputPath に float32 WAV で書き出す。
    // gainMode: 0=そのまま 1=推奨ゲイン (suggestedGainDb) を適用。
    // 自動正規化はしない。サンプルレート不一致はリサンプリングせずエラー。
    // outDry / outWet / outSampleRate が非 null なら A/B 波形プロット用に
    // ドライ (選択後モノ) / ウェット先頭チャンネル (書き出し値) を返す。
    static acoustics::AcousticResult<acoustics::ConvolutionInfo>
    convolveFiles(const QString &dryPath, const QString &rirPath,
                  const QString &outputPath, int gainMode,
                  std::vector<double> *outDry = nullptr,
                  std::vector<double> *outWet = nullptr,
                  double *outSampleRate = nullptr);
};

} // namespace ofd
