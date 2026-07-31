// test_formant.cpp — FormantEstimator (LPC F1/F2/F3) を検証する。
//
//   (a) Levinson-Durbin: 既知 AR(2) の理論自己相関 (Yule-Walker の厳密解)
//       から係数を 1e-6 で回復。高次 (p=4) でも余剰係数 ≈ 0
//   (b) Durand-Kerner 法 (決定的初期値 (0.4+0.9i)^k、乱数不使用):
//       実根 / 複素共役根 / 非モニック / 5 次 / 1 次 / 重根 / 異常系
//   (c) 合成母音 /a/ 相当 (F0=120 Hz、F1=700/F2=1200/F3=2600 Hz、
//       B=80/90/120 Hz の全極モデル = 2 次共振カスケード) → F1/F2/F3 が
//       公称値の ±10%。48 kHz → 内部 fs 9.6 kHz (1/5 間引き)、p=12
//   (d) 合成母音 /i/ 相当 (F0=100 Hz、F1=300/F2=2300 Hz) → ±10%
//   (e) 白色雑音 (無声) → 代表値 invalid (有声フレームなし)
//   (f) フレーム時刻が F0 軌跡と一致し、無声フレームでは推定しないこと
#include <cmath>
#include <complex>
#include <cstdio>
#include <vector>

#include "../../src/acoustics/core/FormantEstimator.h"
#include "../../src/acoustics/core/VocalAnalyzer.h"
#include "test_common.h"

using namespace ofd::acoustics;

namespace {

const double kPi = 3.14159265358979323846;
const double kFs = 48000.0;

ArrayView<const double> view(const std::vector<double> &v) {
    return ArrayView<const double>(v.data(), v.size());
}

// 一様白色雑音 ([-amp, amp])
std::vector<double> makeNoise(double amp, double seconds, unsigned seed) {
    const std::size_t n = static_cast<std::size_t>(seconds * kFs + 0.5);
    std::vector<double> x(n);
    unsigned st = seed;
    for (std::size_t i = 0; i < n; ++i) x[i] = amp * testutil::lcgUniform(st);
    return x;
}

// 合成母音: インパルス列 (F0) → 2 次共振カスケード (全極モデル)。
// 各共振は極半径 r = exp(−πB/fs)・角度 θ = 2πF/fs の
//   y[n] = x[n] + 2r·cosθ·y[n−1] − r²·y[n−2]
// で、周波数 F・−3 dB 帯域幅 B の共鳴を与える。決定論的 (乱数不使用)。
std::vector<double> makeVowel(double f0Hz, const double *freqHz,
                              const double *bwHz, int formantCount,
                              double seconds) {
    const std::size_t n = static_cast<std::size_t>(seconds * kFs + 0.5);
    std::vector<double> x(n, 0.0);
    for (std::size_t k = 0;; ++k) {
        const std::size_t idx =
            static_cast<std::size_t>(static_cast<double>(k) * kFs / f0Hz + 0.5);
        if (idx >= n) break;
        x[idx] = 1.0;
    }
    for (int f = 0; f < formantCount; ++f) {
        const double r = std::exp(-kPi * bwHz[f] / kFs);
        const double c = 2.0 * r * std::cos(2.0 * kPi * freqHz[f] / kFs);
        const double r2 = r * r;
        double y1 = 0.0, y2 = 0.0;
        for (std::size_t i = 0; i < n; ++i) {
            const double y = x[i] + c * y1 - r2 * y2;
            y2 = y1;
            y1 = y;
            x[i] = y;
        }
    }
    // ピークを 0.5 に正規化 (ノイズゲート −70 dBFS を確実に超える)
    double peak = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double a = std::fabs(x[i]);
        if (a > peak) peak = a;
    }
    if (peak > 0.0) {
        const double g = 0.5 / peak;
        for (std::size_t i = 0; i < n; ++i) x[i] *= g;
    }
    return x;
}

// 根リストに z0 と ±tol で一致する根が 1 つ以上あるか
bool hasRoot(const std::vector<std::complex<double> > &roots,
             std::complex<double> z0, double tol) {
    for (std::size_t i = 0; i < roots.size(); ++i) {
        if (std::abs(roots[i] - z0) <= tol) return true;
    }
    return false;
}

} // namespace

