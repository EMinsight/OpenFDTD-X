/* test_c_api.c — C API (openfdtd_x_acoustics.h) の純 C (C99) テスト。
 *
 * このファイルは意図的に C としてコンパイルする (gcc -std=c99 相当)。
 * ヘッダが C++ の構文 (クラス / テンプレート / 例外 / STL) を露出して
 * いないこと自体がこのテストの検証項目の一つである。
 *
 * 内容:
 *   1. 人工減衰列 (RT60 = 1.0 s) を C 側で生成 → analyze → 指標妥当性
 *   2. NULL 引数のエラー処理
 *   3. struct_size / api_version 不一致の前方互換検査
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/acoustics/c_api/openfdtd_x_acoustics.h"

#ifdef __cplusplus
#error "test_c_api.c must be compiled as C, not C++"
#endif

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        ++g_checks;                                                          \
        if (!(cond)) {                                                       \
            ++g_failures;                                                    \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);           \
        }                                                                    \
    } while (0)

#define CHECK_NEAR(a, b, tol)                                                \
    do {                                                                     \
        ++g_checks;                                                          \
        double va_ = (a), vb_ = (b), vt_ = (tol);                            \
        if (!(fabs(va_ - vb_) <= vt_)) {                                     \
            ++g_failures;                                                    \
            printf("FAIL %s:%d: %s ~= %s (%.10g vs %.10g, tol %.3g)\n",      \
                   __FILE__, __LINE__, #a, #b, va_, vb_, vt_);               \
        }                                                                    \
    } while (0)

/* 決定的 LCG (tests/acoustics/test_common.h と同一係数) */
static unsigned g_lcg_state = 20260716u;
static double lcg_uniform(void) {
    g_lcg_state = g_lcg_state * 1664525u + 1013904223u;
    return ((double)(g_lcg_state >> 8) / 8388608.0) - 1.0;
}

/* 人工 RIR: 直接音デルタ + 指数減衰白色雑音 (RT 秒で -60 dB)。
 * tests/generators/generate_synthetic_rir.cpp と同モデル。 */
static double *make_synthetic_rir(double rt60, double fs, double duration_s,
                                  size_t direct_index, size_t *out_count) {
    size_t n = (size_t)(duration_s * fs + 0.5);
    double *h = (double *)calloc(n, sizeof(double));
    size_t i;
    if (h == NULL) {
        *out_count = 0;
        return NULL;
    }
    /* 減衰白色雑音 (初期 RMS 0.3; 一様乱数の RMS は 1/sqrt(3)) */
    {
        double k = 6.91 / (rt60 * fs);
        double amp = 0.3 * sqrt(3.0);
        for (i = direct_index; i < n; ++i) {
            h[i] += amp * lcg_uniform() * exp(-k * (double)(i - direct_index));
        }
    }
    /* 直接音デルタ */
    if (direct_index < n) h[direct_index] += 1.0;
    *out_count = n;
    return h;
}

static void test_analyze_valid(void) {
    const double rt60 = 1.0;
    const double fs = 48000.0;
    size_t count = 0;
    double *samples = make_synthetic_rir(rt60, fs, 1.8, 480 /* 10 ms */, &count);
    ofdx_ac_context *ctx;
    ofdx_ac_audio_view view;
    ofdx_ac_metrics metrics;
    ofdx_ac_error err;

    CHECK(samples != NULL);
    if (samples == NULL) return;

    ctx = ofdx_ac_context_create();
    CHECK(ctx != NULL);
    if (ctx == NULL) {
        free(samples);
        return;
    }

    view.samples = samples;
    view.sample_count = count;
    view.sample_rate_hz = fs;

    memset(&metrics, 0, sizeof(metrics));
    metrics.struct_size = sizeof(ofdx_ac_metrics);
    metrics.api_version = OFDX_AC_API_VERSION;

    err = ofdx_ac_analyze_rir(ctx, &view, &metrics);
    CHECK(err == OFDX_AC_SUCCESS);
    if (err == OFDX_AC_SUCCESS) {
        /* 残響時間: RT60 = 1.0 s に対し ±0.1 s 以内 */
        CHECK(metrics.t30.valid);
        CHECK_NEAR(metrics.t30.value, rt60, 0.1);
        CHECK(metrics.t20.valid);
        CHECK_NEAR(metrics.t20.value, rt60, 0.1);
        CHECK(metrics.edt.valid);
        CHECK_NEAR(metrics.edt.value, rt60, 0.1);
        CHECK(metrics.t30.quality == OFDX_AC_QUALITY_VALID ||
              metrics.t30.quality == OFDX_AC_QUALITY_WARNING);

        /* 明瞭度 / Definition / 重心時間: 有効かつ物理的に妥当な範囲 */
        CHECK(metrics.c50.valid);
        CHECK(metrics.c80.valid);
        CHECK(metrics.c80.value > metrics.c50.value); /* C80 > C50 (常に) */
        CHECK(metrics.d50.valid);
        CHECK(metrics.d50.value > 0.0 && metrics.d50.value < 1.0);
        CHECK(metrics.center_time.valid);
        CHECK(metrics.center_time.value > 0.0 &&
              metrics.center_time.value < rt60);

        /* 理論値: エネルギーは -60 dB/RT で減衰するので
         * D50 = 1 - 10^(-6*0.05/RT), Ts = RT/13.82 */
        CHECK_NEAR(metrics.d50.value, 1.0 - pow(10.0, -6.0 * 0.05 / rt60), 0.05);
        CHECK_NEAR(metrics.center_time.value, rt60 / 13.82, 0.02);

        /* 成功時は last_error が空 */
        CHECK(strlen(ofdx_ac_last_error(ctx)) == 0);

        printf("  EDT=%.4f T20=%.4f T30=%.4f C50=%.2f C80=%.2f "
               "D50=%.3f Ts=%.4f\n",
               metrics.edt.value, metrics.t20.value, metrics.t30.value,
               metrics.c50.value, metrics.c80.value, metrics.d50.value,
               metrics.center_time.value);
    } else {
        printf("  analyze failed: %s\n", ofdx_ac_last_error(ctx));
    }

    ofdx_ac_context_destroy(ctx);
    free(samples);
}

