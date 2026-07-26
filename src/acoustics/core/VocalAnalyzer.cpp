// VocalAnalyzer.cpp — 歌唱音声分析の実装。
//
// 出典:
//   YIN 法      : de Cheveigné & Kawahara (2002) "YIN, a fundamental
//                 frequency estimator for speech and music", JASA 111(4).
//   HNR         : Boersma (1993) "Accurate short-term analysis of the
//                 fundamental frequency and the harmonics-to-noise ratio
//                 of a sampled sound", IFA Proceedings 17.
//   LTAS        : Welch (1967) 平均ペリオドグラム法。
//   歌手フォルマント : Sundberg (1974) "Articulatory interpretation of the
//                 'singing formant'", JASA 55(4)。ここでは 2-4 kHz / 0-2 kHz
//                 の LTAS エネルギー比という物理量のみを算出する。
//
// 注意: VoiceType は F0 探索範囲プリセットの選択にのみ用いる。声種から
// 医学的・教育的な結論を導く処理はここには存在しない (意図的な設計)。
#include "VocalAnalyzer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "Fft.h"

namespace ofd {
namespace acoustics {

namespace {

const double kPi = 3.14159265358979323846;
const double kTinyPower = 1e-30;

double toDbAmp(double a) {
    return (a > 0.0) ? 20.0 * std::log10(a) : -300.0;
}
double toDbPower(double p) {
    return (p > 0.0) ? 10.0 * std::log10(p) : -300.0;
}

double medianOf(std::vector<double> v) {
    if (v.empty()) return 0.0;
    const std::size_t mid = v.size() / 2;
    std::nth_element(v.begin(), v.begin() + mid, v.end());
    double m = v[mid];
    if (v.size() % 2 == 0) {
        std::nth_element(v.begin(), v.begin() + mid - 1, v.begin() + mid);
        m = 0.5 * (m + v[mid - 1]);
    }
    return m;
}

// 3 点放物線補間: y2 が極値のとき、頂点の x オフセット (x2 基準, [-1, 1])
double parabolicOffset(double y1, double y2, double y3) {
    const double denom = y1 - 2.0 * y2 + y3;
    if (std::fabs(denom) < 1e-30) return 0.0;
    double off = 0.5 * (y1 - y3) / denom;
    if (off > 1.0) off = 1.0;
    if (off < -1.0) off = -1.0;
    return off;
}

// 4x4 正規方程式のガウス消去 (部分ピボット)。特異なら false。
bool solve4x4(double a[4][4], double b[4], double x[4]) {
    int idx[4] = {0, 1, 2, 3};
    for (int col = 0; col < 4; ++col) {
        int piv = col;
        for (int r = col + 1; r < 4; ++r) {
            if (std::fabs(a[idx[r]][col]) > std::fabs(a[idx[piv]][col]))
                piv = r;
        }
        std::swap(idx[col], idx[piv]);
        const double p = a[idx[col]][col];
        if (std::fabs(p) < 1e-15) return false;
        for (int r = col + 1; r < 4; ++r) {
            const double f = a[idx[r]][col] / p;
            for (int c = col; c < 4; ++c) a[idx[r]][c] -= f * a[idx[col]][c];
            b[idx[r]] -= f * b[idx[col]];
        }
    }
    for (int col = 3; col >= 0; --col) {
        double s = b[idx[col]];
        for (int c = col + 1; c < 4; ++c) s -= a[idx[col]][c] * x[c];
        x[col] = s / a[idx[col]][col];
    }
    return true;
}

// ── YIN (de Cheveigné & Kawahara 2002) ──
struct YinParams {
    std::size_t frameLength;  // 積分窓 W [サンプル]
    std::size_t hopLength;    // ホップ [サンプル]
    std::size_t tauMin;       // = fs / f0Max
    std::size_t tauMax;       // = fs / f0Min
    double threshold;         // CMNDF 絶対閾値
    double noiseGateDbfs;
};

// 1 フレームの YIN。x は先頭が frame 開始、長さ >= W + tauMax。
F0Frame yinFrame(const double *x, const YinParams &p, double fs) {
    F0Frame frame;
    const std::size_t w = p.frameLength;

    // フレーム RMS (積分窓部分)
    double e = 0.0;
    for (std::size_t j = 0; j < w; ++j) e += x[j] * x[j];
    frame.rmsDbfs = toDbAmp(std::sqrt(e / static_cast<double>(w)));

    // 差分関数 d(τ), τ = 1..tauMax (CMNDF の累積は τ=1 から必要)
    std::vector<double> cmndf(p.tauMax + 1, 1.0);
    double cumulative = 0.0;
    for (std::size_t tau = 1; tau <= p.tauMax; ++tau) {
        double d = 0.0;
        const double *a = x;
        const double *b = x + tau;
        for (std::size_t j = 0; j < w; ++j) {
            const double diff = a[j] - b[j];
            d += diff * diff;
        }
        cumulative += d;
        cmndf[tau] = (cumulative > 0.0)
                         ? d * static_cast<double>(tau) / cumulative
                         : 1.0;
    }

    // 絶対閾値: 探索範囲内で最初に閾値を切る谷。なければ範囲内最小値。
    std::size_t best = 0;
    for (std::size_t tau = p.tauMin; tau <= p.tauMax; ++tau) {
        if (cmndf[tau] < p.threshold) {
            while (tau + 1 <= p.tauMax && cmndf[tau + 1] < cmndf[tau]) ++tau;
            best = tau;
            break;
        }
    }
    if (best == 0) {
        best = p.tauMin;
        for (std::size_t tau = p.tauMin + 1; tau <= p.tauMax; ++tau) {
            if (cmndf[tau] < cmndf[best]) best = tau;
        }
    }
    frame.cmndfMin = cmndf[best];

    // 無声判定: CMNDF 最小値 > 閾値、またはフレーム RMS < ノイズゲート
    if (frame.cmndfMin > p.threshold || frame.rmsDbfs < p.noiseGateDbfs) {
        frame.voiced = false;
        frame.f0Hz = 0.0;
        return frame;
    }

    // 放物線補間で τ を細分化
    double tauF = static_cast<double>(best);
    if (best > 1 && best < p.tauMax) {
        tauF += parabolicOffset(cmndf[best - 1], cmndf[best], cmndf[best + 1]);
    }
    frame.voiced = true;
    frame.f0Hz = fs / tauF;
    return frame;
}

// ── HNR (Boersma 1993, 自己相関法) ──
// 正規化相互相関 r(τ) を τ0 近傍 3 点 + 放物線補間で求め、
// HNR = 10·log10(r / (1 - r)) を返す。評価不能なら false。
bool hnrForFrame(const double *x, std::size_t avail, std::size_t w,
                 double fs, double f0Hz, double &hnrDb) {
    if (f0Hz <= 0.0) return false;
    const std::size_t t0 =
        static_cast<std::size_t>(fs / f0Hz + 0.5);
    if (t0 < 2 || w + t0 + 1 >= avail) return false;

    double r3[3];
    for (int k = -1; k <= 1; ++k) {
        const std::size_t tau = t0 + static_cast<std::size_t>(k + 1) - 1;
        double num = 0.0, e0 = 0.0, e1 = 0.0;
        const double *b = x + tau;
        for (std::size_t j = 0; j < w; ++j) {
            num += x[j] * b[j];
            e0 += x[j] * x[j];
            e1 += b[j] * b[j];
        }
        const double denom = std::sqrt(e0 * e1);
        r3[k + 1] = (denom > 0.0) ? num / denom : 0.0;
    }
    double r = r3[1];
    const double off = parabolicOffset(r3[0], r3[1], r3[2]);
    // 頂点値 (放物線) — 谷でなく山であることは r3[1] が最大でなくても
    // 近似としてそのまま使う
    const double denom = r3[0] - 2.0 * r3[1] + r3[2];
    if (std::fabs(denom) > 1e-30) r = r3[1] - 0.25 * (r3[0] - r3[2]) * off;
    if (r < 1e-6) r = 1e-6;             // HNR 下限 -60 dB
    if (r > 1.0 - 1e-6) r = 1.0 - 1e-6; // HNR 上限 +60 dB
    hnrDb = 10.0 * std::log10(r / (1.0 - r));
    return true;
}

// ── LTAS (Welch 法): 片側パワースペクトルの平均 ──
// 戻り値 power[k] は正弦波実効値基準 (振幅 A の正弦波 → ピーク A²/2)。
std::vector<double> computeWelchLtas(ArrayView<const double> x, double fs,
                                     std::size_t fftLen, LtasResult &out) {
    const std::size_t n = x.size();
    std::vector<double> window(fftLen);
    double windowSum = 0.0;
    for (std::size_t j = 0; j < fftLen; ++j) {
        window[j] = 0.5 * (1.0 - std::cos(2.0 * kPi * static_cast<double>(j) /
                                          static_cast<double>(fftLen - 1)));
        windowSum += window[j];
    }

    const std::size_t half = fftLen / 2;
    std::vector<double> accum(half + 1, 0.0);
    std::vector<std::complex<double>> buf(fftLen);
    std::size_t frames = 0;

    if (n < fftLen) {
        // 信号が短い場合はゼロ詰め 1 フレーム (警告)
        for (std::size_t j = 0; j < fftLen; ++j) {
            const double v = (j < n) ? x[j] : 0.0;
            buf[j] = std::complex<double>(v * window[j], 0.0);
        }
        fftForward(buf);
        for (std::size_t k = 0; k <= half; ++k)
            accum[k] += std::norm(buf[k]);
        frames = 1;
        out.warning = "信号長が LTAS の FFT 長より短いためゼロ詰めしました";
    } else {
        const std::size_t hop = fftLen / 2; // 50% オーバーラップ
        for (std::size_t start = 0; start + fftLen <= n; start += hop) {
            for (std::size_t j = 0; j < fftLen; ++j)
                buf[j] = std::complex<double>(x[start + j] * window[j], 0.0);
            fftForward(buf);
            for (std::size_t k = 0; k <= half; ++k)
                accum[k] += std::norm(buf[k]);
            ++frames;
        }
    }

    const double scale =
        1.0 / (static_cast<double>(frames) * windowSum * windowSum);
    std::vector<double> power(half + 1, 0.0);
    out.frequenciesHz.resize(half + 1);
    out.levelsDb.resize(half + 1);
    for (std::size_t k = 0; k <= half; ++k) {
        const double factor = (k == 0 || k == half) ? 1.0 : 2.0;
        power[k] = factor * accum[k] * scale;
        out.frequenciesHz[k] = static_cast<double>(k) * fs /
                               static_cast<double>(fftLen);
        out.levelsDb[k] = toDbPower(power[k] + kTinyPower);
    }
    out.frameCount = frames;
    out.valid = true;
    return power;
}

// LTAS パワーの帯域積算 ([lowHz, highHz)。fullBand は全ビン)
double bandPowerSum(const std::vector<double> &power,
                    const std::vector<double> &freqs, const Band &band) {
    double sum = 0.0;
    for (std::size_t k = 0; k < power.size(); ++k) {
        if (band.fullBand ||
            (freqs[k] >= band.lowHz && freqs[k] < band.highHz)) {
            sum += power[k];
        }
    }
    return sum;
}

// ── ビブラート分析 ──
// 有声区間の F0 軌跡 [cent] を移動平均 (既定 250 ms) でデトレンドし、
// 残差の FFT ピークを 3〜9 Hz で探索 → rate。深さは検出した rate の正弦波を
// 区間全体へ最小二乗フィット (基底 {1, t, cos, sin}) して求める [cent 片振幅]。
VibratoResult analyzeVibrato(const std::vector<F0Frame> &track,
                             double hopSeconds,
                             const VocalAnalyzerConfig &cfg) {
    VibratoResult vr;

    // 最長の連続有声区間を探す
    std::size_t bestStart = 0, bestLen = 0, curStart = 0, curLen = 0;
    for (std::size_t i = 0; i <= track.size(); ++i) {
        if (i < track.size() && track[i].voiced) {
            if (curLen == 0) curStart = i;
            ++curLen;
        } else {
            if (curLen > bestLen) {
                bestStart = curStart;
                bestLen = curLen;
            }
            curLen = 0;
        }
    }
    vr.segmentStartFrame = bestStart;
    vr.segmentFrameCount = bestLen;

    const double segSeconds = static_cast<double>(bestLen) * hopSeconds;
    if (segSeconds < cfg.vibratoMinSegmentSeconds) {
        vr.warning = "有声区間が短くビブラートを評価できません";
        vr.rateHz = makeInvalidMetric(vr.warning);
        vr.depthCents = makeInvalidMetric(vr.warning);
        return vr;
    }

    // F0 軌跡 [cent] (区間中央値基準)
    std::vector<double> f0s(bestLen);
    for (std::size_t i = 0; i < bestLen; ++i)
        f0s[i] = track[bestStart + i].f0Hz;
    const double ref = medianOf(f0s);
    std::vector<double> cents(bestLen);
    for (std::size_t i = 0; i < bestLen; ++i)
        cents[i] = 1200.0 * std::log2(f0s[i] / ref);

    // 移動平均デトレンド (中心対称、奇数長)。端は切り捨てる。
    std::size_t maLen = static_cast<std::size_t>(
        cfg.vibratoDetrendSeconds / hopSeconds + 0.5);
    if (maLen < 3) maLen = 3;
    if (maLen % 2 == 0) ++maLen;
    const std::size_t halfMa = (maLen - 1) / 2;
    if (bestLen < maLen + 32) {
        vr.warning = "デトレンド後の区間が短くビブラートを評価できません";
        vr.rateHz = makeInvalidMetric(vr.warning);
        vr.depthCents = makeInvalidMetric(vr.warning);
        return vr;
    }
    const std::size_t m = bestLen - 2 * halfMa;
    std::vector<double> residual(m);
    for (std::size_t i = 0; i < m; ++i) {
        double mean = 0.0;
        for (std::size_t j = 0; j < maLen; ++j) mean += cents[i + j];
        mean /= static_cast<double>(maLen);
        residual[i] = cents[i + halfMa] - mean;
    }

    // 残差の FFT (Hann 窓 + ゼロ詰め) → 3〜9 Hz のピーク探索
    const double frameRate = 1.0 / hopSeconds;
    std::size_t nf = nextPowerOfTwo(8 * m);
    if (nf < 256) nf = 256;
    std::vector<std::complex<double>> buf(nf, std::complex<double>(0.0, 0.0));
    for (std::size_t i = 0; i < m; ++i) {
        const double w =
            0.5 * (1.0 - std::cos(2.0 * kPi * static_cast<double>(i) /
                                  static_cast<double>(m - 1)));
        buf[i] = std::complex<double>(residual[i] * w, 0.0);
    }
    fftForward(buf);
    const double df = frameRate / static_cast<double>(nf);
    const std::size_t half = nf / 2;
    std::size_t kLo = static_cast<std::size_t>(
        std::ceil(cfg.vibratoMinHz / df));
    std::size_t kHi = static_cast<std::size_t>(
        std::floor(cfg.vibratoMaxHz / df));
    if (kLo < 1) kLo = 1;
    if (kHi > half) kHi = half;
    if (kLo >= kHi) {
        vr.warning = "ビブラート探索帯域の分解能が不足しています";
        vr.rateHz = makeInvalidMetric(vr.warning);
        vr.depthCents = makeInvalidMetric(vr.warning);
        return vr;
    }
    std::size_t kPeak = kLo;
    for (std::size_t k = kLo + 1; k <= kHi; ++k) {
        if (std::norm(buf[k]) > std::norm(buf[kPeak])) kPeak = k;
    }

    // 帯域 SNR: ピークパワー vs 2〜12 Hz の中央値パワー
    std::vector<double> refBand;
    for (std::size_t k = 1; k <= half; ++k) {
        const double f = static_cast<double>(k) * df;
        if (f >= 2.0 && f <= 12.0) refBand.push_back(std::norm(buf[k]));
    }
    const double medPower = medianOf(refBand);
    const double peakPower = std::norm(buf[kPeak]);
    if (peakPower < cfg.vibratoMinPeakRatio * (medPower + kTinyPower)) {
        vr.warning = "変調スペクトルのピークが不明瞭です (帯域 SNR 不足)";
        vr.rateHz = makeInvalidMetric(vr.warning);
        vr.depthCents = makeInvalidMetric(vr.warning);
        return vr;
    }

    // ピーク位置の放物線補間 (対数パワー)
    double kF = static_cast<double>(kPeak);
    if (kPeak > 1 && kPeak < half) {
        const double y1 = std::log10(std::norm(buf[kPeak - 1]) + kTinyPower);
        const double y2 = std::log10(peakPower + kTinyPower);
        const double y3 = std::log10(std::norm(buf[kPeak + 1]) + kTinyPower);
        kF += parabolicOffset(y1, y2, y3);
    }
    const double rateHz = kF * df;

    // 深さ: 検出した rate の正弦波を区間全体の cent 軌跡へ最小二乗フィット。
    // 基底 {1, t, cos(2πft), sin(2πft)} (定数 + 線形トレンドを吸収する)。
    double g[4][4] = {{0.0}};
    double rhs[4] = {0.0, 0.0, 0.0, 0.0};
    const double tMid = 0.5 * static_cast<double>(bestLen - 1) * hopSeconds;
    for (std::size_t i = 0; i < bestLen; ++i) {
        const double t = static_cast<double>(i) * hopSeconds - tMid;
        const double phi[4] = {1.0, t, std::cos(2.0 * kPi * rateHz * t),
                               std::sin(2.0 * kPi * rateHz * t)};
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) g[r][c] += phi[r] * phi[c];
            rhs[r] += phi[r] * cents[i];
        }
    }
    double coef[4];
    if (!solve4x4(g, rhs, coef)) {
        vr.warning = "ビブラート深さの回帰に失敗しました";
        vr.rateHz = makeInvalidMetric(vr.warning);
        vr.depthCents = makeInvalidMetric(vr.warning);
        return vr;
    }
    const double depthCents = std::sqrt(coef[2] * coef[2] + coef[3] * coef[3]);
    if (depthCents < cfg.vibratoMinDepthCents) {
        char msg[128];
        std::snprintf(msg, sizeof(msg),
                      "ビブラート深さが %.1f cent 未満のため無効 (%.2f cent)",
                      cfg.vibratoMinDepthCents, depthCents);
        vr.warning = msg;
        vr.rateHz = makeInvalidMetric(vr.warning);
        vr.depthCents = makeInvalidMetric(vr.warning);
        return vr;
    }

