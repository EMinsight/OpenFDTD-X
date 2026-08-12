// DensityField.h — トポロジー最適化の密度場パラメータ化 (密度場 ⇄ 材料分布)。
//
// 逆設計 (トポロジー最適化) は、設計領域を画素に切って各画素の「密度」
// ρ ∈ [0,1] を設計変数にする。ρ = 0 が背景材料、ρ = 1 が構造材料で、
// 途中の値は誘電率の補間で表す。生の密度場をそのまま形にすると 1 画素の
// 市松模様が出てしまうので、実際には
//
//     ρ  --(密度フィルタ 半径 R)-->  ρ̃  --(Heaviside 射影 β,η)-->  ρ̄
//
// の順に通す。フィルタが**長さの尺度**を与え (公称の最小形状寸法 2R)、
// 射影が中間値を 0/1 へ寄せる。ここにあるのはこの写像そのものと、その結果を
// **軸平行の直方体 (本家の shape=1)** へ落とす分解である。
//
// **2R は保証ではなく公称値である。** フィルタは対称なので、閾値 η の
// まわりで釣り合った模様 (市松模様が典型) は、半径をいくら大きくしても
// **コントラストが縮むだけで閾値をまたがない** — 閾値化するとそのまま
// 市松模様に戻る (selftest で固定してある実測)。だから
// `minRunLength()` で**実際の連なりを測る**手段を別に用意してある。
//
// このファイルは最適化ループを持たない。感度 (∂FoM/∂ρ) はカーネルが
// 返さないため随伴法が組めず、画素数ぶんの設計変数を PSO / GA で回すのは
// 現実的でないので、**トポロジー最適化の反復そのものは未実装**である
// (`core/Optimizer` の PSO / GA は数十変数までを想定している)。
// 実装してあるのはパラメータ化と、既存形状 ⇄ 密度場の相互変換。
//
// 単位は SI (m)。GUI 側 (μm / nm 表示) で換算する。
#pragma once
#include <vector>

#include "Geometry.h"

namespace ofd {
namespace topo {

// 設計領域 (x/y は矩形、z は板厚)
struct Region {
    double x0_m = 0.0, x1_m = 5e-6;
    double y0_m = 0.0, y1_m = 5e-6;
    double z0_m = 0.0, z1_m = 220e-9;

    double width_m()  const { return x1_m - x0_m; }
    double depth_m()  const { return y1_m - y0_m; }
    double height_m() const { return z1_m - z0_m; }
    bool   valid()    const { return width_m() > 0 && depth_m() > 0 && height_m() > 0; }
};

// 画素格子。設計領域を等分するので、画素ピッチは指定解像度**以下**になる
// (切り上げ: 端数の画素を作らない)。
struct Grid {
    int    nx = 0, ny = 0;
    double pitchX_m = 0.0, pitchY_m = 0.0;

