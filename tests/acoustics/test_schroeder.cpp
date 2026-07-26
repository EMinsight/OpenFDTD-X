// test_schroeder.cpp — Schroeder 二乗後方積分と回帰の検証。
// 理想指数減衰 x(t) = e^{-6.91 t / RT} では減衰カーブの傾きは厳密に
// -60/RT [dB/s] になる (Schroeder 1965)。傾き ±1%、決定係数 > 0.999。
#include <cstdio>
#include <vector>

#include "../../src/acoustics/core/SchroederDecay.h"
#include "test_common.h"

using namespace ofd::acoustics;

namespace {

std::vector<double> idealExponential(double rt, double fs, double dur) {
    const std::size_t n = static_cast<std::size_t>(dur * fs + 0.5);
    std::vector<double> x(n);
    const double k = 6.91 / (rt * fs);
    for (std::size_t i = 0; i < n; ++i)
        x[i] = std::exp(-k * static_cast<double>(i));
    return x;
}

void testIdealDecay(double rt, bool noiseComp) {
    const double fs = 48000.0;
    const double dur = 1.5 * rt + 0.3;
    std::vector<double> x = idealExponential(rt, fs, dur);
    ArrayView<const double> xv(x.data(), x.size());

    SchroederOptions opt;
    opt.noiseCompensation = noiseComp;
    SchroederResult dec = computeSchroederDecay(xv, fs, opt);
    CHECK(dec.valid);
    if (!dec.valid) return;
    CHECK(!dec.decayDb.empty());
    CHECK_NEAR(dec.decayDb[0], 0.0, 1e-9); // 正規化: 先頭 0 dB
    // 単調非増加 (数点サンプル確認)
    for (std::size_t i = 1; i < dec.decayDb.size(); i += 4801)
        CHECK(dec.decayDb[i] <= dec.decayDb[i - 1] + 1e-12);

    ArrayView<const double> curve(dec.decayDb.data(), dec.decayDb.size());
    RegressionResult reg =
        regressDecaySegment(curve, fs, -5.0, -35.0, dec.analysisEndIndex);
    CHECK(reg.valid);
    if (!reg.valid) {
        std::printf("  regression failed: %s\n", reg.warning.c_str());
        return;
    }
    const double slopeExpect = -60.0 / rt;
    CHECK_REL(reg.slope, slopeExpect, 0.01); // ±1%
    CHECK(reg.rSquared > 0.999);
    CHECK(reg.standardError >= 0.0);
    CHECK(reg.startDb <= -5.0 && reg.startDb > -6.0);
    CHECK(reg.endDb <= -35.0 && reg.endDb > -36.0);
    std::printf("  RT=%.2f comp=%d: slope=%.4f (expect %.4f, err %.3f%%), "
                "R2=%.6f\n",
                rt, noiseComp ? 1 : 0, reg.slope, slopeExpect,
                100.0 * std::fabs(reg.slope - slopeExpect) /
                    std::fabs(slopeExpect),
                reg.rSquared);
}

} // namespace

int main() {
    testIdealDecay(0.5, false);
    testIdealDecay(1.0, false);
    testIdealDecay(2.0, false);
    testIdealDecay(1.0, true); // ノイズ補正オンでも同等

    // ── 回帰区間が確保できない場合は valid = false ──
    {
        const double fs = 48000.0;
        std::vector<double> x = idealExponential(1.0, fs, 0.4); // 最深 -24 dB 程度
        ArrayView<const double> xv(x.data(), x.size());
        SchroederResult dec = computeSchroederDecay(xv, fs);
        CHECK(dec.valid);
        ArrayView<const double> curve(dec.decayDb.data(), dec.decayDb.size());
        RegressionResult reg =
            regressDecaySegment(curve, fs, -5.0, -200.0, dec.analysisEndIndex);
        CHECK(!reg.valid);
        CHECK(!reg.warning.empty());
    }

    // ── 不正引数 ──
    {
        RegressionResult reg = regressDecaySegment(
            ArrayView<const double>(), 48000.0, -5.0, -25.0, 0);
        CHECK(!reg.valid);
        std::vector<double> flat(100, 0.0);
        RegressionResult reg2 = regressDecaySegment(
            ArrayView<const double>(flat.data(), flat.size()), 48000.0, -25.0,
            -5.0, flat.size()); // start < end は不正
        CHECK(!reg2.valid);
        SchroederResult dec =
            computeSchroederDecay(ArrayView<const double>(), 48000.0);
        CHECK(!dec.valid);
    }

    // ── ノイズつき指数減衰: 分析終了点がノイズ域より前に置かれる ──
    {
        testutil::SyntheticRirSpec spec;
        spec.rt60 = 1.0;
        spec.noiseFloorDb = -50.0;
        spec.directDelaySeconds = 0.0;
        std::vector<double> h = testutil::makeSyntheticRir(spec);
        SchroederResult dec = computeSchroederDecay(
            ArrayView<const double>(h.data(), h.size()), spec.sampleRateHz);
        CHECK(dec.valid);
        CHECK(dec.analysisEndIndex < h.size()); // 末尾より手前で打ち切り
        CHECK(dec.noiseFloorDb > -60.0 && dec.noiseFloorDb < -30.0);
    }

    return testutil::summary("test_schroeder");
}
