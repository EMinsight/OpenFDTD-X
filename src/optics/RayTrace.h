// RayTrace.h — 順次光学系の**実光線追跡** (厳密なスネルの法則)。
// Qt 非依存 / C++17。LensEditorTab のスポットダイアグラムと光線収差図
// (レイファン) の計算実体。
//
// 近軸追跡 (ParaxialTrace) と 3 次収差 (SeidelAberration) は近似で、前者は
// 角度を線形化し、後者はべき級数の 3 次で打ち切る。ここでは**近似せずに**
// 二次曲面との交点をを閉形式で解き、ベクトル形のスネルの法則で屈折させる。
// スポット径・光線収差図はこれが無いと出せない (3 次収差の外挿では、
// 高次収差が効く実際のレンズで平気で数倍ずれる)。
//
// 定式化 (G. H. Spencer & M. V. R. K. Murty, "General Ray-Tracing
// Procedure", J. Opt. Soc. Am. 52, 672 (1962) / W. T. Welford,
// "Aberrations of Optical Systems" §4 と同じ):
//
//   面 (曲率 c = 1/R, コーニック定数 k, 頂点が原点):
//       c(x² + y² + (1+k)z²) − 2z = 0
//   光線 P = P₀ + d·D (D は方向余弦, |D| = 1) を代入すると
//       C·d² − 2B·d + A = 0
//       A = c(x₀² + y₀² + (1+k)z₀²) − 2z₀
//       B = N₀ − c(x₀L + y₀M + (1+k)z₀N₀)
//       C = c(L² + M² + (1+k)N²)
//   を得る。**d = A/(B + √(B² − C·A))** の形で解くと c → 0 (平面) で
//   d = A/(2B) = −z₀/N へ連続に移る (もう一方の根は発散する)。
//
//   法線 (前方向き):  n̂ ∝ (−cx, −cy, 1 − c(1+k)z)
//   屈折 (μ = n/n'):  cosI = D·n̂,  cosI' = √(1 − μ²(1 − cos²I))
//                     D' = μD + (cosI' − μ·cosI)·n̂
//   根号の中が負 = 全反射 (追跡を打ち切る — 「それらしい点」を返さない)。
//
// **瞳の扱い (既知の近似)**: 入射瞳は絞りを近軸で物体空間へ結像した位置と
// 大きさを使い、そこへ向けて光線を放つ。実光線が実際の絞りを通る位置まで
// 反復して合わせる (real ray aiming) ことはしていないので、視野が大きく
// 瞳収差が強い系では瞳のサンプルがわずかに歪む。軸上の量 (球面収差・
// スポット径) はこの影響を受けない。
#ifndef OFD_OPTICS_RAYTRACE_H
#define OFD_OPTICS_RAYTRACE_H

#include <vector>

#include "ParaxialTrace.h"

namespace ofd {
namespace raytrace {

// 実光線追跡の面 (paraxial::Surface にコーニック定数を足したもの。
// 近軸側の構造体は 3 次収差計算と共有しているので、非球面を黙って
// 無視しないよう別の型にしてある)
struct Surface {
    double R = 0.0;         // 曲率半径 [mm] (0 = 平面)
    double thickness = 0.0; // 次の面までの距離 [mm]
    double nAfter = 1.0;    // 面の後ろ側の屈折率
    double semiD = 0.0;     // 有効半径 [mm] (0 = 制限なし)
    double conic = 0.0;     // コーニック定数 k (0 = 球面)
    bool   stop = false;    // 絞り面
};

// paraxial::Surface から変換する (コーニックは 0)
std::vector<Surface> fromParaxial(const std::vector<paraxial::Surface> &s);

// 光学系。物体は既定で無限遠 (objectDistance <= 0)。
struct System {
    std::vector<Surface> surfaces;
    double imageDistance = 0.0;   // 最終面 → 像面 [mm]
    double objectDistance = 0.0;  // 第 1 面 → 物体 [mm] (正の値 = 有限物体距離)
    double nObject = 1.0;         // 物体空間の屈折率
    double objectHeight = 0.0;    // 有限物体のときの物体高 [mm]
    bool   isValid() const;
};

enum class Status {
    Ok = 0,
    Missed,        // 面と交わらない (根号が負)
    Vignetted,     // 有効半径の外側を通った
    TotalReflect,  // 全反射
    Invalid,       // 系が不正
};

struct RayResult {
    Status status = Status::Invalid;
    double x = 0.0, y = 0.0;      // 像面での交点 [mm]
    double opl = 0.0;             // 光路長 Σ n·d [mm]
    double maxIncidenceDeg = 0.0; // 各面の入射角の最大値 [deg]
    int    failedSurface = -1;    // 失敗した面の添字 (成功なら −1)
    bool   ok() const { return status == Status::Ok; }
};

// 入射瞳 (絞りを物体空間へ近軸で結像したもの)
struct Pupil {
    bool   valid = false;
    double z = 0.0;        // 第 1 面頂点からの位置 [mm] (+ = 後ろ)
    double semiD = 0.0;    // 半径 [mm]
    int    stopIndex = 0;  // 使った絞り面
};
// epd <= 0 なら絞り面の有効半径を使う。絞り指定が無ければ第 1 面を絞りとみなす。
Pupil entrancePupil(const System &sys, double epd);

// 正規化瞳座標 (px, py)  (px² + py² <= 1) の光線を 1 本追跡する。
// field_deg は視野半角 [deg] (無限遠物体のときの入射角、y 方向)。
RayResult traceRay(const System &sys, double epd, double field_deg,
                   double px, double py);

// スポットダイアグラム (六方格子サンプリング — 中心 + 各リング 6i 本)。
// rings >= 1。像面での交点群と、重心まわりの RMS / 最大 (GEO) 半径を返す。
struct SpotResult {
    bool   valid = false;
    int    traced = 0, failed = 0;
    double centroidX = 0.0, centroidY = 0.0;
    double chiefX = 0.0, chiefY = 0.0;     // 主光線 (px = py = 0) の交点
    double rmsRadius = 0.0;                // 重心まわり [mm]
    double geoRadius = 0.0;                // 重心からの最大距離 [mm]
    std::vector<double> x, y;              // 交点 (プロット用)
};
SpotResult spotDiagram(const System &sys, double epd, double field_deg,
                       int rings);

// 光線収差図 (レイファン): 瞳座標に対する像面での横収差 (主光線基準)。
struct FanPoint {
    double pupil = 0.0;      // 正規化瞳座標 (−1 … +1)
    double dx = 0.0, dy = 0.0;  // 主光線からのずれ [mm]
    bool   ok = false;
};
struct FanResult {
    bool valid = false;
    std::vector<FanPoint> tangential;   // py を振る (dy を見る)
    std::vector<FanPoint> sagittal;     // px を振る (dx を見る)
};
// samples は片側の点数 (実際は 2·samples+1 点、中央を含む)。
FanResult rayFan(const System &sys, double epd, double field_deg, int samples);

} // namespace raytrace
} // namespace ofd

#endif // OFD_OPTICS_RAYTRACE_H
