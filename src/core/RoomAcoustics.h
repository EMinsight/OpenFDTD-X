// RoomAcoustics.h — 室内音響の統計/幾何モデル (room-acoustics.jsx の実体)。
//
// FDTD 実行前の設計段階で使う標準的な統計手法を実装する:
//   - Sabine / Eyring 残響時間 (吸音バジェット, 帯域別)
//   - Barron 修正理論による客席の G / C80 / C50 / D50 推定
//   - MTF (Houtgast–Steeneken) による STI 推定 (直接音+指数残響, 無騒音)
//   - シューボックス1次鏡像法によるエコーグラム (反射音列, ITDG)
//   - NC (Noise Criteria) 曲線による暗騒音評価
//   - 音響障害の自動検出 (フラッターエコー / ロングディレイエコー)
//   - Schroeder 減衰曲線と ISO 3382-1 の減衰時間 (EDT/T20/T30) の回帰算出
//   - 初期側方エネルギー比 LF / LFC (ISO 3382-1, 1次鏡像法)
//   - 拡声系の音響利得 PAG/NAG (ハウリング余裕) と整合遅延
//
// いずれも「推定」であり、FDTD/幾何音響カーネルの結果を置き換えるものではない
// (UI 上もその旨を表示する)。
#pragma once
#include <QPointF>
#include <QString>
#include <QVector>

