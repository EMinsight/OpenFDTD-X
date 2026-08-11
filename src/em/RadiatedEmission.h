// RadiatedEmission.h — 遠方界パターンから放射妨害波レベルを予測する
//                      (Qt 非依存 / C++17)
//
// EMC/EMI タブの「放射エミッション」で、FDTD の計算結果 (far1d.log) を規格の
// 限度値と同じ土俵 (測定距離での電界強度 [dBμV/m]) に載せるための換算。
// 限度値そのものは `em/EmcStandards` が持つ (規格の公表値)。
//
// ── far1d.log の値が何なのか ────────────────────────────────────────────────
// OpenFDTD の `post/outputFar1d.c` は `20·log10(√(|Eθ|²+|Eφ|²))` を書く。
// この Eθ/Eφ は `sol/farfield.c` の `farfactor()` を掛けたもので、給電がある
// 問題では
//     ffctr = k / √(8π·η0·P_in),   P_in = Σ_feed 0.5·Re(V·I*)
// である。遠方界 E_phys = (k/4πr)·I·e^{−jkr} に対して利得の定義
//     G = 4πr²|E_phys|² / (2·η0·P_in)
// を書き下すと `G = ffctr²·|I|²` となり、**出力値の 2 乗がそのまま利得**に
// なる。したがって
//     far1d.log の E-abs[dB] = 10·log10(G) = 利得 [dBi]
// である (全成分合成なので偏波込みの全利得)。
//
// **平面波入射 (`planewave`) の問題では成り立たない** — その場合 farfactor は
// `k/(E_inc·√(4π))` で、出力は RCS 系の量になる。この換算は給電のある問題
// (`feed` がある .ofd) にのみ適用すること。
//
// ── 電界強度への換算 ────────────────────────────────────────────────────────
// 自由空間の遠方界で、送信電力 P_t [W]・利得 G の等価等方放射から距離 d [m]:
//     E [V/m] = √(30·G·P_t) / d          (EIRP = G·P_t からの標準式)
//     E [dBμV/m] = 134.771 + 10log10(P_t[W]) + G[dBi] − 20log10(d[m])
// 定数 134.771 = 20·log10(√30 × 10⁶)。
// (ANSI C63.4 / CISPR 16-2-3 が用いる自由空間換算。実サイトの測定値は
//  グランド反射で最大 +6 dB 高くなりうる — `kGroundReflectionMaxDb`。)
//
// ── 適用限界 (UI に必ず出すこと) ────────────────────────────────────────────
//   - P_t は**利用者が与える**。.ofd の給電振幅は任意 (規格化されている) ので
//     GUI 側からは実機の送信電力を知り得ない。P_t 無しに絶対値は出せない。
//   - 遠方界条件 d ≳ 2D²/λ を満たさない距離では上式は使えない
//     (`fraunhoferDistanceM`)。近傍界の測定は別の扱いが要る。
//   - far1d.log に含まれる面 (X/Y/Z 面など) の中の最大値しか見ていないので、
//     全球の最大放射方向を捉えているとは限らない (far2d が要る)。
#ifndef OFD_EM_RADIATEDEMISSION_H
#define OFD_EM_RADIATEDEMISSION_H

