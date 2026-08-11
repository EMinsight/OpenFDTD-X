// WaveformSpectrum.cpp — 仕様は WaveformSpectrum.h
#include "WaveformSpectrum.h"

#include <algorithm>
#include <cmath>
#include <complex>

#include "../acoustics/core/Fft.h"

namespace ofd {
namespace wavespec {

namespace {
constexpr double kPi = 3.14159265358979323846;
} // namespace

double taperValue(Apodization apod, double taperFrac, std::size_t i,
                  std::size_t n)
{
    if (apod == Apodization::Off || n < 2) return 1.0;
    const double frac = std::min(0.5, std::max(0.0, taperFrac));
    if (frac <= 0.0) return 1.0;
    // テーパ長 (標本数)。1 未満なら掛けない。
    const double m = frac * double(n - 1);
    if (m < 1.0) return 1.0;

    const double x = double(i);
    const double last = double(n - 1);
    // 前側 raised-cosine: 0 → 1
    const bool doStart = (apod == Apodization::Start || apod == Apodization::Both);
    const bool doEnd   = (apod == Apodization::End   || apod == Apodization::Both);
    if (doStart && x < m)
        return 0.5 * (1.0 - std::cos(kPi * x / m));
    if (doEnd && x > last - m)
        return 0.5 * (1.0 - std::cos(kPi * (last - x) / m));
    return 1.0;
}

Result waveformSpectrum(const std::vector<double> &t,
                        const std::vector<double> &y,
                        audioedit::WindowKind window, Apodization apod,
                        double taperFrac, int minFftSize)
{
    Result r;
    const std::size_t n = t.size();
    if (n < 4 || y.size() != n) return r;

    // 標本間隔 — 等間隔でなければ作らない。
    // **許容幅は 2 % と広めに取る**。ofd_post の time 列は 6 桁の指数表記なので、
    // 丸めだけで隣り合う差が 0.1 % ほど揺れる (実測: dt = 9.309e-12 s に対し
    // 桁落ちが 1e-14 s)。1e-6 で判定していたら実データが全て弾かれていた。
    // 30 % ずれるような本当の非等間隔はこの幅でも落ちる。
    // dt は端から端までの平均で取る (先頭の 1 差分は丸めの外れ値になりうる)。
    const double dt = (t[n - 1] - t[0]) / double(n - 1);
    if (!(dt > 0.0) || !std::isfinite(dt)) return r;
    for (std::size_t i = 1; i < n; ++i) {
        const double d = t[i] - t[i - 1];
        if (!std::isfinite(d) || std::fabs(d - dt) > 0.02 * dt) return r;
    }

    // 窓 × テーパ を掛ける
    std::vector<double> x(n, 0.0);
    double sumW = 0.0, sumW2 = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double w = audioedit::windowValue(window, i, n)
                         * taperValue(apod, taperFrac, i, n);
        x[i] = y[i] * w;
        sumW += w;
        sumW2 += w * w;
    }
    r.coherentGain = sumW / double(n);
    // 等価雑音帯域幅 [bin] = N·Σw² / (Σw)²  (F. J. Harris, Proc. IEEE 66(1), 1978)
    r.enbwBins = (sumW > 0.0) ? (double(n) * sumW2 / (sumW * sumW)) : 0.0;

    const std::size_t minLen =
        (minFftSize > 0) ? std::size_t(minFftSize) : std::size_t(0);
    const acoustics::AcousticResult<std::vector<std::complex<double>>> fft =
        acoustics::realFft(acoustics::ArrayView<const double>(x.data(), x.size()),
                           minLen);
    if (!fft.success()) return r;
    const std::vector<std::complex<double>> &X = fft.value();
    const std::size_t nFft = X.size();
    if (nFft < 2) return r;

    r.dtSec = dt;
    r.fsHz = 1.0 / dt;
    r.dfHz = r.fsHz / double(nFft);
    r.nUsed = int(n);
    r.nFft = int(nFft);

    const std::size_t half = nFft / 2 + 1;      // 0 … fs/2
    r.freqHz.resize(half);
    r.db.resize(half);
    double peak = 0.0;
    std::size_t peakIdx = 0;
    for (std::size_t k = 0; k < half; ++k) {
        const double m = std::abs(X[k]);
        r.freqHz[k] = double(k) * r.dfHz;
        r.db[k] = m;                            // いったん振幅を入れる
        if (m > peak) { peak = m; peakIdx = k; }
    }
    if (!(peak > 0.0)) return r;                // 全 0 の波形
    for (std::size_t k = 0; k < half; ++k) {
        const double rel = r.db[k] / peak;
        r.db[k] = 20.0 * std::log10(rel > 1e-12 ? rel : 1e-12);
    }
    r.peakFreqHz = r.freqHz[peakIdx];
    r.hasPeak = true;
    r.valid = true;
    return r;
}

} // namespace wavespec
} // namespace ofd
