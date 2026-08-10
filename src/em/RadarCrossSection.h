// RadarCrossSection.h — レーダ断面積 (RCS) の単位換算 (Qt 非依存 / C++17)
//
// カーネル (`sol/outputChars.c` → `outputCross.c`) は平面波入射の問題について
// `<kernel>.log` へ **m² の実値**で後方 / 前方散乱断面積を書く。散乱/RCS タブは
// それを読んで表示するが、RCS は分野ごとに違う単位で語られるので換算が要る:
//
//   σ [m²]        そのまま (カーネルの出力単位)
//   σ [dBsm]      = 10·log10(σ / 1 m²)      … レーダ工学の標準表記
//   σ / λ²        波長で正規化 (電気サイズ同士の比較に使う)
//   σ / (πa²)     球の幾何断面積で正規化    … 教科書の Mie 曲線の縦軸
//
// **dB は 10log10 であって 20log10 ではない** (σ は電力次元の量)。ここを
// 取り違えると 2 倍ずれるので selftest で名指しで固定してある。
//
// 完全導体球の光学極限 (ka ≫ 1) では後方散乱 σ → πa² に漸近する。これが
// σ/(πa²) を使う理由で、検証の当たりを付けるのにも使える。
#ifndef OFD_EM_RADARCROSSSECTION_H
#define OFD_EM_RADARCROSSSECTION_H

namespace ofd {
namespace em {

// σ [m²] → dBsm。σ ≤ 0 は −∞ (丸めて有限値にしない)。
double rcsDbsm(double sigma_m2);
// dBsm → σ [m²]
double rcsFromDbsm(double dbsm);

// σ / λ²。f ≤ 0 なら 0。
double rcsPerWavelengthSq(double sigma_m2, double freqHz);

// 半径 a の球の幾何断面積 πa² [m²]。a ≤ 0 なら 0。
double sphereGeometricArea(double radius_m);
// σ / (πa²)。a ≤ 0 なら 0。
double rcsPerGeometric(double sigma_m2, double radius_m);

// 電気サイズ ka = 2πa/λ。a ≤ 0 または f ≤ 0 なら 0。
double sphereKa(double radius_m, double freqHz);

} // namespace em
} // namespace ofd

#endif // OFD_EM_RADARCROSSSECTION_H
