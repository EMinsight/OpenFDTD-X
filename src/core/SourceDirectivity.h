// SourceDirectivity.h — 送波器の指向パターン (正規化ビームパターン) — Qt 非依存。
//
// 水中音響タブの「指向性」「ビーム幅」を、射出角の扇を切るだけでなく
// **BELLHOP の音源ビームパターンファイル (.sbp)** として渡すための元になる
// 部分。角度ごとの相対振幅 b(θ) を閉形式で作る。
//
// ── 変数は sinθ ───────────────────────────────────────────────────────────
// 開口の遠方界は開口面の空間フーリエ変換なので、**角度そのものではなく
// sinθ の関数**になる (θ はビーム軸からの角。BELLHOP では水平を 0° とする
// 俯角 SrcDeclAngle)。ここでも一貫して sinθ を使う。
// 正規化は b(0) = 1、b(±θ₃/2) = 1/√2 (θ₃ = 半値全幅)。
//
// ── 用意した 2 つの形 (どちらも厳密な閉形式) ────────────────────────────
//
//   Gaussian      b = exp(−(ln2/2)·t²),  t = sinθ / sin(θ₃/2)
//                 ガウス開口 (apodize した送波器) の遠方界。フーリエ変換が
//                 ガウスのままなので**これは近似ではなく厳密**。
//                 サイドローブを持たない。
//
//   LineAperture  b = |sin(x)/x|,  x = x₃·t,  x₃ = sin(x)/x = 1/√2 の最小正根
//                 一様励振の直線開口 (等間隔アレイの連続極限) の遠方界。
//                 [1] R. J. Urick, "Principles of Underwater Sound", 3rd ed.,
//                     McGraw-Hill (1983), Ch.3 — 直線開口の指向性関数。
//                 第 1 ヌルは x = π、第 1 サイドローブは tan x = x の根。
//
// **根は記憶した数値を書かず、その場で二分法で解く** (x₃ ≒ 1.3916、
// サイドローブの根 ≒ 4.4934)。定義式に戻せるものを定数表にしない。
//
// ── 扱わないもの (絶対規則 5) ──────────────────────────────────────────────
//   * **円形ピストン (Airy / jinc = 2J₁(u)/u) は入れていない。** 第 1 種
//     ベッセル関数 J₁ が要り、狭いビーム (θ₃ = 1°) では引数が u ≒ 185 まで
//     伸びるので、級数展開は桁落ちし、C++17 の `std::cyl_bessel_j` は MSVC に
//     無い。特殊関数を自前で持ち込むより、厳密に書ける 2 形に絞った。
//   * .sbp は **dB の表**なので**サイドローブの位相反転 (符号) は残らない**。
//     LineAperture の b は絶対値を返す。位相まで要る用途には使えない。
//   * 3 次元の指向性利得 (DI) は出さない。方位方向のパターンが決まらないと
//     立体角の積分ができないため。代わりに sinθ 方向の等価幅を出す。
#ifndef OFD_CORE_SOURCEDIRECTIVITY_H
#define OFD_CORE_SOURCEDIRECTIVITY_H

#include <vector>

namespace ofd {
namespace dir {

enum class Shape {
    Uniform = 0,        // 無指向 (b ≡ 1)
    Gaussian = 1,       // ガウス開口
    LineAperture = 2    // 一様励振の直線開口
};

// ── 定義根 (二分法で解く) ─────────────────────────────────────────────────
// sin(x)/x = 1/√2 の最小正根 (直線開口の半値点)
double sincHalfPowerRoot();
// tan(x) = x の π < x < 3π/2 の根 (第 1 サイドローブのピーク)
double sincFirstSidelobeRoot();

// ── パターン ──────────────────────────────────────────────────────────────
// w3_deg: 半値全幅 [deg] (0 < w3 < 180)。範囲外なら無指向として扱う。
// angle_deg: ビーム軸からの角 [deg]。
double amplitude(Shape s, double w3_deg, double angle_deg);
// 20·log10(b)。floorDb より下はクリップする (.sbp が −∞ を持てないため)。
double amplitudeDb(Shape s, double w3_deg, double angle_deg,
                   double floorDb = -60.0);

struct Pattern {
    std::vector<double> angle_deg;   // 単調増加
    std::vector<double> db;          // 20 log10 b (floorDb でクリップ)
    bool valid() const { return angle_deg.size() >= 2
                             && angle_deg.size() == db.size(); }
};

// [a0, a1] を n 点で等間隔に刻んだ表。n < 2 なら空を返す。
Pattern sample(Shape s, double w3_deg, double a0_deg, double a1_deg, int n,
               double floorDb = -60.0);

// 主ローブを分解するのに要る点数。BELLHOP は表の間を**振幅で線形補間**する
// ので、主ローブが数点しか無いと形が三角形に潰れる。刻みを θ₃/8 以下にする。
int recommendedPoints(double w3_deg, double span_deg);

// ── 解析量 (検証と画面表示に使う) ─────────────────────────────────────────
// 第 1 ヌルの sinθ。ヌルを持たない形、または可視域 (|sinθ| ≤ 1) に無ければ 0。
double firstNullSin(Shape s, double w3_deg);
// 第 1 サイドローブの高さ [dB]。持たない形は 0。
double firstSidelobeDb(Shape s);
// ∫ b²(sinθ) d(sinθ) の閉形式 (等価雑音帯域幅の sinθ 版)。
// Gaussian は sh·√(π/ln2)、LineAperture は sh·π/x₃、Uniform は可視域の幅 2。
// **Gaussian / LineAperture は積分範囲を無限にとった値**なので、可視域
// |sinθ| ≤ 1 を超える裾の分だけ過大になる。ガウスの裾は指数で落ちるので
// 実用上は差が出ないが、**直線開口の裾は 1/y² でしか落ちない**ので効く:
// 可視域だけで積分した値との相対差は ≒ sin(θ₃/2)/x₃² で、半値全幅 20° なら
// 約 4%、5° なら約 1% (幅にほぼ比例する)。selftest がこの関係を判定している。
double equivalentWidthSin(Shape s, double w3_deg);

} // namespace dir
} // namespace ofd

#endif // OFD_CORE_SOURCEDIRECTIVITY_H
