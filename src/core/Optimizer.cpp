// Optimizer.cpp — PSO / 実数値 GA (詳細は .h)
#include "Optimizer.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ofd {
namespace optim {

namespace {

inline uint64_t splitmix64(uint64_t &x)
{
    x += 0x9E3779B97F4A7C15ull;
    uint64_t z = x;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

inline bool isNan(double v) { return std::isnan(v); }

} // namespace

Optimizer::Optimizer(const std::vector<Variable> &vars, const Options &opt)
    : m_vars(vars), m_opt(opt)
{
    m_valid = !vars.empty() && opt.population >= 2 && opt.generations >= 1;
    for (const Variable &v : vars)
        if (!(v.hi > v.lo)) m_valid = false;
    if (opt.method == Method::ParticleSwarm && !(opt.c1 + opt.c2 > 4.0))
        m_valid = false;   // 収縮係数が定義されない
    if (!m_valid) return;

    m_state = opt.seed;
    seedPopulation();
}

double Optimizer::rnd()
{
    return static_cast<double>(splitmix64(m_state) >> 11)
         * (1.0 / 9007199254740992.0);
}

double Optimizer::clampVar(int j, double v) const
{
    const Variable &b = m_vars[static_cast<size_t>(j)];
    return std::min(b.hi, std::max(b.lo, v));
}

bool Optimizer::better(double a, double b) const
{
    if (isNan(a)) return false;          // NaN は必ず負ける
    if (isNan(b)) return true;
    return m_opt.maximize ? (a > b) : (a < b);
}

void Optimizer::seedPopulation()
{
    const int n = static_cast<int>(m_vars.size());
    const int P = m_opt.population;
    m_pending.assign(static_cast<size_t>(P), std::vector<double>(n, 0.0));

    for (int i = 0; i < P; ++i)
        for (int j = 0; j < n; ++j) {
            const Variable &b = m_vars[static_cast<size_t>(j)];
            // 1 個体目は初期値が指定されていればそこから始める
            // (利用者が「今の設計」を起点にできるように)
            m_pending[static_cast<size_t>(i)][static_cast<size_t>(j)] =
                (i == 0 && b.hasInit) ? clampVar(j, b.init)
                                      : b.lo + (b.hi - b.lo) * rnd();
        }

    if (m_opt.method == Method::ParticleSwarm) {
        m_vel.assign(static_cast<size_t>(P), std::vector<double>(n, 0.0));
        m_pbest = m_pending;
        m_pbestVal.assign(static_cast<size_t>(P),
                          std::numeric_limits<double>::quiet_NaN());
    }
}

void Optimizer::tell(const std::vector<double> &values)
{
    if (!m_valid || done()) return;
    if (values.size() != m_pending.size()) return;   // 数が合わなければ進めない

    for (size_t i = 0; i < values.size(); ++i) {
        if (isNan(values[i])) continue;
        ++m_evaluations;
        if (!m_hasBest || better(values[i], m_bestValue)) {
            m_best = m_pending[i];
            m_bestValue = values[i];
            m_hasBest = true;
        }
    }

    ++m_generation;
    if (done()) { m_pending.clear(); return; }

    if (m_opt.method == Method::ParticleSwarm) stepPso(values);
    else                                       stepGa(values);
}

// ── PSO (Clerc–Kennedy の収縮係数) ─────────────────────────────────────────
void Optimizer::stepPso(const std::vector<double> &values)
{
    const int n = static_cast<int>(m_vars.size());
    const int P = m_opt.population;
    const double phi = m_opt.c1 + m_opt.c2;
    const double chi = 2.0 / std::fabs(2.0 - phi
                                       - std::sqrt(phi * phi - 4.0 * phi));

    // 個体最良の更新
    for (int i = 0; i < P; ++i) {
        const size_t u = static_cast<size_t>(i);
        if (isNan(m_pbestVal[u]) || better(values[u], m_pbestVal[u])) {
            if (!isNan(values[u])) {
                m_pbest[u] = m_pending[u];
                m_pbestVal[u] = values[u];
            }
        }
    }

    for (int i = 0; i < P; ++i) {
        const size_t u = static_cast<size_t>(i);
        for (int j = 0; j < n; ++j) {
            const size_t w = static_cast<size_t>(j);
            const double r1 = rnd(), r2 = rnd();
            // 個体最良がまだ無い (全部 NaN だった) 粒子は自分自身を使う
            const double p = isNan(m_pbestVal[u]) ? m_pending[u][w]
                                                  : m_pbest[u][w];
            const double g = m_hasBest ? m_best[w] : m_pending[u][w];
            double v = chi * (m_vel[u][w]
                              + m_opt.c1 * r1 * (p - m_pending[u][w])
                              + m_opt.c2 * r2 * (g - m_pending[u][w]));
            double x = m_pending[u][w] + v;
            const double xc = clampVar(j, x);
            if (xc != x) v = 0.0;        // 吸収壁: 壁に当たった成分は止める
            m_vel[u][w] = v;
            m_pending[u][w] = xc;
        }
    }
}

// ── 実数値 GA (トーナメント + SBX + 多項式突然変異 + エリート保存) ─────────
void Optimizer::stepGa(const std::vector<double> &values)
{
    const int n = static_cast<int>(m_vars.size());
    const int P = m_opt.population;
    const double pm = (m_opt.mutationRate >= 0.0)
                          ? m_opt.mutationRate : 1.0 / n;

    auto tournament = [&]() -> const std::vector<double>& {
        const int a = static_cast<int>(rnd() * P) % P;
        const int b = static_cast<int>(rnd() * P) % P;
        const size_t ua = static_cast<size_t>(a), ub = static_cast<size_t>(b);
        return better(values[ua], values[ub]) ? m_pending[ua] : m_pending[ub];
    };

    std::vector<std::vector<double>> next;
    next.reserve(static_cast<size_t>(P));
    // エリート: これまでの最良を必ず 1 個体残す (最良値が悪化しない)
    if (m_hasBest) next.push_back(m_best);

    while (static_cast<int>(next.size()) < P) {
        std::vector<double> c1 = tournament();
        std::vector<double> c2 = tournament();

        if (rnd() < m_opt.crossoverRate) {
            for (int j = 0; j < n; ++j) {
                const size_t w = static_cast<size_t>(j);
                // SBX (Deb & Agrawal 1995)
                if (rnd() > 0.5) continue;
                if (std::fabs(c1[w] - c2[w]) < 1e-14) continue;
                const double u = rnd();
                const double beta = (u <= 0.5)
                    ? std::pow(2.0 * u, 1.0 / (m_opt.etaC + 1.0))
                    : std::pow(1.0 / (2.0 * (1.0 - u)), 1.0 / (m_opt.etaC + 1.0));
                const double x1 = c1[w], x2 = c2[w];
                c1[w] = clampVar(j, 0.5 * ((1.0 + beta) * x1 + (1.0 - beta) * x2));
                c2[w] = clampVar(j, 0.5 * ((1.0 - beta) * x1 + (1.0 + beta) * x2));
            }
        }
        // 多項式突然変異 (Deb & Goyal 1996)
        auto mutate = [&](std::vector<double> &c) {
            for (int j = 0; j < n; ++j) {
                if (rnd() >= pm) continue;
                const size_t w = static_cast<size_t>(j);
                const Variable &b = m_vars[w];
                const double span = b.hi - b.lo;
                const double u = rnd();
                const double delta = (u < 0.5)
                    ? std::pow(2.0 * u, 1.0 / (m_opt.etaM + 1.0)) - 1.0
                    : 1.0 - std::pow(2.0 * (1.0 - u), 1.0 / (m_opt.etaM + 1.0));
                c[w] = clampVar(j, c[w] + delta * span);
            }
        };
        mutate(c1);
        mutate(c2);

        next.push_back(c1);
        if (static_cast<int>(next.size()) < P) next.push_back(c2);
    }

    m_pending.swap(next);
}

} // namespace optim
} // namespace ofd
