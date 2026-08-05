// AudioEditEngine.cpp — 音響編集・解析エンジンの実装。
// アルゴリズムは mock (audio-editor.jsx / audio-editor-ext.jsx) を移植し、
// FFT と畳み込みは音響コア (acoustics::fftForward / ConvolutionEngine) を使う。
#include "AudioEditEngine.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <random>

#include "../acoustics/core/ConvolutionEngine.h"
#include "../acoustics/core/Fft.h"
#include "../acoustics/core/Resampler.h"

namespace ofd {
namespace audioedit {

namespace {

constexpr double kPi = 3.14159265358979323846;

// 決定的な乱数列 (再現性の規則 — 固定シード)
std::mt19937 seededRng(unsigned seed) { return std::mt19937(seed); }

// [a, z) をクランプする。a >= z なら全範囲
void clampRange(const AudioBuffer &in, std::size_t &a, std::size_t &z)
{
    const std::size_t n = in.sampleCount();
    if (a >= z) { a = 0; z = n; return; }
    a = std::min(a, n);
    z = std::min(z, n);
}

AudioBuffer makeLike(const AudioBuffer &in, std::size_t samples)
{
    AudioBuffer out;
    out.sampleRateHz = in.sampleRateHz;
    out.channels.assign(in.channelCount(), std::vector<double>(samples, 0.0));
    return out;
}

// 範囲だけ書き換えるコピー編集の共通形
template <typename Fn>
AudioBuffer mapRange(const AudioBuffer &in, std::size_t a, std::size_t z,
                     Fn fn)
{
    clampRange(in, a, z);
    AudioBuffer out = in;
    for (std::size_t c = 0; c < out.channelCount(); ++c) {
        const std::vector<double> &src = in.channels[c];
        std::vector<double> &dst = out.channels[c];
        for (std::size_t i = a; i < z; ++i)
            dst[i] = fn(src, i, a, z);
    }
    return out;
}

// modified Bessel I0 (Kaiser 窓用の級数展開)
double besselI0(double x)
{
    double s = 1.0, t = 1.0;
    for (int k = 1; k < 25; ++k) {
        t *= (x / (2.0 * k)) * (x / (2.0 * k));
        s += t;
    }
    return s;
}

// ── biquad 1 段 (direct form I) ─────────────────────────────────────────────
struct Biquad {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;  // a0 正規化済み
    void run(const std::vector<double> &x, std::vector<double> &y) const
    {
        double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        y.resize(x.size());
        for (std::size_t i = 0; i < x.size(); ++i) {
            const double v = b0 * x[i] + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = x[i];
            y2 = y1; y1 = v;
            y[i] = v;
        }
    }
};

Biquad rbjHighShelf(double f0, double q, double gainDb, double fs);

// RBJ Audio EQ Cookbook
Biquad rbjBiquad(BiquadKind kind, double f0, double q, double gainDb,
                 double fs)
{
    Biquad bq;
    if (fs <= 0.0 || f0 <= 0.0 || f0 >= fs * 0.5 || q <= 0.0)
        return bq;  // 不正パラメータは素通し
    if (kind == BiquadKind::HighShelf)
        return rbjHighShelf(f0, q, gainDb, fs);   // 既存実装 (BS.1770) を共用
    const double w0 = 2.0 * kPi * f0 / fs;
    const double cw = std::cos(w0), sw = std::sin(w0);
    const double alpha = sw / (2.0 * q);
    double b0, b1, b2, a0, a1, a2;
    switch (kind) {
    case BiquadKind::Peaking: {
        const double A = std::pow(10.0, gainDb / 40.0);
        b0 = 1 + alpha * A; b1 = -2 * cw; b2 = 1 - alpha * A;
        a0 = 1 + alpha / A; a1 = -2 * cw; a2 = 1 - alpha / A;
        break;
    }
    case BiquadKind::HighPass:
        b0 = (1 + cw) / 2; b1 = -(1 + cw); b2 = (1 + cw) / 2;
        a0 = 1 + alpha;    a1 = -2 * cw;   a2 = 1 - alpha;
        break;
    case BiquadKind::LowShelf: {
        // 低域を gainDb 持ち上げ/下げ、Nyquist 側は 0 dB (Cookbook 閉形式)
        const double A = std::pow(10.0, gainDb / 40.0);
        const double sqA2a = 2.0 * std::sqrt(A) * alpha;
        b0 = A * ((A + 1) - (A - 1) * cw + sqA2a);
        b1 = 2 * A * ((A - 1) - (A + 1) * cw);
        b2 = A * ((A + 1) - (A - 1) * cw - sqA2a);
        a0 = (A + 1) + (A - 1) * cw + sqA2a;
        a1 = -2 * ((A - 1) + (A + 1) * cw);
        a2 = (A + 1) + (A - 1) * cw - sqA2a;
        break;
    }
    case BiquadKind::Notch:
        b0 = 1;         b1 = -2 * cw; b2 = 1;
        a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha;
        break;
    case BiquadKind::BandPass:
        // constant 0 dB peak gain 型: f0 で 0 dB、遠方 ±6 dB/oct スカート
        b0 = alpha;     b1 = 0;       b2 = -alpha;
        a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha;
        break;
    case BiquadKind::LowPass:
    default:
        b0 = (1 - cw) / 2; b1 = 1 - cw;    b2 = (1 - cw) / 2;
        a0 = 1 + alpha;    a1 = -2 * cw;   a2 = 1 - alpha;
        break;
    }
    bq.b0 = b0 / a0; bq.b1 = b1 / a0; bq.b2 = b2 / a0;
    bq.a1 = a1 / a0; bq.a2 = a2 / a0;
    return bq;
}

// RBJ high shelf (BS.1770 K 特性の第 1 段に使用)
Biquad rbjHighShelf(double f0, double q, double gainDb, double fs)
{
    Biquad bq;
    const double A = std::pow(10.0, gainDb / 40.0);
    const double w0 = 2.0 * kPi * f0 / fs;
    const double cw = std::cos(w0), sw = std::sin(w0);
    const double alpha = sw / (2.0 * q);
    const double sqA2a = 2.0 * std::sqrt(A) * alpha;
    const double b0 = A * ((A + 1) + (A - 1) * cw + sqA2a);
    const double b1 = -2 * A * ((A - 1) + (A + 1) * cw);
    const double b2 = A * ((A + 1) + (A - 1) * cw - sqA2a);
    const double a0 = (A + 1) - (A - 1) * cw + sqA2a;
    const double a1 = 2 * ((A - 1) - (A + 1) * cw);
    const double a2 = (A + 1) - (A - 1) * cw - sqA2a;
    bq.b0 = b0 / a0; bq.b1 = b1 / a0; bq.b2 = b2 / a0;
    bq.a1 = a1 / a0; bq.a2 = a2 / a0;
    return bq;
}

// 実信号の一部を窓掛けして 2 の冪 FFT する共通処理。
// 戻り値は複素スペクトル (長さ fftSize)
std::vector<std::complex<double>>
windowedFft(const std::vector<double> &x, std::size_t offset,
            std::size_t fftSize, WindowKind window)
{
    // fftForward は 2 の冪以外を拒否する — 非 2 冪は切り上げて丸める
    if (!acoustics::isPowerOfTwo(fftSize))
        fftSize = acoustics::nextPowerOfTwo(fftSize);
    std::vector<std::complex<double>> spec(fftSize);
    for (std::size_t i = 0; i < fftSize; ++i) {
        const double v = (offset + i < x.size()) ? x[offset + i] : 0.0;
        spec[i] = v * windowValue(window, i, fftSize);
    }
    acoustics::fftForward(spec);
    return spec;
}

} // namespace

// ── 窓関数 ──────────────────────────────────────────────────────────────────
const std::vector<WindowInfo> &windowInfos()
{
    // 特性値 (メインローブ幅 / サイドローブ) は文献値 (harris 1978 ほか)
    static const std::vector<WindowInfo> kInfos = {
        { WindowKind::Rect,     "rect",     "矩形 (なし)",        "Rectangular",
          "0.89", "-13", "過渡信号・IR (窓なし)", "Transients / IR (no window)" },
        { WindowKind::Hann,     "hann",     "Hann",               "Hann",
          "1.44", "-32", "汎用・スペクトログラム標準", "General purpose / spectrogram default" },
        { WindowKind::Hamming,  "hamming",  "Hamming",            "Hamming",
          "1.30", "-43", "音声分析", "Speech analysis" },
        { WindowKind::Blackman, "blackman", "Blackman",           "Blackman",
          "1.68", "-58", "高ダイナミックレンジ", "High dynamic range" },
        { WindowKind::BlackmanHarris4, "bh4", "Blackman-Harris 4項", "Blackman-Harris 4-term",
          "1.90", "-92", "微小スプリアス検出", "Low-level spurious detection" },
        { WindowKind::Nuttall,  "nuttall",  "Nuttall",            "Nuttall",
          "1.98", "-93", "最小サイドローブ", "Minimum sidelobe" },
        { WindowKind::FlatTop,  "flattop",  "Flat-top (振幅精密)", "Flat-top (amplitude accurate)",
          "3.72", "-93", "振幅校正・レベル測定", "Amplitude calibration / level metering" },
        { WindowKind::Bartlett, "bartlett", "Bartlett (三角)",     "Bartlett (triangular)",
          "1.28", "-27", "簡易解析", "Simple analysis" },
        { WindowKind::Welch,    "welch",    "Welch (放物線)",      "Welch (parabolic)",
          "1.20", "-21", "パワースペクトル推定", "Power spectrum estimation" },
        { WindowKind::Gauss,    "gauss",    "Gaussian σ=0.4",     "Gaussian σ=0.4",
          "1.55", "-55", "時間-周波数トレードオフ可変", "Adjustable time-frequency trade-off" },
        { WindowKind::Tukey,    "tukey",    "Tukey α=0.5",        "Tukey α=0.5",
          "1.15", "-18", "過渡+定常の混在信号", "Mixed transient + steady signals" },
        { WindowKind::Kaiser,   "kaiser",   "Kaiser β=8.6",       "Kaiser β=8.6",
          "1.83", "-82", "β可変・フィルタ設計", "Adjustable β / filter design" },
        { WindowKind::Cosine,   "cosine",   "Cosine (sine窓)",    "Cosine (sine window)",
          "1.19", "-23", "MDCT系コーデック", "MDCT-family codecs" },
        { WindowKind::Lanczos,  "lanczos",  "Lanczos (sinc)",     "Lanczos (sinc)",
          "1.30", "-26", "リサンプリング", "Resampling" },
        { WindowKind::Exponential, "exp",   "指数 (IR解析用)",     "Exponential (for IR analysis)",
          "—", "—", "残響IRの後部雑音抑制", "Suppresses late noise in reverberant IRs" },
    };
    return kInfos;
}

double windowValue(WindowKind w, std::size_t i, std::size_t n)
{
    if (n < 2) return 1.0;
    const double N1 = static_cast<double>(n - 1);
    const double x = 2.0 * kPi * i / N1;
    switch (w) {
    case WindowKind::Rect:     return 1.0;
    case WindowKind::Hann:     return 0.5 - 0.5 * std::cos(x);
    case WindowKind::Hamming:  return 0.54 - 0.46 * std::cos(x);
    case WindowKind::Blackman:
        return 0.42 - 0.5 * std::cos(x) + 0.08 * std::cos(2 * x);
    case WindowKind::BlackmanHarris4:
        return 0.35875 - 0.48829 * std::cos(x) + 0.14128 * std::cos(2 * x)
             - 0.01168 * std::cos(3 * x);
    case WindowKind::Nuttall:
        return 0.355768 - 0.487396 * std::cos(x) + 0.144232 * std::cos(2 * x)
             - 0.012604 * std::cos(3 * x);
    case WindowKind::FlatTop:
        return 0.21557895 - 0.41663158 * std::cos(x)
             + 0.277263158 * std::cos(2 * x) - 0.083578947 * std::cos(3 * x)
             + 0.006947368 * std::cos(4 * x);
    case WindowKind::Bartlett:
        return 1.0 - std::fabs((i - N1 / 2) / (N1 / 2));
    case WindowKind::Welch: {
        const double u = (i - N1 / 2) / (N1 / 2);
        return 1.0 - u * u;
    }
    case WindowKind::Gauss: {
        const double u = (i - N1 / 2) / (0.4 * N1 / 2);
        return std::exp(-0.5 * u * u);
    }
    case WindowKind::Tukey: {
        const double a = 0.5, u = i / N1;
        if (u < a / 2)     return 0.5 * (1 + std::cos(kPi * (2 * u / a - 1)));
        if (u > 1 - a / 2) return 0.5 * (1 + std::cos(kPi * (2 * u / a - 2 / a + 1)));
        return 1.0;
    }
    case WindowKind::Kaiser: {
        const double b = 8.6, u = 2.0 * i / N1 - 1.0;
        return besselI0(b * std::sqrt(std::max(0.0, 1.0 - u * u))) / besselI0(b);
    }
    case WindowKind::Cosine:   return std::sin(kPi * i / N1);
    case WindowKind::Lanczos: {
        const double u = 2.0 * i / N1 - 1.0;
        return (u == 0.0) ? 1.0 : std::sin(kPi * u) / (kPi * u);
    }
    case WindowKind::Exponential:
        return std::exp(-4.6 * static_cast<double>(i) / n);
    }
    return 1.0;
}

// ── 信号生成 ────────────────────────────────────────────────────────────────
AudioBuffer generateSignal(SignalKind kind, double f1Hz, double f2Hz,
                           double durationSec, double amp, double sampleRate)
{
    AudioBuffer out;
    out.sampleRateHz = sampleRate;
    const std::size_t n = (sampleRate > 0.0 && durationSec > 0.0)
        ? static_cast<std::size_t>(durationSec * sampleRate) : 0;
    out.channels.assign(1, std::vector<double>(n, 0.0));
    if (n == 0) return out;
    std::vector<double> &d = out.channels[0];
    const double sr = sampleRate;

    switch (kind) {
    case SignalKind::Sine:
        for (std::size_t i = 0; i < n; ++i)
            d[i] = amp * std::sin(2 * kPi * f1Hz * i / sr);
        break;
    case SignalKind::ExpSweep: {
        // Farina ESS: x(t) = sin(2π f1 T/ln(f2/f1) (e^{t·ln(f2/f1)/T} − 1))
        if (f1Hz <= 0.0 || f2Hz <= f1Hz) break;
        const double T = n / sr, L = std::log(f2Hz / f1Hz);
        const double K = T * f1Hz / L;
        for (std::size_t i = 0; i < n; ++i) {
            const double t = i / sr;
            d[i] = amp * std::sin(2 * kPi * K * (std::exp(t * L / T) - 1.0));
        }
        break;
    }
    case SignalKind::LinSweep:
        for (std::size_t i = 0; i < n; ++i) {
            const double t = i / sr, T = n / sr;
            d[i] = amp * std::sin(2 * kPi * (f1Hz * t + (f2Hz - f1Hz) * t * t / (2 * T)));
        }
        break;
    case SignalKind::White: {
        std::mt19937 rng = seededRng(0x0FD0A001u);
        std::uniform_real_distribution<double> uni(-1.0, 1.0);
        for (std::size_t i = 0; i < n; ++i) d[i] = amp * uni(rng);
        break;
    }
    case SignalKind::Pink: {
        // Paul Kellet の economy 3 極フィルタ
        std::mt19937 rng = seededRng(0x0FD0A002u);
        std::uniform_real_distribution<double> uni(-1.0, 1.0);
        double b0 = 0, b1 = 0, b2 = 0;
        for (std::size_t i = 0; i < n; ++i) {
            const double w = uni(rng);
            b0 = 0.99765 * b0 + w * 0.0990460;
            b1 = 0.96300 * b1 + w * 0.2965164;
            b2 = 0.57000 * b2 + w * 1.0526913;
            d[i] = amp * (b0 + b1 + b2 + w * 0.1848) * 0.25;
        }
        break;
    }
    case SignalKind::Mls: {
        // 17bit 最大長 LFSR (x^17 + x^14 + 1)
        unsigned s = 0x1FFFFu;
        for (std::size_t i = 0; i < n; ++i) {
            const unsigned bit = ((s >> 16) ^ (s >> 13)) & 1u;
            s = ((s << 1) | bit) & 0x1FFFFu;
            d[i] = amp * (bit ? 1.0 : -1.0);
        }
        break;
    }
    case SignalKind::Impulse:
        d[0] = amp;
        break;
    case SignalKind::Click:
        for (std::size_t i = 0; i < n; ++i)
            d[i] = amp * std::exp(-static_cast<double>(i) / (sr * 0.002))
                 * std::sin(2 * kPi * 1000.0 * i / sr);
        break;
    }
    return out;
}

// ── 基本編集 ────────────────────────────────────────────────────────────────
AudioBuffer trimToRange(const AudioBuffer &in, std::size_t a, std::size_t z)
{
    clampRange(in, a, z);
    AudioBuffer out = makeLike(in, z - a);
    for (std::size_t c = 0; c < in.channelCount(); ++c)
        std::copy(in.channels[c].begin() + a, in.channels[c].begin() + z,
                  out.channels[c].begin());
    return out;
}

AudioBuffer deleteRange(const AudioBuffer &in, std::size_t a, std::size_t z)
{
    clampRange(in, a, z);
    const std::size_t n = in.sampleCount() - (z - a);
    AudioBuffer out = makeLike(in, n);
    for (std::size_t c = 0; c < in.channelCount(); ++c) {
        const std::vector<double> &s = in.channels[c];
        std::vector<double> &t = out.channels[c];
        std::copy(s.begin(), s.begin() + a, t.begin());
        std::copy(s.begin() + z, s.end(), t.begin() + a);
    }
    return out;
}

AudioBuffer silenceRange(const AudioBuffer &in, std::size_t a, std::size_t z)
{
    return mapRange(in, a, z,
        [](const std::vector<double> &, std::size_t, std::size_t, std::size_t) {
            return 0.0;
        });
}

AudioBuffer reverseRange(const AudioBuffer &in, std::size_t a, std::size_t z)
{
    return mapRange(in, a, z,
        [](const std::vector<double> &src, std::size_t i, std::size_t aa,
           std::size_t zz) { return src[zz - 1 - (i - aa)]; });
}

AudioBuffer normalizeRange(const AudioBuffer &in, std::size_t a, std::size_t z,
                           double targetPeak, double *appliedGainDb)
{
    clampRange(in, a, z);
    double pk = 0.0;
    for (std::size_t c = 0; c < in.channelCount(); ++c)
        for (std::size_t i = a; i < z; ++i)
            pk = std::max(pk, std::fabs(in.channels[c][i]));
    const double gain = (pk > 0.0) ? targetPeak / pk : 1.0;
    if (appliedGainDb)
        *appliedGainDb = 20.0 * std::log10(std::max(gain, 1e-12));
    return mapRange(in, a, z,
        [gain](const std::vector<double> &src, std::size_t i, std::size_t,
               std::size_t) { return src[i] * gain; });
}

AudioBuffer fadeRange(const AudioBuffer &in, std::size_t a, std::size_t z,
                      bool fadeIn)
{
    return mapRange(in, a, z,
        [fadeIn](const std::vector<double> &src, std::size_t i, std::size_t aa,
                 std::size_t zz) {
            const double u = static_cast<double>(i - aa)
                           / std::max<std::size_t>(1, zz - aa);
            return src[i] * (fadeIn ? u : 1.0 - u);
        });
}

AudioBuffer gainRange(const AudioBuffer &in, std::size_t a, std::size_t z,
                      double gainDb)
{
    const double g = std::pow(10.0, gainDb / 20.0);
    return mapRange(in, a, z,
        [g](const std::vector<double> &src, std::size_t i, std::size_t,
            std::size_t) { return src[i] * g; });
}

AudioBuffer removeDcRange(const AudioBuffer &in, std::size_t a, std::size_t z)
{
    clampRange(in, a, z);
    AudioBuffer out = in;
    for (std::size_t c = 0; c < in.channelCount(); ++c) {
        double s = 0.0;
        for (std::size_t i = a; i < z; ++i) s += in.channels[c][i];
        const double dc = s / std::max<std::size_t>(1, z - a);
        for (std::size_t i = a; i < z; ++i) out.channels[c][i] -= dc;
    }
    return out;
}

AudioBuffer smoothRange(const AudioBuffer &in, std::size_t a, std::size_t z)
{
    return mapRange(in, a, z,
        [](const std::vector<double> &src, std::size_t i, std::size_t,
           std::size_t) {
            const double next = (i + 1 < src.size()) ? src[i + 1] : src[i];
            return (src[i] + next) / 2.0;
        });
}

// ── 範囲編集の補完 ──────────────────────────────────────────────────────────
AudioBuffer insertSilence(const AudioBuffer &in, std::size_t at,
                          double durationSec)
{
    const std::size_t n = in.sampleCount();
    at = std::min(at, n);
    const std::size_t ins =
        (in.sampleRateHz > 0.0 && durationSec > 0.0)
            ? static_cast<std::size_t>(
                  std::llround(durationSec * in.sampleRateHz))
            : 0;
    if (ins == 0) return in;
    AudioBuffer out = makeLike(in, n + ins);
    for (std::size_t c = 0; c < in.channelCount(); ++c) {
        const std::vector<double> &s = in.channels[c];
        std::vector<double> &t = out.channels[c];
        std::copy(s.begin(), s.begin() + at, t.begin());
        // [at, at+ins) は makeLike の 0 のまま (厳密な無音)
        std::copy(s.begin() + at, s.end(), t.begin() + at + ins);
    }
    return out;
}

AudioBuffer repeatRange(const AudioBuffer &in, std::size_t a, std::size_t z,
                        int count)
{
    clampRange(in, a, z);
    if (count < 1 || a >= z) return in;
    const std::size_t seg = z - a;
    const std::size_t n =
        in.sampleCount() + seg * static_cast<std::size_t>(count - 1);
    AudioBuffer out = makeLike(in, n);
    for (std::size_t c = 0; c < in.channelCount(); ++c) {
        const std::vector<double> &s = in.channels[c];
        std::vector<double> &t = out.channels[c];
        std::copy(s.begin(), s.begin() + a, t.begin());
        for (int k = 0; k < count; ++k)   // 各コピーはビット一致
            std::copy(s.begin() + a, s.begin() + z,
                      t.begin() + a + static_cast<std::size_t>(k) * seg);
        std::copy(s.begin() + z, s.end(),
                  t.begin() + a + static_cast<std::size_t>(count) * seg);
    }
    return out;
}

AudioBuffer crossfadeConcat(const AudioBuffer &a, const AudioBuffer &b,
                            double overlapSec)
{
    if (a.sampleCount() == 0) return b;
    if (b.sampleCount() == 0) return a;
    // fs が異なる場合は b を a の fs へ変換して結合する。変換不能な fs
    // (非整数等) のみ b のサンプルをそのまま用いる (resampleTo は失敗時に
    // 入力を変更せず返す)
    AudioBuffer bb;
    const AudioBuffer *pb = &b;
    if (a.sampleRateHz > 0.0 && b.sampleRateHz > 0.0 &&
        a.sampleRateHz != b.sampleRateHz) {
        bb = resampleTo(b, a.sampleRateHz);
        pb = &bb;
    }
    const double sr = (a.sampleRateHz > 0.0) ? a.sampleRateHz
                                             : pb->sampleRateHz;
    const std::size_t na = a.sampleCount(), nb = pb->sampleCount();
    std::size_t ov =
        (sr > 0.0 && overlapSec > 0.0)
            ? static_cast<std::size_t>(std::llround(overlapSec * sr))
            : 0;
    ov = std::min(ov, std::min(na, nb));
    const std::size_t n = na + nb - ov;
    const std::size_t nch = std::max(a.channelCount(), pb->channelCount());
    AudioBuffer out;
    out.sampleRateHz = sr;
    out.channels.assign(nch, std::vector<double>(n, 0.0));
    for (std::size_t c = 0; c < nch; ++c) {
        const std::vector<double> &xa =
            a.channels[std::min(c, a.channelCount() - 1)];
        const std::vector<double> &xb =
            pb->channels[std::min(c, pb->channelCount() - 1)];
        std::vector<double> &y = out.channels[c];
        std::copy(xa.begin(), xa.begin() + (na - ov), y.begin());
        for (std::size_t i = 0; i < ov; ++i) {
            // 等パワー: gA = cos θ, gB = sin θ (gA² + gB² = 1)
            const double th = 0.5 * kPi * (i + 0.5) / ov;
            y[na - ov + i] = xa[na - ov + i] * std::cos(th)
                           + xb[i] * std::sin(th);
        }
        std::copy(xb.begin() + ov, xb.end(), y.begin() + na);
    }
    return out;
}

// ── サンプルレート変換 (音響コアへ委譲 — 再実装しない) ──────────────────────
AudioBuffer resampleTo(const AudioBuffer &in, double dstRateHz,
                       std::string *error)
{
    if (error) error->clear();
    const acoustics::AcousticResult<AudioBuffer> r =
        acoustics::resampleBuffer(in, dstRateHz);
    if (!r.success()) {
        if (error) *error = r.message();
        return in;   // 失敗時は入力を変更しない (呼び出し側は error で判定)
    }
    return r.value();
}

// ── エフェクト ──────────────────────────────────────────────────────────────
AudioBuffer applyBiquad(const AudioBuffer &in, BiquadKind kind,
                        double freqHz, double q, double gainDb)
{
    const Biquad bq = rbjBiquad(kind, freqHz, q, gainDb, in.sampleRateHz);
    AudioBuffer out = in;
    for (std::size_t c = 0; c < in.channelCount(); ++c)
        bq.run(in.channels[c], out.channels[c]);
    return out;
}

AudioBuffer applyDelay(const AudioBuffer &in, double delayMs,
                       double feedback, double mix)
{
    const double sr = in.sampleRateHz;
    const std::size_t dly =
        std::max<std::size_t>(1, static_cast<std::size_t>(delayMs / 1000.0 * sr));
    const std::size_t tail = static_cast<std::size_t>(sr * 2.0);
    const std::size_t n = in.sampleCount() + tail;
    AudioBuffer out = makeLike(in, n);
    // フィードバックの発振防止 (|fb| < 1 にクランプ)
    const double fb = std::max(-0.99, std::min(0.99, feedback));
    for (std::size_t c = 0; c < in.channelCount(); ++c) {
        const std::vector<double> &x = in.channels[c];
        std::vector<double> &y = out.channels[c];
        std::vector<double> wet(n, 0.0);
        for (std::size_t i = 0; i < n; ++i) {
            const double dryV = (i < x.size()) ? x[i] : 0.0;
            const double delayed = (i >= dly) ? wet[i - dly] : 0.0;
            wet[i] = dryV + delayed * fb;
            y[i] = dryV + delayed * mix;
        }
    }
    return out;
}

AudioBuffer applyCompressor(const AudioBuffer &in, double thresholdDb,
                            double ratio, double attackSec, double releaseSec)
{
    if (ratio < 1.0) ratio = 1.0;
    const double sr = in.sampleRateHz;
    const double aAtk = std::exp(-1.0 / std::max(1.0, attackSec * sr));
    const double aRel = std::exp(-1.0 / std::max(1.0, releaseSec * sr));
    AudioBuffer out = in;
    for (std::size_t c = 0; c < in.channelCount(); ++c) {
        const std::vector<double> &x = in.channels[c];
        std::vector<double> &y = out.channels[c];
        double env = 0.0;
        for (std::size_t i = 0; i < x.size(); ++i) {
            const double ax = std::fabs(x[i]);
            const double a = (ax > env) ? aAtk : aRel;
            env = a * env + (1.0 - a) * ax;
            const double envDb = 20.0 * std::log10(env + 1e-12);
            double gainDb = 0.0;
            if (envDb > thresholdDb)
                gainDb = (thresholdDb + (envDb - thresholdDb) / ratio) - envDb;
            y[i] = x[i] * std::pow(10.0, gainDb / 20.0);
        }
    }
    return out;
}

AudioBuffer applyRate(const AudioBuffer &in, double rate)
{
    if (rate <= 0.0) return in;
    const std::size_t n =
        static_cast<std::size_t>(std::ceil(in.sampleCount() / rate));
    AudioBuffer out = makeLike(in, n);
    for (std::size_t c = 0; c < in.channelCount(); ++c) {
        const std::vector<double> &x = in.channels[c];
        std::vector<double> &y = out.channels[c];
        for (std::size_t i = 0; i < n; ++i) {
            const double pos = i * rate;
            const std::size_t k = static_cast<std::size_t>(pos);
            if (k + 1 < x.size()) {
                const double f = pos - k;
                y[i] = x[k] * (1.0 - f) + x[k + 1] * f;
            } else if (k < x.size()) {
                y[i] = x[k];
            }
        }
    }
    return out;
}

AudioBuffer synthesizeHallIr(double rt60Sec, double sampleRate)
{
    AudioBuffer ir;
    ir.sampleRateHz = sampleRate;
    const std::size_t iN =
        static_cast<std::size_t>(std::ceil(sampleRate * rt60Sec * 1.3));
    ir.channels.assign(2, std::vector<double>(std::max<std::size_t>(iN, 1), 0.0));
    std::mt19937 rng = seededRng(0x0FD0A003u);
    std::uniform_real_distribution<double> uni(-1.0, 1.0);
    const std::size_t pre = static_cast<std::size_t>(sampleRate * 0.012);
    for (std::size_t c = 0; c < 2; ++c) {
        std::vector<double> &d = ir.channels[c];
        for (std::size_t i = 0; i < d.size(); ++i) {
            const double t = i / sampleRate;
            const double env = std::pow(10.0, -3.0 * t / rt60Sec);  // -60dB を RT で
            const double early = (i > pre && i < pre + 40) ? 1.6 : 1.0;
            d[i] = (i < pre) ? 0.0 : uni(rng) * env * early;
        }
        const std::size_t direct = static_cast<std::size_t>(sampleRate * 0.001);
        if (direct < d.size()) d[direct] = 0.9;  // 直接音
    }
    return ir;
}

AudioBuffer applyConvolution(const AudioBuffer &in, const AudioBuffer &ir,
                             double mix)
{
    acoustics::ConvolutionEngine engine;
    const acoustics::AcousticResult<acoustics::ConvolvedAudio> res =
        engine.convolve(in, ir);
    if (!res.success()) return in;
    const AudioBuffer &wet = res.value().audio;
    // ウェットのピークを IR エネルギーで正規化 (ConvolverNode.normalize 相当)
    double irEnergy = 0.0;
    for (const std::vector<double> &ch : ir.channels)
        for (double v : ch) irEnergy += v * v;
    irEnergy /= std::max<std::size_t>(1, ir.channelCount());
    const double norm = (irEnergy > 0.0) ? 1.0 / std::sqrt(irEnergy) : 1.0;

    const double dryGain = 1.0 - mix * 0.6;
    AudioBuffer out;
    out.sampleRateHz = in.sampleRateHz;
    const std::size_t n = wet.sampleCount();
    out.channels.assign(wet.channelCount(), std::vector<double>(n, 0.0));
    for (std::size_t c = 0; c < out.channelCount(); ++c) {
        // dry はモノ/同 ch を対応させる (wet が 2ch で dry が 1ch なら複製)
        const std::vector<double> &dry =
            in.channels[std::min(c, in.channelCount() - 1)];
        const std::vector<double> &w = wet.channels[c];
        std::vector<double> &y = out.channels[c];
        for (std::size_t i = 0; i < n; ++i) {
            const double dryV = (i < dry.size()) ? dry[i] : 0.0;
            y[i] = dryV * dryGain + w[i] * norm * mix;
        }
    }
    return out;
}

AudioBuffer applyReverb(const AudioBuffer &in, double rt60Sec, double mix)
{
    if (rt60Sec <= 0.0 || in.sampleRateHz <= 0.0) return in;
    return applyConvolution(in, synthesizeHallIr(rt60Sec, in.sampleRateHz),
                            mix);
}

AudioBuffer pitchShift(const AudioBuffer &in, double semitones)
{
    const double ratio = std::pow(2.0, semitones / 12.0);
    const double sr = in.sampleRateHz;
    const std::size_t N = in.sampleCount();
    const std::size_t grain =
        std::max<std::size_t>(8, static_cast<std::size_t>(sr * 0.08));
    const std::size_t hop = grain / 2;
    AudioBuffer out = makeLike(in, N);
    for (std::size_t c = 0; c < in.channelCount(); ++c) {
        const std::vector<double> &x = in.channels[c];
        std::vector<double> &y = out.channels[c];
        std::vector<double> wsum(N, 0.0);
        for (std::size_t o = 0; o < N; o += hop) {
            for (std::size_t i = 0; i < grain; ++i) {
                const std::size_t oi = o + i;
                if (oi >= N) break;
                const std::size_t si =
                    static_cast<std::size_t>(o + i * ratio);
                if (si >= N) break;
                const double w = 0.5 - 0.5 * std::cos(2 * kPi * i / grain);
                y[oi] += x[si] * w;
                wsum[oi] += w;
            }
        }
        for (std::size_t i = 0; i < N; ++i)
            if (wsum[i] > 1e-4) y[i] /= wsum[i];
    }
    return out;
}

AudioBuffer timeStretch(const AudioBuffer &in, double factor)
{
    if (factor <= 0.0) return in;
    const double sr = in.sampleRateHz;
    const std::size_t N = in.sampleCount();
    const std::size_t M = static_cast<std::size_t>(N * factor);
    const std::size_t grain =
        std::max<std::size_t>(8, static_cast<std::size_t>(sr * 0.09));
    const std::size_t synHop = grain / 2;
    const std::size_t anaHop =
        std::max<std::size_t>(1, static_cast<std::size_t>(synHop / factor));
    AudioBuffer out = makeLike(in, M);
    for (std::size_t c = 0; c < in.channelCount(); ++c) {
        const std::vector<double> &x = in.channels[c];
        std::vector<double> &y = out.channels[c];
        std::vector<double> wsum(M, 0.0);
        std::size_t oa = 0, os = 0;
        while (os + grain < M && oa + grain < N) {
            for (std::size_t i = 0; i < grain; ++i) {
                const double w = 0.5 - 0.5 * std::cos(2 * kPi * i / grain);
                y[os + i] += x[oa + i] * w;
                wsum[os + i] += w;
            }
            oa += anaHop;
            os += synHop;
        }
        for (std::size_t i = 0; i < M; ++i)
            if (wsum[i] > 1e-4) y[i] /= wsum[i];
    }
    return out;
}

std::vector<double> noiseProfile(const AudioBuffer &in, std::size_t a,
                                 std::size_t z)
{
    clampRange(in, a, z);
    const std::size_t FS = 2048;
    std::vector<double> prof(FS / 2 + 1, 0.0);
    if (in.channelCount() == 0) return prof;
    const std::vector<double> &x = in.channels[0];
    std::size_t cnt = 0;
    for (std::size_t o = a; o + FS <= z; o += FS) {
        const std::vector<std::complex<double>> spec =
            windowedFft(x, o, FS, WindowKind::Hann);
        for (std::size_t k = 0; k <= FS / 2; ++k)
            prof[k] += std::abs(spec[k]);
        ++cnt;
    }
    if (cnt == 0)
        return std::vector<double>();   // 選択が短すぎ (< 2048) — 学習失敗
    for (double &v : prof) v /= cnt;
    return prof;
}

AudioBuffer denoise(const AudioBuffer &in, const std::vector<double> &profile,
                    double reduceDb)
{
    const std::size_t FS = 2048, hop = FS / 4;
    if (profile.size() < FS / 2 + 1 || in.sampleCount() < FS) return in;
    const double g = std::pow(10.0, -reduceDb / 20.0);
    AudioBuffer out = makeLike(in, in.sampleCount());
    for (std::size_t c = 0; c < in.channelCount(); ++c) {
        const std::vector<double> &x = in.channels[c];
        std::vector<double> &y = out.channels[c];
        std::vector<double> wsum(x.size(), 0.0);
        for (std::size_t o = 0; o + FS <= x.size(); o += hop) {
            std::vector<std::complex<double>> spec =
                windowedFft(x, o, FS, WindowKind::Hann);
            for (std::size_t k = 0; k < FS; ++k) {
                const double mag = std::abs(spec[k]);
                // 上半分 (負周波数) は共役対称の鏡像ビンのプロファイルを使う
                const double np = profile[std::min(k, FS - k)] * 2.5;
                if (np <= 0.0) continue;   // プロファイル無しのビンは素通し
                if (mag < np) {
                    spec[k] *= g;                                  // ゲート
                } else {
                    const double soft = std::min(1.0, (mag - np) / np);
                    spec[k] *= g + (1.0 - g) * soft;
                }
            }
            acoustics::fftInverse(spec);
            for (std::size_t i = 0; i < FS; ++i) {
                const double w = 0.5 - 0.5 * std::cos(2 * kPi * i / FS);
                y[o + i] += spec[i].real() * w;
                wsum[o + i] += w * w;
            }
        }
        for (std::size_t i = 0; i < x.size(); ++i)
            if (wsum[i] > 1e-4) y[i] /= wsum[i];
            else y[i] = x[i];   // 端の未処理部は原音
    }
    return out;
}

AudioBuffer applyStereoOp(const AudioBuffer &in, StereoOp op)
{
    const std::size_t n = in.sampleCount();
    AudioBuffer out;
    out.sampleRateHz = in.sampleRateHz;
    out.channels.assign(2, std::vector<double>(n, 0.0));
    if (in.channelCount() == 0) return out;
    const std::vector<double> &L = in.channels[0];
    const std::vector<double> &R =
        in.channels[in.channelCount() > 1 ? 1 : 0];
    std::vector<double> &l = out.channels[0];
    std::vector<double> &r = out.channels[1];
    for (std::size_t i = 0; i < n; ++i) {
        switch (op) {
        case StereoOp::Mono: {
            const double m = (L[i] + R[i]) / 2;
            l[i] = m; r[i] = m;
            break;
        }
        case StereoOp::Swap:
            l[i] = R[i]; r[i] = L[i];
            break;
        case StereoOp::Side: {
            const double s = (L[i] - R[i]) / 2;
            l[i] = s; r[i] = -s;
            break;
        }
        case StereoOp::Widen: {
            const double m = (L[i] + R[i]) / 2, s = (L[i] - R[i]) / 2 * 1.6;
            l[i] = m + s; r[i] = m - s;
            break;
        }
        case StereoOp::InvertLeft:
            l[i] = -L[i]; r[i] = R[i];
            break;
        }
    }
    return out;
}

// ── 解析 ────────────────────────────────────────────────────────────────────
std::vector<SpectrumPoint> spectrum(const AudioBuffer &in, std::size_t a,
                                    WindowKind window, std::size_t fftSize)
{
    std::vector<SpectrumPoint> pts;
    if (in.channelCount() == 0 || in.sampleCount() == 0 ||
        in.sampleRateHz <= 0.0)
        return pts;
    const std::vector<double> &d = in.channels[0];
    const std::size_t off =
        (d.size() > fftSize) ? std::min(a, d.size() - fftSize) : 0;
    const std::vector<std::complex<double>> spec =
        windowedFft(d, off, fftSize, window);
    const double sr = in.sampleRateHz;
    for (std::size_t k = 2; k < fftSize / 2;
         k = static_cast<std::size_t>(std::ceil(k * 1.06))) {
        const double f = k * sr / fftSize;
        const double db =
            20.0 * std::log10(std::abs(spec[k]) / (fftSize / 4.0) + 1e-9);
        pts.push_back({ std::log10(f), db });
    }
    return pts;
}

LevelMetrics analyzeLevels(const AudioBuffer &in, std::size_t a,
                           std::size_t z)
{
    LevelMetrics m;
    clampRange(in, a, z);
    if (in.channelCount() == 0 || a >= z || in.sampleRateHz <= 0.0) return m;
    const std::vector<double> &d = in.channels[0];
    const double sr = in.sampleRateHz;
    const std::size_t len = z - a;
    m.durationSec = len / sr;

    double pk = 0.0, sum = 0.0, dc = 0.0;
    for (std::size_t i = a; i < z; ++i) {
        const double v = d[i];
        pk = std::max(pk, std::fabs(v));
        sum += v * v;
        dc += v;
    }
    const double rms = std::sqrt(sum / len);
    m.peakDbfs = 20.0 * std::log10(pk + 1e-12);
    m.rmsDbfs  = 20.0 * std::log10(rms + 1e-12);
    m.crestDb  = 20.0 * std::log10((pk + 1e-12) / (rms + 1e-12));
    m.dcOffset = dc / len;

    // Schroeder 逆積分 (選択範囲を IR とみなす)。到達しないレベルは無効のまま
    std::vector<double> sch(len);
    double acc = 0.0;
    for (std::size_t i = z; i-- > a;) {
        acc += d[i] * d[i];
        sch[i - a] = acc;
    }
    const double ref = 10.0 * std::log10(sch[0] + 1e-30);
    auto timeAt = [&](double targetDb, double *outSec) -> bool {
        for (std::size_t i = 0; i < len; ++i) {
            if (10.0 * std::log10(sch[i] + 1e-30) - ref <= targetDb) {
                *outSec = i / sr;
                return true;
            }
        }
        return false;
    };
    double t5 = 0, t10 = 0, t25 = 0, t35 = 0;
    const bool h5 = timeAt(-5.0, &t5), h10 = timeAt(-10.0, &t10);
    const bool h25 = timeAt(-25.0, &t25), h35 = timeAt(-35.0, &t35);
    if (h10)       { m.hasEdt = true; m.edtSec = t10 * 6.0; }
    if (h5 && h25) { m.hasT20 = true; m.t20Sec = (t25 - t5) * 3.0; }
    if (h5 && h35) { m.hasT30 = true; m.t30Sec = (t35 - t5) * 2.0; }
    return m;
}

LoudnessMetrics analyzeLoudness(const AudioBuffer &in)
{
    LoudnessMetrics lm;
    const double sr = in.sampleRateHz;
    if (in.channelCount() == 0 || in.sampleCount() == 0 || sr <= 0.0)
        return lm;

    // K 特性 (BS.1770): 高域シェルフ + ハイパスをアナログ原型から任意 fs で導出
    // (原型パラメータは ITU-R BS.1770 の 48kHz 係数を再現する公知の値)
    const Biquad shelf = rbjHighShelf(1681.9744509555319, 0.7071752369554193,
                                      3.99984385397, sr);
    const Biquad hp = rbjBiquad(BiquadKind::HighPass, 38.13547087613982,
                                0.5003270373253953, 0.0, sr);

    std::vector<std::vector<double>> filt(in.channelCount());
    for (std::size_t c = 0; c < in.channelCount(); ++c) {
        std::vector<double> t;
        shelf.run(in.channels[c], t);
        hp.run(t, filt[c]);
    }

    // 400ms ブロック, 75% オーバーラップ
    const std::size_t blk = static_cast<std::size_t>(sr * 0.4);
    const std::size_t hop = std::max<std::size_t>(1, blk / 4);
    std::vector<double> blocks;
    if (blk > 0) {
        for (std::size_t o = 0; o + blk <= in.sampleCount(); o += hop) {
            double s = 0.0;
            for (const std::vector<double> &y : filt)
                for (std::size_t i = o; i < o + blk; ++i) s += y[i] * y[i];
            blocks.push_back(-0.691 +
                10.0 * std::log10(s / (blk * filt.size()) + 1e-15));
        }
    }
    if (blocks.empty()) return lm;
    lm.momentaryMaxLufs = *std::max_element(blocks.begin(), blocks.end());

    // 絶対ゲート -70 LUFS → 相対ゲート -10 LU
    std::vector<double> g1;
    for (double b : blocks) if (b > -70.0) g1.push_back(b);
    if (g1.empty()) return lm;
    double e = 0.0;
    for (double b : g1) e += std::pow(10.0, b / 10.0);
    const double m1 = 10.0 * std::log10(e / g1.size());
    std::vector<double> g2;
    for (double b : g1) if (b > m1 - 10.0) g2.push_back(b);
    if (!g2.empty()) {
        e = 0.0;
        for (double b : g2) e += std::pow(10.0, b / 10.0);
        lm.integratedLufs = 10.0 * std::log10(e / g2.size());
        if (g2.size() > 4) {
            std::vector<double> sorted = g2;
            std::sort(sorted.begin(), sorted.end());
            lm.rangeLu = sorted[static_cast<std::size_t>(sorted.size() * 0.95)]
                       - sorted[static_cast<std::size_t>(sorted.size() * 0.10)];
        }
    }

    // True Peak (簡易 4x 線形補間オーバーサンプリング)
    double tp = 0.0;
    for (const std::vector<double> &d : in.channels) {
        for (std::size_t i = 0; i + 1 < d.size(); ++i) {
            const double x0 = d[i], x1 = d[i + 1];
            tp = std::max({ tp, std::fabs(x0),
                            std::fabs(x0 * 0.75 + x1 * 0.25),
                            std::fabs((x0 + x1) / 2),
                            std::fabs(x0 * 0.25 + x1 * 0.75) });
        }
        if (!d.empty()) tp = std::max(tp, std::fabs(d.back()));
    }
    lm.truePeakDbtp = 20.0 * std::log10(tp + 1e-12);
    return lm;
}

std::vector<OctaveBand> octaveBands(const AudioBuffer &in)
{
    std::vector<OctaveBand> out;
    if (in.channelCount() == 0 || in.sampleCount() == 0 ||
        in.sampleRateHz <= 0.0)
        return out;
    const std::size_t FS = 8192;
    const std::vector<double> &x = in.channels[0];
    const std::size_t n = std::min(FS, x.size());
    std::vector<std::complex<double>> spec(FS);
    for (std::size_t i = 0; i < n; ++i)
        spec[i] = x[i] * (0.5 - 0.5 * std::cos(2 * kPi * i / n));
    acoustics::fftForward(spec);
    const double sr = in.sampleRateHz;
    static const double kBands[] = { 31.5, 63, 125, 250, 500,
                                     1000, 2000, 4000, 8000, 16000 };
    for (double fc : kBands) {
        // Nyquist 超の帯域は測れない — 「それらしい値」を返さず除外する
        if (fc / std::sqrt(2.0) >= sr * 0.5) continue;
        const std::size_t k1 = std::max<std::size_t>(
            1, static_cast<std::size_t>(fc / std::sqrt(2.0) * FS / sr));
        const std::size_t k2 = std::min<std::size_t>(
            FS / 2 - 1,
            static_cast<std::size_t>(std::ceil(fc * std::sqrt(2.0) * FS / sr)));
        double s = 0.0;
        for (std::size_t k = k1; k <= k2 && k1 <= k2; ++k)
            s += std::norm(spec[k]);
        out.push_back({ fc, 10.0 * std::log10(s / (FS * FS / 16.0) + 1e-15) });
    }
    return out;
}

std::vector<float> spectrogram(const AudioBuffer &in, int cols, int rows,
                               WindowKind window, std::size_t fftSize)
{
    std::vector<float> img;
    if (cols <= 0 || rows <= 0 || in.channelCount() == 0 ||
        in.sampleCount() == 0)
        return img;
    img.assign(static_cast<std::size_t>(cols) * rows, 0.0f);
    const std::vector<double> &d = in.channels[0];
    const std::size_t N = d.size();
    const std::size_t hop =
        std::max<std::size_t>(1, N / static_cast<std::size_t>(cols));
    for (int xcol = 0; xcol < cols; ++xcol) {
        const std::vector<std::complex<double>> spec =
            windowedFft(d, static_cast<std::size_t>(xcol) * hop, fftSize,
                        window);
        for (int y = 0; y < rows; ++y) {
            // 上端 = 高周波。周波数軸はべき乗マッピング (mock と同じ)
            const std::size_t k = static_cast<std::size_t>(
                std::pow(1.0 - static_cast<double>(y) / rows, 2.2)
                * (fftSize / 2.0 - 1.0)) + 1;
            const double mag = std::abs(spec[k]) / (fftSize / 4.0);
            const double db = 20.0 * std::log10(mag + 1e-9);
            const double p = std::max(0.0, std::min(1.0, (db + 84.0) / 84.0));
            img[static_cast<std::size_t>(y) * cols + xcol] =
                static_cast<float>(p);
        }
    }
    return img;
}

} // namespace audioedit
} // namespace ofd
