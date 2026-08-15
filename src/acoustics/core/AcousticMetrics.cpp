// AcousticMetrics.cpp — ISO 3382-1 指標の実装。
#include "AcousticMetrics.h"

#include <cmath>
#include <cstdio>

namespace ofd {
namespace acoustics {

namespace {
const double kEnergyFloor = 1e-30;

// 残響時間: 回帰の傾き slope [dB/s] から RT = -60 / slope (ISO 3382-1 6.3)。
// 評価下限 endDb + マージンがノイズフロアを下回る場合は無効。
MetricValue reverberationFromRegression(const RegressionResult &reg,
                                        double endDb, double noiseFloorDb,
                                        const MetricsOptions &opt,
                                        const char *name) {
    char buf[200];
    if (noiseFloorDb > endDb - opt.validityMarginDb) {
        std::snprintf(buf, sizeof(buf),
                      "%s: insufficient dynamic range (noise floor %.1f dB > "
                      "required %.1f dB = %.0f dB - %.0f dB margin)",
                      name, noiseFloorDb, endDb - opt.validityMarginDb, endDb,
                      opt.validityMarginDb);
        return makeInvalidMetric(buf);
    }
    if (!reg.valid) {
        std::snprintf(buf, sizeof(buf), "%s: regression failed (%s)", name,
                      reg.warning.c_str());
        return makeInvalidMetric(buf);
    }
    if (reg.slope >= -1e-9) {
        std::snprintf(buf, sizeof(buf), "%s: decay slope is not negative", name);
        return makeInvalidMetric(buf);
    }
    const double rt = -60.0 / reg.slope;
    if (reg.rSquared < opt.rSquaredWarningThreshold) {
        std::snprintf(buf, sizeof(buf),
                      "%s: low regression quality (R^2 = %.3f < %.2f)", name,
                      reg.rSquared, opt.rSquaredWarningThreshold);
        return makeWarningMetric(rt, buf);
    }
    return makeValidMetric(rt);
}
} // namespace

AcousticMetricsSet computeAcousticMetrics(ArrayView<const double> rir,
                                          double sampleRateHz,
                                          std::size_t directIndex,
                                          const MetricsOptions &options) {
    AcousticMetricsSet out;
    if (rir.empty() || sampleRateHz <= 0.0 || directIndex >= rir.size()) {
        const std::string why = "invalid input for metrics computation";
        out.edt = out.t20 = out.t30 = makeInvalidMetric(why);
        out.c50 = out.c80 = out.d50 = out.ts = makeInvalidMetric(why);
        out.earlyLate50 = out.earlyLate80 = makeInvalidMetric(why);
        return out;
    }

    // 直接音以降を t = 0 として切り出す (ISO 3382-1 は直接音到来を原点とする)
    ArrayView<const double> h = rir.subview(directIndex, rir.size() - directIndex);
    const std::size_t n = h.size();

    // ── Schroeder 減衰カーブと回帰 (EDT / T20 / T30) ──
    const SchroederResult dec =
        computeSchroederDecay(h, sampleRateHz, options.schroeder);
    out.decayNoiseFloorDb = dec.noiseFloorDb;
    if (dec.valid) {
        ArrayView<const double> curve(dec.decayDb.data(), dec.decayDb.size());
        out.edtRegression = regressDecaySegment(curve, sampleRateHz, 0.0, -10.0,
                                                dec.analysisEndIndex);
        out.t20Regression = regressDecaySegment(curve, sampleRateHz, -5.0, -25.0,
                                                dec.analysisEndIndex);
        out.t30Regression = regressDecaySegment(curve, sampleRateHz, -5.0, -35.0,
                                                dec.analysisEndIndex);
        out.edt = reverberationFromRegression(out.edtRegression, -10.0,
                                              dec.noiseFloorDb, options, "EDT");
        out.t20 = reverberationFromRegression(out.t20Regression, -25.0,
                                              dec.noiseFloorDb, options, "T20");
        out.t30 = reverberationFromRegression(out.t30Regression, -35.0,
                                              dec.noiseFloorDb, options, "T30");
    } else {
        out.edt = out.t20 = out.t30 =
            makeInvalidMetric("decay curve computation failed: " + dec.warning);
    }

    // ── エネルギー比指標 (C50 / C80 / D50 / Ts / Early-Late) ──
    // 境界サンプル: 早期 = [0, b), 後期 = [b, N)
    const std::size_t b50 = static_cast<std::size_t>(
        std::floor(0.050 * sampleRateHz + 0.5));
    const std::size_t b80 = static_cast<std::size_t>(
        std::floor(0.080 * sampleRateHz + 0.5));
    // 舞台支援 (ST) の窓境界 (ISO 3382-1 Annex C)
    const std::size_t b10 = static_cast<std::size_t>(
        std::floor(0.010 * sampleRateHz + 0.5));
    const std::size_t b20 = static_cast<std::size_t>(
        std::floor(0.020 * sampleRateHz + 0.5));
    const std::size_t b100 = static_cast<std::size_t>(
        std::floor(0.100 * sampleRateHz + 0.5));
    const std::size_t b1000 = static_cast<std::size_t>(
        std::floor(1.000 * sampleRateHz + 0.5));

    double total = 0.0, firstMoment = 0.0;
    double early50 = 0.0, early80 = 0.0;
    double stRef = 0.0, stEarlySum = 0.0, stLateSum = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double p2 = h[i] * h[i];
        total += p2;
        firstMoment += (static_cast<double>(i) / sampleRateHz) * p2;
        if (i < b50) early50 += p2;
        if (i < b80) early80 += p2;
        if (i < b10) stRef += p2;
        if (i >= b20 && i < b100) stEarlySum += p2;
        if (i >= b100 && i < b1000) stLateSum += p2;
    }

