// test_vocal.cpp — VocalAnalyzer (YIN / ビブラート / HNR / LTAS 重心 / SPL) を
// 48 kHz の合成信号で検証する。
//
//   (a) 440 Hz 定常正弦波 3 s → f0 中央値 440±1 Hz、voicedRatio > 0.9、HNR > 30 dB
//   (b) ビブラート波形 F0 = 440·2^((50/1200)·sin(2π·5.5t)) (位相連続、3 s)
//       → rate 5.5±0.3 Hz、depth 50±8 cent
//   (c) 白色雑音 3 s → 無声 (f0MedianHz.valid==false または voicedRatio < 0.1)
//   (d) 440 Hz + 白色雑音 SNR 10 dB → HNR = 10±3 dB
//   (e) 2 トーン (1000 Hz A=1, 3000 Hz A=0.5) → スペクトル重心 (パワー重み)
//       = (1000·1 + 3000·0.25) / 1.25 = 1400 Hz ±2%
//   (f) VoiceType::Bass / Soprano で F0 探索範囲プリセットが変わる
//   (g) SPL 系は CalibrationState::Absolute + オフセット指定時のみ valid
//   (h) FFT 自己相関版 YIN が定義どおりの素朴な差分関数 (O(W·τmax) の
//       二重ループ) と数値的に一致すること (全フレームで有声判定が一致、
//       F0 の相対差 < 1e-9)
//   (i) F0 フレーム中心時刻 = (start + W/2)/fs (W = frameSeconds·fs)。
//       τmax 分の系統的な遅れが乗らないこと
#include <cmath>
#include <cstdio>
#include <vector>

#include "../../src/acoustics/core/VocalAnalyzer.h"
#include "test_common.h"

using namespace ofd::acoustics;

namespace {

const double kPi = 3.14159265358979323846;
const double kFs = 48000.0;

ArrayView<const double> view(const std::vector<double> &v) {
    return ArrayView<const double>(v.data(), v.size());
}

// 定常正弦波
std::vector<double> makeSine(double freqHz, double amp, double seconds) {
    const std::size_t n = static_cast<std::size_t>(seconds * kFs + 0.5);
    std::vector<double> x(n);
    for (std::size_t i = 0; i < n; ++i) {
        x[i] = amp * std::sin(2.0 * kPi * freqHz * static_cast<double>(i) / kFs);
    }
    return x;
}

// 一様白色雑音 ([-amp, amp])
std::vector<double> makeNoise(double amp, double seconds, unsigned seed) {
    const std::size_t n = static_cast<std::size_t>(seconds * kFs + 0.5);
    std::vector<double> x(n);
    unsigned st = seed;
    for (std::size_t i = 0; i < n; ++i) x[i] = amp * testutil::lcgUniform(st);
    return x;
}

// ビブラート波形: F0(t) = f0·2^((depthCents/1200)·sin(2π·rate·t))。
// 位相連続にするため瞬時位相を積分して生成する (φ += 2π·f(t)·dt)。
std::vector<double> makeVibrato(double f0Hz, double rateHz, double depthCents,
                                double amp, double seconds) {
    const std::size_t n = static_cast<std::size_t>(seconds * kFs + 0.5);
    std::vector<double> x(n);
    double phi = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / kFs;
        const double f =
            f0Hz * std::pow(2.0, (depthCents / 1200.0) *
                                     std::sin(2.0 * kPi * rateHz * t));
        x[i] = amp * std::sin(phi);
        phi += 2.0 * kPi * f / kFs;
    }
    return x;
}

// 倍音 + 微小雑音を含む有声らしい合成信号 (決定論的)
std::vector<double> makeVoiceLike(double f0Hz, double seconds, unsigned seed) {
    const std::size_t n = static_cast<std::size_t>(seconds * kFs + 0.5);
    std::vector<double> x(n);
    unsigned st = seed;
    for (std::size_t i = 0; i < n; ++i) {
        const double ph = 2.0 * kPi * f0Hz * static_cast<double>(i) / kFs;
        x[i] = 0.5 * std::sin(ph) + 0.2 * std::sin(2.0 * ph) +
               0.1 * std::sin(3.0 * ph) + 0.02 * testutil::lcgUniform(st);
    }
    return x;
}

