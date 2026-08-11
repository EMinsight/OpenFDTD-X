// IlluminationTrace.cpp — 非順次モンテカルロ・レイトレーサ (詳細は .h)
#include "IlluminationTrace.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace ofd {
namespace illum {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kEps = 1.0e-9;      // 交点の自己ヒット回避 [mm]
constexpr int    kMaxBounce = 64;    // これを超えたら打ち切って吸収に計上

// ── 3 次元ベクトル (この翻訳単位だけの最小限) ──────────────────────────────
struct V3 {
    double x = 0.0, y = 0.0, z = 0.0;
};
inline V3 operator+(const V3 &a, const V3 &b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
inline V3 operator-(const V3 &a, const V3 &b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
inline V3 operator*(const V3 &a, double s) { return { a.x * s, a.y * s, a.z * s }; }
inline double dot(const V3 &a, const V3 &b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3 normalize(const V3 &a)
{
    const double n = std::sqrt(dot(a, a));
    return (n > 0.0) ? V3{ a.x / n, a.y / n, a.z / n } : V3{ 0.0, 0.0, 1.0 };
}

// n̂ に直交する正規直交基底 (Duff et al. の分岐なし版と同じ考え方だが、
// ここは可読性を優先して大きい成分を避ける古典的な作り方にしてある)
void basisFrom(const V3 &n, V3 *t1, V3 *t2)
{
    const V3 a = (std::fabs(n.z) < 0.9) ? V3{ 0.0, 0.0, 1.0 } : V3{ 1.0, 0.0, 0.0 };
    const V3 u = { a.y * n.z - a.z * n.y, a.z * n.x - a.x * n.z, a.x * n.y - a.y * n.x };
    *t1 = normalize(u);
    *t2 = { n.y * t1->z - n.z * t1->y,
            n.z * t1->x - n.x * t1->z,
            n.x * t1->y - n.y * t1->x };
}

// ── 準乱数 / ハッシュ乱数 (状態を持たない = 再現性が保証される) ────────────
double halton(long long index, int base)
{
    double f = 1.0 / base, r = 0.0;
    long long n = index + 1;   // 0 番目が 0.0 にならないよう +1
    while (n > 0) {
        r += f * static_cast<double>(n % base);
        n /= base;
        f /= base;
    }
    return r;
}

inline uint64_t splitmix64(uint64_t &x)
{
    x += 0x9E3779B97F4A7C15ull;
    uint64_t z = x;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

inline double u01(uint64_t &s)
{
    return static_cast<double>(splitmix64(s) >> 11) * (1.0 / 9007199254740992.0);
}

// n̂ まわりの cos 分布 (ランバート)
V3 cosineAbout(const V3 &n, double u1, double u2)
{
    V3 t1, t2;
    basisFrom(n, &t1, &t2);
    const double r = std::sqrt(u1);
    const double phi = 2.0 * kPi * u2;
    const double z = std::sqrt(std::max(0.0, 1.0 - u1));
    return normalize(t1 * (r * std::cos(phi)) + t2 * (r * std::sin(phi)) + n * z);
}

// ── 系の幾何 ───────────────────────────────────────────────────────────────

// 回転放物面 x²+y² = 4f(z+f) と光線 O+tD の最近接交点 (ρ ≤ R)。
// 見つからなければ false。
bool hitParaboloid(const V3 &o, const V3 &d, double f, double R,
                   double tMin, double *tOut)
{
    const double a = d.x * d.x + d.y * d.y;
    const double b = 2.0 * (o.x * d.x + o.y * d.y) - 4.0 * f * d.z;
    const double c = o.x * o.x + o.y * o.y - 4.0 * f * o.z - 4.0 * f * f;

    double t[2] = { -1.0, -1.0 };
    if (std::fabs(a) < 1.0e-14) {
        if (std::fabs(b) < 1.0e-14) return false;
        t[0] = -c / b;
    } else {
        const double disc = b * b - 4.0 * a * c;
        if (disc < 0.0) return false;
        const double sq = std::sqrt(disc);
        // 桁落ちを避ける古典的な解き方
        const double q = -0.5 * (b + ((b >= 0.0) ? sq : -sq));
        t[0] = q / a;
        t[1] = (std::fabs(q) > 0.0) ? c / q : -1.0;
        if (t[0] > t[1]) std::swap(t[0], t[1]);
    }
    for (int i = 0; i < 2; ++i) {
        if (t[i] <= tMin) continue;
        const double x = o.x + t[i] * d.x;
        const double y = o.y + t[i] * d.y;
        if (x * x + y * y > R * R) continue;
        *tOut = t[i];
        return true;
    }
    return false;
}

// 放物面の内向き法線 (キャビティの内側 = +z 側を向く)
V3 paraboloidNormal(const V3 &p, double f)
{
    return normalize(V3{ -p.x, -p.y, 2.0 * f });
}

// z = zPlane の円板 (半径 Rd) との交点
bool hitDisk(const V3 &o, const V3 &d, double zPlane, double Rd,
             double tMin, double *tOut)
{
    if (std::fabs(d.z) < 1.0e-14) return false;
    const double t = (zPlane - o.z) / d.z;
    if (t <= tMin) return false;
    const double x = o.x + t * d.x;
    const double y = o.y + t * d.y;
    if (x * x + y * y > Rd * Rd) return false;
    *tOut = t;
    return true;
}

// 散乱方向を決める。scatterOk = false は地平線を越えた (= 切り捨て) 場合。
// n は「出ていく側」の法線 (反射なら面法線、透過なら進行方向側の法線)。
V3 scatterDirection(Scatter model, const AbgSampler &abg,
                    const V3 &dIn, const V3 &n, const V3 &specular,
                    uint64_t &rng, bool *scatterOk)
{
    *scatterOk = true;
    switch (model) {
    case Scatter::Specular:
        return specular;
    case Scatter::Lambertian: {
        const double u1 = u01(rng), u2 = u01(rng);
        return cosineAbout(n, u1, u2);
    }
    case Scatter::ABG:
    default: {
        if (!abg.valid()) return specular;
        V3 t1, t2;
        basisFrom(n, &t1, &t2);
        const double bx = dot(specular, t1);
        const double by = dot(specular, t2);
        const double db = abg.sample(u01(rng));
        const double ph = 2.0 * kPi * u01(rng);
        const double nx = bx + db * std::cos(ph);
        const double ny = by + db * std::sin(ph);
        const double s = nx * nx + ny * ny;
        if (s >= 1.0) { *scatterOk = false; return specular; }
        const double nz = std::sqrt(1.0 - s);
        (void)dIn;
        return normalize(t1 * nx + t2 * ny + n * nz);
    }
    }
}

} // namespace

// ── ABG サンプラ ───────────────────────────────────────────────────────────
// p(Δβ) ∝ Δβ·A/(B + Δβ^g) を台形則で積分して累積分布の数表を作る。
AbgSampler::AbgSampler(const AbgParams &p, double dbetaMax, int nodes)
{
    if (!(p.A > 0.0) || !(p.B > 0.0) || !(p.g > 0.0) || !(dbetaMax > 0.0)
        || nodes < 8)
        return;

    m_max = dbetaMax;
    m_step = dbetaMax / nodes;
    m_cdf.assign(static_cast<size_t>(nodes) + 1, 0.0);

    auto pdf = [&p](double d) {
        return d * p.A / (p.B + std::pow(d, p.g));
    };
    // 区間ごとの Simpson 則。B が小さいと pdf が Δβ ≈ √B に鋭い山を作るので、
    // 台形則では節点間隔が山幅に対して粗くなり数 % ずれる (g = 2 の閉形式と
    // 比べると見える)。Simpson なら同じ節点数で誤差が 4 次で落ちる。
    double acc = 0.0;
    double prev = pdf(0.0);
    for (int i = 1; i <= nodes; ++i) {
        const double d = m_step * i;
        const double cur = pdf(d);
        const double mid = pdf(d - 0.5 * m_step);
        acc += (m_step / 6.0) * (prev + 4.0 * mid + cur);
        m_cdf[static_cast<size_t>(i)] = acc;
        prev = cur;
    }
    if (!(acc > 0.0)) { m_cdf.clear(); return; }
    for (double &v : m_cdf) v /= acc;
    m_cdf.back() = 1.0;
    m_valid = true;
}

double AbgSampler::cdf(double dbeta) const
{
    if (!m_valid) return 0.0;
    if (dbeta <= 0.0) return 0.0;
    if (dbeta >= m_max) return 1.0;
    const double x = dbeta / m_step;
    const size_t i = static_cast<size_t>(x);
    if (i + 1 >= m_cdf.size()) return 1.0;
    const double f = x - static_cast<double>(i);
    return m_cdf[i] * (1.0 - f) + m_cdf[i + 1] * f;
}

double AbgSampler::sample(double u) const
{
    if (!m_valid) return 0.0;
    if (u <= 0.0) return 0.0;
    if (u >= 1.0) return m_max;
    const auto it = std::lower_bound(m_cdf.begin(), m_cdf.end(), u);
    const size_t hi = static_cast<size_t>(it - m_cdf.begin());
    if (hi == 0) return 0.0;
    const size_t lo = hi - 1;
    const double f0 = m_cdf[lo], f1 = m_cdf[hi];
    const double t = (f1 > f0) ? (u - f0) / (f1 - f0) : 0.0;
    return m_step * (static_cast<double>(lo) + t);
}

// ── 追跡が成り立つかの判定 ─────────────────────────────────────────────────
const char *traceBlocker(const Scene &s, long long nRays)
{
    if (nRays <= 0) return "rays";
    if (!(s.source.flux_lm > 0.0)) return "flux";
    if (s.source.kind == Source::Chip && !(s.source.size_mm > 0.0)) return "flux";
    if (s.reflector.enabled) {
        if (!(s.reflector.focal_mm > 0.0)) return "focal";
        // R ≤ 2f では開口の縁が焦点より下に来て、光線を 1 本も捕まえられない
        if (!(s.reflector.radius_mm > 2.0 * s.reflector.focal_mm)) return "radius";
    }
    if (s.diffuser.enabled && !(s.diffuser.radius_mm > 0.0)) return "radius";
    if (s.target.cells < 3) return "cells";

    double zMax = 0.0;
    if (s.reflector.enabled) {
        const double f = s.reflector.focal_mm, R = s.reflector.radius_mm;
        zMax = std::max(zMax, R * R / (4.0 * f) - f);
    }
    if (s.diffuser.enabled) zMax = std::max(zMax, s.diffuser.z_mm);
    if (!(s.target.distance_mm > zMax) || !(s.target.half_mm > 0.0)) return "target";
    return nullptr;
}

// ── 本体 ───────────────────────────────────────────────────────────────────
Result trace(const Scene &s, long long nRays)
{
    Result r;
    if (traceBlocker(s, nRays) != nullptr) return r;

    const int bins = std::max(2, s.angleBins);
    const int cells = (s.target.cells % 2 == 0) ? s.target.cells + 1 : s.target.cells;
    r.cells = cells;
    r.intensity_cd.assign(static_cast<size_t>(bins), 0.0);
    r.illuminance_lx.assign(static_cast<size_t>(cells) * cells, 0.0);

    std::vector<double> binFlux(static_cast<size_t>(bins), 0.0);
    std::vector<double> cellFlux(static_cast<size_t>(cells) * cells, 0.0);

    const AbgSampler abgR(s.reflector.abg);
    const AbgSampler abgD(s.diffuser.abg);

    const double w0 = s.source.flux_lm / static_cast<double>(nRays);
    const double f = s.reflector.focal_mm;
    const double R = s.reflector.radius_mm;
    const double rho = std::min(1.0, std::max(0.0, s.reflector.reflectance));
    const double tau = std::min(1.0, std::max(0.0, s.diffuser.transmittance));

    double absorbed = 0.0, out = 0.0, onTarget = 0.0;

    for (long long i = 0; i < nRays; ++i) {
        // 放射: 準乱数で cos 分布 (+z 半球)。位置はチップ面上で一様。
        const double u1 = halton(i, 2), u2 = halton(i, 3);
        const double sinT = std::sqrt(u1);
        const double cosT = std::sqrt(std::max(0.0, 1.0 - u1));
        const double phi = 2.0 * kPi * u2;
        V3 d{ sinT * std::cos(phi), sinT * std::sin(phi), cosT };

        V3 o{ 0.0, 0.0, 0.0 };
        if (s.source.kind == Source::Chip) {
            const double h = 0.5 * s.source.size_mm;
            o.x = (2.0 * halton(i, 5) - 1.0) * h;
            o.y = (2.0 * halton(i, 7) - 1.0) * h;
        }

        uint64_t rng = static_cast<uint64_t>(i) * 0x2545F4914F6CDD1Dull
                     + 0x9E3779B97F4A7C15ull;
        double w = w0;

        int bounce = 0;
        for (;; ++bounce) {
            if (bounce >= kMaxBounce) {
                absorbed += w;
                ++r.raysTrapped;
                w = 0.0;
                break;
            }

            double tRef = 0.0, tDif = 0.0;
            const bool hRef = s.reflector.enabled
                            && hitParaboloid(o, d, f, R, kEps, &tRef);
            const bool hDif = s.diffuser.enabled
                            && hitDisk(o, d, s.diffuser.z_mm,
                                       s.diffuser.radius_mm, kEps, &tDif);

            const bool takeRef = hRef && (!hDif || tRef <= tDif);
            const bool takeDif = hDif && (!hRef || tDif < tRef);

            if (!takeRef && !takeDif) break;   // 系の外へ出た

            if (takeRef) {
                const V3 p = o + d * tRef;
                const V3 n = paraboloidNormal(p, f);
                absorbed += w * (1.0 - rho);
                w *= rho;
                if (w <= w0 * 1.0e-9) { absorbed += w; w = 0.0; break; }
                const V3 spec = normalize(d - n * (2.0 * dot(d, n)));
                bool ok = true;
                const V3 nd = scatterDirection(s.reflector.model, abgR,
                                               d, n, spec, rng, &ok);
                if (!ok) { absorbed += w; w = 0.0; break; }
                o = p;
                d = nd;
            } else {
                const V3 p = o + d * tDif;
                const V3 n = (d.z > 0.0) ? V3{ 0.0, 0.0, 1.0 } : V3{ 0.0, 0.0, -1.0 };
                absorbed += w * (1.0 - tau);
                w *= tau;
                if (w <= w0 * 1.0e-9) { absorbed += w; w = 0.0; break; }
                bool ok = true;
                // 透過なので鏡面方向 = 進行方向のまま
                const V3 nd = scatterDirection(s.diffuser.model, abgD,
                                               d, n, d, rng, &ok);
                if (!ok) { absorbed += w; w = 0.0; break; }
                o = p;
                d = nd;
            }
        }

        if (w <= 0.0) continue;

        out += w;

        // 遠方界 (軸対称を仮定して θ ビンへ)
        const double ct = std::min(1.0, std::max(-1.0, d.z));
        const double th = std::acos(ct);
        int k = static_cast<int>(th / kPi * bins);
        k = std::min(bins - 1, std::max(0, k));
        binFlux[static_cast<size_t>(k)] += w;

        // 評価面
        if (d.z > 0.0 && o.z < s.target.distance_mm) {
            const double t = (s.target.distance_mm - o.z) / d.z;
            const double x = o.x + t * d.x;
            const double y = o.y + t * d.y;
            const double W = s.target.half_mm;
            if (std::fabs(x) <= W && std::fabs(y) <= W) {
                int cx = static_cast<int>((x + W) / (2.0 * W) * cells);
                int cy = static_cast<int>((y + W) / (2.0 * W) * cells);
                cx = std::min(cells - 1, std::max(0, cx));
                cy = std::min(cells - 1, std::max(0, cy));
                cellFlux[static_cast<size_t>(cy) * cells + cx] += w;
                onTarget += w;
                ++r.raysOnTarget;
            }
        }
    }

    // ── 集計 ───────────────────────────────────────────────────────────────
    r.rays = nRays;
    r.fluxIn_lm = s.source.flux_lm;
    r.fluxOut_lm = out;
    r.fluxAbsorbed_lm = absorbed;
    r.fluxTarget_lm = onTarget;
    r.efficiency = out / s.source.flux_lm;
    r.targetEfficiency = onTarget / s.source.flux_lm;

    // 強度 [cd] = ビンの光束 / ビンの立体角
    for (int k = 0; k < bins; ++k) {
        const double a0 = kPi * k / bins, a1 = kPi * (k + 1) / bins;
        const double omega = 2.0 * kPi * (std::cos(a0) - std::cos(a1));
        r.intensity_cd[static_cast<size_t>(k)] =
            (omega > 0.0) ? binFlux[static_cast<size_t>(k)] / omega : 0.0;
    }
    r.axialIntensity_cd = r.intensity_cd[0];

    // ビーム角 (FWHM): 中心強度の半分へ落ちる θ をビン中心間で線形補間
    if (r.axialIntensity_cd > 0.0) {
        const double half = 0.5 * r.axialIntensity_cd;
        const double dth = 180.0 / bins;
        for (int k = 1; k < bins; ++k) {
            const double a = r.intensity_cd[static_cast<size_t>(k - 1)];
            const double b = r.intensity_cd[static_cast<size_t>(k)];
            if (a >= half && b < half) {
                const double t = (a > b) ? (a - half) / (a - b) : 0.0;
                const double thc = dth * ((k - 1) + 0.5 + t);
                r.beamAngleFwhm_deg = 2.0 * thc;
                r.beamValid = true;
                break;
            }
        }
    }

    // 照度 [lx] = セルの光束 [lm] / セル面積 [m²]
    const double cw = 2.0 * s.target.half_mm / cells;     // [mm]
    const double area_m2 = (cw * 1.0e-3) * (cw * 1.0e-3);
    double mn = 0.0, mx = 0.0, sum = 0.0;
    for (size_t c = 0; c < cellFlux.size(); ++c) {
        const double e = cellFlux[c] / area_m2;
        r.illuminance_lx[c] = e;
        sum += e;
        if (c == 0) { mn = mx = e; }
        else { mn = std::min(mn, e); mx = std::max(mx, e); }
    }
    const size_t nc = cellFlux.size();
    r.illumAvg_lx = sum / static_cast<double>(nc);
    r.illumMin_lx = mn;
    r.illumMax_lx = mx;
    r.illumCenter_lx = r.illuminance_lx[static_cast<size_t>(cells / 2) * cells
                                       + static_cast<size_t>(cells / 2)];
    r.uniformityMinAvg = (r.illumAvg_lx > 0.0) ? mn / r.illumAvg_lx : 0.0;
    // セルあたり平均 10 本を切ると min/avg は統計誤差の方が大きくなる
    r.uniformityValid = (r.illumAvg_lx > 0.0)
                     && (r.raysOnTarget >= static_cast<long long>(10 * nc));

    r.valid = true;
    return r;
}

} // namespace illum
} // namespace ofd
