// GdsGeometry.cpp — 仕様は GdsGeometry.h
#include "GdsGeometry.h"

#include <algorithm>
#include <cmath>

namespace ofd {

namespace {

// 座標の同一視しきい値。GDSII のデータベース単位は普通 1 nm なので、
// その 1/1000 なら丸め誤差だけを吸収して実形状は潰さない。
constexpr double kEps = 1e-12;   // [m]

// 靴紐公式 (符号付き面積の 2 倍)
double shoelace2(const GdsPolygon &p)
{
    const int n = p.x_m.size();
    double s = 0.0;
    for (int i = 0; i < n; ++i) {
        const int j = (i + 1) % n;
        s += p.x_m[i] * p.y_m[j] - p.x_m[j] * p.y_m[i];
    }
    return s;
}

} // namespace

GdsDecompose decomposePolygon(const GdsPolygon &p)
{
    GdsDecompose out;
    const int n = p.x_m.size();
    if (n < 3 || p.y_m.size() != n) return out;

    // 末尾が始点と同じ (閉じている) なら重複点を 1 つ落として扱う
    GdsPolygon q = p;
    if (n >= 4 && std::fabs(q.x_m.first() - q.x_m.last()) < kEps
        && std::fabs(q.y_m.first() - q.y_m.last()) < kEps) {
        q.x_m.removeLast();
        q.y_m.removeLast();
    }
    const int m = q.x_m.size();
    if (m < 3) return out;

    out.polygonArea = std::fabs(shoelace2(q)) * 0.5;
    if (!(out.polygonArea > 0.0)) return out;   // 面積 0 (退化) は作らない

    // 全辺が軸平行か
    for (int i = 0; i < m; ++i) {
        const int j = (i + 1) % m;
        const double dx = std::fabs(q.x_m[j] - q.x_m[i]);
        const double dy = std::fabs(q.y_m[j] - q.y_m[i]);
        if (dx > kEps && dy > kEps) { out.manhattan = false; break; }
    }

    // 1) 頂点の y でスラブに切る
    QVector<double> ys = q.y_m;
    std::sort(ys.begin(), ys.end());
    ys.erase(std::unique(ys.begin(), ys.end(),
                         [](double a, double b) {
                             return std::fabs(a - b) <= kEps;
                         }),
             ys.end());
    if (ys.size() < 2) return out;

    // 2) スラブごとに内部の x 区間を求める
    //    (中央 y で辺と交わり、x 昇順に並べて偶奇規則で対にする)
    struct Span { double x0, x1; };
    QVector<QVector<Span>> spans(ys.size() - 1);
    QVector<double> xs;
    for (int s = 0; s + 1 < ys.size(); ++s) {
        const double ym = 0.5 * (ys[s] + ys[s + 1]);
        xs.clear();
        for (int i = 0; i < m; ++i) {
            const int j = (i + 1) % m;
            const double y1 = q.y_m[i], y2 = q.y_m[j];
            if ((y1 <= ym) == (y2 <= ym)) continue;      // 跨いでいない
            const double t = (ym - y1) / (y2 - y1);
            xs.push_back(q.x_m[i] + t * (q.x_m[j] - q.x_m[i]));
        }
        if (xs.size() < 2) continue;
        std::sort(xs.begin(), xs.end());
        for (int k = 0; k + 1 < xs.size(); k += 2) {
            if (xs[k + 1] - xs[k] > kEps)
                spans[s].push_back({ xs[k], xs[k + 1] });
        }
    }

    // 3) 上下で x 区間が一致するスラブを 1 つの矩形へ結合する
    //    (結合しないと縦に細切れの直方体が大量に出る)
    QVector<bool> used;
    for (int s = 0; s + 1 < ys.size(); ++s) {
        for (int a = 0; a < spans[s].size(); ++a) {
            const Span sp = spans[s][a];
            double yTop = ys[s + 1];
            // 同じ x 区間が続く限り上へ伸ばし、その区間は消費済みにする
            for (int u = s + 1; u + 1 < ys.size(); ++u) {
                int hit = -1;
                for (int b = 0; b < spans[u].size(); ++b) {
                    if (std::fabs(spans[u][b].x0 - sp.x0) <= kEps
                        && std::fabs(spans[u][b].x1 - sp.x1) <= kEps) {
                        hit = b;
                        break;
                    }
                }
                if (hit < 0) break;
                yTop = ys[u + 1];
                spans[u].remove(hit);
            }
            GdsRect r;
            r.x0 = sp.x0; r.x1 = sp.x1;
            r.y0 = ys[s];  r.y1 = yTop;
            out.rects.push_back(r);
            out.rectArea += r.area();
        }
    }
    out.valid = !out.rects.isEmpty();
    return out;
}

GdsToGeometryResult gdsToGeometry(const GdsLibrary &lib,
                                  const QString &topCell,
                                  const QVector<GdsLayerExtrude> &layers)
{
    GdsToGeometryResult out;
    if (lib.structures.isEmpty() || layers.isEmpty()) return out;

    // トップセル: 名前一致 (無ければ最初の構造)
    const GdsStructure *st = &lib.structures.first();
    if (!topCell.isEmpty()) {
        for (const GdsStructure &s : lib.structures)
            if (s.name == topCell) { st = &s; break; }
    }

    for (const GdsLayerExtrude &le : layers) {
        if (!(le.z1_m > le.z0_m)) continue;     // 厚みが無い指定は無視
        int idx = 0;
        for (const GdsPolygon &poly : st->polygons) {
            if (poly.layer != le.layer) continue;
            ++out.polygons;
            const GdsDecompose d = decomposePolygon(poly);
            if (!d.valid) { ++out.skippedPolygons; continue; }
            if (!d.manhattan) ++out.nonManhattan;
            for (const GdsRect &r : d.rects) {
                Geometry g;
                g.materialId = le.materialId;
                g.shape = 1;                    // 直方体
                g.g[0] = r.x0; g.g[1] = r.x1;
                g.g[2] = r.y0; g.g[3] = r.y1;
                g.g[4] = le.z0_m; g.g[5] = le.z1_m;
                g.name = le.name.isEmpty()
                             ? QStringLiteral("gds%1_%2").arg(le.layer).arg(++idx)
                             : QStringLiteral("%1_%2").arg(le.name).arg(++idx);
                out.units.push_back(g);
                out.totalArea_m2 += r.area();
                ++out.rects;
            }
        }
    }
    return out;
}

} // namespace ofd
