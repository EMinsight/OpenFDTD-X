// SoundInsulation.cpp — 建築遮音の予測と単一数値評価 (実装)。
// 式の出典はヘッダのコメントを参照。
#include "SoundInsulation.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace ofd {
namespace acoustics {
namespace insulation {

namespace {
const double kPi = 3.14159265358979323846;

double clampd(double v, double lo, double hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

bool isFinite(double v) { return v == v && v > -1e300 && v < 1e300; }

// ── 基準曲線・スペクトル (いずれも 100..3150 / 125..4000 Hz の 16 帯域) ──
// ISO 717-1:2013 表 1 (空気音遮断性能の基準曲線, 100..3150 Hz)
const double kIsoRefR[kIsoCount] = {
    33, 36, 39, 42, 45, 48, 51, 52, 53, 54, 55, 56, 56, 56, 56, 56
};
// ASTM E413 の基準等級曲線 (125..4000 Hz)。500 Hz を 0 とした相対値。
const double kAstmContour[kAstmCount] = {
    -16, -13, -10, -7, -4, -1, 0, 1, 2, 3, 4, 4, 4, 4, 4, 4
};
// ISO 717-2:2013 表 1 (床衝撃音遮断性能の基準曲線, 100..3150 Hz)
const double kIsoRefLn[kIsoCount] = {
    62, 62, 62, 62, 62, 62, 61, 60, 59, 58, 57, 54, 51, 48, 45, 42
};
// ISO 717-1:2013 表 2 スペクトル No.1 (ピンクノイズ A 特性) / No.2 (交通騒音)
// いずれも Σ10^(Li/10) = 1 (A 特性総和 0 dB) に正規化されている。
const double kSpectrum1[kIsoCount] = {
    -29, -26, -23, -21, -19, -17, -15, -13, -12, -11, -10, -9, -9, -9, -9, -9
};
const double kSpectrum2[kIsoCount] = {
    -20, -20, -18, -16, -15, -14, -13, -12, -11, -9, -8, -9, -10, -11, -13, -15
};
const int kIndex500Iso  = 7;   // 100 Hz から数えて 500 Hz
const int kIndex500Astm = 6;   // 125 Hz から数えて 500 Hz

// 1 枚の葉 (連続する実体層のまとまり)
struct Leaf {
    double mass;    // 面密度 [kg/m²]
    double fc;      // 限界周波数 [Hz] (0 = 剛性不明)
    double eta;     // 損失係数 (面密度加重平均)
    Leaf() : mass(0), fc(0), eta(0.01) {}
};

// 単一葉の音響透過損失 (Sharp 1973)。η は EN 12354-1 の全損失係数を使う。
//   f < 0.5fc : 場入射質量則         R = 20log10(m f) − 48
//   f >= fc   : コインシデンス域     R = 20log10(m f) + 10log10(2ηf/(πfc)) − 45
//   0.5fc..fc : 上の 2 値を log f 上で直線補間
// fc <= 0 (剛性不明) のときは全帯域で質量則のみ。
double singleLeafR(double f, double mass, double fc, double etaInternal)
{
    if (f <= 0 || mass <= 0) return 0.0;
    const double massR = 20.0 * std::log10(mass * f) - 48.0;
    if (fc <= 0) return std::max(0.0, massR);
    const double half = 0.5 * fc;
    if (f < half) return std::max(0.0, massR);
    const double eFc = totalLossFactor(etaInternal, mass, fc);
    const double atFc = 20.0 * std::log10(mass * fc) - 45.0
                      + 10.0 * std::log10(2.0 * eFc / kPi);
    if (f >= fc) {
        const double e = totalLossFactor(etaInternal, mass, f);
        const double R = 20.0 * std::log10(mass * f) - 45.0
                       + 10.0 * std::log10(2.0 * e * f / (kPi * fc));
        return std::max(0.0, R);
    }
    const double atHalf = 20.0 * std::log10(mass * half) - 48.0;
    const double t = (std::log10(f) - std::log10(half))
                   / (std::log10(fc) - std::log10(half));
    return std::max(0.0, atHalf + t * (atFc - atHalf));
}
} // namespace

const double kThirdOctaveHz[kNumBands] = {
    50, 63, 80, 100, 125, 160, 200, 250, 315, 400, 500,
    630, 800, 1000, 1250, 1600, 2000, 2500, 3150, 4000, 5000
};

// 20 ℃ 1 気圧の空気
const double kRhoC       = 413.6;   // ρ0·c0 [Pa·s/m]
const double kSoundSpeed = 343.0;   // c0 [m/s]

TlResult::TlResult()
    : valid(false), model(ModelNone), leafCount(0), reducedToTwoLeaves(false),
      cavityAbsorbed(false), surfaceMass(0), cavityDepthM(0),
      massAirMassHz(0), limitingHz(0), lossFactor500(0)
{
    leafMass[0] = leafMass[1] = 0;
    leafCriticalHz[0] = leafCriticalHz[1] = 0;
    for (int i = 0; i < kNumBands; ++i) R[i] = 0;
}

double criticalFrequency(double youngsPa, double poisson, double densityKgM3,
                         double thicknessM)
{
    if (youngsPa <= 0 || densityKgM3 <= 0 || thicknessM <= 0) return 0.0;
    const double nu = clampd(poisson, 0.0, 0.49);
    const double mass = densityKgM3 * thicknessM;                 // [kg/m²]
    const double bend = youngsPa * thicknessM * thicknessM * thicknessM
                      / (12.0 * (1.0 - nu * nu));                 // B' [N·m]
    if (bend <= 0) return 0.0;
    return kSoundSpeed * kSoundSpeed / (2.0 * kPi) * std::sqrt(mass / bend);
}

double fieldIncidenceMassLaw(double freqHz, double surfaceMass)
{
    if (freqHz <= 0 || surfaceMass <= 0) return 0.0;
    return std::max(0.0, 20.0 * std::log10(surfaceMass * freqHz) - 48.0);
}

double totalLossFactor(double internalEta, double surfaceMass, double freqHz)
{
    double e = std::max(0.0, internalEta);
    if (surfaceMass > 0 && freqHz > 0)
        e += surfaceMass / (485.0 * std::sqrt(freqHz));
    return clampd(e, 1e-4, 0.5);
}

TlResult transmissionLoss(const std::vector<Layer> &layers, bool decoupled)
{
    TlResult out;

    // 層列を「葉」と「空隙」に分解する。連続する実体層は 1 枚の葉、
    // 連続する空隙層はまとめて 1 つの空隙とする。
    std::vector<Leaf>   leaves;
    std::vector<double> gaps;        // 葉と葉の間の空隙厚 [m]
    std::vector<bool>   gapAbsorb;   // その空隙に吸音材充填があるか
    Leaf   cur;
    bool   curOpen = false;
    double gap = 0;
    bool   gapFill = false;
    bool   pendingGap = false;

    for (std::size_t i = 0; i < layers.size(); ++i) {
        const Layer &L = layers[i];
        if (L.thicknessM <= 0) continue;
        if (L.cavity || L.densityKgM3 <= 0) {
            // 空隙層 (空気層・多孔質充填)。葉と葉の間にあるときだけ意味を持つ。
            if (curOpen) {
                leaves.push_back(cur);
                cur = Leaf();
                curOpen = false;
                gap = 0;
                gapFill = false;
                pendingGap = true;
            }
            if (pendingGap) {
                gap += L.thicknessM;
                gapFill = gapFill || L.porousFill || L.densityKgM3 > 0;
            }
            continue;
        }
        if (pendingGap) {
            gaps.push_back(gap);
            gapAbsorb.push_back(gapFill);
            pendingGap = false;
        }
        const double m = L.surfaceMass();
        if (m <= 0) continue;
        const double fc = criticalFrequency(L.youngsPa, L.poisson,
                                            L.densityKgM3, L.thicknessM);
        // 葉の限界周波数は「もっとも低い fc」を採る (最初に現れるディップ)。
        if (fc > 0 && (cur.fc <= 0 || fc < cur.fc)) cur.fc = fc;
        cur.eta = curOpen ? (cur.eta * cur.mass + L.lossFactor * m)
                                / (cur.mass + m)
                          : L.lossFactor;
        cur.mass += m;
        curOpen = true;
    }
    if (curOpen) leaves.push_back(cur);

    if (leaves.empty()) return out;   // 実体層が無い → 計算しない

    out.leafCount = static_cast<int>(leaves.size());
    for (std::size_t i = 0; i < leaves.size(); ++i)
        out.surfaceMass += leaves[i].mass;

    // 3 枚以上の葉は「もっとも厚い空隙」で 2 葉へ集約する (近似)。
    Leaf a = leaves[0], b;
    double depth = 0;
    bool   absorbed = false;
    if (leaves.size() >= 2) {
        std::size_t split = 0;
        for (std::size_t g = 1; g < gaps.size(); ++g)
            if (gaps[g] > gaps[split]) split = g;
        if (gaps.empty()) split = 0;
        a = Leaf();
        b = Leaf();
        for (std::size_t i = 0; i < leaves.size(); ++i) {
            Leaf &dst = (i <= split) ? a : b;
            const double m = leaves[i].mass;
            dst.eta = (dst.mass + m > 0)
                          ? (dst.eta * dst.mass + leaves[i].eta * m)
                                / (dst.mass + m)
                          : leaves[i].eta;
            dst.mass += m;
            if (leaves[i].fc > 0 && (dst.fc <= 0 || leaves[i].fc < dst.fc))
                dst.fc = leaves[i].fc;
        }
        depth = gaps.empty() ? 0 : gaps[split];
        absorbed = gaps.empty() ? false : gapAbsorb[split];
        out.reducedToTwoLeaves = (leaves.size() > 2);
    }

    out.cavityDepthM   = depth;
    out.cavityAbsorbed = absorbed;

    if (!decoupled || leaves.size() < 2 || depth <= 0
        || a.mass <= 0 || b.mass <= 0) {
        // 単一壁 — 全層の面密度を合算し、もっとも低い fc をコインシデンスに使う
        Leaf s;
        for (std::size_t i = 0; i < leaves.size(); ++i) {
            const double m = leaves[i].mass;
            s.eta = (s.mass + m > 0)
                        ? (s.eta * s.mass + leaves[i].eta * m) / (s.mass + m)
                        : leaves[i].eta;
            s.mass += m;
            if (leaves[i].fc > 0 && (s.fc <= 0 || leaves[i].fc < s.fc))
                s.fc = leaves[i].fc;
        }
        out.model = ModelSingleLeaf;
        out.leafMass[0] = s.mass;
        out.leafCriticalHz[0] = s.fc;
        out.cavityDepthM = 0;
        out.reducedToTwoLeaves = false;
        out.lossFactor500 = totalLossFactor(s.eta, s.mass, 500.0);
        for (int i = 0; i < kNumBands; ++i)
            out.R[i] = singleLeafR(kThirdOctaveHz[i], s.mass, s.fc, s.eta);
        out.valid = (s.mass > 0);
        return out;
    }

    // 二重壁 (Sharp)
    out.model = ModelDoubleLeaf;
    out.leafMass[0] = a.mass;
    out.leafMass[1] = b.mass;
    out.leafCriticalHz[0] = a.fc;
    out.leafCriticalHz[1] = b.fc;
    // 質量-空気-質量共鳴 f0 = (1/2π)·√(ρ0c²(m1+m2)/(d·m1·m2))、ρ0c² = 1.4e5 Pa
    const double rhoC2 = 1.4e5;
    out.massAirMassHz = 1.0 / (2.0 * kPi)
                      * std::sqrt(rhoC2 * (a.mass + b.mass)
                                  / (depth * a.mass * b.mass));
    out.limitingHz = 55.0 / depth;   // fl = 55/d [Hz] (d [m])
    out.lossFactor500 = totalLossFactor(a.eta, a.mass, 500.0);
    const double mTot = a.mass + b.mass;
    for (int i = 0; i < kNumBands; ++i) {
        const double f = kThirdOctaveHz[i];
        double R;
        if (f <= out.massAirMassHz) {
            // 共鳴以下は合計質量の質量則 (剛性は効かない)
            R = fieldIncidenceMassLaw(f, mTot);
        } else {
            const double R1 = singleLeafR(f, a.mass, a.fc, a.eta);
            const double R2 = singleLeafR(f, b.mass, b.fc, b.eta);
            R = (f < out.limitingHz)
                    ? R1 + R2 + 20.0 * std::log10(f * depth) - 29.0
                    : R1 + R2 + 6.0;
        }
        out.R[i] = std::max(0.0, R);
    }
    out.valid = true;
    return out;
}

// ── 単一数値評価 ────────────────────────────────────────────────────────────
RatingResult contourRating(const double *measured, const double *ref, int n,
                           int index500, double maxSum, double maxSingle,
                           bool higherIsBetter)
{
    RatingResult r;
    if (!measured || !ref || n <= 0 || index500 < 0 || index500 >= n) return r;
    for (int i = 0; i < n; ++i)
        if (!isFinite(measured[i])) return r;

    const int kSpan = 400;   // ±400 dB あればあらゆる実測値を覆える
    for (int step = 0; step <= 2 * kSpan; ++step) {
        // R 系は「上へ寄せられるだけ寄せる」= 大きい shift から探す。
        // Ln 系は「下へ寄せられるだけ寄せる」= 小さい shift から探す。
        const int shift = higherIsBetter ? (kSpan - step) : (step - kSpan);
        double sum = 0, worst = 0;
        for (int i = 0; i < n; ++i) {
            const double d = higherIsBetter
                                 ? (ref[i] + shift - measured[i])
                                 : (measured[i] - (ref[i] + shift));
            if (d > 0) {
                sum += d;
                if (d > worst) worst = d;
            }
        }
        const bool ok = (sum <= maxSum + 1e-9)
                     && (maxSingle <= 0 || worst <= maxSingle + 1e-9);
        if (!ok) continue;
        r.valid = true;
        r.shift = shift;
        r.sumDeficiency = sum;
        r.maxDeficiency = worst;
        r.value = static_cast<int>(std::floor(ref[index500] + shift + 0.5));
        return r;
    }
    return r;
}

RatingResult weightedReduction(const double *R21)
{
    if (!R21) return RatingResult();
    return contourRating(R21 + kIsoFirst, kIsoRefR, kIsoCount, kIndex500Iso,
                         32.0, 0.0, true);
}

RatingResult soundTransmissionClass(const double *R21)
{
    if (!R21) return RatingResult();
    // ASTM E413: 不利偏差の合計 32 dB 以下、かつ 1 帯域 8 dB 以下
    return contourRating(R21 + kAstmFirst, kAstmContour, kAstmCount,
                         kIndex500Astm, 32.0, 8.0, true);
}

RatingResult weightedImpact(const double *Ln21)
{
    if (!Ln21) return RatingResult();
    return contourRating(Ln21 + kIsoFirst, kIsoRefLn, kIsoCount, kIndex500Iso,
                         32.0, 0.0, false);
}

int spectrumAdaptation(const double *R21, int spectrumKind, int rwValue,
                       bool *ok)
{
    if (ok) *ok = false;
    if (!R21) return 0;
    const double *L = (spectrumKind == SpectrumTraffic) ? kSpectrum2
                                                        : kSpectrum1;
    double sum = 0;
    for (int i = 0; i < kIsoCount; ++i) {
        const double R = R21[kIsoFirst + i];
        if (!isFinite(R)) return 0;
        sum += std::pow(10.0, (L[i] - R) / 10.0);
    }
    if (sum <= 0) return 0;
    const double xa = -10.0 * std::log10(sum);
    if (ok) *ok = true;
    // ISO 717-1 6.2: X_A を整数に丸めてから Rw を引く
    return static_cast<int>(std::floor(xa + 0.5)) - rwValue;
}

// ── 現場・複合の標準式 ──────────────────────────────────────────────────────
double compositeReduction(const double *areas, const double *R, int n)
{
    if (!areas || !R || n <= 0) return 0.0;
    double sTot = 0, tauS = 0;
    for (int i = 0; i < n; ++i) {
        if (areas[i] <= 0) continue;
        sTot += areas[i];
        // R が有限でない要素は「開口 (τ = 1)」として扱わず無視する
        if (!isFinite(R[i])) continue;
        tauS += areas[i] * std::pow(10.0, -R[i] / 10.0);
    }
    if (sTot <= 0 || tauS <= 0) return 0.0;
    return -10.0 * std::log10(tauS / sTot);
}

double receivingLevel(double lp1, double R, double areaM2, double absorptionA)
{
    if (areaM2 <= 0 || absorptionA <= 0) return 0.0;
    return lp1 - R + 10.0 * std::log10(areaM2 / absorptionA);
}

double sabineAbsorption(double volumeM3, double rt60S)
{
    if (volumeM3 <= 0 || rt60S <= 0) return 0.0;
    return 0.161 * volumeM3 / rt60S;
}

double standardizedLevelDifference(double Rw, double volumeM3, double areaM2)
{
    if (volumeM3 <= 0 || areaM2 <= 0) return 0.0;
    return Rw + 10.0 * std::log10(0.32 * volumeM3 / areaM2);
}

double enclosureInsertionLoss(double Rwall, double wallArea, double openArea,
                              double interiorAlpha)
{
    const double total = wallArea + openArea;
    if (wallArea <= 0 || total <= 0) return 0.0;
    const double alpha = clampd(interiorAlpha, 0.01, 1.0);
    // 開口は τ = 1 として面積加重の τ 平均を採る
    const double tau = (wallArea * std::pow(10.0, -Rwall / 10.0) + openArea)
                     / total;
    if (tau <= 0) return 0.0;
    const double Reff = -10.0 * std::log10(tau);
    // A_in = S·ᾱ なので 10log10(S/A) = −10log10(ᾱ)
    return Reff + 10.0 * std::log10(alpha);
}

// ── ダクト系 ────────────────────────────────────────────────────────────────
double linedDuctAttenuation(double alpha, double perimeterM, double areaM2)
{
    if (alpha <= 0 || perimeterM <= 0 || areaM2 <= 0) return 0.0;
    const double a = clampd(alpha, 0.0, 1.0);
    return 1.05 * std::pow(a, 1.4) * perimeterM / areaM2;
}

double elbowAttenuation(double freqHz, double widthM, bool lined)
{
    if (freqHz <= 0 || widthM <= 0) return 0.0;
    // ASHRAE の表は f·w (w はインチ) を引数に取る
    const double fw = freqHz * widthM / 0.0254;
    if (lined) {
        if (fw < 48)  return 0.0;
        if (fw < 96)  return 1.0;
        if (fw < 190) return 6.0;
        if (fw < 380) return 11.0;
        return 10.0;
    }
    if (fw < 48)  return 0.0;
    if (fw < 96)  return 1.0;
    if (fw < 190) return 5.0;
    if (fw < 380) return 8.0;
    return 4.0;
}

double branchAttenuation(double branchAreaM2, double totalAreaM2)
{
    if (branchAreaM2 <= 0 || totalAreaM2 <= 0) return 0.0;
    if (branchAreaM2 >= totalAreaM2) return 0.0;
    return 10.0 * std::log10(totalAreaM2 / branchAreaM2);
}

double endReflectionLoss(double freqHz, double areaM2, bool flanged)
{
    if (freqHz <= 0 || areaM2 <= 0) return 0.0;
    const double a = std::sqrt(areaM2 / kPi);            // 等価半径 [m]
    const double ka = 2.0 * kPi * freqHz / kSoundSpeed * a;
    double t = ka * ka;                                  // フランジ付き
    if (!flanged) t *= 0.5;                              // 自由端
    if (t >= 1.0) return 0.0;
    return -10.0 * std::log10(t);
}

double reverberantLevel(double pwl, double absorptionA)
{
    if (absorptionA <= 0) return 0.0;
    return pwl + 10.0 * std::log10(4.0 / absorptionA);
}

// ── STI ─────────────────────────────────────────────────────────────────────
namespace {
// IEC 60268-16 の重み係数 (男声)。125 Hz〜8 kHz の 7 オクターブ帯域。
// Σα − Σβ = 1.381 − 0.381 = 1.000 → 全帯域 MTI = 1 のとき STI = 1。
const double kAlpha[7] = { 0.085, 0.127, 0.230, 0.233, 0.309, 0.224, 0.173 };
const double kBeta[6]  = { 0.085, 0.078, 0.065, 0.011, 0.047, 0.095 };
// 変調周波数 (1/3 オクターブ 0.63〜12.5 Hz, 14 点)
const double kModHz[14] = { 0.63, 0.8, 1.0, 1.25, 1.6, 2.0, 2.5,
                            3.15, 4.0, 5.0, 6.3, 8.0, 10.0, 12.5 };

double bandMti(double rt60S, double snrDb)
{
    const double T = std::max(0.0, rt60S);
    // S/N による変調度の低下 (IEC 60268-16 の雑音項)
    const double snFactor = 1.0 / (1.0 + std::pow(10.0, -snrDb / 10.0));
    double sum = 0;
    for (int i = 0; i < 14; ++i) {
        const double x = 2.0 * kPi * kModHz[i] * T / 13.8;
        const double m = clampd(snFactor / std::sqrt(1.0 + x * x), 0.0, 0.9999);
        double snApp = 10.0 * std::log10(m / std::max(1e-9, 1.0 - m));
        snApp = clampd(snApp, -15.0, 15.0);
        sum += (snApp + 15.0) / 30.0;
    }
    return sum / 14.0;
}
} // namespace

double stiBands(const double rt60Bands[7], const double snrBands[7])
{
    if (!rt60Bands || !snrBands) return 0.0;
    double mti[7];
    for (int k = 0; k < 7; ++k) mti[k] = bandMti(rt60Bands[k], snrBands[k]);
    double s = 0;
    for (int k = 0; k < 7; ++k) s += kAlpha[k] * mti[k];
    for (int k = 0; k < 6; ++k) s -= kBeta[k] * std::sqrt(mti[k] * mti[k + 1]);
    return clampd(s, 0.0, 1.0);
}

double sti(double rt60S, double snrDb)
{
    double t[7], s[7];
    for (int k = 0; k < 7; ++k) { t[k] = rt60S; s[k] = snrDb; }
    return stiBands(t, s);
}

} // namespace insulation
} // namespace acoustics
} // namespace ofd
