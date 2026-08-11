// SarMetrics.h — SAR (比吸収率) の定義式と電波防護指針の指針値 — Qt 非依存 / C++17
//
// SAR タブ (src/tabs/SarTab.cpp) が使う計算・規格値の実体。GUI に式や数値を
// 直書きせず、selftest から定義式・規格値と直接突き合わせられるようにここへ
// 集約する。
//
// 収録するもの:
//   (a) 定義式
//       - SAR = σ|E|²/(2ρ)   (|E| は振幅) / σ|E_rms|²/ρ
//         IEEE Std C95.1-2019 §3 の定義、IEC 62704-1:2017 §3.1.28。
//       - 平面波の電力密度 S = |E_rms|²/Z0 (Z0 = 376.730 Ω)。
//       - 断熱温度上昇 ΔT = SAR·t/c_p
//         (Pennes 生体熱方程式 H. H. Pennes, J. Appl. Physiol. 1, 93 (1948) で
//          熱伝導・血流灌流の項を落とした短時間曝露の上限評価)。
//   (b) 指針値 (基本制限)
//       - ICNIRP 2020 Guidelines, Health Phys. 118(5), 483-524 (2020),
//         Tables 2-4 および参考レベル Table 6。
//       - IEEE Std C95.1-2019, Tables 1・7 (DRL / ERL)。
//       - FCC 47 CFR §1.1310 / §2.1093 (ANSI/IEEE C95.1-1992 準拠)。
//
// 注意 (UI にも明示すること):
//   - ここで返す SAR は「点 SAR」であり、指針値の 1 g / 10 g 空間平均 SAR とは
//     別量。適合判定には空間平均 SAR (体積内平均) が必要で、それには電界分布
//     全体が要る。
//   - 指針値は代表的な条項の抜粋。実際の適合評価は規格本文と
//     IEC 62704 シリーズの手順に従うこと。
#ifndef OFD_EM_SARMETRICS_H
#define OFD_EM_SARMETRICS_H

namespace ofd {
namespace em {

// 自由空間の波動インピーダンス [Ω] (CODATA 2018)
constexpr double kFreeSpaceImpedance = 376.730313668;

// ── 定義式 ──────────────────────────────────────────────────────────────────
// SAR = σ|E|²/(2ρ)  [W/kg]  — |E| は正弦定常の振幅 (ピーク値)
double sarFromPeakField(double sigma_Sm, double ePeak_Vm, double rho_kgm3);
// SAR = σ|E_rms|²/ρ  [W/kg]
double sarFromRmsField(double sigma_Sm, double eRms_Vm, double rho_kgm3);

// 平面波の電力密度 S = |E_rms|²/Z0 [W/m²] とその逆算
double planeWavePowerDensityFromRms(double eRms_Vm);
double rmsFieldFromPowerDensity(double s_Wm2);

// ── 曝露源からの入射量 (遠方界) ────────────────────────────────────────────
// 送信電力とアンテナ利得から、距離 d の**遠方界**での入射電力密度を求める。
//   S = P_t·G / (4πd²)   [W/m²]           (等価等方放射の定義そのもの)
//   E_rms = √(S·Z0)                        (平面波の関係)
// ICNIRP 2020 / IEEE C95.1-2019 の**参考レベル**は S と E で与えられるので、
// この値はそのまま Metric::IncidentPowerDensity と比べられる。
//
// **近傍界では使えない。** 反応性近傍界の境界は λ/(2π) で、携帯電話を身体に
// 密着させる配置などはここに入る。その場合は SAR を場の分布から直接求める
// 必要があり、この式で代用してはいけない (呼び出し側で距離を検査すること)。
double dbmToWatts(double dBm);
double farFieldPowerDensity(double power_W, double gainDbi, double dist_m);
double reactiveNearFieldBoundary(double frequency_Hz);   // λ/(2π) [m]

// 断熱温度上昇 ΔT = SAR·t/c_p [K] (熱伝導・血流灌流を無視した上限)
//   specificHeat_JkgK: 組織の比熱 (筋肉 ≈ 3421 J/(kg·K), IT'IS V4.1)
double adiabaticTemperatureRise(double sar_Wkg, double time_s,
                                double specificHeat_JkgK);

// ── 指針値 ──────────────────────────────────────────────────────────────────
enum class Standard {
    Icnirp2020,      // ICNIRP 2020 Guidelines
    IeeeC95_1_2019,  // IEEE Std C95.1-2019
    Fcc47Cfr         // FCC 47 CFR §1.1310 / §2.1093
};

enum class Category {
    GeneralPublic,   // 一般環境 / uncontrolled / unrestricted
    Occupational     // 職業 / controlled / restricted
};

enum class Metric {
    LocalSar10g,          // 局所 SAR (10 g 平均)
    LocalSar1g,           // 局所 SAR (1 g 平均)
    WholeBodySar,         // 全身平均 SAR
    IncidentPowerDensity, // 入射電力密度 (全身、参考レベル)
    AbsorbedPowerDensity, // 局所吸収電力密度 (4 cm² 平均、6 GHz 超)
    LocalTemperatureRise  // 局所温度上昇 (限度値ではなく根拠値)
};

struct ExposureLimit {
    bool   defined = false;      // その規格・区分でこの指標が定義されているか
    bool   applicable = false;   // さらに、与えた周波数が適用範囲内か
    bool   isBasis = false;      // true = 限度値ではなく健康影響の根拠値
    double value = 0.0;
    const char *unit = "";       // "W/kg" / "W/m^2" / "K"
    double fmin_Hz = 0.0;
    double fmax_Hz = 0.0;
    double averagingMass_g = 0.0;  // 0 = 該当なし
    double averagingTime_s = 0.0;  // 0 = 該当なし
    const char *reference = "";    // 出典条項 (表示用、翻訳しない)
};

// 規格 × 区分 × 指標 × 周波数 → 指針値。
// 定義が無い組み合わせは defined = false、周波数が範囲外なら
// defined = true / applicable = false を返す。
ExposureLimit exposureLimit(Standard standard, Category category,
                            Metric metric, double frequency_Hz);

// 判定 (算出値がまだ無いケースを型で表す)
enum class Verdict {
    NotEvaluated,   // 算出値が無い (未計算)
    NotApplicable,  // この周波数・規格では対象外
    Compliant,
    NonCompliant
};

// value が有限で limit が適用可能なときのみ Compliant / NonCompliant を返す。
// hasValue = false のときは常に NotEvaluated (「適合」を勝手に出さない)。
Verdict evaluate(const ExposureLimit &limit, double value, bool hasValue);

} // namespace em
} // namespace ofd

#endif // OFD_EM_SARMETRICS_H
