// EncircledEnergy.h — 包絡エネルギー (スポット内に入る光線の割合) — Qt 非依存。
//
// レンズエディタタブの「Encircled Energy」。実光線追跡の像面交点から、
// **中心から半径 r の円内に入る光線の割合**を出す。
//
//   EE(r) = (r 以内にある光線の本数) / (全本数)
//
// 光線 1 本を等しい重みとみなす (瞳を面積で等分に刻んで追跡しているので、
// 本数がそのままエネルギーの割合になる)。**ヒストグラムに刻まず、半径を
// 並べ替えて数える**ので、ビン幅による誤差が入らない。
//
// ── 中心の取り方 ──────────────────────────────────────────────────────────
// 重心 (centroid) と主光線 (chief ray) のどちらを中心にするかで値が変わる。
// 収差が大きいと両者は離れるので、**どちらで測ったかを必ず示す**こと。
// ここは中心を引数で受け取るだけにして、選択は呼び手に任せる。
//
// ── 出さないもの ──────────────────────────────────────────────────────────
// 回折の包絡エネルギー (エアリーパターン: 1 − J₀²(x) − J₁²(x)) は出さない。
// 第 1 種ベッセル関数が要り、MSVC に `std::cyl_bessel_j` が無い
// (`core/SourceDirectivity` で円形ピストンを見送ったのと同じ理由)。
// 幾何の包絡エネルギーだけを出し、回折を含まないことを画面に書く。
#ifndef OFD_OPTICS_ENCIRCLEDENERGY_H
#define OFD_OPTICS_ENCIRCLEDENERGY_H

#include <vector>

namespace ofd {
namespace optics {

struct EeCurve {
    std::vector<double> radius_mm;   // 単調増加
    std::vector<double> fraction;    // 0..1、単調非減少
    double rmsRadius_mm = 0.0;       // 中心まわりの RMS 半径
    double maxRadius_mm = 0.0;       // 最も外れた光線までの距離
    int    rays = 0;
    bool   valid() const { return radius_mm.size() >= 2; }
};

// 交点 (x, y) と中心から包絡エネルギー曲線を作る (points 点、0 → 最外まで)。
EeCurve encircledEnergy(const std::vector<double> &x_mm,
                        const std::vector<double> &y_mm,
                        double cx_mm, double cy_mm, int points);

// 割合 f (0..1) を包む半径。**実際に f 以上を包む最小の光線半径**を返す
// (補間しない — 光線は離散なので、間の値は「そこに光線が無い」だけ)。
double radiusForFraction(const std::vector<double> &x_mm,
                         const std::vector<double> &y_mm,
                         double cx_mm, double cy_mm, double f);

} // namespace optics
} // namespace ofd

#endif // OFD_OPTICS_ENCIRCLEDENERGY_H
