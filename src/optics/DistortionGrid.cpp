// DistortionGrid.cpp
#include "DistortionGrid.h"

#include <cmath>

namespace ofd {
namespace optics {

namespace {
const double kPi = 3.14159265358979323846;   // MSVC 対策 (M_PI を使わない)
} // namespace

double distortionPercent(double yReal_mm, double yIdeal_mm)
{
    // 軸上は 0/0 になるので 0 と定義する (回転対称系では θ→0 で D→0)
    if (!(std::fabs(yIdeal_mm) > 0.0)) return 0.0;
    return (yReal_mm - yIdeal_mm) / yIdeal_mm * 100.0;
}

DistortionGridResult distortionGrid(const FieldMapping &realHeight,
                                    double efl_mm, double halfField_deg, int n)
{
    DistortionGridResult g;
    if (!realHeight || !(efl_mm > 0.0) || !(halfField_deg > 0.0) || n < 2)
        return g;
    g.n = n;
    g.nodes.reserve(static_cast<std::size_t>(n) * n);

    // 格子は視野角の正方格子。隅が最大視野になるよう、辺の半分を
    // halfField/√2 にとる (隅の距離が halfField になる)。
    const double half = halfField_deg / std::sqrt(2.0);
    double worst = 0.0;
    for (int j = 0; j < n; ++j) {
        for (int i = 0; i < n; ++i) {
            const double fx = -half + 2.0 * half * i / (n - 1);
            const double fy = -half + 2.0 * half * j / (n - 1);
            const double th = std::sqrt(fx * fx + fy * fy);   // 視野半角 [deg]

            DistortionNode nd;
            nd.field_deg = th;
            const double yi = efl_mm * std::tan(th * kPi / 180.0);
            const double yr = realHeight(th);
            // 方位角は変わらない (回転対称)。半径だけを入れ替える。
            const double c = (th > 0.0) ? fx / th : 0.0;
            const double s = (th > 0.0) ? fy / th : 0.0;
            nd.xIdeal_mm = yi * c;
            nd.yIdeal_mm = yi * s;
            nd.xReal_mm  = yr * c;
            nd.yReal_mm  = yr * s;
            nd.percent = distortionPercent(yr, yi);
            if (std::fabs(nd.percent) > std::fabs(worst)) worst = nd.percent;
            g.nodes.push_back(nd);
        }
    }
    g.maxPercent = worst;
    // 隅 (最初の節点 = (−half, −half)) が最大視野
    g.cornerPercent = g.nodes.empty() ? 0.0 : g.nodes.front().percent;
    return g;
}

} // namespace optics
} // namespace ofd
