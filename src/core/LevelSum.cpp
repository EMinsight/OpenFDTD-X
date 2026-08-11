// LevelSum.cpp — 仕様は LevelSum.h
#include "LevelSum.h"

#include <algorithm>
#include <cmath>

namespace ofd {
namespace levelsum {

Result energySum(const std::vector<double> &levels_db)
{
    Result r;
    if (levels_db.empty()) return r;
    for (double v : levels_db)
        if (!std::isfinite(v)) return r;

    double sum = 0.0;
    std::vector<double> lin(levels_db.size(), 0.0);
    for (std::size_t i = 0; i < levels_db.size(); ++i) {
        lin[i] = std::pow(10.0, levels_db[i] / 10.0);
        sum += lin[i];
    }
    if (!(sum > 0.0) || !std::isfinite(sum)) return r;

    r.total_db = 10.0 * std::log10(sum);
    r.parts.resize(levels_db.size());
    double best = -1.0;
    for (std::size_t i = 0; i < levels_db.size(); ++i) {
        Contribution c;
        c.level_db = levels_db[i];
        c.share = lin[i] / sum;
        // ΔL = −10·log10(1 − p)。1 個しか無ければ全部消えるので
        // 無限大になる — その場合は「合計そのもの」を返さず、
        // 表示側が扱いやすいよう 0 のままにしない: 十分大きな値にする。
        const double rest = 1.0 - c.share;
        c.removalGain_db = (rest > 1e-15) ? (-10.0 * std::log10(rest)) : 1e3;
        r.parts[i] = c;
        if (c.share > best) { best = c.share; r.dominantIndex = int(i); }
    }

    // 上位 2 つのレベル差 (支配の強さの目安)
    if (levels_db.size() >= 2) {
        std::vector<double> sorted = levels_db;
        std::sort(sorted.begin(), sorted.end(), std::greater<double>());
        r.topTwoGap_db = sorted[0] - sorted[1];
    }
    r.valid = true;
    return r;
}

SumResult coherentSum(const std::vector<double> &levels_db)
{
    SumResult r;
    if (levels_db.empty()) return r;
    double amp = 0.0;
    for (double v : levels_db) {
        if (!std::isfinite(v)) return r;
        amp += std::pow(10.0, v / 20.0);
    }
    if (!(amp > 0.0) || !std::isfinite(amp)) return r;
    r.total_db = 20.0 * std::log10(amp);
    r.valid = true;
    return r;
}

double spreadingLoss_db(double distance_m)
{
    if (!(distance_m > 0.0) || !std::isfinite(distance_m)) return 0.0;
    return -20.0 * std::log10(distance_m);
}

} // namespace levelsum
} // namespace ofd
