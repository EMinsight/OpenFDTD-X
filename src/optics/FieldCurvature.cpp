// FieldCurvature.cpp
#include "FieldCurvature.h"

#include <cmath>

namespace ofd {
namespace optics {

double crossingZ(double a0, double b0, double aD, double bD, double dz,
                 bool *ok)
{
    if (ok) *ok = false;
    if (!(std::fabs(dz) > 0.0)) return 0.0;
    // 2 直線 a(z), b(z) が交わる z (像空間では光線は直線なので厳密)
    const double da = aD - a0;      // dz 進む間の変化
    const double db = bD - b0;
    const double den = da - db;
    if (!(std::fabs(den) > 0.0)) return 0.0;    // 平行 = 交わらない
    const double z = dz * (b0 - a0) / den;
    if (!std::isfinite(z)) return 0.0;
    if (ok) *ok = true;
    return z;
}

FieldCurvatureResult fieldCurvature(PairTracer tracer, void *user,
                                    double halfField_deg, int points,
                                    double dz_mm)
{
    FieldCurvatureResult r;
    if (!tracer || !(halfField_deg > 0.0) || points < 2
        || !(std::fabs(dz_mm) > 0.0))
        return r;

    double worst = 0.0;
    for (int i = 0; i < points; ++i) {
        const double th = halfField_deg * static_cast<double>(i)
                                        / static_cast<double>(points - 1);
        double at0[4] = { 0, 0, 0, 0 }, atD[4] = { 0, 0, 0, 0 };
        if (!tracer(th, 0.0, at0, user)) continue;
        if (!tracer(th, dz_mm, atD, user)) continue;

        FieldCurvaturePoint p;
        p.field_deg = th;
        p.tangential_mm = crossingZ(at0[0], at0[1], atD[0], atD[1], dz_mm,
                                    &p.tangentialOk);
        p.sagittal_mm = crossingZ(at0[2], at0[3], atD[2], atD[3], dz_mm,
                                  &p.sagittalOk);
        if (p.sagittalOk && p.tangentialOk) {
            const double ast = std::fabs(p.tangential_mm - p.sagittal_mm);
            if (ast > worst) worst = ast;
        }
        r.points.push_back(p);
    }
    r.maxAstigmatism_mm = worst;
    return r;
}

} // namespace optics
} // namespace ofd
