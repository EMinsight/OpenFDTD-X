// ParaxialTrace.cpp
#include "ParaxialTrace.h"

#include <cmath>

namespace ofd {
namespace paraxial {

namespace {
constexpr double kPi = 3.14159265358979323846;

// y-nu 追跡 1 回ぶん。無限遠物体 (平行光, y=1, u=0) を入れて、
// 最終面での高さ yLast と像空間の角 uLast を返す。
struct TraceOut {
    bool   ok = false;
    double yFirst = 1.0;
    double yLast = 0.0;
    double uLast = 0.0;    // 像空間 (最終面の後ろ) の角度 u'
};

TraceOut traceParallel(const std::vector<Surface> &s)
{
    TraceOut out;
    if (s.empty()) return out;
    double y = 1.0, u = 0.0, n = 1.0;
    for (std::size_t i = 0; i < s.size(); ++i) {
        const double nAfter = (s[i].nAfter > 0.0) ? s[i].nAfter : 1.0;
        const double phi = (s[i].R != 0.0) ? (nAfter - n) / s[i].R : 0.0;
        const double nu = n * u - y * phi;      // n'u'
        u = nu / nAfter;
        n = nAfter;
        if (i + 1 == s.size()) { out.yLast = y; break; }
        y += u * s[i].thickness;
    }
    out.uLast = u;
    out.ok = std::isfinite(y) && std::isfinite(u);
    return out;
}
} // namespace

SystemData analyze(const std::vector<Surface> &surfaces, double imageDistance,
                   double epd, double fieldHalf_deg)
{
    SystemData d;
    if (surfaces.empty()) return d;

    const TraceOut fwd = traceParallel(surfaces);
    if (!fwd.ok || std::fabs(fwd.uLast) < 1e-12) return d;   // アフォーカル

    d.efl = -fwd.yFirst / fwd.uLast;
    d.bfl = -fwd.yLast / fwd.uLast;
    d.backPrincipal = d.bfl - d.efl;

    // 反転系 (光を逆向きに通す) のバックフォーカス = 前側焦点距離
    std::vector<Surface> rev;
    rev.reserve(surfaces.size());
    for (std::size_t j = 0; j < surfaces.size(); ++j) {
        const std::size_t i = surfaces.size() - 1 - j;
        Surface r;
        r.R = -surfaces[i].R;
        // 反転後の「後ろ側」= 元の「前側」の屈折率
        r.nAfter = (i == 0) ? 1.0 : ((surfaces[i - 1].nAfter > 0.0)
                                     ? surfaces[i - 1].nAfter : 1.0);
        r.thickness = (i == 0) ? 0.0 : surfaces[i - 1].thickness;
        rev.push_back(r);
    }
    const TraceOut bwd = traceParallel(rev);
    if (bwd.ok && std::fabs(bwd.uLast) > 1e-12) {
        d.ffl = -bwd.yLast / bwd.uLast;
        d.frontPrincipal = d.ffl - (-bwd.yFirst / bwd.uLast);
    }

    for (std::size_t i = 0; i + 1 < surfaces.size(); ++i)
        d.totalTrack += surfaces[i].thickness;

    if (epd > 0.0) d.fnumber = d.efl / epd;
    if (fieldHalf_deg > 0.0 && fieldHalf_deg < 90.0)
        d.imageHeight = d.efl * std::tan(fieldHalf_deg * kPi / 180.0);
    if (imageDistance >= 0.0) {
        d.hasImagePlane = true;
        d.defocus = d.bfl - imageDistance;
    }
    d.valid = std::isfinite(d.efl) && std::isfinite(d.bfl);
    return d;
}

} // namespace paraxial
} // namespace ofd
