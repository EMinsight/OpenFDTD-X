// SliceLineIntegral.cpp
#include "SliceLineIntegral.h"

#include <algorithm>
#include <cmath>

namespace ofd {

namespace {

// 昇順の節点座標配列から、実座標 x に対応する**小数添字**を返す。
// 等間隔を仮定せず二分探索する (ofd の格子は軸ごとに間隔が変わる)。
// 範囲外なら false。
bool fractionalIndex(const QVector<double> &coord, double x, double *out)
{
    const int n = coord.size();
    if (n < 2 || !out) return false;
    const double lo = coord.first(), hi = coord.last();
    if (!(x >= lo - 1e-12) || !(x <= hi + 1e-12)) return false;
    if (x <= lo) { *out = 0.0; return true; }
    if (x >= hi) { *out = double(n - 1); return true; }
    // coord[i] <= x < coord[i+1] を満たす i
    const auto it = std::upper_bound(coord.begin(), coord.end(), x);
    int i = int(it - coord.begin()) - 1;
    i = std::max(0, std::min(i, n - 2));
    const double d = coord[i + 1] - coord[i];
    *out = (d > 0.0) ? (double(i) + (x - coord[i]) / d) : double(i);
    return true;
}

} // namespace

bool sliceLineIntegral(const QVector<double> &cells, int rows, int cols,
                       const QVector<double> &uCoord,
                       const QVector<double> &vCoord,
                       double u0, double v0, double u1, double v1,
                       int nSamples, LineIntegralResult *out)
{
    if (!out) return false;
    *out = LineIntegralResult();
    if (rows < 2 || cols < 2) return false;
    if (qint64(cells.size()) < qint64(rows) * qint64(cols)) return false;
    // 座標の数が行列と食い違うまま進めない (どちらかがずれていると
    // 位置が黙って狂う)
    if (uCoord.size() != cols || vCoord.size() != rows) return false;
    if (nSamples < 2) return false;
    for (const double q : { u0, v0, u1, v1 })
        if (!std::isfinite(q)) return false;

    const double du = u1 - u0, dv = v1 - v0;
    const double len = std::hypot(du, dv);
    if (!(len > 0.0)) return false;         // 長さ 0 の線分は積分できない

    out->samples.reserve(nSamples);
    double integral = 0.0;
    double prevVal = 0.0;
    for (int k = 0; k < nSamples; ++k) {
        const double t = double(k) / double(nSamples - 1);
        const double u = u0 + du * t;
        const double v = v0 + dv * t;

        double fu = 0.0, fv = 0.0;
        // 範囲外の点があれば積分そのものを行わない (端で打ち切った値を
        // 「線分に沿った積分」として出さない)
        if (!fractionalIndex(uCoord, u, &fu)) return false;
        if (!fractionalIndex(vCoord, v, &fv)) return false;

        // 行 0 = vCoord の + 側 なので、行方向は逆順になる
        const double fr = double(rows - 1) - fv;

        const int c0 = std::max(0, std::min(int(std::floor(fu)), cols - 2));
        const int r0 = std::max(0, std::min(int(std::floor(fr)), rows - 2));
        const double tc = std::max(0.0, std::min(fu - double(c0), 1.0));
        const double tr = std::max(0.0, std::min(fr - double(r0), 1.0));

        const double v00 = cells[r0 * cols + c0];
        const double v01 = cells[r0 * cols + c0 + 1];
        const double v10 = cells[(r0 + 1) * cols + c0];
        const double v11 = cells[(r0 + 1) * cols + c0 + 1];
        const double val = v00 * (1 - tc) * (1 - tr) + v01 * tc * (1 - tr)
                         + v10 * (1 - tc) * tr       + v11 * tc * tr;

        LineSample sm;
        sm.s = len * t;
        sm.value = val;
        out->samples.push_back(sm);
        out->maxAbs = std::max(out->maxAbs, std::fabs(val));

        // 台形則 (分点は等間隔なので幅は len/(n-1))
        if (k > 0)
            integral += 0.5 * (prevVal + val) * (len / double(nSamples - 1));
        prevVal = val;
    }

    out->integral = integral;
    out->length = len;
    out->mean = integral / len;
    out->ok = true;
    return true;
}

} // namespace ofd
