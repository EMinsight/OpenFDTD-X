// SoundInsulation.h — 建築遮音の予測と単一数値評価 (Qt 非依存 / C++14)。
//
// 防音設計タブ (SoundproofTab) が使う計算をここに集約する。GUI に式を書かず、
// selftest から解析解・規格の不変量と直接突き合わせられるようにするため。
//
// 収録するもの (すべて公表された式・規格の手順):
//   - 単一壁の音響透過損失 R(f): 場入射質量則 + コインシデンス
//     (B. H. Sharp, "A Study of Techniques to Increase the Sound Insulation of
//      Building Elements", Wyle Laboratories WR 73-5, 1973。
//      Bies & Hansen, "Engineering Noise Control" 4th ed., §8.2 に再掲)
//   - 二重壁の R(f): 同 Sharp のスキーム (質量-空気-質量共鳴 f0 と限界周波数 fl)
//   - 限界 (コインシデンス) 周波数: Cremer の薄板曲げ波理論
//   - Rw   : ISO 717-1 の基準曲線あてはめ手順
//   - STC  : ASTM E413 の基準曲線あてはめ手順
//   - Ln,w : ISO 717-2 の基準曲線あてはめ手順
//   - C / Ctr : ISO 717-1 のスペクトル適応項 (スペクトル No.1 / No.2)
//   - 複合壁の R (面積加重の τ 平均)、受音室レベル、DnT,w
//   - 囲いの挿入損失 IL、ダクト系の減衰 (ASHRAE)
//   - STI : IEC 60268-16 の MTF 法
//
// すべて拡散音場入射・無限大パネルの理想化に基づく「予測」であり、
// 実測値や FDTD の結果を置き換えるものではない (UI 側でその旨を表示する)。
#pragma once
#include <vector>

namespace ofd {
namespace acoustics {
namespace insulation {

// ── 帯域 ────────────────────────────────────────────────────────────────────
// 1/3 オクターブ中心周波数 50 Hz〜5 kHz (21 バンド)。
// ISO 717-1 / 717-2 は 100〜3150 Hz (添字 3..18)、
// ASTM E413 は 125〜4000 Hz (添字 4..19) を使う。
const int kNumBands = 21;
extern const double kThirdOctaveHz[kNumBands];

const int kIsoFirst  = 3;    // 100 Hz
const int kIsoCount  = 16;   // 100..3150 Hz
const int kAstmFirst = 4;    // 125 Hz
const int kAstmCount = 16;   // 125..4000 Hz

// 空気の特性インピーダンス ρ0·c0 [Pa·s/m] と音速 [m/s] (20 ℃)
extern const double kRhoC;
extern const double kSoundSpeed;

// ── 層構成 ──────────────────────────────────────────────────────────────────
// cavity = true の層は「葉」を分ける空隙 (空気層 / 多孔質充填) として扱う。
struct Layer {
    double thicknessM;    // 厚さ [m]
    double densityKgM3;   // 密度 [kg/m³] (空隙層では質量に算入しない)
    double youngsPa;      // ヤング率 [Pa] (0 = 剛性不明 → 質量則のみ)
    double poisson;       // ポアソン比
    double lossFactor;    // 材料の内部損失係数 η_int (境界損失は別途加算)
    bool   cavity;        // 空隙層 (葉の分割点)
    bool   porousFill;    // 空隙層に吸音材が充填されている

    Layer()
        : thicknessM(0), densityKgM3(0), youngsPa(0), poisson(0.3),
          lossFactor(0.01), cavity(false), porousFill(false) {}

    double surfaceMass() const   // 面密度 [kg/m²]
    {
        return cavity ? 0.0 : thicknessM * densityKgM3;
    }
};

enum ModelKind {
    ModelNone       = 0,   // 計算できない (有効な層が無い)
    ModelSingleLeaf = 1,   // 単一壁 (質量則 + コインシデンス)
    ModelDoubleLeaf = 2    // 二重壁 (Sharp)
};

struct TlResult {
    bool   valid;
    int    model;                // ModelKind
    int    leafCount;            // 層構成から検出した葉の数
    bool   reducedToTwoLeaves;   // 3 枚以上を最大空隙で 2 葉へ集約した
    bool   cavityAbsorbed;       // 空隙に吸音材充填あり (Sharp の前提)
    double surfaceMass;          // 全体の面密度 [kg/m²]
    double leafMass[2];          // 各葉の面密度 [kg/m²]
    double leafCriticalHz[2];    // 各葉の限界周波数 [Hz] (0 = 剛性不明)
    double cavityDepthM;         // 空隙厚 [m]
    double massAirMassHz;        // 二重壁の質量-空気-質量共鳴 f0 [Hz]
    double limitingHz;           // 二重壁の限界周波数 fl = 55/d [Hz]
    double lossFactor500;        // 500 Hz における全損失係数 η_tot (第 1 葉)
    double R[kNumBands];         // 音響透過損失 [dB]

