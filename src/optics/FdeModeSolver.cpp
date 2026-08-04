// FdeModeSolver.cpp — 断面 FDE ソルバ本体 (虚軸伝搬 / ADI + Thomas 法)
//
// ── 定式化 ─────────────────────────────────────────────────────────────────
// 一様格子 (節点中心) 上で横方向演算子を 5 点差分にする。
//     L = Lx + Ly + V,   V = k0² n²,   L E = β² E,   neff = β/k0
// 境界は Dirichlet (窓の外は E=0)。窓はモードの裾が壁に届かない広さを取る。
//
// ── 虚軸伝搬法 (imaginary-distance FD-BPM) ─────────────────────────────────
// 近軸 BPM の z を虚数にすると拡散型になり、β² が最大のモードが指数的に卓越する:
//     ∂E/∂τ = (L − λref) E,   λref = k0² n_max²  (これで L − λref は負定値)
// これを陰的 (後退 Euler) に進めると
//     E^{k+1} = [ I − Δτ(L − λref) ]⁻¹ E^k
// で、Δτ→∞ の極限が M⁻¹E (M = λref − L, 正定値) — すなわちシフト逆べき乗法に
// なる。本実装はこの極限を採る。理由は 2 つ:
//   (1) 有限 Δτ の ADI 分解 (Peaceman-Rachford) は演算子分解誤差 O(Δτ²[A,B])
//       を持ち、その誤差が「反復の不動点そのもの」に残る。屈折率が階段状に
//       変わる導波路では交換子 [Lx, V] が界面で 1 格子幅の巨大値になるため、
//       固有ベクトルに界面局在の誤差が居座る (2D では検出も難しい)。
//   (2) M x = b を ADI 反復 (下記) で解く形なら、ADI はあくまで**線形方程式の
//       反復解法**なので、収束先は分解誤差ゼロの厳密解になる。
// 1 反復 = M x = E を解く → 既知モードへ Gram-Schmidt 直交化 → 正規化。
//
// ── ADI 内部ソルバ (Peaceman-Rachford, 各半段が Thomas 法) ─────────────────
//     M = A + B,  A = −Lx + (λref − V)/2,  B = −Ly + (λref − V)/2
// A・B はどちらも正定値 (−Lx, −Ly は Dirichlet ラプラシアンで正定値、
// (λref − V)/2 ≥ 0)。PR 反復
//     (A + τI) h    = b + (τI − B) x
//     (B + τI) x_new= b + (τI − A) h
// は x 方向・y 方向の三重対角解 (Thomas 法) 2 回に分解でき、不動点は
// (A+B)x = b の厳密解。τ は α..β の幾何数列 (ADI パラメータ巡回) を使う。
// τ の選び方は収束速度にしか効かない (不動点は τ に依らない)。
//
// ── 半ベクトル差分 (採用スキーム) ──────────────────────────────────────────
// 不連続方向 s の主成分 u については D_s = ε u が連続なので、ψ = ε u を主変数
// として保存形 ∂/∂s[(1/ε) ∂ψ/∂s] を 3 点差分にする。界面 (格子中点) の係数
// 1/ε は流束連続から調和平均 c_{i+1/2} = 2/(ε_i + ε_{i+1}) を取る
// (Stern, IEE Proc. J 135 (1988) の半ベクトル FD と同型)。
// ε が一様なら通常の 2 階中心差分に厳密に一致する (スカラーに帰着)。
// SemiVecTE は x 方向のみ、SemiVecTM は y 方向のみこの扱いにする。
//
// ── neff の取り出し ────────────────────────────────────────────────────────
// 収束場 φ に対する Rayleigh 商 β² = <φ, Lφ> / <φ, φ>。伝搬定数の反復推定値を
// そのまま使うより収束が速く、相対残差 ‖Lφ − β²φ‖/‖β²φ‖ を収束判定にも使える。
//
// ── 決定性 ─────────────────────────────────────────────────────────────────
// 乱数を一切使わない。初期場は「コア重心のガウシアン × 座標の固定べき乗パターン」
// で、同じ入力なら演算順序まで同一 → 出力はビット一致する。
//
// ── 実測精度 (tests/selftest.cpp の testFdeModeSolver) ─────────────────────
// 対称スラブ (n_core=3.476, n_clad=1.444, t=0.22um, λ=1.55um) を y 方向にだけ
// 変化させた断面で解き、x 方向の離散量子化分を閉形式で除いてから厳密解
// (超越方程式) と比べたときの neff 誤差 (実測):
//     dy=10.0nm : TE0 +1.021e-3 / TM0 +1.860e-3
//     dy= 5.0nm : TE0 +2.555e-4 / TM0 +4.648e-4
//     dy= 2.5nm : TE0 +6.389e-5 / TM0 +1.162e-4
// 比はいずれも 4.00 で、誤差が離散化誤差 (2 次収束) であることを示す。
// 速度 (実測, -O3 / 単スレッド, modes=4 を要求した場合の全体):
//   Si 450x220nm・格子 10nm (168x142 = 23k 点) : TE 0.58 s / TM 0.70 s (1 本)
//   Si 900x220nm・格子 10nm (270x142 = 38k 点) : TE 2.1 s / TM 2.0 s  (2 本)
// いずれも「導波モードでない状態と判定して打ち切る」時間を含む。

