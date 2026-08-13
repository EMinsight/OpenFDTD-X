// SlicePieces.cpp
#include "SlicePieces.h"

#include "H5Reader.h"

#include <algorithm>
#include <cmath>

namespace ofd {

QVector<SlicePiece> cutSlices(const QVector<SlicePlane> &planes)
{
    QVector<SlicePiece> out;
    for (int i = 0; i < planes.size(); ++i) {
        const SlicePlane &s = planes[i];
        if (s.axis < 0 || s.axis > 2) continue;
        const double ulo = std::min(s.u0, s.u1), uhi = std::max(s.u0, s.u1);
        const double vlo = std::min(s.v0, s.v1), vhi = std::max(s.v0, s.v1);
        // 面積を持たない指定は捨てる (0 幅の小片を作らない)
        if (!(uhi > ulo) || !(vhi > vlo)) continue;
        if (!std::isfinite(ulo) || !std::isfinite(uhi)
            || !std::isfinite(vlo) || !std::isfinite(vhi)) continue;

        // 面内 2 軸の対応は H5Reader が唯一の出所 (二重管理にしない)
        int uAxis = 0, vAxis = 1;
        H5Reader::seriesSliceAxes(s.axis, &uAxis, &vAxis);

        QVector<double> uc{ ulo, uhi }, vc{ vlo, vhi };
        for (int j = 0; j < planes.size(); ++j) {
            if (j == i) continue;
            const SlicePlane &o = planes[j];
            if (o.axis < 0 || o.axis > 2 || !std::isfinite(o.pos)) continue;
            // o の固定軸が s の面内軸のときだけ切れる (平行な面は切らない)。
            // 位置が範囲の内側にあるときだけ — 端で切っても片方が空になる
            if (o.axis == uAxis && o.pos > ulo && o.pos < uhi)
                uc.push_back(o.pos);
            if (o.axis == vAxis && o.pos > vlo && o.pos < vhi)
                vc.push_back(o.pos);
        }
        std::sort(uc.begin(), uc.end());
        std::sort(vc.begin(), vc.end());
        // 同じ位置の面が複数あっても切れ目は 1 本 (0 幅の小片を作らない)
        uc.erase(std::unique(uc.begin(), uc.end()), uc.end());
        vc.erase(std::unique(vc.begin(), vc.end()), vc.end());

        for (int a = 0; a + 1 < uc.size(); ++a) {
            for (int b = 0; b + 1 < vc.size(); ++b) {
                SlicePiece pc;
                pc.plane = i;
                pc.ua = uc[a]; pc.ub = uc[a + 1];
                pc.va = vc[b]; pc.vb = vc[b + 1];
                out.push_back(pc);
            }
        }
    }
    return out;
}

} // namespace ofd