namespace ofd {

struct AcousticOpts;

namespace roomac {

// 帯域: 125, 250, 500, 1k, 2k, 4k Hz (index 0..5)
extern const double kBandHz[6];

// 客席行の実効α係数 (occupancy 0=空席 → 0.70, 1=半分 → 0.85, 2=満席 → 1.0)
double occupancyFactor(int occupancy);

// 総吸音力 A [Sabin] (enabled 行のみ、客席は occupancy 係数、Air 行は airA)
double totalAbsorption(const AcousticOpts &a, int band);

// 残響時間 [s]。formula: 0=Sabine RT=0.161V/A,
// 1=Eyring RT=0.161V/(−S·ln(1−ᾱ)+A_air) — A_air は Air 行の吸音力,
// 2=Fitzroy T=0.161·V/S²·Σ Sᵢ/(−ln(1−ᾱᵢ)) — 非均一吸音の直交3方向和
//   (D. Fitzroy, JASA 31(7), 893-897, 1959)。方向割当は x=舞台/後壁,
//   y=側壁, z=床/天井/客席。方向情報のある行が無ければ Eyring へ
//   フォールバック。
double rt60(const AcousticOpts &a, int band, int formula);
double rt60(const AcousticOpts &a, int band);   // a.rtFormula を使用

// ── Barron 修正理論による席位置の指標推定 ──────────────────────────────
// r: 音源からの距離 [m]、T: その帯域の RT60、V: 室容積。
struct SeatMetrics {
    double G = 0;      // strength [dB]
    double C80 = 0;    // clarity [dB]
    double C50 = 0;
    double D50 = 0;    // definition [0..1]
    double STI = 0;    // 推定 (無騒音, MTF法)
    double RT = 0;     // 使用した RT60 (帯域値)
    double Ts = 0;     // 重心時間 [ms] (ISO 3382-1 A.2.5)
    double Glate = 0;  // 後期 (80ms 以降) エネルギーの strength [dB]
};
SeatMetrics seatMetrics(double r, double T, double V);

// 複数音源版 (拡声系)。r[i] = 各音源までの距離 [m]、gainDb[i] = 相対ゲイン [dB]。
// 各音源が独立に Barron の場を作るとしてエネルギー加算する。
// **無指向近似** — スピーカーの指向性データ (GLL) を持たないため。
// G / Glate は「基準音源1台 (0 dB)」に対する相対値であり絶対 SPL ではない。
SeatMetrics seatMetrics(const double *r, const double *gainDb, int n,
                        double T, double V);

// ── Schroeder 減衰曲線と ISO 3382-1 の減衰時間 ──────────────────────────
// 統計モデルの逆積分曲線 L(t) = 10·log₁₀(E(t)/E(0)):
//   E(t) = D·1{t≤0} + R·exp(−13.8·t/T)     (D = 直接音, R = 残響の全エネルギー)
// 先頭点は (0, 0 dB)、t>0 は直接音が抜けた分だけ落ちた直線になる。
QVector<QPointF> schroederCurve(double r, double T, double V,
                                double tMax, int nPoints);

// 減衰曲線 (x = 時刻 [s], y = レベル [dB], 単調減少) の fromDb..toDb 区間に
// 最小二乗直線を当て、60 dB 減衰へ外挿した時間 [s] を返す
// (ISO 3382-1:2009 6.3 / A.2.2)。評価区間に 3 点未満しか無い、曲線が toDb に
// 到達しない、傾きが非負 のときは 0 (= 評価不能) を返す。
double decayTimeFromCurve(const QVector<QPointF> &curve,
                          double fromDb, double toDb);

struct DecayTimes {
    double EDT = 0;    //   0 … −10 dB の回帰 ×6
    double T20 = 0;    //  −5 … −25 dB の回帰 ×3
    double T30 = 0;    //  −5 … −35 dB の回帰 ×2
    bool   valid = false;   // 3 つとも評価できた
};
DecayTimes decayTimes(const QVector<QPointF> &curve);

// ── 初期側方エネルギー比 LF / LFC (ISO 3382-1:2009 A.2.6) ───────────────
//   LF  = ∫₅^80 p_L² dt / ∫₀^80 p² dt        (p_L = 8 字マイク出力 ∝ cosθ)
//   LFC = ∫₅^80 |p_L·p| dt / ∫₀^80 p² dt
// θ は音源→受音点の水平軸に直交する水平軸 (側方軸) からの角度。
// **1次鏡像法のエコーグラムのみを用いた幾何推定** で、2次以上の反射・
// 拡散反射は含まない (UI 上でその旨を明示すること)。
struct LateralEnergy {
    double LF = 0;
    double LFC = 0;
    int    nEarly = 0;   // 分母に入った 80ms 以内の1次反射の数
    bool   valid = false;
};
LateralEnergy lateralEnergy(const AcousticOpts &a,
                            const double src[3], const double rcv[3]);

// ── 拡声系 ──────────────────────────────────────────────────────────────
// 音速 [m/s] (乾燥空気): c = 331.3·√(1 + t/273.15)  — ISO 9613-1:1993
double soundSpeed(double tempC = 20.0);

// ディレイスピーカーの整合遅延 [ms]。Δt = (dFar − dNear)/c。
// dFar < dNear (= 遅延不要) のときは 0 を返す。
double alignmentDelayMs(double dFar, double dNear, double tempC = 20.0);

// PAG / NAG によるハウリング余裕。
//   D. Davis & E. Patronis, "Sound System Engineering", 3rd ed.,
//   Focal Press (2006), Ch. "Acoustic Gain" の音響利得式:
//     NAG = 20·log₁₀(D0/EAD)
//     PAG = 20·log₁₀(D0) + 20·log₁₀(Ds) − 20·log₁₀(D1) − 20·log₁₀(D2)
//           − 10·log₁₀(NOM) − FSM
//   D0 = 話者→最遠聴取点, D1 = 話者→マイク, D2 = スピーカー→最遠聴取点,
//   Ds = スピーカー→マイク, EAD = 等価音響距離, NOM = 開マイク数,
//   FSM = 安定余裕 (慣用 6 dB)。margin = PAG − NAG (>0 で成立)。
struct GainBeforeFeedback {
    double D0 = 0, D1 = 0, D2 = 0, Ds = 0;   // [m]
    double EAD = 0;                          // [m]
    int    NOM = 1;
    double FSM = 0;                          // [dB]
    double NAG = 0, PAG = 0, margin = 0;     // [dB]
    bool   valid = false;
};
GainBeforeFeedback pagNag(double D0, double D1, double D2, double Ds,
                          int NOM = 1, double EAD = 2.4, double FSM = 6.0);

// ── エコーグラム (シューボックス1次鏡像法) ─────────────────────────────
struct Reflection {
    double  timeMs = 0;    // 直接音を 0 とした相対到達時刻
    double  levelDb = 0;   // 直接音を 0 dB とした相対レベル
    QString surface;       // 床/天井/側壁L/側壁R/舞台側/後壁 ("" = 直接音)
    bool    early = false; // 80ms 以内
    double  dir[3] = { 0, 0, 0 };  // 受音点への到来方向 (単位ベクトル)
};
// src/rcv は室内座標 [0..L]×[0..W]×[0..H]。先頭要素は直接音 (time=0)。
QVector<Reflection> echogram(const AcousticOpts &a,
                             const double src[3], const double rcv[3]);
double itdgMs(const QVector<Reflection> &refl);   // 初期時間遅れ間隙

// ── NC 評価 ────────────────────────────────────────────────────────────
// levels: 63,125,250,500,1k,2k,4k Hz のオクターブ帯域騒音レベル [dB]。
// 戻り値: NC 値 (タンジェント法, 15..70 に丸め、範囲外は端値)。
int ncRating(const double levels[7]);
// NC-XX 基準曲線の帯域値 (プロット用)。nc は 15..70 の5刻み。
QVector<double> ncCurve(int nc);

// ── 音響障害検出 ────────────────────────────────────────────────────────
struct Defect {
    QString name;      // フラッターエコー / ロングディレイエコー …
    QString place;
    QString cause;
    int     severity;  // 0=低, 1=中, 2=高
};
QVector<Defect> detectDefects(const AcousticOpts &a,
                              const double src[3], const double rcv[3]);

} // namespace roomac
} // namespace ofd
