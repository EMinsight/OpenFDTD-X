// ThinFilmStack.h — 多層薄膜の分光特性 (特性行列法 / Abeles 行列) — Qt 非依存
//
// 斜入射・s/p 偏波・吸収 (消衰係数 k) に対応した特性行列法で、反射率 R・
// 透過率 T・吸収率 A・反射位相・群遅延を計算する。さらに設計評価に使う
// メリット関数、膜厚感度、製造誤差モンテカルロを提供する。
// GUI 側 (src/tabs/ThinFilmTab.cpp) はこのヘッダのみを使う。
//
// 出典 (式はすべて公刊の標準形):
//   [1] H. A. Macleod, "Thin-Film Optical Filters", 4th ed., CRC Press (2010),
//       Chapter 2 — 特性行列、傾斜光学アドミッタンス η、R/T/A の定義。
//   [2] F. Abelès, Ann. Physique 12, 596 (1950) — 特性行列法の原論文。
//   [3] Sh. A. Furman, A. V. Tikhonravov, "Basics of Optics of Multilayer
//       Systems", Editions Frontières (1992), Ch. 1 — メリット関数の標準形。
//
// 規約 (Macleod [1] と同じ):
//   - 時間因子 exp(+iωt)、複素屈折率 Ñ = n − i·k (k ≥ 0 が損失)。
//   - 各層は完全にコヒーレント。基板は半無限 (裏面反射は含まない)。
//   - 入射媒質は無損失 (実屈折率)。
//   - アドミッタンスは自由空間アドミッタンス単位。s 偏波 η = Ñ·cosθ、
//     p 偏波 η = Ñ/cosθ。
//
// この規約下では無損失系で R + T = 1 が厳密に成り立つ (selftest で検証)。
#ifndef OFD_OPTICS_THINFILMSTACK_H
#define OFD_OPTICS_THINFILMSTACK_H

#include <functional>
#include <cstdint>
#include <vector>

