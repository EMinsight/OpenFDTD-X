#include "TlSlice.h"
#include "ShdReader.h"

#include <cmath>
#include <limits>

namespace ofd {
namespace io {

TlSlice3D tlSlice3D(const ShdField &f, double range0_m, double range1_m,
                    double depth_m)
{
    TlSlice3D s;
    if (!f.isValid()) return s;
    if (!(range1_m > range0_m) || !(depth_m > 0.0)) return s;

    // 有効値 (kNoField を除く) の範囲。ShdField の minTL/maxTL をそのまま
    // 使わず数え直す — 断面を作り直したときに食い違わないようにする。
    double lo = std::numeric_limits<double>::infinity();
    double hi = -std::numeric_limits<double>::infinity();
    for (float v : f.tl_dB) {
        if (v >= ShdField::kNoField || !std::isfinite(v)) continue;
        lo = std::min(lo, double(v));
        hi = std::max(hi, double(v));
    }
    if (!(hi > lo)) return s;      // 一様 or 有効値なし → 色を付けられない

    s.rows = f.nrz;
    s.cols = f.nrr;
    s.axis = 1;                    // Y 一定 (XZ 鉛直面)
    s.pos_m = 0.0;
    s.u0_m = range0_m;
    s.u1_m = range1_m;
    s.v0_m = -depth_m;             // 海底
    s.v1_m = 0.0;                  // 海面 (= 第 2 軸の + 側 = 行 0)
    s.refTl_dB = hi;               // 最も静かな点を 0 に置く
    s.spanTl_dB = hi - lo;

    s.cells.resize(qsizetype(s.rows) * s.cols);
    for (qsizetype i = 0; i < s.cells.size(); ++i) {
        const float v = f.tl_dB[int(i)];
        if (v >= ShdField::kNoField || !std::isfinite(v)) {
            s.cells[i] = std::numeric_limits<double>::quiet_NaN();
            ++s.noFieldCells;
        } else {
            s.cells[i] = hi - double(v);   // 相対レベル (大きいほど大きい音)
        }
    }
    return s;
}

} // namespace io
} // namespace ofd
