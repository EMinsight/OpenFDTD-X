// DispersionFit.cpp — 分散モデルの最小二乗フィットと診断 (出典はヘッダ参照)
//
// 手順:
//   ① 極位置 λ_p を固定すると ε = ε∞ + Σ Δε_p·λ²/(λ²−λ_p²) は係数について
//      **線形**なので、正規方程式 (小さな密行列) を Gauss 消去で解く。
//   ② 極位置は座標降下で改善する (乗算ステップを半分ずつ縮める)。
//      極は参照データの波長域の外に留める — 透明域の中に共鳴を置くと
//      発散点が生じ、物理的にも「吸収の無い帯域」と矛盾するため。
//   ③ 極数 P は 1 から maxPoles まで増やし、RMS が許容値を切ったら止める
//      (係数を増やしすぎない)。
#include "optics/DispersionFit.h"

#include <algorithm>
#include <cmath>

namespace ofd {
namespace optics {
namespace {

const int kMaxBasis = 16;   // ε∞ + 極 15 個 (GUI の上限より十分大きい)

// 基底関数の数
int basisCount(FitModel m, int poles)
{
    return (m == FitModel::Drude) ? 2 : poles + 1;
}

// 基底関数値。g は basisCount 個以上の領域を指すこと。
void basisAt(FitModel m, const std::vector<double> &lam0, double lam, double *g)
{
    const double l2 = lam * lam;
    g[0] = 1.0;
    if (m == FitModel::Drude) {
        g[1] = -l2;                       // 係数 = 1/λ_p²
        return;
    }
    for (std::size_t p = 0; p < lam0.size(); ++p) {
        const double d = l2 - lam0[p] * lam0[p];
        // 極に近付きすぎたら基底を潰す (発散させない)
        g[1 + p] = (std::fabs(d) > 1e-12) ? (l2 / d) : 0.0;
    }
}

// Gauss 消去 (部分ピボット)。A は m×m の row-major、解は b に入る。
bool solveLinear(double *A, double *b, int m)
{
    for (int c = 0; c < m; ++c) {
        int piv = c;
        double best = std::fabs(A[c * m + c]);
        for (int r = c + 1; r < m; ++r) {
            const double v = std::fabs(A[r * m + c]);
            if (v > best) { best = v; piv = r; }
        }
        if (!(best > 0.0)) return false;
        if (piv != c) {
            for (int j = 0; j < m; ++j) std::swap(A[c * m + j], A[piv * m + j]);
            std::swap(b[c], b[piv]);
        }
        const double d = A[c * m + c];
        for (int r = c + 1; r < m; ++r) {
            const double f = A[r * m + c] / d;
            if (f == 0.0) continue;
            for (int j = c; j < m; ++j) A[r * m + j] -= f * A[c * m + j];
            b[r] -= f * b[c];
        }
    }
    for (int r = m - 1; r >= 0; --r) {
        double s = b[r];
        for (int j = r + 1; j < m; ++j) s -= A[r * m + j] * b[j];
        b[r] = s / A[r * m + r];
    }
    for (int r = 0; r < m; ++r)
        if (!std::isfinite(b[r])) return false;
    return true;
}

// 極位置を固定して係数を解き、n の RMS 残差を返す。
// fixEpsInf = true なら ε∞ を 1 に固定して残りだけ解く (ε(∞) = 1 は受動媒質の
// 要請 [2]。無拘束解が ε∞ < 1 に落ちたときの制約付き解を得るために使う)。
// coef は常に coef[0] = ε∞、以降が Δε_p という並びで返す。
// 解けなければ false (rms は書き換えない)。
bool fitFixedPoles(const std::vector<NkSample> &s, FitModel model,
                   const std::vector<double> &lam0, std::vector<double> &coef,
                   double &rms, double &maxErr, bool fixEpsInf = false)
{
    const int nb = basisCount(model, static_cast<int>(lam0.size()));
    const int m = fixEpsInf ? nb - 1 : nb;           // 解く未知数の数
    if (nb > kMaxBasis || m < 1) return false;
    if (static_cast<int>(s.size()) < m) return false;

    double A[kMaxBasis * kMaxBasis] = { 0.0 };
    double b[kMaxBasis] = { 0.0 };
    double g[kMaxBasis] = { 0.0 };
    const int off = fixEpsInf ? 1 : 0;               // 定数列を外すか
    for (const NkSample &p : s) {
        basisAt(model, lam0, p.lambda_um, g);
        double y = p.n * p.n;                        // 目標 ε (k = 0 の透明域)
        if (fixEpsInf) y -= 1.0;                     // ε∞ = 1 を移項
        for (int i = 0; i < m; ++i) {
            for (int j = 0; j < m; ++j)
                A[i * m + j] += g[i + off] * g[j + off];
            b[i] += g[i + off] * y;
        }
    }
    // 悪条件対策の微小 Tikhonov 項 (対角の代表値に対して 1e-12)
    double tr = 0.0;
    for (int i = 0; i < m; ++i) tr += A[i * m + i];
    const double ridge = 1e-12 * (tr / m);
    for (int i = 0; i < m; ++i) A[i * m + i] += ridge;

    if (!solveLinear(A, b, m)) return false;

    coef.assign(nb, 0.0);
    if (fixEpsInf) {
        coef[0] = 1.0;
        for (int i = 0; i < m; ++i) coef[1 + i] = b[i];
    } else {
        for (int i = 0; i < m; ++i) coef[i] = b[i];
    }

    double sum = 0.0, worst = 0.0;
    for (const NkSample &p : s) {
        basisAt(model, lam0, p.lambda_um, g);
        double eps = 0.0;
        for (int i = 0; i < nb; ++i) eps += coef[i] * g[i];
        const double nfit = (eps > 0.0) ? std::sqrt(eps) : 0.0;
        const double e = nfit - p.n;
        sum += e * e;
        worst = std::max(worst, std::fabs(e));
    }
    rms = std::sqrt(sum / s.size());
    maxErr = worst;
    return true;
}

// 無拘束で解き、ε∞ < 1 (非受動 [2]) に落ちたら ε∞ = 1 固定で解き直す。
bool fitPassive(const std::vector<NkSample> &s, FitModel model,
                const std::vector<double> &lam0, std::vector<double> &coef,
                double &rms, double &maxErr)
{
    if (!fitFixedPoles(s, model, lam0, coef, rms, maxErr, false)) return false;
    if (coef[0] >= 1.0) return true;
    std::vector<double> c2;
    double r2 = 0.0, e2 = 0.0;
    if (!fitFixedPoles(s, model, lam0, c2, r2, e2, true)) return true;
    coef = c2; rms = r2; maxErr = e2;
    return true;
}

// 極の初期配置。UV 側 (データ域より短波長) に P−1 個、IR 側に 1 個。
// P = 1 のときは UV 側 1 個 (可視〜近赤外の誘電体はこれで概ね合う)。
std::vector<double> initialPoles(int P, double lamMin, double lamMax)
{
    std::vector<double> v;
    if (P <= 0) return v;
    if (P == 1) { v.push_back(0.5 * lamMin); return v; }
    const int nuv = P - 1;
    const double a = 0.10 * lamMin, b = 0.80 * lamMin;
    for (int i = 0; i < nuv; ++i) {
        const double t = (nuv == 1) ? 0.5 : double(i) / (nuv - 1);
        v.push_back(a * std::pow(b / a, t));         // 対数等間隔
    }
    v.push_back(8.0 * lamMax);                       // IR 極
    return v;
}

// 極位置の座標降下。データ域の外という制約を守る。
void refinePoles(const std::vector<NkSample> &s, FitModel model,
                 std::vector<double> &lam0, std::vector<double> &coef,
                 double &rms, double &maxErr, int iterations,
                 double lamMin, double lamMax)
{
    double step = 0.25;
    for (int it = 0; it < std::max(0, iterations); ++it) {
        bool improved = false;
        for (std::size_t p = 0; p < lam0.size(); ++p) {
            const bool uv = (lam0[p] < lamMin);
            for (int sgn = -1; sgn <= 1; sgn += 2) {
                std::vector<double> trial = lam0;
                double cand = trial[p] * (1.0 + sgn * step);
                if (uv) cand = std::min(cand, 0.98 * lamMin);
                else    cand = std::max(cand, 1.02 * lamMax);
                if (!(cand > 0.0)) continue;
                trial[p] = cand;
                std::vector<double> c2;
                double r2 = 0.0, e2 = 0.0;
                if (!fitPassive(s, model, trial, c2, r2, e2)) continue;
                if (r2 < rms) {
                    lam0 = trial; coef = c2; rms = r2; maxErr = e2;
                    improved = true;
                }
            }
        }
        if (!improved) step *= 0.5;
        if (step < 1e-4) break;
    }
}

} // namespace

double modelIndex(const FitReport &r, double lambda_um)
{
    if (r.status != FitStatus::Ok || r.interpolation) return 0.0;
    double eps = r.epsInf;
    if (r.model == FitModel::Drude) {
        if (r.lambda0_um.empty() || !(r.lambda0_um[0] > 0.0)) return 0.0;
        const double t = lambda_um / r.lambda0_um[0];
        eps -= t * t;
    } else {
        const double l2 = lambda_um * lambda_um;
        for (std::size_t p = 0; p < r.lambda0_um.size() &&
                                p < r.deltaEps.size(); ++p) {
            const double d = l2 - r.lambda0_um[p] * r.lambda0_um[p];
            if (std::fabs(d) > 1e-12) eps += r.deltaEps[p] * l2 / d;
        }
    }
    return (eps > 0.0) ? std::sqrt(eps) : 0.0;
}

FitReport fitDispersion(const std::vector<NkSample> &samples,
                        const FitOptions &opt)
{
    FitReport rep;
    rep.model = opt.model;

    // 有効な点だけを λ 昇順で集める
    std::vector<NkSample> s;
    s.reserve(samples.size());
    for (const NkSample &p : samples) {
        if (!std::isfinite(p.lambda_um) || !(p.lambda_um > 0.0)) continue;
        if (!std::isfinite(p.n) || !(p.n > 0.0)) continue;
        s.push_back(p);
    }
    std::sort(s.begin(), s.end(),
              [](const NkSample &a, const NkSample &b) {
                  return a.lambda_um < b.lambda_um;
              });
    rep.points = static_cast<int>(s.size());
    if (s.empty()) { rep.status = FitStatus::NoData; return rep; }

    rep.hasK = true;
    for (const NkSample &p : s)
        if (!(p.k >= 0.0)) { rep.hasK = false; break; }

    // ── 因果律の必要条件 [1] — 参照データそのものに対する判定 ──
    // 透明域では ε は ω の非減少関数、すなわち λ について非増加。
    // 吸収域 (k > 0 の点がある) では必要条件が使えないので評価対象外。
    bool absorbing = false;
    if (rep.hasK)
        for (const NkSample &p : s)
            if (p.k > 0.0) { absorbing = true; break; }
    if (!absorbing && s.size() >= 2) {
        rep.causalityEvaluable = true;
        for (std::size_t i = 0; i + 1 < s.size(); ++i) {
            const double e0 = s[i].n * s[i].n;
            const double e1 = s[i + 1].n * s[i + 1].n;
            ++rep.causalityChecks;
            if (e1 > e0 + 1e-9 * std::max(1.0, e0)) ++rep.causalityViolations;
        }
    }

    const double lamMin = s.front().lambda_um;
    const double lamMax = s.back().lambda_um;

    // ── Sampled: 補間なのでフィットしない (残差は定義上 0) ──
    if (opt.model == FitModel::Sampled) {
        rep.status = FitStatus::Ok;
        rep.interpolation = true;
        rep.rmsN = 0.0;
        rep.maxErrN = 0.0;
        rep.nMin = rep.nMax = s.front().n;
        for (const NkSample &p : s) {
            rep.nMin = std::min(rep.nMin, p.n);
            rep.nMax = std::max(rep.nMax, p.n);
        }
        // 実データが k を持つなら Im ε ≥ 0 をそのまま判定できる
        rep.passivityOk = true;
        if (rep.hasK)
            for (const NkSample &p : s)
                if (p.k < 0.0) { rep.passivityOk = false; break; }
        return rep;
    }

    // ── Drude: 基底 [1, −λ²] の 2 係数 (極位置探索は不要) ──
    if (opt.model == FitModel::Drude) {
        std::vector<double> lam0, coef;
        double rms = 0.0, maxErr = 0.0;
        if (!fitPassive(s, FitModel::Drude, lam0, coef, rms, maxErr)) {
            rep.status = (static_cast<int>(s.size()) < 2)
                             ? FitStatus::TooFewPoints : FitStatus::Singular;
            return rep;
        }
        rep.status = FitStatus::Ok;
        rep.poles = 1;
        rep.epsInf = coef[0];
        const double a = coef[1];                 // a = 1/λ_p²
        rep.lambda0_um.push_back(a > 0.0 ? 1.0 / std::sqrt(a) : 0.0);
        rep.deltaEps.push_back(0.0);              // Drude では未使用
        rep.rmsN = rms;
        rep.maxErrN = maxErr;
        // 受動性 [2]: ε∞ ≥ 1 かつ ω_p² ≥ 0
        rep.passivityOk = (rep.epsInf >= 1.0 - 1e-9) && (a >= 0.0);
        rep.nMin = rep.nMax = modelIndex(rep, lamMin);
        for (const NkSample &p : s) {
            const double nf = modelIndex(rep, p.lambda_um);
            rep.nMin = std::min(rep.nMin, nf);
            rep.nMax = std::max(rep.nMax, nf);
        }
        if (rep.hasK) {
            double sum = 0.0;                     // 無損失モデルなので k_model = 0
            for (const NkSample &p : s) sum += p.k * p.k;
            rep.rmsK = std::sqrt(sum / s.size());
        }
        return rep;
    }

    // ── 多極 / 単極 Lorentz ──
    const int maxP = (opt.model == FitModel::Lorentz)
                         ? 1 : std::max(1, opt.maxPoles);
    std::vector<double> bestPoles, bestCoef;
    double bestRms = 0.0, bestMax = 0.0;
    bool have = false;
    for (int P = 1; P <= maxP; ++P) {
        std::vector<double> lam0 = initialPoles(P, lamMin, lamMax);
        std::vector<double> coef;
        double rms = 0.0, maxErr = 0.0;
        if (!fitPassive(s, opt.model, lam0, coef, rms, maxErr)) continue;
        refinePoles(s, opt.model, lam0, coef, rms, maxErr, opt.iterations,
                    lamMin, lamMax);
        if (!have || rms < bestRms) {
            have = true;
            bestPoles = lam0; bestCoef = coef; bestRms = rms; bestMax = maxErr;
        }
        if (rms <= opt.rmsTol) break;             // 許容値に達したら増やさない
    }
    if (!have) {
        rep.status = (static_cast<int>(s.size()) < 2)
                         ? FitStatus::TooFewPoints : FitStatus::Singular;
        return rep;
    }

    rep.status = FitStatus::Ok;
    rep.poles = static_cast<int>(bestPoles.size());
    rep.epsInf = bestCoef[0];
    rep.lambda0_um = bestPoles;
    rep.deltaEps.assign(bestCoef.begin() + 1, bestCoef.end());
    rep.rmsN = bestRms;
    rep.maxErrN = bestMax;

    // 受動性 [2]: ε∞ ≥ 1 かつ全ての振動子強度 Δε_p ≥ 0
    rep.passivityOk = (rep.epsInf >= 1.0 - 1e-9);
    for (double d : rep.deltaEps)
        if (d < -1e-12) { rep.passivityOk = false; break; }

    // FDTD 安定性 [3] 用に、モデルの n の範囲を参照域で調べる
    rep.nMin = rep.nMax = modelIndex(rep, lamMin);
    for (const NkSample &p : s) {
        const double nf = modelIndex(rep, p.lambda_um);
        rep.nMin = std::min(rep.nMin, nf);
        rep.nMax = std::max(rep.nMax, nf);
    }
    if (rep.hasK) {
        double sum = 0.0;                         // 無損失モデルは k = 0
        for (const NkSample &p : s) sum += p.k * p.k;
        rep.rmsK = std::sqrt(sum / s.size());
    }
    return rep;
}

} // namespace optics
} // namespace ofd
