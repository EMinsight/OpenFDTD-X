// openfdtd_x_acoustics.cpp — C API 実装。C++14 で RirAnalyzer に委譲する。
// C++ の例外・型は境界を越えない: 例外はすべて捕捉して INTERNAL_ERROR に
// 変換し、エラーメッセージは context 内の固定長 char バッファへ複写する。
#include "openfdtd_x_acoustics.h"

#include <cstdio>
#include <cstring>
#include <new>

#include "../core/AcousticError.h"
#include "../core/AnalysisQuality.h"
#include "../core/ArrayView.h"
#include "../core/RirAnalyzer.h"

namespace {

const std::size_t kLastErrorCapacity = 512;

} // namespace

// 不透明コンテキストの実体。ヘッダには一切露出しない。
struct ofdx_ac_context {
    char lastError[kLastErrorCapacity];

    ofdx_ac_context() { lastError[0] = '\0'; }

    void clearError() { lastError[0] = '\0'; }

    void setError(const char *message) {
        if (message == nullptr) message = "";
        std::snprintf(lastError, kLastErrorCapacity, "%s", message);
    }
};

namespace {

using ofd::acoustics::AcousticErrorCode;
using ofd::acoustics::AnalysisQuality;
using ofd::acoustics::MetricValue;

// コアのエラーコード → C API のエラーコード
ofdx_ac_error mapErrorCode(AcousticErrorCode code) {
    switch (code) {
    case AcousticErrorCode::Ok:
        return OFDX_AC_SUCCESS;
    case AcousticErrorCode::InvalidArgument:
        return OFDX_AC_INVALID_ARGUMENT;
    case AcousticErrorCode::EmptyInput:
    case AcousticErrorCode::InputTooShort:
    case AcousticErrorCode::NonFiniteSample:
    case AcousticErrorCode::ClippingDetected:
    case AcousticErrorCode::UnsupportedSampleRate:
    case AcousticErrorCode::UnsupportedFormat:
        return OFDX_AC_INVALID_AUDIO;
    case AcousticErrorCode::DirectSoundNotFound:
        return OFDX_AC_DIRECT_SOUND_NOT_FOUND;
    case AcousticErrorCode::InsufficientDynamicRange:
    case AcousticErrorCode::NoiseFloorTooHigh:
        return OFDX_AC_INSUFFICIENT_DYNAMIC_RANGE;
    case AcousticErrorCode::RegressionFailed:
    case AcousticErrorCode::FilterDesignFailed:
        return OFDX_AC_NUMERICAL_FAILURE;
    default:
        return OFDX_AC_INTERNAL_ERROR;
    }
}

int mapQuality(AnalysisQuality q) {
    switch (q) {
    case AnalysisQuality::Valid:   return OFDX_AC_QUALITY_VALID;
    case AnalysisQuality::Warning: return OFDX_AC_QUALITY_WARNING;
    default:                       return OFDX_AC_QUALITY_INVALID;
    }
}

ofdx_ac_metric mapMetric(const MetricValue &m) {
    ofdx_ac_metric out;
    out.value = m.value;
    out.valid = m.valid ? 1 : 0;
    out.quality = mapQuality(m.quality);
    return out;
}

// struct_size / api_version による前方互換検査。
// バージョン 1 では struct_size == sizeof(ofdx_ac_metrics) を要求する。
// 将来フィールドを末尾追加した場合は api_version を上げ、旧サイズも
// ここで受理する (旧レイアウト分のみ書き込む)。
bool checkAbi(const ofdx_ac_metrics *m, ofdx_ac_context *ctx) {
    if (m->api_version != OFDX_AC_API_VERSION) {
        char buf[128];
        std::snprintf(buf, sizeof(buf),
                      "api_version mismatch: caller=%u, library=%u",
                      static_cast<unsigned>(m->api_version),
                      static_cast<unsigned>(OFDX_AC_API_VERSION));
        ctx->setError(buf);
        return false;
    }
    if (m->struct_size != sizeof(ofdx_ac_metrics)) {
        char buf[128];
        std::snprintf(buf, sizeof(buf),
                      "struct_size mismatch: caller=%u, library=%u",
                      static_cast<unsigned>(m->struct_size),
                      static_cast<unsigned>(sizeof(ofdx_ac_metrics)));
        ctx->setError(buf);
        return false;
    }
    return true;
}

// 分析本体 (例外はここから漏れ得るので呼び出し側の try で受ける)
ofdx_ac_error analyzeImpl(ofdx_ac_context *ctx,
                          const ofdx_ac_audio_view *audio,
                          ofdx_ac_metrics *out) {
    using namespace ofd::acoustics;

    // 広帯域 (フィルタなし) の指標のみを返す構成
    RirAnalyzerConfig cfg;
    cfg.bandSet = BandSet::FullBandOnly;
    RirAnalyzer analyzer(cfg);

    AcousticResult<RirAnalysisResult> r = analyzer.analyze(
        ArrayView<const double>(audio->samples, audio->sample_count),
        audio->sample_rate_hz);
    if (!r.success()) {
        ctx->setError(r.message().c_str());
        ofdx_ac_error e = mapErrorCode(r.errorCode());
        return (e == OFDX_AC_SUCCESS) ? OFDX_AC_INTERNAL_ERROR : e;
    }

    const RirAnalysisResult &res = r.value();
    if (res.bands.empty()) {
        ctx->setError("internal: analysis produced no band results");
        return OFDX_AC_INTERNAL_ERROR;
    }
    const AcousticMetricsSet &m = res.bands[0].metrics;
    out->edt = mapMetric(m.edt);
    out->t20 = mapMetric(m.t20);
    out->t30 = mapMetric(m.t30);
    out->c50 = mapMetric(m.c50);
    out->c80 = mapMetric(m.c80);
    out->d50 = mapMetric(m.d50);
    out->center_time = mapMetric(m.ts);
    return OFDX_AC_SUCCESS;
}

} // namespace

