// PhotonicCircuit.cpp
#include "PhotonicCircuit.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ofd {
namespace optics {

namespace {
const double kPi = 3.14159265358979323846;

// dB/cm → 振幅の減衰係数 α/2 [1/μm]
// 強度 exp(-α L)、α[1/m] = ln(10)/10 · (dB/m) = ln(10)/10 · 100·(dB/cm)
double halfAlphaPerUm(double loss_dBcm)
{
    const double alpha_perM = std::log(10.0) / 10.0 * 100.0 * loss_dBcm;
    return 0.5 * alpha_perM * 1e-6;   // 1/μm、振幅なので 1/2
}

// 無損失 2×2 方向性結合器。through = sqrt(1-κ²)、cross = -j κ
struct Coupler {
    double t, k;
    explicit Coupler(double kappa)
        : t(std::sqrt(std::max(0.0, 1.0 - kappa * kappa))), k(kappa) {}
};

// 過剰損失 [dB] → 振幅係数
double ampFromLossDb(double loss_dB)
{
    return std::pow(10.0, -loss_dB / 20.0);
}
} // namespace

double Waveguide::neffAt(double lambda_nm) const
{
    if (!(ng > 0.0) || !(lambda0_nm > 0.0)) return neff;
    // 一次分散: neff(λ) = neff0 + (neff0 - ng)(λ - λ0)/λ0
    return neff + (neff - ng) * (lambda_nm - lambda0_nm) / lambda0_nm;
}

cplx Waveguide::transfer(double lambda_nm, double length_um) const
{
    if (!(lambda_nm > 0.0)) return cplx(0.0, 0.0);
    const double lam_um = lambda_nm * 1e-3;
    const double beta = 2.0 * kPi * neffAt(lambda_nm) / lam_um;   // [1/μm]
    const double a = halfAlphaPerUm(loss_dBcm) * length_um;
    return std::exp(cplx(-a, -beta * length_um));
}

double RingResonator::circumference_um() const { return 2.0 * kPi * radius_um; }

cplx RingResonator::through(double lambda_nm) const
{
    const Coupler c1(kappa1), c2(kappa2);
    const double g = ampFromLossDb(couplerLoss_dB);
    const cplx a = wg.transfer(lambda_nm, circumference_um());
    // 全域通過 (κ2 = 0):  t_th = (t1 - a) / (1 - t1 a)
    // アド・ドロップ:      t_th = (t1 - t2 a) / (1 - t1 t2 a)
    const cplx num = cplx(c1.t, 0.0) - cplx(c2.t * g * g, 0.0) * a;
    const cplx den = cplx(1.0, 0.0) - cplx(c1.t * c2.t * g * g, 0.0) * a;
    if (std::abs(den) < 1e-300) return cplx(0.0, 0.0);
    return num / den;
}

cplx RingResonator::drop(double lambda_nm) const
{
    if (!(kappa2 > 0.0)) return cplx(0.0, 0.0);
    const Coupler c1(kappa1), c2(kappa2);
    const double g = ampFromLossDb(couplerLoss_dB);
    // ドロップは半周ぶんの伝搬 (両結合器が対向する配置)
    const cplx half = wg.transfer(lambda_nm, 0.5 * circumference_um());
    const cplx a = wg.transfer(lambda_nm, circumference_um());
    const cplx num = cplx(-c1.k * c2.k * g * g, 0.0) * half;
    const cplx den = cplx(1.0, 0.0) - cplx(c1.t * c2.t * g * g, 0.0) * a;
    if (std::abs(den) < 1e-300) return cplx(0.0, 0.0);
    return num / den;
}

cplx MachZehnder::bar(double lambda_nm) const
{
    const Coupler c1(kappa1), c2(kappa2);
    const cplx a1 = wg.transfer(lambda_nm, length1_um)
                    * std::exp(cplx(0.0, -phaseShift_rad));
    const cplx a2 = wg.transfer(lambda_nm, length2_um);
    // 入力 1 → (t1, -j k1) → アーム → (t2, -j k2) → 出力 1
    return cplx(c1.t * c2.t, 0.0) * a1
         + cplx(0.0, -c1.k) * a2 * cplx(0.0, -c2.k);
}

cplx MachZehnder::cross(double lambda_nm) const
{
    const Coupler c1(kappa1), c2(kappa2);
    const cplx a1 = wg.transfer(lambda_nm, length1_um)
                    * std::exp(cplx(0.0, -phaseShift_rad));
    const cplx a2 = wg.transfer(lambda_nm, length2_um);
    return cplx(c1.t, 0.0) * a1 * cplx(0.0, -c2.k)
         + cplx(0.0, -c1.k) * a2 * cplx(c2.t, 0.0);
}

namespace {
double toDb(const cplx &v)
{
    const double p = std::norm(v);
    return (p > 0.0) ? 10.0 * std::log10(p) : -300.0;
}

std::vector<SweepPoint> sweepGrid(double l1, double l2, int n)
{
    std::vector<SweepPoint> s;
    if (n < 2 || !(l2 > l1)) return s;
    s.resize(n);
    for (int i = 0; i < n; ++i)
        s[i].lambda_nm = l1 + (l2 - l1) * i / double(n - 1);
    return s;
}
} // namespace

std::vector<SweepPoint> sweepRing(const RingResonator &ring, double l1,
                                  double l2, int n)
{
    std::vector<SweepPoint> s = sweepGrid(l1, l2, n);
    for (SweepPoint &p : s) {
        p.through_dB = toDb(ring.through(p.lambda_nm));
        p.drop_dB = (ring.kappa2 > 0.0) ? toDb(ring.drop(p.lambda_nm)) : -300.0;
    }
    return s;
}

std::vector<SweepPoint> sweepMzi(const MachZehnder &mzi, double l1, double l2,
                                 int n)
{
    std::vector<SweepPoint> s = sweepGrid(l1, l2, n);
    for (SweepPoint &p : s) {
        p.through_dB = toDb(mzi.bar(p.lambda_nm));
        p.drop_dB = toDb(mzi.cross(p.lambda_nm));
    }
    return s;
}

double analyticFsr_nm(double lambda_nm, double ng, double length_um)
{
    if (!(ng > 0.0) || !(length_um > 0.0)) return 0.0;
    // FSR = λ²/(ng L)。λ[nm]・L[μm] を揃える (L を nm へ)
    return lambda_nm * lambda_nm / (ng * length_um * 1e3);
}

ResonatorMetrics analyseSweep(const std::vector<SweepPoint> &s)
{
    ResonatorMetrics m;
    if (s.size() < 5) {
        m.note = "sweep has too few points";
        return m;
    }
    // 極小 (共振) を拾う — 端点は共振と断定できないので除く
    std::vector<std::size_t> dips;
    for (std::size_t i = 1; i + 1 < s.size(); ++i)
        if (s[i].through_dB < s[i - 1].through_dB
            && s[i].through_dB <= s[i + 1].through_dB)
            dips.push_back(i);

    double lo = s[0].through_dB, hi = s[0].through_dB;
    for (const SweepPoint &p : s) {
        lo = std::min(lo, p.through_dB);
        hi = std::max(hi, p.through_dB);
    }
    m.extinction_dB = hi - lo;

    if (dips.empty()) {
        m.note = "no resonance dip in the swept range";
        return m;
    }
    // 最も深い共振
    std::size_t deepest = dips.front();
    for (const std::size_t i : dips)
        if (s[i].through_dB < s[deepest].through_dB) deepest = i;
    m.resonance_nm = s[deepest].lambda_nm;

    if (dips.size() >= 2) {
        // 隣接する共振の間隔の中央値を FSR とする (端の切れかけを避ける)
        std::vector<double> gaps;
        for (std::size_t i = 1; i < dips.size(); ++i)
            gaps.push_back(s[dips[i]].lambda_nm - s[dips[i - 1]].lambda_nm);
        std::sort(gaps.begin(), gaps.end());
        m.fsr_nm = gaps[gaps.size() / 2];
    } else {
        m.note = "only one resonance in range — FSR needs at least two";
    }

    // FWHM は **深さの半分** で測る (線形電力)。
    // 「谷底 + 3 dB」で測ると、消光の浅い共振 (臨界結合から外れた場合) では
    // しきい値がベースラインを超えてしまい交点が見つからない。
    // 臨界結合 (T_min = 0) では T_half = T_max/2 となり従来の -3 dB と一致する。
    const double linMin = std::pow(10.0, s[deepest].through_dB / 10.0);
    const double linMax = std::pow(10.0, hi / 10.0);
    const double halfLin = 0.5 * (linMax + linMin);
    auto lin = [](double dB) { return std::pow(10.0, dB / 10.0); };
    auto crossing = [&](int dir) -> double {
        for (std::size_t k = 1; k < s.size(); ++k) {
            if (dir < 0 && deepest < k) break;
            const std::size_t i = (dir < 0) ? deepest - k : deepest + k;
            if (dir > 0 && i >= s.size()) break;
            const std::size_t prev = (dir < 0) ? i + 1 : i - 1;
            if (lin(s[i].through_dB) >= halfLin) {
                const double y0 = lin(s[prev].through_dB), y1 = lin(s[i].through_dB);
                const double x0 = s[prev].lambda_nm, x1 = s[i].lambda_nm;
                if (std::abs(y1 - y0) < 1e-300) return x1;
                return x0 + (halfLin - y0) * (x1 - x0) / (y1 - y0);
            }
        }
        return std::numeric_limits<double>::quiet_NaN();
    };
    const double a = crossing(-1), b = crossing(+1);
    if (std::isfinite(a) && std::isfinite(b) && b > a) {
        m.fwhm_nm = b - a;
        if (m.fwhm_nm > 0.0) {
            m.qFactor = m.resonance_nm / m.fwhm_nm;
            if (m.fsr_nm > 0.0) m.finesse = m.fsr_nm / m.fwhm_nm;
        }
    } else if (m.note.empty()) {
        m.note = "the resonance is not resolved (widen the sweep or add points)";
    }
    m.valid = (m.fwhm_nm > 0.0);
    return m;
}

} // namespace optics
} // namespace ofd
