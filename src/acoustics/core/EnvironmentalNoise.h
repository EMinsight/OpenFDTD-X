// EnvironmentalNoise.h — 屋外騒音伝搬の初等計算 (Qt 非依存 / C++14)。
//
// 屋外騒音タブ (OutdoorNoiseTab) が使う計算実体。GUI に式を書かず、
// selftest から解析解・規格の値と直接突き合わせられるようにここへ置く。
//
// 収録するもの (すべて公表された式・規格):
//   - 幾何拡散 A_div (ISO 9613-2:1996 §7.1)
//       点音源 A_div = 20·lg(d/d0) + 11 dB     (d0 = 1 m, 全空間放射)
//       線音源 A_div = 10·lg(d/d0) +  8 dB     (無限長非干渉線音源)
//     → 距離 2 倍で点音源 −6.02 dB / 線音源 −3.01 dB
//   - 薄板遮蔽物の回折減衰 (前川チャート)
//       ΔL = 10·lg(3 + 20N)  [dB],  N = 2δ/λ (フレネル数), δ = 経路差
//     (Z. Maekawa, "Noise reduction by screens", Applied Acoustics 1(3),
//      157-173, 1968。半無限薄板・点音源に対する実測整理式)
//   - 日本の騒音に係る環境基準の基準値
//     (平成 10 年 9 月 30 日 環境庁告示第 64 号「騒音に係る環境基準について」。
//      評価量は等価騒音レベル LAeq、昼間 6:00-22:00 / 夜間 22:00-6:00)
//
// 含まないもの (未実装 — UI 側で明示すること):
//   空気吸収 A_atm、地面効果 A_gr、植栽・住宅群 A_misc、気象補正 C_met、
//   および交通量・車速から音響パワーレベルを求める発生源モデル
//   (ASJ RTN-Model / CNOSSOS-EU など)。
#pragma once

namespace ofd {
namespace acoustics {
namespace outdoor {

// ── 幾何拡散 (ISO 9613-2:1996 §7.1) ─────────────────────────────────────────
// 基準距離 d0 = 1 m からの減衰量 [dB]。距離が非正なら 0 を返す。
double divergencePoint(double distM);   // 20·lg(d) + 11
double divergenceLine(double distM);    // 10·lg(d) +  8

// 基準距離 refDistM における既知レベルからの相対減衰 [dB]:
//   点音源 20·lg(d/dref)、線音源 10·lg(d/dref)
// どちらかが非正なら 0 を返す。
double divergenceRelative(double distM, double refDistM, bool lineSource);

// ── 前川チャートによる回折減衰 ──────────────────────────────────────────────
// ΔL の実用上限 [dB]。前川のチャートは N が大きい領域で頭打ちになり、
// ISO 9613-2 も単一回折の上限を 20 dB としている。ここでは前川のチャートの
// 到達値である 24 dB で頭打ちにする。
extern const double kMaekawaMaxDb;

// フレネル数 N = 2δ/λ。λ = c/f。f または c が非正なら 0。
double fresnelNumber(double pathDiffM, double freqHz, double soundSpeedMs);

// 前川の整理式 ΔL = 10·lg(3 + 20N) [dB]。
// N <= 0 (受音点が見通し領域) では 0 を返す — 遮蔽が成立していない場合に
// 減衰を与えないための保守側の扱い (ISO 9613-2 も見通しがある場合は
// 回折減衰を計上しない)。上限は kMaekawaMaxDb。
double maekawaAttenuation(double fresnelN);

// 断面 2 次元の遮蔽幾何 (地面を y = 0 とする鉛直断面)。
// 音源を x = 0 に置き、x 軸正の向きに受音点がある。
struct BarrierGeometry {
    double srcHeightM;    // 音源高さ [m]
    double barDistM;      // 音源から壁までの水平距離 [m]
    double barHeightM;    // 壁の頂部高さ [m]
    double recvDistM;     // 音源から受音点までの水平距離 [m]
    double recvHeightM;   // 受音点高さ [m]

    BarrierGeometry()
        : srcHeightM(0), barDistM(0), barHeightM(0),
          recvDistM(0), recvHeightM(0) {}
};

struct BarrierResult {
    bool   valid;         // 幾何が成立している (0 < barDist < recvDist など)
    bool   shadow;        // 壁が音源-受音点の見通し線を遮っている
    double pathDiffM;     // 経路差 δ = (A+B) − d [m] (見通しのときは負)
    double wavelengthM;   // λ [m]
    double fresnelN;      // N = 2δ/λ
    double attenDb;       // ΔL [dB] (見通しのときは 0)
    bool   clamped;       // kMaekawaMaxDb で頭打ちになった

