// RadioPropagation.cpp — 電波伝搬リンクバジェット (式の出典はヘッダ参照)
#include "em/RadioPropagation.h"

#include <algorithm>
#include <cmath>

namespace ofd {
namespace em {
namespace propagation {
namespace {

const double kPi = 3.14159265358979323846;

// 直接波・反射波の行路長 [m] (平面大地)
void rayLengths(double d, double ht, double hr, double &d1, double &d2)
{
    d1 = std::sqrt(d * d + (ht - hr) * (ht - hr));
    d2 = std::sqrt(d * d + (ht + hr) * (ht + hr));
}

} // namespace

double wavelength(double freq_hz)
{
    if (!(freq_hz > 0.0)) return 0.0;
    return kC0 / freq_hz;
}

double freeSpacePathLossDb(double dist_m, double freq_hz)
{
    const double lam = wavelength(freq_hz);
    if (!(lam > 0.0) || !(dist_m > 0.0)) return 0.0;
    return 20.0 * std::log10(4.0 * kPi * dist_m / lam);
}

// 2 波干渉。振幅比 |E/E0| = (λ/4π)·|e^{-jk d1}/d1 − |Γ|·e^{-jk d2}/d2|
double twoRayPathLossDb(double dist_m, double hTx_m, double hRx_m,
                        double freq_hz, double reflection, double gammaSign)
{
    const double lam = wavelength(freq_hz);
    if (!(lam > 0.0) || !(dist_m > 0.0)) return 0.0;
    if (!(hTx_m >= 0.0) || !(hRx_m >= 0.0)) return 0.0;
    double d1 = 0.0, d2 = 0.0;
    rayLengths(dist_m, hTx_m, hRx_m, d1, d2);
    if (!(d1 > 0.0) || !(d2 > 0.0)) return 0.0;
    const double k = 2.0 * kPi / lam;
    // Γ = gammaSign·|Γ| — 既定の −1 は位相反転 (水平偏波 / grazing 入射)、
    // +1 は完全導体面の垂直偏波
    const double g = (gammaSign >= 0.0) ? reflection : -reflection;
    const double re = std::cos(k * d1) / d1 + g * std::cos(k * d2) / d2;
    const double im = -std::sin(k * d1) / d1 - g * std::sin(k * d2) / d2;
    const double amp = (lam / (4.0 * kPi)) * std::sqrt(re * re + im * im);
    if (!(amp > 0.0)) return kMaxPathLossDb;      // 完全なヌル (発散を切る)
    const double loss = -20.0 * std::log10(amp);
    return (loss > kMaxPathLossDb) ? kMaxPathLossDb : loss;
}

double breakpointDistance(double hTx_m, double hRx_m, double freq_hz)
{
    const double lam = wavelength(freq_hz);
    if (!(lam > 0.0) || !(hTx_m > 0.0) || !(hRx_m > 0.0)) return 0.0;
    return 4.0 * hTx_m * hRx_m / lam;
}

double pathLossExponent(double loss1Db, double d1_m, double loss2Db, double d2_m)
{
    if (!(d1_m > 0.0) || !(d2_m > 0.0)) return 0.0;
    const double den = 10.0 * std::log10(d2_m / d1_m);
    if (std::fabs(den) < 1e-12) return 0.0;
    return (loss2Db - loss1Db) / den;
}

double twoRayKFactorDb(double dist_m, double hTx_m, double hRx_m,
                       double reflection)
{
    if (!(dist_m > 0.0) || !(reflection > 0.0)) return 0.0;
    double d1 = 0.0, d2 = 0.0;
    rayLengths(dist_m, hTx_m, hRx_m, d1, d2);
    if (!(d1 > 0.0) || !(d2 > 0.0)) return 0.0;
    return 20.0 * std::log10(d2 / (d1 * reflection));
}

double twoRayExcessDelay(double dist_m, double hTx_m, double hRx_m)
{
    if (!(dist_m > 0.0)) return 0.0;
    double d1 = 0.0, d2 = 0.0;
    rayLengths(dist_m, hTx_m, hRx_m, d1, d2);
    return (d2 - d1) / kC0;
}

double receivedPowerDbm(double eirpDbm, double pathLossDb, double rxGainDbi)
{
    return eirpDbm - pathLossDb + rxGainDbi;
}

double thermalNoiseDbm(double bandwidth_hz, double noiseFigureDb)
{
    if (!(bandwidth_hz > 0.0)) return 0.0;
    // N[W] = k·T0·B → dBm は 1 mW 基準
    const double n_w = kBoltzmann * kT0 * bandwidth_hz;
    return 10.0 * std::log10(n_w / 1e-3) + noiseFigureDb;
}

double shannonCapacity(double bandwidth_hz, double snrDb)
{
    if (!(bandwidth_hz > 0.0)) return 0.0;
    const double snr = std::pow(10.0, snrDb / 10.0);
    return bandwidth_hz * std::log2(1.0 + snr);
}

double arrayGainDb(int elements)
{
    if (elements < 1) return 0.0;
    return 10.0 * std::log10(double(elements));
}

double mimoCapacity(double bandwidth_hz, double snrDb, int nTx, int nRx)
{
    if (!(bandwidth_hz > 0.0)) return 0.0;
    if (nTx < 1 || nRx < 1) return 0.0;
    const int streams = (nTx < nRx) ? nTx : nRx;
    // 送信電力を Nt 本へ等分するので、1 本あたりの SNR は SNR/Nt
    const double snr = std::pow(10.0, snrDb / 10.0) / double(nTx);
    return double(streams) * bandwidth_hz * std::log2(1.0 + snr);
}

CoverageGrid coverageMap(double halfSpan_m, int n,
                         double hTx_m, double hRx_m, double freq_hz,
                         double eirpDbm, double rxGainDbi,
                         double reflection, double minDistance_m)
{
    CoverageGrid g;
    if (n <= 0 || !(halfSpan_m > 0.0) || !(freq_hz > 0.0)) return g;
    if (!(minDistance_m > 0.0)) minDistance_m = 1.0;
    g.n = n;
    g.halfSpan_m = halfSpan_m;
    g.dbm.resize(std::size_t(n) * std::size_t(n));
    double lo = 0.0, hi = 0.0;
    bool first = true;
    for (int iy = 0; iy < n; ++iy) {
        const double y = g.coord(iy);
        for (int ix = 0; ix < n; ++ix) {
            const double x = g.coord(ix);
            double d = std::sqrt(x * x + y * y);
            if (d < minDistance_m) d = minDistance_m;   // 原点の発散を切る
            const double loss =
                twoRayPathLossDb(d, hTx_m, hRx_m, freq_hz, reflection);
            const double p = receivedPowerDbm(eirpDbm, loss, rxGainDbi);
            g.dbm[std::size_t(iy) * std::size_t(n) + std::size_t(ix)] = p;
            if (first) { lo = hi = p; first = false; }
            else { if (p < lo) lo = p; if (p > hi) hi = p; }
        }
    }
    g.minDbm = lo;
    g.maxDbm = hi;
    return g;
}

// ── 複数 AP ───────────────────────────────────────────────────────────────
std::vector<AccessPoint> apRing(int count, double radius_m, double h_m,
                                double eirpDbm)
{
    std::vector<AccessPoint> aps;
    if (count <= 0) return aps;
    aps.reserve(std::size_t(count));
    if (count == 1) {                       // 1 局は中心 (従来の図と揃える)
        aps.push_back({ 0.0, 0.0, h_m, eirpDbm });
        return aps;
    }
    for (int k = 0; k < count; ++k) {
        const double a = 2.0 * kPi * k / count;
        aps.push_back({ radius_m * std::cos(a), radius_m * std::sin(a),
                        h_m, eirpDbm });
    }
    return aps;
}

MultiCoverage coverageMapMulti(const std::vector<AccessPoint> &aps,
                               double halfSpan_m, int n,
                               double hRx_m, double freq_hz, double rxGainDbi,
                               double noiseDbm, double thresholdDbm,
                               double reflection, double minDistance_m)
{
    MultiCoverage g;
    if (aps.empty() || n <= 0 || !(halfSpan_m > 0.0) || !(freq_hz > 0.0))
        return g;
    if (!(minDistance_m > 0.0)) minDistance_m = 1.0;
    g.n = n;
    g.halfSpan_m = halfSpan_m;
    const std::size_t cells = std::size_t(n) * std::size_t(n);
    g.bestDbm.resize(cells);
    g.server.assign(cells, -1);
    g.sinrDb.resize(cells);

    // 雑音は真値 [mW] で持つ (電力の足し算を dB のままやらない)
    const double noise_mW = std::pow(10.0, noiseDbm / 10.0);

    double lo = 0.0, hi = 0.0;
    bool first = true;
    long long covered = 0;
    for (int iy = 0; iy < n; ++iy) {
        const double y = g.coord(iy);
        for (int ix = 0; ix < n; ++ix) {
            const double x = g.coord(ix);
            double bestDbm = 0.0, total_mW = 0.0;
            int best = -1;
            for (std::size_t k = 0; k < aps.size(); ++k) {
                const double dx = x - aps[k].x_m, dy = y - aps[k].y_m;
                double d = std::sqrt(dx * dx + dy * dy);
                if (d < minDistance_m) d = minDistance_m;
                const double loss =
                    twoRayPathLossDb(d, aps[k].h_m, hRx_m, freq_hz, reflection);
                const double p = receivedPowerDbm(aps[k].eirpDbm, loss, rxGainDbi);
                total_mW += std::pow(10.0, p / 10.0);
                if (best < 0 || p > bestDbm) { bestDbm = p; best = int(k); }
            }
            const std::size_t c = std::size_t(iy) * std::size_t(n) + std::size_t(ix);
            g.bestDbm[c] = bestDbm;
            g.server[c] = best;
            // 干渉 = 最良サーバ以外の合計 (真値で引く)
            const double best_mW = std::pow(10.0, bestDbm / 10.0);
            const double intf_mW = std::max(0.0, total_mW - best_mW);
            g.sinrDb[c] = 10.0 * std::log10(best_mW / (intf_mW + noise_mW));
            if (bestDbm >= thresholdDbm) ++covered;
            if (first) { lo = hi = bestDbm; first = false; }
            else { if (bestDbm < lo) lo = bestDbm; if (bestDbm > hi) hi = bestDbm; }
        }
    }
    g.minDbm = lo;
    g.maxDbm = hi;
    g.coveredFraction = double(covered) / double(cells);
    return g;
}

} // namespace propagation
} // namespace em
} // namespace ofd

