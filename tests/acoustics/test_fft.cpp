// test_fft.cpp — 自前 radix-2 FFT (Fft.{h,cpp}) の検証。
//
// 規約 (Fft.h): 順変換 X[k] = Σ x[n]·exp(-j2πkn/N) (正規化なし)、
// 逆変換で 1/N 正規化。従って:
//   (a) 振幅 A・整数周期 k の正弦波 → |X[k]| = A·N/2 (窓なし、漏れなし)
//   (b) forward→inverse の往復は恒等 (誤差 < 1e-12)
//   (c) パーセバル: Σ|x[n]|² = (1/N)·Σ|X[k]|²
//   (d) 非 2 冪長: fftForward/fftInverse は false (入力不変)、
//       realFft はゼロ詰めで 2 の冪長に揃える
#include <cmath>
#include <complex>
#include <cstdio>
#include <vector>

#include "../../src/acoustics/core/Fft.h"
#include "test_common.h"

using namespace ofd::acoustics;

namespace {
const double kPi = 3.14159265358979323846;
} // namespace

int main() {
    // ── ユーティリティ (nextPowerOfTwo / isPowerOfTwo) ──
    CHECK(nextPowerOfTwo(0) == 1);
    CHECK(nextPowerOfTwo(1) == 1);
    CHECK(nextPowerOfTwo(1000) == 1024);
    CHECK(nextPowerOfTwo(1024) == 1024);
    CHECK(nextPowerOfTwo(1025) == 2048);
    CHECK(isPowerOfTwo(1));
    CHECK(isPowerOfTwo(4096));
    CHECK(!isPowerOfTwo(0));
    CHECK(!isPowerOfTwo(1000));

    // ── (a) 単一正弦波: N=1024 内に整数周期 k=37、振幅 A=0.8 ──
    {
        const std::size_t n = 1024;
        const std::size_t kBin = 37; // 整数周期 → 漏れなし
        const double amp = 0.8;
        std::vector<std::complex<double>> x(n);
        for (std::size_t i = 0; i < n; ++i) {
            const double ph = 2.0 * kPi * static_cast<double>(kBin) *
                              static_cast<double>(i) / static_cast<double>(n);
            x[i] = std::complex<double>(amp * std::sin(ph), 0.0);
        }
        CHECK(fftForward(x));

        // 正の周波数側 (1..N/2) のピーク位置は k=37
        std::size_t peak = 1;
        for (std::size_t k = 2; k <= n / 2; ++k) {
            if (std::abs(x[k]) > std::abs(x[peak])) peak = k;
        }
        CHECK(peak == kBin);

        // 正規化なし順変換なので |X[k]| = A·N/2 (負周波数側 N-k も同値)
        const double expectedMag = amp * static_cast<double>(n) / 2.0;
        CHECK_NEAR(std::abs(x[kBin]), expectedMag, 1e-9);
        CHECK_NEAR(std::abs(x[n - kBin]), expectedMag, 1e-9);
        // sin の X[k] は純虚数 -j·A·N/2
        CHECK_NEAR(x[kBin].real(), 0.0, 1e-9);
        CHECK_NEAR(x[kBin].imag(), -expectedMag, 1e-9);
        // 他のビンはほぼゼロ (DC と隣接ビンで代表確認)
        CHECK_NEAR(std::abs(x[0]), 0.0, 1e-9);
        CHECK_NEAR(std::abs(x[kBin - 1]), 0.0, 1e-9);
        CHECK_NEAR(std::abs(x[kBin + 1]), 0.0, 1e-9);
        std::printf("  peak bin=%zu |X|=%.6f (theory %.6f)\n", peak,
                    std::abs(x[kBin]), expectedMag);
    }

    // ── (b) 往復誤差 と (c) パーセバル (決定的乱数の複素信号 N=2048) ──
    {
        const std::size_t n = 2048;
        unsigned st = 12345u;
        std::vector<std::complex<double>> orig(n);
        for (std::size_t i = 0; i < n; ++i) {
            orig[i] = std::complex<double>(testutil::lcgUniform(st),
                                           testutil::lcgUniform(st));
        }

        // 時間領域エネルギー Σ|x|²
        double timeEnergy = 0.0;
        for (std::size_t i = 0; i < n; ++i) timeEnergy += std::norm(orig[i]);

        std::vector<std::complex<double>> x = orig;
        CHECK(fftForward(x));

        // (c) パーセバル: Σ|x[n]|² = (1/N)·Σ|X[k]|²
        double freqEnergy = 0.0;
        for (std::size_t k = 0; k < n; ++k) freqEnergy += std::norm(x[k]);
        freqEnergy /= static_cast<double>(n);
        CHECK_REL(freqEnergy, timeEnergy, 1e-12);

        // (b) 往復: forward → inverse で元信号に一致 (< 1e-12)
        CHECK(fftInverse(x));
        double maxErr = 0.0;
        for (std::size_t i = 0; i < n; ++i) {
            const double e = std::abs(x[i] - orig[i]);
            if (e > maxErr) maxErr = e;
        }
        CHECK(maxErr < 1e-12);
        std::printf("  roundtrip max err=%.3e, Parseval %.15g vs %.15g\n",
                    maxErr, timeEnergy, freqEnergy);
    }

    // ── (d) 非 2 冪長の扱い ──
    {
        // fftForward / fftInverse は false を返し、入力を変更しない
        const std::size_t n = 1000; // 2 の冪ではない
        unsigned st = 777u;
        std::vector<std::complex<double>> x(n);
        for (std::size_t i = 0; i < n; ++i) {
            x[i] = std::complex<double>(testutil::lcgUniform(st), 0.0);
        }
        const std::vector<std::complex<double>> copy = x;
        CHECK(!fftForward(x));
        CHECK(!fftInverse(x));
        bool unchanged = true;
        for (std::size_t i = 0; i < n; ++i) {
            if (x[i] != copy[i]) unchanged = false;
        }
        CHECK(unchanged);

        // realFft は次の 2 の冪 (1024) にゼロ詰めして成功する
        std::vector<double> r(n, 0.0);
        for (std::size_t i = 0; i < n; ++i) r[i] = 1.0; // 定数信号
        AcousticResult<std::vector<std::complex<double>>> sp =
            realFft(ArrayView<const double>(r.data(), r.size()));
        CHECK(sp.success());
        CHECK(sp.value().size() == 1024);
        // 定数 1 × 1000 サンプル → X[0] = 1000 (ゼロ詰めは DC に影響しない)
        CHECK_NEAR(sp.value()[0].real(), 1000.0, 1e-9);
        CHECK_NEAR(sp.value()[0].imag(), 0.0, 1e-9);

        // minLength 指定: max(size, minLength) 以上の 2 の冪長になる
        AcousticResult<std::vector<std::complex<double>>> sp2 =
            realFft(ArrayView<const double>(r.data(), 300), 2000);
        CHECK(sp2.success());
        CHECK(sp2.value().size() == 2048);

        // 空入力は EmptyInput エラー
        AcousticResult<std::vector<std::complex<double>>> spEmpty =
            realFft(ArrayView<const double>());
        CHECK(!spEmpty.success());
        CHECK(spEmpty.errorCode() == AcousticErrorCode::EmptyInput);
    }

    return testutil::summary("test_fft");
}
