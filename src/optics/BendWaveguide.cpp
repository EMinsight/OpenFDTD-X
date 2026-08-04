// BendWaveguide.cpp — 共形変換と重なり積分 (出典はヘッダ参照)
#include "optics/BendWaveguide.h"

#include <cmath>

namespace ofd {
namespace optics {

CrossSection bendEquivalent(const CrossSection &cs, double radius_um)
{
    CrossSection out = cs;
    if (!(radius_um > 0.0) || cs.nx <= 0 || cs.ny <= 0) return out;
    for (int ix = 0; ix < cs.nx; ++ix) {
        // makeRectangularCore と同じセル中心座標 (コア中心が x = 0)
        const double x = (ix + 0.5 - 0.5 * cs.nx) * cs.dx_um;
        double f = 1.0 + x / radius_um;
        if (f < 1e-6) f = 1e-6;            // 内周側の破綻を避ける (|x| ≪ R 前提)
        for (int iy = 0; iy < cs.ny; ++iy) {
            const std::size_t i = static_cast<std::size_t>(iy) * cs.nx + ix;
            if (i < out.n.size()) out.n[i] = cs.n[i] * f;
        }
    }
    return out;
}

double conformalRatio(const CrossSection &cs, double radius_um)
{
    if (!(radius_um > 0.0) || cs.nx <= 0) return 0.0;
    const double xEdge = (cs.nx - 1 + 0.5 - 0.5 * cs.nx) * cs.dx_um;
    return std::fabs(xEdge) / radius_um;
}

double overlapEfficiency(const std::vector<double> &a,
                         const std::vector<double> &b)
{
    if (a.empty() || a.size() != b.size()) return 0.0;
    double ab = 0.0, aa = 0.0, bb = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        ab += a[i] * b[i];
        aa += a[i] * a[i];
        bb += b[i] * b[i];
    }
    if (!(aa > 0.0) || !(bb > 0.0)) return 0.0;
    const double eta = (ab * ab) / (aa * bb);
    return (eta > 1.0) ? 1.0 : eta;        // 丸め誤差で 1 を僅かに超えることがある
}

double mismatchLossDb(double efficiency)
{
    if (!(efficiency > 0.0)) return 300.0;
    return -10.0 * std::log10(efficiency);
}

double radiationCaustic(double radius_um, double neff, double nClad)
{
    if (!(radius_um > 0.0) || !(nClad > 0.0) || !(neff > nClad)) return 0.0;
    return radius_um * (neff / nClad - 1.0);
}

} // namespace optics
} // namespace ofd
