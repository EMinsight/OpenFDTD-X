// Mtf.h — レンズの MTF (変調伝達関数) — Qt 非依存 / C++17。
//
// レンズエディタタブの「MTF」。**2 つの MTF は別物**なので分けて出す:
//
//   回折限界 MTF   収差がゼロでも開口の有限さで決まる上限。円形瞳の
//                  自己相関 (2 つの円の重なり面積) の閉形式:
//                    MTF(ν) = (2/π)(φ − cos φ · sin φ),  φ = arccos(ν/νc)
//                    νc = 1/(λ·F#)        (カットオフ空間周波数)
//                  **近似ではなく厳密**。selftest は瞳の重なり面積を数値
//                  積分で独立に出して突き合わせている。
//
//   幾何 MTF       実光線追跡の像面交点の分布から出す。線像分布関数 (LSF) の
//                  フーリエ変換の絶対値だが、**ヒストグラムに刻む必要はない**:
//                    MTF(ν) = |(1/N) Σ_k exp(−i2πν x_k)|
//                  これは交点分布の特性関数そのもので、ビン幅による誤差が
//                  入らない。収差が大きいときの目安になる一方、**回折を
//                  含まないので高い空間周波数では実際より良く出る**
//                  (両方を並べて出し、そのことを画面に書く)。
//
// ── 出さないもの ──────────────────────────────────────────────────────────
// 波面 (OPD) から瞳関数を組んで自己相関する「回折 MTF (収差込み)」は出さない。
// 瞳での位相を面ごとの光路長から正しく組む必要があり、いまの追跡結果
// (`RayTrace::RayResult::opl`) だけでは基準球面の取り方が決まらない。
// 回折限界と幾何の 2 本に留め、**間を推測で埋めない**。
#ifndef OFD_OPTICS_MTF_H
#define OFD_OPTICS_MTF_H

#include <vector>

namespace ofd {
namespace optics {

// カットオフ空間周波数 νc = 1/(λ·F#) [cycles/mm]。λ は nm、F# は無次元。
double mtfCutoff_cyc_per_mm(double lambda_nm, double fNumber);

// 回折限界 MTF (円形瞳、収差なし)。ν ≥ νc では 0、ν = 0 で 1。
double diffractionLimitedMtf(double nu_cyc_per_mm, double lambda_nm,
                             double fNumber);

// 幾何 MTF — 像面交点の 1 軸座標 [mm] の並びから。
// **平行移動しても値は変わらない** (絶対値を取るため)。点が 1 点に集まって
// いれば全周波数で 1 (収差ゼロ = 幾何的には完全)。
double geometricMtf(const std::vector<double> &coord_mm, double nu_cyc_per_mm);

struct MtfCurve {
    std::vector<double> nu;          // [cycles/mm]
    std::vector<double> diffraction; // 回折限界
    std::vector<double> geometric;   // 幾何 (座標が空なら空)
    bool valid() const { return nu.size() >= 2; }
};

// 0 から νc まで points 点の曲線をまとめて作る。
MtfCurve mtfCurve(const std::vector<double> &coord_mm, double lambda_nm,
                  double fNumber, int points);

// MTF が値 target を下回る最初の周波数 (線形補間)。見つからなければ 0。
double frequencyAtMtf(const std::vector<double> &nu,
                      const std::vector<double> &mtf, double target);

} // namespace optics
} // namespace ofd

#endif // OFD_OPTICS_MTF_H
