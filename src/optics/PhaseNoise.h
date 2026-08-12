// PhaseNoise.h — レーザ線幅 → 干渉計を通した強度雑音への換算 — Qt 非依存。
//
// 配線図タブの「位相雑音」。**干渉計は位相雑音を強度雑音に変える**ので、
// 線幅 Δν のレーザを遅延 τ の干渉計 (MZI / リング) に通すと、出力の
// 強度が揺らぐ。ここはその換算だけを担う (受光器側の雑音は
// `core/ReceiverNoise`)。
//
// ── 土台にしている厳密な関係 ──────────────────────────────────────────────
// ローレンツ線幅 Δν [Hz] のレーザの位相は **Wiener 過程** (白色周波数雑音の
// 積分) で、遅延 τ を隔てた位相差 Δφ = φ(t) − φ(t−τ) は
//
//     Δφ ~ 正規分布,  平均 0,  分散  σ² = 2π·Δν·τ           … (1)
//
// これは近似ではなく、ローレンツ線幅の定義そのものから出る厳密な結果
// (自己相関 ⟨E*(t)E(t−τ)⟩ = exp(−π·Δν·|τ|) と (1) は同じことを言っている)。
// コヒーレンス時間は τ_c = 1/(π·Δν)。
//
// ── 干渉計の出力 (すべて閉形式) ──────────────────────────────────────────
// 2 光路が等分に合わさる干渉計の出力を P = (P₀/2)·(1 + cos(φ₀ + Δφ)) とする。
// Δφ が正規分布なので、⟨cos(φ₀+Δφ)⟩ = e^{−σ²/2}·cos φ₀ が厳密に書け、
// 二乗平均からは
//
//     ⟨P⟩    = (P₀/2)(1 + V·cos φ₀),        V = e^{−σ²/2} = e^{−π·Δν·τ}
//     Var[P] = (P₀/2)²·[ (1 − V⁴·cos2φ₀ … ) ]   ← 下の実装は展開した形
//
// が出る。**V は干渉の可視度**そのもので、τ → 0 または Δν → 0 で 1
// (雑音なし)、τ ≫ τ_c で 0 (干渉が消える) になる。
//
// 直交バイアス (φ₀ = π/2) は位相→強度の変換利得が最大で、実装上いちばん
// 効く動作点。小さい σ では σ_P/⟨P⟩ → σ (位相のゆらぎがそのまま相対強度の
// ゆらぎになる) という分かりやすい極限を持つ — selftest はこの極限と
// 厳密式の一致も見ている。
//
// ── 出さないもの ──────────────────────────────────────────────────────────
// 周波数ごとの雑音スペクトル密度 (RIN(f) の形) は出さない。**帯域内の
// 総量**だけを扱う。スペクトルの形を出すには干渉計の伝達関数と
// 周波数雑音 PSD の積を帯域で積分する必要があり、白色周波数雑音以外
// (1/f 雑音) の情報が要る — 持っていないものを推測で描かない。
#ifndef OFD_OPTICS_PHASENOISE_H
#define OFD_OPTICS_PHASENOISE_H

namespace ofd {
namespace optics {

struct PhaseNoiseInput {
    double linewidth_Hz = 1.0e6;   // レーザのローレンツ線幅 Δν
    double delay_s = 0.0;          // 干渉計の光路差による遅延 τ
    double bias_rad = 1.5707963267948966;  // 動作点 φ₀ (既定は直交バイアス)
    double power_W = 1.0e-3;       // 入力光パワー P₀
    bool valid() const {
        return linewidth_Hz >= 0.0 && delay_s >= 0.0 && power_W > 0.0;
    }
};

struct PhaseNoiseResult {
    bool   valid = false;
    double phaseVariance = 0.0;    // σ² = 2π·Δν·τ [rad²]
    double phaseRms_rad = 0.0;     // σ
    double coherenceTime_s = 0.0;  // τ_c = 1/(π·Δν) (Δν = 0 なら無限大)
    double visibility = 0.0;       // V = e^{−σ²/2}
    double meanPower_W = 0.0;      // ⟨P⟩
    double rmsPower_W = 0.0;       // √Var[P]
    double relativeIntensityNoise = 0.0;  // √Var[P]/⟨P⟩ (無次元、帯域内の総量)
    double rin_dB = 0.0;           // 20 log10(上の比) [dB]
};

// 線幅と遅延から干渉計出力の強度ゆらぎを出す (すべて閉形式)。
PhaseNoiseResult analyse(const PhaseNoiseInput &in);

// 位相差の分散 σ² = 2π·Δν·τ (Wiener 過程の厳密解)
double phaseVariance(double linewidth_Hz, double delay_s);

// 干渉の可視度 V = e^{−π·Δν·τ}
double visibility(double linewidth_Hz, double delay_s);

} // namespace optics
} // namespace ofd

#endif // OFD_OPTICS_PHASENOISE_H
