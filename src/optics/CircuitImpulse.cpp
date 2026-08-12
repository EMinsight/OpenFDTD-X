// CircuitImpulse.cpp
#include "CircuitImpulse.h"

#include <algorithm>
#include <cmath>

#include "../acoustics/core/Fft.h"

namespace ofd {
namespace pic {

namespace {
const double kC = 299792458.0;   // 真空中の光速 [m/s]
} // namespace

ImpulseResult impulse(const SpectrumFn &H, const ImpulseConfig &cfg)
{
    ImpulseResult r;
    if (!H || !cfg.valid()) return r;

    const std::size_t M =
        acoustics::nextPowerOfTwo(static_cast<std::size_t>(cfg.points));
    const double B = (cfg.bandwidth_Hz > 0.0)
                         ? cfg.bandwidth_Hz
                         : cfg.fsrHint_Hz * std::max(1, cfg.fsrMultiple);
    if (!(B > 0.0)) return r;
    r.bandwidth_Hz = B;
    r.dt_s = 1.0 / B;
    r.span_s = M * r.dt_s;

    const double f0 = kC / (cfg.lambda0_nm * 1.0e-9);
    const double df = B / static_cast<double>(M);

    // 包絡線なので**エルミート対称にしない** (負の Δf は搬送波より低い側の
    // 実在の周波数であって、複素共役ではない)。並びは DFT の慣用どおり
    // k < M/2 が正の Δf、k ≥ M/2 が負の Δf。
    std::vector<cplx> Hk(M);
    for (std::size_t k = 0; k < M; ++k) {
        const double dfk = (k < M / 2)
                               ? static_cast<double>(k) * df
                               : (static_cast<double>(k) - static_cast<double>(M)) * df;
        const double f = f0 + dfk;
        if (!(f > 0.0)) { Hk[k] = cplx(0.0, 0.0); continue; }
        const double lam_nm = kC / f * 1.0e9;
        Hk[k] = H(lam_nm);
        if (!std::isfinite(Hk[k].real()) || !std::isfinite(Hk[k].imag()))
            Hk[k] = cplx(0.0, 0.0);
    }
    if (!acoustics::fftInverse(Hk)) return r;
    r.h = Hk;

    // 主到達と、尾の残り
    std::size_t peak = 0;
    double tot = 0.0, tail = 0.0;
    for (std::size_t i = 0; i < M; ++i) {
        const double a = std::abs(r.h[i]);
        if (a > std::abs(r.h[peak])) peak = i;
        const double e = a * a;
        tot += e;
        if (i >= 3 * M / 4) tail += e;
    }
    r.mainDelay_s = peak * r.dt_s;
    r.energy = tot;
    r.tailFraction = (tot > 0.0) ? tail / tot : 0.0;

    // タップ間隔と減衰比 — 上位のピークから読む (リングの 1 周時間と
    // 1 周あたりの振幅比)。ピークが 2 本未満なら 0 のまま (でっち上げない)。
    const std::vector<std::pair<double, double>> pk = peaks(r, 0.02, 8);
    if (pk.size() >= 2) {
        // ピークは時刻順に並んでいるので、隣り合う間隔の最小値を 1 周とみなす
        double minGap = 0.0;
        for (std::size_t i = 1; i < pk.size(); ++i) {
            const double g = pk[i].first - pk[i - 1].first;
            if (g > 0.0 && (minGap == 0.0 || g < minGap)) minGap = g;
        }
        r.tapSpacing_s = minGap;
        // 減衰比は 1 周ぶん離れた 2 本の振幅比 (主到達の次と、その次)
        if (pk.size() >= 3 && pk[1].second > 0.0)
            r.decayRatio = pk[2].second / pk[1].second;
        else if (pk[0].second > 0.0)
            r.decayRatio = pk[1].second / pk[0].second;
    }
    return r;
}

std::vector<std::pair<double, double>> peaks(const ImpulseResult &r,
                                             double thresh,
                                             std::size_t maxCount)
{
    std::vector<std::pair<double, double>> out;
    if (!r.ok()) return out;
    double mx = 0.0;
    for (const cplx &v : r.h) mx = std::max(mx, std::abs(v));
    if (mx <= 0.0) return out;
    const double lim = thresh * mx;
    const std::size_t M = r.h.size();
    for (std::size_t i = 0; i < M && out.size() < maxCount; ++i) {
        const double a = std::abs(r.h[i]);
        if (a < lim) continue;
        // 局所最大だけ拾う (裾の連なりを 1 本に潰す)。端は巡回で見る。
        const double prev = std::abs(r.h[(i + M - 1) % M]);
        const double next = std::abs(r.h[(i + 1) % M]);
        if (a >= prev && a >= next) out.push_back({ i * r.dt_s, a });
    }
    return out;
}

} // namespace pic
} // namespace ofd
