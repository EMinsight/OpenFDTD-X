// TestSignal.cpp — 仕様は TestSignal.h
#include "TestSignal.h"

#include <cmath>

namespace ofd {
namespace acoustics {

namespace {

const double kPi = 3.14159265358979323846;

// sinc(x) = sin(πx)/(πx)、sinc(0) = 1
double sinc(double x)
{
    if (std::fabs(x) < 1e-12) return 1.0;
    const double a = kPi * x;
    return std::sin(a) / a;
}

} // namespace

bool ClickSpec::valid() const
{
    if (!(sampleRateHz > 0.0)) return false;
    if (!(lowHz >= 0.0)) return false;
    if (!(highHz > lowHz)) return false;
    if (!(highHz < 0.5 * sampleRateHz)) return false;
    if (!(durationSec > 0.0)) return false;
    if (!(amplitude > 0.0) || amplitude > 1.0) return false;
    // 窓が短すぎると帯域制限の意味が無い
    return tapCount() >= 9;
}

std::size_t ClickSpec::tapCount() const
{
    if (!(sampleRateHz > 0.0) || !(durationSec > 0.0)) return 0;
    const double raw = durationSec * sampleRateHz;
    if (!(raw >= 1.0) || raw > 1.0e8) return 0;
    std::size_t n = static_cast<std::size_t>(raw + 0.5);
    if (n % 2 == 0) ++n;          // 中心サンプルを持つよう奇数にする
    return n;
}

double ClickSpec::transitionWidthHz() const
{
    const std::size_t n = tapCount();
    if (n == 0) return 0.0;
    return 3.1 * sampleRateHz / static_cast<double>(n);
}

AudioBuffer generateClick(const ClickSpec &spec)
{
    AudioBuffer out;
    if (!spec.valid()) return out;

    const std::size_t n = spec.tapCount();
    const double c = 0.5 * static_cast<double>(n - 1);   // 中心 (整数)
    const double fs = spec.sampleRateHz;

    std::vector<double> h(n, 0.0);
    std::vector<double> win(n, 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        const double t = (static_cast<double>(i) - c) / fs;
        // 理想帯域通過のインパルス応答 (高域通過分 − 低域通過分)
        const double ideal = 2.0 * spec.highHz / fs * sinc(2.0 * spec.highHz * t)
                           - 2.0 * spec.lowHz  / fs * sinc(2.0 * spec.lowHz  * t);
        // Hann 窓 (両端が 0 の対称窓)
        const double w = 0.5 - 0.5 * std::cos(2.0 * kPi * static_cast<double>(i)
                                              / static_cast<double>(n - 1));
        win[i] = w;
        h[i] = w * ideal;
    }

    // 直流除去。窓長が有限なので、遮断 fl が遷移帯域幅より狭いと直流が
    // 残る (既定の fl = 20 Hz はまさにその状態)。窓そのものの形で引くと
    // Σh = 0 を厳密に満たしつつ両端の 0 と対称性が保たれる — 引いた分の
    // スペクトルは直流まわり ±Δf に集中するので通過域はほぼ動かない。
    {
        double sumH = 0.0, sumW = 0.0;
        for (std::size_t i = 0; i < n; ++i) { sumH += h[i]; sumW += win[i]; }
        if (sumW > 0.0) {
            const double k = sumH / sumW;
            for (std::size_t i = 0; i < n; ++i) h[i] -= k * win[i];
        }
    }

    double peak = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double a = std::fabs(h[i]);
        if (a > peak) peak = a;
    }
    if (!(peak > 0.0)) return out;

    // ピークをちょうど amplitude に合わせる (中央サンプルが最大)
    const double g = spec.amplitude / peak;
    for (std::size_t i = 0; i < n; ++i) h[i] *= g;

    out.sampleRateHz = fs;
    out.channels.push_back(h);
    return out;
}

} // namespace acoustics
} // namespace ofd
