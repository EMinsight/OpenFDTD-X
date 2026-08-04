// EmcStandards.cpp — EMC 規格の公表限度値と対策効果の古典式 (実装)
#include "EmcStandards.h"

#include <cmath>

namespace ofd {
namespace em {
namespace emc {

namespace {

// MSVC は <cmath> だけでは M_PI を定義しない (RadioPropagation.cpp と同じ流儀)
const double kPi = 3.14159265358979323846;

// μV/m → dBμV/m
double dbuvm(double uv_m) { return 20.0 * std::log10(uv_m); }

// Ott (2009) Table 6-1: 相対導電率 σr (銅基準) と比透磁率 μr (低周波の代表値)。
// 材料名は UI 文字列なのでここには置かない (I18n::tr — EmcTab.cpp が持つ)。
const ShieldMaterial kShieldMaterials[kShieldMaterialCount] = {
    { 1.00, 1.0 },        // 0 = 銅
    { 0.61, 1.0 },        // 1 = アルミニウム
    { 0.10, 1000.0 },     // 2 = 鋼 (低炭素)
    { 0.02, 1.0 },        // 3 = ステンレス SUS304
    { 0.03, 20000.0 },    // 4 = パーマロイ (ミューメタル)
};

} // namespace

// ── (a) 放射妨害波の限度値 ──────────────────────────────────────────────────
int radiatedLimits(Standard s, EmClass c, LimitSegment *out, int max)
{
    if (!out || max <= 0) return 0;
    int n = 0;
    auto add = [&](double f1, double f2, double lim, double d) {
        if (n < max) {
            out[n].f1_MHz = f1;
            out[n].f2_MHz = f2;
            out[n].limit_dBuVm = lim;
            out[n].refDist_m = d;
            ++n;
        }
    };

    if (s == Standard::Cispr32) {
        // CISPR 32:2015 Table A.3 (Class A) / A.4 (Class B) — 10 m, QP
        if (c == EmClass::A) {
            add(30.0, 230.0, 40.0, 10.0);
            add(230.0, 1000.0, 47.0, 10.0);
        } else {
            add(30.0, 230.0, 30.0, 10.0);
            add(230.0, 1000.0, 37.0, 10.0);
        }
    } else if (s == Standard::Fcc15) {
        if (c == EmClass::A) {
            // 47 CFR §15.109(b): 10 m、90/150/210/300 μV/m
            add(30.0, 88.0, dbuvm(90.0), 10.0);
            add(88.0, 216.0, dbuvm(150.0), 10.0);
            add(216.0, 960.0, dbuvm(210.0), 10.0);
            add(960.0, 1000.0, dbuvm(300.0), 10.0);
        } else {
            // 47 CFR §15.109(a): 3 m、100/150/200/500 μV/m
            add(30.0, 88.0, dbuvm(100.0), 3.0);
            add(88.0, 216.0, dbuvm(150.0), 3.0);
            add(216.0, 960.0, dbuvm(200.0), 3.0);
            add(960.0, 1000.0, dbuvm(500.0), 3.0);
        }
    }
    return n;
}

double limitAtDistance(const LimitSegment &seg, double d_m)
{
    if (d_m <= 0 || seg.refDist_m <= 0) return seg.limit_dBuVm;
    return seg.limit_dBuVm + 20.0 * std::log10(seg.refDist_m / d_m);
}

int limitSegmentIndex(const LimitSegment *seg, int n, double f_MHz)
{
    if (!seg) return -1;
    for (int i = 0; i < n; ++i) {
        // 最初の区間だけ下端を含める (30 MHz ちょうどを拾うため)
        const bool lowOk = (i == 0) ? (f_MHz >= seg[i].f1_MHz)
                                    : (f_MHz > seg[i].f1_MHz);
        if (lowOk && f_MHz <= seg[i].f2_MHz) return i;
    }
    return -1;
}

// ── (b) 金属シールドの遮蔽効果 ──────────────────────────────────────────────
const ShieldMaterial &shieldMaterial(int index)
{
    if (index < 0 || index >= kShieldMaterialCount) index = 0;
    return kShieldMaterials[index];
}

ShieldSE shieldEffectiveness(double f_Hz, double thickness_m,
                             double sigmaRel, double muRel)
{
    ShieldSE se;
    if (f_Hz <= 0 || thickness_m <= 0 || sigmaRel <= 0 || muRel <= 0)
        return se;

    const double sigma = sigmaRel * kSigmaCu;
    const double mu = muRel * kMu0;
    // 表皮深さ δ = 1/√(π f μ σ)
    se.skinDepth_m = 1.0 / std::sqrt(kPi * f_Hz * mu * sigma);
    const double u = thickness_m / se.skinDepth_m;

    // 吸収損 A = 8.686 t/δ [dB] (Ott eq. 6-9)
    se.absorption_dB = 8.686 * u;
    // 反射損 (平面波) R = 168 + 10log10(σr/(μr f)) [dB] (Ott eq. 6-11)
    se.reflection_dB = 168.0 + 10.0 * std::log10(sigmaRel / (muRel * f_Hz));
    // 多重反射補正 B = 20log10|1 − e^(−2t/δ)e^(−j2t/δ)| (Ott eq. 6-12)
    //   |…|² = 1 − 2e^(−2u)cos(2u) + e^(−4u)
    const double e2 = std::exp(-2.0 * u);
    const double mag2 = 1.0 - 2.0 * e2 * std::cos(2.0 * u) + e2 * e2;
    se.multiRefl_dB = (mag2 > 0) ? 10.0 * std::log10(mag2) : 0.0;
    se.total_dB = se.absorption_dB + se.reflection_dB + se.multiRefl_dB;
    se.valid = true;
    return se;
}

double apertureSE_dB(double f_Hz, double longest_m, int count)
{
    if (f_Hz <= 0 || longest_m <= 0 || count < 1) return 0.0;
    const double lambda = kC0 / f_Hz;
    double se = 20.0 * std::log10(lambda / (2.0 * longest_m));
    se -= 10.0 * std::log10(double(count));
    return (se > 0.0) ? se : 0.0;
}

// ── (c) 挿入損失 / ESD 電流 / 電力密度 ──────────────────────────────────────
double insertionLoss_dB(double zSeries_ohm, double zCircuit_ohm)
{
    if (zCircuit_ohm <= 0 || zSeries_ohm <= 0) return 0.0;
    return 20.0 * std::log10(1.0 + zSeries_ohm / zCircuit_ohm);
}

double inductiveReactance(double f_Hz, double l_H)
{
    if (f_Hz <= 0 || l_H <= 0) return 0.0;
    return 2.0 * kPi * f_Hz * l_H;
}

double apertureShrinkGain_dB(double ratio)
{
    if (ratio <= 0 || ratio >= 1.0) return 0.0;
    return 20.0 * std::log10(1.0 / ratio);
}

EsdContactCurrent esdContactCurrent(double kV)
{
    EsdContactCurrent c;
    if (kV <= 0) return c;
    c.firstPeak_A = 3.75 * kV;
    c.at30ns_A = 2.0 * kV;
    c.at60ns_A = 1.0 * kV;
    return c;
}

double powerDensity_Wm2(double eRms_Vm)
{
    if (eRms_Vm <= 0) return 0.0;
    return eRms_Vm * eRms_Vm / kZ0;
}

double amModulatedPeakField(double eRms_Vm)
{
    return (eRms_Vm > 0) ? 1.8 * eRms_Vm : 0.0;
}

} // namespace emc
} // namespace em
} // namespace ofd