static void test_null_arguments(void) {
    double samples[8] = {0.0};
    ofdx_ac_context *ctx = ofdx_ac_context_create();
    ofdx_ac_audio_view view;
    ofdx_ac_metrics metrics;

    CHECK(ctx != NULL);
    if (ctx == NULL) return;

    memset(&metrics, 0, sizeof(metrics));
    metrics.struct_size = sizeof(ofdx_ac_metrics);
    metrics.api_version = OFDX_AC_API_VERSION;

    view.samples = samples;
    view.sample_count = 8;
    view.sample_rate_hz = 48000.0;

    /* ctx == NULL */
    CHECK(ofdx_ac_analyze_rir(NULL, &view, &metrics) ==
          OFDX_AC_INVALID_ARGUMENT);
    /* audio == NULL */
    CHECK(ofdx_ac_analyze_rir(ctx, NULL, &metrics) ==
          OFDX_AC_INVALID_ARGUMENT);
    CHECK(strlen(ofdx_ac_last_error(ctx)) > 0);
    /* out_metrics == NULL */
    CHECK(ofdx_ac_analyze_rir(ctx, &view, NULL) == OFDX_AC_INVALID_ARGUMENT);
    /* samples == NULL */
    view.samples = NULL;
    CHECK(ofdx_ac_analyze_rir(ctx, &view, &metrics) ==
          OFDX_AC_INVALID_ARGUMENT);
    view.samples = samples;
    /* sample_count == 0 */
    view.sample_count = 0;
    CHECK(ofdx_ac_analyze_rir(ctx, &view, &metrics) ==
          OFDX_AC_INVALID_ARGUMENT);
    view.sample_count = 8;
    /* sample_rate_hz <= 0 */
    view.sample_rate_hz = 0.0;
    CHECK(ofdx_ac_analyze_rir(ctx, &view, &metrics) ==
          OFDX_AC_INVALID_ARGUMENT);
    view.sample_rate_hz = -48000.0;
    CHECK(ofdx_ac_analyze_rir(ctx, &view, &metrics) ==
          OFDX_AC_INVALID_ARGUMENT);
    view.sample_rate_hz = 48000.0;

    /* 短すぎる有効入力 → INVALID_AUDIO (INVALID_ARGUMENT ではない) */
    CHECK(ofdx_ac_analyze_rir(ctx, &view, &metrics) == OFDX_AC_INVALID_AUDIO);
    CHECK(strlen(ofdx_ac_last_error(ctx)) > 0);

    /* ctx == NULL の last_error は NULL でなく空文字列 */
    CHECK(ofdx_ac_last_error(NULL) != NULL);
    CHECK(strlen(ofdx_ac_last_error(NULL)) == 0);

    /* destroy(NULL) がクラッシュしないこと */
    ofdx_ac_context_destroy(NULL);

    ofdx_ac_context_destroy(ctx);
}

static void test_abi_mismatch(void) {
    double samples[8] = {0.0};
    ofdx_ac_context *ctx = ofdx_ac_context_create();
    ofdx_ac_audio_view view;
    ofdx_ac_metrics metrics;

    CHECK(ctx != NULL);
    if (ctx == NULL) return;

    view.samples = samples;
    view.sample_count = 8;
    view.sample_rate_hz = 48000.0;

    /* struct_size 不一致 */
    memset(&metrics, 0, sizeof(metrics));
    metrics.struct_size = sizeof(ofdx_ac_metrics) - 1;
    metrics.api_version = OFDX_AC_API_VERSION;
    CHECK(ofdx_ac_analyze_rir(ctx, &view, &metrics) ==
          OFDX_AC_INVALID_ARGUMENT);
    CHECK(strstr(ofdx_ac_last_error(ctx), "struct_size") != NULL);

    /* struct_size 未設定 (0) */
    metrics.struct_size = 0;
    CHECK(ofdx_ac_analyze_rir(ctx, &view, &metrics) ==
          OFDX_AC_INVALID_ARGUMENT);

    /* api_version 不一致 */
    metrics.struct_size = sizeof(ofdx_ac_metrics);
    metrics.api_version = OFDX_AC_API_VERSION + 1u;
    CHECK(ofdx_ac_analyze_rir(ctx, &view, &metrics) ==
          OFDX_AC_INVALID_ARGUMENT);
    CHECK(strstr(ofdx_ac_last_error(ctx), "api_version") != NULL);

    ofdx_ac_context_destroy(ctx);
}

int main(void) {
    printf("== C API: analyze synthetic RIR ==\n");
    test_analyze_valid();
    printf("== C API: NULL arguments ==\n");
    test_null_arguments();
    printf("== C API: struct_size / api_version mismatch ==\n");
    test_abi_mismatch();

    printf("test_c_api: %d checks, %d failures — %s\n", g_checks, g_failures,
           g_failures == 0 ? "PASS" : "FAIL");
    return g_failures == 0 ? 0 : 1;
}