    TlResult();
};

// 層構成 (上から順) から拡散 (場) 入射の音響透過損失 R(f) を求める。
//   decoupled = true  : 空隙層で葉が分離しているとみなす (二重壁モデル)
//   decoupled = false : 空隙をまたぐ構造的結合があるとみなし、
//                       全層を合計面密度の単一壁として扱う (保守側)
TlResult transmissionLoss(const std::vector<Layer> &layers,
                          bool decoupled = true);

// 建物に組み込まれた要素の全損失係数 (EN 12354-1:2000 附属書 C):
//   η_tot = η_int + m'/(485·√f)      (m' = 面密度 [kg/m²], f [Hz])
// 第 2 項は境界 (支持部) への振動エネルギー流出。実験室の自由支持パネルより
// 現場のほうがコインシデンス域の R が高くなる主因。
double totalLossFactor(double internalEta, double surfaceMass, double freqHz);

// 限界 (コインシデンス) 周波数 fc = c²/(2π)·√(m/B')  [Hz]
//   B' = E·h³/(12(1−ν²)) : 単位幅あたり曲げ剛性、m = ρ·h : 面密度
// (L. Cremer, Akust. Z. 7, 81-104, 1942)
double criticalFrequency(double youngsPa, double poisson, double densityKgM3,
                         double thicknessM);

// 場入射質量則 R = 20·log10(m·f) − 48 [dB]
// (垂直入射 R0 = 10log10(1+(πmf/ρ0c)²) から場入射補正 −5 dB。
//  ρ0c = 413.6 のとき定数は −47.4 ≒ −48。Sharp 1973 / Bies & Hansen §8.2.1)
double fieldIncidenceMassLaw(double freqHz, double surfaceMass);

// ── 単一数値評価 (基準曲線あてはめ) ─────────────────────────────────────────
struct RatingResult {
    bool   valid;
    int    value;           // Rw / STC / Ln,w [dB]
    double sumDeficiency;   // 不利偏差の合計 [dB]
    double maxDeficiency;   // 最大不利偏差 [dB]
    int    shift;           // 基準曲線のずらし量 [dB]

