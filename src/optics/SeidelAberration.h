// SeidelAberration.h — 3 次収差 (ザイデル和) の計算 (Qt 非依存 / C++17)
//
// **3 次収差は実光線追跡を必要としない**。近軸の主光線 (chief) と縁光線
// (marginal) の 2 本を追跡すれば、球面収差・コマ・非点・像面湾曲・歪曲の
// 5 つの和 (S_I … S_V) は閉形式で決まる。ここではその古典的な定式化を
// そのまま実装する (W. T. Welford, "Aberrations of Optical Systems" §8 /
// R. Kingslake, "Lens Design Fundamentals" §10 と同じ式)。
//
//   A  = n(y·c + u)        縁光線の屈折不変量
//   Ā  = n(ȳ·c + ū)        主光線の屈折不変量
//   H  = n(ū·y − u·ȳ)      ラグランジュ不変量 (全面で一定)
//   Δ(u/n) = u'/n' − u/n,  Δ(1/n) = 1/n' − 1/n
//
//   S_I   = −A²·y·Δ(u/n)              球面収差
//   S_II  = −A·Ā·y·Δ(u/n)             コマ
//   S_III = −Ā²·y·Δ(u/n)              非点収差
//   S_IV  = −H²·c·Δ(1/n)              ペッツバール (像面湾曲)
//   S_V   = −(Ā/A)·(S_III + S_IV)     歪曲
//
// **S_V の教科書形は A を分母に持つ**ので、平行光が当たる平面のように
// A = 0 になる面で 0/0 になる。屈折不変量の恒等式を使って A を約分した
// 等価形 (.cpp 参照) を使うので、ここでは全ての面で値が出る。
//
// 物体は無限遠 (平行光入射) を前提とする。LensEditorTab の面テーブルが
// OBJ 行を無限遠として扱っているため、それに合わせている。
#ifndef OFD_OPTICS_SEIDELABERRATION_H
#define OFD_OPTICS_SEIDELABERRATION_H

#include <vector>

#include "ParaxialTrace.h"

namespace ofd {
namespace seidel {

// 1 面ぶんの寄与 (単位は長さ [mm] — 波面収差係数と同じ次元)
struct SurfaceTerms {
    double sI = 0.0, sII = 0.0, sIII = 0.0, sIV = 0.0, sV = 0.0;
};

struct Result {
    bool   valid = false;
    bool   hasField = false;      // 視野半角 > 0 (H ≠ 0) か
    double sI = 0.0, sII = 0.0, sIII = 0.0, sIV = 0.0, sV = 0.0;
    double lagrange = 0.0;        // ラグランジュ不変量 H
    double petzvalSum = 0.0;      // Σ c·Δ(1/n)  [1/mm]
    double petzvalRadius = 0.0;   // ペッツバール像面半径 [mm] (単レンズで −n·f)
    bool   hasPetzval = false;
    int    stopIndex = -1;        // 絞りに使った面 (指定が無ければ 0)
    std::vector<SurfaceTerms> perSurface;
};

// surfaces: 屈折面の並び (paraxial::Surface と同じもの)。
// epd: 入射瞳径 [mm] (<= 0 なら計算しない)、fieldHalf_deg: 視野半角 [deg]。
// 視野が 0 のときは H = 0 なので S_I 以外は 0 になる (hasField = false)。
Result analyze(const std::vector<paraxial::Surface> &surfaces,
               double epd, double fieldHalf_deg);

// 波面収差の峰値 [波長] (瞳端・視野端)。lambda_mm は設計波長 [mm]。
// W = S_I/8 + S_II/2 + S_III/2 + S_IV/4 + S_V/2 (Welford の正規化)
struct Waves {
    bool   valid = false;
    double spherical = 0.0;    // S_I / (8λ)
    double coma = 0.0;         // S_II / (2λ)
    double astigmatism = 0.0;  // S_III / (2λ)
    double fieldCurv = 0.0;    // S_IV / (4λ)
    double distortion = 0.0;   // S_V / (2λ)
};
Waves toWaves(const Result &r, double lambda_mm);

} // namespace seidel
} // namespace ofd

#endif // OFD_OPTICS_SEIDELABERRATION_H
