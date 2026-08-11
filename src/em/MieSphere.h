// MieSphere.h — 完全導体球の Mie 厳密解 (Qt 非依存 / C++17)
//
// 散乱/RCS タブが、カーネルの出した散乱断面積と**並べて比べる**ための理論値。
// 球は直交格子で階段近似されるので一致はしないが、桁と傾向が合っているかを
// その場で確かめられる (メッシュが粗すぎる・単位を取り違えた、が一目で分かる)。
//
// 級数 (Ruck et al., "Radar Cross Section Handbook", Plenum (1970), §3.2;
// Bohren & Huffman, "Absorption and Scattering of Light by Small Particles",
// Wiley (1983), §4.4):
//
//   x = ka = 2πa/λ
//   a_n = j_n(x) / h_n(x)
//   b_n = (x·j_{n-1}(x) − n·j_n(x)) / (x·h_{n-1}(x) − n·h_n(x))
//   h_n = j_n + i·y_n   (第 1 種ハンケル関数)
//
//   後方散乱  σ_b = (πa²/x²)·|Σ_{n≥1} (−1)ⁿ (2n+1)(a_n − b_n)|²
//   前方散乱  σ_f = (πa²/x²)·|Σ_{n≥1}     (2n+1)(a_n + b_n)|²
//
// 打ち切り次数は Wiscombe の目安 N = x + 4·x^(1/3) + 2 に余裕を足したもの。
//
// **検証の出所はコードの外にある**: Rayleigh 極限 σ_b/(πa²) → 9x⁴ (x → 0) と、
// 本家 OpenFDTD の検証スクリプト data/sample/sphere_rcs_check.sh が使う
// ka = 3.0 / a = 0.05 m の値 (後方 4.0901e-03 m² / 前方 8.4797e-02 m²)。
// selftest はこの両方に当てている。
#ifndef OFD_EM_MIESPHERE_H
#define OFD_EM_MIESPHERE_H

namespace ofd {
namespace em {

struct MieSphereRcs {
    double ka = 0.0;             // 電気サイズ x = 2πa/λ
    double backward_m2 = 0.0;    // 後方散乱 (モノスタティック RCS) [m²]
    double forward_m2 = 0.0;     // 前方散乱 [m²]
    int    terms = 0;            // 使った級数の項数
    bool   valid = false;        // 半径・周波数が正でなければ false
};

// 完全導体球の厳密解。半径 [m] と周波数 [Hz]。
// どちらかが非正なら valid = false を返す (0 を返して計算済みに見せない)。
MieSphereRcs pecSphereRcs(double radius_m, double freq_hz);

} // namespace em
} // namespace ofd

#endif // OFD_EM_MIESPHERE_H
