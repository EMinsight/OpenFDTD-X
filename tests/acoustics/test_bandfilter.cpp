// test_bandfilter.cpp — BandFilter (4次バターワース帯域通過) の数値精度検証。
//
// 目的 (負債 #7): 従来の帯域フィルタ検証は fs = 48 kHz のみだったため、
// fc/fs が小さい条件 (例 63 Hz @ 96 kHz → fc/fs = 6.6e-4) で双一次変換の
// 係数が悪条件にならないかが未検証だった。本テストは 48 kHz と 96 kHz の
// 両方で同じ帯域 (63 / 125 / 250 Hz オクターブ、100 Hz 1/3 オクターブ) を
// 設計し、**解析解と直接比較**して精度を確認する。
//
// 検証項目:
//   (a) 通過帯域: 中心周波数の正弦波の利得が 1 (設計時に正規化される値)
//   (b) 帯域エッジ: -3.01 dB (1/√2)
//   (c) 阻止帯域: ±1 オクターブ / ±2 オクターブでの減衰が解析解と一致
//   (d) 発散・NaN が出ないこと (係数が悪条件になっていないこと)
//   (e) 96 kHz の RirAnalyzer 統合パイプラインで 63 Hz 帯まで T30 が出ること
#include <cmath>
#include <cstdio>
#include <vector>

#include "../../src/acoustics/core/BandFilter.h"
#include "../../src/acoustics/core/RirAnalyzer.h"
#include "test_common.h"

using namespace ofd::acoustics;

namespace {

const double kPi = 3.14159265358979323846;

// ── 設計された離散フィルタの厳密な期待振幅特性 ──
// BandFilter は (1) プリワーピング W = tan(π f / fs)、(2) 2次バターワース
// 低域原型を低域→帯域通過変換、(3) 双一次変換、(4) 中心で利得 1 に正規化、
// という手順で設計される。双一次変換は W 平面と z 平面を 1 対 1 に写すので、
// 得られる離散フィルタの振幅特性は W 平面上のアナログ特性と厳密に一致する:
//
//   Ω(f) = (W² - W0²) / (B·W),  W0 = √(W1·W2),  B = W2 - W1
//   |H(f)| = 1 / √(1 + Ω⁴)      (2次バターワース低域原型 |H|² = 1/(1+Ω⁴))
//
// したがってこの式との差は「係数計算・IIR 実行の数値誤差そのもの」であり、
// 許容誤差の根拠として使える (fc/fs に依存しない厳密解)。
double analyticGain(double freqHz, double lowHz, double highHz, double fs) {
    const double w = std::tan(kPi * freqHz / fs);
    const double w1 = std::tan(kPi * lowHz / fs);
    const double w2 = std::tan(kPi * highHz / fs);
    const double w0 = std::sqrt(w1 * w2);
    const double bw = w2 - w1;
    const double om = (w * w - w0 * w0) / (bw * w);
    return 1.0 / std::sqrt(1.0 + om * om * om * om);
}

// 正弦波応答の振幅測定。定常部 [startFrac, endFrac) の区間で
// y ≈ a·cos(ωn) + b·sin(ωn) を最小二乗フィットし √(a²+b²) を返す。
// 定常応答は厳密にこの形なので、フィットは (過渡が減衰していれば)
// 周期数に依存せず正確 — ピーク値法のようなサンプリング位相誤差が無い。
struct GainMeasurement {
    double gain;
    bool finite;   // 出力が全て有限
    double maxAbs; // 出力の最大絶対値 (発散検出用)