int main() {
    // ── (a) Levinson-Durbin: AR(2) 係数回復 ──
    {
        // AR(2): x[n] = φ1·x[n−1] + φ2·x[n−2] + e[n] (定常・安定)。
        // 理論自己相関 (Yule-Walker): ρ1 = φ1/(1−φ2)、
        // ρk = φ1·ρ(k−1) + φ2·ρ(k−2)。r0 = 1 に正規化。
        const double phi1 = 0.75, phi2 = -0.5;
        std::vector<double> r(5);
        r[0] = 1.0;
        r[1] = phi1 / (1.0 - phi2);
        for (int k = 2; k <= 4; ++k)
            r[static_cast<std::size_t>(k)] =
                phi1 * r[static_cast<std::size_t>(k - 1)] +
                phi2 * r[static_cast<std::size_t>(k - 2)];

        std::vector<double> a;
        double err = 0.0;
        CHECK(levinsonDurbin(r, 2, a, err));
        CHECK(a.size() == 3);
        if (a.size() == 3) {
            // A(z) = 1 − φ1 z^-1 − φ2 z^-2 → a[1] = −φ1, a[2] = −φ2
            CHECK_NEAR(a[0], 1.0, 1e-12);
            CHECK_NEAR(a[1], -phi1, 1e-6);
            CHECK_NEAR(a[2], -phi2, 1e-6);
            std::printf("  (a) AR(2) recovery: a1=%.9f (want %.4f) "
                        "a2=%.9f (want %.4f) err=%.6f\n",
                        -a[1], phi1, -a[2], phi2, err);
        }
        CHECK(err > 0.0);

        // 高次 (p=4) でも AR(2) 部分を回復し余剰係数 ≈ 0
        std::vector<double> a4;
        double err4 = 0.0;
        CHECK(levinsonDurbin(r, 4, a4, err4));
        if (a4.size() == 5) {
            CHECK_NEAR(a4[1], -phi1, 1e-6);
            CHECK_NEAR(a4[2], -phi2, 1e-6);
            CHECK_NEAR(a4[3], 0.0, 1e-6);
            CHECK_NEAR(a4[4], 0.0, 1e-6);
        }

        // 異常系: r[0] <= 0 / 次数 0 / 配列長不足
        std::vector<double> bad(3, 0.0);
        std::vector<double> tmp;
        double e = 0.0;
        CHECK(!levinsonDurbin(bad, 2, tmp, e));
        CHECK(!levinsonDurbin(r, 0, tmp, e));
        CHECK(!levinsonDurbin(std::vector<double>(2, 1.0), 3, tmp, e));
    }

    // ── (b) Durand-Kerner 法 ──
    {
        typedef std::complex<double> C;
        std::vector<C> roots;

        // 実根: z² − 3z + 2 = (z−1)(z−2)
        {
            const double c[] = {1.0, -3.0, 2.0};
            CHECK(durandKernerRoots(std::vector<double>(c, c + 3), roots));
            CHECK(roots.size() == 2);
            CHECK(hasRoot(roots, C(1.0, 0.0), 1e-9));
            CHECK(hasRoot(roots, C(2.0, 0.0), 1e-9));
        }
        // 複素共役根: z² + 1 = 0 → ±i
        {
            const double c[] = {1.0, 0.0, 1.0};
            CHECK(durandKernerRoots(std::vector<double>(c, c + 3), roots));
            CHECK(hasRoot(roots, C(0.0, 1.0), 1e-9));
            CHECK(hasRoot(roots, C(0.0, -1.0), 1e-9));
        }
        // 非モニック: 2z² − 6z + 4 → 根は同じ 1, 2
        {
            const double c[] = {2.0, -6.0, 4.0};
            CHECK(durandKernerRoots(std::vector<double>(c, c + 3), roots));
            CHECK(hasRoot(roots, C(1.0, 0.0), 1e-9));
            CHECK(hasRoot(roots, C(2.0, 0.0), 1e-9));
        }
        // 5 次: (z−1)(z+1)(z−2)(z+2)(z−3) = z⁵ −3z⁴ −5z³ +15z² +4z −12
        {
            const double c[] = {1.0, -3.0, -5.0, 15.0, 4.0, -12.0};
            CHECK(durandKernerRoots(std::vector<double>(c, c + 6), roots));
            CHECK(roots.size() == 5);
            CHECK(hasRoot(roots, C(1.0, 0.0), 1e-8));
            CHECK(hasRoot(roots, C(-1.0, 0.0), 1e-8));
            CHECK(hasRoot(roots, C(2.0, 0.0), 1e-8));
            CHECK(hasRoot(roots, C(-2.0, 0.0), 1e-8));
            CHECK(hasRoot(roots, C(3.0, 0.0), 1e-8));
        }
        // LPC 極と同形の共役対: z² − 2r·cosθ·z + r² (r=0.95, θ=0.3)
        {
            const double r = 0.95, th = 0.3;
            const double c[] = {1.0, -2.0 * r * std::cos(th), r * r};
            CHECK(durandKernerRoots(std::vector<double>(c, c + 3), roots));
            CHECK(hasRoot(roots, C(r * std::cos(th), r * std::sin(th)), 1e-9));
            CHECK(hasRoot(roots, C(r * std::cos(th), -r * std::sin(th)), 1e-9));
        }
        // 1 次: 2z − 4 → z = 2。先頭 0 係数は除去される: {0, 2, −4} も同じ
        {
            const double c[] = {2.0, -4.0};
            CHECK(durandKernerRoots(std::vector<double>(c, c + 2), roots));
            CHECK(roots.size() == 1);
            CHECK(hasRoot(roots, C(2.0, 0.0), 1e-9));
            const double c0[] = {0.0, 2.0, -4.0};
            CHECK(durandKernerRoots(std::vector<double>(c0, c0 + 3), roots));
            CHECK(roots.size() == 1);
            CHECK(hasRoot(roots, C(2.0, 0.0), 1e-9));
        }
        // 重根: (z−1)² (収束は残差判定で扱う — 精度は緩めに ±1e-3)
        {
            const double c[] = {1.0, -2.0, 1.0};
            CHECK(durandKernerRoots(std::vector<double>(c, c + 3), roots));
            CHECK(roots.size() == 2);
            CHECK(hasRoot(roots, C(1.0, 0.0), 1e-3));
        }
        // 異常系: 空 / 定数 / 全て 0 → false
        {
            CHECK(!durandKernerRoots(std::vector<double>(), roots));
            CHECK(!durandKernerRoots(std::vector<double>(1, 5.0), roots));
            CHECK(!durandKernerRoots(std::vector<double>(3, 0.0), roots));
        }
        std::printf("  (b) Durand-Kerner: real/conjugate/quintic/multiple/"
                    "degenerate OK\n");
    }

    // ── (c) 合成母音 /a/ 相当: F1=700 / F2=1200 / F3=2600 (±10%) ──
    {
        const double freqs[] = {700.0, 1200.0, 2600.0};
        const double bws[] = {80.0, 90.0, 120.0};
        const std::vector<double> x = makeVowel(120.0, freqs, bws, 3, 1.5);
        VocalAnalyzer az;
        AcousticResult<VocalAnalysisResult> r = az.analyze(view(x), kFs);
        CHECK(r.success());
        const FormantTrackResult &fo = r.value().formants;

        // 48 kHz → 1/5 間引き = 9.6 kHz、p = 2 + round(9.6) = 12
        CHECK(fo.decimationFactor == 5);
        CHECK_NEAR(fo.internalRateHz, 9600.0, 1e-9);
        CHECK(fo.lpcOrder == 12);

        CHECK(fo.f1MedianHz.valid);
        CHECK(fo.f2MedianHz.valid);
        CHECK(fo.f3MedianHz.valid);
        if (fo.f1MedianHz.valid) CHECK_REL(fo.f1MedianHz.value, 700.0, 0.10);
        if (fo.f2MedianHz.valid) CHECK_REL(fo.f2MedianHz.value, 1200.0, 0.10);
        if (fo.f3MedianHz.valid) CHECK_REL(fo.f3MedianHz.value, 2600.0, 0.10);
        std::printf("  (c) /a/: F1=%.1f (700) F2=%.1f (1200) F3=%.1f (2600) "
                    "Hz [fsInt=%.0f p=%d]\n",
                    fo.f1MedianHz.value, fo.f2MedianHz.value,
                    fo.f3MedianHz.value, fo.internalRateHz, fo.lpcOrder);
    }

    // ── (d) 合成母音 /i/ 相当: F1=300 / F2=2300 (±10%) ──
    {
        const double freqs[] = {300.0, 2300.0};
        const double bws[] = {80.0, 100.0};
        const std::vector<double> x = makeVowel(100.0, freqs, bws, 2, 1.5);
        VocalAnalyzer az;
        AcousticResult<VocalAnalysisResult> r = az.analyze(view(x), kFs);
        CHECK(r.success());
        const FormantTrackResult &fo = r.value().formants;
        CHECK(fo.f1MedianHz.valid);
        CHECK(fo.f2MedianHz.valid);
        if (fo.f1MedianHz.valid) CHECK_REL(fo.f1MedianHz.value, 300.0, 0.10);
        if (fo.f2MedianHz.valid) CHECK_REL(fo.f2MedianHz.value, 2300.0, 0.10);
        std::printf("  (d) /i/: F1=%.1f (300) F2=%.1f (2300) Hz\n",
                    fo.f1MedianHz.value, fo.f2MedianHz.value);
    }

    // ── (e) 白色雑音 (無声) → 代表値 invalid ──
    {
        const std::vector<double> x = makeNoise(0.3, 1.5, 20260716u);
        VocalAnalyzer az;
        AcousticResult<VocalAnalysisResult> r = az.analyze(view(x), kFs);
        CHECK(r.success());
        const VocalAnalysisResult &res = r.value();
        // 無声 (YIN が有声と誤判定しない前提は test_vocal (c) と同じ)
        CHECK(res.voicedRatio < 0.1);
        const FormantTrackResult &fo = res.formants;
        CHECK(!fo.f1MedianHz.valid);
        CHECK(!fo.f2MedianHz.valid);
        CHECK(!fo.f3MedianHz.valid);
        CHECK(!fo.f1MedianHz.warning.empty());
        std::printf("  (e) noise: F1 valid=%d (%s)\n",
                    fo.f1MedianHz.valid ? 1 : 0, fo.f1MedianHz.warning.c_str());
    }

    // ── (f) フレーム整合: 時刻が F0 軌跡と一致 / 無声フレームは非推定 ──
    {
        const double freqs[] = {700.0, 1200.0};
        const double bws[] = {80.0, 90.0};
        const std::vector<double> x = makeVowel(120.0, freqs, bws, 2, 1.0);
        VocalAnalyzer az;
        AcousticResult<VocalAnalysisResult> r = az.analyze(view(x), kFs);
        CHECK(r.success());
        const VocalAnalysisResult &res = r.value();
        CHECK(res.formants.frames.size() == res.f0Track.size());
        bool timesMatch = true;
        bool unvoicedClean = true;
        std::size_t validCount = 0;
        for (std::size_t i = 0; i < res.formants.frames.size() &&
                                i < res.f0Track.size(); ++i) {
            const FormantFrame &ff = res.formants.frames[i];
            if (std::fabs(ff.timeSeconds - res.f0Track[i].timeSeconds) > 1e-12)
                timesMatch = false;
            if (ff.voiced != res.f0Track[i].voiced) unvoicedClean = false;
            if (!res.f0Track[i].voiced &&
                (ff.valid || ff.f1Hz != 0.0 || ff.f2Hz != 0.0))
                unvoicedClean = false;
            if (ff.valid) ++validCount;
        }
        CHECK(timesMatch);
        CHECK(unvoicedClean);
        CHECK(validCount > 0);
        std::printf("  (f) frames=%zu valid=%zu (times/voiced consistent)\n",
                    res.formants.frames.size(), validCount);
    }

    return testutil::summary("test_formant");
}
