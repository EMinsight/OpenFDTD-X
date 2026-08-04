// PlasmaDispersion.cpp — 自由キャリア (プラズマ) 分散 (Qt 非依存 / C++17)
// 式の出典はヘッダ冒頭の [1][2] を参照。
#include "PlasmaDispersion.h"

#include <cmath>

namespace ofd {
namespace optics {

namespace {

constexpr double kPi = 3.14159265358979323846;
// Np → dB (10·log10(e²) = 20/ln10)
constexpr double kNpToDb = 8.685889638065035;

// cm⁻³ → m⁻³
inline double toPerM3(double perCm3) { return perCm3 * 1.0e6; }

} // namespace

double plasmaAngularFrequency(double density_m3, double effectiveMass)
{
    if (density_m3 <= 0.0 || effectiveMass <= 0.0) return 0.0;
    const double m = effectiveMass * kElectronMass;
    return std::sqrt(density_m3 * kElementaryCharge * kElementaryCharge
                     / (kVacuumPermittivity * m));
}

ComplexEps drudePermittivity(double epsInf, double omega, double omegaP,
                             double gamma)
{
    ComplexEps e;
    if (omega <= 0.0) { e.re = epsInf; return e; }
    // ε = ε∞ − ω_p²/(ω² + iωγ) = ε∞ − ω_p²(ω² − iωγ)/(ω⁴ + ω²γ²)
    const double den = omega * omega * (omega * omega + gamma * gamma);
    if (den <= 0.0) { e.re = epsInf; return e; }
    e.re = epsInf - omegaP * omegaP * omega * omega / den;
    e.im = omegaP * omegaP * omega * gamma / den;
    return e;
}

PlasmaResult drudeFreeCarrier(double lambda_nm, double nBackground,
                              const CarrierState &c)
{
    PlasmaResult r;
    if (lambda_nm <= 0.0 || nBackground <= 0.0) return r;
    if (c.deltaN_cm3 < 0.0 || c.deltaP_cm3 < 0.0) return r;
    if (c.meffElectron <= 0.0 || c.meffHole <= 0.0) return r;

    const double lambda_m = lambda_nm * 1.0e-9;
    const double omega = 2.0 * kPi * kSpeedOfLight / lambda_m;
    r.valid = true;
    r.omega_rad_s = omega;

    const double Ne = toPerM3(c.deltaN_cm3);
    const double Nh = toPerM3(c.deltaP_cm3);
    r.omegaP_e_rad_s = plasmaAngularFrequency(Ne, c.meffElectron);
    r.omegaP_h_rad_s = plasmaAngularFrequency(Nh, c.meffHole);

    // Δn = −(ω_pe² + ω_ph²)/(2 n ω²)  ([2] の小摂動展開。展開すると [1] 式(1))
    const double wp2 = r.omegaP_e_rad_s * r.omegaP_e_rad_s
                     + r.omegaP_h_rad_s * r.omegaP_h_rad_s;
    r.deltaN_index = -wp2 / (2.0 * nBackground * omega * omega);

    // Δα = (e³λ²)/(4π²c³ε0 n)·(ΔN/(m_ce*²μ_e) + ΔP/(m_ch*²μ_h))   ([1] 式(2))
    // 移動度は cm²/(V·s) → m²/(V·s) (×1e-4)、有効質量は kg で扱う。
    const double e3 = kElementaryCharge * kElementaryCharge * kElementaryCharge;
    const double pre = e3 * lambda_m * lambda_m
                     / (4.0 * kPi * kPi * kSpeedOfLight * kSpeedOfLight
                        * kSpeedOfLight * kVacuumPermittivity * nBackground);
    const double me = c.meffElectron * kElectronMass;
    const double mh = c.meffHole * kElectronMass;
    const double mue = c.muElectron_cm2Vs * 1.0e-4;
    const double muh = c.muHole_cm2Vs * 1.0e-4;
    double sum = 0.0;
    if (mue > 0.0) sum += Ne / (me * me * mue);
    if (muh > 0.0) sum += Nh / (mh * mh * muh);
    const double alpha_per_m = pre * sum;
    r.deltaAlpha_per_cm = alpha_per_m * 0.01;      // m⁻¹ → cm⁻¹
    // α は強度の減衰係数 (I = I0·e^{−αz}) なので dB/cm は 10·log10(e)·α
    r.deltaAlpha_dB_per_cm = r.deltaAlpha_per_cm * (kNpToDb / 2.0);
    return r;
}

SorefBennettBand nearestSorefBennettBand(double lambda_nm)
{
    // 1.3 μm と 1.55 μm の中点 (1425 nm) で切り替える
    return (lambda_nm < 1425.0) ? SorefBennettBand::Lambda1310nm
                                : SorefBennettBand::Lambda1550nm;
}

bool sorefBennettApplicable(double lambda_nm)
{
    if (lambda_nm <= 0.0) return false;
    const double target = (nearestSorefBennettBand(lambda_nm)
                           == SorefBennettBand::Lambda1310nm) ? 1310.0 : 1550.0;
    return std::fabs(lambda_nm - target) <= 0.05 * target;
}

PlasmaResult sorefBennettSilicon(double lambda_nm, double dN, double dP)
{
    PlasmaResult r;
    if (lambda_nm <= 0.0 || dN < 0.0 || dP < 0.0) return r;
    r.valid = true;
    r.omega_rad_s = 2.0 * kPi * kSpeedOfLight / (lambda_nm * 1.0e-9);

    // [1] Table/式: ΔN, ΔP は cm⁻³、Δα は cm⁻¹
    double an, ap, bn, bp;
    if (nearestSorefBennettBand(lambda_nm) == SorefBennettBand::Lambda1310nm) {
        an = 6.2e-22; ap = 6.0e-18;   // Δn 側の係数 (正孔項は指数 0.8)
        bn = 6.0e-18; bp = 4.0e-18;   // Δα 側の係数
    } else {
        an = 8.8e-22; ap = 8.5e-18;
        bn = 8.5e-18; bp = 6.0e-18;
    }
    const double holeTerm = (dP > 0.0) ? ap * std::pow(dP, 0.8) : 0.0;
    r.deltaN_index = -(an * dN + holeTerm);
    r.deltaAlpha_per_cm = bn * dN + bp * dP;
    r.deltaAlpha_dB_per_cm = r.deltaAlpha_per_cm * (kNpToDb / 2.0);
    return r;
}

} // namespace optics
} // namespace ofd
