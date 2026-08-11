// PatternMetrics.cpp — 遠方界パターンの指標 (仕様は PatternMetrics.h)
#include "PatternMetrics.h"

#include <cmath>

namespace ofd {
namespace em {

namespace {

// 角度差を −180..180 へ畳む
double wrap180(double d)
{
    while (d > 180.0)  d -= 360.0;
    while (d < -180.0) d += 360.0;
    return d;
}

// i から step 方向へ 1 歩進んだ添字 (端は折り返す = パターンは周期的)
int step(int i, int s, int n) { return (i + s + n) % n; }

} // namespace

PatternMetrics patternMetrics(const std::vector<double> &deg,
                              const std::vector<double> &db)
{
    PatternMetrics m;
    const int n = int(deg.size());
    if (n < 3 || db.size() != deg.size()) return m;

    // ── ピーク ──────────────────────────────────────────────────────────
    int pk = 0;
    for (int i = 1; i < n; ++i) if (db[i] > db[pk]) pk = i;
    m.peakDb = db[pk];
    m.peakDeg = deg[pk];
    m.hasPeak = true;

    // ── 3 dB 幅 ────────────────────────────────────────────────────────
    // ピークから両側へ歩き、−3 dB を跨いだところで線形内挿する。
    // 跨がないまま 1 周したら「幅が定義できない」= hasHpbw = false。
    const double half = m.peakDb - 3.0;
    auto edge = [&](int dir, bool *ok) {
        int i = pk;
        for (int k = 0; k < n; ++k) {
            const int j = step(i, dir, n);
            if (db[j] <= half) {
                // db[i] > half >= db[j] — 角度で内挿する
                const double t = (db[i] - half) / (db[i] - db[j]);
                *ok = true;
                return wrap180(deg[i] - m.peakDeg)
                       + t * wrap180(deg[j] - deg[i]);
            }
            i = j;
        }
        *ok = false;
        return 0.0;
    };
    bool okR = false, okL = false;
    const double right = edge(+1, &okR);
    const double left  = edge(-1, &okL);
    if (okR && okL) {
        m.hpbwDeg = std::fabs(right) + std::fabs(left);
        m.hasHpbw = true;
    }

    // ── 主ビームの境界 (最初のヌル) → その外側で SLL ──────────────────
    // ピークから下り続けるあいだは主ビーム。上向きへ転じた点が最初のヌル。
    auto firstNull = [&](int dir, bool *ok) {
        int i = pk;
        for (int k = 0; k < n - 1; ++k) {
            const int j = step(i, dir, n);
            if (db[j] > db[i]) { *ok = true; return i; }   // ここで上向き
            i = j;
        }
        *ok = false;
        return i;
    };
    bool okNr = false, okNl = false;
    const int nullR = firstNull(+1, &okNr);
    const int nullL = firstNull(-1, &okNl);
    if (okNr && okNl && nullR != nullL) {
        // ヌルの外側 (主ビームを含まない側) を歩いて最大を拾う。
        // **歩いている途中でピークに戻ったら、その 2 つのヌルはパターン全体を
        // 囲っている = ローブが 1 つしかない**。無理にサイドローブを作らない
        // (カージオイドのような単一ローブでこれが起きる)。
        double best = -1e300;
        bool wrappedToPeak = false;
        int i = step(nullR, +1, n);
        while (i != nullL) {
            if (i == pk) { wrappedToPeak = true; break; }
            if (db[i] > best) best = db[i];
            i = step(i, +1, n);
        }
        if (!wrappedToPeak && best > -1e299) {
            m.sllDb = best - m.peakDb;      // ピーク基準 = 負値
            m.hasSll = true;
        }
    }

    // ── 前後比: ピーク方向とその 180° 逆 ────────────────────────────────
    {
        int back = -1;
        double bestD = 1e300;
        for (int i = 0; i < n; ++i) {
            const double d = std::fabs(wrap180(deg[i] - (m.peakDeg + 180.0)));
            if (d < bestD) { bestD = d; back = i; }
        }
        // 逆方向が実際に取れているとき (角度分解能の半分以内) だけ出す
        if (back >= 0 && bestD <= 5.0) {
            m.fbDb = m.peakDb - db[back];
            m.hasFb = true;
        }
    }
    return m;
}

} // namespace em
} // namespace ofd