    vr.rateHz = makeValidMetric(rateHz);
    vr.depthCents = makeValidMetric(depthCents);
    vr.valid = true;
    return vr;
}

} // namespace

F0SearchRange f0SearchRangeFor(VoiceType type) {
    switch (type) {
    case VoiceType::Soprano:      return F0SearchRange(220.0, 1400.0);
    case VoiceType::MezzoSoprano: return F0SearchRange(180.0, 1000.0);
    case VoiceType::Contralto:    return F0SearchRange(150.0, 800.0);
    case VoiceType::Tenor:        return F0SearchRange(120.0, 700.0);
    case VoiceType::Baritone:     return F0SearchRange(90.0, 500.0);
    case VoiceType::Bass:         return F0SearchRange(70.0, 400.0);
    case VoiceType::Unknown:      return F0SearchRange(60.0, 1500.0);
    }
    return F0SearchRange(60.0, 1500.0);
}

VocalAnalyzer::VocalAnalyzer(const VocalAnalyzerConfig &config)
    : m_config(config) {}

AcousticResult<VocalAnalysisResult>
VocalAnalyzer::analyze(ArrayView<const double> x, double sampleRateHz) const {
    typedef AcousticResult<VocalAnalysisResult> Result;

    if (x.empty())
        return Result::error(AcousticErrorCode::EmptyInput, "input is empty");
    if (!(sampleRateHz > 0.0))
        return Result::error(AcousticErrorCode::UnsupportedSampleRate,
                             "sample rate must be positive");
    for (std::size_t i = 0; i < x.size(); ++i) {
        if (!std::isfinite(x[i]))
            return Result::error(AcousticErrorCode::NonFiniteSample,
                                 "input contains NaN/Inf");
    }

    // ── F0 探索範囲 (声種プリセット。用途はこれのみ) ──
    F0SearchRange range = f0SearchRangeFor(m_config.voiceType);
    if (m_config.f0MinHz > 0.0) range.minHz = m_config.f0MinHz;
    if (m_config.f0MaxHz > 0.0) range.maxHz = m_config.f0MaxHz;
    if (!(range.minHz > 0.0) || range.minHz >= range.maxHz)
        return Result::error(AcousticErrorCode::InvalidArgument,
                             "invalid F0 search range");
    if (!(m_config.yinThreshold > 0.0) || !(m_config.hopSeconds > 0.0))
        return Result::error(AcousticErrorCode::InvalidArgument,
                             "invalid YIN threshold or hop");

    const double fs = sampleRateHz;
    YinParams yp;
    yp.tauMax = static_cast<std::size_t>(fs / range.minHz);
    yp.tauMin = static_cast<std::size_t>(fs / range.maxHz);
    if (yp.tauMin < 2) yp.tauMin = 2;
    if (yp.tauMax <= yp.tauMin)
        return Result::error(AcousticErrorCode::InvalidArgument,
                             "F0 search range too narrow for sample rate");

    // フレーム長: 既定 40 ms、かつ最低 F0 の 2 周期以上
    double frameSec = m_config.frameSeconds;
    if (frameSec <= 0.0) {
        frameSec = 0.040;
        const double twoPeriods = 2.0 / range.minHz;
        if (twoPeriods > frameSec) frameSec = twoPeriods;
    }
    yp.frameLength = static_cast<std::size_t>(frameSec * fs + 0.5);
    if (yp.frameLength < 2 * yp.tauMax) yp.frameLength = 2 * yp.tauMax;
    yp.hopLength = static_cast<std::size_t>(m_config.hopSeconds * fs + 0.5);
    if (yp.hopLength < 1) yp.hopLength = 1;
    yp.threshold = m_config.yinThreshold;
    yp.noiseGateDbfs = m_config.noiseGateDbfs;

    const std::size_t frameSpan = yp.frameLength + yp.tauMax;
    if (x.size() < frameSpan) {
        char msg[160];
        std::snprintf(msg, sizeof(msg),
                      "input too short: %zu samples (< %zu = frame + max lag)",
                      x.size(), frameSpan);
        return Result::error(AcousticErrorCode::InputTooShort, msg);
    }

    VocalAnalysisResult res;
    res.frameSeconds = static_cast<double>(yp.frameLength) / fs;
    res.hopSeconds = static_cast<double>(yp.hopLength) / fs;
    res.f0SearchMinHz = range.minHz;
    res.f0SearchMaxHz = range.maxHz;

    // ── YIN による F0 軌跡と HNR ──
    double hnrSumDb = 0.0;
    std::size_t hnrCount = 0;
    for (std::size_t start = 0; start + frameSpan <= x.size();
         start += yp.hopLength) {
        F0Frame f = yinFrame(x.data() + start, yp, fs);
        f.timeSeconds =
            (static_cast<double>(start) + 0.5 * static_cast<double>(frameSpan)) /
            fs;
        if (f.voiced) {
            double hnr = 0.0;
            if (hnrForFrame(x.data() + start, x.size() - start, yp.frameLength,
                            fs, f.f0Hz, hnr)) {
                hnrSumDb += hnr;
                ++hnrCount;
            }
        }
        res.f0Track.push_back(f);
    }
    res.totalFrameCount = res.f0Track.size();

    // ── F0 統計 ──
    std::vector<double> voicedF0;
    for (std::size_t i = 0; i < res.f0Track.size(); ++i) {
        if (res.f0Track[i].voiced) voicedF0.push_back(res.f0Track[i].f0Hz);
    }
    res.voicedFrameCount = voicedF0.size();
    res.voicedRatio = (res.totalFrameCount > 0)
                          ? static_cast<double>(res.voicedFrameCount) /
                                static_cast<double>(res.totalFrameCount)
                          : 0.0;

    const bool lowVoicedRatio = res.voicedRatio < 0.5;
    if (voicedF0.empty()) {
        const std::string why = "有声フレームが検出されませんでした";
        res.f0MedianHz = makeInvalidMetric(why);
        res.f0MeanHz = makeInvalidMetric(why);
        res.f0MinHz = makeInvalidMetric(why);
        res.f0MaxHz = makeInvalidMetric(why);
        res.pitchStabilityCents = makeInvalidMetric(why);
        res.hnrDb = makeInvalidMetric(why);
        res.warnings.push_back(why);
    } else {
        const double med = medianOf(voicedF0);
        double sum = 0.0, lo = voicedF0[0], hi = voicedF0[0];
        double devSq = 0.0;
        for (std::size_t i = 0; i < voicedF0.size(); ++i) {
            sum += voicedF0[i];
            if (voicedF0[i] < lo) lo = voicedF0[i];
            if (voicedF0[i] > hi) hi = voicedF0[i];
            const double c = 1200.0 * std::log2(voicedF0[i] / med);
            devSq += c * c;
        }
        const double stability =
            std::sqrt(devSq / static_cast<double>(voicedF0.size()));
        const std::string why = "有声フレーム率が 50% 未満です";
        res.f0MedianHz = lowVoicedRatio ? makeWarningMetric(med, why)
                                        : makeValidMetric(med);
        res.f0MeanHz = lowVoicedRatio
                           ? makeWarningMetric(
                                 sum / static_cast<double>(voicedF0.size()), why)
                           : makeValidMetric(
                                 sum / static_cast<double>(voicedF0.size()));
        res.f0MinHz = lowVoicedRatio ? makeWarningMetric(lo, why)
                                     : makeValidMetric(lo);
        res.f0MaxHz = lowVoicedRatio ? makeWarningMetric(hi, why)
                                     : makeValidMetric(hi);
        res.pitchStabilityCents = lowVoicedRatio
                                      ? makeWarningMetric(stability, why)
                                      : makeValidMetric(stability);
        if (lowVoicedRatio) res.warnings.push_back(why);

        if (hnrCount > 0) {
            const double hnr = hnrSumDb / static_cast<double>(hnrCount);
            res.hnrDb = lowVoicedRatio ? makeWarningMetric(hnr, why)
                                       : makeValidMetric(hnr);
        } else {
            res.hnrDb = makeInvalidMetric("HNR を評価できるフレームがありません");
        }
    }

    // ── ビブラート ──
    res.vibrato = analyzeVibrato(res.f0Track, res.hopSeconds, m_config);
    if (!res.vibrato.valid && !res.vibrato.warning.empty())
        res.warnings.push_back("ビブラート: " + res.vibrato.warning);

    // ── LTAS (Welch 法) と派生指標 ──
    std::size_t fftLen = m_config.ltasFftLength;
    if (!isPowerOfTwo(fftLen)) fftLen = nextPowerOfTwo(fftLen);
    if (fftLen < 64) fftLen = 64;
    std::vector<double> ltasPower = computeWelchLtas(x, fs, fftLen, res.ltas);
    if (!res.ltas.warning.empty()) res.warnings.push_back(res.ltas.warning);
    const std::vector<double> &freqs = res.ltas.frequenciesHz;

    // スペクトル重心 (パワー重み)
    {
        double num = 0.0, den = 0.0;
        for (std::size_t k = 0; k < ltasPower.size(); ++k) {
            num += freqs[k] * ltasPower[k];
            den += ltasPower[k];
        }
        res.spectralCentroidHz =
            (den > 0.0) ? makeValidMetric(num / den)
                        : makeInvalidMetric("スペクトルパワーがゼロです");
    }

    // 倍音レベル H1..H8 (F0 中央値の ±3% 近傍の LTAS ピーク、H1 相対 dB)
    res.harmonicLevelsDb.assign(8, MetricValue());
    if (res.f0MedianHz.valid) {
        const double f0 = res.f0MedianHz.value;
        const double df = fs / static_cast<double>(fftLen);
        double h1Db = 0.0;
        bool h1Ok = false;
        std::vector<double> rawDb(8, -300.0);
        std::vector<bool> ok(8, false);
        for (int h = 1; h <= 8; ++h) {
            const double fh = f0 * static_cast<double>(h);
            if (fh >= 0.5 * fs) break; // ナイキスト超過
            std::size_t kLo = static_cast<std::size_t>(0.97 * fh / df);
            std::size_t kHi =
                static_cast<std::size_t>(std::ceil(1.03 * fh / df));
            const std::size_t kc = static_cast<std::size_t>(fh / df + 0.5);
            if (kLo > kc - 1 && kc >= 1) kLo = kc - 1; // 最低 ±1 ビン
            if (kHi < kc + 1) kHi = kc + 1;
            if (kHi >= ltasPower.size()) kHi = ltasPower.size() - 1;
            if (kLo > kHi) continue;
            double best = res.ltas.levelsDb[kLo];
            for (std::size_t k = kLo + 1; k <= kHi; ++k) {
                if (res.ltas.levelsDb[k] > best) best = res.ltas.levelsDb[k];
            }
            rawDb[h - 1] = best;
            ok[h - 1] = true;
            if (h == 1) {
                h1Db = best;
                h1Ok = true;
            }
        }
        for (int h = 1; h <= 8; ++h) {
            if (ok[h - 1] && h1Ok) {
                res.harmonicLevelsDb[h - 1] =
                    makeValidMetric(rawDb[h - 1] - h1Db);
            } else {
                res.harmonicLevelsDb[h - 1] = makeInvalidMetric(
                    h1Ok ? "ナイキスト周波数を超えるため評価不能"
                         : "H1 を評価できません");
            }
        }
    } else {
        for (int h = 0; h < 8; ++h) {
            res.harmonicLevelsDb[h] =
                makeInvalidMetric("F0 が無効のため倍音を評価できません");
        }
    }

    // 帯域エネルギー (LTAS パワー積算): 全帯域 / 0-2k / 歌手フォルマント帯域
    {
        std::vector<Band> bands;
        bands.push_back(Band("full", 0.0, 0.0, 0.0, true));
        bands.push_back(Band("0-2k", 1000.0, 0.0, 2000.0));
        const std::vector<Band> sf = makeBands(BandSet::SingerFormant);
        for (std::size_t i = 0; i < sf.size(); ++i) bands.push_back(sf[i]);
        for (std::size_t i = 0; i < bands.size(); ++i) {
            BandEnergyValue bev;
            bev.band = bands[i];
            const double p = bandPowerSum(ltasPower, freqs, bands[i]);
            bev.levelDb = (p > 0.0)
                              ? makeValidMetric(toDbPower(p))
                              : makeInvalidMetric("帯域パワーがゼロです");
            res.bandEnergies.push_back(bev);
        }
    }

    // 歌手フォルマント指標: 2-4 kHz / 0-2 kHz の LTAS エネルギー比 [dB]
    // (Sundberg 1974 の singer's formant 概念に基づく比率。物理量のみを返し、
    //  発声の巧拙等の評価は行わない)
    {
        if (0.5 * fs < 4000.0) {
            res.singerFormantRatioDb = makeInvalidMetric(
                "サンプリング周波数が低く 2-4 kHz 帯域を評価できません");
        } else {
            const Band lowBand("0-2k", 1000.0, 0.0, 2000.0);
            const Band highBand("2-4k", 2828.0, 2000.0, 4000.0);
            const double pLow = bandPowerSum(ltasPower, freqs, lowBand);
            const double pHigh = bandPowerSum(ltasPower, freqs, highBand);
            res.singerFormantRatioDb =
                (pLow > 0.0 && pHigh > 0.0)
                    ? makeValidMetric(10.0 * std::log10(pHigh / pLow))
                    : makeInvalidMetric("帯域パワーがゼロです");
        }
    }

    // ── レベル (dBFS) と Leq。SPL は Absolute 校正時のみ valid ──
    {
        double peak = 0.0, energy = 0.0;
        for (std::size_t i = 0; i < x.size(); ++i) {
            const double a = std::fabs(x[i]);
            if (a > peak) peak = a;
            energy += x[i] * x[i];
        }
        const double meanSq = energy / static_cast<double>(x.size());
        res.peakDbfs = makeValidMetric(toDbAmp(peak));
        res.rmsDbfs = makeValidMetric(toDbAmp(std::sqrt(meanSq)));
        // Leq (全長の等価レベル)。未校正では dBFS 基準の相対値。
        res.leqDbfs = makeValidMetric(toDbPower(meanSq + kTinyPower));
        if (m_config.calibration == CalibrationState::Absolute) {
            res.leqSplDb = makeValidMetric(res.leqDbfs.value +
                                           m_config.calibrationOffsetDb);
            res.peakSplDb = makeValidMetric(res.peakDbfs.value +
                                            m_config.calibrationOffsetDb);
        } else {
            const std::string why =
                "Absolute 校正がないため SPL 絶対値は評価できません";
            res.leqSplDb = makeInvalidMetric(why);
            res.peakSplDb = makeInvalidMetric(why);
        }
    }

    res.overallQuality = res.warnings.empty() ? AnalysisQuality::Valid
                                              : AnalysisQuality::Warning;
    return Result::ok(std::move(res));
}

AcousticResult<VocalAnalysisResult>
VocalAnalyzer::analyze(const AudioBuffer &buffer, std::size_t channel) const {
    typedef AcousticResult<VocalAnalysisResult> Result;
    if (channel >= buffer.channelCount())
        return Result::error(AcousticErrorCode::InvalidArgument,
                             "channel index out of range");
    const std::vector<double> &ch = buffer.channels[channel];
    return analyze(ArrayView<const double>(ch.data(), ch.size()),
                   buffer.sampleRateHz);
}

} // namespace acoustics
} // namespace ofd
