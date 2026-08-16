// test_strength.cpp — G (音の強さ、ISO 3382-1 A.2.6) の検証。
//
// G = 10log10(∫p²dt / ∫p₁₀²dt) は**比**なので、閉形式アンカーは代数だけで
// 厳密に書ける (フィルタも回帰も挟まらない)。使うアンカー:
//   (1) 恒等   : 実測 = 基準            → G = 0 dB (厳密)
//   (2) 相似   : 実測 = k × 基準        → G = 20log10(k) (厳密)
//   (3) 距離   : 基準を r [m] で測った  → G が −20log10(r/10) だけずれる
//   (4) fs 非依存 : エネルギーは Σx²/fs なので、実測と基準の fs が違っても
//                   同じ物理信号なら G は変わらない (矩形パルスで厳密に検査)
//   (5) 早期/後期 : 10^(Ge/10) + 10^(Gl/10) = 10^(G/10) (厳密)
//   (6) 指数減衰 : 等比級数の閉形式 E = A²/fs·(1−q^N)/(1−q), q = e^{-1/(fs·τ)}
// さらに前提未達 (基準なし / 無音 / 距離 ≤ 0 / 80 ms 未満) が invalid になること。
#include <cmath>
#include <cstdio>
#include <vector>

#include "../../src/acoustics/core/SoundStrength.h"
#include "test_common.h"

using namespace ofd::acoustics;

namespace {

// 振幅 A・長さ N の矩形パルス。E = N·A²/fs。
std::vector<double> rect(std::size_t n, double a)
{
    return std::vector<double>(n, a);
}

// h(t) = A·e^{-t/(2τ)} → h² = A²e^{-t/τ}
std::vector<double> decay(double fs, double tau, double a, double durationSec)
{
    const std::size_t n = static_cast<std::size_t>(durationSec * fs + 0.5);
    std::vector<double> h(n, 0.0);
    for (std::size_t i = 0; i < n; ++i)
        h[i] = a * std::exp(-0.5 * (static_cast<double>(i) / fs) / tau);
    return h;
}

SoundStrengthReference refFrom(const std::vector<double> &ir, double fs,
                               double distanceM = 10.0)
{
    ArrayView<const double> v(ir.data(), ir.size());
    AcousticResult<SoundStrengthReference> r =
        makeSoundStrengthReference(v, fs, distanceM);
    CHECK(r.success());
    return r.success() ? r.value() : SoundStrengthReference();
}

} // namespace