    if (total <= kEnergyFloor) {
        const std::string why = "signal has no energy after direct sound";
        out.c50 = out.c80 = out.d50 = out.ts = makeInvalidMetric(why);
        out.earlyLate50 = out.earlyLate80 = makeInvalidMetric(why);
        out.stEarly = out.stLate = makeInvalidMetric(why);
        return out;
    }

    // C50/C80 (ISO 3382-1 A.2.4): C_te = 10*log10(∫0..te p² / ∫te..∞ p²)
    const double late50 = total - early50;
    const double late80 = total - early80;

    if (n <= b50) {
        out.c50 = makeInvalidMetric("C50: signal shorter than 50 ms after direct");
        out.earlyLate50 = makeInvalidMetric("early/late(50ms): signal too short");
    } else if (late50 <= kEnergyFloor) {
        out.c50 = makeInvalidMetric("C50: no late energy (ratio undefined)");
        out.earlyLate50 = makeInvalidMetric("early/late(50ms): no late energy");
    } else {
        out.c50 = makeValidMetric(10.0 * std::log10(early50 / late50));
        out.earlyLate50 = makeValidMetric(early50 / late50);
    }

    if (n <= b80) {
        out.c80 = makeInvalidMetric("C80: signal shorter than 80 ms after direct");
        out.earlyLate80 = makeInvalidMetric("early/late(80ms): signal too short");
    } else if (late80 <= kEnergyFloor) {
        out.c80 = makeInvalidMetric("C80: no late energy (ratio undefined)");
        out.earlyLate80 = makeInvalidMetric("early/late(80ms): no late energy");
    } else {
        out.c80 = makeValidMetric(10.0 * std::log10(early80 / late80));
        out.earlyLate80 = makeValidMetric(early80 / late80);
    }

    // D50 (ISO 3382-1 A.2.3): 早期エネルギー / 全エネルギー
    if (n <= b50) {
        out.d50 = makeInvalidMetric("D50: signal shorter than 50 ms after direct");
    } else {
        out.d50 = makeValidMetric(early50 / total);
    }

    // Ts (ISO 3382-1 A.2.5): 重心時間 [s]
    out.ts = makeValidMetric(firstMoment / total);

    // ST_early / ST_late (ISO 3382-1 Annex C): 舞台支援。
    // 基準は直接音を含む 0-10 ms。窓が信号長で欠けるときは値を出さない
    // (欠けた窓の積分は常に過小で、支援が弱いように見える誤値になる)。
    if (stRef <= kEnergyFloor) {
        const std::string why = "ST: no reference energy (0-10 ms)";
        out.stEarly = makeInvalidMetric(why);
        out.stLate = makeInvalidMetric(why);
    } else {
        if (n < b100) {
            out.stEarly = makeInvalidMetric(
                "ST_early: signal shorter than 100 ms after direct");
        } else if (stEarlySum <= kEnergyFloor) {
            out.stEarly = makeInvalidMetric(
                "ST_early: no energy in 20-100 ms window");
        } else {
            out.stEarly = makeValidMetric(10.0 * std::log10(stEarlySum / stRef));
        }
        if (n < b1000) {
            out.stLate = makeInvalidMetric(
                "ST_late: signal shorter than 1 s after direct");
        } else if (stLateSum <= kEnergyFloor) {
            out.stLate = makeInvalidMetric(
                "ST_late: no energy in 100-1000 ms window");
        } else {
            out.stLate = makeValidMetric(10.0 * std::log10(stLateSum / stRef));
        }
    }

    return out;
}

double requiredInrDb(DecayMetricKind k)
{
    // 評価区間の深さ + ISO 3382-2 の 10 dB マージン
    switch (k) {
        case DecayMetricKind::EDT: return 10.0 + 10.0;
        case DecayMetricKind::T20: return 25.0 + 10.0;
        case DecayMetricKind::T30: return 35.0 + 10.0;
    }
    return 45.0;
}

bool inrSufficient(double inrDb, DecayMetricKind k)
{
    return inrDb >= requiredInrDb(k);
}

} // namespace acoustics
} // namespace ofd
