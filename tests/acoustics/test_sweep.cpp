// test_sweep.cpp — 指数掃引正弦波 (ESS) の生成と逆畳み込みの検証。
//
// 期待値はすべて Farina (AES 108th Convention, Preprint 5093, 2000) の
// 定義から独立に導き、テスト側で計算する:
//   - 掃引の瞬時周波数 f(t) = f1·(f2/f1)^{t/T} — ゼロ交差間隔から測る
//   - 掃引 ⊛ 逆フィルタ = 振幅 1 の単一インパルス (正規化の定義)
//   - 既知の IR を掃引に畳み込んで録音を作り、逆畳み込みで **その IR が
//     戻る** こと (これが本題)。ただし戻るのは掃引の帯域に制限された IR で、
//     デルタは復元できない — 判定は「復元結果 == h ⊛ 帯域制限核」で行う
//   - 2 乗歪みを加えると 2 次高調波が Δt2 = T·ln2/ln(f2/f1) だけ前に出る
#include <cmath>
#include <cstdio>
#include <vector>

#include "../../src/acoustics/core/SweepDeconvolution.h"
#include "test_common.h"

using namespace ofd::acoustics;

namespace {

SweepSpec makeSpec() {
    SweepSpec s;
    s.startHz = 100.0;
    s.endHz = 8000.0;
    s.durationSec = 1.0;
    s.sampleRateHz = 48000.0;
    s.amplitude = 0.5;
    s.fadeInSec = 0.0;      // 検証しやすいようフェード無し
    s.fadeOutSec = 0.0;
    return s;
}

// ── 仕様の検証 (不正入力を通さない) ────────────────────────────────────────
void testSpecValidation() {
    std::printf("-- sweep specification validation\n");
    CHECK(makeSpec().valid());

    SweepSpec s = makeSpec();
    s.endHz = s.startHz;               CHECK(!s.valid());
    s = makeSpec(); s.startHz = 0.0;   CHECK(!s.valid());
    s = makeSpec(); s.endHz = 30000.0; CHECK(!s.valid());  // ナイキスト超
    s = makeSpec(); s.durationSec = 0; CHECK(!s.valid());
    s = makeSpec(); s.sampleRateHz = 0; CHECK(!s.valid());

    CHECK(generateSweep(SweepSpec()).channels.size() == 1);   // 既定は有効
    s = makeSpec(); s.endHz = s.startHz;
    CHECK(generateSweep(s).channels.empty());                 // 不正は空
    CHECK(sweepInverseFilter(s).empty());
}

// ── 掃引の瞬時周波数が f1·(f2/f1)^{t/T} であること ─────────────────────────
void testSweepFrequency() {
    std::printf("-- instantaneous frequency follows the exponential law\n");
    const SweepSpec s = makeSpec();
    const AudioBuffer sw = generateSweep(s);
    CHECK(sw.channels.size() == 1);
    CHECK(sw.channels[0].size() == std::size_t(s.durationSec * s.sampleRateHz));

    const std::vector<double> &x = sw.channels[0];
    double pk = 0.0;
    for (std::size_t i = 0; i < x.size(); ++i) pk = std::max(pk, std::fabs(x[i]));
    CHECK_NEAR(pk, s.amplitude, 1e-3);

    // 各時点の周辺でゼロ交差を数えて周波数を推定する (窓 = 0.05 s)
    const double win = 0.05;
    const std::size_t w = std::size_t(win * s.sampleRateHz);
    const double ratio = s.endHz / s.startHz;
    for (double t : { 0.2, 0.5, 0.8 }) {
        const std::size_t a = std::size_t(t * s.sampleRateHz);
        if (a + w >= x.size()) continue;
        int cross = 0;
        for (std::size_t i = a + 1; i < a + w; ++i)
            if ((x[i - 1] < 0.0) != (x[i] < 0.0)) ++cross;
        const double measured = double(cross) / (2.0 * win);
        // 窓の中で周波数が動くので、比べる相手は瞬時値ではなく **窓平均**:
        //   (1/w)·∫_t^{t+w} f1·r^{s/T} ds = f1·T/(w·L)·(r^{(t+w)/T} − r^{t/T})
        const double L = std::log(ratio);
        const double expected =
            s.startHz * s.durationSec / (win * L)
            * (std::pow(ratio, (t + win) / s.durationSec)
               - std::pow(ratio, t / s.durationSec));
        std::printf("   t=%.1f s: measured %.1f Hz, window mean %.1f Hz "
                    "(%+.2f %%)\n", t, measured, expected,
                    100.0 * (measured - expected) / expected);
        // ゼロ交差の数え上げは 1 交差 = 1/(2·win) Hz の量子化があるので、
        // 判定は相対値ではなく **量子化幅 2 交差ぶん** の絶対許容で行う
        CHECK_NEAR(measured, expected, 1.0 / win);
    }
}

// ── 掃引 ⊛ 逆フィルタ = 振幅 1 の単一インパルス ────────────────────────────
void testInverseFilterNormalisation() {
    std::printf("-- sweep convolved with its inverse gives a unit impulse\n");
    const SweepSpec s = makeSpec();
    // 掃引そのものを「録音」として逆畳み込みする (系 = 恒等)
    SweepSpec raw = s;
    raw.amplitude = 1.0;
    const AudioBuffer sw = generateSweep(raw);
    const ofd::acoustics::AcousticResult<SweepDeconvolutionResult> r =
        deconvolveSweep(ArrayView<const double>(sw.channels[0].data(),
                                                sw.channels[0].size()), s, 0);
    CHECK(r.success());
    if (!r.success()) return;
    const SweepDeconvolutionResult &d = r.value();
    CHECK(d.valid);
    CHECK(!d.linear.empty());

    // ピークは linearIndex ちょうど、振幅 1
    double pk = 0.0;
    std::size_t at = 0;
    for (std::size_t i = 0; i < d.response.size(); ++i)
        if (std::fabs(d.response[i]) > pk) { pk = std::fabs(d.response[i]); at = i; }
    std::printf("   peak %.6f at index %zu (linearIndex %zu)\n", pk, at,
                d.linearIndex);
    CHECK_NEAR(pk, 1.0, 1e-6);
    CHECK(at == d.linearIndex);
}

// ── 既知の IR が逆畳み込みで戻ること (本題) ────────────────────────────────
// 掃引が 100 Hz〜8 kHz なので、逆畳み込みで戻るのは **その帯域に制限された**
// インパルス応答である (デルタは復元できない — 帯域外の情報が録音に無い)。
// したがって判定はこう組む:
//   ① 系の核 k = 掃引そのものの逆畳み込み (帯域制限インパルス) を測り、
//      これがデルタではないことを確認する (この制約自体を明示する)
//   ② 逆畳み込みが線形時不変であること — 復元結果が h ⊛ k と一致すること。
//      これが成り立てば「IR がそのまま戻る (帯域内で)」と言える
//   ③ 直接音・反射の位置と符号が h のタップどおりであること
void testRecoversKnownImpulseResponse() {
    std::printf("-- a known impulse response is recovered (within the band)\n");
    const SweepSpec s = makeSpec();
    SweepSpec raw = s;
    raw.amplitude = 1.0;
    const AudioBuffer sw = generateSweep(raw);
    const std::vector<double> &x = sw.channels[0];

    // ① 帯域制限された核 k
    const ofd::acoustics::AcousticResult<SweepDeconvolutionResult> kr =
        deconvolveSweep(ArrayView<const double>(x.data(), x.size()), s, 0);
    CHECK(kr.success());
    if (!kr.success()) return;
    const std::vector<double> &k = kr.value().linear;
    CHECK(!k.empty());
    CHECK_NEAR(k[0], 1.0, 1e-6);           // 中心は振幅 1
    // 100 Hz 下端の帯域制限なので ±10 ms 前後にリンギングが残る。
    // 「デルタが戻る」と誤解しないよう、その大きさを検証で固定しておく。
    double ring = 0.0;
    for (std::size_t i = 240; i < k.size() && i < 2400; ++i)
        ring = std::max(ring, std::fabs(k[i]));
    std::printf("   band-limited kernel: peak %.4f, ringing beyond 5 ms %.4f "
                "(%.1f dB)\n", k[0], ring, 20.0 * std::log10(ring / k[0]));
    CHECK(ring > 0.01);   // デルタではない (帯域制限の事実)
    CHECK(ring < 0.30);

    // 既知の IR: 直接音 + 2 つの反射 (振幅も遅延もテスト側が決めた値)
    std::vector<double> h(2400, 0.0);            // 50 ms
    h[0] = 1.0;
    h[480] = -0.5;                               // 10 ms 後
    h[1200] = 0.25;                              // 25 ms 後

    // 録音 = 掃引 ⊛ IR (素朴な直和。FFT 実装と独立にするため)
    std::vector<double> rec(x.size() + h.size() - 1, 0.0);
    for (std::size_t i = 0; i < h.size(); ++i) {
        if (h[i] == 0.0) continue;
        for (std::size_t n = 0; n < x.size(); ++n) rec[n + i] += x[n] * h[i];
    }

    const ofd::acoustics::AcousticResult<SweepDeconvolutionResult> r =
        deconvolveSweep(ArrayView<const double>(rec.data(), rec.size()), s, 0);
    CHECK(r.success());
    if (!r.success()) return;
    const SweepDeconvolutionResult &d = r.value();
    CHECK(d.linear.size() >= h.size());
    if (d.linear.size() < h.size()) return;

    // ② 線形時不変性: 復元 == h ⊛ (掃引単体の逆畳み込み)。
    // 核は **非因果的** (帯域制限のリンギングがピークの前にも出る) なので、
    // linear (= linearIndex 以降) ではなく response 全体で畳み込む。
    const std::vector<double> &kFull = kr.value().response;
    double worst = 0.0;
    const std::size_t lo = d.linearIndex > 4000 ? d.linearIndex - 4000 : 0;
    const std::size_t hi = std::min(d.response.size(), d.linearIndex + 4000);
    for (std::size_t n = lo; n < hi; ++n) {
        double expect = 0.0;
        for (std::size_t i = 0; i < h.size(); ++i) {
            if (h[i] == 0.0 || i > n) continue;
            if (n - i < kFull.size()) expect += h[i] * kFull[n - i];
        }
        worst = std::max(worst, std::fabs(d.response[n] - expect));
    }
    std::printf("   worst |recovered - (h * sweep-kernel)| over ±83 ms = %.2e\n",
                worst);
    CHECK(worst < 1e-9);

    // ③ タップの位置と符号
    std::printf("   taps at 0 / 10 ms / 25 ms: %.4f / %.4f / %.4f "
                "(nominal 1 / -0.5 / 0.25)\n",
                d.linear[0], d.linear[480], d.linear[1200]);
    CHECK(d.linear[0] > 0.9);
    CHECK(d.linear[480] < -0.4);
    CHECK(d.linear[1200] > 0.2);
    // 直接音が全体のピークで、その位置は 0 (時間原点がずれていない)
    double pk = 0.0;
    std::size_t at = 0;
    for (std::size_t i = 0; i < 2400; ++i)
        if (std::fabs(d.linear[i]) > pk) { pk = std::fabs(d.linear[i]); at = i; }
    CHECK(at == 0);
}

// ── 高調波が Δt_N = T·ln(N)/ln(f2/f1) だけ前に出ること ─────────────────────
void testHarmonicSeparation() {
    std::printf("-- harmonics appear ahead of the linear response\n");
    // 高調波の分離には掃引を長くしてフェードを付ける (次数間の間隔が広がり、
    // 掃引端の不連続による前後リンギングが減る — 実測でもこうする)
    SweepSpec s = makeSpec();
    s.durationSec = 2.0;
    s.fadeInSec = 0.02;
    s.fadeOutSec = 0.06;

    // 遅延の閉形式 (テスト側で独立に計算)
    const double L = std::log(s.endHz / s.startHz);
    for (int k = 2; k <= 4; ++k) {
        const double expect = s.durationSec * std::log(double(k)) / L;
        CHECK_NEAR(harmonicDelaySec(s, k), expect, 1e-12);
    }
    CHECK(harmonicDelaySec(s, 1) == 0.0);

    SweepSpec raw = s;
    raw.amplitude = 1.0;
    const AudioBuffer sw = generateSweep(raw);
    const std::vector<double> &x = sw.channels[0];

    // 非線形系: y = x + 0.2·x² (2 次歪みを既知の量だけ入れる)
    std::vector<double> rec(x.size());
    for (std::size_t n = 0; n < x.size(); ++n) rec[n] = x[n] + 0.2 * x[n] * x[n];

    const ofd::acoustics::AcousticResult<SweepDeconvolutionResult> r =
        deconvolveSweep(ArrayView<const double>(rec.data(), rec.size()), s, 4);
    CHECK(r.success());
    if (!r.success()) return;
    const SweepDeconvolutionResult &d = r.value();
    CHECK(!d.harmonics.empty());

    bool found2 = false;
    for (std::size_t i = 0; i < d.harmonics.size(); ++i) {
        const HarmonicComponent &h = d.harmonics[i];
        if (!h.separable) continue;
        const double aheadSec =
            double(d.linearIndex - h.index) / s.sampleRateHz;
        std::printf("   order %d: %.4f s ahead (expected %.4f s), %.1f dBc\n",
                    h.order, aheadSec, harmonicDelaySec(s, h.order), h.levelDbc);
        if (h.order == 2) {
            found2 = true;
            // ピーク位置が閉形式と 1 ms 以内で一致
            CHECK_NEAR(aheadSec, harmonicDelaySec(s, 2), 1.0e-3);
            // 2 次歪みが実在するので線形応答より下、かつ無視できない大きさ
            CHECK(h.levelDbc < 0.0 && h.levelDbc > -60.0);
        }
    }
    CHECK(found2);
    CHECK(d.thdValid && d.thdPercent > 0.0);
    std::printf("   THD = %.2f %%\n", d.thdPercent);

    // 線形系 (歪み無し) では 2 次高調波が桁違いに小さい
    const ofd::acoustics::AcousticResult<SweepDeconvolutionResult> clean =
        deconvolveSweep(ArrayView<const double>(x.data(), x.size()), s, 4);
    CHECK(clean.success());
    if (clean.success()) {
        double lin2 = 0.0, dist2 = 0.0;
        for (std::size_t i = 0; i < clean.value().harmonics.size(); ++i)
            if (clean.value().harmonics[i].order == 2)
                lin2 = clean.value().harmonics[i].levelDbc;
        for (std::size_t i = 0; i < d.harmonics.size(); ++i)
            if (d.harmonics[i].order == 2) dist2 = d.harmonics[i].levelDbc;
        std::printf("   2nd harmonic: linear system %.1f dBc, "
                    "distorted system %.1f dBc\n", lin2, dist2);
        // 線形系の「2 次高調波」窓に見えるのは帯域制限核のリンギングで、
        // 本物の歪みより十分小さい (実測 24 dB 差)
        CHECK(lin2 < dist2 - 15.0);
    }
}

// ── 不正入力 ───────────────────────────────────────────────────────────────
void testErrors() {
    std::printf("-- invalid inputs are rejected\n");
    const SweepSpec s = makeSpec();
    CHECK(!deconvolveSweep(ArrayView<const double>(), s, 4).success());

    std::vector<double> tiny(10, 0.0);
    SweepSpec bad = s;
    bad.endHz = bad.startHz;
    CHECK(!deconvolveSweep(ArrayView<const double>(tiny.data(), tiny.size()),
                           bad, 4).success());

    // 全 0 の録音 → 復元できるものが無いのでエラー
    std::vector<double> zeros(std::size_t(s.durationSec * s.sampleRateHz), 0.0);
    CHECK(!deconvolveSweep(ArrayView<const double>(zeros.data(), zeros.size()),
                           s, 4).success());
}

} // namespace

int main() {
    std::printf("== exponential sine sweep deconvolution ==\n");
    testSpecValidation();
    testSweepFrequency();
    testInverseFilterNormalisation();
    testRecoversKnownImpulseResponse();
    testHarmonicSeparation();
    testErrors();
    return testutil::summary("sweep");
}
