// Optimizer.h — 微分を使わない箱制約付き最適化 (PSO / 実数値 GA)。
// Qt 非依存 / C++17。OptimizeTab の「PSO」「GA」手法の実体。
//
// ── なぜ ask / tell なのか ─────────────────────────────────────────────────
//
// 目的関数の 1 回の評価が「カーネルを 1 回まわす」= 数秒〜数分の非同期処理に
// なる。関数ポインタを渡して中で回す形 (`minimize(f, ...)`) にすると GUI が
// その間止まるので、**世代ぶんの設計点を渡す `ask()` と、その評価値を受け取る
// `tell()`** に分けてある。呼び出し側は
//
//     while (!opt.done()) {
//         const auto &pts = opt.ask();      // population 行 × 変数の数
//         ... 各行を評価 (SweepRunner で 1 世代ぶんまわす) ...
//         opt.tell(values);                 // 評価できなかった点は NaN
//     }
//
// と回す。GUI 側はこのループを世代ごとのシグナルで展開する。
//
// ── 手法 ───────────────────────────────────────────────────────────────────
//
// **PSO** — Clerc–Kennedy の収縮係数版 (M. Clerc & J. Kennedy, IEEE Trans.
// Evol. Comput. 6, 58 (2002))。φ = c₁ + c₂ > 4 に対し
//     χ = 2 / |2 − φ − √(φ² − 4φ)|
//     v ← χ(v + c₁r₁(p − x) + c₂r₂(g − x)),  x ← x + v
// で、慣性重みを手で決めずに収束が保証される形。箱の外へ出た成分は境界へ
// 貼り付けて速度成分を 0 にする (吸収壁)。
//
// **GA** — 実数値符号化。トーナメント選択 (2 個体) + SBX 交叉 + 多項式突然変異
// (K. Deb & R. B. Agrawal, Complex Systems 9, 115 (1995) / Deb & Goyal 1996)。
// 最良個体は必ず次世代へ残す (エリート保存) ので、最良値は世代を追って
// 悪くならない。
//
// ── 評価できなかった点 ─────────────────────────────────────────────────────
//
// カーネルが落ちた・FoM が取れない点は **NaN** で `tell()` する。NaN は
// 「最悪」として扱い、最良値・個体最良・トーナメントのいずれにも採らない
// (それらしい値で埋めない)。全点 NaN の世代があっても探索は続く。
//
// ── 再現性 ─────────────────────────────────────────────────────────────────
//
// 乱数は seed から決まる splitmix64 だけを使い、外部状態を持たない。
// 同じ seed・同じ評価値を与えれば**同じ設計点の列**が出る (selftest で検証)。
//
// ── 扱わないもの (絶対規則 5) ──────────────────────────────────────────────
//
// - 勾配 (随伴法) — カーネルが感度を返さないので実装できない。
// - 代理モデル (ベイズ最適化) — GP の実装が要る。
// - 多目的 (Pareto フロント) — ここは単一目的。
// - 箱以外の制約 (等式・不等式) — ペナルティの設計が問題ごとに違うので、
//   呼び出し側が目的関数に織り込む形にしてある。
#ifndef OFD_CORE_OPTIMIZER_H
#define OFD_CORE_OPTIMIZER_H

#include <cstdint>
#include <vector>

namespace ofd {
namespace optim {

enum class Method { ParticleSwarm = 0, Genetic = 1 };

// 1 個の設計変数 (箱制約)
struct Variable {
    double lo = 0.0;
    double hi = 1.0;
    double init = 0.0;
    bool   hasInit = false;   // true なら初期集団の 1 個体目に使う
};

struct Options {
    Method   method = Method::ParticleSwarm;
    int      population = 20;   // >= 2
    int      generations = 10;  // >= 1
    bool     maximize = false;
    uint64_t seed = 20260811ull;

    // PSO (φ = c1 + c2 > 4 でないと収縮係数が定義されない)
    double c1 = 2.05, c2 = 2.05;

    // GA
    double crossoverRate = 0.9;  // SBX を適用する確率
    double etaC = 15.0;          // SBX の分布指数
    double mutationRate = -1.0;  // < 0 なら 1/(変数の数)
    double etaM = 20.0;          // 多項式突然変異の分布指数
};

class Optimizer {
public:
    Optimizer(const std::vector<Variable> &vars, const Options &opt);

    // 変数が 0 個 / lo >= hi / population < 2 / generations < 1 /
    // PSO で c1 + c2 <= 4 のときは false。この状態では ask() は空を返す。
    bool valid() const { return m_valid; }

    // 次に評価する設計点 (population 行)。done() なら空。
    const std::vector<std::vector<double>> &ask() const { return m_pending; }

    // ask() の各行に対する評価値。NaN = 評価できなかった点。
    // 行数が合わないときは何もしない (黙って進めない)。
    void tell(const std::vector<double> &values);

    int  generation() const { return m_generation; }   // 済んだ世代数
    bool done() const { return !m_valid || m_generation >= m_opt.generations; }
    int  evaluations() const { return m_evaluations; } // 有効だった評価の数

    bool hasBest() const { return m_hasBest; }
    const std::vector<double> &best() const { return m_best; }
    double bestValue() const { return m_bestValue; }

    // 最大化なら a > b、最小化なら a < b。NaN は必ず負ける。
    bool better(double a, double b) const;

private:
    void seedPopulation();
    void stepPso(const std::vector<double> &values);
    void stepGa(const std::vector<double> &values);
    double rnd();                       // [0,1)
    double clampVar(int j, double v) const;

    std::vector<Variable> m_vars;
    Options  m_opt;
    bool     m_valid = false;

    std::vector<std::vector<double>> m_pending;   // 評価待ちの集団
    std::vector<std::vector<double>> m_vel;       // PSO の速度
    std::vector<std::vector<double>> m_pbest;     // PSO の個体最良
    std::vector<double>              m_pbestVal;
    std::vector<double>              m_curVal;    // GA: 現世代の評価値

    std::vector<double> m_best;
    double   m_bestValue = 0.0;
    bool     m_hasBest = false;
    int      m_generation = 0;
    int      m_evaluations = 0;
    uint64_t m_state = 0;
};

} // namespace optim
} // namespace ofd

#endif // OFD_CORE_OPTIMIZER_H
