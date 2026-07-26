// SchroederDecay.cpp — Schroeder (1965) 二乗後方積分と最小二乗回帰の実装。
#include "SchroederDecay.h"

#include <cmath>

#include "NoiseFloorEstimator.h"

namespace ofd {
namespace acoustics {

namespace {
// エネルギー比の数値下限。10*log10(1e-30) = -300 dB。
const double kEnergyFloor = 1e-30;

double energyDb(double ratio) {
    if (ratio < kEnergyFloor) ratio = kEnergyFloor;
    return 10.0 * std::log10(ratio);
}
} // namespace

SchroederResult computeSchroederDecay(ArrayView<const double> rir,
                                      double sampleRateHz,
                                      const SchroederOptions &options) {
    SchroederResult res;
    const std::size_t n = rir.size();
    if (n == 0) {
        res.warning = "input is empty";
        return res;
    }
    if (sampleRateHz <= 0.0) {
        res.warning = "invalid sample rate";
        return res;
    }

    // ノイズパワー推定 (末尾区間の x^2 平均)
    const NoiseFloorEstimate nf =
        estimateNoiseFloor(rir, options.tailFraction, options.minTailSamples);
    const double noisePower = nf.noiseRms * nf.noiseRms;
    res.noisePower = noisePower;

    // 後方積分 E(n) = Σ_{k≥n} x^2 (補正: - p*(N-n))
    std::vector<double> energy(n, 0.0);
    double acc = 0.0;
    for (std::size_t i = n; i > 0; --i) {
        const double v = rir[i - 1];
        acc += v * v;
        energy[i - 1] = acc;
    }
    if (options.noiseCompensation && noisePower > 0.0) {
        for (std::size_t i = 0; i < n; ++i) {
            const double corr =
                noisePower * static_cast<double>(n - i);
            energy[i] -= corr;
            if (energy[i] < 0.0) energy[i] = 0.0;
        }
    }
    const double e0 = energy[0];
    if (e0 <= 0.0) {
        res.warning = "signal has no energy";
        return res;
    }
    res.totalEnergy = e0;

    res.decayDb.resize(n);
    for (std::size_t i = 0; i < n; ++i)
        res.decayDb[i] = energyDb(energy[i] / e0);

    // ── 平滑化包絡線 (移動平均した x^2) によるノイズフロアと分析終了点 ──
    // 指数減衰では包絡線パワーの dB と Schroeder カーブの dB は同じ傾きで
    // 減衰するため、包絡線基準のノイズフロアを減衰カーブの評価下限判定に
    // そのまま使える (Lundeby 法の簡略版)。
    std::size_t w = static_cast<std::size_t>(
        std::floor(options.smoothingWindowSeconds * sampleRateHz + 0.5));
    if (w < 1) w = 1;
    if (w > n) w = n;
    double winSum = 0.0;
    for (std::size_t i = 0; i < w; ++i) winSum += rir[i] * rir[i];
    double maxSmooth = winSum / static_cast<double>(w);
    std::size_t maxSmoothIdx = 0;
    {
        double s = winSum;
        for (std::size_t i = w; i < n; ++i) {
            s += rir[i] * rir[i] - rir[i - w] * rir[i - w];
            const double m = s / static_cast<double>(w);
            if (m > maxSmooth) {
                maxSmooth = s / static_cast<double>(w);
                maxSmoothIdx = i - w + 1;
            }
        }
    }
    if (maxSmooth <= 0.0) maxSmooth = kEnergyFloor;
    res.noiseFloorDb = energyDb((noisePower + kEnergyFloor) / maxSmooth);

    // 分析終了点: 平滑化包絡線が noisePower * 10^(margin/10) を下回る最初の点
    res.analysisEndIndex = n;
    if (noisePower > 0.0) {
        const double limit =
            noisePower * std::pow(10.0, options.analysisEndMarginDb / 10.0);
        double s = winSum;
        // 窓 [i, i+w) を前進させ、包絡線値は窓の先頭 i に割り当てる
        for (std::size_t i = 0; i + w <= n; ++i) {
            if (i > 0) s += rir[i + w - 1] * rir[i + w - 1] -
                            rir[i - 1] * rir[i - 1];
            if (i > maxSmoothIdx && s / static_cast<double>(w) <= limit) {
                res.analysisEndIndex = i;
                break;
            }
        }
    }

    res.valid = true;
    return res;
}

RegressionResult regressDecaySegment(ArrayView<const double> decayDb,
                                     double sampleRateHz, double startDb,
                                     double endDb,
                                     std::size_t analysisEndIndex) {
    RegressionResult reg;
    const std::size_t n = decayDb.size();
    if (n == 0 || sampleRateHz <= 0.0) {
        reg.warning = "empty decay curve or invalid sample rate";
        return reg;
    }
    if (endDb >= startDb) {
        reg.warning = "endDb must be below startDb";
        return reg;
    }
    if (analysisEndIndex > n) analysisEndIndex = n;

    // 開始点: decayDb <= startDb となる最初の点
    std::size_t i0 = analysisEndIndex;
    for (std::size_t i = 0; i < analysisEndIndex; ++i) {
        if (decayDb[i] <= startDb) {
            i0 = i;
            break;
        }
    }
    if (i0 >= analysisEndIndex) {
        reg.warning = "start level not reached within analysis range";
        return reg;
    }
    // 終了点: decayDb <= endDb となる最初の点
    std::size_t i1 = analysisEndIndex;
    for (std::size_t i = i0; i < analysisEndIndex; ++i) {
        if (decayDb[i] <= endDb) {
            i1 = i;
            break;
        }
    }
    if (i1 >= analysisEndIndex) {
        reg.warning = "end level not reached before noise floor";
        return reg;
    }
    const std::size_t count = i1 - i0 + 1;
    if (count < 4) {
        reg.warning = "too few points for regression";
        return reg;
    }

    // 最小二乗: y = slope * t + intercept (t = i / fs)
    double sx = 0.0, sy = 0.0;
    for (std::size_t i = i0; i <= i1; ++i) {
        sx += static_cast<double>(i) / sampleRateHz;
        sy += decayDb[i];
    }
    const double nn = static_cast<double>(count);
    const double mx = sx / nn, my = sy / nn;
    double sxx = 0.0, sxy = 0.0, syy = 0.0;
    for (std::size_t i = i0; i <= i1; ++i) {
        const double dx = static_cast<double>(i) / sampleRateHz - mx;
        const double dy = decayDb[i] - my;
        sxx += dx * dx;
        sxy += dx * dy;
        syy += dy * dy;
    }
    if (sxx <= 0.0) {
        reg.warning = "degenerate abscissa";
        return reg;
    }
    reg.slope = sxy / sxx;
    reg.intercept = my - reg.slope * mx;

    // 決定係数と傾きの標準誤差
    double sse = 0.0;
    for (std::size_t i = i0; i <= i1; ++i) {
        const double t = static_cast<double>(i) / sampleRateHz;
        const double e = decayDb[i] - (reg.slope * t + reg.intercept);
        sse += e * e;
    }
    reg.rSquared = (syy > 0.0) ? 1.0 - sse / syy : 1.0;
    reg.standardError =
        (count > 2) ? std::sqrt(sse / (nn - 2.0) / sxx) : 0.0;
    reg.startDb = decayDb[i0];
    reg.endDb = decayDb[i1];
    reg.valid = true;
    return reg;
}

} // namespace acoustics
} // namespace ofd