// ── 環境別の経験式 ─────────────────────────────────────────────────────────
namespace {

// 奥村-秦の移動局高補正 a(hm) [dB]
double hataMobileCorrection(double freq_mhz, double hm_m, bool largeCity)
{
    if (!(freq_mhz > 0.0) || !(hm_m > 0.0)) return 0.0;
    const double lf = std::log10(freq_mhz);
    if (largeCity) {
        // 大都市: 200 MHz 以下と 400 MHz 以上で式が違う (中間は補間しない —
        // 原典が定めていないため、400 MHz 以上の式を使う側へ寄せる)
        if (freq_mhz <= 200.0) {
            const double t = std::log10(1.54 * hm_m);
            return 8.29 * t * t - 1.10;
        }
        const double t = std::log10(11.75 * hm_m);
        return 3.20 * t * t - 4.97;
    }
    return (1.1 * lf - 0.7) * hm_m - (1.56 * lf - 0.8);
}

} // namespace

double ofd::em::propagation::logDistancePathLossDb(double dist_m, double freq_hz,
                                                   double exponent, double d0_m)
{
    if (!(dist_m > 0.0) || !(freq_hz > 0.0) || !(d0_m > 0.0)) return 0.0;
    const double l0 = freeSpacePathLossDb(d0_m, freq_hz);
    const double l = l0 + 10.0 * exponent * std::log10(dist_m / d0_m);
    return std::min(l, kMaxPathLossDb);
}

