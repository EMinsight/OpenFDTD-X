// SarMetrics.cpp — SAR の定義式と電波防護指針の指針値 (Qt 非依存 / C++17)
// 出典はヘッダ冒頭を参照。
#include "SarMetrics.h"

#include <cmath>

namespace ofd {
namespace em {

namespace {

constexpr double kMHz = 1.0e6;
constexpr double kGHz = 1.0e9;

ExposureLimit makeLimit(double value, const char *unit, double fmin,
                        double fmax, double mass_g, double time_s,
                        const char *ref, bool isBasis = false)
{
    ExposureLimit l;
    l.defined = true;
    l.isBasis = isBasis;
    l.value = value;
    l.unit = unit;
    l.fmin_Hz = fmin;
    l.fmax_Hz = fmax;
    l.averagingMass_g = mass_g;
    l.averagingTime_s = time_s;
    l.reference = ref;
    return l;
}

// ICNIRP 2020 (Health Phys. 118(5), 483-524) の基本制限・参考レベル
ExposureLimit icnirp(Category cat, Metric m)
{
    const bool gp = (cat == Category::GeneralPublic);
    switch (m) {
    case Metric::LocalSar10g:
        // Table 2/4: 頭部・体幹 局所 SAR (10 g, 6 分平均)。四肢は 2 倍。
        return makeLimit(gp ? 2.0 : 10.0, "W/kg", 100.0e3, 6.0 * kGHz,
                         10.0, 360.0,
                         "ICNIRP 2020 Table 2/4 (head & torso, 10 g, 6 min)");
    case Metric::WholeBodySar:
        // Table 2/4: 全身平均 SAR (30 分平均)
        return makeLimit(gp ? 0.08 : 0.4, "W/kg", 100.0e3, 6.0 * kGHz,
                         0.0, 1800.0,
                         "ICNIRP 2020 Table 2/4 (whole body, 30 min)");
    case Metric::AbsorbedPowerDensity:
        // Table 3/5: 局所吸収電力密度 S_ab (4 cm² 平均, 6 分)
        return makeLimit(gp ? 20.0 : 100.0, "W/m^2", 6.0 * kGHz, 300.0 * kGHz,
                         0.0, 360.0,
                         "ICNIRP 2020 Table 3/5 (S_ab, 4 cm^2, 6 min)");
    case Metric::IncidentPowerDensity:
        // Table 6/7: 全身平均の参考レベル S_inc (30 分平均)
        return makeLimit(gp ? 10.0 : 50.0, "W/m^2", 2.0 * kGHz, 300.0 * kGHz,
                         0.0, 1800.0,
                         "ICNIRP 2020 Table 6/7 (whole-body S_inc, 30 min)");
    case Metric::LocalTemperatureRise:
        // Appendix A: 局所組織 1 ℃ 上昇が健康影響の閾値 (限度値ではない)
        return makeLimit(1.0, "K", 100.0e3, 300.0 * kGHz, 0.0, 0.0,
                         "ICNIRP 2020 Appendix A (adverse-effect threshold)",
                         true);
    case Metric::LocalSar1g:
    default:
        return ExposureLimit();   // ICNIRP は 1 g 平均を採用していない
    }
}

// IEEE Std C95.1-2019 (Tables 1・7)
ExposureLimit ieee(Category cat, Metric m)
{
    const bool gp = (cat == Category::GeneralPublic);   // unrestricted
    switch (m) {
    case Metric::LocalSar10g:
        return makeLimit(gp ? 2.0 : 10.0, "W/kg", 100.0e3, 6.0 * kGHz,
                         10.0, 360.0,
                         "IEEE C95.1-2019 Table 1 (local psSAR, 10 g, 6 min)");
    case Metric::WholeBodySar:
        return makeLimit(gp ? 0.08 : 0.4, "W/kg", 100.0e3, 6.0 * kGHz,
                         0.0, 1800.0,
                         "IEEE C95.1-2019 Table 1 (WB-SAR, 30 min)");
    case Metric::AbsorbedPowerDensity:
        return makeLimit(gp ? 20.0 : 100.0, "W/m^2", 6.0 * kGHz, 300.0 * kGHz,
                         0.0, 360.0,
                         "IEEE C95.1-2019 Table 1 (epithelial power density)");
    case Metric::IncidentPowerDensity:
        return makeLimit(gp ? 10.0 : 50.0, "W/m^2", 2.0 * kGHz, 300.0 * kGHz,
                         0.0, 1800.0,
                         "IEEE C95.1-2019 Table 7 (ERL, whole body)");
    case Metric::LocalTemperatureRise:
        return makeLimit(1.0, "K", 100.0e3, 300.0 * kGHz, 0.0, 0.0,
                         "IEEE C95.1-2019 Annex C (1 C local tissue rise)",
                         true);
    case Metric::LocalSar1g:
    default:
        return ExposureLimit();
    }
}

// FCC 47 CFR §1.1310 / §2.1093 (ANSI/IEEE C95.1-1992 準拠)
ExposureLimit fcc(Category cat, Metric m)
{
    const bool gp = (cat == Category::GeneralPublic);   // uncontrolled
    switch (m) {
    case Metric::LocalSar1g:
        return makeLimit(gp ? 1.6 : 8.0, "W/kg", 100.0e3, 6.0 * kGHz,
                         1.0, gp ? 1800.0 : 360.0,
                         "FCC 47 CFR 2.1093(d)(2) (1 g, partial body)");
    case Metric::LocalSar10g:
        // 四肢 (手首・足首より先) のみ 10 g 平均で規定されている
        return makeLimit(gp ? 4.0 : 20.0, "W/kg", 100.0e3, 6.0 * kGHz,
                         10.0, gp ? 1800.0 : 360.0,
                         "FCC 47 CFR 2.1093(d)(2) (10 g, extremities only)");
    case Metric::WholeBodySar:
        return makeLimit(gp ? 0.08 : 0.4, "W/kg", 100.0e3, 6.0 * kGHz,
                         0.0, gp ? 1800.0 : 360.0,
                         "FCC 47 CFR 2.1093(d)(1) (whole body)");
    case Metric::IncidentPowerDensity:
        // §1.1310 Table 1: 1500-100000 MHz の MPE (電力密度)
        return makeLimit(gp ? 10.0 : 50.0, "W/m^2", 1500.0 * kMHz,
                         100.0 * kGHz, 0.0, gp ? 1800.0 : 360.0,
                         "FCC 47 CFR 1.1310 Table 1 (MPE, 1.5-100 GHz)");
    case Metric::AbsorbedPowerDensity:
    case Metric::LocalTemperatureRise:
    default:
        return ExposureLimit();
    }
}

} // namespace

// ── 定義式 ──────────────────────────────────────────────────────────────────
double sarFromPeakField(double sigma, double ePeak, double rho)
{
    if (sigma < 0.0 || rho <= 0.0) return 0.0;
    return sigma * ePeak * ePeak / (2.0 * rho);
}

double sarFromRmsField(double sigma, double eRms, double rho)
{
    if (sigma < 0.0 || rho <= 0.0) return 0.0;
    return sigma * eRms * eRms / rho;
}

double planeWavePowerDensityFromRms(double eRms)
{
    return eRms * eRms / kFreeSpaceImpedance;
}

double rmsFieldFromPowerDensity(double s)
{
    if (s <= 0.0) return 0.0;
    return std::sqrt(s * kFreeSpaceImpedance);
}

double adiabaticTemperatureRise(double sar, double time_s, double cp)
{
    if (sar <= 0.0 || time_s <= 0.0 || cp <= 0.0) return 0.0;
    return sar * time_s / cp;
}

// ── 指針値 ──────────────────────────────────────────────────────────────────
ExposureLimit exposureLimit(Standard standard, Category category,
                            Metric metric, double frequency_Hz)
{
    ExposureLimit l;
    switch (standard) {
    case Standard::Icnirp2020:     l = icnirp(category, metric); break;
    case Standard::IeeeC95_1_2019: l = ieee(category, metric);   break;
    case Standard::Fcc47Cfr:       l = fcc(category, metric);    break;
    }
    if (l.defined)
        l.applicable = (frequency_Hz >= l.fmin_Hz && frequency_Hz <= l.fmax_Hz);
    return l;
}

Verdict evaluate(const ExposureLimit &limit, double value, bool hasValue)
{
    if (!limit.defined || !limit.applicable) return Verdict::NotApplicable;
    if (!hasValue || !std::isfinite(value))  return Verdict::NotEvaluated;
    return (value <= limit.value) ? Verdict::Compliant : Verdict::NonCompliant;
}

} // namespace em
} // namespace ofd
