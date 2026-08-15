// test_sti.cpp — STI (IEC 60268-16 間接法) の検証。
//
// 主アンカー: 指数減衰 RIR h²(t) = e^{-t/τ} (τ = T/13.8155) の MTF は
//   m(fm) = 1 / √(1 + (2π fm τ)²)  = 1 / √(1 + (2π fm T / 13.8155)²)
// と閉形式で書ける (∫₀^∞ e^{-t/τ}e^{-j2πft}dt = τ/(1+j2πfτ) の絶対値比)。
// 試験信号は帯域中心のトーン搬送波 × 指数包絡を使う (下の makeDecayTone の
// 注記参照)。実測のずれは最悪 0.0007 で、許容 ±0.02 に十分収まる。
//
// ほかに規格上の性質を確認する:
//   - 無響 (単一インパルス) では全変調周波数で m = 1 → STI = 1
//   - 残響が長いほど STI は単調に下がる
//   - fs 不足 / RIR 短すぎ / 直接音位置不正は invalid
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "../../src/acoustics/core/SpeechTransmissionIndex.h"
#include "test_common.h"

using namespace ofd::acoustics;

namespace {

const double kPi = 3.14159265358979323846;

// 指数減衰のトーン搬送波 RIR。
// h(t) = e^{-t/2τ}·cos(2π fc t) とすると
//   h²(t) = e^{-t/τ}·(1 + cos(2·2π fc t))/2
// で、第 2 項は 2fc (≥250 Hz) にあり変調周波数帯 (≤12.5 Hz) へ寄与しない。
// したがってエネルギー包絡は e^{-t/τ} そのもので、MTF の閉形式が厳密に効く。
// **乱数雑音を使ってはいけない** — u² の揺らぎが変調スペクトル全域に白色に
// 載り、m を押し上げる (実測で最大 +0.135 のずれ。テスト側の設計の問題で
// あって実装の誤りではない)。
std::vector<double> makeDecayTone(double fs, double T, double fc,
                                  double durationSec)
{
    const std::size_t n = static_cast<std::size_t>(durationSec * fs + 0.5);
    std::vector<double> h(n, 0.0);
    const double tau = T / 13.8155;
    for (std::size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / fs;
        h[i] = std::exp(-0.5 * t / tau) * std::cos(2.0 * kPi * fc * t);
    }
    return h;
}

// 全帯域に成分を持たせるため、7 帯域の中心トーンを重ねた RIR。
// 各トーンの交差項は帯域通過で分離され、帯域内では単一トーンとして働く。
std::vector<double> makeDecayToneStack(double fs, double T, double durationSec)
{
    std::size_t nb = 0;
    const double *bands = stiOctaveBands(&nb);
    const std::size_t n = static_cast<std::size_t>(durationSec * fs + 0.5);
    std::vector<double> h(n, 0.0);
    const double tau = T / 13.8155;
    for (std::size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / fs;
        const double env = std::exp(-0.5 * t / tau);
        double v = 0.0;
        for (std::size_t k = 0; k < nb; ++k)
            v += std::cos(2.0 * kPi * bands[k] * t);
        h[i] = env * v;
    }
    return h;
}

double mtfTheory(double fm, double T)
{
    const double x = 2.0 * kPi * fm * T / 13.8155;
    return 1.0 / std::sqrt(1.0 + x * x);
}

} // namespace

