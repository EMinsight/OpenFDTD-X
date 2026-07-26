// QtAcousticAdapter.cpp
#include "QtAcousticAdapter.h"
#include "../io/WavWriter.h"

#include <QFile>
#include <QFileInfo>
#include <algorithm>
#include <cmath>

using namespace ofd;
using namespace ofd::acoustics;

AcousticResult<AudioBuffer> QtAcousticAdapter::readWav(const QString &path)
{
    if (path.trimmed().isEmpty())
        return AcousticResult<AudioBuffer>::error(
            AcousticErrorCode::InvalidArgument, "empty file path");
    // 非 ASCII パスでもコアに届くよう、Qt 側で読み込んでメモリ経由で渡す
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return AcousticResult<AudioBuffer>::error(
            AcousticErrorCode::FileNotFound,
            std::string("cannot open: ") + path.toUtf8().constData());
    const QByteArray data = f.readAll();
    return readWavFromMemory(
        reinterpret_cast<const unsigned char *>(data.constData()),
        static_cast<std::size_t>(data.size()));
}

std::vector<double> QtAcousticAdapter::selectChannel(const AudioBuffer &buffer,
                                                     int channelMode)
{
    const std::size_t nCh = buffer.channelCount();
    if (nCh == 0) return std::vector<double>();

    if (channelMode == 2 && nCh >= 2) {           // 全チャンネル平均モノ
        const std::size_t n = buffer.sampleCount();
        std::vector<double> mono(n, 0.0);
        for (std::size_t ch = 0; ch < nCh; ++ch) {
            const std::vector<double> &src = buffer.channels[ch];
            const std::size_t m = std::min(n, src.size());
            for (std::size_t i = 0; i < m; ++i) mono[i] += src[i];
        }
        for (std::size_t i = 0; i < n; ++i) mono[i] /= double(nCh);
        return mono;
    }
    // 0=L 1=R (無ければ先頭チャンネルにフォールバック)
    std::size_t ch = (channelMode == 1) ? 1 : 0;
    if (ch >= nCh) ch = 0;
    return buffer.channels[ch];
}

RirAnalyzerConfig
QtAcousticAdapter::toAnalyzerConfig(const OperaAcousticSettings &s)
{
    RirAnalyzerConfig cfg;

    switch (s.calibrationState) {
    case 0:  cfg.calibration = CalibrationState::Absolute;     break;
    case 1:  cfg.calibration = CalibrationState::Relative;     break;
    default: cfg.calibration = CalibrationState::Uncalibrated; break;
    }
    switch (s.directSoundMethod) {
    case 0:  cfg.directSound.method = DirectSoundMethod::Peak;               break;
    case 2:  cfg.directSound.method = DirectSoundMethod::MovingRmsThreshold; break;
    default: cfg.directSound.method = DirectSoundMethod::EnvelopeThreshold;  break;
    }
    switch (s.bandMode) {
    case 1:  cfg.bandSet = BandSet::Octave63To8k;       break;
    case 2:  cfg.bandSet = BandSet::ThirdOctave100To5k; break;
    case 3:  cfg.bandSet = BandSet::SingerFormant;      break;
    default: cfg.bandSet = BandSet::Compat6;            break;
    }
    cfg.metrics.schroeder.noiseCompensation = s.noiseCorrection;
    // 「最小動的範囲」を下回る入力はエラー。警告閾値は矛盾しないよう引き上げる。
    cfg.minDynamicRangeDb = s.minimumDynamicRangeDb;
    cfg.warnDynamicRangeDb =
        std::max(cfg.warnDynamicRangeDb, s.minimumDynamicRangeDb);
    return cfg;
}

AcousticResult<RirAnalysisResult>
QtAcousticAdapter::analyze(const std::vector<double> &samples,
                           double sampleRateHz,
                           const OperaAcousticSettings &settings)
{
    const RirAnalyzer analyzer(toAnalyzerConfig(settings));
    return analyzer.analyze(ArrayView<const double>(samples), sampleRateHz);
}

AcousticResult<RirAnalysisResult>
QtAcousticAdapter::analyzeFile(const OperaAcousticSettings &settings,
                               std::vector<double> *outSamples,
                               double *outSampleRate)
{
    const AcousticResult<AudioBuffer> wav = readWav(settings.rirPath);
    if (!wav.success())
        return AcousticResult<RirAnalysisResult>::error(wav.errorCode(),
                                                        wav.message());
    std::vector<double> samples =
        selectChannel(wav.value(), settings.channelMode);
    const double fs = wav.value().sampleRateHz;
    if (outSamples) *outSamples = samples;
    if (outSampleRate) *outSampleRate = fs;
    return analyze(samples, fs, settings);
}

