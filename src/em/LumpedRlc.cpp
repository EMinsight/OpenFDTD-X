// LumpedRlc.cpp — 集中定数 RLC のインピーダンス (実装)
#include "LumpedRlc.h"

#include <cmath>

namespace ofd {
namespace em {

namespace {
const double kPi = 3.14159265358979323846;   // MSVC 対策 (M_PI を使わない)
}

RlcImpedance rlcImpedance(const RlcModel &m, double f_Hz)
{
    RlcImpedance z;
    if (f_Hz <= 0) return z;
    const bool hasR = m.r_ohm > 0;
    const bool hasL = m.l_H > 0;
    const bool hasC = m.c_F > 0;
    if (!hasR && !hasL && !hasC) return z;

    const double w = 2.0 * kPi * f_Hz;
    z.xL_ohm = hasL ? w * m.l_H : 0.0;
    z.xC_ohm = hasC ? 1.0 / (w * m.c_F) : 0.0;

    if (m.topology == RlcTopology::Series) {
        // Z = R + j(ωL − 1/ωC) — 不在の素子は短絡 (0 Ω)
        const double x = z.xL_ohm - z.xC_ohm;
        z.magnitude_ohm = std::sqrt(m.r_ohm * m.r_ohm + x * x);
    } else {
        // Y = G + j(ωC − 1/ωL) — 不在の素子はアドミタンス 0 (開放)
        const double g = hasR ? 1.0 / m.r_ohm : 0.0;
        const double bC = hasC ? w * m.c_F : 0.0;
        const double bL = hasL ? 1.0 / (w * m.l_H) : 0.0;
        const double b = bC - bL;
        const double y = std::sqrt(g * g + b * b);
        z.magnitude_ohm = (y > 0) ? 1.0 / y : 0.0;
    }
    z.valid = true;
    return z;
}

double rlcResonanceHz(double l_H, double c_F)
{
    if (l_H <= 0 || c_F <= 0) return 0.0;
    return 1.0 / (2.0 * kPi * std::sqrt(l_H * c_F));
}

} // namespace em
} // namespace ofd
