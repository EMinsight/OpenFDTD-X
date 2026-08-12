// TransmissionLine.h — 伝送線路の準 TEM 解析 (閉形式)。
// Qt 非依存 / C++17。TransmissionLineTab の Z₀ / γ / S パラメータの計算実体。
//
// ── なぜ閉形式なのか ───────────────────────────────────────────────────────
//
// このタブは断面形状が標準的な線路 (マイクロストリップ・ストリップライン・
// 同軸・平行 2 線・コプレーナ) を扱う。これらの Z₀ と実効誘電率には
// **公刊された閉形式**があり、任意断面を数値で解く (OpenFEM / OpenPEEC) の
// は「標準形以外」を扱うときの話になる。標準形をカーネル起動で解くのは
// 過剰で、しかも遅い。
//
// ── 使っている式と、その素性 ───────────────────────────────────────────────
//
// **同軸** (厳密):      Z₀ = (η₀/2π)·ln(b/a)/√εr
// **平行 2 線** (厳密): Z₀ = (η₀/π)·arccosh(D/d)/√εr
// **ストリップライン** (厚みゼロで厳密 — Cohn 1954 の等角写像):
//                       Z₀ = (η₀/4)·K(k')/K(k)/√εr,  k = sech(πW/2b)
// **コプレーナ (CPW)** (厚みゼロ・基板無限厚で厳密):
//                       Z₀ = (30π)·K(k')/K(k)/√ε_eff,  k = S/(S+2W)
//                       ε_eff = (εr+1)/2
// **マイクロストリップ** (Hammerstad & Jensen 1980 の近似式。**厳密解は無い** —
//   準 TEM の数値解に対して 1% 級で合う経験式):
//       ε_eff = (εr+1)/2 + (εr−1)/2·(1 + 10/u)^(−a(u)·b(εr)),  u = W/h
//       Z₀    = (η₀/2π)·ln[F(u)/u + √(1+(2/u)²)] / √ε_eff
//
// 完全楕円積分の比 K(k)/K(k') は AGM (算術幾何平均) で求める — **級数の
// 打ち切りではないので桁落ちが無く**、k → 0 / 1 の端でも安定する。
//
// ── 損失 ───────────────────────────────────────────────────────────────────
//
// 誘電損 (準 TEM の一般形。均質線路では ε_eff = εr で α_d = β·tanδ/2 に厳密に
// 戻る — selftest で確かめている):
//       α_d = (π/λ₀)·(εr/√ε_eff)·((ε_eff−1)/(εr−1))·tanδ   [Np/m]
// 導体損は**同軸だけ厳密** (α_c = Rs/(2π Z₀)·(1/a+1/b))。他は表皮抵抗を
// 導体幅で割る広線路近似で、**細い線路では過小評価**になる (注記あり)。
//
// ── 扱わないもの (絶対規則 5) ──────────────────────────────────────────────
//
// - 導体厚 t の補正 (すべて厚みゼロとして扱う)。
// - 高次モード・導波管モード (準 TEM の範囲のみ)。
// - 不連続部 (ベンド・ステップ・クロストーク・アイダイアグラム)。
// - 任意断面 — そこは数値解 (OpenFEM / OpenPEEC) の領分。
#ifndef OFD_CORE_TRANSMISSIONLINE_H
#define OFD_CORE_TRANSMISSIONLINE_H

#include <complex>

namespace ofd {
namespace tline {

enum class Kind {
    Microstrip = 0,   // W (線路幅), H (基板厚)
    Stripline  = 1,   // W (線路幅), B (地板間隔)
    Coax       = 2,   // A (内導体半径), B (外導体内半径)
    TwoWire    = 3,   // D (中心間隔), Dia (線径)
    Coplanar   = 4,   // S (中心導体幅), W (スロット幅)
};

// 断面の寸法はすべて mm。種別ごとに使うメンバが違う (上の対応表)。
struct Line {
    Kind   kind = Kind::Microstrip;
    double w_mm = 3.0;      // 線路幅 / CPW の中心導体幅
    double h_mm = 1.6;      // 基板厚 / ストリップラインの地板間隔 B
    double a_mm = 0.5;      // 同軸の内導体半径
    double b_mm = 1.68;     // 同軸の外導体内半径
    double d_mm = 3.0;      // 平行 2 線の中心間隔
    double dia_mm = 1.0;    // 平行 2 線の線径
    double slot_mm = 0.3;   // CPW のスロット幅

    double epsr = 4.4;      // 比誘電率
    double tanD = 0.02;     // 誘電正接
    double sigma_Sm = 5.8e7;// 導体導電率 [S/m] (0 = 無損失)
    double length_mm = 50.0;// 線路長
};

struct Result {
    bool   valid = false;
    double z0_ohm = 0.0;      // 特性インピーダンス (実数、準 TEM)
    double epsEff = 0.0;      // 実効誘電率
    double vp_mps = 0.0;      // 位相速度 c/√ε_eff
    double beta_radm = 0.0;   // 位相定数 β
    double alphaC_Npm = 0.0;  // 導体損
    double alphaD_Npm = 0.0;  // 誘電損
    double alpha_dBm = 0.0;   // 合計減衰 [dB/m]
    double delay_s = 0.0;     // 線路長ぶんの群遅延 (非分散なら ℓ√ε_eff/c)
    // 損失を含む複素特性インピーダンス Z₀ = √((R+jωL)/(G+jωC))。
    // 無損失では虚部が厳密に 0 で実部が z0_ohm に一致する。
    std::complex<double> z0Complex;
    // 導体損が広線路近似 (細い線路で過小評価) かどうか。同軸は false。
    bool   alphaCApprox = true;
};

// 周波数 f [Hz] における線路の特性。幾何が不正なら valid = false。
Result analyze(const Line &line, double freq_Hz);

// 完全楕円積分の比 K(k)/K(k') を AGM で求める (0 < k < 1)。
// k = 1/√2 でちょうど 1 になる (自己補対称) — selftest の判定に使う。
double ellipticRatio(double k);

// 長さ ℓ の一様線路 (特性インピーダンス Z₀・伝搬定数 γ) を基準インピーダンス
// z0Ref で見た S パラメータ。相反・対称なので S11 = S22, S21 = S12。
struct SParam {
    bool valid = false;
    std::complex<double> s11, s21;
};
SParam sParameters(const Result &r, double length_mm, double z0Ref_ohm);

} // namespace tline
} // namespace ofd

#endif // OFD_CORE_TRANSMISSIONLINE_H
