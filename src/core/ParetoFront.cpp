#include "ParetoFront.h"

#include <algorithm>
#include <cmath>

namespace ofd {
namespace pareto {

namespace {

// 「大きいほど良い」向きへ揃える (呼び出し側で符号を書かないための一手間)
inline double up(double v, bool maximize) { return maximize ? v : -v; }

} // namespace

bool dominates(const Point &p, const Point &q, bool maxA, bool maxB)
{
    if (!p.valid || !q.valid) return false;
    const double pa = up(p.a, maxA), pb = up(p.b, maxB);
    const double qa = up(q.a, maxA), qb = up(q.b, maxB);
    // 両目的で以上、かつ少なくとも一方で真に良い
    return pa >= qa && pb >= qb && (pa > qa || pb > qb);
}

std::vector<int> front(const std::vector<Point> &pts, bool maxA, bool maxB)
{
    std::vector<int> out;
    for (std::size_t i = 0; i < pts.size(); ++i) {
        if (!pts[i].valid) continue;
        bool beaten = false;
        for (std::size_t j = 0; j < pts.size() && !beaten; ++j) {
            if (j == i) continue;
            if (dominates(pts[j], pts[i], maxA, maxB)) beaten = true;
        }
        if (!beaten) out.push_back(static_cast<int>(i));
    }
    return out;
}

std::vector<int> frontSortedByA(const std::vector<Point> &pts,
                               bool maxA, bool maxB)
{
    std::vector<int> f = front(pts, maxA, maxB);
    std::stable_sort(f.begin(), f.end(), [&](int i, int j) {
        const double ai = up(pts[static_cast<std::size_t>(i)].a, maxA);
        const double aj = up(pts[static_cast<std::size_t>(j)].a, maxA);
        return ai < aj;
    });
    return f;
}

double hypervolume(const std::vector<Point> &pts, bool maxA, bool maxB,
                   double refA, double refB)
{
    const std::vector<int> f = front(pts, maxA, maxB);
    const double ra = up(refA, maxA), rb = up(refB, maxB);

    // 参照点より両目的で良い点だけが面積に寄与する
    std::vector<std::pair<double, double>> v;
    v.reserve(f.size());
    for (int i : f) {
        const double a = up(pts[static_cast<std::size_t>(i)].a, maxA);
        const double b = up(pts[static_cast<std::size_t>(i)].b, maxB);
        if (a > ra && b > rb) v.push_back({ a, b });
    }
    if (v.empty()) return 0.0;

    // A の良い順に走査し、B が伸びたぶんだけ短冊を足す (2 次元の標準手順)
    std::sort(v.begin(), v.end(), [](const std::pair<double, double> &p,
                                     const std::pair<double, double> &q) {
        return p.first > q.first;
    });
    double hv = 0.0, bPrev = rb;
    for (const auto &p : v) {
        if (p.second <= bPrev) continue;          // 支配されている短冊は幅 0
        hv += (p.first - ra) * (p.second - bPrev);
        bPrev = p.second;
    }
    return hv;
}

} // namespace pareto
} // namespace ofd
