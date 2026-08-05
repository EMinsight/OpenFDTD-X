// Colorimetry.h — CIE 表色系の測色計算 (照明光学タブの測色表で使う).
//
// Qt 非依存の純粋計算 (core 相当)。等色関数の数表を持たずに済むよう、
// CIE 1931 2° 等色関数は下記の解析近似 (多ローブ区分ガウシアン) を使う:
//
//   Chris Wyman, Peter-Pike Sloan, Peter Shirley,
//   "Simple Analytic Approximations to the CIE XYZ Color Matching Functions",
//   Journal of Computer Graphics Techniques (JCGT) 2(2), 1-11 (2013).
//
// 光源スペクトルは
//   - ガウシアンローブの重ね合わせ (青 LED + 蛍光体 / RGB 3 チップ / 単色)
//   - プランク黒体放射 (フルスペクトル光源の近似)
// で解析的に定義する。いずれも利用者が指定したパラメータから作られるので、
// 出てくる色度・相関色温度は「計算結果」であってサンプル値ではない。
//
// 未実装 (数表が要るもの):
//   - 演色評価数 Ra (CIE 13.3 の試験色 R1..R8 の分光反射率表が必要)
//   - TM-30 Rf/Rg (IES TM-30 の 99 試験色)
#pragma once
#include <functional>
#include <vector>

namespace ofd {
namespace colorimetry {

// 積分範囲 (nm) と刻み。CIE の推奨 (360-830nm) のうち寄与のある範囲。
constexpr double kLambdaMin = 360.0;
constexpr double kLambdaMax = 830.0;
constexpr double kLambdaStep = 1.0;

// ── CIE 1931 2° 等色関数 (Wyman et al. 2013 の多ローブ近似) ─────────────────
double cieXbar(double lambda_nm);
double cieYbar(double lambda_nm);   // = 標準比視感度 V(λ)
double cieZbar(double lambda_nm);

// ── 分光分布モデル ──────────────────────────────────────────────────────────
// ガウシアンローブ 1 個 (LED の発光ピーク / 蛍光体の broad band を表す)
struct GaussLobe {
    double peak_nm = 550.0;   // ピーク波長
    double fwhm_nm = 30.0;    // 半値全幅
    // ピーク値の相対強度 (>= 0)。放射束比は weight × fwhm に比例する。
    double weight  = 1.0;
};

// ローブの重ね合わせ S(λ) [相対単位]
double lobeSpectrum(const std::vector<GaussLobe> &lobes, double lambda_nm);

// プランクの放射則 (相対値。定数倍は色度に影響しない)
double planckSpectrum(double lambda_nm, double temperature_K);

// ── 測色量 ──────────────────────────────────────────────────────────────────
struct XYZ { double X = 0, Y = 0, Z = 0; };

// 分光分布 S(λ) → 三刺激値 (kLambdaMin..kLambdaMax を kLambdaStep で数値積分)
XYZ integrate(const std::function<double(double)> &spd);

struct Chromaticity {
    bool   valid = false;
    double x = 0, y = 0;      // CIE 1931 xy
    double u1960 = 0, v1960 = 0;   // CIE 1960 UCS (CCT の計算に使う)
    double up = 0, vp = 0;    // CIE 1976 u', v'  (u' = u, v' = 1.5 v)
};
Chromaticity chromaticity(const XYZ &c);

// 相関色温度 (CCT) と Duv。
// CIE 1960 UCS 上で黒体軌跡までの距離を最小化して求める (Judd の定義そのもの)。
// 探索範囲 1000..25000 K。範囲外/軌跡から遠すぎる (|Duv| > 0.05) 場合は
// valid=false (CCT は定義されない — 「—」表示にする)。
struct CctResult {
    bool   valid = false;
    double cct_K = 0;
    double duv = 0;           // + が黒体軌跡の上側 (緑寄り)
};
CctResult correlatedColorTemperature(const Chromaticity &c);

// 放射束あたりの光束 (放射発光効率) K = 683 ∫S·V / ∫S  [lm/W]
double luminousEfficacyOfRadiation(const std::function<double(double)> &spd);

// 分光分布のピーク波長 [nm] (積分範囲内での最大値の位置)
double peakWavelength(const std::function<double(double)> &spd);

// 2 つの色度間の CIE 1976 色差 Δu'v'
double deltaUV(const Chromaticity &a, const Chromaticity &b);

} // namespace colorimetry
} // namespace ofd