extern "C" {

ofdx_ac_context *ofdx_ac_context_create(void) {
    return new (std::nothrow) ofdx_ac_context();
}

void ofdx_ac_context_destroy(ofdx_ac_context *ctx) {
    delete ctx;
}

ofdx_ac_error ofdx_ac_analyze_rir(ofdx_ac_context *ctx,
                                  const ofdx_ac_audio_view *audio,
                                  ofdx_ac_metrics *out_metrics) {
    if (ctx == nullptr) return OFDX_AC_INVALID_ARGUMENT;
    ctx->clearError();
    try {
        if (audio == nullptr) {
            ctx->setError("audio view is NULL");
            return OFDX_AC_INVALID_ARGUMENT;
        }
        if (out_metrics == nullptr) {
            ctx->setError("out_metrics is NULL");
            return OFDX_AC_INVALID_ARGUMENT;
        }
        if (!checkAbi(out_metrics, ctx)) return OFDX_AC_INVALID_ARGUMENT;
        if (audio->samples == nullptr || audio->sample_count == 0) {
            ctx->setError("audio samples are NULL or empty");
            return OFDX_AC_INVALID_ARGUMENT;
        }
        if (!(audio->sample_rate_hz > 0.0)) {
            ctx->setError("sample_rate_hz must be > 0");
            return OFDX_AC_INVALID_ARGUMENT;
        }
        return analyzeImpl(ctx, audio, out_metrics);
    } catch (const std::exception &e) {
        ctx->setError(e.what());
        return OFDX_AC_INTERNAL_ERROR;
    } catch (...) {
        ctx->setError("unknown exception in acoustic core");
        return OFDX_AC_INTERNAL_ERROR;
    }
}

const char *ofdx_ac_last_error(const ofdx_ac_context *ctx) {
    if (ctx == nullptr) return "";
    return ctx->lastError;
}

} // extern "C"
