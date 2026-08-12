// EncircledEnergy.cpp
#include "EncircledEnergy.h"

#include <algorithm>
#include <cmath>

namespace ofd {
namespace optics {

namespace {
// 中心からの距離を昇順に並べたもの (両方の関数が使う)
std::vector<double> sortedRadii(const std::vector<double> &x,
                                const std::vector<double> &y,
                                double cx, double cy)
{
    std::vector<double> r;
    const std::size_t n = std::min(x.size(), y.size());
    r.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        const double dx = x[i] - cx, dy = y[i] - cy;
        r.push_back(std::sqrt(dx * dx + dy * dy));
    }
    std::sort(r.begin(), r.end());
    return r;
}
} // namespace

EeCurve encircledEnergy(const std::vector<double> &x_mm,
                        const std::vector<double> &y_mm,
                        double cx_mm, double cy_mm, int points)
{
    EeCurve c;
    const std::vector<double> r = sortedRadii(x_mm, y_mm, cx_mm, cy_mm);
    if (r.empty() || points < 2) return c;
    c.rays = static_cast<int>(r.size());
    c.maxRadius_mm = r.back();

    double sum2 = 0.0;
    for (double v : r) sum2 += v * v;
    c.rmsRadius_mm = std::sqrt(sum2 / static_cast<double>(r.size()));

    // 0 → 最外まで等間隔に刻み、各半径以下の本数を数える。
    // 並べ替え済みなので upper_bound で数えられる (ヒストグラム不要)。
    const double rmax = (r.back() > 0.0) ? r.back() : 1.0;
    c.radius_mm.reserve(points);
    c.fraction.reserve(points);
    for (int i = 0; i < points; ++i) {
        const double rad = rmax * static_cast<double>(i)
                                / static_cast<double>(points - 1);
        const std::size_t k = static_cast<std::size_t>(
            std::upper_bound(r.begin(), r.end(), rad) - r.begin());
        c.radius_mm.push_back(rad);
        c.fraction.push_back(static_cast<double>(k)
                             / static_cast<double>(r.size()));
    }
    return c;
}

double radiusForFraction(const std::vector<double> &x_mm,
                         const std::vector<double> &y_mm,
                         double cx_mm, double cy_mm, double f)
{
    const std::vector<double> r = sortedRadii(x_mm, y_mm, cx_mm, cy_mm);
    if (r.empty() || !(f > 0.0) || f > 1.0) return 0.0;
    // f 以上を包む最小の本数 → その光線の半径 (補間しない)
    const double need = f * static_cast<double>(r.size());
    std::size_t k = static_cast<std::size_t>(std::ceil(need));
    if (k < 1) k = 1;
    if (k > r.size()) k = r.size();
    return r[k - 1];
}

} // namespace optics
} // namespace ofd
