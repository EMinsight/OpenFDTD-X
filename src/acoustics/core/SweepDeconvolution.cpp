// SweepDeconvolution.cpp
#include "SweepDeconvolution.h"

#include <algorithm>
#include <cmath>
#include <complex>

#include "Fft.h"

namespace ofd {
namespace acoustics {

namespace {

const double kPi = 3.14159265358979323846;

// FFT による線形畳み込み (長さ = a + b - 1)
std::vector<double> convolveLinear(const std::vector<double> &a,
                                   const std::vector<double> &b)
{
    std::vector<double> out;
    if (a.empty() || b.empty()) return out;
    const std::size_t n = a.size() + b.size() - 1;
    const std::size_t nfft = nextPowerOfTwo(n);

    std::vector<std::complex<double> > A(nfft, std::complex<double>(0.0, 0.0));
    std::vector<std::complex<double> > B(nfft, std::complex<double>(0.0, 0.0));
    for (std::size_t i = 0; i < a.size(); ++i) A[i] = std::complex<double>(a[i], 0.0);
    for (std::size_t i = 0; i < b.size(); ++i) B[i] = std::complex<double>(b[i], 0.0);
    if (!fftForward(A) || !fftForward(B)) return out;
    for (std::size_t i = 0; i < nfft; ++i) A[i] *= B[i];
    if (!fftInverse(A)) return out;

    out.resize(n);
    for (std::size_t i = 0; i < n; ++i) out[i] = A[i].real();
    return out;
}

// 掃引の生の標本 (フェード・振幅を掛けない素の sin)
std::vector<double> rawSweep(const SweepSpec &s, std::size_t &count)
{
    const double fs = s.sampleRateHz;
    const double T = s.durationSec;
    count = std::size_t(T * fs + 0.5);
    std::vector<double> x(count, 0.0);
    if (count == 0) return x;
    const double w1 = 2.0 * kPi * s.startHz;
    const double L = std::log(s.endHz / s.startHz);
    const double k = w1 * T / L;
    for (std::size_t n = 0; n < count; ++n) {
        const double t = double(n) / fs;
        x[n] = std::sin(k * (std::exp(t * L / T) - 1.0));
    }
    return x;
}

double energyOf(const std::vector<double> &v, std::size_t a, std::size_t z)
{
    double e = 0.0;
    for (std::size_t i = a; i < z && i < v.size(); ++i) e += v[i] * v[i];
    return e;
}

} // namespace

bool SweepSpec::valid() const
{
    if (!(sampleRateHz > 0.0) || !(durationSec > 0.0)) return false;
    if (!(startHz > 0.0) || !(endHz > startHz)) return false;
    if (!(endHz < 0.5 * sampleRateHz)) return false;       // ナイキスト超は不可
    if (!(durationSec * sampleRateHz >= 8.0)) return false; // 短すぎ
    return true;
}

AudioBuffer generateSweep(const SweepSpec &spec)
{
    AudioBuffer out;
    if (!spec.valid()) return out;

    std::size_t n = 0;
    std::vector<double> x = rawSweep(spec, n);
    if (n == 0) return out;

    const double fs = spec.sampleRateHz;
    const std::size_t fi = std::min(n, std::size_t(std::max(0.0, spec.fadeInSec) * fs));
    const std::size_t fo = std::min(n, std::size_t(std::max(0.0, spec.fadeOutSec) * fs));
    for (std::size_t i = 0; i < n; ++i) {
        double w = 1.0;
        if (fi > 0 && i < fi)
            w *= 0.5 * (1.0 - std::cos(kPi * double(i) / double(fi)));
        if (fo > 0 && i >= n - fo) {
            const double u = double(n - 1 - i) / double(fo);
            w *= 0.5 * (1.0 - std::cos(kPi * u));
        }
        x[i] *= spec.amplitude * w;
    }
    out.sampleRateHz = fs;
    out.channels.push_back(x);
    return out;
}

std::vector<double> sweepInverseFilter(const SweepSpec &spec)
{
    std::vector<double> f;
    if (!spec.valid()) return f;

    std::size_t n = 0;
    const std::vector<double> x = rawSweep(spec, n);
    if (n == 0) return f;

    // 時間反転 + 6 dB/oct の振幅補正。反転後の時刻 t における瞬時周波数は
    // ω(T−t) なので、1/ω に比例させる = e^{−(T−t)L/T} を掛ける。
    const double L = std::log(spec.endHz / spec.startHz);
    f.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        const double t = double(i) / spec.sampleRateHz;
        const double env = std::exp(-(spec.durationSec - t) * L / spec.durationSec);
        f[i] = x[n - 1 - i] * env;
    }

