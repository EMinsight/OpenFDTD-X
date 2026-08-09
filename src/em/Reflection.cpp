// Reflection.cpp — 反射係数とスミスチャートの幾何 (定義は Reflection.h)
#include "Reflection.h"

#include <cmath>
#include <limits>

namespace ofd {
namespace em {

namespace {
const double kPi = 3.14159265358979323846;   // MSVC 対策 (M_PI を使わない)
} // namespace

Reflection reflectionFromZ(double r_ohm, double x_ohm, double z0_ohm)
{
    Reflection out;
    if (!(z0_ohm > 0) || !std::isfinite(r_ohm) || !std::isfinite(x_ohm))
        return out;

    // Γ = (R − Z0 + jX) / (R + Z0 + jX)
    const double nre = r_ohm - z0_ohm, nim = x_ohm;
    const double dre = r_ohm + z0_ohm, dim = x_ohm;
    const double den = dre * dre + dim * dim;
    if (!(den > 0)) return out;               // Z = −Z0 (物理的には起きない)

    out.gammaRe = (nre * dre + nim * dim) / den;
    out.gammaIm = (nim * dre - nre * dim) / den;
    out.magnitude = std::hypot(out.gammaRe, out.gammaIm);
    out.phaseDeg = std::atan2(out.gammaIm, out.gammaRe) * 180.0 / kPi;

    const double inf = std::numeric_limits<double>::infinity();
    if (out.magnitude > 0) {
        out.s11Db = 20.0 * std::log10(out.magnitude);
        out.returnLossDb = -out.s11Db;
    } else {
        out.s11Db = -inf;                     // 完全整合
        out.returnLossDb = inf;
    }
    out.vswr = (out.magnitude < 1.0)
                   ? (1.0 + out.magnitude) / (1.0 - out.magnitude)
                   : inf;                     // 全反射 (受動なら |Γ| = 1)
    out.valid = true;
    return out;
}

bool impedanceFromGamma(double gammaRe, double gammaIm, double z0_ohm,
                        double *r_ohm, double *x_ohm)
{
    if (!(z0_ohm > 0) || !std::isfinite(gammaRe) || !std::isfinite(gammaIm))
        return false;
    // z = (1 + Γ)/(1 − Γ)
    const double nre = 1.0 + gammaRe, nim = gammaIm;
    const double dre = 1.0 - gammaRe, dim = -gammaIm;
    const double den = dre * dre + dim * dim;
    if (!(den > 0)) return false;             // Γ = 1 (開放)
    if (r_ohm) *r_ohm = (nre * dre + nim * dim) / den * z0_ohm;
    if (x_ohm) *x_ohm = (nim * dre - nre * dim) / den * z0_ohm;
    return true;
}

SmithCircle constantResistanceCircle(double rNorm)
{
    SmithCircle c;
    if (!(rNorm >= 0) || !std::isfinite(rNorm)) return c;
    c.cx = rNorm / (1.0 + rNorm);
    c.cy = 0.0;
    c.radius = 1.0 / (1.0 + rNorm);
    c.valid = true;
    return c;
}

SmithCircle constantReactanceCircle(double xNorm)
{
    SmithCircle c;
    if (!std::isfinite(xNorm) || xNorm == 0.0) return c;   // x = 0 は実軸
    c.cx = 1.0;
    c.cy = 1.0 / xNorm;
    c.radius = std::fabs(1.0 / xNorm);
    c.valid = true;
    return c;
}

} // namespace em
} // namespace ofd
