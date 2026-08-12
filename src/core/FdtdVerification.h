// FdtdVerification.h — FDTD 解析設定の検証計算 (Qt 非依存 / C++17)
//
// VerificationTab (精度検証) が表示する数値のうち、**プロジェクト設定だけから
// 決まる量** をここで計算する。GUI に式を直書きしないための計算実体で、
// tests/selftest.cpp から直接呼んで解析解・極限値と突き合わせる。
//
// ここで計算するのは以下の 4 種類:
//   ① メッシュ解像度の計画値 (セル数 / λ/Δx / 推定メモリ) — 実データ
//   ② 吸収境界の **設計** 反射率 (連続体近似の理論値) — 実計算
//   ③ ソルバー実行ログ (ofd.log 等) の収束履歴の解析 — 実データ
//   ④ 安定条件・分解能・配置の自動診断 — 実判定
//
// ここに **実測値は無い**。②は離散化による数値反射を含まない設計値、
// ③は実行済みログがあるときだけ値を持つ (無ければ空)。
//
// 出典 (式はすべて公刊のもの):
//   [1] A. Taflove, S. C. Hagness, "Computational Electrodynamics: The
//       Finite-Difference Time-Domain Method", 3rd ed., Artech House (2005).
//       §4.5 数値安定条件 (Courant), §4.7 数値分散と 1 波長あたりのセル数,
//       §7.7 PML の多項式グレーディングと設計反射率 R(0)。
//   [2] J.-P. Berenger, "A perfectly matched layer for the absorption of
//       electromagnetic waves", J. Comput. Phys. 114, 185–200 (1994).
//       式 (25)(26): σ の多項式分布と、斜入射反射 R(θ) = R(0)^cosθ。
//   [3] G. Mur, "Absorbing boundary conditions for the finite-difference
//       approximation of the time-domain electromagnetic-field equations",
//       IEEE Trans. Electromagn. Compat. EMC-23, 377–382 (1981)。
//   [4] B. Engquist, A. Majda, "Absorbing boundary conditions for the
//       numerical simulation of waves", Math. Comp. 31, 629–651 (1977)。
//       1 次近似の残留反射係数 R(θ) = (1 − cosθ)/(1 + cosθ)。
//   [5] C. A. Balanis, "Antenna Theory: Analysis and Design", 4th ed.,
//       Wiley (2016), §2.2.4 — 反応性近傍界の境界 r = λ/2π。
#ifndef OFD_CORE_FDTDVERIFICATION_H
#define OFD_CORE_FDTDVERIFICATION_H

#include <cstddef>
#include <string>
#include <vector>