// ── (h) 用: 定義どおりの素朴な YIN 参照実装 ──
// d(τ) = Σ_{j<W} (x[j] − x[j+τ])² を二重ループで直接計算する
// (VocalAnalyzer が FFT 自己相関で置き換えた部分の参照)。
// パラメータは analyze() の結果から復元する。
struct NaiveYinFrame {
    double f0Hz;
    bool voiced;
};

NaiveYinFrame naiveYinFrame(const double *x, std::size_t w, std::size_t tauMin,
                            std::size_t tauMax, double threshold,
                            double noiseGateDbfs) {
    NaiveYinFrame out;
    out.f0Hz = 0.0;
    out.voiced = false;

    double e = 0.0;
    for (std::size_t j = 0; j < w; ++j) e += x[j] * x[j];
    const double rms = std::sqrt(e / static_cast<double>(w));
    const double rmsDbfs = (rms > 0.0) ? 20.0 * std::log10(rms) : -300.0;

    std::vector<double> cmndf(tauMax + 1, 1.0);
    double cumulative = 0.0;
    for (std::size_t tau = 1; tau <= tauMax; ++tau) {
        double d = 0.0;
        for (std::size_t j = 0; j < w; ++j) {
            const double diff = x[j] - x[j + tau];
            d += diff * diff;
        }
        cumulative += d;
        cmndf[tau] = (cumulative > 0.0)
                         ? d * static_cast<double>(tau) / cumulative
                         : 1.0;
    }

    std::size_t best = 0;
    for (std::size_t tau = tauMin; tau <= tauMax; ++tau) {
        if (cmndf[tau] < threshold) {
            while (tau + 1 <= tauMax && cmndf[tau + 1] < cmndf[tau]) ++tau;
            best = tau;
            break;
        }
    }
    if (best == 0) {
        best = tauMin;
        for (std::size_t tau = tauMin + 1; tau <= tauMax; ++tau) {
            if (cmndf[tau] < cmndf[best]) best = tau;
        }
    }
    if (cmndf[best] > threshold || rmsDbfs < noiseGateDbfs) return out;

    double tauF = static_cast<double>(best);
    if (best > 1 && best < tauMax) {
        const double y1 = cmndf[best - 1], y2 = cmndf[best], y3 = cmndf[best + 1];
        const double denom = y1 - 2.0 * y2 + y3;
        if (std::fabs(denom) >= 1e-30) {
            double off = 0.5 * (y1 - y3) / denom;
            if (off > 1.0) off = 1.0;
            if (off < -1.0) off = -1.0;
            tauF += off;
        }
    }
    out.voiced = true;
    out.f0Hz = kFs / tauF;
    return out;
}