SchroederResult
QtAcousticAdapter::decayCurve(const std::vector<double> &samples,
                              double sampleRateHz,
                              const OperaAcousticSettings &settings)
{
    SchroederOptions opt;
    opt.noiseCompensation = settings.noiseCorrection;
    return computeSchroederDecay(ArrayView<const double>(samples),
                                 sampleRateHz, opt);
}

// ── 歌声分析 (フェーズ3) ─────────────────────────────────────────────────────
VocalAnalyzerConfig
QtAcousticAdapter::toVocalConfig(const OperaAcousticSettings &s)
{
    VocalAnalyzerConfig cfg;

    // 0..5 = Soprano..Bass, それ以外 = Unknown (enum と同順、docs §2)
    switch (s.voiceType) {
    case 0:  cfg.voiceType = VoiceType::Soprano;      break;
    case 1:  cfg.voiceType = VoiceType::MezzoSoprano; break;
    case 2:  cfg.voiceType = VoiceType::Contralto;    break;
    case 3:  cfg.voiceType = VoiceType::Tenor;        break;
    case 4:  cfg.voiceType = VoiceType::Baritone;     break;
    case 5:  cfg.voiceType = VoiceType::Bass;         break;
    default: cfg.voiceType = VoiceType::Unknown;      break;
    }
    // > 0 のときのみ声種プリセットを上書き (0 = 自動)
    cfg.f0MinHz = s.vocalF0MinHz;
    cfg.f0MaxHz = s.vocalF0MaxHz;

    switch (s.calibrationState) {
    case 0:  cfg.calibration = CalibrationState::Absolute;     break;
    case 1:  cfg.calibration = CalibrationState::Relative;     break;
    default: cfg.calibration = CalibrationState::Uncalibrated; break;
    }
    // calibrationOffsetDb (dBFS→SPL) は .ofdx 未導入 (docs 予約キー) のため 0
    return cfg;
}

AcousticResult<VocalAnalysisResult>
QtAcousticAdapter::analyzeVocalFile(const QString &path,
                                    const OperaAcousticSettings &settings)
{
    const AcousticResult<AudioBuffer> wav = readWav(path);
    if (!wav.success())
        return AcousticResult<VocalAnalysisResult>::error(wav.errorCode(),
                                                          wav.message());
    const std::vector<double> samples =
        selectChannel(wav.value(), settings.channelMode);
    const VocalAnalyzer analyzer(toVocalConfig(settings));
    return analyzer.analyze(ArrayView<const double>(samples),
                            wav.value().sampleRateHz);
}

// ── 可聴化 (フェーズ4) ───────────────────────────────────────────────────────
AcousticResult<ConvolutionInfo>
QtAcousticAdapter::convolveFiles(const QString &dryPath, const QString &rirPath,
                                 const QString &outputPath, int gainMode,
                                 std::vector<double> *outDry,
                                 std::vector<double> *outWet,
                                 double *outSampleRate)
{
    typedef AcousticResult<ConvolutionInfo> Result;

    const AcousticResult<AudioBuffer> dry = readWav(dryPath);
    if (!dry.success())
        return Result::error(dry.errorCode(),
                             std::string("dry: ") + dry.message());
    const AcousticResult<AudioBuffer> rir = readWav(rirPath);
    if (!rir.success())
        return Result::error(rir.errorCode(),
                             std::string("rir: ") + rir.message());
    if (outputPath.trimmed().isEmpty())
        return Result::error(AcousticErrorCode::InvalidArgument,
                             "empty output path");

    // 畳み込み (fs 不一致はコアがリサンプリングせずエラーにする)
    const ConvolutionEngine engine;
    AcousticResult<ConvolvedAudio> conv =
        engine.convolve(dry.value(), rir.value());
    if (!conv.success())
        return Result::error(conv.errorCode(), conv.message());

    ConvolvedAudio &out = conv.value();
    ConvolutionInfo info = out.info;

    // gainMode 1: 推奨ゲイン (ピーク→フルスケール) を書き出し値に適用する。
    // info の outputPeak / clippedSampleCount は畳み込み値そのままの報告。
    if (gainMode == 1) {
        const double g = std::pow(10.0, info.suggestedGainDb / 20.0);
        for (std::size_t ch = 0; ch < out.audio.channelCount(); ++ch)
            for (double &v : out.audio.channels[ch])
                v *= g;
        info.warnings.push_back("suggested gain applied to output file");
    }

    const AcousticResult<bool> wr = writeWavFile(
        std::string(QFile::encodeName(outputPath).constData()),
        out.audio, WavSampleFormat::Float32);
    if (!wr.success())
        return Result::error(wr.errorCode(), wr.message());

    if (outDry) *outDry = selectChannel(dry.value(), 2);
    if (outWet && out.audio.channelCount() > 0) *outWet = out.audio.channels[0];
    if (outSampleRate) *outSampleRate = out.audio.sampleRateHz;
    return Result::ok(info);
}
