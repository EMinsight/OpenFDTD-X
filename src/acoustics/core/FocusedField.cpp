// FocusedField.cpp — 集束超音波源の軸上音場・安全指標 (Qt 非依存 / C++14)
// 式の出典はヘッダ冒頭の [1]〜[4] を参照。
#include "FocusedField.h"

#include <cmath>

namespace ofd {
namespace acoustics {
namespace ultrasound {

namespace {

const double kPi = 3.14159265358979323846;
// dB ↔ Np の換算 (20/ln10)
const double kDbPerNp = 8.685889638065035;
// Airy パターン (jinc²) の半値全幅係数 — 2J1(x)/x = 1/√2 の根 x = 1.61634 より
// FWHM = 2·x·λ·F#/π = 1.028·λ·F#  ([4] Ch.6)
const double kAiryFwhm = 1.0288;

// sin(x)/x の数値的に安全な評価 (x → 0 で 1)
double sinc(double x)
{
    if (std::fabs(x) < 1.0e-8) return 1.0 - x * x / 6.0;
    return std::sin(x) / x;
}

bool validMedium(const Medium &m)
{
    return m.rho > 0.0 && m.c > 0.0 && m.alpha0_dBcmMHz >= 0.0
           && m.alphaExponent > 0.0;
}

bool validSource(const FocusedSource &s)
{
    return s.apertureRadius_m > 0.0 && s.focalLength_m > 0.0
           && s.apertureRadius_m < s.focalLength_m && s.frequency_Hz > 0.0
           && s.power_W >= 0.0;
}

} // namespace

// ── 基本量 ──────────────────────────────────────────────────────────────────
double capHeight(double a, double R)
{
    if (a <= 0.0 || R <= 0.0 || a >= R) return 0.0;
    return R - std::sqrt(R * R - a * a);
}

double capArea(double a, double R)
{
    return 2.0 * kPi * R * capHeight(a, R);
}

double surfaceVelocity(const FocusedSource &src, const Medium &med)
{
    if (!validSource(src) || !validMedium(med)) return 0.0;
    const double S = capArea(src.apertureRadius_m, src.focalLength_m);
    if (S <= 0.0) return 0.0;
    // W = ½ ρ c u0² S  ⇒  u0 = √(2W /(ρ c S))
    return std::sqrt(2.0 * src.power_W / (med.rho * med.c * S));
}

double attenuation_dB_per_m(const Medium &med, double frequency_Hz)
{
    if (!validMedium(med) || frequency_Hz <= 0.0) return 0.0;
    const double fMHz = frequency_Hz * 1.0e-6;
    // α0 [dB/cm/MHz^y] · f^y → [dB/cm] → ×100 で [dB/m]
    return med.alpha0_dBcmMHz * std::pow(fMHz, med.alphaExponent) * 100.0;
}

double attenuation_Np_per_m(const Medium &med, double frequency_Hz)
{
    return attenuation_dB_per_m(med, frequency_Hz) / kDbPerNp;
}

double acousticImpedance(const Medium &med)
{
    return med.rho * med.c;
}

// ── 軸上音圧 (O'Neil の厳密閉形式) ──────────────────────────────────────────
double axialPressureAmplitude(const FocusedSource &src, const Medium &med,
                              double u0, double z_m)
{
    if (!validSource(src) || !validMedium(med) || u0 <= 0.0 || z_m <= 0.0)
        return 0.0;

    const double a = src.apertureRadius_m;
    const double R = src.focalLength_m;
    const double k = 2.0 * kPi * src.frequency_Hz / med.c;
    const double cosAlpha = std::sqrt(1.0 - (a * a) / (R * R));  // cos α
    const double zeta = R - z_m;      // 幾何焦点からの符号付き距離

    // r1 = |R−ζ| = z (頂点からの距離), r2 = √(R²+ζ²−2Rζcosα) (開口縁からの距離)。
    // r2 − r1 は差を直接取ると桁落ちするので
    //   r2² − r1² = 2Rζ(1−cosα) ⇒ r2 − r1 = 2Rζ(1−cosα)/(r2+r1)
    // と書き換える (ζ → 0 でも安定)。
    const double r1 = std::fabs(R - zeta);
    const double r2 = std::sqrt(R * R + zeta * zeta
                                - 2.0 * R * zeta * cosAlpha);
    const double sum = r1 + r2;
    if (sum <= 0.0) return 0.0;
    const double g = k * R * (1.0 - cosAlpha) / sum;   // (k/2)(r2−r1)/ζ

    // |p| = 2 ρ c u0 (R/|ζ|)|sin(g ζ)| = 2 ρ c u0 R g |sinc(gζ)|
    return 2.0 * med.rho * med.c * u0 * R * std::fabs(g)
           * std::fabs(sinc(g * zeta));
}

double focalPressureLossless(const FocusedSource &src, const Medium &med,
                             double u0)
{
    if (!validSource(src) || !validMedium(med) || u0 <= 0.0) return 0.0;
    const double k = 2.0 * kPi * src.frequency_Hz / med.c;
    const double h = capHeight(src.apertureRadius_m, src.focalLength_m);
    return med.rho * med.c * u0 * k * h;
}

// ── まとめて評価 ────────────────────────────────────────────────────────────
FocusedFieldResult evaluateFocus(const FocusedSource &src, const Medium &med)
{
    FocusedFieldResult r;
    if (!validSource(src) || !validMedium(med)) return r;
    r.valid = true;

    const double a = src.apertureRadius_m;
    const double R = src.focalLength_m;
    const double k = 2.0 * kPi * src.frequency_Hz / med.c;
    const double lambda = med.c / src.frequency_Hz;

    r.capHeight_m = capHeight(a, R);
    r.capArea_m2  = capArea(a, R);
    r.surfaceVelocity_mps = surfaceVelocity(src, med);
    r.surfaceIntensity_Wm2 =
        (r.capArea_m2 > 0.0) ? src.power_W / r.capArea_m2 : 0.0;
    r.pressureGain = k * r.capHeight_m;
    r.focalPressureLossless_Pa =
        focalPressureLossless(src, med, r.surfaceVelocity_mps);

    // 媒質減衰 (音源→焦点の片道 R)
    r.attenuation_dB = attenuation_dB_per_m(med, src.frequency_Hz) * R;
    r.focalPressure_Pa =
        r.focalPressureLossless_Pa * std::pow(10.0, -r.attenuation_dB / 20.0);
    // 連続波の時間平均強度 (進行波近似)
    r.focalIntensity_Wm2 =
        r.focalPressure_Pa * r.focalPressure_Pa / (2.0 * med.rho * med.c);

    r.fNumber = R / (2.0 * a);
    r.beamWidth6dB_m = kAiryFwhm * lambda * r.fNumber;

    // MI ([3]): 0.3 dB/cm/MHz で焦点までデレーティングした最大希薄音圧 [MPa]
    // を √f_awf [MHz] で割る。正弦波を仮定し p_r = |p| とする。
    const double fMHz = src.frequency_Hz * 1.0e-6;
    const double deratedDb = 0.3 * fMHz * (R * 100.0);   // R を cm に
    const double pDerated =
        r.focalPressureLossless_Pa * std::pow(10.0, -deratedDb / 20.0);
    r.mechanicalIndex = (pDerated * 1.0e-6) / std::sqrt(fMHz);

    // 非線形指標 ([2]) — B/A が既知で、焦点音圧が正のときのみ
    if (med.bOverA >= 0.0 && r.focalPressure_Pa > 0.0) {
        r.nonlinearValid = true;
        r.betaNonlinear = 1.0 + 0.5 * med.bOverA;
        r.machNumber = r.focalPressure_Pa / (med.rho * med.c * med.c);
        r.shockDistance_m = 1.0 / (r.betaNonlinear * r.machNumber * k);
        const double alphaNp = attenuation_Np_per_m(med, src.frequency_Hz);
        if (alphaNp > 0.0) {
            r.goldberg = 1.0 / (alphaNp * r.shockDistance_m);
            r.regime = (r.goldberg >= 10.0)  ? RegimeShock
                       : (r.goldberg > 0.1)  ? RegimeTransitional
                                             : RegimeQuasiLinear;
        } else {
            r.goldberg = -1.0;             // 吸収ゼロ → Γ = ∞
            r.regime = RegimeShock;
        }
    }
    return r;
}

// ── 文献値データベース ──────────────────────────────────────────────────────
namespace {

Medium makeMedium(double rho, double c, double alpha0, double y, double ba)
{
    Medium m;
    m.rho = rho;
    m.c = c;
    m.alpha0_dBcmMHz = alpha0;
    m.alphaExponent = y;
    m.bOverA = ba;
    return m;
}

// 生体 (Duck 1990 / IT'IS V4.1 / Szabo 2014 Table 4.1 / H&B 1998 Table 1.1)
const MediumEntry kBio[5] = {
    { "water",       makeMedium(1000.0, 1480.0, 0.002, 2.0,  5.0) },
    { "soft_tissue", makeMedium(1045.0, 1540.0, 0.54,  1.1,  6.8) },
    { "fat",         makeMedium( 950.0, 1450.0, 0.48,  1.1, 10.0) },
    { "liver",       makeMedium(1060.0, 1590.0, 0.50,  1.1,  6.8) },
    { "bone",        makeMedium(1900.0, 4080.0, 6.90,  1.0, -1.0) },
};

// NDT (Krautkrämer 1990 Appendix / 水は上と同じ)
const MediumEntry kNdt[4] = {
    { "steel_long",  makeMedium(7850.0, 5900.0, 0.02,  1.0, -1.0) },
    { "steel_shear", makeMedium(7850.0, 3230.0, 0.05,  1.0, -1.0) },
    { "cfrp",        makeMedium(1560.0, 3070.0, 1.20,  1.0, -1.0) },
    { "water",       makeMedium(1000.0, 1480.0, 0.002, 2.0,  5.0) },
};

} // namespace

int bioMediumCount() { return 5; }

const MediumEntry &bioMedium(int index)
{
    if (index < 0) index = 0;
    if (index > 4) index = 4;
    return kBio[index];
}

int ndtMediumCount() { return 4; }

const MediumEntry &ndtMedium(int index)
{
    if (index < 0) index = 0;
    if (index > 3) index = 3;
    return kNdt[index];
}

} // namespace ultrasound
} // namespace acoustics
} // namespace ofd
