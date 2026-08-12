// CircuitImpulse.h — PIC 回路の時間領域応答 (複素包絡線) — Qt 非依存 / C++17。
//
// `optics/PhotonicCircuit` は素子の振幅伝達 H(λ) を周波数領域で出す。
// 配線図タブの「時間領域」モードはその**インパルス応答**を見るためのもの
// (リングに溜まって出てくる遅延パルス列、MZI の 2 経路、導波路の群遅延)。
//
// ── 光の時間領域は「包絡線」で扱う ────────────────────────────────────────
// 1550 nm の搬送波は 193 THz で、これを直接標本化することはできない
// (1 周期 5 fs)。**搬送波 λ0 まわりの帯域 B だけを見た複素包絡線**として
// 扱う。得られる h(t) は複素で、時間分解能は dt = 1/B、見える時間長は
// M·dt = M/B。
//
//   H_bb(Δf) = H(λ(f0 + Δf))     — Δf ∈ [−B/2, B/2)
//   h(t)     = IFFT{H_bb}        — 複素 (実部だけ取ると意味が壊れる)
//
// **搬送波の位相は含まない。** 遅延 τ は包絡線の遅延 (群遅延) として出る。
//
// ── 標本化の制約 (黙って踏まないための約束) ──────────────────────────────
//   * 見える時間長 M/B より長い遅延は**巡回して先頭へ回り込む**。リングの
//     ように応答が無限に続くものは必ず尾を切ることになるので、切った量を
//     `tailFraction` に出す。
//   * 帯域 B が素子の周期 (リングの FSR) の整数倍でないと、周波数格子が
//     周期関数を割り切らずタップがにじむ。**B を FSR の整数倍に選ぶと
//     タップは厳密に出る** (selftest はこの条件で閉形式と突き合わせている)。
//
// ── 検証できること (selftest がこれを判定している) ───────────────────────
//   * 直線導波路 → 群遅延 τ = ng·L/c にただ 1 本のタップ
//   * MZI → 2 本のアームの群遅延にちょうど 2 本のタップ
//   * 全域通過リング → タップは**厳密な等比級数**:
//       h₀ = t₁,  hₙ = (t₁² − 1)·t₁^(n−1)·aⁿ   (n ≥ 1)
//     (H = (t₁ − a e^{−jφ})/(1 − t₁ a e^{−jφ}) を a e^{−jφ} で展開したもの。
//      t₁ = √(1−κ₁²) は自己結合、a は 1 周の振幅透過率)
//   * 無損失の全域通過リングは |H| ≡ 1 なので Σ|hₙ|² = 1 (エネルギー保存)
#ifndef OFD_OPTICS_CIRCUITIMPULSE_H
#define OFD_OPTICS_CIRCUITIMPULSE_H

#include <complex>
#include <cstddef>
#include <functional>
#include <vector>

namespace ofd {
namespace pic {

using cplx = std::complex<double>;

// 波長 [nm] を受けて振幅伝達を返す関数 (PhotonicCircuit の through/drop/bar/cross)
using SpectrumFn = std::function<cplx(double lambda_nm)>;

struct ImpulseConfig {
    double lambda0_nm = 1550.0;  // 搬送波 (包絡線の中心)
    double bandwidth_Hz = 0.0;   // 帯域 B。0 なら fsrHint から自動で決める
    double fsrHint_Hz = 0.0;     // 素子の FSR。B を自動にするときの単位
    int    fsrMultiple = 16;     // 自動のとき B = fsrMultiple × FSR
    int    points = 1024;        // 標本数 M (2 の冪へ切り上げ)
    bool   valid() const {
        return lambda0_nm > 0.0 && points >= 16
               && (bandwidth_Hz > 0.0 || fsrHint_Hz > 0.0);
    }
};

struct ImpulseResult {
    double dt_s = 0.0;                 // 時間分解能 1/B
    double span_s = 0.0;               // 見えている時間長 M·dt
    double bandwidth_Hz = 0.0;         // 実際に使った B
    std::vector<cplx> h;               // 複素インパルス応答 (長さ M)
    double mainDelay_s = 0.0;          // 主到達の遅延 (|h| 最大の位置)
    double tapSpacing_s = 0.0;         // 上位タップの間隔 (リングの 1 周時間)
    double decayRatio = 0.0;           // 隣り合うタップの振幅比 (0 = 出せず)
    double energy = 0.0;               // Σ|h|²
    double tailFraction = 0.0;         // 後ろ 1/4 に残った電力の割合 (尾の目安)
    bool   ok() const { return !h.empty() && dt_s > 0.0; }
};

// H(λ) から複素包絡線のインパルス応答を作る。
ImpulseResult impulse(const SpectrumFn &H, const ImpulseConfig &cfg);

// |h| のピークを大きい順に拾う (時刻 [s] と振幅)。thresh は最大値に対する比。
std::vector<std::pair<double, double>> peaks(const ImpulseResult &r,
                                             double thresh = 0.02,
                                             std::size_t maxCount = 16);

} // namespace pic
} // namespace ofd

#endif // OFD_OPTICS_CIRCUITIMPULSE_H