// 素朴な参照実装と analyze() の F0 軌跡を突き合わせる
void compareWithNaiveYin(const char *label, const std::vector<double> &x,
                         const VocalAnalyzerConfig &cfg) {
    VocalAnalyzer az(cfg);
    AcousticResult<VocalAnalysisResult> r = az.analyze(view(x), kFs);
    CHECK(r.success());
    if (!r.success()) return;
    const VocalAnalysisResult &res = r.value();

    // analyze() が実際に使った YIN パラメータを復元する
    const std::size_t w =
        static_cast<std::size_t>(res.frameSeconds * kFs + 0.5);
    const std::size_t hop = static_cast<std::size_t>(res.hopSeconds * kFs + 0.5);
    const std::size_t tauMax =
        static_cast<std::size_t>(kFs / res.f0SearchMinHz);
    std::size_t tauMin = static_cast<std::size_t>(kFs / res.f0SearchMaxHz);
    if (tauMin < 2) tauMin = 2;

    CHECK(res.f0Track.size() > 0);
    std::size_t voicedMismatch = 0;
    double maxRelF0 = 0.0;
    for (std::size_t i = 0; i < res.f0Track.size(); ++i) {
        const std::size_t start = i * hop;
        const NaiveYinFrame ref =
            naiveYinFrame(x.data() + start, w, tauMin, tauMax,
                          cfg.yinThreshold, cfg.noiseGateDbfs);
        if (ref.voiced != res.f0Track[i].voiced) ++voicedMismatch;
        if (ref.voiced && res.f0Track[i].voiced && ref.f0Hz > 0.0) {
            const double rel =
                std::fabs(res.f0Track[i].f0Hz - ref.f0Hz) / ref.f0Hz;
            if (rel > maxRelF0) maxRelF0 = rel;
        }
    }
    CHECK(voicedMismatch == 0);
    CHECK(maxRelF0 < 1e-9);
    std::printf("  (h) %s: frames=%zu voicedMismatch=%zu maxRelF0=%.3e\n",
                label, res.f0Track.size(), voicedMismatch, maxRelF0);
}

} // namespace