    GainMeasurement() : gain(0.0), finite(true), maxAbs(0.0) {}
};

GainMeasurement measureGain(const BandFilter &f, double fs, double freqHz,
                            double seconds, bool zeroPhase, double startFrac,
                            double endFrac) {
    GainMeasurement m;
    const std::size_t n = static_cast<std::size_t>(seconds * fs + 0.5);
    const double w = 2.0 * kPi * freqHz / fs;
    std::vector<double> x(n);
    for (std::size_t i = 0; i < n; ++i)
        x[i] = std::sin(w * static_cast<double>(i));
    const std::vector<double> y =
        f.apply(ArrayView<const double>(x.data(), x.size()), zeroPhase);

    for (std::size_t i = 0; i < y.size(); ++i) {
        if (!std::isfinite(y[i])) m.finite = false;
        const double a = std::fabs(y[i]);
        if (a > m.maxAbs) m.maxAbs = a;
    }
    if (!m.finite) return m;

    double scc = 0.0, scs = 0.0, sss = 0.0, byc = 0.0, bys = 0.0;
    const std::size_t i0 = static_cast<std::size_t>(startFrac * n);
    const std::size_t i1 = static_cast<std::size_t>(endFrac * n);
    for (std::size_t i = i0; i < i1; ++i) {
        const double c = std::cos(w * static_cast<double>(i));
        const double s = std::sin(w * static_cast<double>(i));
        scc += c * c;
        scs += c * s;
        sss += s * s;
        byc += y[i] * c;
        bys += y[i] * s;
    }
    const double det = scc * sss - scs * scs;
    const double a = (byc * sss - bys * scs) / det;
    const double b = (bys * scc - byc * scs) / det;
    m.gain = std::sqrt(a * a + b * b);
    return m;
}

double toDb(double g) { return 20.0 * std::log10(g > 0.0 ? g : 1e-300); }

// ── 許容誤差 ──
// 実測 (Linux/gcc x86-64) の解析解との相対誤差は
//   63 Hz @ 96 kHz  : 3.7e-6 (最悪値。fc/fs = 6.6e-4)
//   125 Hz @ 96 kHz : 5.4e-7
//   63 Hz @ 48 kHz  : 3.2e-7
// なので 1e-3 (≈ 0.0087 dB) は最悪ケースに対して約 270 倍の余裕がある。
// 48 kHz の既存検証 (test_reverberation の T30 ±5%) と同様「物理的に
// 意味のある誤差より十分小さい」水準で、かつ悪条件化 (数 % 以上のずれ)
// は確実に捕まえられる値として設定する。
const double kGainRelTol = 1.0e-3;

// 決定的な多重トーン RIR。8 オクターブ中心周波数の正弦波を同一の指数包絡
// exp(-6.91 t / RT) で減衰させて足し合わせる (乱数を使わない)。
// 各帯域は自帯域のトーンを取り出し、隣接帯域からの漏れも同じ時定数で減衰する
// ので、帯域別の Schroeder 減衰は理論値どおり RT になる。
std::vector<double> makeMultiToneRir(double fs, double rt, double seconds) {
    const std::size_t n = static_cast<std::size_t>(seconds * fs + 0.5);
    const std::size_t d0 = static_cast<std::size_t>(0.010 * fs + 0.5);
    const double fc[8] = {63.0,   125.0,  250.0,  500.0,
                          1000.0, 2000.0, 4000.0, 8000.0};
    std::vector<double> h(n, 0.0);
    const double k = 6.91 / (rt * fs);
    for (std::size_t i = d0; i < n; ++i) {
        const double t = static_cast<double>(i - d0);
        double s = 0.0;
        for (int b = 0; b < 8; ++b) {
            // 位相をずらして全トーンが同時にピークにならないようにする
            s += std::sin(2.0 * kPi * fc[b] * t / fs + 0.3 * b);
        }
        h[i] = 0.1 * s * std::exp(-k * t);
    }
    return h;
}

// 1 つの帯域を全項目検証する
void checkBand(double fs, double fc, double lowHz, double highHz,
               const char *what) {
    AcousticResult<BandFilter> fr = BandFilter::design(lowHz, highHz, fs);
    CHECK(fr.success());
    if (!fr.success()) {
        std::printf("  %s fs=%.0f fc=%.1f: DESIGN FAILED (%s)\n", what, fs, fc,
                    fr.message().c_str());
        return;
    }
    const BandFilter &f = fr.value();
    CHECK(f.valid());

    // 過渡が十分減衰する長さ (最低 2 s、最低 60 周期)
    const double seconds = (60.0 / fc > 2.0) ? (60.0 / fc) : 2.0;

    std::printf("  %s fs=%.0f fc=%.4g Hz (fc/fs=%.3e)\n", what, fs, fc,
                fc / fs);

    // (a) 通過帯域: 中心周波数で利得 1
    {
        const GainMeasurement m =
            measureGain(f, fs, fc, seconds, false, 0.5, 1.0);
        CHECK(m.finite);
        CHECK_NEAR(m.gain, 1.0, kGainRelTol);
        std::printf("    fc        : gain=%.9f (%+.4f dB) err=%.2e\n", m.gain,
                    toDb(m.gain), std::fabs(m.gain - 1.0));
    }

    // (b) 帯域エッジ: 1/√2 (-3.01 dB)
    {
        const double edges[2] = {lowHz, highHz};
        for (int e = 0; e < 2; ++e) {
            const GainMeasurement m =
                measureGain(f, fs, edges[e], seconds, false, 0.5, 1.0);
            CHECK(m.finite);
            CHECK_REL(m.gain, 1.0 / std::sqrt(2.0), kGainRelTol);
            std::printf("    edge %-5s: gain=%.9f (%+.4f dB)\n",
                        e == 0 ? "low" : "high", m.gain, toDb(m.gain));
        }
    }

    // (c) 阻止帯域: ±1 / ±2 オクターブ。解析解と一致し、かつ実際に減衰する。
    {
        const double ratios[4] = {0.5, 2.0, 0.25, 4.0};
        // ±1 オクターブ: 理論 -13.27 dB、±2 オクターブ: 理論 -28.99 dB。
        // ナイキストや DC に近づくと理論値からずれるので下限は緩めに置く。
        const double minAttenDb[4] = {13.0, 13.0, 28.5, 28.5};
        for (int k = 0; k < 4; ++k) {
            const double freq = fc * ratios[k];
            if (freq >= 0.45 * fs) continue; // 測定に適さない高域は除外
            const GainMeasurement m =
                measureGain(f, fs, freq, seconds, false, 0.5, 1.0);
            const double ana = analyticGain(freq, lowHz, highHz, fs);
            CHECK(m.finite);
            CHECK_REL(m.gain, ana, kGainRelTol);
            CHECK(-toDb(m.gain) >= minAttenDb[k]);
            std::printf("    x%-5.2f   : gain=%.9f (%+.4f dB) ana=%.9f "
                        "rel=%.2e\n",
                        ratios[k], m.gain, toDb(m.gain), ana,
                        std::fabs(m.gain - ana) / ana);
        }
    }

    // (d) 十分離れた高域 (0.45·fs) が強く減衰する
    {
        const GainMeasurement m =
            measureGain(f, fs, 0.45 * fs, seconds, false, 0.5, 1.0);
        CHECK(m.finite);
        CHECK(-toDb(m.gain) >= 60.0);
        std::printf("    0.45fs    : %+.2f dB\n", toDb(m.gain));
    }

    // (e) ゼロ位相 (前後方向) でも同じ精度で振幅特性が 2 乗になる。
    //     端の過渡を避けるため中央 30% の区間で測る。
    {
        const double freqs[3] = {fc, fc * 0.5, fc * 2.0};
        for (int k = 0; k < 3; ++k) {
            const double seconds2 = (240.0 / fc > 4.0) ? (240.0 / fc) : 4.0;
            const GainMeasurement m =
                measureGain(f, fs, freqs[k], seconds2, true, 0.35, 0.65);
            const double ana = analyticGain(freqs[k], lowHz, highHz, fs);
            CHECK(m.finite);
            CHECK_REL(m.gain, ana * ana, kGainRelTol);
        }
        // 振幅特性が解析解の 2 乗になっていることを確認済み
        std::printf("    zeroPhase : OK (squared magnitude matches analytic)\n");
    }

    // (f) 数値安定性: インパルス応答が有限で末尾が完全に減衰する。
    //     係数が悪条件 (極が単位円上/外) なら末尾が減衰しない。
    {
        const std::size_t n = static_cast<std::size_t>(seconds * fs + 0.5);
        std::vector<double> imp(n, 0.0);
        imp[0] = 1.0;
        const std::vector<double> h =
            f.apply(ArrayView<const double>(imp.data(), imp.size()), false);
        double peak = 0.0, tail = 0.0;
        bool finite = true;
        for (std::size_t i = 0; i < n; ++i) {
            if (!std::isfinite(h[i])) finite = false;
            const double a = std::fabs(h[i]);
            if (a > peak) peak = a;
            if (i >= n / 2 && a > tail) tail = a;
        }
        CHECK(finite);
        CHECK(peak > 0.0);
        CHECK(tail <= 1.0e-9 * peak);
        std::printf("    impulse   : peak=%.3e tail/peak=%.2e\n", peak,
                    peak > 0.0 ? tail / peak : 0.0);
    }

    // (g) DC 入力 (定数 1.0) が漏れない = 零点 z=+1 が効いている
    {
        const std::size_t n = static_cast<std::size_t>(seconds * fs + 0.5);
        const std::vector<double> dc(n, 1.0);
        const std::vector<double> y =
            f.apply(ArrayView<const double>(dc.data(), dc.size()), false);
        double tail = 0.0;
        bool finite = true;
        for (std::size_t i = 0; i < n; ++i) {
            if (!std::isfinite(y[i])) finite = false;
            if (i >= n / 2 && std::fabs(y[i]) > tail) tail = std::fabs(y[i]);
        }
        CHECK(finite);
        CHECK(tail <= 1.0e-9);
        std::printf("    DC leak   : %.3e\n", tail);
    }
}

} // namespace