namespace ofd {
namespace verify {

// 判定。Unknown = 「判定に必要な情報が無い」(偽の OK を出さないための状態)
enum class Verdict { Unknown, Ok, Warn, Ng };

// ── ① メッシュ解像度の計画値 ───────────────────────────────────────────────

struct AxisGrid {
    double    dxMin_m = 0.0;   // この軸の最小セル幅 [m]
    double    dxMax_m = 0.0;   // この軸の最大セル幅 [m]
    long long cells   = 0;     // この軸の分割数
};

struct Grid { AxisGrid axis[3]; };

// 1 セルあたりの推定メモリ [byte]。E/H 6 成分 (double) + 材質 ID 等の補助配列。
// Project::estimatedMemoryMB() と同じ係数 (2 箇所で表示が食い違わないように)。
constexpr double kBytesPerCell = 60.0;

struct MeshLevel {
    double    refine       = 1.0;  // 現在のメッシュに対する「1 軸あたり分割数」倍率
    long long cells        = 0;    // 総セル数
    double    dxMax_m      = 0.0;  // 最大セル幅 [m] (最も粗い方向)
    double    lambdaOverDx = 0.0;  // λ/Δx_max。0 = 波長不明で未計算
    double    memoryMB     = 0.0;  // 推定メモリ [MB]
};

// 現在のメッシュを一様に refine 倍して得られる各解像度レベルの計画値。
// 分割数は各軸独立に round(cells*r) (最低 1)、セル幅はその逆比で縮む。
// lambda_m <= 0 なら lambdaOverDx は 0 (未計算) のままにする。
std::vector<MeshLevel> meshConvergenceLevels(const Grid &grid, double lambda_m,
                                             const std::vector<double> &refine);

// ── ② 吸収境界の設計反射率 (連続体近似の理論値) ─────────────────────────────

// PML の設計反射係数 (振幅比)。多項式グレーディングで σ_max を
//   σ_max = −(m+1)·ε0·c·ln R0 / (2d)
// と決めたときの斜入射反射は R(θ) = R0^cosθ となる ([1] §7.7, [2] 式(26))。
// r0 は .ofd の pml キー第 3 引数 (垂直入射の設計反射係数)。
// 適用範囲: 連続体 (無限に細かい格子) の理論値。離散化による数値反射
// (層数不足・急峻なグレーディング) は含まないので、実際の反射はこれより大きい。
double pmlDesignReflection(double r0, double thetaDeg);

// 1 次 Mur (Engquist–Majda 1 次片側波動方程式) の残留反射係数 (振幅比)
//   R(θ) = (1 − cosθ) / (1 + cosθ)                      ([3][4])
// 垂直入射で 0、斜入射で急増する。θ→90° で 1 (全反射)。
double murDesignReflection(double thetaDeg);

// 振幅比 → dB (20·log10)。ratio <= 0 は floorDb を返す (log(0) を出さない)。
double toDb(double amplitudeRatio, double floorDb = -300.0);

// ── ③ ソルバー実行ログの収束履歴 ───────────────────────────────────────────

// OpenFDTD 系ソルバーは反復ごとに "  <step>  <Eの平均>  <Hの平均>" を
// 標準出力 / *.log へ書く (sol/solve.c)。MainWindow が実行中に拾っている
// 行と同じもの。
struct ConvergencePoint {
    long long step = 0;
    double    e    = 0.0;   // 平均 |E| (相対)
    double    h    = 0.0;   // 平均 |H| (相対)
};

// ログ本文から収束履歴を抽出する。
// 判定条件: 空白区切りでちょうど 3 トークン、第 1 が非負整数、第 2・第 3 が
// 有限の実数、かつ step が直前より大きいこと (他の数表を誤って拾わない)。
// maxPoints を超えた分は捨てる (巨大ログでの GUI 停止を避ける)。
std::vector<ConvergencePoint> parseConvergenceLog(const std::string &text,
                                                  std::size_t maxPoints = 200000);

// 収束履歴の最終点が収束判定値 (.ofd の solver キー第 3 引数) を下回ったか。
// 履歴が空なら Unknown (未実行 — 「OK」を捏造しない)。
Verdict convergenceVerdict(const std::vector<ConvergencePoint> &history,
                           double threshold);

// ── ④ 自動診断 ─────────────────────────────────────────────────────────────

// Courant 数 S = c·Δt·sqrt(Σ_a 1/Δa²) ([1] 式 (4.60a))。
// 3 次元 Yee 格子の安定条件は S ≤ 1。Δt ≤ 0 (自動決定) や不正な格子では 0。
double courantNumber(double dt_s, double speed_mps, const double dxMin_m[3]);

// 目標解像度 λ/N からセル寸法を出す (Δx = c / (f·N))。
// **λ は媒質中ではなく指定した速度に対する波長**なので、真空の光速を渡せば
// 真空波長、音速を渡せば音の波長になる (呼び手が決める)。
// f ≤ 0 / N ≤ 0 なら 0。
double targetCellSize(double speed_mps, double freq_Hz, int lambdaDiv);

// 立方セル (Δx = Δy = Δz) の一様格子での Courant 限界 Δt。
//   dims = 3: Δx/(c√3)、2: Δx/(c√2)、1: Δx/c
// **courantNumber() の逆**で、この Δt を courantNumber へ入れると S = 1 に
// なる (selftest が往復で確認している)。不正な引数では 0。
double courantLimitDt(double speed_mps, double dx_m, int dims);

// S ≤ 0.99 → Ok、0.99 < S ≤ 1.0 → Warn (安定限界ぎりぎり)、
// S > 1.0 → Ng (発散)。S <= 0 は Unknown。
Verdict courantVerdict(double courant);

// 1 波長あたりのセル数 λ/Δx。[1] §4.7 は 10〜20 セル/λ を目安とする。
// ≥ 10 → Ok、6 以上 10 未満 → Warn、6 未満 → Ng。0 以下は Unknown。
Verdict resolutionVerdict(double lambdaOverDx);

// 吸収境界の設定。PML で層数 ≥ 8 → Ok、5〜7 → Warn、5 未満 → Ng。
// 1 次 Mur (pml=false) は斜入射で反射が大きいので Warn。
// 層数の目安は [1] §7.7 (標準的な問題で 5〜10 層、要求が厳しければ更に厚く)。
Verdict absorbingBoundaryVerdict(bool pml, int layers);

// 波源と観測点の距離 (波長単位) の判定。
// ≥ 1λ → Ok、≥ 1/(2π) λ (反応性近傍界の境界 [5]) → Warn、それ未満 → Ng。
// 0 以下は Unknown。
Verdict separationVerdict(double distanceOverLambda);

// 解析領域端までの余裕 (波長単位) の判定。
// 注意: これは規格値ではなく実務上の目安である。放射体を吸収境界から
// λ/4 以上離す運用に合わせて ≥ 0.25λ → Ok、≥ 0.125λ → Warn、
// それ未満 → Ng とする。0 未満 (領域外) は Ng。
Verdict marginVerdict(double marginOverLambda);

} // namespace verify
} // namespace ofd

#endif // OFD_CORE_FDTDVERIFICATION_H
