// ReceiverNoise.h — 受光器 (フォトダイオード) の雑音収支。
// Qt 非依存 / C++17。SchematicTab「ノイズ・温度効果」の計算実体。
//
// ── 式 (いずれも定義そのもの) ──────────────────────────────────────────────
//
//   光電流        I  = R·P + I_dark            (R = 受光感度 [A/W])
//   ショット雑音   ⟨i²⟩ = 2q·I·B                (q は素電荷)
//   熱雑音        ⟨i²⟩ = 4k·T·B / R_L          (Johnson–Nyquist。**T は絶対温度**)
//   RIN           ⟨i²⟩ = rin·(R·P)²·B          (rin = 10^(RIN[dB/Hz]/10))
//   SNR           = (R·P)² / Σ⟨i²⟩
//   NEP           = √(Σ⟨i²⟩/B) / R             [W/√Hz]
//
// 高パワー極限では RIN が P² で効くので **SNR → 1/(rin·B) で頭打ち**になる
// (光を強くしても良くならない)。selftest はこの漸近値を判定に使っている。
//
// ── 扱わないもの (絶対規則 5) ──────────────────────────────────────────────
//
// - アバランシェ増倍 (APD の過剰雑音指数 F(M))。
// - レーザの位相雑音 → 強度雑音への変換 (干渉計の遅延と線幅が要る)。
// - 増幅器の雑音指数・入力換算雑音電流。
#ifndef OFD_CORE_RECEIVERNOISE_H
#define OFD_CORE_RECEIVERNOISE_H

namespace ofd {
namespace rxnoise {

struct Receiver {
    double responsivity_A_W = 0.9;   // 受光感度
    double opticalPower_W = 1.0e-3;  // 受光パワー
    double darkCurrent_A = 1.0e-9;
    double loadResistance_ohm = 50.0;
    double temperature_C = 25.0;
    double bandwidth_Hz = 1.0e9;
    double rin_dBHz = -155.0;        // 相対強度雑音 [dB/Hz]
    // どの項を数えるか (タブのチェックにそのまま対応する)
    bool shot = true, thermal = true, rin = true;
};

struct Noise {
    bool   valid = false;
    double photocurrent_A = 0.0;   // R·P + I_dark
    double signalCurrent_A = 0.0;  // R·P (信号成分)
    double shot_A2 = 0.0;          // 各項の分散 [A²]
    double thermal_A2 = 0.0;
    double rin_A2 = 0.0;
    double total_A2 = 0.0;
    double rms_A = 0.0;
    double snr_dB = 0.0;
    bool   snrValid = false;       // 有効な雑音項が 1 つも無ければ false
    double nep_W_rtHz = 0.0;
};

// 帯域・負荷抵抗・感度が正でなければ valid = false。
Noise analyze(const Receiver &rx);

// RIN が支配する極限の SNR [dB] = 10log10(1/(rin·B))。
// 光パワーをいくら上げてもこれ以上は良くならない。
double rinLimitedSnrDb(double rin_dBHz, double bandwidth_Hz);

} // namespace rxnoise
} // namespace ofd

#endif // OFD_CORE_RECEIVERNOISE_H