namespace ofd {
namespace optics {

// 1 層 (複素屈折率 Ñ = n − i·k と物理膜厚)
struct FilmLayer {
    double n = 1.0;
    double k = 0.0;      // 消衰係数 (0 = 無損失)
    double d_nm = 0.0;   // 物理膜厚 [nm]
};

enum class Pol { S, P };

struct FilmResponse {
    bool   valid = false;
    double R = 0.0;           // 反射率 (0..1)
    double T = 0.0;           // 透過率 (0..1)
    double A = 0.0;           // 吸収率 = 1 − R − T (負にはしない)
    double phase_rad = 0.0;   // 反射係数 r の位相 arg(r)
};

// 単一波長・単一入射角・単一偏波の応答。layers は入射側 → 基板側の順。
// λ ≤ 0 / 入射角が [0,90) の外 / n0 ≤ 0 などでは valid = false を返す。
FilmResponse filmResponse(double n0, const std::vector<FilmLayer> &layers,
                          double nsub, double ksub, double lambda_nm,
                          double aoi_deg, Pol pol);

// ある波長における層構成一式。屈折率の分散は呼び出し側 (GUI) が
// MaterialDispersion などで解決してこの形に詰める。
struct StackSample {
    double n0   = 1.0;              // 入射媒質 (実屈折率)
    double nsub = 1.5, ksub = 0.0;  // 基板
    std::vector<FilmLayer> layers;  // 入射側 → 基板側
};

// λ [nm] における層構成を返すコールバック。材料データの有効範囲外などで
// 構成を作れないときは false を返す — その λ は評価から除外される
// (範囲外へ外挿した「それらしい値」を作らない)。
using StackAtLambda = std::function<bool(double lambda_nm, StackSample &out)>;

struct SpectrumPoint {
    double lambda_nm = 0;
    double Rs = 0, Rp = 0, Ts = 0, Tp = 0, As = 0, Ap = 0;
    // 反射の群遅延 [ps] (withGroupDelay = true のときのみ計算)。
    // τ_g = (λ²/2πc)·dφ/dλ を中心差分で求める (exp(+iωt) 規約)。
    double gds_ps = 0, gdp_ps = 0;
    bool   gdValid = false;
};

// λ グリッド上のスペクトル。stack が false を返した λ は結果に含めない
// (返る点数が points より少なくなり得る)。points ≤ 1 のときは中央 1 点。
std::vector<SpectrumPoint> spectrum(const StackAtLambda &stack,
                                    double lamMin_nm, double lamMax_nm,
                                    int points, double aoi_deg,
                                    bool withGroupDelay);

struct AnglePoint {
    double aoi_deg = 0;
    double Rs = 0, Rp = 0, Ts = 0, Tp = 0;
};

// 入射角掃引 (λ 固定)。stack が false を返す λ では空を返す。
std::vector<AnglePoint> angleSweep(const StackAtLambda &stack, double lambda_nm,
                                   double aoiMin_deg, double aoiMax_deg,
                                   int points);

// ── 設計評価 ────────────────────────────────────────────────────────────────
enum class Quantity { R, T };
enum class PolMode  { S, P, Average };   // Average = (s + p)/2 (無偏光)

// 設計ターゲット 1 行。goal / tol は 0..1 の比率 (% ではない)。
struct TargetBand {
    double   lam0_nm = 400.0, lam1_nm = 700.0;
    Quantity q       = Quantity::R;
    PolMode  pol     = PolMode::Average;
    double   goal    = 0.0;     // 目標値 (0..1)
    double   tol     = 0.005;   // 許容差 (> 0)
    double   weight  = 1.0;     // 重み (> 0)
    int      samples = 21;      // 帯域内のサンプル点数 (≥ 1)
};

struct MeritResult {
    bool   valid   = false;
    double merit   = 0.0;   // 出典 [3] の定義 (0 = 目標に完全一致)
    int    used    = 0;     // 評価に使えた λ 点数
    int    skipped = 0;     // 材料の有効範囲外などで除外した λ 点数
};

// メリット関数 (出典 [3]):
//   F = sqrt( (Σ w_i ((Q_i − goal_i)/tol_i)²) / (Σ w_i) )
// F = 1 は「平均して許容差ぶんだけ目標から外れている」ことを意味する。
MeritResult merit(const StackAtLambda &stack,
                  const std::vector<TargetBand> &targets, double aoi_deg);

struct SensitivityResult {
    bool valid = false;
    // 層ごとの |dQ̄/dd| [%/nm]。Q̄ はターゲット帯域上の対象量の平均。
    std::vector<double> dQ_pctPerNm;
    int  worst = -1;        // 最大感度の層 index (0 始まり)。無効なら −1
};

// 膜厚感度 (中心差分)。delta_nm は片側の摂動量 [nm] (> 0)。
SensitivityResult thicknessSensitivity(const StackAtLambda &stack,
                                       const std::vector<TargetBand> &targets,
                                       double aoi_deg, double delta_nm);

// ── 膜厚最適化 (Nelder–Mead シンプレックス) ─────────────────────────────────
// 層数・材料は固定し、**物理膜厚だけ**を動かしてメリット関数を最小化する。
// 手法は J. A. Nelder, R. Mead, Computer Journal 7, 308 (1965)。標準の
// 係数 (反射 1 / 拡大 2 / 縮小 0.5 / 収縮 0.5) を使う。乱数を使わないので
// 同じ入力からは常に同じ結果になる (selftest で決定性を検証)。
//
// ── 遺伝的アルゴリズム (GA) ────────────────────────────────────────────────
// シンプレックスは初期値の近くの谷しか降りられない (局所的)。膜厚の設計は
// 谷がいくつもあるので、**箱全体を探す**手法を選べるようにした。実体は
// `core/Optimizer` の実数値 GA (トーナメント選択 + SBX 交叉 + 多項式突然変異
// + エリート保存) をそのまま使う — このファイルに探索を書き直さない。
//
// 3 つの約束:
//   1. **初期集団の 1 個体目を初期膜厚にする**。エリート保存と合わせて、
//      結果が初期値より悪くなることが原理的に起こらない。
//   2. 乱数は seed からのみ決まるので、同じ入力・同じ seed なら同じ結果。
//   3. 収束判定は無い (世代数だけ回す)。`converged` は常に false で、
//      「収束した」とは言わない。
//
// needle / tunneling といった層数を変える手法は実装していない
// (層の挿入は材料選択と一体で、別の設計判断が要る)。
enum class OptimizeMethod {
    Simplex = 0,   // Nelder-Mead (決定的・局所)
    Genetic = 1    // 実数値 GA (core/Optimizer。seed で決定的・大域)
};

struct OptimizeOptions {
    OptimizeMethod method = OptimizeMethod::Simplex;
    int    maxIter     = 600;      // 反復上限 (シンプレックス)
    double tolMerit    = 1e-7;     // シンプレックスの merit 幅がこれ未満で収束
    double minThick_nm = 1.0;      // 膜厚の下限 (これ未満には縮めない)
    double maxThick_nm = 5000.0;   // 膜厚の上限
    double initStep    = 0.10;     // 初期シンプレックスの相対ステップ (10%)

