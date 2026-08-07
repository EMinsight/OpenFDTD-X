// QtAcousticAdapter.cpp
#include "QtAcousticAdapter.h"
#include "../core/Resampler.h"
#include "../io/WavWriter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace ofd;
using namespace ofd::acoustics;

namespace {

// 校正状態 (0=Absolute 1=Relative 2=Uncalibrated) の int → enum 変換。
// RIR 分析と歌声分析で規則を分けないよう 1 か所に集約する。
CalibrationState toCalibrationState(int calibrationState)
{
    switch (calibrationState) {
    case 0:  return CalibrationState::Absolute;
    case 1:  return CalibrationState::Relative;
    default: return CalibrationState::Uncalibrated;
    }
}

// 分析へ渡す校正オフセット。**Absolute のときだけ** モデル値を渡す。
// Relative / Uncalibrated では 0 を返し、未校正なのにオフセットが
// 効いてしまう事故を防ぐ (CLAUDE.md 絶対規則 6 / ADR)。
double effectiveCalibrationOffsetDb(const OperaAcousticSettings &s)
{
    return (toCalibrationState(s.calibrationState) == CalibrationState::Absolute)
               ? s.calibrationOffsetDb
               : 0.0;
}

} // namespace

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

    cfg.calibration = toCalibrationState(s.calibrationState);
    cfg.calibrationOffsetDb = effectiveCalibrationOffsetDb(s);
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

    cfg.calibration = toCalibrationState(s.calibrationState);
    cfg.calibrationOffsetDb = effectiveCalibrationOffsetDb(s);
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

// ── ソルバー metadata.json (ADR-0007 契約) ──────────────────────────────────
QtAcousticAdapter::SolverMetadata
QtAcousticAdapter::readSolverMetadata(const QString &metadataPath)
{
    SolverMetadata m;
    QFile f(metadataPath);
    if (!f.open(QIODevice::ReadOnly)) return m;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return m;
    const QJsonObject o = doc.object();
    // sample_rate が無いものは契約に沿った metadata.json ではない
    if (!o.contains(QStringLiteral("sample_rate"))) return m;
    m.valid = true;
    m.sampleRateHz = o.value(QStringLiteral("sample_rate")).toDouble(0.0);
    m.solver = o.value(QStringLiteral("solver")).toString();
    m.tSabineS = o.value(QStringLiteral("t_sabine_s")).toDouble(0.0);
    const QJsonObject src = o.value(QStringLiteral("source")).toObject();
    m.sourceFmaxHz = src.value(QStringLiteral("fmax_hz")).toDouble(0.0);
    m.sourceSigmaS = src.value(QStringLiteral("sigma_s")).toDouble(0.0);
    m.sourceT0S    = src.value(QStringLiteral("t0_s")).toDouble(0.0);
    const QJsonObject grid = o.value(QStringLiteral("grid")).toObject();
    m.gridDxM = grid.value(QStringLiteral("dx_m")).toDouble(0.0);
    m.method = o.value(QStringLiteral("method")).toString();
    const QJsonArray band = o.value(QStringLiteral("valid_band_hz")).toArray();
    if (band.size() == 2) {
        m.validBandLoHz = band.at(0).toDouble(0.0);
        m.validBandHiHz = band.at(1).toDouble(0.0);
    }
    return m;
}

QtAcousticAdapter::SolverMetadata
QtAcousticAdapter::metadataForRir(const QString &rirPath)
{
    if (rirPath.trimmed().isEmpty()) return SolverMetadata();
    const QFileInfo fi(rirPath);
    if (!fi.exists()) return SolverMetadata();
    return readSolverMetadata(
        QDir(fi.absolutePath()).absoluteFilePath(QStringLiteral("metadata.json")));
}

// ── 可聴化 (フェーズ4) ───────────────────────────────────────────────────────
AcousticResult<ConvolutionInfo>
QtAcousticAdapter::convolveFiles(const QString &dryPath, const QString &rirPath,
                                 const QString &outputPath, int gainMode,
                                 std::vector<double> *outDry,
                                 std::vector<double> *outWet,
                                 double *outSampleRate,
                                 RirResampleNote *outResample)
{
    typedef AcousticResult<ConvolutionInfo> Result;

    if (outResample) *outResample = RirResampleNote();
    const AcousticResult<AudioBuffer> dry = readWav(dryPath);
    if (!dry.success())
        return Result::error(dry.errorCode(),
                             std::string("dry: ") + dry.message());
    const AcousticResult<AudioBuffer> rir = readWav(rirPath);
    if (!rir.success())
        return Result::error(rir.errorCode(),
                             std::string("rir: ") + rir.message());
    return convolveBuffers(dry.value(), rir.value(), outputPath, gainMode,
                           outDry, outWet, outSampleRate, outResample);
}

