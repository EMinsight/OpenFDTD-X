#include "IlluminationScene.h"

#include "Project.h"

#include <algorithm>

namespace ofd {
namespace illum {

const char *sceneFromOpts(const IlluminationOpts &o, Scene *sc, long long *nRays,
                          long long maxRays)
{
    // 追跡モデルに入っていない選択 — 値を出さずに理由を返す (絶対規則 5)
    if (o.srcModel == 1) return "raydata";      // レイデータ (実測)
    if (o.surface == 2)  return "bsdf";         // BSDF 実測
    if (o.tirLens || o.lightGuide || o.phosphor) return "elem";

    Scatter model = Scatter::Specular;
    if (o.surface == 1)      model = Scatter::Lambertian;
    else if (o.surface == 3) model = Scatter::ABG;

    Scene &s = *sc;
    s.source.kind = (o.srcModel == 2) ? Source::Chip : Source::Point;
    s.source.size_mm = o.chipSize_mm;
    s.source.flux_lm = o.flux_lm;

    s.reflector.enabled = o.reflector;
    s.reflector.focal_mm = o.reflFocal_mm;
    s.reflector.radius_mm = o.reflRadius_mm;
    s.reflector.reflectance = o.reflReflect;
    s.reflector.model = model;
    s.reflector.abg = { o.abgA, o.abgB, o.abgG };

    s.diffuser.enabled = o.diffuser;
    s.diffuser.z_mm = o.diffZ_mm;
    s.diffuser.radius_mm = o.diffRadius_mm;
    s.diffuser.transmittance = o.diffTrans;
    // 拡散板の「鏡面」は素通し (散乱しない透明板) の意味になる
    s.diffuser.model = model;
    s.diffuser.abg = { o.abgA, o.abgB, o.abgG };

    s.target.distance_mm = o.targetDist_mm;
    s.target.half_mm = o.targetHalf_mm;

    const double want = (o.rays > 0.0) ? o.rays : 0.0;
    *nRays = (maxRays > 0)
           ? static_cast<long long>(std::min(want, static_cast<double>(maxRays)))
           : static_cast<long long>(want);

    return traceBlocker(s, *nRays);
}

} // namespace illum
} // namespace ofd