    // GA。探索範囲は初期膜厚の ±gaRange 倍 (min/maxThick_nm でも切る)
    int      population  = 40;     // >= 2
    int      generations = 60;     // >= 1
    double   gaRange     = 0.5;    // 0.5 = 初期値の 0.5〜1.5 倍を探す
    uint64_t seed        = 20260813ull;
};

struct OptimizeResult {
    bool   valid = false;
    std::vector<double> d_nm;      // 最適化後の物理膜厚 (入力と同じ長さ)
    double meritStart = 0.0;
    double meritEnd   = 0.0;
    int    iterations = 0;
    bool   converged  = false;     // tolMerit に達したか (false = 反復上限)。
                                   // GA は収束判定を持たないので常に false
};

// d0_nm は初期膜厚 (入射側 → 基板側)。stack が返す層数と一致していること。
// stack が返す層の膜厚は最適化中に d で上書きされるので、呼び出し側は
// 屈折率だけ正しく詰めればよい。ターゲットが空 / 層数不一致 / 初期メリットが
// 評価できない場合は valid = false。
OptimizeResult optimizeThickness(const StackAtLambda &stack,
                                 const std::vector<TargetBand> &targets,
                                 double aoi_deg,
                                 const std::vector<double> &d0_nm,
                                 const OptimizeOptions &opt);

struct ToleranceOptions {
    int      trials     = 1000;
    double   sigmaRel   = 0.005;   // 膜厚のランダム誤差 1σ (相対値)
    bool     systematic = false;   // 全層共通のレートドリフト (同じ 1σ) を加える
    unsigned seed       = 20240801u;
};

struct ToleranceResult {
    bool   valid  = false;
    int    trials = 0, passed = 0;
    int    used   = 0, skipped = 0;
    double yield        = 0.0;   // passed / trials (0..1)
    double meritNominal = 0.0;   // 公称設計のメリット関数
    double meritMean    = 0.0;
    double meritP90     = 0.0;   // 90 パーセンタイル
};

// 製造誤差モンテカルロ。合格判定は「全ターゲット帯域の全サンプル点で
// |Q − goal| ≤ tol」。乱数は std::mt19937 + Box-Muller で、seed が同じなら
// 処理系に依らず同じ結果になる (selftest で決定性を検証)。
ToleranceResult monteCarlo(const StackAtLambda &stack,
                           const std::vector<TargetBand> &targets,
                           double aoi_deg, const ToleranceOptions &opt);

} // namespace optics
} // namespace ofd

#endif // OFD_OPTICS_THINFILMSTACK_H
