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

    // OperaAcousticSettings → コアの RirAnalyzerConfig 変換。
    // calibrationOffsetDb は calibrationState==Absolute のときだけ渡す
    // (それ以外では 0 — 未校正のまま SPL がずれるのを防ぐ)。
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
    // (voiceType / calibrationState / calibrationOffsetDb /
    //  vocalF0MinHz / vocalF0MaxHz)。
    // 校正オフセットの扱いは toAnalyzerConfig と同じ (Absolute 時のみ)。
    static acoustics::VocalAnalyzerConfig
    toVocalConfig(const OperaAcousticSettings &settings);

    // 歌唱 WAV を読み込み → チャンネル選択 → VocalAnalyzer で分析する
    static acoustics::AcousticResult<acoustics::VocalAnalysisResult>
    analyzeVocalFile(const QString &path, const OperaAcousticSettings &settings);

    // ── 可聴化 (フェーズ4) ──────────────────────────────────────────────────
    // fs 不一致で RIR をドライ側 fs へリサンプリングしたことの通知 (UI 表示用。
    // 黙って変換しない — 呼び出し側は resampled のとき必ずユーザーに明示する)
    struct RirResampleNote {
        bool   resampled;   // RIR をリサンプリングしたか
        // 下の 2 つは変換の有無に関わらず埋まる (RIR の帯域は変換しても
        // 広がらないため、呼び出し側が帯域制限の注記を出せるように)。
        double fromHz;      // RIR ファイル本来の fs
        double toHz;        // 出力の fs (= ドライの fs)
        RirResampleNote() : resampled(false), fromHz(0.0), toHz(0.0) {}
    };

    // ── ソルバー metadata.json (ADR-0007 契約) の読み取り ──────────────────
    // RIR の**物理的に有効な帯域**は fs/2 ではなく、格子分解能で決まる
    // fmax = c/(10·dx) (OpenAcoustics)。この値と音源パルス (σ, t0) が
    // metadata.json に出ているので、帯域の明示とハイブリッド合成の
    // 逆フィルタに使う。欠落キーは 0 のまま (valid は必須キーの有無で判定)。
    struct SolverMetadata {
        bool    valid;          // metadata.json を読めて sample_rate があった
        double  sampleRateHz;   // "sample_rate"
        double  sourceFmaxHz;   // "source.fmax_hz" — 有効帯域の上限
        double  sourceSigmaS;   // "source.sigma_s" — ガウシアン微分パルス
        double  sourceT0S;      // "source.t0_s"    — 音源の遅延
        double  gridDxM;        // "grid.dx_m"
        double  tSabineS;       // "t_sabine_s" (-1 = 無限大)
        // "valid_band_hz": [lo, hi] — 幾何音響ソルバー (ofdx_acoustic_ga) が
        // 申告する有効帯域。lo は Schroeder 周波数で、これより下は幾何音響の
        // 前提が崩れる。FDTD 側は出さないので 0 のまま
        double  validBandLoHz;
        double  validBandHiHz;
        QString method;         // "method" (幾何音響のみ。FDTD は空)
        QString solver;         // "solver"
        SolverMetadata()
            : valid(false), sampleRateHz(0.0), sourceFmaxHz(0.0),
              sourceSigmaS(0.0), sourceT0S(0.0), gridDxM(0.0),
              tSabineS(0.0), validBandLoHz(0.0), validBandHiHz(0.0),
              method(), solver() {}
    };

    // metadata.json を直接読む
    static SolverMetadata readSolverMetadata(const QString &metadataPath);
    // rir.wav と同じディレクトリの metadata.json を探して読む
    // (ADR-0007 の契約でソルバーは両方を同じ作業ディレクトリへ出す)。
    // 見つからなければ valid = false のまま返す (実測 RIR など)。
    static SolverMetadata metadataForRir(const QString &rirPath);

    // dry WAV × rir WAV を畳み込み、outputPath に float32 WAV で書き出す。
    // gainMode: 0=そのまま 1=推奨ゲイン (suggestedGainDb) を適用。
    // gainMode=1 のとき、返す ConvolutionInfo の outputPeak /
    // outputPeakDbfs / clippedSampleCount / clipped は「書き出した
    // (ゲイン適用後の) サンプル」で測り直した値。suggestedGainDb は
    // 適用したゲイン量を保持する。
    // 自動正規化はしない。サンプルレート不一致は RIR をドライ側 fs へ
    // リサンプリング (acoustics::resampleBuffer — Kaiser 窓 sinc、負債 #12)
    // して続行し、outResample で通知する (音源素材のドライは変えない)。
    // fs 自体が不正で変換できない場合は従来どおりエラー。
    // outDry / outWet / outSampleRate が非 null なら A/B 波形プロット用に
    // ドライ (選択後モノ) / ウェット先頭チャンネル (書き出し値) を返す。
    static acoustics::AcousticResult<acoustics::ConvolutionInfo>
    convolveFiles(const QString &dryPath, const QString &rirPath,
                  const QString &outputPath, int gainMode,
                  std::vector<double> *outDry = nullptr,
                  std::vector<double> *outWet = nullptr,
                  double *outSampleRate = nullptr,
                  RirResampleNote *outResample = nullptr);
};

} // namespace ofd
