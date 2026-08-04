// FdeModeSolver.h — 導波路断面の有限差分固有モード (FDE) ソルバ
//
// 断面 (x,y) 上の 2D ヘルムホルツ固有値問題
//     [ ∂x(..∂x..) + ∂y(..∂y..) + k0² n(x,y)² ] E = β² E     (β = k0·neff)
// を有限差分で離散化し、支配モードから順に neff・閉込め係数・実効断面積を求める。
//
// Qt には依存しない (std::vector のみ)。selftest から直接呼べる。
// GUI 側 (src/tabs/ModeSolverTab.cpp) はこのヘッダのみを使う。
//
// 実装の詳細 (差分スキーム・反復法・実測精度) は FdeModeSolver.cpp 冒頭を参照。

#ifndef OFD_OPTICS_FDEMODESOLVER_H
#define OFD_OPTICS_FDEMODESOLVER_H

#include <vector>

namespace ofd {
namespace optics {

// 断面の屈折率分布 (row-major, ny 行 × nx 列。index = iy*nx + ix)
struct CrossSection {
    int    nx = 0, ny = 0;
    double dx_um = 0.0, dy_um = 0.0;   // 格子間隔 [um]
    std::vector<double> n;             // 実屈折率 (損失は扱わない)
    std::vector<char>   core;          // 1 = コア領域 (閉込め係数 Γ の積分域)
};

// 偏波近似。SemiVecTE は Ex 主成分 (縦界面での不連続を x 方向に扱う)、
// SemiVecTM は Ey 主成分 (横界面での不連続を y 方向に扱う)。
enum class Polarization { Scalar, SemiVecTE, SemiVecTM };

struct ModeResult {
    double neff = 0.0;
    double gamma = 0.0;                // コア閉込め係数 (0..1)
    double aeff_um2 = 0.0;             // 実効断面積
    bool   guided = false;             // neff > 最大クラッド屈折率
    std::vector<double> intensity;     // |E|^2 (ny*nx, 最大値 1 に正規化)
    // 符号付きの主電界成分 (ny*nx, 離散 L2 ノルム 1)。intensity は
    // これの 2 乗を最大 1 に正規化したもの。モード間の直交性検証や
    // モード波源への受け渡しに使う (表示だけなら intensity で足りる)。
    std::vector<double> field;
};

struct SolveOptions {
    Polarization pol = Polarization::Scalar;
    int    modes = 4;                  // 求めるモード数
    int    maxSteps = 4000;            // 1 モードあたりの上限反復
    double tol = 1e-9;                 // neff の相対収束判定
};

// λ [um] で解く。neff 降順に最大 opt.modes 個。
// 収束しなかったモードは返さない (存在しないモードを作らない)。
// 非導波 (neff ≤ 最大クラッド屈折率) に落ちた反復は Dirichlet 窓が作る箱モード
// であって導波路のモードではないため、そこで探索を打ち切る。したがって返る
// ModeResult は常に guided == true になる。
std::vector<ModeResult> solveModes(const CrossSection &cs, double lambda_um,
                                   const SolveOptions &opt);

// 矩形コアの断面を組み立てるヘルパ (GUI が使う)。
// 窓は自動 (コアの周囲に marginRatio 倍のクラッドを確保。ただしモードの裾が
// Dirichlet 壁に届かないよう片側 0.6um を下限とする)。格子間隔はコア寸法が
// 整数セルになるよう targetDx_um から丸めるので、実際の値は返り値の
// dx_um / dy_um を見ること (材料界面をセル境界に載せるために必要)。
// リブのスラブ厚も dy_um の整数倍へ丸める。寸法が不正なら nx = 0 を返す。
// 計算量は格子点数にほぼ比例する — 1550nm の Si 導波路なら 10〜20nm が実用域
// (10nm・450x220nm・4 モード要求で 0.6 秒程度。実測値は .cpp 冒頭)。
CrossSection makeRectangularCore(double coreW_um, double coreH_um,
                                 double slabH_um,   // 0 = ストリップ, >0 = リブ
                                 double nCore, double nClad, double nSub,
                                 double targetDx_um, double marginRatio = 1.6);

} // namespace optics
} // namespace ofd

#endif // OFD_OPTICS_FDEMODESOLVER_H