double ofd::em::propagation::indoorP1238PathLossDb(double dist_m, double freq_hz,
                                                   double distCoef,
                                                   double floorLossDb)
{
    if (!(dist_m > 0.0) || !(freq_hz > 0.0)) return 0.0;
    const double fMHz = freq_hz / 1e6;
    const double l = 20.0 * std::log10(fMHz) + distCoef * std::log10(dist_m)
                     - 28.0 + floorLossDb;
    return std::min(l, kMaxPathLossDb);
}

double ofd::em::propagation::hataUrbanPathLossDb(double dist_m, double freq_hz,
                                                 double hb_m, double hm_m,
                                                 bool largeCity)
{
    if (!(dist_m > 0.0) || !(freq_hz > 0.0) || !(hb_m > 0.0) || !(hm_m > 0.0))
        return 0.0;
    const double fMHz = freq_hz / 1e6;
    const double dkm = dist_m / 1000.0;
    const double lf = std::log10(fMHz), lb = std::log10(hb_m);
    const double a = hataMobileCorrection(fMHz, hm_m, largeCity);
    const double l = 69.55 + 26.16 * lf - 13.82 * lb - a
                     + (44.9 - 6.55 * lb) * std::log10(dkm);
    return std::min(l, kMaxPathLossDb);
}