#include "FdeModeSolver.h"

#include <algorithm>
#include <cmath>

namespace ofd {
namespace optics {

namespace {

const double kPi = 3.14159265358979323846;

struct Grid {
    int nx = 0, ny = 0, n = 0;
    double dx = 0.0, dy = 0.0;
};

// L = Lx + Ly + V の係数 (節点ごと)
struct Coeff {
    std::vector<double> xw, xc, xe;   // Lx: 西 / 中央 / 東
    std::vector<double> ys, yc, yn;   // Ly: 南 / 中央 / 北
    std::vector<double> v;            // k0² n²
};

// ── 三重対角 (Thomas 法) ───────────────────────────────────────────────────
// 線 (start, stride, len) ごとに前進消去の係数を作る。a は下副対角 (τ に依らない)。
void triFactorLine(const double *a, const double *b, const double *c,
                   double *cprime, double *invden,
                   int start, int stride, int len)
{
    invden[start] = 1.0 / b[start];
    cprime[start] = c[start] * invden[start];
    int idx = start;
    for (int k = 1; k < len; ++k) {
        idx += stride;
        const double den = b[idx] - a[idx] * cprime[idx - stride];
        invden[idx] = 1.0 / den;
        cprime[idx] = c[idx] * invden[idx];
    }
}

void triSolveLine(const double *a, const double *cprime, const double *invden,
                  const double *rhs, double *x,
                  int start, int stride, int len)
{
    int idx = start;
    x[start] = rhs[start] * invden[start];
    for (int k = 1; k < len; ++k) {
        idx += stride;
        x[idx] = (rhs[idx] - a[idx] * x[idx - stride]) * invden[idx];
    }
    for (int k = len - 2; k >= 0; --k) {
        idx -= stride;
        x[idx] -= cprime[idx] * x[idx + stride];
    }
}

// ── 三重対角行列ベクトル積 ────────────────────────────────────────────────
void applyTriX(const Grid &g, const std::vector<double> &low,
               const std::vector<double> &diag, const std::vector<double> &up,
               const std::vector<double> &x, std::vector<double> &out)
{
    for (int iy = 0; iy < g.ny; ++iy) {
        const int row = iy * g.nx;
        for (int ix = 0; ix < g.nx; ++ix) {
            const int i = row + ix;
            double s = diag[i] * x[i];
            if (ix > 0)          s += low[i] * x[i - 1];
            if (ix < g.nx - 1)   s += up[i] * x[i + 1];
            out[i] = s;
        }
    }
}

void applyTriY(const Grid &g, const std::vector<double> &low,
               const std::vector<double> &diag, const std::vector<double> &up,
               const std::vector<double> &x, std::vector<double> &out)
{
    for (int iy = 0; iy < g.ny; ++iy) {
        const int row = iy * g.nx;
        for (int ix = 0; ix < g.nx; ++ix) {
            const int i = row + ix;
            double s = diag[i] * x[i];
            if (iy > 0)          s += low[i] * x[i - g.nx];
            if (iy < g.ny - 1)   s += up[i] * x[i + g.nx];
            out[i] = s;
        }
    }
}

// ── ADI 演算子 (M = A + B と τ 巡回の LU) ─────────────────────────────────
struct AdiOp {
    std::vector<double> aLow, aDiag, aUp;   // A (x 方向三重対角)
    std::vector<double> bLow, bDiag, bUp;   // B (y 方向三重対角)
    std::vector<double> tau;
    std::vector<std::vector<double> > acp, ainv;   // τ ごとの Thomas 係数 (A)
    std::vector<std::vector<double> > bcp, binv;   // 同 (B)
};

double dot(const std::vector<double> &a, const std::vector<double> &b)
{
    double s = 0.0;
    for (size_t i = 0; i < a.size(); ++i) s += a[i] * b[i];
    return s;
}

double norm2(const std::vector<double> &a) { return std::sqrt(dot(a, a)); }

// L (全演算子) の作用
void applyL(const Grid &g, const Coeff &c,
            const std::vector<double> &u, std::vector<double> &out)
{
    for (int iy = 0; iy < g.ny; ++iy) {
        const int row = iy * g.nx;
        for (int ix = 0; ix < g.nx; ++ix) {
            const int i = row + ix;
            double s = (c.xc[i] + c.yc[i] + c.v[i]) * u[i];
            if (ix > 0)         s += c.xw[i] * u[i - 1];
            if (ix < g.nx - 1)  s += c.xe[i] * u[i + 1];
            if (iy > 0)         s += c.ys[i] * u[i - g.nx];
            if (iy < g.ny - 1)  s += c.yn[i] * u[i + g.nx];
            out[i] = s;
        }
    }
}

// M x = b を PR-ADI 反復で解く。x は入出力 (初期推定を与えると速い)。
// relTol は ‖b − Mx‖/‖b‖ の目標。外側 (逆べき乗) の残差に合わせて緩めてよい
// (不正確逆反復) ため、呼び出し側が段階的に厳しくする。
void adiSolve(const Grid &g, const AdiOp &op, const std::vector<double> &b,
              std::vector<double> &x, double relTol, int maxSweeps,
              std::vector<double> &t1, std::vector<double> &t2)
{
    const int m = static_cast<int>(op.tau.size());
    const double bn = norm2(b);
    if (bn <= 0.0) { std::fill(x.begin(), x.end(), 0.0); return; }

    for (int s = 0; s < maxSweeps; ++s) {
        const double tau = op.tau[s % m];
        // (A + τI) h = b + (τI − B) x
        applyTriY(g, op.bLow, op.bDiag, op.bUp, x, t1);
        for (int i = 0; i < g.n; ++i) t1[i] = b[i] + tau * x[i] - t1[i];
        for (int iy = 0; iy < g.ny; ++iy)
            triSolveLine(op.aLow.data(), op.acp[s % m].data(),
                         op.ainv[s % m].data(),
                         t1.data(), t2.data(), iy * g.nx, 1, g.nx);
        // (B + τI) x = b + (τI − A) h
        applyTriX(g, op.aLow, op.aDiag, op.aUp, t2, t1);
        for (int i = 0; i < g.n; ++i) t1[i] = b[i] + tau * t2[i] - t1[i];
        for (int ix = 0; ix < g.nx; ++ix)
            triSolveLine(op.bLow.data(), op.bcp[s % m].data(),
                         op.binv[s % m].data(),
                         t1.data(), x.data(), ix, g.nx, g.ny);

        // 残差 ‖b − Mx‖/‖b‖ (毎回だと 5 割増しになるので 3 掃引に 1 回)
        if ((s % 3) == 2 || s == maxSweeps - 1) {
            applyTriX(g, op.aLow, op.aDiag, op.aUp, x, t1);
            applyTriY(g, op.bLow, op.bDiag, op.bUp, x, t2);
            double r2 = 0.0;
            for (int i = 0; i < g.n; ++i) {
                const double r = b[i] - (t1[i] + t2[i]);
                r2 += r * r;
            }
            if (std::sqrt(r2) <= relTol * bn) break;
        }
    }
}

// ── 係数の組み立て ────────────────────────────────────────────────────────
// dir: 0 = x, 1 = y。semi が真なら半ベクトル (調和平均) 差分。
void buildDirCoeff(const Grid &g, const std::vector<double> &eps, int dir,
                   bool semi, std::vector<double> &low,
                   std::vector<double> &cen, std::vector<double> &up)
{
    const int len   = (dir == 0) ? g.nx : g.ny;
    const int cnt   = (dir == 0) ? g.ny : g.nx;
    const int strd  = (dir == 0) ? 1 : g.nx;
    const int outer = (dir == 0) ? g.nx : 1;
    const double h  = (dir == 0) ? g.dx : g.dy;
    const double ih2 = 1.0 / (h * h);

    low.assign(g.n, 0.0);
    cen.assign(g.n, 0.0);
    up.assign(g.n, 0.0);

    for (int q = 0; q < cnt; ++q) {
        const int base = q * outer;
        for (int k = 0; k < len; ++k) {
            const int i = base + k * strd;
            if (!semi) {
                if (k > 0)       low[i] = ih2;
                if (k < len - 1) up[i]  = ih2;
                cen[i] = -2.0 * ih2;
            } else {
                const double ec = eps[i];
                const double em = (k > 0)       ? eps[i - strd] : ec;
                const double ep = (k < len - 1) ? eps[i + strd] : ec;
                const double cW = 2.0 / (em + ec);   // 1/ε の調和平均 (西界面)
                const double cE = 2.0 / (ec + ep);   // 同 (東界面)
                if (k > 0)       low[i] = cW * em * ih2;
                if (k < len - 1) up[i]  = cE * ep * ih2;
                cen[i] = -(cW + cE) * ec * ih2;
            }
        }
    }
}

// 固定の初期場 (乱数不使用)。ガウシアン × 座標べき乗パターンの固定重み和。
//
// 重要: 単一パターン (例えば x に奇関数だけ) を種にすると、左右対称な断面では
// 演算子が奇/偶の部分空間を厳密に保存するため反復がその部分空間から出られず、
// 「neff 降順」の順序が崩れる (実測: 対称スラブで mode1 と mode2 が入れ替わった)。
// そこで全パターンを固定重みで重ね、どの固有ベクトルとも重なりが 0 にならない
// ようにする。order 番のパターンに主重みを置くのは収束を速めるためだけ。
void seedField(const Grid &g, int order, double xc, double yc,
               double wx, double wy, std::vector<double> &phi)
{
    static const int kPow[][2] = {
        {0,0},{1,0},{0,1},{1,1},{2,0},{0,2},{2,1},{1,2},
        {2,2},{3,0},{0,3},{3,1},{1,3},{4,0},{0,4},{3,3}
    };
    const int npat = static_cast<int>(sizeof(kPow) / sizeof(kPow[0]));
    double w[16];
    for (int j = 0; j < npat; ++j) {
        const int d = (j > order) ? (j - order) : (order - j);
        w[j] = (j == order % npat) ? 1.0 : 0.25 / (1.0 + d);
    }

    phi.assign(g.n, 0.0);
    for (int iy = 0; iy < g.ny; ++iy) {
        const double y = (iy + 0.5) * g.dy;
        const double by = (y - yc) / wy;
        for (int ix = 0; ix < g.nx; ++ix) {
            const double x = (ix + 0.5) * g.dx;
            const double ax = (x - xc) / wx;
            double px[5], py[5];
            px[0] = py[0] = 1.0;
            for (int t = 1; t < 5; ++t) { px[t] = px[t - 1] * ax; py[t] = py[t - 1] * by; }
            double s = 0.0;
            for (int j = 0; j < npat; ++j) s += w[j] * px[kPow[j][0]] * py[kPow[j][1]];
            phi[iy * g.nx + ix] = std::exp(-(ax * ax + by * by)) * s;
        }
    }
}

} // namespace

// ── 本体 ──────────────────────────────────────────────────────────────────
std::vector<ModeResult> solveModes(const CrossSection &cs, double lambda_um,
                                   const SolveOptions &opt)
{
    std::vector<ModeResult> out;
    if (cs.nx < 3 || cs.ny < 3 || lambda_um <= 0.0) return out;
    if (cs.dx_um <= 0.0 || cs.dy_um <= 0.0) return out;
    const int N = cs.nx * cs.ny;
    if (static_cast<int>(cs.n.size()) != N) return out;
    const int wanted = std::min(std::max(opt.modes, 0), 32);
    if (wanted == 0) return out;

    Grid g;
    g.nx = cs.nx; g.ny = cs.ny; g.n = N; g.dx = cs.dx_um; g.dy = cs.dy_um;

    const bool hasCore = (static_cast<int>(cs.core.size()) == N);
    const double k0 = 2.0 * kPi / lambda_um;
    const double k02 = k0 * k0;

    // ε = n²、最大屈折率、最大クラッド屈折率
    std::vector<double> eps(N);
    double nMax = 0.0, nCladMax = -1.0, nMin = 1e30;
    for (int i = 0; i < N; ++i) {
        const double nv = cs.n[i];
        if (nv <= 0.0) return out;            // 屈折率が不正な断面は解かない
        eps[i] = nv * nv;
        nMax = std::max(nMax, nv);
        nMin = std::min(nMin, nv);
        if (!hasCore || cs.core[i] == 0) nCladMax = std::max(nCladMax, nv);
    }
    if (nCladMax < 0.0) nCladMax = nMin;      // 全域がコア指定のときの保険

    // 差分係数
    Coeff c;
    const bool semiX = (opt.pol == Polarization::SemiVecTE);
    const bool semiY = (opt.pol == Polarization::SemiVecTM);
    buildDirCoeff(g, eps, 0, semiX, c.xw, c.xc, c.xe);
    buildDirCoeff(g, eps, 1, semiY, c.ys, c.yc, c.yn);
    c.v.resize(N);
    for (int i = 0; i < N; ++i) c.v[i] = k02 * eps[i];

    // シフト λref = k0² n_max² (M = λref − L を正定値にする最小のシフト)
    const double lref = k02 * nMax * nMax;

    AdiOp op;
    op.aLow.resize(N); op.aDiag.resize(N); op.aUp.resize(N);
    op.bLow.resize(N); op.bDiag.resize(N); op.bUp.resize(N);
    for (int i = 0; i < N; ++i) {
        const double half = 0.5 * (lref - c.v[i]);   // ≥ 0
        op.aLow[i] = -c.xw[i]; op.aUp[i] = -c.xe[i];
        op.aDiag[i] = -c.xc[i] + half;
        op.bLow[i] = -c.ys[i]; op.bUp[i] = -c.yn[i];
        op.bDiag[i] = -c.yc[i] + half;
    }

    // ADI パラメータ巡回。α は Dirichlet ラプラシアンの最小固有値 (閉形式)、
    // β は Gershgorin 上界。収束先には影響せず収束速度だけを決める。
    const double aX = (4.0 / (g.dx * g.dx))
                    * std::pow(std::sin(kPi / (2.0 * (g.nx + 1))), 2.0);
    const double aY = (4.0 / (g.dy * g.dy))
                    * std::pow(std::sin(kPi / (2.0 * (g.ny + 1))), 2.0);
    double alpha = std::min(aX, aY);
    double bmax = 0.0;
    for (int i = 0; i < N; ++i) {
        bmax = std::max(bmax, op.aDiag[i] + std::fabs(op.aLow[i]) + std::fabs(op.aUp[i]));
        bmax = std::max(bmax, op.bDiag[i] + std::fabs(op.bLow[i]) + std::fabs(op.bUp[i]));
    }
    if (alpha <= 0.0) alpha = 1e-6;
    const double ratio = std::max(bmax / alpha, 10.0);
    int m = static_cast<int>(std::ceil(std::log(ratio) / 1.2));
    m = std::max(4, std::min(m, 12));
    op.tau.resize(m);
    for (int j = 0; j < m; ++j)
        op.tau[j] = alpha * std::pow(ratio, (2.0 * j + 1.0) / (2.0 * m));

    op.acp.resize(m); op.ainv.resize(m); op.bcp.resize(m); op.binv.resize(m);
    std::vector<double> dtmp(N);
    for (int j = 0; j < m; ++j) {
        const double tau = op.tau[j];
        op.acp[j].assign(N, 0.0); op.ainv[j].assign(N, 0.0);
        for (int i = 0; i < N; ++i) dtmp[i] = op.aDiag[i] + tau;
        for (int iy = 0; iy < g.ny; ++iy)
            triFactorLine(op.aLow.data(), dtmp.data(), op.aUp.data(),
                          op.acp[j].data(), op.ainv[j].data(), iy * g.nx, 1, g.nx);
        op.bcp[j].assign(N, 0.0); op.binv[j].assign(N, 0.0);
        for (int i = 0; i < N; ++i) dtmp[i] = op.bDiag[i] + tau;
        for (int ix = 0; ix < g.nx; ++ix)
            triFactorLine(op.bLow.data(), dtmp.data(), op.bUp.data(),
                          op.bcp[j].data(), op.binv[j].data(), ix, g.nx, g.ny);
    }

    // 初期場の中心・幅 (コア領域の外接矩形。コア指定が無ければ窓の中央)
    double xc = 0.5 * g.nx * g.dx, yc = 0.5 * g.ny * g.dy;
    double wx = 0.25 * g.nx * g.dx, wy = 0.25 * g.ny * g.dy;
    if (hasCore) {
        int x0 = g.nx, x1 = -1, y0 = g.ny, y1 = -1;
        for (int iy = 0; iy < g.ny; ++iy)
            for (int ix = 0; ix < g.nx; ++ix)
                if (cs.core[iy * g.nx + ix]) {
                    x0 = std::min(x0, ix); x1 = std::max(x1, ix);
                    y0 = std::min(y0, iy); y1 = std::max(y1, iy);
                }
        if (x1 >= x0) {
            xc = 0.5 * (x0 + x1 + 1.0) * g.dx;
            yc = 0.5 * (y0 + y1 + 1.0) * g.dy;
            wx = std::max(0.75 * (x1 - x0 + 1) * g.dx, 2.0 * g.dx);
            wy = std::max(0.75 * (y1 - y0 + 1) * g.dy, 2.0 * g.dy);
        }
    }

    const double tol = (opt.tol > 0.0) ? opt.tol : 1e-9;
    const int maxIt = std::max(20, opt.maxSteps);

    std::vector<std::vector<double> > found;   // 正規化済みの収束場 (直交化用)
    std::vector<double> phi(N), xv(N), lp(N), t1(N), t2(N);

    for (int k = 0; k < wanted; ++k) {
        // ── 初期場: 既知モードに直交する成分が残るパターンを選ぶ
        bool seeded = false;
        for (int trial = 0; trial < 16 && !seeded; ++trial) {
            seedField(g, k + trial, xc, yc, wx, wy, phi);
            for (size_t q = 0; q < found.size(); ++q) {
                const double d = dot(found[q], phi);
                for (int i = 0; i < N; ++i) phi[i] -= d * found[q][i];
            }
            const double nn = norm2(phi);
            if (nn > 1e-8) {
                for (int i = 0; i < N; ++i) phi[i] /= nn;
                seeded = true;
            }
        }
        if (!seeded) break;

        applyL(g, c, phi, lp);
        double beta2 = dot(phi, lp);
        double mu = std::max(lref - beta2, 1e-6);
        for (int i = 0; i < N; ++i) xv[i] = phi[i] / mu;

        double prevNeff = 0.0;
        bool converged = false;
        double neff = 0.0, relRes = 1.0;
        double innerTol = 1e-2;              // 不正確逆反復: 初回は粗く
        double stallRes = 1e300;             // 停滞検出用 (50 反復前の残差)

        for (int it = 0; it < maxIt; ++it) {
            adiSolve(g, op, phi, xv, innerTol, 6 * m, t1, t2);

            // 既知モードを除去 (デフレーション)
            for (size_t q = 0; q < found.size(); ++q) {
                const double d = dot(found[q], xv);
                for (int i = 0; i < N; ++i) xv[i] -= d * found[q][i];
            }
            const double nn = norm2(xv);
            if (!(nn > 0.0)) break;
            for (int i = 0; i < N; ++i) phi[i] = xv[i] / nn;

            // 符号を決定的に固定 (最大振幅の要素を正にする)
            int imax = 0;
            for (int i = 1; i < N; ++i)
                if (std::fabs(phi[i]) > std::fabs(phi[imax])) imax = i;
            if (phi[imax] < 0.0)
                for (int i = 0; i < N; ++i) phi[i] = -phi[i];

            applyL(g, c, phi, lp);
            beta2 = dot(phi, lp);
            neff = (beta2 > 0.0) ? std::sqrt(beta2) / k0 : 0.0;

            double r2 = 0.0;
            for (int i = 0; i < N; ++i) {
                const double r = lp[i] - beta2 * phi[i];
                r2 += r * r;
            }
            relRes = std::sqrt(r2) / std::max(std::fabs(beta2), 1e-30);

            if (it > 0 && std::fabs(neff - prevNeff) <= tol * std::max(neff, 1.0)
                && relRes < 1e-4) {
                converged = true;
                break;
            }
            prevNeff = neff;
            innerTol = std::max(1e-11, 0.02 * relRes);

            // 窓 (Dirichlet 箱) 由来の非導波状態は導波路のモードではないので
            // 深追いしない。デフレーション後の Rayleigh 商は残りの最大固有値を
            // 上限に持ち、逆反復でそこへ向かって上がっていく。60 反復しても最大
            // クラッド屈折率に届かないなら、行き先も導波モードではない。
            // (誤って導波モードを捨てないよう、収束に必要な反復数 ~50 より
            //  余裕を持たせてある)
            if (it >= 60 && neff <= nCladMax) break;

            // 停滞検出。半ベクトル (非対称演算子) では Euclid 内積の
            // Gram-Schmidt が厳密なデフレーションにならず、既知モードの残りが
            // 除去しきれずに残差が下げ止まることがある。50 反復で残差が半分にも
            // ならなければ以後も下がらないので打ち切る (残差が既に十分小さい
            // ときだけ収束とみなす)。実測: 900x220nm TE の 3 番目 (窓モード) が
            // これに該当し、無ければ上限 4000 反復まで走って 40 秒以上かかった。
            if ((it % 50) == 49) {
                if (relRes > 0.5 * stallRes) {
                    if (relRes < 1e-6) converged = true;
                    break;
                }
                stallRes = relRes;
            }

            mu = std::max(lref - beta2, 1e-6);
            for (int i = 0; i < N; ++i) xv[i] = phi[i] / mu;
        }

        // 収束しなかったモード / 非導波 (窓) モードは返さない。
        // ここで打ち切るのは、以降のモードの順序 (neff 降順) を保証できないため。
        if (!converged || neff <= nCladMax) break;

        // ── 指標の算出
        ModeResult r;
        r.neff = neff;
        r.guided = (neff > nCladMax);
        r.field = phi;                 // 離散 L2 ノルム 1、符号は決定的に固定済み
        r.intensity.resize(N);
        double imaxv = 0.0, sumI = 0.0, sumI2 = 0.0, sumCore = 0.0;
        for (int i = 0; i < N; ++i) {
            const double ii = phi[i] * phi[i];
            r.intensity[i] = ii;
            imaxv = std::max(imaxv, ii);
            sumI += ii;
            sumI2 += ii * ii;
            if (hasCore && cs.core[i]) sumCore += ii;
        }
        const double dA = g.dx * g.dy;
        r.gamma = (sumI > 0.0) ? sumCore / sumI : 0.0;
        r.aeff_um2 = (sumI2 > 0.0) ? (sumI * sumI) * dA / sumI2 : 0.0;
        if (imaxv > 0.0)
            for (int i = 0; i < N; ++i) r.intensity[i] /= imaxv;
        out.push_back(r);

        found.push_back(phi);
    }

    return out;
}

// ── 矩形コア断面の組み立て ────────────────────────────────────────────────
CrossSection makeRectangularCore(double coreW_um, double coreH_um,
                                 double slabH_um,
                                 double nCore, double nClad, double nSub,
                                 double targetDx_um, double marginRatio)
{
    CrossSection cs;
    if (coreW_um <= 0.0 || coreH_um <= 0.0 || targetDx_um <= 0.0) return cs;
    if (nCore <= 0.0 || nClad <= 0.0 || nSub <= 0.0) return cs;
    if (marginRatio < 0.2) marginRatio = 0.2;

    // コア幅・高さが整数セルになるよう格子間隔を丸める
    // (材料界面がセル境界 = 節点の中点に載るので、半ベクトル差分が正しく効く)
    int ncw = 2 * static_cast<int>(std::lround(coreW_um / (2.0 * targetDx_um)));
    if (ncw < 2) ncw = 2;
    const double dx = coreW_um / ncw;
    int nch = static_cast<int>(std::lround(coreH_um / targetDx_um));
    if (nch < 1) nch = 1;
    const double dy = coreH_um / nch;

    // クラッド余白。基本モードの裾 (1/e 長 ≒ 0.1um オーダー) が Dirichlet 壁に
    // 届かないよう、比率指定に加えて 0.6um の下限を置く。
    const int nmx = std::max(1, static_cast<int>(
        std::lround(std::max(marginRatio * coreW_um, 0.6) / dx)));
    const int nmy = std::max(1, static_cast<int>(
        std::lround(std::max(marginRatio * coreH_um, 0.6) / dy)));

    const int nx = ncw + 2 * nmx;
    const int ny = nch + 2 * nmy;
    cs.nx = nx; cs.ny = ny; cs.dx_um = dx; cs.dy_um = dy;
    cs.n.assign(static_cast<size_t>(nx) * ny, nClad);
    cs.core.assign(static_cast<size_t>(nx) * ny, 0);

    // リブのスラブ厚は dy の整数倍へ丸める (界面をセル境界に載せるため)
    double slab = 0.0;
    if (slabH_um > 0.0)
        slab = std::min(coreH_um, std::lround(slabH_um / dy) * dy);

    const double halfW = 0.5 * coreW_um;
    for (int iy = 0; iy < ny; ++iy) {
        // y = 0 がコア/スラブ底面、y = coreH がコア上面 (どちらもセル境界)
        const double y = (iy + 0.5 - nmy) * dy;
        for (int ix = 0; ix < nx; ++ix) {
            const double x = (ix + 0.5 - 0.5 * nx) * dx;
            const size_t i = static_cast<size_t>(iy) * nx + ix;
            if (y < 0.0) {
                cs.n[i] = nSub;
            } else if (y < coreH_um) {
                if (std::fabs(x) < halfW || (slab > 0.0 && y < slab)) {
                    cs.n[i] = nCore;
                    cs.core[i] = 1;
                } else {
                    cs.n[i] = nClad;
                }
            } else {
                cs.n[i] = nClad;
            }
        }
    }
    return cs;
}

} // namespace optics
} // namespace ofd