int main() {
    const double fs = 48000.0;

    // ── (1) 指数減衰 RIR の MTF が閉形式と一致するか (主アンカー) ──
    {
        const double T = 1.0;   // 残響時間 1 s
        const std::vector<double> h = makeDecayToneStack(fs, T, 4.0);
        ArrayView<const double> hv(h.data(), h.size());
        const StiResult r = computeSti(hv, fs, 0);
        CHECK(r.sti.valid);
        CHECK(r.bands.size() == 7);

        std::size_t nFm = 0;
        const double *fm = stiModulationFrequencies(&nFm);
        CHECK(nFm == 14);

        // 中央の帯域 (500 Hz / 1 k / 2 k) で閉形式と比較する。
        // 端の帯域 (125 Hz / 8 k) はフィルタの遷移で包絡が鈍りやすい。
        double worst = 0.0;
        for (std::size_t k = 2; k <= 4; ++k) {
            CHECK(r.bands[k].valid);
            for (std::size_t j = 0; j < nFm; ++j) {
                const double th = mtfTheory(fm[j], T);
                const double d = std::fabs(r.bands[k].mtf[j] - th);
                if (d > worst) worst = d;
            }
        }
        std::printf("  MTF worst |measured - theory| = %.4f (T = %.1f s)\n",
                    worst, T);
        CHECK(worst < 0.02);
    }

    // ── (1b) 単一トーン搬送波でも同じ (帯域を絞った確認) ──
    {
        const double T = 1.5;
        const std::vector<double> h = makeDecayTone(fs, T, 1000.0, 5.0);
        ArrayView<const double> hv(h.data(), h.size());
        const StiResult r = computeSti(hv, fs, 0);
        std::size_t nFm = 0;
        const double *fm = stiModulationFrequencies(&nFm);
        // 1 kHz 帯 (index 3) だけを見る (他帯域はトーンが無く無エネルギー)
        const StiBandResult &b = r.bands[3];
        CHECK(b.valid);
        double worst = 0.0;
        for (std::size_t j = 0; j < nFm; ++j)
            worst = std::max(worst, std::fabs(b.mtf[j] - mtfTheory(fm[j], T)));
        std::printf("  single-tone 1 kHz band worst = %.4f (T = %.1f s)\n",
                    worst, T);
        CHECK(worst < 0.02);
    }

    // ── (2) 残響が長いほど STI は下がる (単調性) ──
    {
        double prev = 2.0;
        for (double T : { 0.5, 1.0, 2.0, 3.0 }) {
            const std::vector<double> h = makeDecayToneStack(fs, T, 5.0);
            ArrayView<const double> hv(h.data(), h.size());
            const StiResult r = computeSti(hv, fs, 0);
            CHECK(r.sti.valid);
            std::printf("  T = %.1f s -> STI = %.3f\n", T, r.sti.value);
            CHECK(r.sti.value < prev);      // 単調減少
            prev = r.sti.value;
        }
    }

    // ── (3) 無響 (単一インパルス) は m = 1 / STI = 1 ──
    {
        std::vector<double> h(static_cast<std::size_t>(2.0 * fs), 0.0);
        h[0] = 1.0;
        ArrayView<const double> hv(h.data(), h.size());
        const StiResult r = computeSti(hv, fs, 0);
        CHECK(r.sti.valid);
        std::printf("  anechoic STI = %.4f\n", r.sti.value);
        // インパルス 1 本のエネルギー包絡は δ なので m = 1 (全 fm)。
        // 帯域通過で僅かに広がるため 0.99 以上を要求する。
        CHECK(r.sti.value > 0.99);
    }

    // ── (4) 前提を満たさない入力は invalid (黙って値を出さない) ──
    {
        // fs 不足 (8 kHz 帯が折り返しで汚れる)
        std::vector<double> h(20000, 0.0);
        h[0] = 1.0;
        ArrayView<const double> hv(h.data(), h.size());
        CHECK(!computeSti(hv, 8000.0, 0).sti.valid);

        // RIR が最低変調周波数の 1 周期 (≈1.6 s) より短い
        std::vector<double> s(static_cast<std::size_t>(0.5 * fs), 0.0);
        s[0] = 1.0;
        ArrayView<const double> sv(s.data(), s.size());
        CHECK(!computeSti(sv, fs, 0).sti.valid);

        // 直接音位置が範囲外
        CHECK(!computeSti(hv, fs, 999999).sti.valid);

        // 空入力
        ArrayView<const double> empty(nullptr, 0);
        CHECK(!computeSti(empty, fs, 0).sti.valid);
    }

    return testutil::summary("test_sti");
}
