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
        // Z = R + j(ωL − 1/ωC) — 不在の素子は短絡 (0 Ω)。
        // R も hasR で切る: 負値をそのまま二乗すると「絶対値ぶんの抵抗が
        // ある」ことになり、表が「—」なのに |Z| だけ増える食い違いになる
        const double r = hasR ? m.r_ohm : 0.0;
        const double x = z.xL_ohm - z.xC_ohm;
        z.resistance_ohm = r;
        z.reactance_ohm = x;
        z.magnitude_ohm = std::sqrt(r * r + x * x);
    } else {
        // Y = G + j(ωC − 1/ωL) — 不在の素子はアドミタンス 0 (開放)
        const double g = hasR ? 1.0 / m.r_ohm : 0.0;
        const double bC = hasC ? w * m.c_F : 0.0;
        const double bL = hasL ? 1.0 / (w * m.l_H) : 0.0;
        const double b = bC - bL;
        const double y2 = g * g + b * b;
        // 無損失並列 LC の共振点は |Y| = 0 = 開放。0 Ω (短絡) と偽らない
        if (y2 <= 0) return z;
        // Z = 1/Y = (G − jB)/|Y|²
        z.resistance_ohm = g / y2;
        z.reactance_ohm = -b / y2;
        z.magnitude_ohm = 1.0 / std::sqrt(y2);
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
