// ArrayDirectivity.cpp — 仕様と式は ArrayDirectivity.h
#include "ArrayDirectivity.h"

#include <cmath>

namespace ofd {
namespace acoustics {

namespace {
const double kPi = 3.14159265358979323846;

double deg2rad(double d) { return d * kPi / 180.0; }

// 高さ h の連続線音源の指向性 sinc(π·h·sinψ/λ)。h = 0 で無指向性。
double elementDirectivity(double psiRad, double h, double lambda)
{
    if (h <= 0.0 || lambda <= 0.0) return 1.0;
    const double u = kPi * h * std::sin(psiRad) / lambda;
    if (std::fabs(u) < 1e-12) return 1.0;
    return std::sin(u) / u;
}
} // namespace

ArrayPattern beamPattern(const std::vector<ArrayElement> &els,
                         double freqHz, double soundSpeed,
                         double elementHeight_m,
                         double degMin, double degMax, int nAngles)
{
    ArrayPattern out;
    if (els.empty() || freqHz <= 0.0 || soundSpeed <= 0.0 || nAngles < 3
        || !(degMax > degMin))
        return out;

    const double lambda = soundSpeed / freqHz;
    const double k0 = 2.0 * kPi / lambda;
    const double omega = 2.0 * kPi * freqHz;

    out.deg.resize(std::size_t(nAngles));
    out.db.resize(std::size_t(nAngles));
    std::vector<double> mag(std::size_t(nAngles), 0.0);

    double peak = 0.0;
    int peakIdx = 0;
    for (int i = 0; i < nAngles; ++i) {
        const double d = degMin + (degMax - degMin) * i / double(nAngles - 1);
        const double th = deg2rad(d);
        const double ux = std::cos(th), uz = -std::sin(th);
        double re = 0.0, im = 0.0;
        for (std::size_t kk = 0; kk < els.size(); ++kk) {
            const ArrayElement &e = els[kk];
            const double dir = elementDirectivity(th - deg2rad(e.tiltDeg),
                                                  elementHeight_m, lambda);
            const double ph = k0 * (ux * e.x + uz * e.z) - omega * e.delay_s;
            const double a = e.gain * dir;
            re += a * std::cos(ph);
            im += a * std::sin(ph);
        }
        const double m = std::sqrt(re * re + im * im);
        out.deg[std::size_t(i)] = d;
        mag[std::size_t(i)] = m;
        if (m > peak) { peak = m; peakIdx = i; }
    }
    if (!(peak > 0.0)) return out;

    for (int i = 0; i < nAngles; ++i) {
        const double rel = mag[std::size_t(i)] / peak;
        out.db[std::size_t(i)] = 20.0 * std::log10(rel > 1e-12 ? rel : 1e-12);
    }
    out.peakDeg = out.deg[std::size_t(peakIdx)];
    out.valid = true;
    return out;
}

BeamMetrics beamMetrics(const ArrayPattern &p)
{
    BeamMetrics m;
    const int n = int(p.db.size());
    if (!p.valid || n < 3) return m;

    int pk = 0;
    for (int i = 1; i < n; ++i) if (p.db[std::size_t(i)] > p.db[std::size_t(pk)]) pk = i;
    const double peak = p.db[std::size_t(pk)];
    const double half = peak - 3.0;

    // −3 dB を跨ぐ点を線形内挿する。範囲の端に達したら「幅は取れない」
    // (周回しない — ここが em/PatternMetrics との違い)。
    double edge[2] = { 0.0, 0.0 };
    bool got[2] = { false, false };
    const int dirs[2] = { +1, -1 };
    for (int s = 0; s < 2; ++s) {
        int i = pk;
        while (true) {
            const int j = i + dirs[s];
            if (j < 0 || j >= n) break;
            if (p.db[std::size_t(j)] <= half) {
                const double t = (p.db[std::size_t(i)] - half)
                                 / (p.db[std::size_t(i)] - p.db[std::size_t(j)]);
                edge[s] = p.deg[std::size_t(i)]
                          + t * (p.deg[std::size_t(j)] - p.deg[std::size_t(i)]);
                got[s] = true;
                break;
            }
            i = j;
        }
    }
    if (got[0] && got[1]) {
        m.hpbwDeg = std::fabs(edge[0] - edge[1]);
        m.hasHpbw = true;
    }

    // 主ビームの境界 = ピークから下って最初に上向きへ転じる点。その外側で最大。
    int bound[2] = { pk, pk };
    bool hasNull[2] = { false, false };
    for (int s = 0; s < 2; ++s) {
        int i = pk;
        while (true) {
            const int j = i + dirs[s];
            if (j < 0 || j >= n) break;
            if (p.db[std::size_t(j)] > p.db[std::size_t(i)]) {
                bound[s] = i;
                hasNull[s] = true;
                break;
            }
            i = j;
        }
    }
    double best = -1e300;
    int bestIdx = -1;
    for (int s = 0; s < 2; ++s) {
        if (!hasNull[s]) continue;
        for (int i = bound[s] + dirs[s]; i >= 0 && i < n; i += dirs[s])
            if (p.db[std::size_t(i)] > best) { best = p.db[std::size_t(i)]; bestIdx = i; }
    }
    if (bestIdx >= 0) {
        m.sllDb = best - peak;
        m.sllDeg = p.deg[std::size_t(bestIdx)];
        m.hasSll = true;
    }

    // −6 dB 以上が及ぶ角度の端 (連続とは限らない — 端だけを見る)
    int lo = -1, hi = -1;
    for (int i = 0; i < n; ++i) {
        if (p.db[std::size_t(i)] >= peak - 6.0) {
            if (lo < 0) lo = i;
            hi = i;
        }
    }
    if (lo >= 0) {
        m.coverageMinDeg = p.deg[std::size_t(lo)];
        m.coverageMaxDeg = p.deg[std::size_t(hi)];
        m.hasCoverage = true;
    }
    return m;
}

std::vector<ArrayElement> buildLineArray(int n, double spacing_m,
                                         const std::vector<double> &splayDeg,
                                         double steerDeg, double soundSpeed)
{
    std::vector<ArrayElement> els;
    if (n <= 0 || spacing_m <= 0.0 || soundSpeed <= 0.0) return els;
    els.reserve(std::size_t(n));

    double tilt = 0.0;          // 累積 splay (箱 0 は 0°)
    double x = 0.0, z = 0.0;
    for (int k = 0; k < n; ++k) {
        if (k > 0) {
            // 1 つ上の箱の面に沿って下へ積む (下向きは (−sinφ, −cosφ))
            const double prev = deg2rad(tilt);
            x += -spacing_m * std::sin(prev);
            z += -spacing_m * std::cos(prev);
            // 箱 k の splay (足りない分は最後の値を繰り返す)
            if (!splayDeg.empty()) {
                const std::size_t idx =
                    (std::size_t(k) < splayDeg.size()) ? std::size_t(k)
                                                       : splayDeg.size() - 1;
                tilt += splayDeg[idx];
            }
        } else if (!splayDeg.empty()) {
            tilt = splayDeg[0];
        }
        ArrayElement e;
        e.x = x;
        e.z = z;
        e.tiltDeg = tilt;
        els.push_back(e);
    }

    // ステアリング: û_s 方向で全素子が同相になるよう遅延を入れる。
    // 最小遅延が 0 になるよう平行移動する (負の遅延は実現できない)。
    const double th = deg2rad(steerDeg);
    const double ux = std::cos(th), uz = -std::sin(th);
    double minDelay = 0.0;
    for (std::size_t i = 0; i < els.size(); ++i) {
        els[i].delay_s = (ux * els[i].x + uz * els[i].z) / soundSpeed;
        if (i == 0 || els[i].delay_s < minDelay) minDelay = els[i].delay_s;
    }
    for (std::size_t i = 0; i < els.size(); ++i) els[i].delay_s -= minDelay;
    return els;
}

double gratingLobeFreq(double spacing_m, double steerDeg, double soundSpeed)
{
    if (spacing_m <= 0.0 || soundSpeed <= 0.0) return 0.0;
    const double s = std::fabs(std::sin(deg2rad(steerDeg)));
    return soundSpeed / (spacing_m * (1.0 + s));
}

EndfireResult endfire(double spacing_m, double delay_s, double freqHz,
                      double soundSpeed, bool reversePolarity)
{
    EndfireResult r;
    if (spacing_m <= 0.0 || freqHz <= 0.0 || soundSpeed <= 0.0) return r;
    const double omega = 2.0 * kPi * freqHz;
    const double tAc = spacing_m / soundSpeed;     // 箱の間を音が渡る時間
    const double s = reversePolarity ? -1.0 : 1.0;

    // |1 + s·exp(−i·φ)|
    const double phFront = omega * (tAc + delay_s);
    const double phBack  = omega * (delay_s - tAc);
    const double reF = 1.0 + s * std::cos(phFront);
    const double imF = -s * std::sin(phFront);
    const double reB = 1.0 + s * std::cos(phBack);
    const double imB = -s * std::sin(phBack);
    const double front = std::sqrt(reF * reF + imF * imF);
    const double back  = std::sqrt(reB * reB + imB * imB);

    r.frontDb = 20.0 * std::log10(front > 1e-12 ? front : 1e-12);
    r.backDb  = 20.0 * std::log10(back  > 1e-12 ? back  : 1e-12);
    r.frontBackDb = r.frontDb - r.backDb;
    // 逆相なら τ = d/c で後方の位相差が 0 → 1 − 1 = 0 (全周波数で消える)。
    // 同相なら後方の位相差を π にする必要があり、周波数に依存するので
    // 「全周波数で消える遅延」は存在しない (0 のままにする)。
    if (reversePolarity) r.optimumDelay_s = tAc;
    r.bestFreqHz = soundSpeed / (4.0 * spacing_m);
    r.valid = true;
    return r;
}

} // namespace acoustics
} // namespace ofd
