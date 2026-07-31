// RoomAcoustics.h — 室内音響の統計/幾何モデル (room-acoustics.jsx の実体)。
//
// FDTD 実行前の設計段階で使う標準的な統計手法を実装する:
//   - Sabine / Eyring 残響時間 (吸音バジェット, 帯域別)
//   - Barron 修正理論による客席の G / C80 / C50 / D50 推定
//   - MTF (Houtgast–Steeneken) による STI 推定 (直接音+指数残響, 無騒音)
//   - シューボックス1次鏡像法によるエコーグラム (反射音列, ITDG)
//   - NC (Noise Criteria) 曲線による暗騒音評価
//   - 音響障害の自動検出 (フラッターエコー / ロングディレイエコー)
//
// いずれも「推定」であり、FDTD/幾何音響カーネルの結果を置き換えるものではない
// (UI 上もその旨を表示する)。
#pragma once
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
};
SeatMetrics seatMetrics(double r, double T, double V);

// ── エコーグラム (シューボックス1次鏡像法) ─────────────────────────────
struct Reflection {
    double  timeMs = 0;    // 直接音を 0 とした相対到達時刻
    double  levelDb = 0;   // 直接音を 0 dB とした相対レベル
    QString surface;       // 床/天井/側壁L/側壁R/舞台側/後壁 ("" = 直接音)
    bool    early = false; // 80ms 以内
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