namespace ofd {
namespace em {

// E[dBμV/m] = kEirpToFieldDb + 10log10(P) + G − 20log10(d) の定数項。
// 20·log10(√30 × 10⁶) を実行時に計算する (定数の丸めを持ち込まない)。
double eirpToFieldConstantDb();

// 自由空間の電界強度 [dBμV/m]。P_t ≤ 0 または d ≤ 0 なら valid = false。
struct FieldStrength {
    double dBuVm = 0;       // 電界強度 [dBμV/m]
    double vPerM = 0;       // 同 [V/m]
    double eirpDbm = 0;     // EIRP [dBm] = 10log10(P·G·1000)
    bool   valid = false;
};
FieldStrength fieldStrength(double gainDbi, double powerW, double distM);

// グランド反射のある試験サイト (OATS / 半電波暗室) で直接波と反射波が同相で
// 合成されるときの上限 [dB]。20·log10(2) = 6.02 dB。
// CISPR 16-1-4 / ANSI C63.4 の測定は高さ走査でこの最大を拾う前提なので、
// 自由空間予測と実測を比べるときの上振れ分として使う。
double groundReflectionMaxDb();

// ── 試験サイトのグランド反射 (試験配置の設定がここに効く) ──────────────────
//
// `groundReflectionMaxDb()` は「同相合成の上限」という**周波数にも配置にも
// 依らない一律 6.02 dB** で、どんな距離・アンテナ高でも同じ値を返す。
// 実際の増分は 2 波の干渉なので配置で決まり、打ち消し側に振れることもある。
// ここは試験配置 (サイト種別・測定距離・アンテナ高) から実際の増分を出す。
//
// 前提と出典:
//   - EUT は卓上機器の標準配置で高さ 0.8 m (CISPR 16-1-4 / ANSI C63.4)。
//   - 受信アンテナは 1〜4 m を走査し、**その最大値**を測定値とする
//     (同上)。`scanMaxDb` がその走査の最大、`atHeightDb` が画面で指定した
//     高さでの値。
//   - 反射係数は**完全導体面の値**を使う: 水平偏波 Γ = −1、垂直偏波 Γ = +1。
//     これは境界条件から厳密に決まる (接線方向の電界が 0 / 法線方向の磁界が
//     0)。規格は両偏波で測るので、Both は 2 つの大きい方を採る。
//   - 全電波暗室にはグランド反射が無い (applies = false)。反射室
//     (リバブレーション) は距離基準の測定ではないので同じく扱わない。
//   - 金属床を模擬しない設定 (pecGround = false) も反射なしとして扱う。
enum class EmcSite {
    OpenArea = 0,        // オープンサイト (OATS)
    SemiAnechoic = 1,    // 半電波暗室 (グランドプレーンあり)
    FullyAnechoic = 2,   // 全電波暗室 (反射なし)
    Reverberation = 3,   // 反射室
};

// 測定する偏波。規格 (CISPR 16-2-3 / ANSI C63.4) は水平・垂直の両方で測り、
// 大きい方を測定値とするので Both が既定の運用に対応する。
enum class EmcPolarization {
    Horizontal = 0,   // 完全導体面で Γ = −1
    Vertical   = 1,   // 完全導体面で Γ = +1
    Both       = 2,   // 2 つの大きい方 (規格の運用)
};

struct GroundEnhancement {
    double atHeightDb = 0.0;   // 指定アンテナ高での自由空間からの増分 [dB]
    double scanMaxDb  = 0.0;   // 1〜4 m 走査の最大 [dB]
    bool   applies    = false; // グランド反射のあるサイトか
    bool   valid      = false; // 入力が計算できるか
};

// 試験配置から反射の増分を求める。距離・高さ・周波数が非正なら valid = false。
// applies = false のサイト (全電波暗室・反射室・金属床を模擬しない設定) では
// 増分 0 を返す (0 dB は「反射が無い」の意味)。
GroundEnhancement groundEnhancement(EmcSite site, double distM,
                                    double antHeightM, double freqHz,
                                    double eutHeightM = 0.8,
                                    EmcPolarization pol = EmcPolarization::Horizontal,
                                    bool pecGround = true);

// 遠方界 (Fraunhofer) 距離 2D²/λ [m]。D は放射体の最大寸法 [m]。
// D ≤ 0 または λ ≤ 0 なら 0 を返す。
double fraunhoferDistanceM(double maxDimM, double lambdaM);

// 波長 [m] (f_Hz ≤ 0 なら 0)
double wavelengthM(double freqHz);

// マージン [dB] (正 = 限度値に対する余裕)。
double marginDb(double levelDbuVm, double limitDbuVm);

} // namespace em
} // namespace ofd

#endif // OFD_EM_RADIATEDEMISSION_H
