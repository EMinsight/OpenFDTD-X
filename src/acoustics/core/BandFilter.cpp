// BandFilter.cpp — 4次バターワース帯域通過フィルタの実装。
//
// 設計手順:
//   1. 2次バターワース低域原型の極 p = exp(j*3π/4), exp(j*5π/4)
//   2. 低域→帯域通過変換 s' で 4 極へ: s² - (B·p)s + w0² = 0
//      (w0 = √(W1·W2), B = W2 - W1, Wk = tan(π fk / fs) はプリワーピング)
//   3. 双一次変換 z = (1+s)/(1-s) で離散化
//   4. 零点は z = +1 (2個), z = -1 (2個)。中心周波数で振幅 1 に正規化。
#include "BandFilter.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdio>

namespace ofd {
namespace acoustics {

namespace {
const double kPi = 3.14159265358979323846;

// 1 オクターブ帯域のエッジ: fc / √2 〜 fc·√2
Band octaveBand(double fc, const std::string &label) {
    const double r = std::sqrt(2.0);
    return Band(label, fc, fc / r, fc * r);
}

// 1/3 オクターブ帯域のエッジ: fc·2^(-1/6) 〜 fc·2^(1/6)
Band thirdOctaveBand(double fc, const std::string &label) {
    const double r = std::pow(2.0, 1.0 / 6.0);
    return Band(label, fc, fc / r, fc * r);
}

std::string hzLabel(double fc) {
    char buf[32];
    if (fc >= 1000.0) {
        std::snprintf(buf, sizeof(buf), "%gk", fc / 1000.0);
    } else {
        std::snprintf(buf, sizeof(buf), "%g", fc);
    }
    return std::string(buf);
}

// 多項式 (z^-1 昇冪) に因子 (1 - p·z^-1) を掛ける
void multiplyPoleFactor(std::complex<double> *coeff, std::size_t &degree,
                        const std::complex<double> &p) {
    ++degree;
    for (std::size_t k = degree; k >= 1; --k)
        coeff[k] = coeff[k] - p * coeff[k - 1];
}
} // namespace

std::vector<Band> makeBands(BandSet set) {
    std::vector<Band> bands;
    switch (set) {
    case BandSet::Compat6: {
        // 既存互換 (src/core/RoomAcoustics.h の kBandHz と同じ 6 帯域)
        const double fc[6] = {125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0};
        for (int i = 0; i < 6; ++i)
            bands.push_back(octaveBand(fc[i], hzLabel(fc[i])));
        break;
    }
    case BandSet::FullBandOnly: {
        Band b("full", 0.0, 0.0, 0.0, true);
        bands.push_back(b);
        break;
    }
    case BandSet::Octave63To8k: {
        const double fc[8] = {63.0, 125.0, 250.0, 500.0,
                              1000.0, 2000.0, 4000.0, 8000.0};
        for (int i = 0; i < 8; ++i)
            bands.push_back(octaveBand(fc[i], hzLabel(fc[i])));
        break;
    }
    case BandSet::ThirdOctave100To5k: {
        // 公称中心周波数 (IEC 61260)
        const double fc[18] = {100.0,  125.0,  160.0,  200.0,  250.0,  315.0,
                               400.0,  500.0,  630.0,  800.0,  1000.0, 1250.0,
                               1600.0, 2000.0, 2500.0, 3150.0, 4000.0, 5000.0};
        for (int i = 0; i < 18; ++i)
            bands.push_back(thirdOctaveBand(fc[i], hzLabel(fc[i])));
        break;
    }
    case BandSet::SingerFormant: {
        // 歌手フォルマント (singer's formant) 周辺の分析帯域
        const double edges[4][2] = {
            {2000.0, 2500.0}, {2500.0, 3150.0}, {3150.0, 4000.0}, {2000.0, 4000.0}};
        const char *labels[4] = {"2.0-2.5k", "2.5-3.15k", "3.15-4.0k",
                                 "2.0-4.0k"};
        for (int i = 0; i < 4; ++i) {
            const double lo = edges[i][0], hi = edges[i][1];
            bands.push_back(Band(labels[i], std::sqrt(lo * hi), lo, hi));
        }
        break;
    }
    }
    return bands;
}

BandFilter::BandFilter() : m_valid(false) {
    for (int i = 0; i < 5; ++i) {
        m_b[i] = 0.0;
        m_a[i] = 0.0;
    }
    m_a[0] = 1.0;
    m_b[0] = 1.0; // 無効時は恒等 (使用前に valid() を確認すること)
}

AcousticResult<BandFilter> BandFilter::design(double lowHz, double highHz,
                                              double sampleRateHz) {
    typedef AcousticResult<BandFilter> Result;
    if (sampleRateHz <= 0.0)
        return Result::error(AcousticErrorCode::UnsupportedSampleRate,
                             "sample rate must be positive");
    if (lowHz <= 0.0 || highHz <= lowHz)
        return Result::error(AcousticErrorCode::InvalidArgument,
                             "band edges must satisfy 0 < lowHz < highHz");
    if (highHz >= 0.5 * sampleRateHz)
        return Result::error(AcousticErrorCode::FilterDesignFailed,
                             "upper band edge reaches Nyquist frequency");

    // プリワーピング (双一次変換用)
    const double w1 = std::tan(kPi * lowHz / sampleRateHz);
    const double w2 = std::tan(kPi * highHz / sampleRateHz);
    const double w0 = std::sqrt(w1 * w2); // 中心 (幾何平均)
    const double bw = w2 - w1;            // 帯域幅

    // 2次バターワース原型極 → 帯域通過 4 極 → 双一次変換で z 領域へ
    const std::complex<double> proto[2] = {std::polar(1.0, 3.0 * kPi / 4.0),
                                           std::polar(1.0, 5.0 * kPi / 4.0)};
    std::complex<double> zp[4];
    int np = 0;
    for (int i = 0; i < 2; ++i) {
        const std::complex<double> bp = proto[i] * bw;
        const std::complex<double> disc =
            std::sqrt(bp * bp - std::complex<double>(4.0 * w0 * w0, 0.0));
        const std::complex<double> s1 = 0.5 * (bp + disc);
        const std::complex<double> s2 = 0.5 * (bp - disc);
        zp[np++] = (1.0 + s1) / (1.0 - s1);
        zp[np++] = (1.0 + s2) / (1.0 - s2);
    }
    for (int i = 0; i < 4; ++i) {
        if (std::abs(zp[i]) >= 1.0)
            return Result::error(AcousticErrorCode::FilterDesignFailed,
                                 "unstable pole after bilinear transform");
    }

    // 分母多項式 Π(1 - p·z^-1)
    std::complex<double> ac[5] = {std::complex<double>(1.0, 0.0)};
    std::size_t deg = 0;
    for (int i = 0; i < 4; ++i) multiplyPoleFactor(ac, deg, zp[i]);

    BandFilter f;
    for (int i = 0; i < 5; ++i) f.m_a[i] = ac[i].real();
    // 分子: (1 - z^-1)²(1 + z^-1)² = 1 - 2 z^-2 + z^-4 (ゲインは後で正規化)
    const double num[5] = {1.0, 0.0, -2.0, 0.0, 1.0};

    // 中心周波数 (双一次変換後の対応周波数 ω0 = 2·atan(w0)) で振幅 1 に正規化
    const double omega0 = 2.0 * std::atan(w0);
    const std::complex<double> zi =
        std::exp(std::complex<double>(0.0, -omega0)); // z^-1
    std::complex<double> nv(0.0, 0.0), dv(0.0, 0.0), zk(1.0, 0.0);
    for (int k = 0; k < 5; ++k) {
        nv += num[k] * zk;
        dv += f.m_a[k] * zk;
        zk *= zi;
    }
    const double mag = std::abs(nv / dv);
    if (!(mag > 0.0) || !std::isfinite(mag))
        return Result::error(AcousticErrorCode::FilterDesignFailed,
                             "gain normalization failed");
    const double g = 1.0 / mag;
    for (int i = 0; i < 5; ++i) f.m_b[i] = g * num[i];
    f.m_valid = true;
    return Result::ok(f);
}

namespace {
// 直接形 II 転置による 4 次 IIR
void runIir(const double b[5], const double a[5], const double *x,
            std::size_t n, double *y) {
    double s1 = 0.0, s2 = 0.0, s3 = 0.0, s4 = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double xi = x[i];
        const double yi = b[0] * xi + s1;
        s1 = b[1] * xi - a[1] * yi + s2;
        s2 = b[2] * xi - a[2] * yi + s3;
        s3 = b[3] * xi - a[3] * yi + s4;
        s4 = b[4] * xi - a[4] * yi;
        y[i] = yi;
    }
}
} // namespace

std::vector<double> BandFilter::apply(ArrayView<const double> x,
                                      bool zeroPhase) const {
    std::vector<double> y(x.size(), 0.0);
    if (x.empty() || !m_valid) return y;
    runIir(m_b, m_a, x.data(), x.size(), y.data());
    if (zeroPhase) {
        // 前後方向 (filtfilt 相当): 反転 → 再フィルタ → 反転 でゼロ位相
        std::reverse(y.begin(), y.end());
        std::vector<double> y2(y.size(), 0.0);
        runIir(m_b, m_a, y.data(), y.size(), y2.data());
        std::reverse(y2.begin(), y2.end());
        return y2;
    }
    return y;
}

AcousticResult<std::vector<double>> filterBand(ArrayView<const double> x,
                                               double sampleRateHz,
                                               const Band &band,
                                               bool zeroPhase) {
    typedef AcousticResult<std::vector<double>> Result;
    if (x.empty())
        return Result::error(AcousticErrorCode::EmptyInput, "input is empty");
    if (band.fullBand) {
        // 全帯域: フィルタを適用せず複製を返す (自動正規化もしない)
        return Result::ok(std::vector<double>(x.begin(), x.end()));
    }
    AcousticResult<BandFilter> f =
        BandFilter::design(band.lowHz, band.highHz, sampleRateHz);
    if (!f.success()) return Result::error(f.errorCode(), f.message());
    return Result::ok(f.value().apply(x, zeroPhase));
}

} // namespace acoustics
} // namespace ofd
