// ThinFilmStack.cpp — 特性行列法 (Abeles 行列) の実装。
//
// 記号は Macleod "Thin-Film Optical Filters" 4th ed. Ch.2 に合わせる。
// 層 j の特性行列 (自由空間アドミッタンス単位):
//
//     M_j = [ cos δ_j          (i/η_j) sin δ_j ]
//           [ i η_j sin δ_j     cos δ_j        ]
//
//     δ_j = 2π d_j q_j / λ,   q_j = Ñ_j cos θ_j = sqrt(Ñ_j² − (n0 sin θ0)²)
//     η_j = q_j            (s 偏波)
//     η_j = Ñ_j² / q_j     (p 偏波)
//
// 全体は [B; C] = (Π_j M_j)·[1; η_sub] で、
//     r = (η0 B − C)/(η0 B + C),  R = |r|²
//     T = 4 η0 Re(η_sub) / |η0 B + C|²
//     A = 1 − R − T
// 無損失なら R + T = 1 が厳密に成り立つ。
//
// q の分岐は Im(q) ≤ 0 (Ñ = n − i k 規約で減衰する向き) を選ぶ。層の行列は
// q → −q に対して不変なので分岐が効くのは基板 (η_sub) だけであり、全反射時に
// Re(η_sub) = 0 → T = 0、|r| = 1 となって正しく振る舞う。
#include "optics/ThinFilmStack.h"

#include "core/Optimizer.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>
#include <random>

