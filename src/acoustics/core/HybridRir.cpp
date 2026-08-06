// HybridRir.cpp — 低域 FDTD + 高域 幾何音響 のハイブリッド RIR 合成の実装。
// 設計根拠はヘッダ冒頭を参照。
#include "HybridRir.h"

#include "Fft.h"
#include "Resampler.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ofd {
namespace acoustics {

namespace {

const double kPi = 3.14159265358979323846;
const double kStopbandDb = 90.0;   // クロスオーバー FIR の阻止域減衰 [dB]

// 第 1 種 0 次変形ベッセル関数 (Resampler と同じ級数)
double besselI0(double x)
{
    double sum = 1.0, term = 1.0;
    const double half = 0.5 * x;
    for (int k = 1; k <= 64; ++k) {
        term *= (half / static_cast<double>(k));
        const double add = term * term;
        sum += add;
        if (add < sum * 1e-17) break;
    }
    return sum;
}

// 直接畳み込み (RIR は高々数秒なので FFT 化しない — 決定的で読みやすい)。
// 出力長は x.size() + h.size() - 1。
std::vector<double> convolve(const std::vector<double> &x,
                             const std::vector<double> &h)
{
    if (x.empty() || h.empty()) return std::vector<double>();
    std::vector<double> y(x.size() + h.size() - 1, 0.0);
    for (std::size_t i = 0; i < x.size(); ++i) {
        const double xi = x[i];
        if (xi == 0.0) continue;
        for (std::size_t k = 0; k < h.size(); ++k) y[i + k] += xi * h[k];
    }
    return y;
}

// [f1, f2] の帯域エネルギー (片側スペクトルの |X|² 合計)。
double bandEnergy(const std::vector<double> &x, double fs, double f1,
                  double f2)
{
    if (x.empty() || !(fs > 0.0)) return 0.0;
    const AcousticResult<std::vector<std::complex<double>>> sp =
        realFft(ArrayView<const double>(x));
    if (!sp.success()) return 0.0;
    const std::vector<std::complex<double>> &X = sp.value();
    const std::size_t n = X.size();
    double e = 0.0;
    for (std::size_t k = 1; k < n / 2; ++k) {
        const double f = double(k) * fs / double(n);
        if (f < f1 || f > f2) continue;
        e += std::norm(X[k]);
    }
    return e;
}

// 先頭チャンネルを取り出す (多チャンネルは warning)
const std::vector<double> *firstChannel(const AudioBuffer &b, const char *what,
                                        std::vector<std::string> *warn)
{
    if (b.channelCount() == 0 || b.sampleCount() == 0) return 0;
    if (b.channelCount() > 1 && warn) {
        char buf[128];
        std::snprintf(buf, sizeof(buf),
                      "%s is multi-channel; only channel 1 is used", what);
        warn->push_back(buf);
    }
    return &b.channels[0];
}

} // namespace

AcousticResult<std::vector<double>>
deconvolveSourcePulse(ArrayView<const double> rir, double sampleRateHz,
                      double sigmaS, double t0S, double epsilon)
{
    typedef AcousticResult<std::vector<double>> Result;
    if (rir.empty())
        return Result::error(AcousticErrorCode::EmptyInput, "rir is empty");
    if (!(sampleRateHz > 0.0))
        return Result::error(AcousticErrorCode::InvalidArgument,
                             "sample rate must be positive");
    if (!(sigmaS > 0.0))
        return Result::error(AcousticErrorCode::InvalidArgument,
                             "source sigma must be positive");
    if (!(epsilon > 0.0))
        return Result::error(AcousticErrorCode::InvalidArgument,
                             "regularization epsilon must be positive");

    const std::size_t n = rir.size();
    // 循環畳み込みの巻き込みを避けるため 2 倍以上へゼロ詰めする
    const std::size_t N = nextPowerOfTwo(2 * n + 1);

    // 音源パルスを同じ時間格子でサンプリングする。t0 を含むので、これで
    // 割ると t0 の遅延も一緒に落ちる (= 直接音が r/c に来る)。
    std::vector<std::complex<double>> S(N, std::complex<double>(0.0, 0.0));
    for (std::size_t k = 0; k < N; ++k) {
        const double t = double(k) / sampleRateHz;
        const double u = (t - t0S) / sigmaS;
        if (u > 12.0) break;               // 以降は完全に 0 (exp(-72))
        if (u < -12.0) continue;
        S[k] = std::complex<double>(-u * std::exp(0.5 - 0.5 * u * u), 0.0);
    }
    std::vector<std::complex<double>> P(N, std::complex<double>(0.0, 0.0));
    for (std::size_t k = 0; k < n; ++k)
        P[k] = std::complex<double>(rir[k], 0.0);

    if (!fftForward(S) || !fftForward(P))
        return Result::error(AcousticErrorCode::InvalidArgument,
                             "internal error: FFT length is not a power of two");

    double maxS2 = 0.0;
    for (std::size_t k = 0; k < N; ++k)
        maxS2 = std::max(maxS2, std::norm(S[k]));
    if (!(maxS2 > 0.0))
        return Result::error(AcousticErrorCode::InvalidArgument,
                             "source pulse spectrum is empty (sigma too small "
                             "for this sample rate?)");

    const double reg = epsilon * maxS2;
    for (std::size_t k = 0; k < N; ++k)
        P[k] = P[k] * std::conj(S[k]) / (std::norm(S[k]) + reg);
    if (!fftInverse(P))
        return Result::error(AcousticErrorCode::InvalidArgument,
                             "internal error: inverse FFT failed");

    std::vector<double> h(n, 0.0);
    for (std::size_t k = 0; k < n; ++k) h[k] = P[k].real();
    return Result::ok(h);
}

AcousticResult<std::vector<double>>
designComplementaryLowpass(double sampleRateHz, double crossoverHz,
                           double transitionWidth)
{
    typedef AcousticResult<std::vector<double>> Result;
    if (!(sampleRateHz > 0.0))
        return Result::error(AcousticErrorCode::InvalidArgument,
                             "sample rate must be positive");
    if (!(crossoverHz > 0.0) || crossoverHz >= 0.5 * sampleRateHz)
        return Result::error(AcousticErrorCode::InvalidArgument,
                             "crossover must be in (0, fs/2)");
    if (!(transitionWidth > 0.0))
        return Result::error(AcousticErrorCode::InvalidArgument,
                             "transition width must be positive");

    // Kaiser 窓 sinc (Resampler と同じ設計式)
    const double dw = 2.0 * kPi * transitionWidth * crossoverHz / sampleRateHz;
    const double beta = 0.1102 * (kStopbandDb - 8.7);
    long long half = static_cast<long long>(
        std::ceil(((kStopbandDb - 7.95) / (2.285 * dw) - 1.0) / 2.0));
    if (half < 1) half = 1;
    // 遷移帯域が下端を割らないよう、長すぎる設計は入力長で頭打ちにしない
    // (RIR より長い FIR でも畳み込みは成立する — 精度を優先する)

    const double fc = crossoverHz / sampleRateHz;   // 正規化 (cycles/sample)
    const std::size_t len = static_cast<std::size_t>(2 * half + 1);
    std::vector<double> h(len, 0.0);
    const double i0b = besselI0(beta);
    for (long long k = -half; k <= half; ++k) {
        const double r = double(k) / double(half);
        const double win = besselI0(beta * std::sqrt(1.0 - r * r)) / i0b;
        const double ideal = (k == 0)
            ? 2.0 * fc
            : std::sin(2.0 * kPi * fc * double(k)) / (kPi * double(k));
        h[static_cast<std::size_t>(k + half)] = ideal * win;
    }
    // DC 利得を厳密に 1 にする (相補な高域側の DC 応答を厳密に 0 にするため)
    double sum = 0.0;
    for (std::size_t i = 0; i < len; ++i) sum += h[i];
    if (sum == 0.0)
        return Result::error(AcousticErrorCode::InvalidArgument,
                             "degenerate lowpass design");
    for (std::size_t i = 0; i < len; ++i) h[i] /= sum;
    return Result::ok(h);
}

AcousticResult<AudioBuffer>
buildHybridRir(const AudioBuffer &fdtdRir, const AudioBuffer &gaRir,
               const HybridRirConfig &cfg, HybridRirInfo *outInfo)
{
    typedef AcousticResult<AudioBuffer> Result;
    HybridRirInfo info;

    const std::vector<double> *lowIn =
        firstChannel(fdtdRir, "FDTD RIR", &info.warnings);
    const std::vector<double> *highIn =
        firstChannel(gaRir, "geometric RIR", &info.warnings);
    if (!lowIn)
        return Result::error(AcousticErrorCode::EmptyInput,
                             "FDTD RIR is empty");
    if (!highIn)
        return Result::error(AcousticErrorCode::EmptyInput,
                             "geometric RIR is empty");
    if (!(fdtdRir.sampleRateHz > 0.0) || !(gaRir.sampleRateHz > 0.0))
        return Result::error(AcousticErrorCode::InvalidArgument,
                             "both RIRs need a positive sample rate");

    const double fsOut = gaRir.sampleRateHz;
    info.outputRateHz = fsOut;

    // ── クロスオーバーの決定 ────────────────────────────────────────────────
    double fx = (cfg.crossoverHz > 0.0) ? cfg.crossoverHz : cfg.fdtdFmaxHz;
    if (!(fx > 0.0))
        return Result::error(AcousticErrorCode::InvalidArgument,
                             "crossover frequency is unknown (set crossoverHz "
                             "or fdtdFmaxHz from metadata.json)");
    // FDTD の物理的な上限を超えて低域側に持たせない (分散誤差の帯域を使わない)
    if (cfg.fdtdFmaxHz > 0.0 && fx > cfg.fdtdFmaxHz) {
        char buf[160];
        std::snprintf(buf, sizeof(buf),
                      "crossover %.1f Hz exceeds the FDTD valid band %.1f Hz "
                      "(dispersion error above it)", fx, cfg.fdtdFmaxHz);
        info.warnings.push_back(buf);
    }
    const double nyqLow = 0.5 * fdtdRir.sampleRateHz;
    if (fx >= nyqLow) {
        char buf[160];
        std::snprintf(buf, sizeof(buf),
                      "crossover %.1f Hz is at or above the FDTD Nyquist "
                      "%.1f Hz", fx, nyqLow);
        return Result::error(AcousticErrorCode::InvalidArgument, buf);
    }
    if (fx >= 0.5 * fsOut)
        return Result::error(AcousticErrorCode::InvalidArgument,
                             "crossover is at or above the output Nyquist");
    info.crossoverHz = fx;

    // ── 低域: 逆フィルタ → リサンプル ──────────────────────────────────────
    std::vector<double> low = *lowIn;
    if (cfg.sourceSigmaS > 0.0) {
        const AcousticResult<std::vector<double>> dec = deconvolveSourcePulse(
            ArrayView<const double>(low), fdtdRir.sampleRateHz,
            cfg.sourceSigmaS, cfg.sourceT0S, cfg.deconvEpsilon);
        if (!dec.success())
            return Result::error(dec.errorCode(),
                                 std::string("source deconvolution: ") +
                                     dec.message());
        low = dec.value();
        info.deconvolved = true;
        info.removedDelayS = cfg.sourceT0S;
    } else {
        info.warnings.push_back(
            "source pulse was not deconvolved (sigma unknown): the low band "
            "still carries the excitation spectrum and its t0 delay");
    }
    if (fdtdRir.sampleRateHz != fsOut) {
        AudioBuffer tmp;
        tmp.sampleRateHz = fdtdRir.sampleRateHz;
        tmp.channels.assign(1, low);
        const AcousticResult<AudioBuffer> rs = resampleBuffer(tmp, fsOut);
        if (!rs.success())
            return Result::error(rs.errorCode(),
                                 std::string("low band resample: ") +
                                     rs.message());
        low = rs.value().channels[0];
        info.resampled = true;
    }

    // ── クロスオーバー FIR (相補対) ────────────────────────────────────────
    const AcousticResult<std::vector<double>> lp =
        designComplementaryLowpass(fsOut, fx, cfg.transitionWidth);
    if (!lp.success()) return Result::error(lp.errorCode(), lp.message());
    const std::vector<double> &hlp = lp.value();
    const std::size_t center = (hlp.size() - 1) / 2;
    info.filterLength = hlp.size();

    // ── レベル整合 (クロスオーバー帯のバンドエネルギーを合わせる) ──────────
    double gain = 1.0;
    if (cfg.matchLevels) {
        const double f1 = fx * (1.0 - 0.5 * cfg.transitionWidth);
        const double eLow = bandEnergy(low, fsOut, f1, fx);
        const double eHigh = bandEnergy(*highIn, fsOut, f1, fx);
        if (eLow > 0.0 && eHigh > 0.0) {
            gain = std::sqrt(eHigh / eLow);
        } else {
            info.warnings.push_back(
                "level matching skipped: one of the RIRs has no energy in the "
                "crossover band");
        }
    }
    info.fdtdGainDb = 20.0 * std::log10(gain > 0.0 ? gain : 1.0);

    // ── 合成: y = g·LP(low) + HP(high), HP = δ[center] − LP ────────────────
    const std::vector<double> lowLp = convolve(low, hlp);
    const std::vector<double> highLp = convolve(*highIn, hlp);
    const std::size_t nOut = std::max(low.size(), highIn->size());
    std::vector<double> y(nOut, 0.0);
    for (std::size_t i = 0; i < nOut; ++i) {
        const std::size_t j = i + center;   // 共通遅延を落として時間原点を保つ
        double v = 0.0;
        if (j < lowLp.size()) v += gain * lowLp[j];
        if (i < highIn->size()) v += (*highIn)[i];   // δ[center] 側 (遅延補正後)
        if (j < highLp.size()) v -= highLp[j];
        y[i] = v;
    }

    AudioBuffer out;
    out.sampleRateHz = fsOut;
    out.channels.assign(1, y);
    info.outputLength = y.size();
    if (outInfo) *outInfo = info;
    return Result::ok(out);
}

} // namespace acoustics
} // namespace ofd
