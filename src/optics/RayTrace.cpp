// RayTrace.cpp — 仕様は RayTrace.h
#include "RayTrace.h"

#include <algorithm>
#include <cmath>

namespace ofd {
namespace raytrace {

namespace {

const double kPi = 3.14159265358979323846;

struct Vec {
    double x = 0.0, y = 0.0, z = 0.0;
};

inline double dot(const Vec &a, const Vec &b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

// 面 (曲率 c / コーニック k) と光線の交点までの距離。
// **呼ぶ前に光線を頂点の接平面 (z = 0) まで運んでおくこと** — 2 根のうち
// どちらが「頂点まわりのキャップ」かは、接平面上の点から解いて初めて
// 一意に決まる (遠くの点から解くと、凹面で球の反対側を掴む)。
// 解は d = A/(B + √(B²−C·A)) — 平面 (c = 0) で d = 0 へ連続に移る形。
// 交わらなければ false。
bool intersect(double c, double k, const Vec &p, const Vec &d, double *out)
{
    const double k1 = 1.0 + k;
    const double A = c * (p.x * p.x + p.y * p.y + k1 * p.z * p.z) - 2.0 * p.z;
    const double B = d.z - c * (p.x * d.x + p.y * d.y + k1 * p.z * d.z);
    const double C = c * (d.x * d.x + d.y * d.y + k1 * d.z * d.z);
    const double disc = B * B - C * A;
    if (!(disc >= 0.0)) return false;
    const double root = std::sqrt(disc);
    const double den = B + root;
    if (std::fabs(den) < 1e-300) return false;
    *out = A / den;
    return std::isfinite(*out);
}

// 交点における前方向きの単位法線
Vec normalAt(double c, double k, const Vec &p)
{
    Vec n;
    n.x = -c * p.x;
    n.y = -c * p.y;
    n.z = 1.0 - c * (1.0 + k) * p.z;
    const double len = std::sqrt(dot(n, n));
    if (len > 0.0) { n.x /= len; n.y /= len; n.z /= len; }
    return n;
}

} // namespace

bool System::isValid() const
{
    if (surfaces.empty()) return false;
    if (!(nObject > 0.0)) return false;
    for (const Surface &s : surfaces)
        if (!(s.nAfter > 0.0)) return false;
    return true;
}

std::vector<Surface> fromParaxial(const std::vector<paraxial::Surface> &s)
{
    std::vector<Surface> out;
    out.reserve(s.size());
    for (const paraxial::Surface &p : s) {
        Surface q;
        q.R = p.R;
        q.thickness = p.thickness;
        q.nAfter = p.nAfter;
        q.semiD = p.semiD;
        q.stop = p.stop;
        out.push_back(q);
    }
    return out;
}

Pupil entrancePupil(const System &sys, double epd)
{
    Pupil p;
    if (!sys.isValid()) return p;

    int stopIndex = -1;
    for (std::size_t i = 0; i < sys.surfaces.size(); ++i)
        if (sys.surfaces[i].stop) { stopIndex = int(i); break; }
    if (stopIndex < 0) stopIndex = 0;      // 指定が無ければ第 1 面が絞り
    p.stopIndex = stopIndex;

    double semi = (epd > 0.0) ? 0.5 * epd : sys.surfaces[stopIndex].semiD;
    if (!(semi > 0.0)) semi = 1.0;
    double z = 0.0;      // 絞り面の頂点から測った位置 (絞りそのものは 0)

    // 絞りより前の面で、順に物体空間へ結像していく (右から左へ)
    for (int j = stopIndex - 1; j >= 0; --j) {
        const Surface &s = sys.surfaces[j];
        const double sPrime = s.thickness + z;      // 面 j から見た像位置
        const double nAfter = s.nAfter;
        const double nBefore = (j == 0) ? sys.nObject : sys.surfaces[j - 1].nAfter;
        const double power = (s.R != 0.0) ? (nAfter - nBefore) / s.R : 0.0;
        if (std::fabs(sPrime) < 1e-12) return p;     // 面に密着 = 結像できない
        const double inv = nAfter / sPrime - power;
        if (std::fabs(inv) < 1e-12) return p;        // 物体側で無限遠 (瞳が作れない)
        const double sObj = nBefore / inv;
        const double m = (nBefore * sPrime) / (nAfter * sObj);
        semi *= std::fabs(m);
        z = sObj;
    }
    p.z = z;
    p.semiD = semi;
    p.valid = true;
    return p;
}

RayResult traceRay(const System &sys, double epd, double field_deg,
                   double px, double py)
{
    RayResult r;
    if (!sys.isValid()) return r;
    const Pupil pup = entrancePupil(sys, epd);
    if (!pup.valid) return r;

    // 入射瞳上の狙い点
    const Vec target{ pup.semiD * px, pup.semiD * py, pup.z };

    Vec p, d;
    if (sys.objectDistance > 0.0) {
        // 有限物体: 物体点 → 瞳点
        const Vec o{ 0.0, sys.objectHeight, -sys.objectDistance };
        d.x = target.x - o.x;
        d.y = target.y - o.y;
        d.z = target.z - o.z;
        const double len = std::sqrt(dot(d, d));
        if (!(len > 0.0)) return r;
        d.x /= len; d.y /= len; d.z /= len;
        p = o;
    } else {
        // 無限遠物体: 視野角の平行光を瞳点へ通す
        const double th = field_deg * kPi / 180.0;
        d.x = 0.0;
        d.y = std::sin(th);
        d.z = std::cos(th);
        if (!(d.z > 0.0)) return r;                 // 90° 以上は追跡しない
        const double zStart = std::min(0.0, pup.z) - 10.0 - std::fabs(pup.semiD);
        const double back = (target.z - zStart) / d.z;
        p.x = target.x - d.x * back;
        p.y = target.y - d.y * back;
        p.z = zStart;
    }

    double nBefore = sys.nObject;
    double opl = 0.0;
    double maxInc = 0.0;
    for (std::size_t i = 0; i < sys.surfaces.size(); ++i) {
        const Surface &s = sys.surfaces[i];
        const double c = (s.R != 0.0) ? 1.0 / s.R : 0.0;
        if (!(std::fabs(d.z) > 1e-12)) {
            r.status = Status::Missed;
            r.failedSurface = int(i);
            return r;
        }
        // 頂点の接平面へ運んでから交点を解く (根の取り違えを防ぐ)
        const double dTan = -p.z / d.z;
        p.x += d.x * dTan;
        p.y += d.y * dTan;
        p.z = 0.0;
        double dist = 0.0;
        if (!intersect(c, s.conic, p, d, &dist)) {
            r.status = Status::Missed;
            r.failedSurface = int(i);
            return r;
        }
        p.x += d.x * dist;
        p.y += d.y * dist;
        p.z += d.z * dist;
        opl += nBefore * (dTan + dist);

        if (s.semiD > 0.0) {
            const double rad = std::sqrt(p.x * p.x + p.y * p.y);
            if (rad > s.semiD * (1.0 + 1e-12)) {
                r.status = Status::Vignetted;
                r.failedSurface = int(i);
                return r;
            }
        }

        // ベクトル形のスネルの法則
        const Vec n = normalAt(c, s.conic, p);
        double cosI = dot(d, n);
        Vec nn = n;
        if (cosI < 0.0) { nn.x = -n.x; nn.y = -n.y; nn.z = -n.z; cosI = -cosI; }
        const double mu = nBefore / s.nAfter;
        const double rad2 = 1.0 - mu * mu * (1.0 - cosI * cosI);
        if (!(rad2 >= 0.0)) {
            r.status = Status::TotalReflect;
            r.failedSurface = int(i);
            return r;
        }
        const double cosT = std::sqrt(rad2);
        const double f = cosT - mu * cosI;
        d.x = mu * d.x + f * nn.x;
        d.y = mu * d.y + f * nn.y;
        d.z = mu * d.z + f * nn.z;
        maxInc = std::max(maxInc, std::acos(std::min(1.0, cosI)) * 180.0 / kPi);

        nBefore = s.nAfter;
        p.z -= s.thickness;      // 次の面の頂点を原点にする
    }

    // 像面へ (最終面の頂点から imageDistance。上のループで最終面の
    // thickness を引いてあるので、残りは imageDistance − thickness ではなく
    // 「最終面の厚さを引いた座標系」での位置になる)
    const double lastT = sys.surfaces.back().thickness;
    const double zImage = sys.imageDistance - lastT;
    if (!(std::fabs(d.z) > 0.0)) { r.status = Status::Missed; return r; }
    const double dImg = (zImage - p.z) / d.z;
    p.x += d.x * dImg;
    p.y += d.y * dImg;
    opl += nBefore * dImg;

    r.status = Status::Ok;
    r.x = p.x;
    r.y = p.y;
    r.opl = opl;
    r.maxIncidenceDeg = maxInc;
    return r;
}

SpotResult spotDiagram(const System &sys, double epd, double field_deg,
                       int rings)
{
    SpotResult out;
    if (rings < 1) rings = 1;
    if (!sys.isValid()) return out;

    const RayResult chief = traceRay(sys, epd, field_deg, 0.0, 0.0);
    if (!chief.ok()) return out;
    out.chiefX = chief.x;
    out.chiefY = chief.y;

    // 六方格子 (中心 + 第 i リングに 6i 本)
    for (int ring = 0; ring <= rings; ++ring) {
        const int count = (ring == 0) ? 1 : 6 * ring;
        const double rho = double(ring) / double(rings);
        for (int k = 0; k < count; ++k) {
            const double a = 2.0 * kPi * double(k) / double(count);
            const double px = rho * std::cos(a);
            const double py = rho * std::sin(a);
            const RayResult rr = traceRay(sys, epd, field_deg, px, py);
            if (!rr.ok()) { ++out.failed; continue; }
            out.x.push_back(rr.x);
            out.y.push_back(rr.y);
            ++out.traced;
        }
    }
    if (out.traced <= 0) return out;

    double sx = 0.0, sy = 0.0;
    for (int i = 0; i < out.traced; ++i) { sx += out.x[i]; sy += out.y[i]; }
    out.centroidX = sx / out.traced;
    out.centroidY = sy / out.traced;
    double s2 = 0.0, geo = 0.0;
    for (int i = 0; i < out.traced; ++i) {
        const double dx = out.x[i] - out.centroidX;
        const double dy = out.y[i] - out.centroidY;
        const double r2 = dx * dx + dy * dy;
        s2 += r2;
        geo = std::max(geo, r2);
    }
    out.rmsRadius = std::sqrt(s2 / out.traced);
    out.geoRadius = std::sqrt(geo);
    out.valid = true;
    return out;
}

FanResult rayFan(const System &sys, double epd, double field_deg, int samples)
{
    FanResult out;
    if (samples < 1) samples = 1;
    if (!sys.isValid()) return out;

    const RayResult chief = traceRay(sys, epd, field_deg, 0.0, 0.0);
    if (!chief.ok()) return out;

    for (int i = -samples; i <= samples; ++i) {
        const double rho = double(i) / double(samples);
        FanPoint t, s;
        t.pupil = rho;
        s.pupil = rho;
        const RayResult rt = traceRay(sys, epd, field_deg, 0.0, rho);
        if (rt.ok()) {
            t.ok = true;
            t.dx = rt.x - chief.x;
            t.dy = rt.y - chief.y;
        }
        const RayResult rs = traceRay(sys, epd, field_deg, rho, 0.0);
        if (rs.ok()) {
            s.ok = true;
            s.dx = rs.x - chief.x;
            s.dy = rs.y - chief.y;
        }
        out.tangential.push_back(t);
        out.sagittal.push_back(s);
    }
    out.valid = true;
    return out;
}

} // namespace raytrace
} // namespace ofd
