// test_common.h — 音響コアテスト用の簡易 CHECK マクロと人工 RIR 生成。
// ヘッダオンリー / C++14 / 外部フレームワーク非依存。
#pragma once
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace testutil {

inline int &checkCount() {
    static int c = 0;
    return c;
}
inline int &failCount() {
    static int c = 0;
    return c;
}

inline void reportFail(const char *file, int line, const char *expr,
                       const std::string &detail) {
    std::printf("FAIL %s:%d: %s %s\n", file, line, expr, detail.c_str());
}

// テスト末尾で呼ぶ。戻り値を main の戻り値にする。
inline int summary(const char *name) {
    std::printf("%s: %d checks, %d failures — %s\n", name, checkCount(),
                failCount(), failCount() == 0 ? "PASS" : "FAIL");
    return failCount() == 0 ? 0 : 1;
}

// ── 決定的な擬似乱数 (LCG, Numerical Recipes 係数) ──
inline unsigned lcgNext(unsigned &state) {
    state = state * 1664525u + 1013904223u;
    return state;
}
// [-1, 1] の一様乱数
inline double lcgUniform(unsigned &state) {
    return (static_cast<double>(lcgNext(state) >> 8) / 8388608.0) - 1.0;
}

// ── 人工 RIR 生成 (tests/generators/generate_synthetic_rir.cpp と同モデル) ──
// h(t) = δ(t-t0) + Σ a_i δ(t-t0-τ_i)
//        + n(t)·exp(-6.91 (t-t0) / RT)   (t ≥ t0, n(t) は一様白色雑音)
//        + noiseFloor·w(t)               (全区間)
// exp(-6.91 t / RT) は RT 秒で -60 dB (エネルギーで) に対応する減衰。
struct SyntheticRirSpec {
    double rt60;                // 残響時間 [s]
    double sampleRateHz;
    double durationSeconds;     // 0 以下なら 1.5·RT + 0.3 s
    double directDelaySeconds;  // 直接音の位置
    double directAmplitude;     // 直接音デルタの振幅
    double decayNoiseRms;       // 減衰白色雑音の初期 RMS
    double noiseFloorDb;        // 付加ノイズフロア (≤ -900 で無効) [dBFS rms]
    // {遅延 [s] (直接音基準), レベル [dB] (直接音比)} の反射列
    std::vector<std::pair<double, double>> reflections;
    unsigned seed;

    SyntheticRirSpec()
        : rt60(1.0), sampleRateHz(48000.0), durationSeconds(0.0),
          directDelaySeconds(0.010), directAmplitude(1.0), decayNoiseRms(0.3),
          noiseFloorDb(-1000.0), reflections(), seed(20260716u) {}
};

inline std::vector<double> makeSyntheticRir(const SyntheticRirSpec &spec) {
    const double fs = spec.sampleRateHz;
    const double dur = (spec.durationSeconds > 0.0)
                           ? spec.durationSeconds
                           : 1.5 * spec.rt60 + 0.3;
    const std::size_t n = static_cast<std::size_t>(dur * fs + 0.5);
    const std::size_t d0 =
        static_cast<std::size_t>(spec.directDelaySeconds * fs + 0.5);
    std::vector<double> h(n, 0.0);
    unsigned st = spec.seed;

    // 減衰白色雑音 (一様乱数の RMS は 1/√3 なので √3 倍して指定 RMS に)
    if (spec.decayNoiseRms > 0.0 && spec.rt60 > 0.0) {
        const double k = 6.91 / (spec.rt60 * fs);
        const double amp = spec.decayNoiseRms * std::sqrt(3.0);
        for (std::size_t i = d0; i < n; ++i) {
            h[i] += amp * lcgUniform(st) *
                    std::exp(-k * static_cast<double>(i - d0));
        }
    }
    // 直接音デルタ
    if (d0 < n) h[d0] += spec.directAmplitude;
    // 反射 (デルタ)
    for (std::size_t r = 0; r < spec.reflections.size(); ++r) {
        const std::size_t idx =
            d0 + static_cast<std::size_t>(spec.reflections[r].first * fs + 0.5);
        if (idx < n) {
            h[idx] += spec.directAmplitude *
                      std::pow(10.0, spec.reflections[r].second / 20.0);
        }
    }
    // ノイズフロア
    if (spec.noiseFloorDb > -900.0) {
        const double amp =
            std::pow(10.0, spec.noiseFloorDb / 20.0) * std::sqrt(3.0);
        for (std::size_t i = 0; i < n; ++i) h[i] += amp * lcgUniform(st);
    }
    return h;
}

} // namespace testutil

#define CHECK(cond)                                                           \
    do {                                                                      \
        ++testutil::checkCount();                                             \
        if (!(cond)) {                                                        \
            ++testutil::failCount();                                          \
            testutil::reportFail(__FILE__, __LINE__, #cond, std::string());   \
        }                                                                     \
    } while (0)

#define CHECK_NEAR(a, b, tol)                                                 \
    do {                                                                      \
        ++testutil::checkCount();                                             \
        const double va_ = (a), vb_ = (b), vt_ = (tol);                       \
        if (!(std::fabs(va_ - vb_) <= vt_)) {                                 \
            ++testutil::failCount();                                          \
            char buf_[200];                                                   \
            std::snprintf(buf_, sizeof(buf_), "(%.10g vs %.10g, tol %.3g)",   \
                          va_, vb_, vt_);                                     \
            testutil::reportFail(__FILE__, __LINE__, #a " ~= " #b, buf_);     \
        }                                                                     \
    } while (0)

// 相対誤差チェック: |a - b| <= rel * |b|
#define CHECK_REL(a, b, rel)                                                  \
    do {                                                                      \
        ++testutil::checkCount();                                             \
        const double va_ = (a), vb_ = (b), vr_ = (rel);                       \
        if (!(std::fabs(va_ - vb_) <= vr_ * std::fabs(vb_))) {                \
            ++testutil::failCount();                                          \
            char buf_[200];                                                   \
            std::snprintf(buf_, sizeof(buf_),                                 \
                          "(%.10g vs %.10g, rel err %.4g > %.3g)", va_, vb_,  \
                          std::fabs(va_ - vb_) / std::fabs(vb_), vr_);        \
            testutil::reportFail(__FILE__, __LINE__, #a " ~ " #b, buf_);      \
        }                                                                     \
    } while (0)
