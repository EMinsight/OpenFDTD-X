// DistortionGrid.h — 歪曲格子 (理想格子と実際の像点の比較) — Qt 非依存。
//
// レンズエディタタブの「Distortion grid」。物体側に正方格子を置いたときに
// 像面へどう写るかを描く。
//
//   理想像高  y_ideal = f'·tanθ            (近軸のマッピング)
//   歪曲率    D(θ) = (y_real − y_ideal) / y_ideal × 100 [%]
//
// **系は光軸まわりに回転対称**なので、写像は「軸からの距離」だけの関数
// r' = m(r) になる。格子点の方位角は変わらず、半径だけが伸び縮みする。
// ここはその写像を**呼び手から関数で受け取る**ので、実光線追跡 (主光線) でも
// 解析的なマッピングでも同じ形で扱える (selftest は後者で厳密に検める)。
//
// ── 注意 ──────────────────────────────────────────────────────────────────
// 軸上 (θ = 0) は y_ideal = 0 なので割り算ができない。**歪曲は 0 と定義する**
// (実際、回転対称系では θ → 0 で D → 0)。0 除算を無限大にして絵を壊さない。
#ifndef OFD_OPTICS_DISTORTIONGRID_H
#define OFD_OPTICS_DISTORTIONGRID_H

#include <functional>
#include <vector>

namespace ofd {
namespace optics {

// 視野半角 [deg] → 実際の像高 [mm] (主光線の像面での軸からの距離)
using FieldMapping = std::function<double(double)>;

struct DistortionNode {
    double xIdeal_mm = 0.0, yIdeal_mm = 0.0;
    double xReal_mm = 0.0,  yReal_mm = 0.0;
    double field_deg = 0.0;      // その点の視野半角
    double percent = 0.0;        // 歪曲率 [%]
};

struct DistortionGridResult {
    std::vector<DistortionNode> nodes;   // n × n (行優先)
    int    n = 0;
    double maxPercent = 0.0;             // 絶対値が最大の歪曲率 (符号つき)
    double cornerPercent = 0.0;          // 隅 (最大視野) での歪曲率
    bool   valid() const { return n >= 2 && nodes.size() == std::size_t(n) * n; }
};

// 歪曲率 [%]。yIdeal が 0 なら 0 を返す (軸上)。
double distortionPercent(double yReal_mm, double yIdeal_mm);

// n × n の格子。格子は視野角の正方格子で、隅が最大視野 halfField_deg。
DistortionGridResult distortionGrid(const FieldMapping &realHeight,
                                    double efl_mm, double halfField_deg, int n);

} // namespace optics
} // namespace ofd

#endif // OFD_OPTICS_DISTORTIONGRID_H
