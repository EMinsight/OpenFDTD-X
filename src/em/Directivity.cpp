#include "Directivity.h"

#include <cmath>

namespace ofd {
namespace em {

namespace {
const double kPi = 3.14159265358979323846;
const double kDeg = kPi / 180.0;
} // namespace

double intensityFromEabsDb(double dB)
{
    // E-abs[dB] = 20log10|E| なので |E|² = 10^(dB/10)
    return std::pow(10.0, dB / 10.0);
}

Directivity directivity(const SphericalPattern &p)
{
    Directivity d;
    if (!p.valid()) return d;
    const int nt = p.nTheta(), np = p.nPhi();

    // θ が昇順なら u = cosθ は降順。台形則は区間幅の絶対値で足す。
    std::vector<double> u(static_cast<std::size_t>(nt));
    for (int i = 0; i < nt; ++i) u[static_cast<std::size_t>(i)] = std::cos(p.theta_deg[static_cast<std::size_t>(i)] * kDeg);

    double total = 0.0, peak = 0.0;
    int pi = 0, pj = 0;
    bool first = true;
    // ∮U dΩ = ∫∫ U du dφ を 2 次元の台形則で。
    // 端点に半分の重みが付くので、φ = 0 と 360 の重複は自動的に 1 回分になる。
    for (int i = 0; i < nt; ++i) {
        // u 方向の重み (端は片側だけ)
        double du = 0.0;
        if (nt >= 2) {
            const double um = (i > 0) ? u[static_cast<std::size_t>(i - 1)] : u[0];
            const double up = (i < nt - 1) ? u[static_cast<std::size_t>(i + 1)] : u[static_cast<std::size_t>(nt - 1)];
            du = 0.5 * std::fabs(up - um);
        }
        for (int j = 0; j < np; ++j) {
            double dphi = 0.0;
            const double pm = (j > 0) ? p.phi_deg[static_cast<std::size_t>(j - 1)] : p.phi_deg[0];
            const double pp = (j < np - 1) ? p.phi_deg[static_cast<std::size_t>(j + 1)]
                                           : p.phi_deg[static_cast<std::size_t>(np - 1)];
            dphi = 0.5 * std::fabs(pp - pm) * kDeg;
            const double v = p.u[static_cast<std::size_t>(i) * np + j];
            total += v * du * dphi;
            if (first || v > peak) { peak = v; pi = i; pj = j; first = false; }
        }
    }
    if (!(total > 0.0) || !(peak > 0.0)) return d;

    d.valid = true;
    d.radiatedPower = total;
    d.peak = peak;
    d.directivity = 4.0 * kPi * peak / total;
    d.directivityDbi = 10.0 * std::log10(d.directivity);
    d.beamSolidAngle = total / peak;
    d.peakTheta_deg = p.theta_deg[static_cast<std::size_t>(pi)];
    d.peakPhi_deg = p.phi_deg[static_cast<std::size_t>(pj)];
    return d;
}

} // namespace em
} // namespace ofd
