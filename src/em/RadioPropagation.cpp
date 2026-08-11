// RadioPropagation.cpp — 電波伝搬リンクバジェット (式の出典はヘッダ参照)
#include "em/RadioPropagation.h"

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
                        double freq_hz, double reflection)
{
    const double lam = wavelength(freq_hz);
    if (!(lam > 0.0) || !(dist_m > 0.0)) return 0.0;
    if (!(hTx_m >= 0.0) || !(hRx_m >= 0.0)) return 0.0;
    double d1 = 0.0, d2 = 0.0;
    rayLengths(dist_m, hTx_m, hRx_m, d1, d2);
    if (!(d1 > 0.0) || !(d2 > 0.0)) return 0.0;
    const double k = 2.0 * kPi / lam;
    // Γ = −|Γ| (grazing 入射の完全反射は位相反転)
    const double re = std::cos(k * d1) / d1 - reflection * std::cos(k * d2) / d2;
    const double im = -std::sin(k * d1) / d1 + reflection * std::sin(k * d2) / d2;
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

} // namespace propagation
} // namespace em
} // namespace ofd
