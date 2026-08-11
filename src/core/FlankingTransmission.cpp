// FlankingTransmission.cpp — 側路伝搬の経路合成 (詳細は .h)
#include "FlankingTransmission.h"

#include <cmath>

namespace ofd {
namespace flanking {

Combined combine(const std::vector<Path> &paths)
{
    Combined out;
    double sum = 0.0, sumBase = 0.0;
    double weakest = 0.0;

    for (size_t i = 0; i < paths.size(); ++i) {
        const Path &p = paths[i];
        if (!p.enabled) continue;
        if (!std::isfinite(p.R_dB) || !std::isfinite(p.deltaR_dB)) continue;
        const double r = p.R_dB + p.deltaR_dB;
        sum     += std::pow(10.0, -r / 10.0);
        sumBase += std::pow(10.0, -p.R_dB / 10.0);
        // 合成後に最も弱い (透過率が最大の) 経路 — ここを直さない限り
        // 全体は良くならない、という説明のために持っておく
        if (out.paths == 0 || r < weakest) { weakest = r; out.weakestIndex = int(i); }
        ++out.paths;
    }
    if (out.paths == 0 || !(sum > 0.0) || !(sumBase > 0.0)) return out;

    out.rw_dB = -10.0 * std::log10(sum);
    out.base_dB = -10.0 * std::log10(sumBase);
    out.gain_dB = out.rw_dB - out.base_dB;
    out.valid = true;
    return out;
}

} // namespace flanking
} // namespace ofd
