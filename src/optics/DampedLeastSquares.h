// DampedLeastSquares.h — 減衰最小二乗 (Levenberg–Marquardt) — Qt 非依存。
//
// レンズ設計の最適化は「残差ベクトルの二乗和を最小にする」問題なので、
// 減衰最小二乗が定番。**外部ライブラリは要らない** — ヤコビアンは差分で取り、
// 正規方程式を小さなガウス消去で解く (変数は普通 10 個程度)。
//
//   (JᵀJ + λ·diag(JᵀJ)) δ = −Jᵀr
//
// λ は「うまくいったら小さく (ガウス・ニュートン寄り)、悪化したら大きく
// (最急降下寄り)」と動かす。Marquardt の対角スケーリングを使うので、変数の
// 単位がばらばら (曲率 1/mm と厚み mm) でも破綻しにくい。
//
// ── 呼び手が決めること ────────────────────────────────────────────────────
// **残差の作り方 (何を目標にするか) は呼び手の仕事**。ここは「残差を返す関数」
// を受け取るだけで、レンズの知識は持たない。重み付けも呼び手が残差に織り込む
// (例: EFFL の誤差 [mm] とスポット RMS [mm] を同じ土俵に載せる係数)。
//
// ── 出さないもの ──────────────────────────────────────────────────────────
// 解析的な感度 (随伴法) は使わない。カーネルが感度を返さないので差分で取る。
// **差分の刻みは呼び手が指定する** (曲率と厚みでは適切な刻みが違うため)。
#ifndef OFD_OPTICS_DAMPEDLEASTSQUARES_H
#define OFD_OPTICS_DAMPEDLEASTSQUARES_H

#include <functional>
#include <string>
#include <vector>

namespace ofd {
namespace optics {

// 変数 x から残差 r を作る。作れない (追跡失敗など) なら false。
using ResidualFn = std::function<bool(const std::vector<double> &x,
                                      std::vector<double> &r)>;

struct DlsOptions {
    int    maxIterations = 60;
    double lambda0 = 1.0e-3;      // 減衰の初期値
    double tolerance = 1.0e-12;   // RMS 残差の改善がこれ未満なら収束
    std::vector<double> step;     // 差分の刻み (変数ごと。空なら 1e-6 の相対)
    std::vector<double> lower, upper;   // 箱制約 (空なら無制限)
};

struct DlsResult {
    bool   ok = false;
    std::vector<double> x;        // 最良の変数
    double rms = 0.0;             // 最良の RMS 残差
    double rms0 = 0.0;            // 初期の RMS 残差
    int    iterations = 0;
    int    evaluations = 0;       // 残差関数を呼んだ回数
    std::string note;             // 収束した理由 / 失敗した理由
};

DlsResult solve(const ResidualFn &f, const std::vector<double> &x0,
                const DlsOptions &opt = DlsOptions());

// 小さな連立一次方程式 A x = b (部分ピボットのガウス消去)。
// 特異なら false。**最適化の中だけでなく検証からも直接呼べるように公開する**。
bool solveLinear(std::vector<std::vector<double>> A, std::vector<double> b,
                 std::vector<double> *x);

} // namespace optics
} // namespace ofd

#endif // OFD_OPTICS_DAMPEDLEASTSQUARES_H