int main() {
    const double r2 = std::sqrt(2.0);
    const double fcOct[3] = {63.0, 125.0, 250.0};

    // ── 1 オクターブ帯域: 48 kHz (既存検証済みの基準) と 96 kHz (負債 #7) ──
    // 同じ帯域を 2 つの fs で設計し、精度が fs に依存しないことを確認する。
    std::printf("== octave bands @ 48 kHz (reference) ==\n");
    for (int i = 0; i < 3; ++i)
        checkBand(48000.0, fcOct[i], fcOct[i] / r2, fcOct[i] * r2, "oct");

    std::printf("== octave bands @ 96 kHz (debt #7) ==\n");
    for (int i = 0; i < 3; ++i)
        checkBand(96000.0, fcOct[i], fcOct[i] / r2, fcOct[i] * r2, "oct");

    // ── 1/3 オクターブ 100 Hz: 帯域幅が狭い分 fc/fs 比の影響を受けやすい ──
    std::printf("== 1/3-octave 100 Hz (narrowest low band) ==\n");
    {
        const double r3 = std::pow(2.0, 1.0 / 6.0);
        checkBand(48000.0, 100.0, 100.0 / r3, 100.0 * r3, "1/3oct");
        checkBand(96000.0, 100.0, 100.0 / r3, 100.0 * r3, "1/3oct");
    }

    // ── makeBands() が返す帯域定義そのものが 96 kHz で全て設計可能か ──
    std::printf("== makeBands() designability @ 96 kHz ==\n");
    {
        const BandSet sets[4] = {BandSet::Compat6, BandSet::Octave63To8k,
                                 BandSet::ThirdOctave100To5k,
                                 BandSet::SingerFormant};
        for (int s = 0; s < 4; ++s) {
            const std::vector<Band> bands = makeBands(sets[s]);
            CHECK(!bands.empty());
            for (std::size_t b = 0; b < bands.size(); ++b) {
                if (bands[b].fullBand) continue;
                AcousticResult<BandFilter> fr = BandFilter::design(
                    bands[b].lowHz, bands[b].highHz, 96000.0);
                CHECK(fr.success());
                // 48 kHz でも同じ帯域が設計できること (回帰確認)
                AcousticResult<BandFilter> fr48 = BandFilter::design(
                    bands[b].lowHz, bands[b].highHz, 48000.0);
                CHECK(fr48.success());
            }
        }
    }

    // ── 統合パイプライン (決定的): 48 kHz と 96 kHz で同じ結果になること ──
    // 各オクターブ中心周波数の減衰正弦波の和を RIR とする。各帯域は自帯域の
    // トーンを取り出し、隣接帯域からの漏れ (-13.3 dB) も同じ時定数で減衰する
    // ため、Schroeder 減衰は理論どおり厳密な指数になる。
    // → 人工白色雑音 RIR と違い**単一実現の統計ゆらぎが無く**、低域帯でも
    //    厳しい許容値で fs 依存の劣化を検出できる。
    std::printf("== RirAnalyzer Octave63To8k: deterministic multi-tone ==\n");
    {
        const double rt = 1.0;
        double t30At[2][8];
        for (int a = 0; a < 2; ++a)
            for (int b = 0; b < 8; ++b) t30At[a][b] = 0.0;
        for (int k = 0; k < 2; ++k) {
            const double fs = (k == 0) ? 48000.0 : 96000.0;
            const std::vector<double> h = makeMultiToneRir(fs, rt, 1.8);
            RirAnalyzerConfig cfg;
            cfg.bandSet = BandSet::Octave63To8k;
            cfg.zeroPhaseFiltering = true;
            RirAnalyzer analyzer(cfg);
            AcousticResult<RirAnalysisResult> r =
                analyzer.analyze(ArrayView<const double>(h.data(), h.size()),
                                 fs);
            CHECK(r.success());
            if (!r.success()) continue;
            const RirAnalysisResult &res = r.value();
            CHECK(res.directSound.found);
            CHECK(res.bands.size() == 8);
            std::printf("  fs=%.0f\n", fs);
            for (std::size_t b = 0; b < res.bands.size() && b < 8; ++b) {
                const BandMetricsResult &bm = res.bands[b];
                CHECK(bm.filterOk);
                CHECK(bm.filterWarning.empty());
                CHECK(bm.metrics.t30.valid);
                CHECK(bm.metrics.t20.valid);
                CHECK(bm.metrics.edt.valid);
                t30At[k][b] = bm.metrics.t30.value;
                // 実測誤差は全帯域・両 fs で 0.037% 以下 (63 Hz 帯を含む)。
                // ±0.5% は約 13 倍の余裕で、係数悪条件による数 % の劣化は
                // 確実に検出できる。
                CHECK_REL(bm.metrics.t30.value, rt, 0.005);
                CHECK_REL(bm.metrics.t20.value, rt, 0.005);
                CHECK_REL(bm.metrics.edt.value, rt, 0.005);
                std::printf("    band %-5s: T30=%.5f (%+.3f%%) T20=%.5f "
                            "EDT=%.5f\n",
                            bm.band.label.c_str(), bm.metrics.t30.value,
                            100.0 * (bm.metrics.t30.value - rt) / rt,
                            bm.metrics.t20.value, bm.metrics.edt.value);
            }
        }
        // 48 kHz と 96 kHz で結果が一致する (= 高 fs での精度低下が無い)
        for (int b = 0; b < 8; ++b) CHECK_REL(t30At[1][b], t30At[0][b], 0.002);
    }

    // ── 統合パイプライン (人工白色雑音 RIR) @ 96 kHz ──
    // こちらは実運用に近い入力。ただし帯域分割後の T30 は**単一実現の統計
    // ゆらぎ**が支配的で、63 Hz 帯では 8 シードの実測で 48 kHz が最大 11.8%、
    // 96 kHz が最大 11.2% ばらつく (fs に依存しない = フィルタ精度の問題では
    // ない)。よってここでは「フィルタが設計でき、有限で妥当な値が出る」ことを
    // 確認し、精度そのものは上の決定的テストと周波数特性の直接比較で担保する。
    std::printf("== RirAnalyzer Octave63To8k @ 96 kHz (synthetic noise) ==\n");
    {
        testutil::SyntheticRirSpec spec;
        spec.rt60 = 1.0;
        spec.sampleRateHz = 96000.0;
        spec.seed = 96001u;
        const std::vector<double> h = testutil::makeSyntheticRir(spec);

        RirAnalyzerConfig cfg;
        cfg.bandSet = BandSet::Octave63To8k;
        cfg.zeroPhaseFiltering = true;
        RirAnalyzer analyzer(cfg);
        AcousticResult<RirAnalysisResult> r = analyzer.analyze(
            ArrayView<const double>(h.data(), h.size()), spec.sampleRateHz);
        CHECK(r.success());
        if (r.success()) {
            const RirAnalysisResult &res = r.value();
            CHECK(res.bands.size() == 8);
            for (std::size_t b = 0; b < res.bands.size(); ++b) {
                const BandMetricsResult &bm = res.bands[b];
                CHECK(bm.filterOk);
                CHECK(bm.filterWarning.empty());
                CHECK(bm.metrics.t30.valid);
                CHECK(std::isfinite(bm.metrics.t30.value));
                CHECK(bm.metrics.t30.value > 0.0);
                // 統計ゆらぎの実測範囲 (±11.8%) を包含する上限。
                CHECK_REL(bm.metrics.t30.value, spec.rt60, 0.15);
                std::printf("    band %-5s: filterOk=%d T30=%.4f (%+.2f%%)\n",
                            bm.band.label.c_str(), bm.filterOk ? 1 : 0,
                            bm.metrics.t30.value,
                            100.0 * (bm.metrics.t30.value - spec.rt60) /
                                spec.rt60);
            }
        }
    }

    // ── エラー系: 設計不能な条件 ──
    std::printf("== design error cases ==\n");
    {
        // 上側エッジがナイキスト以上
        AcousticResult<BandFilter> r =
            BandFilter::design(6000.0, 12000.0, 24000.0);
        CHECK(!r.success());
        CHECK(r.errorCode() == AcousticErrorCode::FilterDesignFailed);
    }
    {
        // fs <= 0
        AcousticResult<BandFilter> r = BandFilter::design(100.0, 200.0, 0.0);
        CHECK(!r.success());
        CHECK(r.errorCode() == AcousticErrorCode::UnsupportedSampleRate);
    }
    {
        // low >= high
        AcousticResult<BandFilter> r =
            BandFilter::design(200.0, 100.0, 96000.0);
        CHECK(!r.success());
        CHECK(r.errorCode() == AcousticErrorCode::InvalidArgument);
    }
    {
        // 96 kHz でも 8 kHz オクターブ帯 (上端 11.3 kHz) は設計可能
        AcousticResult<BandFilter> r =
            BandFilter::design(8000.0 / r2, 8000.0 * r2, 96000.0);
        CHECK(r.success());
    }
    {
        // 無効なフィルタ (既定構築) は valid() = false
        BandFilter f;
        CHECK(!f.valid());
        const std::vector<double> x(16, 1.0);
        const std::vector<double> y =
            f.apply(ArrayView<const double>(x.data(), x.size()), false);
        CHECK(y.size() == x.size());
        for (std::size_t i = 0; i < y.size(); ++i) CHECK(y[i] == 0.0);
    }

    // ── filterBand(): fullBand は素通し / 空入力はエラー ──
    {
        const std::vector<double> x(1000, 0.5);
        std::vector<Band> full = makeBands(BandSet::FullBandOnly);
        CHECK(full.size() == 1);
        AcousticResult<std::vector<double>> r = filterBand(
            ArrayView<const double>(x.data(), x.size()), 96000.0, full[0],
            false);
        CHECK(r.success());
        CHECK(r.value().size() == x.size());
        CHECK(r.value()[0] == 0.5);

        AcousticResult<std::vector<double>> e =
            filterBand(ArrayView<const double>(), 96000.0, full[0], false);
        CHECK(!e.success());
        CHECK(e.errorCode() == AcousticErrorCode::EmptyInput);
    }

    return testutil::summary("test_bandfilter");
}
