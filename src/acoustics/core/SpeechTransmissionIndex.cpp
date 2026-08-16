// SpeechTransmissionIndex.cpp — STI (SpeechTransmissionIndex.h 参照)
#include "SpeechTransmissionIndex.h"

#include <algorithm>
#include <cmath>

#include "BandFilter.h"

namespace ofd {
namespace acoustics {

namespace {

// M_PI は MSVC の <cmath> だけでは定義されない (.claude/rules/cpp-qt.md)
const double kPi = 3.14159265358979323846;

// IEC 60268-16: 変調周波数 14 点 (1/3 オクターブ 0.63〜12.5 Hz)
const double kFm[14] = {
    0.63, 0.80, 1.00, 1.25, 1.60, 2.00, 2.50,
    3.15, 4.00, 5.00, 6.30, 8.00, 10.00, 12.50
};

// STI の 7 オクターブ帯域
const double kBandCenters[7] = { 125.0, 250.0, 500.0, 1000.0,
                                 2000.0, 4000.0, 8000.0 };

// IEC 60268-16 (第 4 版) 男声の重み。α は帯域重み、β は隣接帯域の冗長性補正。
// β[k] は帯域 k と k+1 の間に掛かる (6 個)。
const double kAlphaMale[7] = { 0.085, 0.127, 0.230, 0.233, 0.309, 0.224, 0.173 };
const double kBetaMale[6]  = { 0.085, 0.078, 0.065, 0.011, 0.047, 0.095 };

} // namespace

const double *stiModulationFrequencies(std::size_t *count)
{
    if (count) *count = 14;
    return kFm;
}

const double *stiOctaveBands(std::size_t *count)
{
    if (count) *count = 7;
    return kBandCenters;
}

StiResult computeSti(ArrayView<const double> rir, double sampleRateHz,
                     std::size_t directIndex)
{
    StiResult out;

    if (sampleRateHz <= 0.0 || rir.size() == 0 || directIndex >= rir.size()) {
        out.warning = "STI: empty impulse response";
        out.sti = makeInvalidMetric(out.warning);
        return out;
    }
    // 8 kHz 帯を含むため、その上側エッジ (約 11.3 kHz) を通すには
    // fs ≥ 16 kHz が要る。足りないまま計算すると 8 kHz 帯が折り返しで
    // 汚れた値になるので、黙って出さない。
    if (sampleRateHz < 16000.0) {
        out.warning = "STI: sample rate below 16 kHz (8 kHz band unusable)";
        out.sti = makeInvalidMetric(out.warning);
        return out;
    }

    ArrayView<const double> h =
        rir.subview(directIndex, rir.size() - directIndex);
    const std::size_t n = h.size();
    // 最低変調周波数 0.63 Hz の 1 周期 (≈1.6 s) を下回る RIR では
    // 低い変調周波数の積分が 1 周期に満たず、m が過大に出る。
    if (static_cast<double>(n) / sampleRateHz < 1.0 / kFm[0]) {
        out.warning = "STI: impulse response shorter than one period of "
                      "the lowest modulation frequency (0.63 Hz)";
        out.sti = makeInvalidMetric(out.warning);
        return out;
    }

    out.bands.resize(7);
    std::vector<double> mti(7, 0.0);
    bool allValid = true;

    for (std::size_t k = 0; k < 7; ++k) {
        StiBandResult &br = out.bands[k];
        br.centerHz = kBandCenters[k];
        br.mtf.assign(14, 0.0);
        br.ti.assign(14, 0.0);

        // オクターブ帯域 (幾何エッジ)
        const double lo = kBandCenters[k] / std::sqrt(2.0);
        const double hi = kBandCenters[k] * std::sqrt(2.0);
        const Band band("", kBandCenters[k], lo, hi, false);
        AcousticResult<std::vector<double>> filtered =
            filterBand(h, sampleRateHz, band);
        if (!filtered.success()) {
            br.valid = false;
            br.warning = "band filter failed: " + filtered.message();
            allValid = false;
            continue;
        }
        const std::vector<double> &hb = filtered.value();

        // エネルギー包絡 h²(t) の総和
        double denom = 0.0;
        for (std::size_t i = 0; i < hb.size(); ++i) denom += hb[i] * hb[i];
        if (denom <= 1e-300) {
            br.valid = false;
            br.warning = "no energy in band";
            allValid = false;
            continue;
        }

        for (std::size_t j = 0; j < 14; ++j) {
            // m(fm) = |∫h²e^{-j2πfm t}dt| / ∫h²dt
            double re = 0.0, im = 0.0;
            const double w = 2.0 * kPi * kFm[j] / sampleRateHz;
            for (std::size_t i = 0; i < hb.size(); ++i) {
                const double e = hb[i] * hb[i];
                const double ph = w * static_cast<double>(i);
                re += e * std::cos(ph);
                im -= e * std::sin(ph);
            }
            double m = std::sqrt(re * re + im * im) / denom;
            // 数値誤差で 1 をわずかに超えることがある (SNR が発散する)
            m = std::min(m, 0.99999);
            br.mtf[j] = m;

            // SNR_eff → TI。m→0 で -inf になるので下限クリップも要る
            double snr;
            if (m <= 1e-12) snr = -15.0;
            else snr = 10.0 * std::log10(m / (1.0 - m));
            snr = std::max(-15.0, std::min(15.0, snr));
            br.ti[j] = (snr + 15.0) / 30.0;
        }

        double sum = 0.0;
        for (std::size_t j = 0; j < 14; ++j) sum += br.ti[j];
        br.mti = sum / 14.0;
        br.valid = true;
        mti[k] = br.mti;
    }

    if (!allValid) {
        // 一部の帯域が計算できないときは STI を出さない — 欠けた帯域を
        // 0 とみなすと STI が実際より低く出る (誤値になる)
        out.warning = "STI: one or more octave bands could not be computed";
        out.sti = makeInvalidMetric(out.warning);
        out.quality = AnalysisQuality::Invalid;
        return out;
    }

    // STI = Σα·MTI − Σβ·√(MTI_k·MTI_{k+1})
    double s = 0.0;
    for (std::size_t k = 0; k < 7; ++k) s += kAlphaMale[k] * mti[k];
    for (std::size_t k = 0; k < 6; ++k)
        s -= kBetaMale[k] * std::sqrt(mti[k] * mti[k + 1]);
    s = std::max(0.0, std::min(1.0, s));

    out.sti = makeValidMetric(s);
    out.quality = AnalysisQuality::Valid;
    return out;
}

} // namespace acoustics
} // namespace ofd
