// Directivity.h — 遠方界の全球積分から指向性を出す (Qt 非依存 / C++17)。
//
// 切断面 (far1d) だけでは主ビーム幅やサイドローブ比までしか出せない。
// **指向性は全球にわたる積分**なので far2d の (θ, φ) 格子が要る:
//
//     D = 4π U_max / ∮ U dΩ,      Ω_A = ∮U dΩ / U_max,      D = 4π / Ω_A
//
// ── far2d.log の dB は「振幅の 20log10」である ────────────────────────────
// カーネル (`OpenFDTD/post/outputFar2d.c`) は `e[k] = 20*log10(e[k])` と書く。
// つまり E-abs[dB] は**電界振幅**の dB で、放射強度は U ∝ |E|² だから
//
//     U ∝ 10^(dB/20)² = 10^(dB/10)
//
// になる。**10 で割るのは「電力の dB だから」ではなく「振幅の dB を 2 乗した
// から」**である (同じ式でも理由が違う — far1d の dBsm と混同しないこと)。
//
// ── 正規化は落ちる ────────────────────────────────────────────────────────
// D は U の**定数倍に不変**である (分子と分母の両方に同じ係数が掛かる)。
// だから遠方界がどう正規化されていても指向性は取り出せる。逆に言えば
// **放射効率はここからは出ない** — 入力電力と放射電力の比なので、パターンの
// 形だけでは決まらない。呼び出し側でそう明示すること (絶対規則 5)。
//
// ── 積分のしかた ──────────────────────────────────────────────────────────
// θ ではなく **u = cosθ** で台形則を掛ける。dΩ = du dφ なので重み sinθ が
// 要らなくなり、**一様な U に対しては厳密に 4π** を返す (D = 1 が厳密に出る)。
// θ を等間隔に取った格子では u は非等間隔になるが、台形則は節点座標をそのまま
// 使うので問題ない。
//
// φ = 0 と φ = 360 は同じ方向で、カーネルは**両方**を書く。台形則は端点に
// 半分の重みを与えるので、この重複は**自動的に正しく 1 回分**になる。
#ifndef OFD_EM_DIRECTIVITY_H
#define OFD_EM_DIRECTIVITY_H

#include <vector>

namespace ofd {
namespace em {

// (θ, φ) 格子上の放射強度 U (線形、任意の定数倍で可)。
// theta_deg は 0..180 の昇順、phi_deg は 0..360 の昇順 (端点が重複してよい)。
// u[i * nPhi + j] が (theta_deg[i], phi_deg[j]) の値。
struct SphericalPattern {
    std::vector<double> theta_deg, phi_deg;
    std::vector<double> u;           // nTheta * nPhi、行優先 (行 = theta)
    int nTheta() const { return static_cast<int>(theta_deg.size()); }
    int nPhi()   const { return static_cast<int>(phi_deg.size()); }
    bool valid() const
    {
        return nTheta() >= 2 && nPhi() >= 2
            && u.size() == static_cast<std::size_t>(nTheta()) * nPhi();
    }
};

struct Directivity {
    bool   valid = false;
    double radiatedPower = 0.0;   // ∮U dΩ (U と同じ任意単位 × sr)
    double peak = 0.0;            // U_max
    double directivity = 0.0;     // D = 4π U_max / ∮U dΩ (真値)
    double directivityDbi = 0.0;  // 10log10(D)
    double beamSolidAngle = 0.0;  // Ω_A = ∮U dΩ / U_max [sr]
    double peakTheta_deg = 0.0, peakPhi_deg = 0.0;
};

// 全球積分。格子が不正なら valid = false (数字を作らない)。
Directivity directivity(const SphericalPattern &p);

// far2d.log の E-abs[dB] (= 20log10|E|) を放射強度 U へ。
// **10 で割る** — 振幅の dB を 2 乗するため。
double intensityFromEabsDb(double dB);

} // namespace em
} // namespace ofd

#endif // OFD_EM_DIRECTIVITY_H