    BarrierResult()
        : valid(false), shadow(false), pathDiffM(0), wavelengthM(0),
          fresnelN(0), attenDb(0), clamped(false) {}
};

// 薄板遮蔽物の回折減衰。頂部形状 (Y 型・枝付き・吸音型) の付加効果は
// 製品ごとの実測値が要るため含まない (直壁として計算する)。
BarrierResult barrierDiffraction(const BarrierGeometry &g, double freqHz,
                                 double soundSpeedMs);

// ── 日本の環境基準 (平成 10 年環境庁告示第 64 号) ───────────────────────────
enum AreaType {
    AreaAA        = 0,   // AA: 療養施設・社会福祉施設等が集合し特に静穏を要する地域
    AreaA         = 1,   // A : 専ら住居の用に供される地域
    AreaB         = 2,   // B : 主として住居の用に供される地域
    AreaC         = 3,   // C : 相当数の住居と併せて商業・工業等の用に供される地域
    AreaRoadA     = 4,   // 道路に面する地域: A 地域のうち 2 車線以上
    AreaRoadBC    = 5,   // 道路に面する地域: B 地域のうち 2 車線以上 / C 地域のうち車線を有する
    AreaRoadTrunk = 6,   // 幹線交通を担う道路に近接する空間
    AreaTypeCount = 7
};

struct EnvStandard {
    bool   valid;
    double dayDb;     // 昼間 (6:00-22:00) の基準値 LAeq [dB]
    double nightDb;   // 夜間 (22:00-6:00) の基準値 LAeq [dB]

    EnvStandard() : valid(false), dayDb(0), nightDb(0) {}
};

// 地域類型 (AreaType) に対する基準値。未知の類型では valid = false。
EnvStandard environmentalStandardJp(int areaType);

// ── 断面予測モデル ──────────────────────────────────────────────────────────
// 基準距離における既知レベル (実測値・カタログ値・音響パワーからの換算値) を
// 出発点に、幾何拡散と回折減衰だけで受音点レベルを求める簡易断面モデル。
// 評価量 (LAeq / SPL) は基準レベルに与えたものがそのまま予測値の評価量になる。
struct SiteModel {
    double refLevelDb;        // 基準距離での音圧レベル [dB]
    double refDistM;          // 基準距離 [m] (> 0)
    bool   lineSource;        // true = 線音源 (−3 dB/dd), false = 点音源 (−6 dB/dd)
    double srcHeightM;        // 音源高さ [m]
    bool   divergenceEnabled; // 幾何拡散 A_div を計上する (既定 true)
    bool   barrierEnabled;    // 遮音壁の回折減衰 A_bar を計上する
    double barDistM;          // 音源から壁までの水平距離 [m]
    double barHeightM;        // 壁の頂部高さ [m]
    double evalFreqHz;        // 回折減衰の評価周波数 [Hz]
    double soundSpeedMs;      // 音速 [m/s]

    SiteModel()
        : refLevelDb(0), refDistM(1.0), lineSource(false), srcHeightM(0),
          divergenceEnabled(true), barrierEnabled(false), barDistM(0),
          barHeightM(0), evalFreqHz(500.0), soundSpeedMs(343.2) {}
};

struct PredictionResult {
    bool          valid;
    double        levelDb;   // 予測レベル [dB]
    double        aDivDb;    // 幾何拡散による減衰 [dB]
    double        aBarDb;    // 回折減衰 [dB]
    BarrierResult barrier;

    PredictionResult() : valid(false), levelDb(0), aDivDb(0), aBarDb(0) {}
};

// 受音点距離 recvDistM・高さ recvHeightM における予測レベル。
//   L = L_ref − A_div − A_bar
PredictionResult predictLevel(const SiteModel &m, double recvDistM,
                              double recvHeightM);

// L(d) = targetDb となる距離 [m] を [dMinM, dMaxM] で二分探索する。
// L(d) は距離に対し単調非増加であることを前提とする。
// 区間内で target を挟めない場合は 0 を返す。
double distanceForLevel(const SiteModel &m, double targetDb,
                        double recvHeightM, double dMinM, double dMaxM);

// 音源からの音響パワーレベル L_W [dB] を距離 1 m の音圧レベルへ換算する
// (ISO 9613-2 §7.1 の全空間放射: L_p = L_W − 11 dB)。
double pointSourceLevelAt1m(double pwlDb);

} // namespace outdoor
} // namespace acoustics
} // namespace ofd