int main() {
    const double fs = 48000.0;

    // ── (1) 恒等: 実測と基準が同じ信号なら G = 0 dB ──
    {
        const std::vector<double> ir = decay(fs, 0.1, 0.5, 1.0);
        const SoundStrengthReference ref = refFrom(ir, fs);
        CHECK(ref.available);
        ArrayView<const double> v(ir.data(), ir.size());
        const SoundStrengthResult r = computeSoundStrength(v, fs, 0, ref);
        CHECK(r.g.valid);
        CHECK(r.quality == AnalysisQuality::Valid);
        CHECK_NEAR(r.g.value, 0.0, 1e-12);
        CHECK_NEAR(r.distanceCorrectionDb, 0.0, 1e-12);
        CHECK_NEAR(r.measuredEnergyDb, r.referenceEnergyDb, 1e-12);
    }

    // ── (2) 相似: 実測 = k × 基準 → G = 20log10(k) ──
    {
        const std::vector<double> base = decay(fs, 0.2, 0.3, 1.0);
        const SoundStrengthReference ref = refFrom(base, fs);
        for (double k : { 0.1, 0.5, 1.0, 2.0, 10.0 }) {
            std::vector<double> m(base.size());
            for (std::size_t i = 0; i < base.size(); ++i) m[i] = k * base[i];
            ArrayView<const double> v(m.data(), m.size());
            const SoundStrengthResult r = computeSoundStrength(v, fs, 0, ref);
            CHECK(r.g.valid);
            CHECK_NEAR(r.g.value, 20.0 * std::log10(k), 1e-9);
        }
    }

    // ── (3) 距離補正: 基準を r [m] で測ると G は −20log10(r/10) ずれる ──
    {
        const std::vector<double> ir = decay(fs, 0.15, 0.4, 1.0);
        ArrayView<const double> v(ir.data(), ir.size());
        const double g10 =
            computeSoundStrength(v, fs, 0, refFrom(ir, fs, 10.0)).g.value;
        for (double dist : { 1.0, 5.0, 10.0, 20.0 }) {
            const SoundStrengthReference ref = refFrom(ir, fs, dist);
            const SoundStrengthResult r = computeSoundStrength(v, fs, 0, ref);
            CHECK(r.g.valid);
            const double expect = -20.0 * std::log10(dist / 10.0);
            CHECK_NEAR(r.distanceCorrectionDb, expect, 1e-12);
            CHECK_NEAR(r.g.value, g10 + expect, 1e-9);
        }
        std::printf("  distance 20 m reference shifts G by %.3f dB\n",
                    -20.0 * std::log10(2.0));
    }

    // ── (4) fs 非依存: 同じ物理信号なら実測/基準の fs が違っても G は同じ ──
    // 矩形パルス (振幅 A, 長さ D 秒) の E = D·A² は fs に依らず厳密。
    {
        const double dRef = 0.010, aRef = 0.2;   // 基準: 10 ms, 0.2
        const double dMes = 0.040, aMes = 0.5;   // 実測: 40 ms, 0.5
        const double expect = 10.0 * std::log10((dMes * aMes * aMes) /
                                                (dRef * aRef * aRef));
        for (double fsRef : { 48000.0, 96000.0 }) {
            for (double fsMes : { 48000.0, 44100.0 }) {
                const std::vector<double> rf = rect(
                    static_cast<std::size_t>(dRef * fsRef + 0.5), aRef);
                const std::vector<double> ms = rect(
                    static_cast<std::size_t>(dMes * fsMes + 0.5), aMes);
                const SoundStrengthReference ref = refFrom(rf, fsRef);
                ArrayView<const double> v(ms.data(), ms.size());
                const SoundStrengthResult r =
                    computeSoundStrength(v, fsMes, 0, ref);
                CHECK(r.g.valid);
                CHECK_NEAR(r.g.value, expect, 1e-9);
            }
        }
        std::printf("  fs-independent G = %.4f dB (48/96k ref, 48/44.1k meas)\n",
                    expect);
    }

    // ── (5) 早期/後期の分解: 10^(Ge/10) + 10^(Gl/10) = 10^(G/10) ──
    {
        const std::vector<double> ir = decay(fs, 0.3, 0.6, 2.0);
        const SoundStrengthReference ref = refFrom(ir, fs);
        ArrayView<const double> v(ir.data(), ir.size());
        const SoundStrengthResult r = computeSoundStrength(v, fs, 0, ref);
        CHECK(r.g.valid);
        CHECK(r.gEarly.valid);
        CHECK(r.gLate.valid);
        const double sum = std::pow(10.0, r.gEarly.value / 10.0)
                         + std::pow(10.0, r.gLate.value / 10.0);
        CHECK_REL(sum, std::pow(10.0, r.g.value / 10.0), 1e-12);
        // 早期は全体より必ず小さい (エネルギーの一部)
        CHECK(r.gEarly.value < r.g.value);
        CHECK(r.gLate.value < r.g.value);
        std::printf("  G = %.3f dB (early %.3f / late %.3f)\n", r.g.value,
                    r.gEarly.value, r.gLate.value);
    }

    // ── (6) 指数減衰の閉形式 (等比級数) ──
    {
        const double tau = 0.25, amp = 0.7, dur = 3.0;
        const std::vector<double> ir = decay(fs, tau, amp, dur);
        // 基準は単一サンプル (E_ref = aRef²/fs)
        const double aRef = 0.9;
        const std::vector<double> rf(1, aRef);
        const SoundStrengthReference ref = refFrom(rf, fs);

        const std::size_t n = ir.size();
        const double q = std::exp(-1.0 / (fs * tau));
        const double eMeas =
            amp * amp / fs * (1.0 - std::pow(q, static_cast<double>(n)))
            / (1.0 - q);
        const double eRef = aRef * aRef / fs;
        const double expect = 10.0 * std::log10(eMeas / eRef);

        ArrayView<const double> v(ir.data(), ir.size());
        const SoundStrengthResult r = computeSoundStrength(v, fs, 0, ref);
        CHECK(r.g.valid);
        std::printf("  decay closed form: G = %.5f dB (theory %.5f)\n",
                    r.g.value, expect);
        CHECK_NEAR(r.g.value, expect, 1e-6);
    }

    // ── (7) directIndex を進めると直接音前のエネルギーが落ちる ──
    {
        std::vector<double> ir(static_cast<std::size_t>(0.5 * fs), 0.0);
        ir[0] = 1.0;                       // 直接音前の雑音とみなす成分
        ir[static_cast<std::size_t>(0.1 * fs)] = 1.0;
        const std::vector<double> rf(1, 1.0);
        const SoundStrengthReference ref = refFrom(rf, fs);
        ArrayView<const double> v(ir.data(), ir.size());
        const SoundStrengthResult a = computeSoundStrength(v, fs, 0, ref);
        const SoundStrengthResult b = computeSoundStrength(
            v, fs, static_cast<std::size_t>(0.1 * fs), ref);
        CHECK(a.g.valid);
        CHECK(b.g.valid);
        CHECK_NEAR(a.g.value, 10.0 * std::log10(2.0), 1e-9);  // 2 サンプル分
        CHECK_NEAR(b.g.value, 0.0, 1e-9);                     // 1 サンプル分
    }

    // ── (8) dB 直接指定の基準が録音経由と一致する ──
    {
        const std::vector<double> ir = decay(fs, 0.2, 0.35, 1.0);
        const SoundStrengthReference viaIr = refFrom(ir, fs);
        ArrayView<const double> v(ir.data(), ir.size());
        const SoundStrengthResult r0 = computeSoundStrength(v, fs, 0, viaIr);

        AcousticResult<SoundStrengthReference> rdb =
            makeSoundStrengthReferenceDb(r0.referenceEnergyDb, 10.0);
        CHECK(rdb.success());
        const SoundStrengthResult r1 =
            computeSoundStrength(v, fs, 0, rdb.value());
        CHECK(r1.g.valid);
        CHECK_NEAR(r1.g.value, r0.g.value, 1e-9);
    }

    // ── (9) 前提を満たさない入力は invalid (黙って値を出さない) ──
    {
        const std::vector<double> ir = decay(fs, 0.2, 0.5, 1.0);
        ArrayView<const double> v(ir.data(), ir.size());

        // 基準なし — 0 dB を出さないこと (「基準と同じ強さ」に見えるため)
        const SoundStrengthResult noRef =
            computeSoundStrength(v, fs, 0, SoundStrengthReference());
        CHECK(!noRef.g.valid);
        CHECK(!noRef.gEarly.valid);
        CHECK(!noRef.gLate.valid);
        // 基準が無くても実測エネルギーの参考値は出る
        CHECK(noRef.measuredEnergyDb > -300.0);

        const SoundStrengthReference ref = refFrom(ir, fs);

        // 無音の実測
        const std::vector<double> silent(static_cast<std::size_t>(fs), 0.0);
        ArrayView<const double> sv(silent.data(), silent.size());
        CHECK(!computeSoundStrength(sv, fs, 0, ref).g.valid);

        // 空入力 / 位置不正 / fs 不正
        ArrayView<const double> empty(nullptr, 0);
        CHECK(!computeSoundStrength(empty, fs, 0, ref).g.valid);
        CHECK(!computeSoundStrength(v, fs, 999999, ref).g.valid);
        CHECK(!computeSoundStrength(v, 0.0, 0, ref).g.valid);

        // 非有限値
        std::vector<double> bad = ir;
        bad[10] = std::sqrt(-1.0);          // NaN
        ArrayView<const double> bv(bad.data(), bad.size());
        CHECK(!computeSoundStrength(bv, fs, 0, ref).g.valid);

        // 80 ms 未満の RIR: G は出るが早期/後期は出さない
        const std::vector<double> shortIr = decay(fs, 0.02, 0.5, 0.05);
        ArrayView<const double> shv(shortIr.data(), shortIr.size());
        const SoundStrengthResult sr = computeSoundStrength(shv, fs, 0, ref);
        CHECK(sr.g.valid);
        CHECK(!sr.gEarly.valid);
        CHECK(!sr.gLate.valid);
    }

    // ── (10) 基準の作成側が不正入力を弾く ──
    {
        const std::vector<double> ir = decay(fs, 0.2, 0.5, 0.5);
        ArrayView<const double> v(ir.data(), ir.size());
        ArrayView<const double> empty(nullptr, 0);
        const std::vector<double> silent(1000, 0.0);
        ArrayView<const double> sv(silent.data(), silent.size());

        CHECK(!makeSoundStrengthReference(empty, fs).success());
        CHECK(!makeSoundStrengthReference(sv, fs).success());
        CHECK(!makeSoundStrengthReference(v, 0.0).success());
        CHECK(!makeSoundStrengthReference(v, fs, 0.0).success());
        CHECK(!makeSoundStrengthReference(v, fs, -1.0).success());
        CHECK(makeSoundStrengthReference(v, fs, 1.0).success());

        CHECK(!makeSoundStrengthReferenceDb(-300.0).success());
        CHECK(!makeSoundStrengthReferenceDb(-1000.0).success());
        CHECK(!makeSoundStrengthReferenceDb(0.0, 0.0).success());
        CHECK(makeSoundStrengthReferenceDb(-40.0, 10.0).success());
    }

    return testutil::summary("test_strength");
}