int main() {
    // ── (a) 440 Hz 定常正弦波 3 s ──
    {
        const std::vector<double> x = makeSine(440.0, 0.5, 3.0);
        VocalAnalyzer az;
        AcousticResult<VocalAnalysisResult> r = az.analyze(view(x), kFs);
        CHECK(r.success());
        const VocalAnalysisResult &res = r.value();
        CHECK(res.f0MedianHz.valid);
        if (res.f0MedianHz.valid) CHECK_NEAR(res.f0MedianHz.value, 440.0, 1.0);
        CHECK(res.voicedRatio > 0.9);
        CHECK(res.hnrDb.valid);
        if (res.hnrDb.valid) CHECK(res.hnrDb.value > 30.0);
        std::printf("  (a) f0Median=%.3f Hz voiced=%.3f HNR=%.1f dB\n",
                    res.f0MedianHz.value, res.voicedRatio, res.hnrDb.value);

        // ── (g) SPL: 未校正では leqSplDb / peakSplDb は invalid ──
        CHECK(res.leqDbfs.valid); // dBFS 系は常に valid
        CHECK(!res.leqSplDb.valid);
        CHECK(!res.peakSplDb.valid);

        // Absolute 校正 + オフセット指定時のみ valid になり、
        // 値は dBFS + オフセットに一致する
        VocalAnalyzerConfig cal;
        cal.calibration = CalibrationState::Absolute;
        cal.calibrationOffsetDb = 94.0;
        VocalAnalyzer azCal(cal);
        AcousticResult<VocalAnalysisResult> rc = azCal.analyze(view(x), kFs);
        CHECK(rc.success());
        const VocalAnalysisResult &resc = rc.value();
        CHECK(resc.leqSplDb.valid);
        CHECK(resc.peakSplDb.valid);
        if (resc.leqSplDb.valid)
            CHECK_NEAR(resc.leqSplDb.value, resc.leqDbfs.value + 94.0, 1e-9);
        if (resc.peakSplDb.valid)
            CHECK_NEAR(resc.peakSplDb.value, resc.peakDbfs.value + 94.0, 1e-9);
    }

    // ── (b) ビブラート: rate 5.5 Hz, depth 50 cent (片振幅), 3 s ──
    {
        const std::vector<double> x = makeVibrato(440.0, 5.5, 50.0, 0.5, 3.0);
        VocalAnalyzer az;
        AcousticResult<VocalAnalysisResult> r = az.analyze(view(x), kFs);
        CHECK(r.success());
        const VibratoResult &vib = r.value().vibrato;
        CHECK(vib.valid);
        CHECK(vib.rateHz.valid);
        CHECK(vib.depthCents.valid);
        if (vib.rateHz.valid) CHECK_NEAR(vib.rateHz.value, 5.5, 0.3);
        if (vib.depthCents.valid) CHECK_NEAR(vib.depthCents.value, 50.0, 8.0);
        std::printf("  (b) vibrato rate=%.3f Hz depth=%.2f cent (%s)\n",
                    vib.rateHz.value, vib.depthCents.value,
                    vib.valid ? "valid" : "invalid");
    }

    // ── (c) 白色雑音 3 s → 無声 ──
    {
        const std::vector<double> x = makeNoise(0.3, 3.0, 20260716u);
        VocalAnalyzer az;
        AcousticResult<VocalAnalysisResult> r = az.analyze(view(x), kFs);
        CHECK(r.success());
        const VocalAnalysisResult &res = r.value();
        CHECK(!res.f0MedianHz.valid || res.voicedRatio < 0.1);
        std::printf("  (c) noise: f0 valid=%d voiced=%.3f\n",
                    res.f0MedianHz.valid ? 1 : 0, res.voicedRatio);
    }

    // ── (d) 440 Hz + 白色雑音 (SNR 10 dB) → HNR ≈ 10 dB ──
    {
        // 正弦波 (振幅 a): パワー a²/2。一様雑音 ([-A, A]): パワー A²/3。
        // SNR 10 dB → A = √(3·(a²/2)/10)
        const double a = 0.5;
        const double sigPower = a * a / 2.0;
        const double noisePower = sigPower / 10.0; // SNR 10 dB
        const double noiseAmp = std::sqrt(3.0 * noisePower);
        std::vector<double> x = makeSine(440.0, a, 3.0);
        const std::vector<double> nz = makeNoise(noiseAmp, 3.0, 42u);
        for (std::size_t i = 0; i < x.size(); ++i) x[i] += nz[i];

        VocalAnalyzer az;
        AcousticResult<VocalAnalysisResult> r = az.analyze(view(x), kFs);
        CHECK(r.success());
        const VocalAnalysisResult &res = r.value();
        CHECK(res.hnrDb.valid);
        if (res.hnrDb.valid) CHECK_NEAR(res.hnrDb.value, 10.0, 3.0);
        std::printf("  (d) HNR=%.2f dB (theory 10, SNR 10 dB)\n",
                    res.hnrDb.value);
    }

    // ── (e) 2 トーン → スペクトル重心 (パワー重み) ──
    {
        // パワー: 1000 Hz → 1²/2, 3000 Hz → 0.5²/2。
        // 重心 = (1000·1 + 3000·0.25) / (1 + 0.25) = 1400 Hz
        const std::size_t n = static_cast<std::size_t>(2.0 * kFs + 0.5);
        std::vector<double> x(n);
        for (std::size_t i = 0; i < n; ++i) {
            const double t = static_cast<double>(i) / kFs;
            x[i] = 1.0 * std::sin(2.0 * kPi * 1000.0 * t) +
                   0.5 * std::sin(2.0 * kPi * 3000.0 * t);
        }
        VocalAnalyzer az;
        AcousticResult<VocalAnalysisResult> r = az.analyze(view(x), kFs);
        CHECK(r.success());
        const MetricValue &sc = r.value().spectralCentroidHz;
        CHECK(sc.valid);
        if (sc.valid) CHECK_REL(sc.value, 1400.0, 0.02);
        std::printf("  (e) centroid=%.2f Hz (theory 1400)\n", sc.value);
    }

    // ── (f) VoiceType による F0 探索範囲プリセット ──
    {
        // プリセット自体の確認 (VocalAnalyzer.h の表と一致)
        const F0SearchRange bass = f0SearchRangeFor(VoiceType::Bass);
        const F0SearchRange sop = f0SearchRangeFor(VoiceType::Soprano);
        CHECK_NEAR(bass.minHz, 70.0, 1e-12);
        CHECK_NEAR(bass.maxHz, 400.0, 1e-12);
        CHECK_NEAR(sop.minHz, 220.0, 1e-12);
        CHECK_NEAR(sop.maxHz, 1400.0, 1e-12);
        CHECK(bass.minHz != sop.minHz);
        CHECK(bass.maxHz != sop.maxHz);

        // 分析結果に実際の探索範囲として反映される
        VocalAnalyzerConfig cb;
        cb.voiceType = VoiceType::Bass;
        VocalAnalyzer azBass(cb);
        const std::vector<double> xb = makeSine(110.0, 0.5, 0.5);
        AcousticResult<VocalAnalysisResult> rb = azBass.analyze(view(xb), kFs);
        CHECK(rb.success());
        CHECK_NEAR(rb.value().f0SearchMinHz, 70.0, 1e-12);
        CHECK_NEAR(rb.value().f0SearchMaxHz, 400.0, 1e-12);

        VocalAnalyzerConfig cs;
        cs.voiceType = VoiceType::Soprano;
        VocalAnalyzer azSop(cs);
        const std::vector<double> xs = makeSine(440.0, 0.5, 0.5);
        AcousticResult<VocalAnalysisResult> rs = azSop.analyze(view(xs), kFs);
        CHECK(rs.success());
        CHECK_NEAR(rs.value().f0SearchMinHz, 220.0, 1e-12);
        CHECK_NEAR(rs.value().f0SearchMaxHz, 1400.0, 1e-12);
    }

    // ── (h) FFT 自己相関版 YIN ≡ 素朴な差分関数 (数値等価性) ──
    {
        // 倍音つき有声信号 (既定プリセット Unknown: 60–1500 Hz)
        compareWithNaiveYin("voice-like 220 Hz",
                            makeVoiceLike(220.0, 0.5, 20260716u),
                            VocalAnalyzerConfig());

        // 純音 (差分関数が周期点で 0 近傍まで落ち桁落ちが出やすい条件)
        compareWithNaiveYin("pure sine 440 Hz", makeSine(440.0, 0.5, 0.5),
                            VocalAnalyzerConfig());

        // 無声 (白色雑音) — 有声判定の一致も確認する
        compareWithNaiveYin("white noise", makeNoise(0.3, 0.5, 4242u),
                            VocalAnalyzerConfig());

        // 別プリセット (Bass: τmax が変わる → FFT 長も変わる)
        VocalAnalyzerConfig cb;
        cb.voiceType = VoiceType::Bass;
        compareWithNaiveYin("bass 110 Hz", makeVoiceLike(110.0, 0.5, 7u), cb);
    }

    // ── (i) F0 フレーム中心時刻 = (start + W/2)/fs ──
    {
        const std::vector<double> x = makeSine(440.0, 0.5, 1.0);
        VocalAnalyzer az;
        AcousticResult<VocalAnalysisResult> r = az.analyze(view(x), kFs);
        CHECK(r.success());
        const VocalAnalysisResult &res = r.value();
        CHECK(res.f0Track.size() >= 2);
        if (res.f0Track.size() >= 2) {
            // 先頭フレームの中心は積分窓 W の中心 (τmax の遅れを含まない)
            CHECK_NEAR(res.f0Track[0].timeSeconds, 0.5 * res.frameSeconds,
                       1e-12);
            // フレーム間隔はホップに一致
            CHECK_NEAR(res.f0Track[1].timeSeconds - res.f0Track[0].timeSeconds,
                       res.hopSeconds, 1e-12);
            // 最終フレーム中心も信号長の内側 (窓中心なので端に寄りすぎない)
            const double last = res.f0Track.back().timeSeconds;
            CHECK(last < static_cast<double>(x.size()) / kFs);
            std::printf("  (i) t[0]=%.9f s (W/2=%.9f) hop=%.9f s\n",
                        res.f0Track[0].timeSeconds, 0.5 * res.frameSeconds,
                        res.hopSeconds);
        }
    }

    return testutil::summary("test_vocal");
}
