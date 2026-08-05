// EnvironmentalNoise.cpp — 屋外騒音伝搬の初等計算 (実装)。
// 式の出典はヘッダのコメントを参照。
#include "EnvironmentalNoise.h"

#include <cmath>

namespace ofd {
namespace acoustics {
namespace outdoor {

// 前川のチャートは N ≳ 20 で 24 dB 付近に飽和する
const double kMaekawaMaxDb = 24.0;

namespace {

bool isPositive(double v) { return v == v && v > 0.0 && v < 1e300; }

double log10safe(double v) { return std::log10(v); }

} // namespace

// ── 幾何拡散 ────────────────────────────────────────────────────────────────
double divergencePoint(double distM)
{
    if (!isPositive(distM)) return 0.0;
    return 20.0 * log10safe(distM) + 11.0;
}

double divergenceLine(double distM)
{
    if (!isPositive(distM)) return 0.0;
    return 10.0 * log10safe(distM) + 8.0;
}

double divergenceRelative(double distM, double refDistM, bool lineSource)
{
    if (!isPositive(distM) || !isPositive(refDistM)) return 0.0;
    const double k = lineSource ? 10.0 : 20.0;
    return k * log10safe(distM / refDistM);
}

double pointSourceLevelAt1m(double pwlDb)
{
    // ISO 9613-2 §7.1: L_p = L_W + D_c − A_div、D_c = 0 (無指向・全空間)、
    // d = 1 m で A_div = 11 dB
    return pwlDb - 11.0;
}

// ── 前川チャート ────────────────────────────────────────────────────────────
double fresnelNumber(double pathDiffM, double freqHz, double soundSpeedMs)
{
    if (!isPositive(freqHz) || !isPositive(soundSpeedMs)) return 0.0;
    const double lambda = soundSpeedMs / freqHz;
    if (!isPositive(lambda)) return 0.0;
    return 2.0 * pathDiffM / lambda;
}

double maekawaAttenuation(double fresnelN)
{
    if (!(fresnelN > 0.0)) return 0.0;     // 見通し領域 → 減衰を計上しない
    const double dl = 10.0 * log10safe(3.0 + 20.0 * fresnelN);
    if (dl < 0.0) return 0.0;
    return dl > kMaekawaMaxDb ? kMaekawaMaxDb : dl;
}

BarrierResult barrierDiffraction(const BarrierGeometry &g, double freqHz,
                                 double soundSpeedMs)
{
    BarrierResult r;
    if (!isPositive(g.recvDistM) || !isPositive(freqHz)
        || !isPositive(soundSpeedMs))
        return r;
    if (!(g.barDistM > 0.0) || !(g.barDistM < g.recvDistM))
        return r;   // 壁が音源位置または受音点より遠い → 断面が成立しない
    if (!(g.barHeightM > 0.0)) return r;

    r.valid = true;
    r.wavelengthM = soundSpeedMs / freqHz;

    // 見通し線 (音源→受音点) の壁位置での高さ
    const double t = g.barDistM / g.recvDistM;
    const double losHeight = g.srcHeightM + t * (g.recvHeightM - g.srcHeightM);
    r.shadow = (g.barHeightM > losHeight);

    // 経路差 δ = (音源→頂部) + (頂部→受音点) − (音源→受音点)
    const double dx1 = g.barDistM;
    const double dy1 = g.barHeightM - g.srcHeightM;
    const double dx2 = g.recvDistM - g.barDistM;
    const double dy2 = g.barHeightM - g.recvHeightM;
    const double dxd = g.recvDistM;
    const double dyd = g.recvHeightM - g.srcHeightM;
    const double a = std::sqrt(dx1 * dx1 + dy1 * dy1);
    const double b = std::sqrt(dx2 * dx2 + dy2 * dy2);
    const double d = std::sqrt(dxd * dxd + dyd * dyd);
    r.pathDiffM = a + b - d;
    if (!r.shadow) {
        // 見通しがある場合、経路差は幾何的には正でも回折減衰は計上しない
        r.fresnelN = 0.0;
        r.attenDb = 0.0;
        return r;
    }

    r.fresnelN = fresnelNumber(r.pathDiffM, freqHz, soundSpeedMs);
    const double raw = (r.fresnelN > 0.0)
                           ? 10.0 * log10safe(3.0 + 20.0 * r.fresnelN)
                           : 0.0;
    r.attenDb = maekawaAttenuation(r.fresnelN);
    r.clamped = (raw > kMaekawaMaxDb);
    return r;
}

// ── 環境基準 (平成 10 年環境庁告示第 64 号) ─────────────────────────────────
EnvStandard environmentalStandardJp(int areaType)
{
    EnvStandard s;
    switch (areaType) {
    case AreaAA:        s.dayDb = 50; s.nightDb = 40; break;
    case AreaA:         s.dayDb = 55; s.nightDb = 45; break;
    case AreaB:         s.dayDb = 55; s.nightDb = 45; break;
    case AreaC:         s.dayDb = 60; s.nightDb = 50; break;
    case AreaRoadA:     s.dayDb = 60; s.nightDb = 55; break;
    case AreaRoadBC:    s.dayDb = 65; s.nightDb = 60; break;
    case AreaRoadTrunk: s.dayDb = 70; s.nightDb = 65; break;
    default:            return s;    // valid = false
    }
    s.valid = true;
    return s;
}

// ── 断面予測 ────────────────────────────────────────────────────────────────
PredictionResult predictLevel(const SiteModel &m, double recvDistM,
                              double recvHeightM)
{
    PredictionResult r;
    if (!isPositive(recvDistM) || !isPositive(m.refDistM)) return r;

    r.valid = true;
    if (m.divergenceEnabled)
        r.aDivDb = divergenceRelative(recvDistM, m.refDistM, m.lineSource);

    if (m.barrierEnabled) {
        BarrierGeometry g;
        g.srcHeightM  = m.srcHeightM;
        g.barDistM    = m.barDistM;
        g.barHeightM  = m.barHeightM;
        g.recvDistM   = recvDistM;
        g.recvHeightM = recvHeightM;
        r.barrier = barrierDiffraction(g, m.evalFreqHz, m.soundSpeedMs);
        r.aBarDb  = r.barrier.attenDb;
    }

    r.levelDb = m.refLevelDb - r.aDivDb - r.aBarDb;
    return r;
}

double distanceForLevel(const SiteModel &m, double targetDb,
                        double recvHeightM, double dMinM, double dMaxM)
{
    if (!isPositive(dMinM) || !isPositive(dMaxM) || !(dMinM < dMaxM))
        return 0.0;

    const PredictionResult lo = predictLevel(m, dMinM, recvHeightM);
    const PredictionResult hi = predictLevel(m, dMaxM, recvHeightM);
    if (!lo.valid || !hi.valid) return 0.0;
    // L は距離に対し単調非増加。target が区間外なら挟めない
    if (lo.levelDb < targetDb || hi.levelDb > targetDb) return 0.0;

    double a = dMinM, b = dMaxM;
    for (int i = 0; i < 80; ++i) {
        const double mid = 0.5 * (a + b);
        const PredictionResult p = predictLevel(m, mid, recvHeightM);
        if (!p.valid) return 0.0;
        if (p.levelDb >= targetDb) a = mid;
        else                       b = mid;
    }
    return 0.5 * (a + b);
}

} // namespace outdoor
} // namespace acoustics
} // namespace ofd
