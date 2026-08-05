// BendWaveguide.h — 曲がり導波路の等価直線化と接続損 (Qt 非依存 / C++17)
//
// ModeSolverTab の「曲げ損失」節の計算実体。断面 FDE ソルバ
// (src/optics/FdeModeSolver) と組み合わせて使う。
//
// 共形変換 (conformal transformation):
//   曲率半径 R の曲がり導波路の伝搬は、屈折率を
//       n_eq(x) = n(x)·(1 + x/R)        (|x| ≪ R)
//   と傾けた**等価直線導波路**の伝搬と等価である。x はコアの中心を原点とし、
//   曲率中心と反対側 (外周側) を正に取る。
//     M. Heiblum, J. H. Harris, "Analysis of curved optical waveguides by
//     conformal transformation", IEEE J. Quantum Electron. 11, 75-83 (1975).
//     (厳密には n·exp(x/R)。ここでは同論文の |x| ≪ R での 1 次近似を使う)
//
// この等価断面を FDE で解くと**曲げモードの形**が得られる。直線モードとの
// 重なり積分から直線⇄曲げ接続の**モード不整合損**が求まる:
//     η = |∫ E_a·E_b dA|² / (∫|E_a|²dA · ∫|E_b|²dA),  損失 = −10·log10(η)
//     D. Marcuse, "Loss analysis of single-mode fiber splices",
//     Bell Syst. Tech. J. 56, 703-718 (1977) — 重なり積分による結合効率。
//
// **求まらないもの (GUI に明示する)**: 放射損失そのもの。等価屈折率は外周側で
// 単調に増えるため曲げモードは本質的に漏れモード (複素 neff) であり、
// 実対称・Dirichlet 窓の FDE では虚部が出ない。放射が始まる半径方向の位置
// (カウスティック) だけは下の radiationCaustic で求まる。
#ifndef OFD_OPTICS_BENDWAVEGUIDE_H
#define OFD_OPTICS_BENDWAVEGUIDE_H

#include <vector>

#include "optics/FdeModeSolver.h"

namespace ofd {
namespace optics {

// 共形変換による等価直線断面。x はセル中心座標 (コア中心が原点):
//   x_i = (ix + 0.5 − nx/2)·dx        — makeRectangularCore と同じ取り方
// radius_um ≤ 0 のときは入力をそのまま返す (直線)。
// n_eq が負になる内周側 (x < −R) は 0 でクランプせず、そのセルの屈折率を
// 1e-6 まで下げて評価不能にする — 実際には |x| ≪ R の範囲でのみ使うこと。
CrossSection bendEquivalent(const CrossSection &cs, double radius_um);

// 変換の妥当性指標: 窓の端での |x|/R。0.1 を超えると 1 次近似の誤差が効く。
double conformalRatio(const CrossSection &cs, double radius_um);

// 2 つの実場の重なり効率 η (0..1)。長さが違う / ゼロノルムなら 0。
double overlapEfficiency(const std::vector<double> &a,
                         const std::vector<double> &b);

// 接続損 [dB] = −10·log10(η)。η ≤ 0 なら 300 dB (発散を切る)。
double mismatchLossDb(double efficiency);

// 放射カウスティックの半径方向位置 [µm]:
//   等価屈折率 n_clad·(1 + x/R) が neff に達する x_c = R·(neff/n_clad − 1)。
// これより外側では場は振動解 (放射) になる。neff ≤ n_clad / R ≤ 0 なら 0。
double radiationCaustic(double radius_um, double neff, double nClad);

} // namespace optics
} // namespace ofd

#endif // OFD_OPTICS_BENDWAVEGUIDE_H
