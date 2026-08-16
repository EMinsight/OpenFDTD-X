// SoundStrength.cpp — G (音の強さ、SoundStrength.h 参照)
#include "SoundStrength.h"

#include <cmath>

namespace ofd {
namespace acoustics {

namespace {

// エネルギーの下限。これ以下は「無音」として扱う (log で -inf にしない)。
const double kEnergyFloor = 1e-300;

// dB 表示用の下限 (無音を -300 dB とする)
const double kDbFloor = -300.0;

double energyToDb(double e)
{
    return (e > kEnergyFloor) ? 10.0 * std::log10(e) : kDbFloor;
}

// Σx²/fs。非有限値があれば false を返す (黙って 0 として足さない)。
bool sumEnergy(ArrayView<const double> x, double sampleRateHz, double *out)
{
    double s = 0.0;
    for (std::size_t i = 0; i < x.size(); ++i) {
        const double v = x[i];
        if (!(v == v) || v > 1e308 || v < -1e308) return false;  // NaN / Inf
        s += v * v;
    }
    *out = s / sampleRateHz;
    return true;
}

} // namespace

AcousticResult<SoundStrengthReference> makeSoundStrengthReference(
        ArrayView<const double> refIr, double sampleRateHz, double distanceM)
{
    typedef AcousticResult<SoundStrengthReference> R;

    if (refIr.size() == 0)
        return R::error(AcousticErrorCode::EmptyInput,
                        "sound strength reference: empty impulse response");
    if (sampleRateHz <= 0.0)
        return R::error(AcousticErrorCode::UnsupportedSampleRate,
                        "sound strength reference: invalid sample rate");
    if (!(distanceM > 0.0))
        return R::error(AcousticErrorCode::InvalidArgument,
                        "sound strength reference: distance must be positive");

    double e = 0.0;
    if (!sumEnergy(refIr, sampleRateHz, &e))
        return R::error(AcousticErrorCode::NonFiniteSample,
                        "sound strength reference: non-finite sample");
    if (e <= kEnergyFloor)
        return R::error(AcousticErrorCode::EmptyInput,
                        "sound strength reference: silent impulse response");

    SoundStrengthReference ref;
    ref.available = true;
    ref.energy    = e;
    ref.distanceM = distanceM;
    return R::ok(ref);
}

AcousticResult<SoundStrengthReference> makeSoundStrengthReferenceDb(
        double energyDb, double distanceM)
{
    typedef AcousticResult<SoundStrengthReference> R;

    if (!(energyDb == energyDb) || energyDb > 1e308 || energyDb < -1e308)
        return R::error(AcousticErrorCode::InvalidArgument,
                        "sound strength reference: non-finite reference level");
    if (!(distanceM > 0.0))
        return R::error(AcousticErrorCode::InvalidArgument,
                        "sound strength reference: distance must be positive");
    // -300 dB は「無音」の表現なので基準としては受け付けない
    if (energyDb <= kDbFloor)
        return R::error(AcousticErrorCode::InvalidArgument,
                        "sound strength reference: reference level too low");

    SoundStrengthReference ref;
    ref.available = true;
    ref.energy    = std::pow(10.0, energyDb / 10.0);
    ref.distanceM = distanceM;
    return R::ok(ref);
}

SoundStrengthResult computeSoundStrength(ArrayView<const double> rir,
                                         double sampleRateHz,
                                         std::size_t directIndex,
                                         const SoundStrengthReference &ref)
{
    SoundStrengthResult out;

    if (sampleRateHz <= 0.0 || rir.size() == 0 || directIndex >= rir.size()) {
        out.warning = "G: empty impulse response";
        out.g = out.gEarly = out.gLate = makeInvalidMetric(out.warning);
        return out;
    }

    const ArrayView<const double> h =
        rir.subview(directIndex, rir.size() - directIndex);

    double eTotal = 0.0;
    if (!sumEnergy(h, sampleRateHz, &eTotal)) {
        out.warning = "G: non-finite sample";
        out.g = out.gEarly = out.gLate = makeInvalidMetric(out.warning);
        return out;
    }
    out.measuredEnergyDb = energyToDb(eTotal);

    if (eTotal <= kEnergyFloor) {
        out.warning = "G: silent impulse response";
        out.g = out.gEarly = out.gLate = makeInvalidMetric(out.warning);
        return out;
    }

    if (!ref.available || ref.energy <= kEnergyFloor || !(ref.distanceM > 0.0)) {
        // 基準が無いのに 0 dB を出すと「基準と同じ強さ」に見えてしまう。
        out.warning = "G: no free-field reference (10 m) supplied";
        out.g = out.gEarly = out.gLate = makeInvalidMetric(out.warning);
        return out;
    }
    out.referenceEnergyDb = energyToDb(ref.energy);
    // E₁₀ = E_r·(r/10)² → G = 10log10(E/E_r) − 20log10(r/10)
    out.distanceCorrectionDb = -20.0 * std::log10(ref.distanceM / 10.0);

    const double gDb =
        10.0 * std::log10(eTotal / ref.energy) + out.distanceCorrectionDb;
    out.g = makeValidMetric(gDb);
    out.quality = AnalysisQuality::Valid;

    // ── 早期 (〜80 ms) / 後期 (80 ms〜) ──
    // 窓が RIR の外へはみ出す場合は値を出さない (短い RIR で G_early が
    // G と一致してしまい、後期が欠けていることが見えなくなる)。
    const std::size_t n80 =
        static_cast<std::size_t>(0.080 * sampleRateHz + 0.5);
    if (h.size() <= n80) {
        const std::string why =
            "G_early/G_late: impulse response shorter than 80 ms after the "
            "direct sound";
        out.gEarly = makeInvalidMetric(why);
        out.gLate  = makeInvalidMetric(why);
        if (out.warning.empty()) out.warning = why;
        return out;
    }

    double eEarly = 0.0, eLate = 0.0;
    // sumEnergy は上で全体を検査済みなので、ここでの失敗はあり得ない
    sumEnergy(h.first(n80), sampleRateHz, &eEarly);
    sumEnergy(h.subview(n80, h.size() - n80), sampleRateHz, &eLate);

    if (eEarly > kEnergyFloor)
        out.gEarly = makeValidMetric(
            10.0 * std::log10(eEarly / ref.energy) + out.distanceCorrectionDb);
    else
        out.gEarly = makeInvalidMetric("G_early: no energy in 0-80 ms");

    if (eLate > kEnergyFloor)
        out.gLate = makeValidMetric(
            10.0 * std::log10(eLate / ref.energy) + out.distanceCorrectionDb);
    else
        out.gLate = makeInvalidMetric("G_late: no energy after 80 ms");

    return out;
}

} // namespace acoustics
} // namespace ofd
