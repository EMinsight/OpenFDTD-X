// RoomModes.h — 直方体室 (剛壁) の音響固有モード (Qt 非依存 / C++14)。
//
// 車内音響タブ (CabinAcousticsTab) が使う「車室の音響モード周波数」の計算実体。
// GUI に式を書かず、selftest から 1 次元極限などの解析解と直接突き合わせられる
// ようにするためここへ置く。
//
// 収録するもの (すべて古典的な閉形式解):
//   - 剛壁直方体室の固有周波数
//       f(nx,ny,nz) = (c/2)·√((nx/Lx)² + (ny/Ly)² + (nz/Lz)²)
//     (Lord Rayleigh, "The Theory of Sound", Vol. II, §267, 2nd ed. 1896。
//      P. M. Morse & K. U. Ingard, "Theoretical Acoustics", §9.4, 1968 に
//      同じ形で再掲。剛壁 (∂p/∂n = 0) の Helmholtz 方程式の変数分離解)
//   - モードの種別 (軸 / 接線 / 斜め) — 非零次数の個数で決まる
//   - 乾燥空気の音速 c = 331.3·√(1 + t/273.15)  (ISO 9613-1:1993)
//
// 剛壁・直方体・無損失の理想化に基づく「近似」であり、実際の車室 (曲面・座席・
// 吸音内装) の共鳴周波数とは差が出る。UI 側でその旨を表示すること。
#pragma once
#include <vector>

namespace ofd {
namespace acoustics {
namespace roommodes {

// モード種別 = 非零次数の個数。
//   軸 (axial)      : 1 対の平行面のみで往復 (最も強い)
//   接線 (tangential): 4 面
//   斜め (oblique)   : 6 面すべて
enum ModeKind {
    ModeAxial      = 1,
    ModeTangential = 2,
    ModeOblique    = 3
};

struct Mode {
    int    nx, ny, nz;   // モード次数 (0 以上、同時に 0 は不可)
    double freqHz;       // 固有周波数 [Hz]
    int    kind;         // ModeKind

    Mode() : nx(0), ny(0), nz(0), freqHz(0), kind(0) {}
};

// 乾燥空気の音速 [m/s] : c = 331.3·√(1 + t/273.15)   (ISO 9613-1:1993)
// t <= −273.15 ℃ では 0 を返す。
double soundSpeed(double tempC);

// 1 モードの固有周波数 [Hz]。寸法・音速が非正、または次数が全て 0 のときは 0。
double modeFrequency(int nx, int ny, int nz,
                     double lengthM, double widthM, double heightM,
                     double soundSpeedMs);

// fMaxHz 以下の固有モードを周波数の昇順で列挙する。
// maxModes > 0 のときは低い方から maxModes 個で打ち切る。
// 入力が非正 (寸法・音速・上限周波数のいずれか) のときは空を返す。
// 生成数には内部上限 (kEnumerationLimit) があり、超えた場合は
// 低次側から上限個までを返す (GUI を固まらせないため)。
std::vector<Mode> rectangularModes(double lengthM, double widthM,
                                   double heightM, double soundSpeedMs,
                                   double fMaxHz, int maxModes);

// rectangularModes の内部生成上限 (これを超えるモードは列挙しない)
extern const int kEnumerationLimit;

// ── 帰還 (ハウリング) 対策の notch 候補 ──────────────────────────────────
// 拡声系のリンギングは、系のループ利得が突出する周波数で最初に起こる。
// **どの周波数で鳴くかはマイク・スピーカの位置と指向性を含むループ応答で
// 決まり、ここでは予測しない。** ここが返すのは「室のモードのうち、
// 通常まず notch の候補になるもの」であって、鳴く周波数の予測ではない。
// UI 側でその区別を必ず明示すること。
//
// シュレーダー周波数より上は列挙しない — そこから上はモードが密に重なって
// 個別のモードが応答の山として立たなくなるため (統計的な領域)。
//
// 提案 Q はモードの半値幅から出す。減衰時間 T60 のモードの −3 dB 帯域幅は
//   Δf = 2.2 / T60   [Hz]
// (H. Kuttruff, "Room Acoustics", 5th ed., §3.3。残響減衰 e^{−13.8t/T60} の
//  ローレンツ形スペクトルの半値全幅) なので Q = f / Δf = f·T60 / 2.2。

struct NotchCandidate {
    Mode   mode;            // もとになったモード (最も低い次数の代表)
    double freqHz;          // モードの固有周波数
    double q;               // 提案 Q = f·T60/2.2
    double bandwidthHz;     // Δf = 2.2/T60
    int    coincident;      // 半値幅の中に重なるモードの数 (1 = 単独)

    NotchCandidate() : freqHz(0), q(0), bandwidthHz(0), coincident(1) {}
};

// シュレーダー周波数 [Hz] : f_s = 2000·√(T60 / V)
// (M. R. Schroeder, JASA 1962。T60 [s], V [m³])。入力が非正なら 0。
double schroederFrequency(double rt60S, double volumeM3);

// notch 候補をシュレーダー周波数以下で周波数の昇順に返す。
// maxCount > 0 のときは低い方から maxCount 個で打ち切る。
// 寸法・音速・T60 のいずれかが非正なら空を返す。
std::vector<NotchCandidate> notchCandidates(double lengthM, double widthM,
                                            double heightM,
                                            double soundSpeedMs,
                                            double rt60S, int maxCount);

} // namespace roommodes
} // namespace acoustics
} // namespace ofd
