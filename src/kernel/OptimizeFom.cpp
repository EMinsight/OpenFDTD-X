// OptimizeFom.cpp — 掃引結果 → FoM (仕様は OptimizeFom.h)
#include "OptimizeFom.h"

#include <cmath>
#include <limits>

namespace ofd {

bool fomMaximizes(FomKind kind)
{
    switch (kind) {
    case FomKind::MinReflectionDb:
    case FomKind::MinVswr:
        return false;
    case FomKind::MaxPeakGainDb:
    case FomKind::MaxFrontToBackDb:
        return true;
    }
    return false;
}

namespace {

// 給電点表から freqHz に最も近い点を取る。freqHz <= 0 なら nullptr を返し、
// 呼び出し側が「全点から最良」を選ぶ。
const FeedSweepPoint *nearestFeedPoint(const QVector<FeedSweep> &feeds,
                                       double freqHz)
{
    const FeedSweepPoint *best = nullptr;
    double bestDf = std::numeric_limits<double>::max();
    for (const FeedSweep &f : feeds) {
        for (const FeedSweepPoint &p : f.points) {
            const double df = std::fabs(p.freqHz - freqHz);
            if (df < bestDf) { bestDf = df; best = &p; }
        }
    }
    return best;
}

// 遠方界パターンから、指定角度に最も近い点の値 [dB] を取る
bool valueAtDeg(const FarPattern &pat, double deg, double *out)
{
    if (pat.deg.isEmpty() || pat.deg.size() != pat.eAbsDb.size()) return false;
    int at = -1;
    double bestD = std::numeric_limits<double>::max();
    for (int i = 0; i < pat.deg.size(); ++i) {
        // 角度は 0..360 の周期。180 と −180 を同一視する
        double d = std::fabs(pat.deg[i] - deg);
        if (d > 180.0) d = 360.0 - d;
        if (d < bestD) { bestD = d; at = i; }
    }
    if (at < 0) return false;
    *out = pat.eAbsDb[at];
    return true;
}

} // namespace

FomValue evaluateFom(FomKind kind, const SweepResult &r, double freqHz)
{
    FomValue out;
    if (!r.ok) return out;              // 失敗した点は候補から外す

    switch (kind) {
    case FomKind::MinReflectionDb:
    case FomKind::MinVswr: {
        if (r.feeds.isEmpty()) return out;
        const bool vswr = (kind == FomKind::MinVswr);
        if (freqHz > 0) {
            const FeedSweepPoint *p = nearestFeedPoint(r.feeds, freqHz);
            if (!p) return out;
            out.value = vswr ? p->vswr : p->refDb;
            out.valid = true;
        } else {
            // 周波数指定なし = その点で最も良い (小さい) 値を採る
            double best = std::numeric_limits<double>::max();
            bool any = false;
            for (const FeedSweep &f : r.feeds)
                for (const FeedSweepPoint &p : f.points) {
                    const double v = vswr ? p.vswr : p.refDb;
                    if (v < best) { best = v; any = true; }
                }
            if (!any) return out;
            out.value = best;
            out.valid = true;
        }
        // VSWR は 1 未満になり得ない。壊れた行を最良点にしない
        if (vswr && out.value < 1.0) out.valid = false;
        return out;
    }
    case FomKind::MaxPeakGainDb: {
        double best = -std::numeric_limits<double>::max();
        bool any = false;
        for (const FarPattern &pat : r.patterns) {
            if (freqHz > 0 && !pat.eAbsDb.isEmpty()
                && std::fabs(pat.freqHz - freqHz) > 0.5 * freqHz)
                continue;               // 指定周波数から大きく外れる面は見ない
            for (double v : pat.eAbsDb)
                if (v > best) { best = v; any = true; }
        }
        if (!any) return out;
        out.value = best;
        out.valid = true;
        return out;
    }
    case FomKind::MaxFrontToBackDb: {
        // 前方 (0°) と後方 (180°) の差。両方が取れる面だけを見る。
        double best = -std::numeric_limits<double>::max();
        bool any = false;
        for (const FarPattern &pat : r.patterns) {
            if (freqHz > 0 && !pat.eAbsDb.isEmpty()
                && std::fabs(pat.freqHz - freqHz) > 0.5 * freqHz)
                continue;
            double f0 = 0, b0 = 0;
            if (!valueAtDeg(pat, 0.0, &f0)) continue;
            if (!valueAtDeg(pat, 180.0, &b0)) continue;
            const double fb = f0 - b0;
            if (fb > best) { best = fb; any = true; }
        }
        if (!any) return out;
        out.value = best;
        out.valid = true;
        return out;
    }
    }
    return out;
}

int bestPointIndex(FomKind kind, const QVector<FomValue> &values)
{
    const bool maximize = fomMaximizes(kind);
    int at = -1;
    double best = maximize ? -std::numeric_limits<double>::max()
                           :  std::numeric_limits<double>::max();
    for (int i = 0; i < values.size(); ++i) {
        if (!values[i].valid) continue;
        const double v = values[i].value;
        // 厳密な不等号 = 同値なら先に出てきた点を残す (順序で決まり再現する)
        if (maximize ? (v > best) : (v < best)) { best = v; at = i; }
    }
    return at;
}

} // namespace ofd
