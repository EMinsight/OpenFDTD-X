// DispersionFit.h — 実 n,k データへの分散モデルフィットと診断 (Qt 非依存 / C++17)
//
// MaterialExplorerTab (マテリアルエクスプローラ) の「フィット実行」「RMS 誤差」
// 「モデル診断」の計算実体。参照データ (公刊 Sellmeier 係数から生成した n(λ)、
// または実測 n,k) に FDTD で使える極モデルを最小二乗で当て、**残差を実計算**
// する。GUI に式を直書きしないための分離で、tests/selftest.cpp から解析的に
// 既知のデータ (自分で作った Sellmeier) を与えて検証する。
//
// モデル (時間規約 exp(−iωt)、無損失極):
//   多極 / Lorentz : ε(λ) = ε∞ + Σ_p Δε_p·λ²/(λ² − λ_p²)
//                    (γ = 0 の Lorentz 振動子。Sellmeier 展開と同形。
//                     B. Tatian, Appl. Opt. 23, 4477 (1984) — Sellmeier 形の
//                     極は物理的な吸収共鳴に対応する)
//   Drude          : ε(λ) = ε∞ − (λ/λ_p)²  (= ε∞ − ω_p²/ω²、衝突項なし)
//   Sampled        : 補間 (フィットではない — 残差は定義上 0)
//
// 診断で使う判定基準 (すべて公刊のもの):
//   [1] 因果律 (Kramers-Kronig) の必要条件: 吸収の無い透明域では
//       dε/dω ≥ 0 (正常分散)。L. D. Landau, E. M. Lifshitz,
//       "Electrodynamics of Continuous Media", 2nd ed., §84 (1984);
//       J. D. Jackson, "Classical Electrodynamics", 3rd ed., §7.10。
//       **吸収域 (k > 0) ではこの必要条件は使えない** (KK 積分そのものが要る)
//       ので「評価対象外」を返す — 偽の合格を出さない。
//   [2] 受動性: 極モデルが受動媒質であるためには ε∞ ≥ 1 かつ振動子強度
//       Δε_p ≥ 0 (負の強度は利得媒質になる)。
//   [3] FDTD 安定性: 位相速度 c/n が c を超える (n < 1) 帯域があると
//       通常の Courant 条件 (真空基準) では不足する。
//       A. Taflove, S. C. Hagness, 3rd ed., §4.5 / §9 (分散媒質の ADE)。
#ifndef OFD_OPTICS_DISPERSIONFIT_H
#define OFD_OPTICS_DISPERSIONFIT_H

#include <vector>

namespace ofd {
namespace optics {

// 参照データ点。k < 0 は「その材料の k データが無い」ことを表す
// (0 を入れて「吸収ゼロを実測した」と誤解させないため)。
struct NkSample {
    double lambda_um = 0.0;
    double n = 0.0;
    double k = -1.0;
};

enum class FitModel {
    MultiPole = 0,   // 多極 (Multi-coefficient)
    Drude     = 1,
    Lorentz   = 2,   // 単極 Lorentz (無損失)
    Sampled   = 3    // 補間 (フィットしない)
};

enum class FitStatus {
    Ok = 0,
    NoData,          // 参照データが無い (実 n,k を持たない材料)
    TooFewPoints,    // 点数がパラメータ数に足りない
    Singular         // 正規方程式が解けない
};

struct FitOptions {
    FitModel model      = FitModel::MultiPole;
    int      maxPoles   = 6;      // 係数 (極) の最大数。1 以上
    double   rmsTol     = 0.1;    // 許容 RMS 誤差 (n)。到達したら極を増やさない
    int      iterations = 10;     // 極位置の改善反復 (座標降下)
};

struct FitReport {
    FitStatus status = FitStatus::NoData;
    FitModel  model  = FitModel::MultiPole;
    bool   interpolation = false;   // Sampled (フィットではない)
    int    points = 0;
    int    poles  = 0;

    double rmsN    = 0.0;           // n の RMS 残差
    double maxErrN = 0.0;           // n の最大絶対残差
    bool   hasK    = false;         // 参照データが k を持つか
    double rmsK    = 0.0;           // hasK のときのみ意味を持つ

    double epsInf = 0.0;
    std::vector<double> lambda0_um; // 極波長 λ_p
    std::vector<double> deltaEps;   // 振動子強度 Δε_p (Drude では未使用)

    // ── 診断 ──
    bool causalityEvaluable = false;  // [1] の必要条件を適用できたか
    int  causalityChecks    = 0;      // 判定した区間数
    int  causalityViolations = 0;     // dε/dω < 0 だった区間数
    bool passivityOk = false;         // [2]
    double nMin = 0.0, nMax = 0.0;    // フィットモデルの n の範囲 [3]
};

// フィット結果の n(λ)。status != Ok / 範囲外で ε ≤ 0 なら 0 を返す。
double modelIndex(const FitReport &r, double lambda_um);

// 参照データに分散モデルを当てる。samples は λ 昇順である必要はない
// (内部でソートする)。点数が足りなければ TooFewPoints。
FitReport fitDispersion(const std::vector<NkSample> &samples,
                        const FitOptions &opt);

} // namespace optics
} // namespace ofd

#endif // OFD_OPTICS_DISPERSIONFIT_H