double ofd::em::propagation::hataSuburbanPathLossDb(double dist_m,
                                                    double freq_hz,
                                                    double hb_m, double hm_m)
{
    if (!(freq_hz > 0.0)) return 0.0;
    const double urban = hataUrbanPathLossDb(dist_m, freq_hz, hb_m, hm_m, false);
    if (urban == 0.0) return 0.0;
    const double t = std::log10(freq_hz / 1e6 / 28.0);
    return std::min(urban - 2.0 * t * t - 5.4, kMaxPathLossDb);
}

double ofd::em::propagation::hataOpenPathLossDb(double dist_m, double freq_hz,
                                                double hb_m, double hm_m)
{
    if (!(freq_hz > 0.0)) return 0.0;
    const double urban = hataUrbanPathLossDb(dist_m, freq_hz, hb_m, hm_m, false);
    if (urban == 0.0) return 0.0;
    const double lf = std::log10(freq_hz / 1e6);
    return std::min(urban - 4.78 * lf * lf + 18.33 * lf - 40.94,
                    kMaxPathLossDb);
}

double ofd::em::propagation::cost231HataPathLossDb(double dist_m, double freq_hz,
                                                   double hb_m, double hm_m,
                                                   double cityCorrectionDb,
                                                   bool largeCity)
{
    if (!(dist_m > 0.0) || !(freq_hz > 0.0) || !(hb_m > 0.0) || !(hm_m > 0.0))
        return 0.0;
    const double fMHz = freq_hz / 1e6;
    const double dkm = dist_m / 1000.0;
    const double lf = std::log10(fMHz), lb = std::log10(hb_m);
    const double a = hataMobileCorrection(fMHz, hm_m, largeCity);
    const double l = 46.3 + 33.9 * lf - 13.82 * lb - a
                     + (44.9 - 6.55 * lb) * std::log10(dkm) + cityCorrectionDb;
    return std::min(l, kMaxPathLossDb);
}

bool ofd::em::propagation::hataApplicable(double dist_m, double freq_hz,
                                          double hb_m, double hm_m)
{
    const double fMHz = freq_hz / 1e6, dkm = dist_m / 1000.0;
    return fMHz >= 150.0 && fMHz <= 1500.0 && hb_m >= 30.0 && hb_m <= 200.0
           && hm_m >= 1.0 && hm_m <= 10.0 && dkm >= 1.0 && dkm <= 20.0;
}

bool ofd::em::propagation::cost231Applicable(double dist_m, double freq_hz,
                                             double hb_m, double hm_m)
{
    const double fMHz = freq_hz / 1e6, dkm = dist_m / 1000.0;
    return fMHz >= 1500.0 && fMHz <= 2000.0 && hb_m >= 30.0 && hb_m <= 200.0
           && hm_m >= 1.0 && hm_m <= 10.0 && dkm >= 1.0 && dkm <= 20.0;
}
