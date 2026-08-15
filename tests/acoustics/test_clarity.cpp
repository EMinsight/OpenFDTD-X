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

    // ── 舞台支援 ST_early / ST_late (ISO 3382-1 Annex C) ──────────────────
    // (a) 矩形 RIR — サンプル数で厳密に決まるケース。
    //     [0,10ms) = 1.0, [20,100ms) = 0.5, [100,1000ms) = 0.25。
    //     エネルギーは窓のサンプル数 × 振幅² なので期待値は厳密。
    {
        const std::size_t b10 = 480, b20 = 960, b100 = 4800, b1000 = 48000;
        std::vector<double> g(b1000, 0.0);
        for (std::size_t i = 0; i < b10; ++i) g[i] = 1.0;
        for (std::size_t i = b20; i < b100; ++i) g[i] = 0.5;
        for (std::size_t i = b100; i < b1000; ++i) g[i] = 0.25;
        ArrayView<const double> gv(g.data(), g.size());
        const AcousticMetricsSet ms = computeAcousticMetrics(gv, fs, 0);

        const double ref = 1.0 * b10;
        const double e = 0.25 * (b100 - b20);
        const double l = 0.0625 * (b1000 - b100);
        CHECK(ms.stEarly.valid);
        CHECK_NEAR(ms.stEarly.value, 10.0 * std::log10(e / ref), 1e-9);
        CHECK(ms.stLate.valid);
        CHECK_NEAR(ms.stLate.value, 10.0 * std::log10(l / ref), 1e-9);
        std::printf("  ST_early=%.4f dB / ST_late=%.4f dB (exact)\n",
                    ms.stEarly.value, ms.stLate.value);
    }

    // (b) 指数減衰 RIR — 連続の閉形式 ∫a..b e^{-kt} dt と比較。
    //     p²(t) = e^{-kt}, k = 13.8155/T (T = 1 s)。離散化誤差のみを許容。
    {
        const double T = 1.0;
        const double k = 13.8155 / T;
        const std::size_t nExp = static_cast<std::size_t>(1.2 * fs);
        std::vector<double> g(nExp);
        for (std::size_t i = 0; i < nExp; ++i)
            g[i] = std::exp(-0.5 * k * (static_cast<double>(i) / fs));
        ArrayView<const double> gv(g.data(), g.size());
        const AcousticMetricsSet ms = computeAcousticMetrics(gv, fs, 0);

        auto seg = [&](double a, double b) {
            return (std::exp(-k * a) - std::exp(-k * b)) / k;
        };
        const double stE = 10.0 * std::log10(seg(0.020, 0.100) / seg(0.0, 0.010));
        const double stL = 10.0 * std::log10(seg(0.100, 1.000) / seg(0.0, 0.010));
        CHECK(ms.stEarly.valid);
        CHECK_NEAR(ms.stEarly.value, stE, 0.01);   // 離散化誤差のみ (< 0.01 dB)
        CHECK(ms.stLate.valid);
        CHECK_NEAR(ms.stLate.value, stL, 0.01);
        std::printf("  ST_early=%.4f dB (theory %.4f) / ST_late=%.4f dB (theory %.4f)\n",
                    ms.stEarly.value, stE, ms.stLate.value, stL);
    }

    // (c) 窓が欠けるときは値を出さない。
    //     0.4 s の RIR (冒頭の 2 反射ケース) は ST_late の 1 s 窓が欠ける
    {
        AcousticMetricsSet ms = computeAcousticMetrics(hv, fs, d.sampleIndex);
        CHECK(ms.stEarly.valid);           // 100 ms 窓は足りている
        CHECK(!ms.stLate.valid);           // 1 s 窓は欠ける → 無効
    }

    // (d) 基準窓 (0-10 ms) にエネルギーが無ければ両方無効
    {
        std::vector<double> g(static_cast<std::size_t>(1.1 * fs), 0.0);
        g[static_cast<std::size_t>(0.030 * fs)] = 1.0;   // 30 ms に 1 発だけ
        ArrayView<const double> gv(g.data(), g.size());
        // 直接音位置を強制的に 0 とみなす (基準窓を空にするため)
        const AcousticMetricsSet ms = computeAcousticMetrics(gv, fs, 0);
        CHECK(!ms.stEarly.valid);
        CHECK(!ms.stLate.valid);
    }

    return testutil::summary("test_clarity");
}
