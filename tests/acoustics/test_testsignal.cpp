// test_testsignal.cpp — 帯域制限クリック (可聴化のドライ音源) の検証。
//
// 期待値は窓関数法 (windowed-sinc) の教科書的性質から独立に決め、
// テスト側で FFT を掛けて実測する:
//   - 線形位相 = 中心について対称 (畳み込んでも波形が歪まない根拠)
//   - ピークは中央サンプルで、値はちょうど指定振幅
//   - 直流成分が無い (fl > 0)
//   - 通過域が平坦 (Hann 窓のリップルは ±0.1 dB 級)
//   - 阻止域が −40 dB 以下 (Hann 窓の最大サイドローブ −31 dB + ロールオフ)
//   - 遷移帯域幅が Δf ≈ 3.1·fs/N に従う (窓長を倍にすると半分になる)
//   - 畳み込みの単位元性: クリック ⊛ 帯域内の正弦波 = 振幅がそのまま
#include <cmath>
#include <complex>
#include <cstdio>
#include <vector>

#include "../../src/acoustics/core/Fft.h"
#include "../../src/acoustics/core/TestSignal.h"
#include "test_common.h"

using namespace ofd::acoustics;

namespace {

const double kPi = 3.14159265358979323846;

ClickSpec makeSpec() {
    ClickSpec s;
    s.sampleRateHz = 48000.0;
    s.lowHz = 100.0;
    s.highHz = 8000.0;
    s.durationSec = 0.02;      // 960 → 961 タップ
    s.amplitude = 0.5;
    return s;
}

// 振幅スペクトル [dB] を返す (長さ N/2+1、周波数分解能 fs/N)
std::vector<double> magnitudeDb(const std::vector<double> &x, std::size_t nfft) {
    const AcousticResult<std::vector<std::complex<double> > > sp =
        realFft(ArrayView<const double>(x.data(), x.size()), nfft);
    std::vector<double> db;
    if (!sp.success()) return db;
    const std::vector<std::complex<double> > &X = sp.value();
    db.resize(X.size() / 2 + 1);
    for (std::size_t k = 0; k < db.size(); ++k) {
        const double m = std::abs(X[k]);
        db[k] = (m > 0.0) ? 20.0 * std::log10(m) : -400.0;
    }
    return db;
}

// ── 仕様の検証 ────────────────────────────────────────────────────────────
void testSpecValidation() {
    std::printf("-- click specification validation\n");
    CHECK(ClickSpec().valid());              // 既定値は有効
    CHECK(makeSpec().valid());

    ClickSpec s = makeSpec();
    s.highHz = s.lowHz;              CHECK(!s.valid());
    s = makeSpec(); s.highHz = 30000.0;   CHECK(!s.valid());  // ナイキスト超
    s = makeSpec(); s.sampleRateHz = 0.0; CHECK(!s.valid());
    s = makeSpec(); s.durationSec = 0.0;  CHECK(!s.valid());
    s = makeSpec(); s.amplitude = 0.0;    CHECK(!s.valid());
    s = makeSpec(); s.amplitude = 1.5;    CHECK(!s.valid());  // フルスケール超
    s = makeSpec(); s.durationSec = 1e-4; CHECK(!s.valid());  // 4 タップ

    // 不正な仕様は空のバッファ (無音を「生成できた」と装わない)
    s = makeSpec(); s.highHz = s.lowHz;
    CHECK(generateClick(s).channels.empty());

    // タップ数は必ず奇数 (中心サンプルを持つ)
    s = makeSpec();
    CHECK(s.tapCount() % 2 == 1);
    CHECK(s.tapCount() == 961);          // 0.02 s × 48 kHz = 960 → 961
    s.durationSec = 0.04;
    CHECK(s.tapCount() == 1921);
}

// ── 時間波形の性質 ────────────────────────────────────────────────────────
void testWaveform() {
    std::printf("-- click waveform (linear phase, peak, DC)\n");
    const ClickSpec spec = makeSpec();
    const AudioBuffer buf = generateClick(spec);
    CHECK(buf.channelCount() == 1);
    CHECK(buf.sampleCount() == spec.tapCount());
    CHECK(std::fabs(buf.sampleRateHz - spec.sampleRateHz) < 1e-9);

    const std::vector<double> &h = buf.channels[0];
    const std::size_t n = h.size();
    const std::size_t c = (n - 1) / 2;

    // 1) ピークは中央サンプルで、値はちょうど指定振幅
    double peak = 0.0;
    std::size_t peakAt = 0;
    for (std::size_t i = 0; i < n; ++i) {
        if (std::fabs(h[i]) > peak) { peak = std::fabs(h[i]); peakAt = i; }
    }
    CHECK(peakAt == c);
    CHECK_NEAR(h[c], spec.amplitude, 1e-15);

    // 2) 線形位相 = 中心について対称
    double asym = 0.0;
    for (std::size_t k = 1; k <= c; ++k)
        asym = std::max(asym, std::fabs(h[c + k] - h[c - k]));
    CHECK(asym < 1e-15 * spec.amplitude + 1e-18);

    // 3) 両端は Hann 窓により厳密に 0
    CHECK(h[0] == 0.0);
    CHECK(h[n - 1] == 0.0);

    // 4) 直流成分が厳密に 0 (窓形状での直流除去) — 遮断 fl が遷移帯域より
    //    狭い場合 (既定の 20 Hz) でも成り立つことが眼目
    for (int t = 0; t < 2; ++t) {
        ClickSpec s = spec;
        if (t == 1) s.lowHz = 20.0;      // 遷移帯域 (≈155 Hz) より狭い遮断
        const AudioBuffer b = generateClick(s);
        CHECK(!b.channels.empty());
        if (b.channels.empty()) continue;
        double sum = 0.0, absSum = 0.0;
        for (std::size_t i = 0; i < b.channels[0].size(); ++i) {
            sum += b.channels[0][i];
            absSum += std::fabs(b.channels[0][i]);
        }
        CHECK(absSum > 0.0);
        CHECK(std::fabs(sum) / absSum < 1e-12);
    }
}

// ── 振幅スペクトル (通過域の平坦さ・阻止域の減衰) ─────────────────────────
void testSpectrum() {
    std::printf("-- click spectrum (passband ripple, stopband rejection)\n");
    const ClickSpec spec = makeSpec();
    const AudioBuffer buf = generateClick(spec);
    CHECK(!buf.channels.empty());
    if (buf.channels.empty()) return;

    const std::size_t nfft = 32768;
    const std::vector<double> db = magnitudeDb(buf.channels[0], nfft);
    CHECK(!db.empty());
    if (db.empty()) return;
    const double df = spec.sampleRateHz / static_cast<double>(nfft);
    const double trans = spec.transitionWidthHz();
    CHECK(trans > 100.0 && trans < 200.0);       // 3.1·48000/961 ≈ 155 Hz

    // 通過域の基準 = 帯域中心の値
    const std::size_t kMid =
        static_cast<std::size_t>(0.5 * (spec.lowHz + spec.highHz) / df + 0.5);
    const double ref = db[kMid];

    double ripple = 0.0;
    double stop = -400.0;
    for (std::size_t k = 0; k < db.size(); ++k) {
        const double f = static_cast<double>(k) * df;
        if (f > spec.lowHz + trans && f < spec.highHz - trans)
            ripple = std::max(ripple, std::fabs(db[k] - ref));
        if (f > spec.highHz + trans || (f < spec.lowHz - trans && f > 0.0))
            stop = std::max(stop, db[k] - ref);
    }
    // Hann 窓の窓関数法: 通過域リップル ±0.1 dB 級、阻止域 −40 dB 級
    std::printf("   passband ripple %.3f dB, stopband %.1f dB\n", ripple, stop);
    CHECK(ripple < 0.2);
    CHECK(stop < -40.0);

    // 直流はさらに深く落ちている
    CHECK(db[0] - ref < -60.0);
}

// ── 遷移帯域幅が窓長に反比例する ──────────────────────────────────────────
// 教科書の Δf ≈ 3.1·fs/N を「窓長を倍にすると幅が半分」という形で確かめる
// (係数そのものは窓の定義に依存するが、比例則は依存しない)。
void testTransitionScaling() {
    std::printf("-- transition width scales as 1/N\n");
    const std::size_t nfft = 65536;
    double measured[2] = { 0.0, 0.0 };
    for (int t = 0; t < 2; ++t) {
        ClickSpec spec = makeSpec();
        spec.durationSec = (t == 0) ? 0.02 : 0.04;
        const AudioBuffer buf = generateClick(spec);
        CHECK(!buf.channels.empty());
        if (buf.channels.empty()) return;
        const std::vector<double> db = magnitudeDb(buf.channels[0], nfft);
        const double df = spec.sampleRateHz / static_cast<double>(nfft);
        const std::size_t kMid =
            static_cast<std::size_t>(0.5 * (spec.lowHz + spec.highHz) / df + 0.5);
        const double ref = db[kMid];
        // 上側遮断から −40 dB まで落ちきる周波数までを遷移帯域とみなす
        double f40 = 0.0;
        for (std::size_t k = kMid; k < db.size(); ++k) {
            if (db[k] - ref < -40.0) { f40 = static_cast<double>(k) * df; break; }
        }
        CHECK(f40 > spec.highHz);
        measured[t] = f40 - spec.highHz;
    }
    std::printf("   transition %.1f Hz (N=961) vs %.1f Hz (N=1921)\n",
                measured[0], measured[1]);
    // 倍の窓長で半分 (±15%)
    CHECK_NEAR(measured[1] / measured[0], 0.5, 0.075);
}

// ── 畳み込みの応答: 帯域内の正弦波は振幅を保つ ────────────────────────────
// クリックは帯域内で利得 1 ではない (ピーク振幅で正規化しているため) ので、
// 「帯域内の 2 つの周波数で利得が等しい」ことを直接畳み込みで確かめる。
void testConvolutionGain() {
    std::printf("-- in-band tones keep their relative amplitude\n");
    const ClickSpec spec = makeSpec();
    const AudioBuffer buf = generateClick(spec);
    CHECK(!buf.channels.empty());
    if (buf.channels.empty()) return;
    const std::vector<double> &h = buf.channels[0];

    const double fs = spec.sampleRateHz;
    const std::size_t nx = 4096;
    double gain[2] = { 0.0, 0.0 };
    const double tone[2] = { 500.0, 5000.0 };   // どちらも通過域の内側
    for (int t = 0; t < 2; ++t) {
        std::vector<double> x(nx);
        for (std::size_t i = 0; i < nx; ++i)
            x[i] = std::sin(2.0 * kPi * tone[t] * static_cast<double>(i) / fs);
        // 直接畳み込み (過渡を避けて中央付近のピークだけ測る)
        double peak = 0.0;
        for (std::size_t i = h.size(); i + h.size() < nx; ++i) {
            double y = 0.0;
            for (std::size_t j = 0; j < h.size(); ++j) y += h[j] * x[i - j];
            peak = std::max(peak, std::fabs(y));
        }
        gain[t] = peak;
    }
    std::printf("   gain %.4f (500 Hz) vs %.4f (5 kHz)\n", gain[0], gain[1]);
    CHECK(gain[0] > 0.0);
    CHECK(std::fabs(20.0 * std::log10(gain[1] / gain[0])) < 0.2);
}

} // namespace

int main() {
    testSpecValidation();
    testWaveform();
    testSpectrum();
    testTransitionScaling();
    testConvolutionGain();
    return testutil::summary("test_testsignal");
}
