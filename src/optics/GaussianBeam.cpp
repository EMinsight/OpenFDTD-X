#include "GaussianBeam.h"

#include <cmath>

namespace ofd {
namespace gauss {

namespace {
const double kPi = 3.14159265358979323846;

inline bool ok(double w0, double lambda, double n)
{
    return w0 > 0.0 && lambda > 0.0 && n > 0.0;
}
} // namespace

double rayleighRange(double w0_m, double lambda_m, double n)
{
    if (!ok(w0_m, lambda_m, n)) return 0.0;
    return kPi * w0_m * w0_m * n / lambda_m;
}

double beamRadius(double w0_m, double z_m, double lambda_m, double n)
{
    if (!ok(w0_m, lambda_m, n)) return 0.0;
    const double zr = rayleighRange(w0_m, lambda_m, n);
    const double t = z_m / zr;
    return w0_m * std::sqrt(1.0 + t * t);
}

double radiusOfCurvature(double z_m, double w0_m, double lambda_m, double n)
{
    if (!ok(w0_m, lambda_m, n)) return 0.0;
    if (z_m == 0.0) return 0.0;           // 平面波 (無限大の代わりに 0)
    const double zr = rayleighRange(w0_m, lambda_m, n);
    return z_m * (1.0 + (zr / z_m) * (zr / z_m));
}

double divergence(double w0_m, double lambda_m, double n)
{
    if (!ok(w0_m, lambda_m, n)) return 0.0;
    return lambda_m / (kPi * w0_m * n);
}

double gouyPhase(double z_m, double w0_m, double lambda_m, double n)
{
    if (!ok(w0_m, lambda_m, n)) return 0.0;
    return std::atan(z_m / rayleighRange(w0_m, lambda_m, n));
}

double beamParameterProduct(double w0_m, double lambda_m, double n)
{
    if (!ok(w0_m, lambda_m, n)) return 0.0;
    return w0_m * divergence(w0_m, lambda_m, n);
}

double geometricValidDistance(double w0_m, double lambda_m, double n, double tol)
{
    if (!ok(w0_m, lambda_m, n) || !(tol > 0.0)) return 0.0;
    const double zr = rayleighRange(w0_m, lambda_m, n);
    const double s = (1.0 + tol) * (1.0 + tol) - 1.0;
    if (!(s > 0.0)) return 0.0;
    return zr / std::sqrt(s);
}

Waist lensWaist(double w0_m, double d_m, double focal_m, double lambda_m,
                double n)
{
    Waist out;
    if (!ok(w0_m, lambda_m, n) || focal_m == 0.0) return out;
    const double zr = rayleighRange(w0_m, lambda_m, n);
    // Kogelnik & Li: (d − f)² + z_R² を分母にした標準形
    const double den = (d_m - focal_m) * (d_m - focal_m) + zr * zr;
    if (!(den > 0.0)) return out;
    out.w0_m = w0_m * std::fabs(focal_m) / std::sqrt(den);
    out.z_m = focal_m + (d_m - focal_m) * focal_m * focal_m / den;
    return out;
}

double waistFromIntensity(const std::vector<double> &intensity, double pitch_m)
{
    const std::size_t n = intensity.size();
    if (n == 0 || !(pitch_m > 0.0)) return 0.0;
    double sum = 0.0, sx = 0.0;
    for (std::size_t k = 0; k < n; ++k) {
        const double v = (intensity[k] > 0.0) ? intensity[k] : 0.0;
        const double x = (double(k) - 0.5 * (double(n) - 1.0)) * pitch_m;
        sum += v;
        sx += v * x;
    }
    if (!(sum > 0.0)) return 0.0;
    const double xc = sx / sum;
    double sxx = 0.0;
    for (std::size_t k = 0; k < n; ++k) {
        const double v = (intensity[k] > 0.0) ? intensity[k] : 0.0;
        const double x = (double(k) - 0.5 * (double(n) - 1.0)) * pitch_m - xc;
        sxx += v * x * x;
    }
    const double sigma = std::sqrt(sxx / sum);
    // ISO 11146: D4σ = 4σ。理想ガウシアンでは D4σ = 2w なので w = 2σ
    return 2.0 * sigma;
}

} // namespace gauss
} // namespace ofd