    RatingResult() : valid(false), value(0), sumDeficiency(0),
                     maxDeficiency(0), shift(0) {}
};

// ISO 717 / ASTM E413 に共通の手順:
// 基準曲線を 1 dB 刻みでずらし、不利偏差の合計が maxSum を超えず
// (かつ ASTM では 1 帯域の不利偏差が maxSingle を超えない) 範囲で
// もっとも不利側へ寄せた位置を採る。戻り値は 500 Hz における基準曲線の値。
//   measured : 評価する n 帯域の値 [dB]
//   ref      : 基準曲線 n 帯域の値 [dB] (絶対値でも相対値でもよい)
//   index500 : ref/measured 中の 500 Hz の添字
//   maxSingle: 1 帯域あたりの不利偏差上限 (<= 0 なら制限なし)
//   higherIsBetter : true = R 系 (基準を下回る側が不利)、
//                    false = Ln 系 (基準を上回る側が不利)
RatingResult contourRating(const double *measured, const double *ref, int n,
                           int index500, double maxSum, double maxSingle,
                           bool higherIsBetter);

// Rw (ISO 717-1)。R21 は kThirdOctaveHz と同じ 21 帯域。
RatingResult weightedReduction(const double *R21);
// STC (ASTM E413)。
RatingResult soundTransmissionClass(const double *R21);
// Ln,w (ISO 717-2)。Ln21 は同じ 21 帯域の規準化床衝撃音レベル。
RatingResult weightedImpact(const double *Ln21);

// ISO 717-1 のスペクトル (No.1 = ピンクノイズ A 特性、No.2 = 都市交通騒音)
enum SpectrumKind { SpectrumPink = 0, SpectrumTraffic = 1 };

// スペクトル適応項 C (No.1) / Ctr (No.2):
//   X_A = −10·log10( Σ 10^((Lij − Ri)/10) ),  C = X_A − Rw
// (ISO 717-1:2013 §6.2)。ok が false のとき戻り値は無意味。
int spectrumAdaptation(const double *R21, int spectrumKind, int rwValue,
                       bool *ok);

// ── 現場・複合の標準式 ──────────────────────────────────────────────────────
// 面積加重の透過率平均から複合壁の R を求める:
//   τ̄ = Σ Si·10^(−Ri/10) / Σ Si,  R = −10·log10(τ̄)
double compositeReduction(const double *areas, const double *R, int n);

// 受音室の音圧レベル Lp2 = Lp1 − R + 10·log10(S/A)
// (ISO 10140-2 / ISO 12354-1 の R の定義そのもの。S = 隔壁面積、
//  A = 受音室の等価吸音面積)
double receivingLevel(double lp1, double R, double areaM2,
                      double absorptionA);

// Sabine の等価吸音面積 A = 0.161·V/T [m²]
double sabineAbsorption(double volumeM3, double rt60S);

// 標準化レベル差 DnT,w = Rw + 10·log10(0.32·V/S)
// (DnT = D + 10log10(T/T0), T0 = 0.5 s と A = 0.161V/T の組合せ。
//  ISO 12354-1 / ISO 16283-1)
double standardizedLevelDifference(double Rw, double volumeM3, double areaM2);

// 囲いの挿入損失 IL = R_eff − 10·log10(S/A_in)
//   R_eff : 開口を含む面積加重 τ 平均 (開口は τ = 1)
//   A_in  : 囲い内部の等価吸音面積 = S·ᾱ_in
// (ISO 11546-1 / Bies & Hansen §12.3。内部が拡散場で、
//  構造伝搬・気密不良が無い理想状態の上限値)
double enclosureInsertionLoss(double Rwall, double wallArea, double openArea,
                              double interiorAlpha);

// ── ダクト系 (ASHRAE Handbook — HVAC Applications, Sound and Vibration
//    Control の章に収録された式・表) ──────────────────────────────────────
// 内貼りダクトの減衰 [dB/m] : Sabine の式 1.05·α^1.4·P/A
//   (P = 内周長 [m], A = 断面積 [m²]。α ≳ 0.2 で妥当)
double linedDuctAttenuation(double alpha, double perimeterM, double areaM2);

// 直角エルボ (案内羽根なし) の減衰 [dB]。ASHRAE の f·w 表を SI 化したもの
// (w = ダクト幅 [m])。lined = エルボ前後に内貼りあり。
double elbowAttenuation(double freqHz, double widthM, bool lined);

// 分岐の減衰 [dB] = 10·log10(ΣA / A_branch) (音響パワーの断面積分配)
double branchAttenuation(double branchAreaM2, double totalAreaM2);

// 開口端反射損失 [dB]。低周波の放射効率から
//   1 − |Γ|² = (k·a)²      (フランジ付き = 壁面納まり)
//   1 − |Γ|² = (k·a)²/2    (フランジなし = 自由端)
//   a = √(A/π) : 等価半径
// (Levine & Schwinger, Phys. Rev. 73, 383, 1948 の低周波極限。
//  Morse & Ingard, "Theoretical Acoustics" §7.2)
double endReflectionLoss(double freqHz, double areaM2, bool flanged);

// 残響場の音圧レベル Lp = LW + 10·log10(4/A) [dB]
// (拡散音場。直接音は含まない)
double reverberantLevel(double pwl, double absorptionA);

// ── STI (IEC 60268-16 の MTF 法) ────────────────────────────────────────────
// 変調伝達関数 m(F) = 1/√(1+(2πF·T/13.8)²) · 1/(1+10^(−S/N /10))
// を 7 オクターブ帯域 (125 Hz〜8 kHz) × 変調周波数 14 点で求め、
// 見かけの S/N (±15 dB でクリップ) から MTI を得て、
//   STI = Σ αk·MTIk − Σ βk·√(MTIk·MTIk+1)
// (男声の α/β。Σα − Σβ = 1 なので全帯域 MTI = 1 で STI = 1)
double sti(double rt60S, double snrDb);
// 帯域ごとに残響時間と S/N を与える版 (いずれも 125 Hz〜8 kHz の 7 要素)
double stiBands(const double rt60Bands[7], const double snrBands[7]);

} // namespace insulation
} // namespace acoustics
} // namespace ofd
