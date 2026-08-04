// RoomAcoustics.cpp
#include "RoomAcoustics.h"
#include "Project.h"

#include <cmath>

using namespace ofd;

namespace ofd {
namespace roomac {

const double kBandHz[6] = { 125, 250, 500, 1000, 2000, 4000 };

double occupancyFactor(int occupancy)
{
    switch (occupancy) {
        case 0: return 0.70;   // 空席 (椅子のみの吸音)
        case 1: return 0.85;
        default: return 1.0;   // 満席
    }
}

// 空気吸収 4m(f)V の 1kHz 比 (20°C / 50%RH の代表値)。
// UI では A@1k [Sabin] を入力し、帯域値はこの比で換算する。
static const double kAirRatio[6] = { 0.10, 0.20, 0.55, 1.0, 2.1, 6.2 };

double totalAbsorption(const AcousticOpts &a, int band)
{
    band = qBound(0, band, 5);
    double A = 0;
    for (const AbsorptionRow &r : a.absorption) {
        if (!r.enabled) continue;
        if (r.role == AbsorptionRow::Air) {
            A += r.airA * kAirRatio[band];
        } else {
            double alpha = r.alpha[band];
            if (r.role == AbsorptionRow::Audience)
                alpha *= occupancyFactor(a.occupancy);
            A += qBound(0.0, alpha, 1.0) * r.area;
        }
    }
    return A;
}

double rt60(const AcousticOpts &a, int band, int formula)
{
    const double V = std::max(1.0, a.volume);
    const double S = std::max(1.0, a.surface);
    const double A = totalAbsorption(a, band);
    if (A <= 0) return 0;

    if (formula == 0)                      // Sabine
        return 0.161 * V / A;

    // 空気吸収 (Air 行) — Eyring / Fitzroy とも分母へ加算する
    double airA = 0;
    for (const AbsorptionRow &r : a.absorption)
        if (r.enabled && r.role == AbsorptionRow::Air)
            airA += r.airA * kAirRatio[band];

    if (formula == 2) {
        // ── Fitzroy (非均一吸音) ────────────────────────────────────────────
        // D. Fitzroy, "Reverberation Formula Which Seems to Be More Accurate
        // with Nonuniform Distribution of Absorption," J. Acoust. Soc. Am.
        // 31(7), 893-897 (1959).
        //   T = 0.161·V/S² · Σᵢ Sᵢ/(−ln(1−ᾱᵢ))   (i = 直交3方向)
        // 実装形 (空気吸収を各方向の分母へ加算):
        //   T = Σᵢ (Sᵢ/S) · 0.161·V/(−S·ln(1−ᾱᵢ) + A_air)
        // 方向割当: x = 舞台/後壁 (RearWall), y = 側壁 (SideWall),
        //           z = 床/天井/客席 (Floor/Ceiling/Audience,
        //               客席は occupancy 係数適用)。
        // Other 行は方向面積比で3方向へ配分。方向情報のある行が無ければ
        // Eyring へフォールバックする。
        double Sdir[3] = { 0, 0, 0 };   // 方向別の面積
        double Adir[3] = { 0, 0, 0 };   // 方向別の吸音力 Σα·S
        double otherS = 0, otherA = 0;  // 方向情報なし (Other) の行
        for (const AbsorptionRow &r : a.absorption) {
            if (!r.enabled || r.role == AbsorptionRow::Air) continue;
            double alpha = qBound(0.0, r.alpha[band], 1.0);
            int dir = -1;
            switch (r.role) {
                case AbsorptionRow::RearWall: dir = 0; break;   // x
                case AbsorptionRow::SideWall: dir = 1; break;   // y
                case AbsorptionRow::Floor:
                case AbsorptionRow::Ceiling:  dir = 2; break;   // z
                case AbsorptionRow::Audience:                   // z (客席)
                    dir = 2;
                    alpha = qBound(0.0, alpha * occupancyFactor(a.occupancy),
                                   1.0);
                    break;
                default: break;                                 // Other
            }
            if (dir >= 0) {
                Sdir[dir] += r.area;
                Adir[dir] += alpha * r.area;
            } else {
                otherS += r.area;
                otherA += alpha * r.area;
            }
        }
        const double dirTotal = Sdir[0] + Sdir[1] + Sdir[2];
        if (dirTotal <= 0)
            return rt60(a, band, 1);   // 方向情報なし → Eyring フォールバック
        double T = 0;
        for (int i = 0; i < 3; ++i) {
            if (Sdir[i] <= 0) continue;
            const double w = Sdir[i] / dirTotal;   // Other 行の面積比配分
            const double Si = Sdir[i] + otherS * w;
            const double Ai = Adir[i] + otherA * w;
            const double abarI = qBound(0.0, Ai / Si, 0.999);
            const double denomI = -S * std::log(1.0 - abarI) + airA;
            if (denomI > 0)
                T += (Si / S) * 0.161 * V / denomI;
        }
        return T;
    }

    // Eyring: 面吸音は −S·ln(1−ᾱ)、空気吸収 (Air 行) は加算のまま
    const double surfA = std::max(0.0, A - airA);
    const double abar = qBound(0.0, surfA / S, 0.999);
    const double denom = -S * std::log(1.0 - abar) + airA;
    return denom > 0 ? 0.161 * V / denom : 0;
}

double rt60(const AcousticOpts &a, int band)
{
    return rt60(a, band, a.rtFormula);
}

// ── Barron 修正理論 ─────────────────────────────────────────────────────────
// d = 100/r², e+l = (31200·T/V)·e^(−0.04r/T) を t=80ms (C80) / 50ms (C50)
// で分割。G = 10log₁₀(d+e+l)。
// 直接音エネルギー d と残響全エネルギー rev から各指標を組み立てる共通部。
static SeatMetrics metricsFromEnergies(double d, double rev, double T)
{
    SeatMetrics m;
    auto split = [&](double tMs) {   // 0..t の初期エネルギー割合
        return 1.0 - std::exp(-13.8 * (tMs / 1000.0) / T);
    };
    const double e80 = rev * split(80), l80 = rev - e80;
    const double e50 = rev * split(50), l50 = rev - e50;

    m.G = 10.0 * std::log10(std::max(1e-12, d + rev));
    m.C80 = 10.0 * std::log10((d + e80) / std::max(1e-12, l80));
    m.C50 = 10.0 * std::log10((d + e50) / std::max(1e-12, l50));
    m.D50 = (d + e50) / std::max(1e-12, d + rev);
    m.RT = T;
    m.Glate = 10.0 * std::log10(std::max(1e-12, l80));
    // 重心時間 Ts = ∫t·w dt / ∫w dt。直接音は t=0 なので分子に寄与しない。
    // 残響 w(t) = W₀·e^(−13.8t/T) に対し ∫t·w dt = rev·(T/13.8)。
    m.Ts = 1000.0 * rev * (T / 13.8) / std::max(1e-12, d + rev);

    // STI 推定 (Houtgast–Steeneken): 直接音 + 指数残響の MTF。
    //   m(F) = |D + R/(1+jx)| / (D+R),  x = 2πF·T/13.8
    // 変調周波数 0.63..12.5 Hz (1/3oct 14点) の TI 平均。無騒音仮定。
    const double D = d, R = rev;
    double tiSum = 0;
    int n = 0;
    for (int k = 0; k < 14; ++k) {   // 0.63 × 2^(k/3), k=0..13 → 0.63..12.7 Hz
        const double F = 0.63 * std::pow(2.0, k / 3.0);
        const double x = 2.0 * M_PI * F * T / 13.8;
        const double re = D + R / (1.0 + x * x);
        const double im = R * x / (1.0 + x * x);
        const double mtf = std::sqrt(re * re + im * im) / (D + R);
        double snr = 10.0 * std::log10(mtf / std::max(1e-9, 1.0 - mtf));
        snr = qBound(-15.0, snr, 15.0);
        tiSum += (snr + 15.0) / 30.0;
        ++n;
    }
    m.STI = n ? tiSum / n : 0;
    return m;
}

SeatMetrics seatMetrics(double r, double T, double V)
{
    r = std::max(1.0, r);
    T = std::max(0.1, T);
    V = std::max(1.0, V);

    const double d = 100.0 / (r * r);
    const double rev = (31200.0 * T / V) * std::exp(-0.04 * r / T);
    return metricsFromEnergies(d, rev, T);
}

SeatMetrics seatMetrics(const double *r, const double *gainDb, int n,
                        double T, double V)
{
    T = std::max(0.1, T);
    V = std::max(1.0, V);
    double d = 0, rev = 0;
    for (int i = 0; i < n; ++i) {
        const double ri = std::max(1.0, r[i]);
        const double w = std::pow(10.0, (gainDb ? gainDb[i] : 0.0) / 10.0);
        d += w * 100.0 / (ri * ri);
        rev += w * (31200.0 * T / V) * std::exp(-0.04 * ri / T);
    }
    if (n <= 0 || (d <= 0 && rev <= 0)) return SeatMetrics();
    return metricsFromEnergies(d, rev, T);
}

// ── Schroeder 減衰曲線 / 減衰時間 ───────────────────────────────────────────
QVector<QPointF> schroederCurve(double r, double T, double V,
                                double tMax, int nPoints)
{
    QVector<QPointF> out;
    r = std::max(1.0, r);
    T = std::max(0.1, T);
    V = std::max(1.0, V);
    tMax = std::max(1e-3, tMax);
    nPoints = std::max(2, nPoints);

    const double d = 100.0 / (r * r);
    const double rev = (31200.0 * T / V) * std::exp(-0.04 * r / T);
    const double e0 = d + rev;
    out.reserve(nPoints);
    out.push_back({ 0.0, 0.0 });                 // 直接音を含む t=0
    for (int i = 1; i < nPoints; ++i) {
        const double t = tMax * i / double(nPoints - 1);
        const double e = rev * std::exp(-13.8 * t / T);
        out.push_back({ t, 10.0 * std::log10(std::max(1e-30, e / e0)) });
    }
    return out;
}

double decayTimeFromCurve(const QVector<QPointF> &curve,
                          double fromDb, double toDb)
{
    if (curve.size() < 3 || fromDb <= toDb) return 0;
    if (curve.back().y() > toDb) return 0;       // toDb まで減衰していない

    // 評価区間 fromDb ≥ y ≥ toDb の点で最小二乗直線 y = a + b·t を求める。
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    int n = 0;
    for (const QPointF &p : curve) {
        const double y = p.y();
        if (y > fromDb || y < toDb) continue;
        sx += p.x(); sy += y;
        sxx += p.x() * p.x(); sxy += p.x() * y;
        ++n;
    }
    if (n < 3) return 0;
    const double den = n * sxx - sx * sx;
    if (std::fabs(den) < 1e-18) return 0;
    const double b = (n * sxy - sx * sy) / den;  // 傾き [dB/s]
    if (b >= -1e-9) return 0;                    // 減衰していない
    return -60.0 / b;                            // 60 dB 減衰への外挿
}

DecayTimes decayTimes(const QVector<QPointF> &curve)
{
    DecayTimes d;
    d.EDT = decayTimeFromCurve(curve,  0.0, -10.0);
    d.T20 = decayTimeFromCurve(curve, -5.0, -25.0);
    d.T30 = decayTimeFromCurve(curve, -5.0, -35.0);
    d.valid = d.EDT > 0 && d.T20 > 0 && d.T30 > 0;
    return d;
}

// ── エコーグラム (1次鏡像法) ────────────────────────────────────────────────
static double faceAlpha1k(const AcousticOpts &a, int role)
{
    for (const AbsorptionRow &r : a.absorption) {
        if (!r.enabled || r.role != role) continue;
        double al = r.alpha[3];   // 1 kHz
        if (role == AbsorptionRow::Audience)
            al *= occupancyFactor(a.occupancy);
        return qBound(0.0, al, 0.99);
    }
    return 0.2;
}

QVector<Reflection> echogram(const AcousticOpts &a,
                             const double src[3], const double rcv[3])
{
    const double c0 = 343.0;
    const double L = std::max(1.0, a.roomL);
    const double W = std::max(1.0, a.roomW);
    const double H = std::max(1.0, a.roomH);

    auto dist = [](const double p[3], const double q[3]) {
        const double dx = p[0]-q[0], dy = p[1]-q[1], dz = p[2]-q[2];
        return std::sqrt(dx*dx + dy*dy + dz*dz);
    };
    const double rd = std::max(0.1, dist(src, rcv));

    QVector<Reflection> out;
    Reflection dirSnd;                              // 直接音
    dirSnd.early = true;
    for (int k = 0; k < 3; ++k) dirSnd.dir[k] = (rcv[k] - src[k]) / rd;
    out.push_back(dirSnd);

    struct Face { int axis; double plane; const char *name; int role; };
    const Face faces[6] = {
        { 2, 0.0, "床",    AbsorptionRow::Floor    },
        { 2, H,   "天井",  AbsorptionRow::Ceiling  },
        { 1, 0.0, "側壁L", AbsorptionRow::SideWall },
        { 1, W,   "側壁R", AbsorptionRow::SideWall },
        { 0, 0.0, "舞台側", AbsorptionRow::SideWall },
        { 0, L,   "後壁",  AbsorptionRow::RearWall },
    };
    for (const Face &f : faces) {
        double img[3] = { src[0], src[1], src[2] };
        img[f.axis] = 2.0 * f.plane - img[f.axis];
        const double riRaw = std::max(1e-6, dist(img, rcv));
        const double ri = std::max(rd + 1e-6, riRaw);
        const double alpha = faceAlpha1k(a, f.role);
        Reflection r;
        r.timeMs = (ri - rd) / c0 * 1000.0;
        r.levelDb = 20.0 * std::log10(rd / ri)
                  + 10.0 * std::log10(std::max(1e-6, 1.0 - alpha));
        r.surface = QString::fromUtf8(f.name);
        r.early = r.timeMs <= 80.0;
        // 到来方向 = 鏡像音源 → 受音点 (反射経路の最終区間の向き)
        for (int k = 0; k < 3; ++k) r.dir[k] = (rcv[k] - img[k]) / riRaw;
        out.push_back(r);
    }
    std::sort(out.begin(), out.end(),
              [](const Reflection &x, const Reflection &y) {
                  return x.timeMs < y.timeMs;
              });
    return out;
}

// ── 初期側方エネルギー比 LF / LFC (ISO 3382-1:2009 A.2.6) ──────────────────
// 8 字マイクの軸 = 音源→受音点の水平方向に直交する水平軸。
// エネルギー E_i は直接音を 1 とした相対値 (levelDb から復元)。
// 分子は 5–80 ms の1次反射、分母は 0–80 ms の直接音 + 1次反射。
LateralEnergy lateralEnergy(const AcousticOpts &a,
                            const double src[3], const double rcv[3])
{
    LateralEnergy out;
    const double hx = rcv[0] - src[0], hy = rcv[1] - src[1];
    const double hl = std::sqrt(hx * hx + hy * hy);
    if (hl < 1e-6) return out;          // 音源直上 → 側方軸が定義できない
    const double lat[3] = { -hy / hl, hx / hl, 0.0 };   // 水平面内の直交軸

    const QVector<Reflection> refl = echogram(a, src, rcv);
    double num = 0, numC = 0, den = 0;
    for (const Reflection &r : refl) {
        if (r.timeMs > 80.0) continue;
        const double e = std::pow(10.0, r.levelDb / 10.0);
        den += e;
        if (r.surface.isEmpty() || r.timeMs < 5.0) continue;   // 直接音を除く
        const double c = r.dir[0] * lat[0] + r.dir[1] * lat[1]
                       + r.dir[2] * lat[2];
        num += e * c * c;
        numC += e * std::fabs(c);
        ++out.nEarly;
    }
    if (den <= 0) return out;
    out.LF = num / den;
    out.LFC = numC / den;
    out.valid = true;
    return out;
}

// ── 拡声系 ──────────────────────────────────────────────────────────────────
double soundSpeed(double tempC)
{
    // c = 331.3·√(1 + t/273.15)  (乾燥空気, ISO 9613-1:1993)
    return 331.3 * std::sqrt(std::max(0.0, 1.0 + tempC / 273.15));
}

double alignmentDelayMs(double dFar, double dNear, double tempC)
{
    const double c = soundSpeed(tempC);
    if (c <= 0) return 0;
    return std::max(0.0, (dFar - dNear) / c * 1000.0);
}

GainBeforeFeedback pagNag(double D0, double D1, double D2, double Ds,
                          int NOM, double EAD, double FSM)
{
    GainBeforeFeedback g;
    g.D0 = D0; g.D1 = D1; g.D2 = D2; g.Ds = Ds;
    g.EAD = EAD; g.NOM = std::max(1, NOM); g.FSM = FSM;
    if (D0 <= 0 || D1 <= 0 || D2 <= 0 || Ds <= 0 || EAD <= 0) return g;

    // Davis & Patronis, "Sound System Engineering" 3rd ed. の音響利得式
    g.NAG = 20.0 * std::log10(D0 / EAD);
    g.PAG = 20.0 * std::log10(D0) + 20.0 * std::log10(Ds)
          - 20.0 * std::log10(D1) - 20.0 * std::log10(D2)
          - 10.0 * std::log10(double(g.NOM)) - FSM;
    g.margin = g.PAG - g.NAG;
    g.valid = true;
    return g;
}

double itdgMs(const QVector<Reflection> &refl)
{
    for (const Reflection &r : refl)
        if (!r.surface.isEmpty())
            return r.timeMs;      // ソート済み → 最初の反射
    return 0;
}

// ── NC 曲線 (Beranek) ───────────────────────────────────────────────────────
// 帯域: 63,125,250,500,1k,2k,4k Hz (8k は省略 — 入力7帯域に合わせる)
static const int kNcSteps[] = { 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70 };
static const double kNcTable[][7] = {
    { 47, 36, 29, 22, 17, 14, 12 },   // NC-15
    { 51, 40, 33, 26, 22, 19, 17 },   // NC-20
    { 54, 44, 37, 31, 27, 24, 22 },   // NC-25
    { 57, 48, 41, 35, 31, 29, 28 },   // NC-30
    { 60, 52, 45, 40, 36, 34, 33 },   // NC-35
    { 64, 56, 50, 45, 41, 39, 38 },   // NC-40
    { 67, 60, 54, 49, 46, 44, 43 },   // NC-45
    { 71, 64, 58, 54, 51, 49, 48 },   // NC-50
    { 74, 67, 62, 58, 56, 54, 53 },   // NC-55
    { 77, 71, 67, 63, 61, 59, 58 },   // NC-60
    { 80, 75, 71, 68, 66, 64, 63 },   // NC-65
    { 83, 79, 75, 72, 71, 70, 69 },   // NC-70
};
static const int kNcCount = int(sizeof(kNcSteps) / sizeof(int));

int ncRating(const double levels[7])
{
    // タンジェント法: 各帯域で測定値を挟む隣接曲線間を線形補間し、
    // 全帯域の最大値が NC 値。
    double worst = 0;
    for (int b = 0; b < 7; ++b) {
        const double Lb = levels[b];
        double v;
        if (Lb <= kNcTable[0][b]) {
            v = kNcSteps[0] * Lb / std::max(1.0, kNcTable[0][b]);
        } else if (Lb >= kNcTable[kNcCount-1][b]) {
            v = kNcSteps[kNcCount-1];
        } else {
            v = kNcSteps[kNcCount-1];
            for (int i = 0; i + 1 < kNcCount; ++i) {
                if (Lb <= kNcTable[i+1][b]) {
                    const double f = (Lb - kNcTable[i][b])
                                   / (kNcTable[i+1][b] - kNcTable[i][b]);
                    v = kNcSteps[i] + f * (kNcSteps[i+1] - kNcSteps[i]);
                    break;
                }
            }
        }
        worst = std::max(worst, v);
    }
    return qBound(0, int(std::ceil(worst)), kNcSteps[kNcCount-1]);
}

QVector<double> ncCurve(int nc)
{
    for (int i = 0; i < kNcCount; ++i)
        if (kNcSteps[i] == nc)
            return QVector<double>(kNcTable[i], kNcTable[i] + 7);
    return {};
}

// ── 音響障害検出 ────────────────────────────────────────────────────────────
QVector<Defect> detectDefects(const AcousticOpts &a,
                              const double src[3], const double rcv[3])
{
    QVector<Defect> out;

    // フラッターエコー: 対向平行面がどちらも低吸音 (α@1k < 0.2)
    struct Pair { int role1, role2; const char *place; const char *cause; };
    const Pair pairs[3] = {
        { AbsorptionRow::SideWall, AbsorptionRow::SideWall,
          "側壁L-R間", "平行壁面の多重反射" },
        { AbsorptionRow::Floor, AbsorptionRow::Ceiling,
          "床-天井間", "平行面の多重反射" },
        { AbsorptionRow::SideWall, AbsorptionRow::RearWall,
          "舞台-後壁間", "前後面の多重反射" },
    };
    for (const Pair &pr : pairs) {
        const double a1 = faceAlpha1k(a, pr.role1);
        const double a2 = faceAlpha1k(a, pr.role2);
        if (a1 < 0.2 && a2 < 0.2) {
            Defect d;
            d.name = QString::fromUtf8("フラッターエコー");
            d.place = QString::fromUtf8(pr.place);
            d.cause = QString::fromUtf8(pr.cause);
            d.severity = (a1 < 0.12 && a2 < 0.12) ? 2 : 1;
            out.push_back(d);
        }
    }

    // ロングディレイエコー: Δt > 50ms かつ直接音比 −10dB 以内の1次反射
    const QVector<Reflection> refl = echogram(a, src, rcv);
    for (const Reflection &r : refl) {
        if (r.surface.isEmpty()) continue;
        if (r.timeMs > 50.0 && r.levelDb > -10.0) {
            Defect d;
            d.name = QString::fromUtf8("ロングディレイエコー");
            d.place = r.surface;
            d.cause = QString::fromUtf8("%1 からの強反射 (%2ms, %3dB)")
                          .arg(r.surface)
                          .arg(qRound(r.timeMs))
                          .arg(QString::number(r.levelDb, 'f', 1));
            d.severity = r.levelDb > -6.0 ? 2 : 1;
            out.push_back(d);
        }
    }
    return out;
}

} // namespace roomac
} // namespace ofd
