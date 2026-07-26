// test_clarity.cpp — 解析的に計算できる 2 反射 RIR で C50/C80/D50/Ts を検証。
//
// RIR: 直接音 δ (振幅 1.0) + 反射 δ (60 ms, 振幅 0.5) + 反射 δ (100 ms, 0.4)。
// エネルギーは 1.0 / 0.25 / 0.16 なので (ISO 3382-1):
//   C50 = 10·log10(1 / (0.25 + 0.16))
//   C80 = 10·log10((1 + 0.25) / 0.16)
//   D50 = 1 / 1.41
//   Ts  = (0.060·0.25 + 0.100·0.16) / 1.41
#include <cmath>
#include <cstdio>
#include <vector>

#include "../../src/acoustics/core/AcousticMetrics.h"
#include "../../src/acoustics/core/DirectSoundDetector.h"
#include "test_common.h"

using namespace ofd::acoustics;

int main() {
    const double fs = 48000.0;
    const double a1 = 0.5, t1 = 0.060; // 50-80 ms 帯の反射
    const double a2 = 0.4, t2 = 0.100; // 80 ms 以降の反射
    const double directDelay = 0.010;
    const double dur = 0.400;

    std::vector<double> h(static_cast<std::size_t>(dur * fs + 0.5), 0.0);
    const std::size_t d0 = static_cast<std::size_t>(directDelay * fs + 0.5);
    const std::size_t i1 = d0 + static_cast<std::size_t>(t1 * fs + 0.5);
    const std::size_t i2 = d0 + static_cast<std::size_t>(t2 * fs + 0.5);
    h[d0] = 1.0;
    h[i1] = a1;
    h[i2] = a2;
    ArrayView<const double> hv(h.data(), h.size());

    // 直接音は最大振幅かつ最初の到来
    DirectSoundResult d = detectDirectSound(hv, fs);
    CHECK(d.found);
    CHECK(d.sampleIndex == d0);

    AcousticMetricsSet m = computeAcousticMetrics(hv, fs, d.sampleIndex);

    // 理論値
    const double e0 = 1.0, e1 = a1 * a1, e2 = a2 * a2;
    const double total = e0 + e1 + e2;
    const double c50Theory = 10.0 * std::log10(e0 / (e1 + e2));
    const double c80Theory = 10.0 * std::log10((e0 + e1) / e2);
    const double d50Theory = e0 / total;
    const double tsTheory = (t1 * e1 + t2 * e2) / total; // 直接音は t=0
    const double el50Theory = e0 / (e1 + e2);
    const double el80Theory = (e0 + e1) / e2;

    CHECK(m.c50.valid);
    CHECK(m.c80.valid);
    CHECK(m.d50.valid);
    CHECK(m.ts.valid);
    if (m.c50.valid) CHECK_NEAR(m.c50.value, c50Theory, 0.2);   // ±0.2 dB
    if (m.c80.valid) CHECK_NEAR(m.c80.value, c80Theory, 0.2);   // ±0.2 dB
    if (m.d50.valid) CHECK_NEAR(m.d50.value, d50Theory, 0.01);  // ±0.01
    if (m.ts.valid) CHECK_NEAR(m.ts.value, tsTheory, 0.001);    // ±1 ms
    if (m.earlyLate50.valid)
        CHECK_REL(m.earlyLate50.value, el50Theory, 0.01);
    if (m.earlyLate80.valid)
        CHECK_REL(m.earlyLate80.value, el80Theory, 0.01);

    std::printf("  C50=%.4f dB (theory %.4f)\n", m.c50.value, c50Theory);
    std::printf("  C80=%.4f dB (theory %.4f)\n", m.c80.value, c80Theory);
    std::printf("  D50=%.5f    (theory %.5f)\n", m.d50.value, d50Theory);
    std::printf("  Ts =%.5f s  (theory %.5f)\n", m.ts.value, tsTheory);

    // ── 反射が 1 つ (30 ms, 早期のみ) の場合: C50 の後期エネルギーがゼロ
    //    → 比が定義できず invalid になる ──
    {
        std::vector<double> g(static_cast<std::size_t>(0.2 * fs + 0.5), 0.0);
        const std::size_t gd = static_cast<std::size_t>(0.010 * fs + 0.5);
        g[gd] = 1.0;
        g[gd + static_cast<std::size_t>(0.030 * fs + 0.5)] = 0.5;
        AcousticMetricsSet mg = computeAcousticMetrics(
            ArrayView<const double>(g.data(), g.size()), fs, gd);
        CHECK(!mg.c50.valid); // 後期エネルギーなし
        CHECK(!mg.c80.valid);
        CHECK(mg.d50.valid);
        if (mg.d50.valid) CHECK_NEAR(mg.d50.value, 1.0, 1e-9);
        // Ts = 0.03·0.25 / 1.25 = 6 ms
        CHECK(mg.ts.valid);
        if (mg.ts.valid) CHECK_NEAR(mg.ts.value, 0.030 * 0.25 / 1.25, 0.001);
    }

    // ── 80 ms より短い信号では C80 が invalid ──
    {
        std::vector<double> g(static_cast<std::size_t>(0.06 * fs + 0.5), 0.0);
        g[0] = 1.0;
        g[100] = 0.3;
        AcousticMetricsSet mg = computeAcousticMetrics(
            ArrayView<const double>(g.data(), g.size()), fs, 0);
        CHECK(!mg.c80.valid);
    }

    return testutil::summary("test_clarity");
}