namespace ofd {
namespace optics {
namespace {

using cd = std::complex<double>;

const double kPi = 3.14159265358979323846;
// 光速 [nm/ps] (群遅延の単位換算に使う)
const double kC_nm_per_ps = 299792.458;

// q = Ñ cosθ。分岐は Im(q) ≤ 0、Im(q) = 0 のときは Re(q) ≥ 0。
cd qOf(const cd &N, double s0)
{
    cd q = std::sqrt(N * N - cd(s0 * s0, 0.0));
    if (q.imag() > 0.0)                          q = -q;
    else if (q.imag() == 0.0 && q.real() < 0.0)  q = -q;
    return q;
}

cd etaOf(const cd &N, const cd &q, Pol pol)
{
    return (pol == Pol::S) ? q : (N * N) / q;
}

bool finiteAngle(double aoi_deg)
{
    return aoi_deg >= 0.0 && aoi_deg < 90.0;
}

// 位相差を (−π, π] へ折り畳む (群遅延の中心差分で使う)
double wrapPi(double x)
{
    while (x >  kPi) x -= 2.0 * kPi;
    while (x <= -kPi) x += 2.0 * kPi;
    return x;
}

double quantityOf(const FilmResponse &s, const FilmResponse &p,
                  Quantity q, PolMode pol)
{
    const double vs = (q == Quantity::R) ? s.R : s.T;
    const double vp = (q == Quantity::R) ? p.R : p.T;
    switch (pol) {
    case PolMode::S: return vs;
    case PolMode::P: return vp;
    default:         return 0.5 * (vs + vp);
    }
}

// ── ターゲット帯域を λ グリッドへ展開したもの ──────────────────────────────
// メリット関数・感度・モンテカルロで共用する。層構成は λ ごとに 1 度だけ
// 取得し、以降 (膜厚摂動) は再取得しない。
struct Grid {
    std::vector<double>      lambda;
    std::vector<StackSample> sample;
    std::vector<int>         band;      // targets の index
    int  skipped = 0;
    int  nLayers = -1;                  // 全点で共通の層数 (不一致なら −1)
    bool ok = false;
};

Grid buildGrid(const StackAtLambda &stack,
               const std::vector<TargetBand> &targets)
{
    Grid g;
    if (!stack || targets.empty()) return g;
    for (size_t b = 0; b < targets.size(); ++b) {
        const TargetBand &t = targets[b];
        if (!(t.tol > 0.0) || !(t.weight > 0.0)) return g;
        if (!(t.lam0_nm > 0.0) || !(t.lam1_nm > 0.0)) return g;
        const int ns = std::max(1, t.samples);
        const double lo = std::min(t.lam0_nm, t.lam1_nm);
        const double hi = std::max(t.lam0_nm, t.lam1_nm);
        for (int i = 0; i < ns; ++i) {
            const double lam = (ns == 1) ? 0.5 * (lo + hi)
                                         : lo + (hi - lo) * i / (ns - 1);
            StackSample s;
            if (!stack(lam, s)) { ++g.skipped; continue; }
            if (g.nLayers < 0) g.nLayers = int(s.layers.size());
            else if (g.nLayers != int(s.layers.size())) { g.nLayers = -1; return g; }
            g.lambda.push_back(lam);
            g.sample.push_back(s);
            g.band.push_back(int(b));
        }
    }
    g.ok = !g.lambda.empty();
    return g;
}

// グリッド 1 点の対象量。膜厚は 2 通りに差し替えられる:
//   scale ≠ 空 … 公称膜厚に相対摂動を掛ける (製造誤差モンテカルロ)
//   absD  ≠ 0  … 絶対膜厚で置き換える     (膜厚最適化)
// 両方省略なら公称膜厚。
double gridQuantity(const Grid &g, size_t i, const std::vector<TargetBand> &t,
                    double aoi_deg, const std::vector<double> &scale,
                    const std::vector<double> *absD = nullptr)
{
    StackSample s = g.sample[i];
    if (absD && absD->size() == s.layers.size())
        for (size_t j = 0; j < s.layers.size(); ++j)
            s.layers[j].d_nm = (*absD)[j];
    if (!scale.empty() && scale.size() == s.layers.size())
        for (size_t j = 0; j < s.layers.size(); ++j)
            s.layers[j].d_nm *= scale[j];
    const TargetBand &tb = t[g.band[i]];
    const FilmResponse rs = filmResponse(s.n0, s.layers, s.nsub, s.ksub,
                                         g.lambda[i], aoi_deg, Pol::S);
    const FilmResponse rp = filmResponse(s.n0, s.layers, s.nsub, s.ksub,
                                         g.lambda[i], aoi_deg, Pol::P);
    if (!rs.valid || !rp.valid) return std::numeric_limits<double>::quiet_NaN();
    return quantityOf(rs, rp, tb.q, tb.pol);
}

// 出典 [3] のメリット関数
double meritOf(const Grid &g, const std::vector<TargetBand> &t, double aoi_deg,
               const std::vector<double> &scale, bool *allInTol,
               const std::vector<double> *absD = nullptr)
{
    double num = 0.0, den = 0.0;
    bool inTol = true;
    for (size_t i = 0; i < g.lambda.size(); ++i) {
        const TargetBand &tb = t[g.band[i]];
        const double q = gridQuantity(g, i, t, aoi_deg, scale, absD);
        if (!(q == q)) { if (allInTol) *allInTol = false;
                         return std::numeric_limits<double>::quiet_NaN(); }
        const double e = (q - tb.goal) / tb.tol;
        num += tb.weight * e * e;
        den += tb.weight;
        if (std::fabs(q - tb.goal) > tb.tol) inTol = false;
    }
    if (allInTol) *allInTol = inTol;
    return (den > 0.0) ? std::sqrt(num / den) : std::numeric_limits<double>::quiet_NaN();
}

// std::normal_distribution は処理系で分布アルゴリズムが異なり再現しないため、
// Box-Muller を自前で書く (mt19937 は規格で定義済みなので結果が一意)。
struct Gauss {
    std::mt19937 rng;
    bool   has = false;
    double spare = 0.0;
    explicit Gauss(unsigned seed) : rng(seed) {}
    double u01()
    {
        // (0,1) 開区間。log(0) を避ける
        return (rng() + 0.5) / 4294967296.0;
    }
    double operator()()
    {
        if (has) { has = false; return spare; }
        const double u1 = u01(), u2 = u01();
        const double r = std::sqrt(-2.0 * std::log(u1));
        const double th = 2.0 * kPi * u2;
        spare = r * std::sin(th);
        has = true;
        return r * std::cos(th);
    }
};

} // namespace

// ── 単一波長・単一偏波の応答 ────────────────────────────────────────────────
FilmResponse filmResponse(double n0, const std::vector<FilmLayer> &layers,
                          double nsub, double ksub, double lambda_nm,
                          double aoi_deg, Pol pol)
{
    FilmResponse out;
    if (!(lambda_nm > 0.0) || !(n0 > 0.0) || !(nsub > 0.0) || ksub < 0.0)
        return out;
    if (!finiteAngle(aoi_deg)) return out;

    const double th = aoi_deg * kPi / 180.0;
    const double c0 = std::cos(th);
    const double s0 = n0 * std::sin(th);
    if (!(c0 > 0.0)) return out;

    const double eta0 = (pol == Pol::S) ? (n0 * c0) : (n0 / c0);

    const cd Ns(nsub, -ksub);
    const cd qs = qOf(Ns, s0);
    if (std::abs(qs) < 1e-300) return out;          // 基板内で臨界角ちょうど
    const cd etas = etaOf(Ns, qs, pol);

    // 全反射 (無損失基板 + 臨界角超): 基板側の波はエバネッセントで正味の
    // 電力輸送が無く、T は厳密に 0。T = 4·η0·Re(ηs)/|den|² は Re(ηs) が
    // 厳密に 0 になることを前提にしているが、複素平方根・複素除算の実装差で
    // 微小な非零が残る処理系がある (Apple clang / arm64 で実測: macOS だけ
    // T が 0 にならず CI が落ちた)。丸め残差に頼らず入力から判定する。
    const bool tir = (ksub == 0.0) && (s0 > nsub);

    cd B(1.0, 0.0), C = etas;
    for (int j = int(layers.size()) - 1; j >= 0; --j) {
        const FilmLayer &L = layers[j];
        if (!(L.n > 0.0) || L.k < 0.0 || L.d_nm < 0.0) return out;
        const cd Nj(L.n, -L.k);
        const cd qj = qOf(Nj, s0);
        if (std::abs(qj) < 1e-300) return out;
        const cd etaj = etaOf(Nj, qj, pol);
        const cd delta = (2.0 * kPi * L.d_nm / lambda_nm) * qj;
        cd cosd, sind;
        if (delta.imag() == 0.0) {                  // 無損失層の高速経路
            cosd = cd(std::cos(delta.real()), 0.0);
            sind = cd(std::sin(delta.real()), 0.0);
        } else {
            cosd = std::cos(delta);
            sind = std::sin(delta);
        }
        const cd nb = cosd * B + (cd(0.0, 1.0) * sind / etaj) * C;
        const cd nc = (cd(0.0, 1.0) * etaj * sind) * B + cosd * C;
        B = nb;
        C = nc;
    }

    const cd den = eta0 * B + C;
    const double d2 = std::norm(den);
    if (!(d2 > 0.0)) return out;
    const cd r = (eta0 * B - C) / den;

    out.R = std::norm(r);
    out.T = tir ? 0.0 : (4.0 * eta0 * etas.real() / d2);
    if (out.T < 0.0) out.T = 0.0;                   // 残る丸め誤差の保険
    out.A = 1.0 - out.R - out.T;
    if (out.A < 0.0) out.A = 0.0;                   // 無損失系の丸め誤差
    out.phase_rad = std::arg(r);
    out.valid = true;
    return out;
}

// ── スペクトル ──────────────────────────────────────────────────────────────
std::vector<SpectrumPoint> spectrum(const StackAtLambda &stack,
                                    double lamMin_nm, double lamMax_nm,
                                    int points, double aoi_deg,
                                    bool withGroupDelay)
{
    std::vector<SpectrumPoint> out;
    if (!stack || !(lamMin_nm > 0.0) || !(lamMax_nm > 0.0)) return out;
    if (!finiteAngle(aoi_deg)) return out;
    const double lo = std::min(lamMin_nm, lamMax_nm);
    const double hi = std::max(lamMin_nm, lamMax_nm);
    const int n = std::max(1, points);

    auto evalAt = [&](double lam, FilmResponse &rs, FilmResponse &rp) {
        StackSample s;
        if (!stack(lam, s)) return false;
        rs = filmResponse(s.n0, s.layers, s.nsub, s.ksub, lam, aoi_deg, Pol::S);
        rp = filmResponse(s.n0, s.layers, s.nsub, s.ksub, lam, aoi_deg, Pol::P);
        return rs.valid && rp.valid;
    };

    out.reserve(size_t(n));
    for (int i = 0; i < n; ++i) {
        const double lam = (n == 1) ? 0.5 * (lo + hi)
                                    : lo + (hi - lo) * i / (n - 1);
        FilmResponse rs, rp;
        if (!evalAt(lam, rs, rp)) continue;
        SpectrumPoint p;
        p.lambda_nm = lam;
        p.Rs = rs.R; p.Ts = rs.T; p.As = rs.A;
        p.Rp = rp.R; p.Tp = rp.T; p.Ap = rp.A;
        if (withGroupDelay) {
            // τ_g = (λ²/2πc)·dφ/dλ を中心差分で。位相差は (−π,π] へ折り畳む
            // ので、h が大きすぎると折り返しで誤る。h = λ×1e-4 (最低 0.01nm)。
            const double h = std::max(0.01, lam * 1e-4);
            FilmResponse as, ap, bs, bp;
            if (evalAt(lam - h, as, ap) && evalAt(lam + h, bs, bp)) {
                const double k = lam * lam / (2.0 * kPi * kC_nm_per_ps);
                p.gds_ps = k * wrapPi(bs.phase_rad - as.phase_rad) / (2.0 * h);
                p.gdp_ps = k * wrapPi(bp.phase_rad - ap.phase_rad) / (2.0 * h);
                p.gdValid = true;
            }
        }
        out.push_back(p);
    }
    return out;
}

// ── 入射角掃引 ──────────────────────────────────────────────────────────────
std::vector<AnglePoint> angleSweep(const StackAtLambda &stack, double lambda_nm,
                                   double aoiMin_deg, double aoiMax_deg,
                                   int points)
{
    std::vector<AnglePoint> out;
    if (!stack || !(lambda_nm > 0.0)) return out;
    StackSample s;
    if (!stack(lambda_nm, s)) return out;
    const int n = std::max(1, points);
    const double lo = std::min(aoiMin_deg, aoiMax_deg);
    const double hi = std::max(aoiMin_deg, aoiMax_deg);
    out.reserve(size_t(n));
    for (int i = 0; i < n; ++i) {
        const double a = (n == 1) ? 0.5 * (lo + hi) : lo + (hi - lo) * i / (n - 1);
        if (!finiteAngle(a)) continue;
        const FilmResponse rs = filmResponse(s.n0, s.layers, s.nsub, s.ksub,
                                             lambda_nm, a, Pol::S);
        const FilmResponse rp = filmResponse(s.n0, s.layers, s.nsub, s.ksub,
                                             lambda_nm, a, Pol::P);
        if (!rs.valid || !rp.valid) continue;
        AnglePoint p;
        p.aoi_deg = a;
        p.Rs = rs.R; p.Ts = rs.T;
        p.Rp = rp.R; p.Tp = rp.T;
        out.push_back(p);
    }
    return out;
}

// ── メリット関数 ────────────────────────────────────────────────────────────
MeritResult merit(const StackAtLambda &stack,
                  const std::vector<TargetBand> &targets, double aoi_deg)
{
    MeritResult out;
    if (!finiteAngle(aoi_deg)) return out;
    const Grid g = buildGrid(stack, targets);
    out.skipped = g.skipped;
    if (!g.ok) return out;
    const double m = meritOf(g, targets, aoi_deg, std::vector<double>(), nullptr);
    if (!(m == m)) return out;
    out.merit = m;
    out.used  = int(g.lambda.size());
    out.valid = true;
    return out;
}

// ── 膜厚感度 ────────────────────────────────────────────────────────────────
SensitivityResult thicknessSensitivity(const StackAtLambda &stack,
                                       const std::vector<TargetBand> &targets,
                                       double aoi_deg, double delta_nm)
{
    SensitivityResult out;
    if (!finiteAngle(aoi_deg) || !(delta_nm > 0.0)) return out;
    const Grid g = buildGrid(stack, targets);
    if (!g.ok || g.nLayers <= 0) return out;

    const size_t np = g.lambda.size();
    std::vector<double> base(np);
    for (size_t i = 0; i < np; ++i) {
        base[i] = gridQuantity(g, i, targets, aoi_deg, std::vector<double>());
        if (!(base[i] == base[i])) return out;
    }

    out.dQ_pctPerNm.assign(size_t(g.nLayers), 0.0);
    for (int j = 0; j < g.nLayers; ++j) {
        double acc = 0.0;
        int cnt = 0;
        for (size_t i = 0; i < np; ++i) {
            StackSample s = g.sample[i];
            const double d0 = s.layers[size_t(j)].d_nm;
            const TargetBand &tb = targets[g.band[i]];
            auto evalAtD = [&](double d, double &q) {
                s.layers[size_t(j)].d_nm = d;
                const FilmResponse rs = filmResponse(s.n0, s.layers, s.nsub,
                                                     s.ksub, g.lambda[i],
                                                     aoi_deg, Pol::S);
                const FilmResponse rp = filmResponse(s.n0, s.layers, s.nsub,
                                                     s.ksub, g.lambda[i],
                                                     aoi_deg, Pol::P);
                if (!rs.valid || !rp.valid) return false;
                q = quantityOf(rs, rp, tb.q, tb.pol);
                return true;
            };
            double qm = 0.0, qp = 0.0;
            const double dm = std::max(0.0, d0 - delta_nm);
            const double dp = d0 + delta_nm;
            if (!evalAtD(dm, qm) || !evalAtD(dp, qp)) { out.dQ_pctPerNm.clear();
                                                        return out; }
            const double span = dp - dm;
            if (span > 0.0) { acc += std::fabs(qp - qm) / span * 100.0; ++cnt; }
        }
        out.dQ_pctPerNm[size_t(j)] = (cnt > 0) ? acc / cnt : 0.0;
    }

    int worst = 0;
    for (size_t j = 1; j < out.dQ_pctPerNm.size(); ++j)
        if (out.dQ_pctPerNm[j] > out.dQ_pctPerNm[size_t(worst)]) worst = int(j);
    out.worst = out.dQ_pctPerNm.empty() ? -1 : worst;
    out.valid = !out.dQ_pctPerNm.empty();
    return out;
}

// ── 膜厚最適化 (Nelder–Mead) ────────────────────────────────────────────────
OptimizeResult optimizeThickness(const StackAtLambda &stack,
                                 const std::vector<TargetBand> &targets,
                                 double aoi_deg,
                                 const std::vector<double> &d0_nm,
                                 const OptimizeOptions &opt)
{
    OptimizeResult out;
    if (!finiteAngle(aoi_deg) || d0_nm.empty()) return out;
    if (!(opt.minThick_nm >= 0.0) || !(opt.maxThick_nm > opt.minThick_nm))
        return out;
    const Grid g = buildGrid(stack, targets);
    if (!g.ok || g.nLayers <= 0 || size_t(g.nLayers) != d0_nm.size()) return out;

    const size_t n = d0_nm.size();
    auto clampD = [&](std::vector<double> &d) {
        for (double &v : d) {
            if (!(v == v)) v = opt.minThick_nm;                  // NaN 対策
            v = std::min(std::max(v, opt.minThick_nm), opt.maxThick_nm);
        }
    };
    auto f = [&](std::vector<double> d) {
        clampD(d);
        const double m = meritOf(g, targets, aoi_deg, std::vector<double>(),
                                 nullptr, &d);
        // 評価不能な点はシンプレックスから自然に排除されるよう大きな値にする
        return (m == m) ? m : std::numeric_limits<double>::max();
    };

    std::vector<double> start = d0_nm;
    clampD(start);
    const double f0 = f(start);
    if (!(f0 < std::numeric_limits<double>::max())) return out;

    // ── GA (大域探索) ────────────────────────────────────────────────────
    // 探索そのものは core/Optimizer の実数値 GA に任せる。ここは変数の箱を
    // 作って ask / tell を回すだけ (探索アルゴリズムを二重に持たない)。
    if (opt.method == OptimizeMethod::Genetic) {
        if (opt.population < 2 || opt.generations < 1) return out;
        const double range = (opt.gaRange > 0.0) ? opt.gaRange : 0.5;
        std::vector<ofd::optim::Variable> vars(n);
        for (size_t j = 0; j < n; ++j) {
            vars[j].lo = std::max(opt.minThick_nm, start[j] * (1.0 - range));
            vars[j].hi = std::min(opt.maxThick_nm, start[j] * (1.0 + range));
            if (!(vars[j].hi > vars[j].lo)) {   // 箱が潰れる層は動かさない
                vars[j].lo = start[j];
                vars[j].hi = std::nextafter(start[j],
                                            std::numeric_limits<double>::max());
            }
            // 1 個体目を初期膜厚にする。エリート保存と合わせて、
            // **結果が初期値より悪くなることが原理的に起こらない**
            vars[j].init = start[j];
            vars[j].hasInit = true;
        }
        ofd::optim::Options oo;
        oo.method = ofd::optim::Method::Genetic;
        oo.population = opt.population;
        oo.generations = opt.generations;
        oo.maximize = false;
        oo.seed = opt.seed;
        ofd::optim::Optimizer ga(vars, oo);
        if (!ga.valid()) return out;
        while (!ga.done()) {
            const std::vector<std::vector<double>> &pts = ga.ask();
            std::vector<double> vals(pts.size(), 0.0);
            for (size_t i = 0; i < pts.size(); ++i) {
                const double m = f(pts[i]);
                // 評価不能は NaN で返す (Optimizer が「最悪」として扱う)。
                // 大きな有限値で埋めると、それが良い点として残りうる
                vals[i] = (m < std::numeric_limits<double>::max())
                              ? m : std::numeric_limits<double>::quiet_NaN();
            }
            ga.tell(vals);
        }
        if (!ga.hasBest()) return out;
        out.valid = true;
        out.d_nm = ga.best();
        clampD(out.d_nm);
        out.meritStart = f0;
        out.meritEnd = f(out.d_nm);
        out.iterations = ga.generation();
        out.converged = false;      // GA は収束判定を持たない
        return out;
    }

    // 初期シンプレックス: 各軸を initStep の相対量 (下限に張り付く層は
    // 絶対量) だけずらした n+1 頂点。乱数を使わないので再現する。
    std::vector<std::vector<double>> simplex(n + 1, start);
    std::vector<double> fv(n + 1, 0.0);
    const double step = (opt.initStep > 0.0) ? opt.initStep : 0.10;
    for (size_t j = 0; j < n; ++j) {
        double h = std::fabs(start[j]) * step;
        if (!(h > 0.0)) h = std::max(1.0, step * opt.minThick_nm);
        // 上限に張り付いている軸は内側へずらす
        if (start[j] + h > opt.maxThick_nm) h = -h;
        simplex[j + 1][j] = start[j] + h;
    }
    for (size_t i = 0; i <= n; ++i) fv[i] = f(simplex[i]);

    const double kRefl = 1.0, kExp = 2.0, kCon = 0.5, kShrink = 0.5;
    int iter = 0;
    for (; iter < opt.maxIter; ++iter) {
        // 並べ替え (best = 0)
        for (size_t i = 1; i <= n; ++i)
            for (size_t k = i; k > 0 && fv[k] < fv[k - 1]; --k) {
                std::swap(fv[k], fv[k - 1]);
                simplex[k].swap(simplex[k - 1]);
            }
        if (fv[n] - fv[0] <= opt.tolMerit) { out.converged = true; break; }

        // 最悪点を除く重心
        std::vector<double> cen(n, 0.0);
        for (size_t i = 0; i < n; ++i)
            for (size_t j = 0; j < n; ++j) cen[j] += simplex[i][j] / double(n);

        auto along = [&](double t) {
            std::vector<double> p(n);
            for (size_t j = 0; j < n; ++j)
                p[j] = cen[j] + t * (cen[j] - simplex[n][j]);
            return p;
        };

        std::vector<double> xr = along(kRefl);
        const double fr = f(xr);
        if (fr < fv[0]) {                              // 拡大
            std::vector<double> xe = along(kExp);
            const double fe = f(xe);
            if (fe < fr) { simplex[n].swap(xe); fv[n] = fe; }
            else         { simplex[n].swap(xr); fv[n] = fr; }
        } else if (fr < fv[n - 1]) {                   // 反射を採用
            simplex[n].swap(xr); fv[n] = fr;
        } else {                                       // 縮小
            std::vector<double> xc = along(fr < fv[n] ? kCon : -kCon);
            const double fc = f(xc);
            if (fc < std::min(fr, fv[n])) { simplex[n].swap(xc); fv[n] = fc; }
            else {                                     // 全体収縮
                for (size_t i = 1; i <= n; ++i) {
                    for (size_t j = 0; j < n; ++j)
                        simplex[i][j] = simplex[0][j]
                                      + kShrink * (simplex[i][j] - simplex[0][j]);
                    fv[i] = f(simplex[i]);
                }
            }
        }
    }

    size_t best = 0;
    for (size_t i = 1; i <= n; ++i) if (fv[i] < fv[best]) best = i;
    if (!(fv[best] < std::numeric_limits<double>::max())) return out;

    out.d_nm = simplex[best];
    clampD(out.d_nm);
    out.meritStart = f0;
    out.meritEnd   = fv[best];
    out.iterations = iter;
    out.valid = true;
    return out;
}

// ── 製造誤差モンテカルロ ────────────────────────────────────────────────────
ToleranceResult monteCarlo(const StackAtLambda &stack,
                           const std::vector<TargetBand> &targets,
                           double aoi_deg, const ToleranceOptions &opt)
{
    ToleranceResult out;
    if (!finiteAngle(aoi_deg) || opt.trials <= 0 || opt.sigmaRel < 0.0)
        return out;
    const Grid g = buildGrid(stack, targets);
    out.skipped = g.skipped;
    if (!g.ok || g.nLayers < 0) return out;

    const double m0 = meritOf(g, targets, aoi_deg, std::vector<double>(), nullptr);
    if (!(m0 == m0)) return out;
    out.meritNominal = m0;

    Gauss gauss(opt.seed);
    std::vector<double> scale(size_t(g.nLayers), 1.0);
    std::vector<double> merits;
    merits.reserve(size_t(opt.trials));
    double sum = 0.0;
    int passed = 0;
    for (int t = 0; t < opt.trials; ++t) {
        // 系統誤差 = 全層共通のレートドリフト (ランダム誤差と同じ 1σ)
        const double common = opt.systematic ? opt.sigmaRel * gauss() : 0.0;
        for (int j = 0; j < g.nLayers; ++j)
            scale[size_t(j)] = std::max(0.0, 1.0 + common + opt.sigmaRel * gauss());
        bool inTol = false;
        const double m = meritOf(g, targets, aoi_deg, scale, &inTol);
        if (!(m == m)) return out;
        merits.push_back(m);
        sum += m;
        if (inTol) ++passed;
    }

    std::sort(merits.begin(), merits.end());
    out.trials    = opt.trials;
    out.passed    = passed;
    out.used      = int(g.lambda.size());
    out.yield     = double(passed) / double(opt.trials);
    out.meritMean = sum / double(opt.trials);
    // 90 パーセンタイル (最近傍順位法)
    {
        const size_t idx = size_t(std::min<double>(double(merits.size()) - 1.0,
                                                   std::floor(0.9 * (double(merits.size()) - 1.0) + 0.5)));
        out.meritP90 = merits[idx];
    }
    out.valid = true;
    return out;
}

} // namespace optics
} // namespace ofd
