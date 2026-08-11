// MeshRefine.cpp — 仕様は MeshRefine.h
#include "MeshRefine.h"

#include <algorithm>
#include <cmath>

namespace ofd {

namespace {

// 節点として同一とみなす距離。格子は m 単位で、実用上の最小セルは µm 級
// なので、その 1/1000 なら丸め誤差だけを吸収する。
constexpr double kEps = 1e-12;

// x を含む元の区間の添字 (区間 [nodes[i], nodes[i+1]))。範囲外は端に寄せる。
int segmentAt(const MeshAxis &a, double x)
{
    for (int i = 0; i + 1 < a.nodes.size(); ++i)
        if (x < a.nodes[i + 1] - kEps) return i;
    return a.divs.size() - 1;
}

} // namespace

MeshRefineResult refineAxis(const MeshAxis &axis,
                            const QVector<RefineSpan> &spans)
{
    MeshRefineResult out;
    if (!axis.isValid()) return out;

    out.valid = true;
    out.axis = axis;
    out.cellsBefore = axis.totalCells();
    out.cellsAfter = out.cellsBefore;
    out.minSpacingBefore = axis.minSpacing();
    out.minSpacingAfter = out.minSpacingBefore;

    // 効く区間だけ残す (無効・比率 1・範囲外・逆順は落とす)
    const double lo0 = axis.nodes.first(), hi0 = axis.nodes.last();
    QVector<RefineSpan> use;
    for (const RefineSpan &s : spans) {
        if (!(s.ratio > 0.0) || std::fabs(s.ratio - 1.0) < 1e-12) continue;
        const double lo = std::max(s.lo, lo0);
        const double hi = std::min(s.hi, hi0);
        if (!(hi > lo + kEps)) continue;      // 範囲外 / 幅 0
        RefineSpan c = s;
        c.lo = lo;
        c.hi = hi;
        use.push_back(c);
    }
    out.spansApplied = use.size();
    if (use.isEmpty()) {
        // 何も変えない — 入力をそのまま返す (ビット等価)
        double step = 1.0;
        for (int i = 0; i + 1 < axis.divs.size(); ++i) {
            const double a = (axis.nodes[i + 1] - axis.nodes[i]) / axis.divs[i];
            const double b =
                (axis.nodes[i + 2] - axis.nodes[i + 1]) / axis.divs[i + 1];
            if (a > 0.0 && b > 0.0)
                step = std::max(step, std::max(a / b, b / a));
        }
        out.maxStepRatio = step;
        return out;
    }

    // 1) 節点の集合 = 元の節点 + 区間の端
    QVector<double> nodes = axis.nodes;
    for (const RefineSpan &s : use) { nodes.push_back(s.lo); nodes.push_back(s.hi); }
    std::sort(nodes.begin(), nodes.end());
    QVector<double> uniq;
    for (double v : nodes)
        if (uniq.isEmpty() || v - uniq.last() > kEps) uniq.push_back(v);
    if (uniq.size() < 2) return out;

    // 2) 各区間の分割数 — 元のセル幅を保ち、領域内なら ratio 倍する
    MeshAxis res;
    res.nodes = uniq;
    for (int i = 0; i + 1 < uniq.size(); ++i) {
        const double a = uniq[i], b = uniq[i + 1];
        const double mid = 0.5 * (a + b);
        const int seg = segmentAt(axis, mid);
        const double baseCell =
            (axis.nodes[seg + 1] - axis.nodes[seg]) / double(axis.divs[seg]);
        // 元のセル幅で何分割になるか (最低 1)
        int d = (baseCell > 0.0) ? int(std::llround((b - a) / baseCell)) : 1;
        if (d < 1) d = 1;
        // この区間を覆う領域のうち最も細かいものを採る (重なりは細かい方勝ち)
        double ratio = 1.0;
        for (const RefineSpan &s : use)
            if (mid > s.lo + kEps && mid < s.hi - kEps)
                ratio = std::max(ratio, s.ratio);
        if (ratio > 1.0) {
            const int scaled = int(std::llround(d * ratio));
            d = std::max(d, scaled);
        }
        res.divs.push_back(d);
    }
    if (!res.isValid()) return out;      // 作れなかったら元のまま返す

    out.axis = res;
    out.cellsAfter = res.totalCells();
    out.minSpacingAfter = res.minSpacing();
    double step = 1.0;
    for (int i = 0; i + 1 < res.divs.size(); ++i) {
        const double a = (res.nodes[i + 1] - res.nodes[i]) / res.divs[i];
        const double b = (res.nodes[i + 2] - res.nodes[i + 1]) / res.divs[i + 1];
        if (a > 0.0 && b > 0.0) step = std::max(step, std::max(a / b, b / a));
    }
    out.maxStepRatio = step;
    return out;
}

} // namespace ofd
