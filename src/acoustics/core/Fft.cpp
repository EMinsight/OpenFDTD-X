// Fft.cpp — radix-2 反復 FFT の実装 (Cooley & Tukey 1965, 時間間引き)。
//
// 手順:
//   1. ビット反転置換
//   2. バタフライ段 (len = 2, 4, ..., N)。回転因子は誤差蓄積を避けるため
//      事前計算した長さ N/2 のテーブル exp(∓j2πk/N) を引く。
#include "Fft.h"

#include <cmath>

namespace ofd {
namespace acoustics {

namespace {
const double kPi = 3.14159265358979323846;

// 回転因子テーブル exp(sign·j2πk/N), k = 0..N/2-1。
// 同じ (N, sign) なら値は常に同一なので直前の 1 組をスレッド毎に保持して
// 使い回す (YIN のように同一長の変換を何千回も繰り返す経路で cos/sin の
// 再計算が支配的になるため)。値は再計算した場合と完全に一致する。
const std::vector<std::complex<double>> &twiddleTable(std::size_t n,
                                                      bool inverse) {
    static thread_local std::vector<std::complex<double>> cached[2];
    static thread_local std::size_t cachedN[2] = {0, 0};
    const int slot = inverse ? 1 : 0;
    if (cachedN[slot] != n) {
        const double sign = inverse ? 1.0 : -1.0;
        std::vector<std::complex<double>> t(n / 2);
        for (std::size_t k = 0; k < n / 2; ++k) {
            const double ang = sign * 2.0 * kPi * static_cast<double>(k) /
                               static_cast<double>(n);
            t[k] = std::complex<double>(std::cos(ang), std::sin(ang));
        }
        cached[slot].swap(t);
        cachedN[slot] = n;
    }
    return cached[slot];
}

// 共通変換本体 (inverse = true で符号反転 + 1/N 正規化)
void transformPow2(std::vector<std::complex<double>> &a, bool inverse) {
    const std::size_t n = a.size();
    if (n <= 1) return;

    // ビット反転置換
    std::size_t j = 0;
    for (std::size_t i = 1; i < n; ++i) {
        std::size_t bit = n >> 1;
        for (; (j & bit) != 0; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }

    // 回転因子テーブル exp(sign·j2πk/N), k = 0..N/2-1 (キャッシュ済み)
    const std::vector<std::complex<double>> &twiddle = twiddleTable(n, inverse);

    // バタフライ段。複素乗算は実部/虚部を明示して書く
    // (std::complex の operator* は Inf/NaN 補正のため libgcc の __muldc3 を
    //  呼び、有限値しか来ない本ループでは同じ値を数倍のコストで返すため)。
    for (std::size_t len = 2; len <= n; len <<= 1) {
        const std::size_t half = len >> 1;
        const std::size_t step = n / len; // テーブル間引き幅
        for (std::size_t base = 0; base < n; base += len) {
            for (std::size_t k = 0; k < half; ++k) {
                const std::complex<double> &w = twiddle[k * step];
                const double wr = w.real();
                const double wi = w.imag();
                const std::complex<double> &z = a[base + k + half];
                const double vr = z.real() * wr - z.imag() * wi;
                const double vi = z.real() * wi + z.imag() * wr;
                const double ur = a[base + k].real();
                const double ui = a[base + k].imag();
                a[base + k] = std::complex<double>(ur + vr, ui + vi);
                a[base + k + half] = std::complex<double>(ur - vr, ui - vi);
            }
        }
    }

    // 逆変換の 1/N 正規化
    if (inverse) {
        const double invN = 1.0 / static_cast<double>(n);
        for (std::size_t i = 0; i < n; ++i) a[i] *= invN;
    }
}
} // namespace

std::size_t nextPowerOfTwo(std::size_t n) {
    std::size_t p = 1;
    while (p < n) p <<= 1;
    return p;
}

bool isPowerOfTwo(std::size_t n) {
    return n != 0 && (n & (n - 1)) == 0;
}

bool fftForward(std::vector<std::complex<double>> &x) {
    if (!isPowerOfTwo(x.size())) return false;
    transformPow2(x, false);
    return true;
}

bool fftInverse(std::vector<std::complex<double>> &x) {
    if (!isPowerOfTwo(x.size())) return false;
    transformPow2(x, true);
    return true;
}

AcousticResult<std::vector<std::complex<double>>>
realFft(ArrayView<const double> x, std::size_t minLength) {
    typedef std::vector<std::complex<double>> Spectrum;
    if (x.empty()) {
        return AcousticResult<Spectrum>::error(AcousticErrorCode::EmptyInput,
                                               "realFft: input is empty");
    }
    const std::size_t n =
        nextPowerOfTwo(x.size() > minLength ? x.size() : minLength);
    Spectrum a(n, std::complex<double>(0.0, 0.0));
    for (std::size_t i = 0; i < x.size(); ++i)
        a[i] = std::complex<double>(x[i], 0.0);
    transformPow2(a, false);
    return AcousticResult<Spectrum>::ok(std::move(a));
}

} // namespace acoustics
} // namespace ofd
