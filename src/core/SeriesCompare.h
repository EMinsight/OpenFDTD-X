// SeriesCompare.h — 2 本の (x, y) 系列を共通の軸・共通の単位で突き合わせる
// (Qt 非依存 / C++17)。
//
// クロスバリデーション (別のソルバー・実測・文献値との比較) で必要になるのは、
// 「実行そのもの」ではなく **共通の観測量へ揃える部分**である。ソルバーごとに
//   * x 軸の刻みが違う (周波数点・角度点が一致しない)
//   * 単位が違う (線形 / dB、しかも電力の dB と振幅の dB がある)
//   * 範囲が違う (片方が広い / ずれている)
// ので、ここでその 3 つを揃えてから食い違いを数値にする。
//
// ── 電力の dB と振幅の dB を混ぜないこと ──────────────────────────────────
// 10log10 と 20log10 の取り違えは**値が 2 乗ずれる**ので、単位は
// `Scale` で明示的に受け取る。過去に far1d.log の dBsm を 20log10 で読んで
// 10 倍ずれかけた前例があるため、既定値を置かず必ず指定させる。
//
// ── 比較の指標 ────────────────────────────────────────────────────────────
//   maxAbs      最大絶対差          — 最悪値。1 点でも外れると効く
//   rms         二乗平均平方根差    — 全体的なずれ
//   bias        平均差 (b − a)      — 系統的なオフセット (校正ずれ)
//   relL2       ‖a−b‖₂ / ‖a‖₂      — 相対量。スケールに依らない
//   correlation ピアソン相関         — 形が合っているか (オフセットと倍率に不変)
//
// **bias と correlation を分けて見ること**が要点で、「形は完全に合っているが
// 一定量ずれている」(校正・正規化の違い) と「形そのものが違う」(物理が違う)
// を区別できる。
#pragma once
#include <vector>

namespace ofd {
namespace cmp {

// x は昇順であること (compare() は昇順を仮定して補間する)
struct Series {
    std::vector<double> x, y;
    bool valid() const { return x.size() == y.size() && x.size() >= 2; }
};

// 値の単位。dB は電力 (10log10) と振幅 (20log10) を必ず区別する。
enum class Scale { Linear = 0, PowerDb = 1, AmplitudeDb = 2 };

double toLinear(double v, Scale s);
double fromLinear(double v, Scale s);
// 系列ごと変換する (Linear → Linear は恒等)
Series convert(const Series &s, Scale from, Scale to);

// b を x 軸 xs へ線形補間で載せ替える。**外挿はしない** — b の範囲外の
// x は落とす (無い値を作らない)。返る系列の x は xs の部分列。
Series resampleTo(const Series &b, const std::vector<double> &xs);

struct Agreement {
    int    n = 0;              // 重なった範囲で比較できた点数
    double maxAbs = 0.0;
    double rms = 0.0;
    double bias = 0.0;         // 平均 (b − a)
    double relL2 = 0.0;        // ‖a−b‖₂ / ‖a‖₂ (a が全 0 なら 0)
    double correlation = 0.0;  // 分散が 0 のときは 0
    bool   valid = false;      // 重なりが 2 点未満なら false
};

// a の x 軸の上で比較する (b を補間して載せ替える)。
// **重なりが 2 点に満たなければ valid = false** を返す (数字を作らない)。
Agreement compare(const Series &a, const Series &b);

// a の x のうち b の範囲に入っている割合 (0..1)。重なりの少なさの警告用。
double overlapFraction(const Series &a, const Series &b);

} // namespace cmp
} // namespace ofd