AcousticResult<ConvolutionInfo>
QtAcousticAdapter::convolveBuffers(const AudioBuffer &dryBuf,
                                   const AudioBuffer &rirBuf,
                                   const QString &outputPath, int gainMode,
                                   std::vector<double> *outDry,
                                   std::vector<double> *outWet,
                                   double *outSampleRate,
                                   RirResampleNote *outResample)
{
    typedef AcousticResult<ConvolutionInfo> Result;

    if (outResample) *outResample = RirResampleNote();
    if (outputPath.trimmed().isEmpty())
        return Result::error(AcousticErrorCode::InvalidArgument,
                             "empty output path");

    // fs 不一致は RIR をドライ側 fs へリサンプリングして続行する (負債 #12)。
    // 音源素材 (ドライ) は変えない。変換の事実は outResample で通知し、
    // 呼び出し側 UI が必ず表示する (黙って変換しない)。fs 自体が不正
    // (非正・非整数など) で変換できない場合はここでエラーになる。
    AudioBuffer rirUsed = rirBuf;
    // fromHz / toHz は変換の有無に関わらず埋める (RIR の帯域は変換しても
    // 広がらないので、呼び出し側が帯域の注記を出せるようにするため)。
    if (outResample) {
        outResample->fromHz = rirUsed.sampleRateHz;
        outResample->toHz = dryBuf.sampleRateHz;
    }
    if (dryBuf.sampleRateHz > 0.0 && rirUsed.sampleRateHz > 0.0 &&
        rirUsed.sampleRateHz != dryBuf.sampleRateHz) {
        const double fromHz = rirUsed.sampleRateHz;
        AcousticResult<AudioBuffer> rs =
            resampleBuffer(rirUsed, dryBuf.sampleRateHz);
        if (!rs.success())
            return Result::error(rs.errorCode(),
                                 std::string("rir resample: ") + rs.message());
        rirUsed = std::move(rs.value());
        if (outResample) {
            outResample->resampled = true;
            outResample->fromHz = fromHz;
            outResample->toHz = rirUsed.sampleRateHz;
        }
    }

    // 畳み込み (fs はここで一致済み。コアは不一致を引き続きエラーにする)
    const ConvolutionEngine engine;
    AcousticResult<ConvolvedAudio> conv =
        engine.convolve(dryBuf, rirUsed);
    if (!conv.success())
        return Result::error(conv.errorCode(), conv.message());

    ConvolvedAudio &out = conv.value();
    ConvolutionInfo info = out.info;

    // gainMode 1: 推奨ゲイン (ピーク→フルスケール) を書き出し値に適用する。
    if (gainMode == 1) {
        const double appliedGainDb = info.suggestedGainDb;
        const double g = std::pow(10.0, appliedGainDb / 20.0);
        for (std::size_t ch = 0; ch < out.audio.channelCount(); ++ch)
            for (double &v : out.audio.channels[ch])
                v *= g;

        // ピーク / クリップ数は「実際に書き出したサンプル」から取り直す。
        // (ラベルを「正規化前」表記に変える案もあるが、A/B 波形はゲイン
        //  適用後の値を描くため、数値と波形の基準が食い違ったままになる。
        //  実測し直す方が UI 全体で基準が 1 つに揃う。)
        const bool wasClipped = info.clipped;
        const double thr = engine.config().clipThreshold;
        double peak = 0.0;
        std::size_t clipped = 0;
        for (std::size_t ch = 0; ch < out.audio.channelCount(); ++ch) {
            const std::vector<double> &y = out.audio.channels[ch];
            for (std::size_t i = 0; i < y.size(); ++i) {
                const double a = std::fabs(y[i]);
                if (a > peak) peak = a;
                if (a > thr) ++clipped;
            }
        }
        info.outputPeak = peak;
        info.outputPeakDbfs = (peak > 0.0) ? 20.0 * std::log10(peak) : -300.0;
        info.clippedSampleCount = clipped;
        info.clipped = (clipped > 0);
        // suggestedGainDb は「書き出しに適用したゲイン」として保持する
        // (適用後の残り推奨量 ≒ 0 dB を報告しても情報にならない)。

        // 適用前の値を引用するコアのクリップ警告 (最後に追加される) は捨て、
        // 適用後も超過が残る場合だけ貼り直す。
        if (wasClipped && !info.warnings.empty())
            info.warnings.pop_back();
        char msg[224];
        std::snprintf(msg, sizeof(msg),
                      "suggested gain %.2f dB applied to output file "
                      "(peak / clipping are measured on the written samples)",
                      appliedGainDb);
        info.warnings.push_back(msg);
        if (info.clipped) {
            std::snprintf(msg, sizeof(msg),
                          "%llu samples still exceed full scale after gain "
                          "(peak %.3f)",
                          static_cast<unsigned long long>(clipped), peak);
            info.warnings.push_back(msg);
        }
    }

    const AcousticResult<bool> wr = writeWavFile(
        std::string(QFile::encodeName(outputPath).constData()),
        out.audio, WavSampleFormat::Float32);
    if (!wr.success())
        return Result::error(wr.errorCode(), wr.message());

    if (outDry) *outDry = selectChannel(dryBuf, 2);
    if (outWet && out.audio.channelCount() > 0) *outWet = out.audio.channels[0];
    if (outSampleRate) *outSampleRate = out.audio.sampleRateHz;
    return Result::ok(info);
}
