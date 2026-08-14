// DisplayMetrics.cpp
#include "DisplayMetrics.h"

#include <algorithm>
#include <cmath>

namespace ofd {
namespace displayoptics {

namespace {
constexpr double kPi = 3.14159265358979323846;
inline double deg(double rad) { return rad * 180.0 / kPi; }
inline double rad(double d)   { return d * kPi / 180.0; }
} // namespace

double criticalAngle_deg(double n)
{
    if (!(n > 1.0)) return 90.0;
    return deg(std::asin(1.0 / n));
}

WaveguideFov waveguideFov(double period_nm, double lambda_nm, double nSub,
                          double guideMax_deg)
{
    WaveguideFov r;
    if (!(period_nm > 0.0) || !(lambda_nm > 0.0) || !(nSub > 1.0)) return r;
    r.critAngle_deg = criticalAngle_deg(nSub);

    const double thetaMax = std::min(90.0, std::max(r.critAngle_deg, guideMax_deg));
    const double g = lambda_nm / period_nm;             // λ/Λ (格子ベクトル)

    // 空気側 sinθair = n·sinθg − λ/Λ。θg = θc で n·sinθc = 1。
    const double sLow  = 1.0 - g;
    const double sHigh = nSub * std::sin(rad(thetaMax)) - g;
    if (sLow < -1.0 || sLow > 1.0 || sHigh < -1.0 || sHigh > 1.0) return r;
    if (!(sHigh > sLow)) return r;

    r.fovMin_deg = deg(std::asin(sLow));
    r.fovMax_deg = deg(std::asin(sHigh));
    r.fov_deg = r.fovMax_deg - r.fovMin_deg;
    r.valid = r.fov_deg > 0.0;
    return r;
}

double eyeboxWidth_mm(double outcouplerLen_mm, double eyeRelief_mm,
                      double fov_deg)
{
    if (!(outcouplerLen_mm > 0.0) || eyeRelief_mm < 0.0) return 0.0;
    if (!(fov_deg > 0.0) || fov_deg >= 180.0) return 0.0;
    const double w = outcouplerLen_mm
                   - 2.0 * eyeRelief_mm * std::tan(rad(0.5 * fov_deg));
    return std::max(0.0, w);
}

double fresnelNormalTransmittance(double n)
{
    if (!(n > 0.0)) return 0.0;
    return 4.0 * n / ((n + 1.0) * (n + 1.0));
}

EyeboxSweep eyeboxVsEyeRelief(double outcouplerLen_mm, double fov_deg,
                              double erMax_mm, int n,
                              double *er, double *w)
{
    EyeboxSweep s;
    if (!(outcouplerLen_mm > 0.0) || !(fov_deg > 0.0) || fov_deg >= 180.0)
        return s;
    if (n < 2 || !er || !w || !(erMax_mm > 0.0)) return s;
    const double t = std::tan(rad(0.5 * fov_deg));
    if (!(t > 0.0)) return s;
    s.slope_mm_per_mm = -2.0 * t;
    s.zeroEyeRelief_mm = outcouplerLen_mm / (2.0 * t);
    for (int i = 0; i < n; ++i) {
        er[i] = erMax_mm * double(i) / double(n - 1);
        // 幅は既存の閉形式をそのまま呼ぶ (0 での打ち切りも同じ振る舞いになる)
        w[i] = eyeboxWidth_mm(outcouplerLen_mm, er[i], fov_deg);
    }
    s.valid = true;
    return s;
}

double minFovPeriod_nm(double lambda_nm, double nSub, double guideMax_deg)
{
    if (!(lambda_nm > 0.0) || !(nSub > 1.0)) return 0.0;
    const double thetaMax =
        std::min(90.0, std::max(criticalAngle_deg(nSub), guideMax_deg));
    const double den = 1.0 + nSub * std::sin(rad(thetaMax));
    if (!(den > 0.0)) return 0.0;
    return 2.0 * lambda_nm / den;
}

int fovEyeboxTradeoff(double periodMin_nm, double periodMax_nm, int n,
                      double lambda_nm, double nSub, double guideMax_deg,
                      double outcouplerLen_mm, double eyeRelief_mm,
                      double *period, double *fov, double *eyebox, bool *ok)
{
    if (n < 2 || !period || !fov || !eyebox || !ok) return 0;
    if (!(periodMax_nm > periodMin_nm) || !(periodMin_nm > 0.0)) return 0;
    int good = 0;
    for (int i = 0; i < n; ++i) {
        const double p = periodMin_nm
                       + (periodMax_nm - periodMin_nm) * double(i) / double(n - 1);
        period[i] = p;
        // 帯域とアイボックスは既存の閉形式をそのまま呼ぶ (表と同じ値になる)
        const WaveguideFov f = waveguideFov(p, lambda_nm, nSub, guideMax_deg);
        ok[i] = f.valid;
        fov[i] = f.valid ? f.fov_deg : 0.0;
        eyebox[i] = f.valid
                        ? eyeboxWidth_mm(outcouplerLen_mm, eyeRelief_mm, f.fov_deg)
                        : 0.0;
        if (f.valid) ++good;
    }
    return good;
}

double slabTransmittance(double nSub)
{
    if (!(nSub > 0.0)) return 0.0;
    const double r = (nSub - 1.0) / (nSub + 1.0);
    const double R = r * r;
    return (1.0 - R) / (1.0 + R);
}

double escapeConeFraction(double n)
{
    if (!(n > 1.0)) return 0.5;
    const double thc = std::asin(1.0 / n);
    return 0.5 * (1.0 - std::cos(thc));
}

double oledOutcoupling(double nOrganic)
{
    if (!(nOrganic > 0.0)) return 0.0;
    return 1.0 / (2.0 * nOrganic * nOrganic);
}

double ledExtractionTopFace(double n)
{
    return escapeConeFraction(n) * fresnelNormalTransmittance(n);
}

double ledExtractionCube(double n)
{
    if (!(n > 0.0)) return 0.0;
    return std::min(1.0, 3.0 / (2.0 * n * n));
}

double sidewallDeratedIqe(double iqe0, double chipSize_um,
                          double surfaceVelocity_cm_s, double lifetime_ns)
{
    if (!(chipSize_um > 0.0)) return 0.0;
    if (surfaceVelocity_cm_s < 0.0 || lifetime_ns < 0.0) return iqe0;
    // S [cm/s] → [μm/s] は ×1e4、τ [ns] → [s] は ×1e-9。積は μm。
    const double sTau_um = surfaceVelocity_cm_s * 1e4 * lifetime_ns * 1e-9;
    return iqe0 / (1.0 + 4.0 * sTau_um / chipSize_um);
}

AmbientContrast ambientContrast(double peakLuminance_cdm2, double darkroomCr,
                                double ambient_lx, double reflectance)
{
    AmbientContrast r;
    if (!(peakLuminance_cdm2 > 0.0) || !(darkroomCr > 1.0)) return r;
    if (ambient_lx < 0.0 || reflectance < 0.0 || reflectance > 1.0) return r;
    r.ambientLuminance_cdm2 = reflectance * ambient_lx / kPi;
    r.blackLuminance_cdm2 = peakLuminance_cdm2 / darkroomCr;
    const double den = r.blackLuminance_cdm2 + r.ambientLuminance_cdm2;
    if (!(den > 0.0)) return r;
    r.contrast = (peakLuminance_cdm2 + r.ambientLuminance_cdm2) / den;
    r.valid = true;
    return r;
}

double lambertianHalfAngle_deg()
{
    return 60.0;   // cosθ = 1/2
}

} // namespace displayoptics
} // namespace ofd