    int  count() const { return nx * ny; }
    bool valid() const { return nx > 0 && ny > 0; }
    // 画素 (i,j) の中心座標
    double cx_m(const Region &r, int i) const { return r.x0_m + (i + 0.5) * pitchX_m; }
    double cy_m(const Region &r, int j) const { return r.y0_m + (j + 0.5) * pitchY_m; }
};

// 解像度 [m/pixel] から格子を作る。resolution <= 0 や領域が空なら無効な格子。
Grid gridFor(const Region &r, double resolution_m);

// ── 密度フィルタ ────────────────────────────────────────────────────────
// Bruns & Tortorelli の円錐重み w(d) = max(0, R − d) を行和 1 に正規化した
// 線形作用素。行和が 1 なので
//   * 定数場は厳密に不変 (境界でも欠けない — 分母が近傍だけの和になる)
//   * 出力は入力の最小値と最大値の間に必ず入る (最大値原理)
// R が画素ピッチより小さいと自分自身しか重みを持たないので恒等写像になる。
std::vector<double> filter(const std::vector<double> &rho, const Grid &g,
                           double radius_m);

// ── 平滑化 Heaviside 射影 (Wang, Lazarov & Sigmund 2011) ────────────────
//   ρ̄ = (tanh(βη) + tanh(β(ρ−η))) / (tanh(βη) + tanh(β(1−η)))
// β → 0 で恒等写像、β → ∞ で閾値 η の階段関数。ρ = 0 と ρ = 1 は β・η に
// 依らず厳密に 0 と 1 のまま (体積の端点がずれない)。
double project(double rho, double beta, double eta);
std::vector<double> project(const std::vector<double> &rho, double beta, double eta);

// ── 指標 ────────────────────────────────────────────────────────────────
double volumeFraction(const std::vector<double> &rho);
// Sigmund の非離散度 M_nd = Σ 4ρ(1−ρ) / N。二値なら 0、全部 0.5 なら 1。
double nonDiscreteness(const std::vector<double> &rho);

// SIMP 補間 eps(ρ) = eps1 + ρ^p (eps2 − eps1)。光では p = 1 (線形) が既定。
double epsFromDensity(double rho, double eps1, double eps2, double p = 1.0);

// フィルタの公称最小形状寸法 (円錐フィルタの直径)。**保証ではない** —
// 実際の寸法は `minRunLength()` で測る (ヘッダ冒頭の注意を参照)。
inline double minFeature_m(double radius_m) { return 2.0 * radius_m; }

// ── 対称性の拘束 ────────────────────────────────────────────────────────
// 設計に対称性を課すのは、密度場を**対称な成分へ射影する** (鏡像との平均を
// 取る) だけでよい。平均なので
//   * 結果は厳密に対称
//   * 2 回かけても 1 回と同じ (冪等)
//   * 充填率 (平均) は厳密に変わらない
// が成り立つ。Rot90 は正方格子 (nx == ny) でしか定義できないので、
// そうでなければ**何もせずに入力を返す** (黙って別のものにしない)。
enum class Symmetry { None = 0, MirrorX = 1, MirrorY = 2, Quadrant = 3, Rot90 = 4 };

std::vector<double> symmetrize(const std::vector<double> &rho, const Grid &g,
                               Symmetry sym);
bool isSymmetric(const std::vector<double> &rho, const Grid &g, Symmetry sym,
                 double tol = 1e-12);
// Rot90 が使えるか (正方格子か)
inline bool symmetryApplicable(const Grid &g, Symmetry sym)
{ return (sym != Symmetry::Rot90) || (g.nx == g.ny); }

// ── 最小形状寸法の実測 (製造ルールの判定) ───────────────────────────────
// フィルタ半径が与えるのは「そうなるはず」の寸法で、射影と閾値を通した後の
// 形が本当にその寸法を満たすかは別問題。閾値化した場について、行方向・列方向
// の**連なりの最短長 (画素数)** を実際に数える。
//   * 材料側 (on) と背景側 (off) を別に返す — 細い線と細い隙間は別の規則。
//   * 端で切れている連なりは数えない (領域の外がどうなっているか分からない)。
//   * 連なりが 1 本も無ければ 0。
struct RunLengths { int minOn = 0, minOff = 0; };
RunLengths minRunLength(const std::vector<double> &rho, const Grid &g,
                        double threshold = 0.5);

// ── 密度場 → 材料分布 ───────────────────────────────────────────────────
// 閾値 threshold 以上の画素を、重なりの無い軸平行矩形へ貪欲に分解する。
// [i0,i1) × [j0,j1) の半開区間。矩形の総面積は閾値以上の画素数に厳密に
// 一致し、矩形どうしは重ならない。
struct Rect { int i0 = 0, j0 = 0, i1 = 0, j1 = 0; };
std::vector<Rect> rectangles(const std::vector<double> &rho, const Grid &g,
                             double threshold = 0.5);

// ── 材料分布 ⇄ 形状ユニット ─────────────────────────────────────────────
// 既存形状を設計領域の密度場へ写す (画素中心が材料の内側なら 1)。
// 判定できるのは直方体 (1)・楕円体 (2)・円柱 (11/12/13) のみ。三角柱・
// 角錐台・円錐台は内外判定を持たないので **数えて飛ばす** (skipped)。
// 材料 0 (空気) のユニットは背景なので数えない。
std::vector<double> rasterize(const Geometry *units, int count,
                              const Region &r, const Grid &g,
                              int *skipped = nullptr);

// 矩形を本家 shape=1 (直方体) のユニットへ落とす。z は設計領域の板厚。
std::vector<Geometry> toGeometry(const std::vector<Rect> &rects,
                                 const Region &r, const Grid &g,
                                 int materialId);

} // namespace topo
} // namespace ofd
