#include "DispersionModels.h"

#include <algorithm>
#include <cmath>

namespace ofd {
namespace disp {

namespace {
const double kPi = 3.14159265358979323846;
const double kC0 = 2.99792458e8;      // [m/s]
} // namespace

Complex lorentzPermittivity(double epsInf, double deltaEps,
                            double omega0_rad_s, double gamma_rad_s,
                            double omega_rad_s)
{
    Complex e;
    e.re = epsInf;
    if (!(omega0_rad_s > 0.0)) return e;
    // ε = ε∞ + Δε ω₀² / (ω₀² − ω² − iγω)
    const double w02 = omega0_rad_s * omega0_rad_s;
    const double a = w02 - omega_rad_s * omega_rad_s;   // 実部の分母
    const double b = -gamma_rad_s * omega_rad_s;        // 虚部の分母
    const double den = a * a + b * b;
    if (!(den > 0.0)) return e;
    // 1/(a + ib) = (a − ib)/(a²+b²)
    const double num = deltaEps * w02;
    e.re = epsInf + num * a / den;
    e.im = -num * b / den;                              // b < 0 なので im > 0
    return e;
}

double sellmeierIndex(const std::vector<SellmeierTerm> &terms, double lambda_um)
{
    if (!(lambda_um > 0.0)) return 0.0;
    const double l2 = lambda_um * lambda_um;
    double n2 = 1.0;
    for (const SellmeierTerm &t : terms) {
        const double d = l2 - t.c_um2;
        if (d == 0.0) return 0.0;                       // 極そのもの
        n2 += t.b * l2 / d;
    }
    return (n2 > 0.0) ? std::sqrt(n2) : 0.0;            // 極の内側は返さない
}

double sellmeierIndexSlope(const std::vector<SellmeierTerm> &terms,
                           double lambda_um)
{
    const double n = sellmeierIndex(terms, lambda_um);
    if (!(n > 0.0)) return 0.0;
    // d(n²)/dλ = Σ Bᵢ · d/dλ [λ²/(λ²−Cᵢ)] = Σ Bᵢ · (−2λCᵢ)/(λ²−Cᵢ)²
    const double l2 = lambda_um * lambda_um;
    double dn2 = 0.0;
    for (const SellmeierTerm &t : terms) {
        const double d = l2 - t.c_um2;
        if (d == 0.0) return 0.0;
        dn2 += t.b * (-2.0 * lambda_um * t.c_um2) / (d * d);
    }
    return dn2 / (2.0 * n);                             // dn/dλ = (dn²/dλ)/(2n)
}

double sellmeierGroupIndex(const std::vector<SellmeierTerm> &terms,
                           double lambda_um)
{
    const double n = sellmeierIndex(terms, lambda_um);
    if (!(n > 0.0)) return 0.0;
    return n - lambda_um * sellmeierIndexSlope(terms, lambda_um);
}

double sellmeierLongWaveIndex(const std::vector<SellmeierTerm> &terms)
{
    double n2 = 1.0;
    for (const SellmeierTerm &t : terms) n2 += t.b;
    return (n2 > 0.0) ? std::sqrt(n2) : 0.0;
}

Complex indexFromPermittivity(const Complex &eps)
{
    // n + ik = √ε (時間因子 exp(−iωt) なので k ≥ 0 の枝を採る)
    const double mag = std::sqrt(eps.re * eps.re + eps.im * eps.im);
    Complex out;
    out.re = std::sqrt(std::max(0.0, 0.5 * (mag + eps.re)));
    out.im = std::sqrt(std::max(0.0, 0.5 * (mag - eps.re)));
    if (eps.im < 0.0) out.im = -out.im;                 // 利得側はそのまま返す
    return out;
}

double angularFrequency(double lambda_um)
{
    if (!(lambda_um > 0.0)) return 0.0;
    return 2.0 * kPi * kC0 / (lambda_um * 1e-6);
}

} // namespace disp
} // namespace ofd
