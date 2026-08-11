// TransmissionLine.cpp — 伝送線路の準 TEM 解析 (詳細は .h)
#include "TransmissionLine.h"

#include <algorithm>
#include <cmath>

namespace ofd {
namespace tline {

namespace {

constexpr double kPi   = 3.14159265358979323846;
constexpr double kC0   = 299792458.0;          // 真空中の光速 [m/s]
constexpr double kMu0  = 4.0e-7 * kPi;         // 真空の透磁率
constexpr double kEta0 = 376.730313668;        // 自由空間インピーダンス [Ω]

// 完全楕円積分 K(k) を AGM で。K = π/(2·AGM(1, k'))
double ellipticK(double k)
{
    double a = 1.0, b = std::sqrt(std::max(0.0, 1.0 - k * k));
    for (int i = 0; i < 60 && std::fabs(a - b) > 1e-16 * a; ++i) {
        const double an = 0.5 * (a + b);
        b = std::sqrt(a * b);
        a = an;
    }
    return kPi / (2.0 * a);
}

// Hammerstad & Jensen (1980) の実効誘電率
double hjEpsEff(double u, double epsr)
{
    const double u2 = u * u, u3 = u2 * u, u4 = u2 * u2;
    const double a = 1.0
        + (1.0 / 49.0) * std::log((u4 + (u / 52.0) * (u / 52.0))
                                  / (u4 + 0.432))
        + (1.0 / 18.7) * std::log(1.0 + u3 / (18.1 * 18.1 * 18.1));
    const double b = 0.564 * std::pow((epsr - 0.9) / (epsr + 3.0), 0.053);
    return 0.5 * (epsr + 1.0)
         + 0.5 * (epsr - 1.0) * std::pow(1.0 + 10.0 / u, -a * b);
}

// 同 (空気中の) 特性インピーダンス Z₀₁(u)
double hjZ01(double u)
{
    const double f = 6.0 + (2.0 * kPi - 6.0)
                         * std::exp(-std::pow(30.666 / u, 0.7528));
    return (kEta0 / (2.0 * kPi))
         * std::log(f / u + std::sqrt(1.0 + (2.0 / u) * (2.0 / u)));
}

} // namespace

double ellipticRatio(double k)
{
    if (!(k > 0.0) || !(k < 1.0)) return 0.0;
    const double kp = std::sqrt(1.0 - k * k);
    const double kk = ellipticK(k);
    return (kk > 0.0) ? ellipticK(kp) / kk : 0.0;
}

Result analyze(const Line &L, double freq_Hz)
{
    Result r;
    if (!(freq_Hz > 0.0) || !(L.epsr >= 1.0)) return r;

    const double mm = 1.0e-3;
    double z0 = 0.0, epsEff = 0.0;
    bool homogeneous = true;   // 断面が一様媒質か (ε_eff = εr)

    switch (L.kind) {
    case Kind::Microstrip: {
        if (!(L.w_mm > 0.0) || !(L.h_mm > 0.0)) return r;
        const double u = L.w_mm / L.h_mm;
        epsEff = hjEpsEff(u, L.epsr);
        z0 = hjZ01(u) / std::sqrt(epsEff);
        homogeneous = false;
        break;
    }
    case Kind::Stripline: {
        // 厚みゼロの厳密解 (Cohn)。h_mm を地板間隔 B として使う。
        if (!(L.w_mm > 0.0) || !(L.h_mm > 0.0)) return r;
        const double k = 1.0 / std::cosh(kPi * L.w_mm / (2.0 * L.h_mm));
        const double ratio = ellipticRatio(k);
        if (!(ratio > 0.0)) return r;
        epsEff = L.epsr;                       // 断面が一様に誘電体で満たされる
        z0 = (kEta0 / 4.0) * ratio / std::sqrt(epsEff);
        break;
    }
    case Kind::Coax: {
        if (!(L.a_mm > 0.0) || !(L.b_mm > L.a_mm)) return r;
        epsEff = L.epsr;
        z0 = (kEta0 / (2.0 * kPi)) * std::log(L.b_mm / L.a_mm)
           / std::sqrt(epsEff);
        break;
    }
    case Kind::TwoWire: {
        if (!(L.dia_mm > 0.0) || !(L.d_mm > L.dia_mm)) return r;
        epsEff = L.epsr;
        z0 = (kEta0 / kPi) * std::acosh(L.d_mm / L.dia_mm)
           / std::sqrt(epsEff);
        break;
    }
    case Kind::Coplanar: {
        // 基板が十分厚い場合の厳密解。ε_eff は上下が空気と誘電体なので
        // ちょうど平均になる。
        if (!(L.w_mm > 0.0) || !(L.slot_mm > 0.0)) return r;
        const double k = L.w_mm / (L.w_mm + 2.0 * L.slot_mm);
        const double ratio = ellipticRatio(k);
        if (!(ratio > 0.0)) return r;
        epsEff = 0.5 * (L.epsr + 1.0);
        z0 = 30.0 * kPi * ratio / std::sqrt(epsEff);
        homogeneous = false;
        break;
    }
    }
    if (!(z0 > 0.0) || !(epsEff > 0.0)) return r;

    r.z0_ohm = z0;
    r.epsEff = epsEff;
    r.vp_mps = kC0 / std::sqrt(epsEff);
    r.beta_radm = 2.0 * kPi * freq_Hz / r.vp_mps;

    // ── 誘電損 ─────────────────────────────────────────────────────────────
    // 均質線路 (ε_eff = εr) では充填率が 1 になり α_d = β·tanδ/2 に厳密に戻る
    if (L.tanD > 0.0) {
        const double lambda0 = kC0 / freq_Hz;
        double fill = 1.0;
        if (!homogeneous && L.epsr > 1.0)
            fill = (L.epsr / std::sqrt(epsEff)) * ((epsEff - 1.0) / (L.epsr - 1.0));
        else
            fill = std::sqrt(epsEff);
        r.alphaD_Npm = (kPi / lambda0) * fill * L.tanD;
    }

    // ── 導体損 ─────────────────────────────────────────────────────────────
    if (L.sigma_Sm > 0.0) {
        const double rs = std::sqrt(kPi * freq_Hz * kMu0 / L.sigma_Sm);
        switch (L.kind) {
        case Kind::Coax:
            // 内外両導体の表面抵抗 — 同軸はこれが厳密
            r.alphaC_Npm = rs / (2.0 * kPi * z0)
                         * (1.0 / (L.a_mm * mm) + 1.0 / (L.b_mm * mm));
            r.alphaCApprox = false;
            break;
        case Kind::TwoWire:
            r.alphaC_Npm = rs / (kPi * (L.dia_mm * mm) * z0);
            break;
        case Kind::Coplanar:
            r.alphaC_Npm = rs / (z0 * (L.w_mm * mm));
            break;
        default:
            // 広線路近似: 単位長あたり R = Rs/W、α_c = R/(2Z₀)
            r.alphaC_Npm = rs / (2.0 * z0 * (L.w_mm * mm)) * 2.0;
            break;
        }
    }
    r.alpha_dBm = (r.alphaC_Npm + r.alphaD_Npm) * 8.685889638;

    // ── 複素 Z₀ ────────────────────────────────────────────────────────────
    // 無損失値から等価な単位長 R/L/G/C を作り、Z₀ = √((R+jωL)/(G+jωC))。
    // α_c = R/(2Z₀), α_d = G·Z₀/2 の関係を逆に解いて R と G を得る。
    {
        const double omega = 2.0 * kPi * freq_Hz;
        const double lp = z0 * std::sqrt(epsEff) / kC0;   // L' [H/m]
        const double cp = std::sqrt(epsEff) / (kC0 * z0); // C' [F/m]
        const double rp = 2.0 * r.alphaC_Npm * z0;        // R' [Ω/m]
        const double gp = 2.0 * r.alphaD_Npm / z0;        // G' [S/m]
        const std::complex<double> zs(rp, omega * lp);
        const std::complex<double> ys(gp, omega * cp);
        r.z0Complex = (std::abs(ys) > 0.0) ? std::sqrt(zs / ys)
                                           : std::complex<double>(z0, 0.0);
    }
    r.delay_s = (L.length_mm * mm) * std::sqrt(epsEff) / kC0;
    r.valid = true;
    return r;
}

SParam sParameters(const Result &r, double length_mm, double z0Ref_ohm)
{
    SParam s;
    if (!r.valid || !(z0Ref_ohm > 0.0) || length_mm < 0.0) return s;

    const std::complex<double> gamma(r.alphaC_Npm + r.alphaD_Npm, r.beta_radm);
    const std::complex<double> gl = gamma * (length_mm * 1.0e-3);
    const std::complex<double> z0(r.z0_ohm, 0.0);
    const std::complex<double> zr(z0Ref_ohm, 0.0);

    // 一様線路の S 行列 (Pozar, Microwave Engineering の標準形)。
    // N = 2·Z₀·Zr·cosh(γℓ) + (Z₀² + Zr²)·sinh(γℓ) を共通の分母にする。
    const std::complex<double> ch = std::cosh(gl), sh = std::sinh(gl);
    const std::complex<double> den = 2.0 * z0 * zr * ch + (z0 * z0 + zr * zr) * sh;
    if (std::abs(den) < 1e-300) return s;

    s.s11 = ((z0 * z0 - zr * zr) * sh) / den;
    s.s21 = (2.0 * z0 * zr) / den;
    s.valid = true;
    return s;
}

} // namespace tline
} // namespace ofd