    // 掃引 ⊛ 逆フィルタ が振幅 1 の単一インパルスになるよう正規化する。
    // 解析的な係数を書き下すより、実際に畳み込んでピークで割るほうが
    // 実装 (フェード無しの素の掃引・離散化) と厳密に整合する。
    const std::vector<double> probe = convolveLinear(x, f);
    double peak = 0.0;
    for (std::size_t i = 0; i < probe.size(); ++i)
        peak = std::max(peak, std::fabs(probe[i]));
    if (peak > 0.0)
        for (std::size_t i = 0; i < n; ++i) f[i] /= peak;
    return f;
}

double harmonicDelaySec(const SweepSpec &spec, int order)
{
    if (!spec.valid() || order < 1) return 0.0;
    const double L = std::log(spec.endHz / spec.startHz);
    if (!(L > 0.0)) return 0.0;
    return spec.durationSec * std::log(double(order)) / L;
}

AcousticResult<SweepDeconvolutionResult>
deconvolveSweep(ArrayView<const double> recorded, const SweepSpec &spec,
                int maxHarmonic)
{
    typedef AcousticResult<SweepDeconvolutionResult> Result;
    if (recorded.empty())
        return Result::error(AcousticErrorCode::EmptyInput, "empty recording");
    if (!spec.valid())
        return Result::error(AcousticErrorCode::InvalidArgument,
                             "invalid sweep specification");

    const std::vector<double> f = sweepInverseFilter(spec);
    if (f.empty())
        return Result::error(AcousticErrorCode::InvalidArgument,
                             "cannot build the inverse filter");

    std::vector<double> rec(recorded.begin(), recorded.end());
    SweepDeconvolutionResult r;
    r.response = convolveLinear(rec, f);
    if (r.response.empty())
        return Result::error(AcousticErrorCode::InvalidArgument,
                             "deconvolution failed");
    r.sampleRateHz = spec.sampleRateHz;

    // 掃引の自己相関ピークは、逆フィルタ長 − 1 の位置に来る。
    // 高調波はこれより前 (Farina 2000)。
    r.linearIndex = f.size() - 1;
    if (r.linearIndex >= r.response.size())
        return Result::error(AcousticErrorCode::InvalidArgument,
                             "the recording is shorter than the sweep");

    double peakAll = 0.0;
    for (std::size_t i = 0; i < r.response.size(); ++i)
        peakAll = std::max(peakAll, std::fabs(r.response[i]));
    if (!(peakAll > 0.0))
        return Result::error(AcousticErrorCode::InvalidArgument,
                             "the deconvolved response is all zeros");

    r.linear.assign(r.response.begin() + long(r.linearIndex), r.response.end());

    // 線形応答のピーク (高調波レベルの基準)
    double linPeak = 0.0;
    for (std::size_t i = 0; i < r.linear.size(); ++i)
        linPeak = std::max(linPeak, std::fabs(r.linear[i]));

    // ── 高調波の分離 ──
    // N 次は linearIndex − Δt_N·fs の近傍。隣の次数との中点までを持ち分と
    // し、窓が線形応答へ食い込む / 応答の先頭より前へ出る場合は
    // separable = false にして「切り出せなかった」ことを明示する
    // (重なった値を高調波として出さない)。
    const int nmax = std::min(8, maxHarmonic);
    double harmEnergy = 0.0;
    bool anyHarm = false;
    for (int k = 2; k <= nmax; ++k) {
        const double dt = harmonicDelaySec(spec, k);
        const double center = double(r.linearIndex) - dt * spec.sampleRateHz;
        HarmonicComponent h;
        h.order = k;
        if (center < 0.0) { r.harmonics.push_back(h); continue; }

        // 窓: 隣接次数との中点
        const double dtPrev = harmonicDelaySec(spec, k - 1);
        const double dtNext = harmonicDelaySec(spec, k + 1);
        const double hiT = 0.5 * (dt + dtPrev);      // 上側 (線形寄り)
        const double loT = 0.5 * (dt + dtNext);      // 下側 (さらに前)
        double loD = double(r.linearIndex) - loT * spec.sampleRateHz;
        double hiD = double(r.linearIndex) - hiT * spec.sampleRateHz;
        if (loD < 0.0) loD = 0.0;
        if (hiD < loD) { r.harmonics.push_back(h); continue; }
        const std::size_t lo = std::size_t(loD);
        const std::size_t hi = std::min(std::size_t(hiD) + 1, r.linearIndex);
        if (hi <= lo) { r.harmonics.push_back(h); continue; }

        double pk = 0.0;
        std::size_t at = lo;
        for (std::size_t i = lo; i < hi; ++i) {
            const double v = std::fabs(r.response[i]);
            if (v > pk) { pk = v; at = i; }
        }
        h.index = at;
        h.peak = pk;
        h.separable = true;
        h.levelDbc = (pk > 0.0 && linPeak > 0.0)
                         ? 20.0 * std::log10(pk / linPeak) : -300.0;
        harmEnergy += energyOf(r.response, lo, hi);
        anyHarm = true;
        r.harmonics.push_back(h);
    }

    const double linEnergy = energyOf(r.linear, 0, r.linear.size());
    if (anyHarm && linEnergy > 0.0) {
        r.thdValid = true;
        r.thdPercent = 100.0 * std::sqrt(harmEnergy / linEnergy);
    } else if (nmax >= 2) {
        r.warning = "no harmonic window could be separated";
    }

    r.valid = true;
    return Result::ok(r);
}

} // namespace acoustics
} // namespace ofd
