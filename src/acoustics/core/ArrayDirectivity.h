// ArrayDirectivity.h — ラインアレイ / サブアレイの遠方界指向性 (Qt 非依存 / C++14)
//
// 音源タブ「アレイ・ライン音源」ページの設定 (素子数・素子間隔・splay 角・
// ステアリング・サブの配置と遅延) から、**実際に鳴る指向性**を合成する。
// 外部ソルバーは要らない — 素子位置と遅延が決まれば遠方界は和で書ける。
//
// 座標系は鉛直面内の 2 次元 (ラインアレイの効く面):
//   x = 前方 [m]、z = 上方 [m]、観測角 θ は水平から**下向き**を正 [deg]。
//   観測方向の単位ベクトルは (cosθ, −sinθ)。
//
// 遠方界 (振幅は距離で割った後の相対値):
//   p(θ) = Σ_k g_k · D(θ − φ_k) · exp(i·k₀·û(θ)·p_k) · exp(−i·ω·τ_k)
// φ_k は素子 k の下向き傾き、τ_k は電気遅延、D は素子単体の指向性。
// 素子は高さ h の連続線音源とみなし D(ψ) = sinc(π·h·sinψ/λ) (h = 0 で無指向性)。
//
// splay 角は「1 つ上の箱に対する相対角」で与える (EASE / ArrayCalc と同じ流儀)。
// 箱は 1 つ前の箱の面に沿って下へ積むので、J カーブでは下の箱ほど前に出る。
#ifndef OFD_ACOUSTICS_ARRAYDIRECTIVITY_H
#define OFD_ACOUSTICS_ARRAYDIRECTIVITY_H

#include <vector>

namespace ofd {
namespace acoustics {

// 1 素子 (キャビネット 1 箱)
struct ArrayElement {
    double x = 0.0;         // 位置 (前方) [m]
    double z = 0.0;         // 位置 (上方) [m]
    double tiltDeg = 0.0;   // 素子軸の下向き角 [deg]
    double delay_s = 0.0;   // 電気遅延 [s]
    double gain = 1.0;      // 振幅重み (1 = 等振幅)
};

struct ArrayPattern {
    bool   valid = false;
    std::vector<double> deg;   // 観測角 [deg] (下向きが正)
    std::vector<double> db;    // 最大を 0 dB とした正規化レベル
    double peakDeg = 0.0;      // 最大方向 [deg]
};

// 開いた角度範囲 (周回しない) のビーム指標。
// em/PatternMetrics は 0〜360° を周回するパターン用なので別に持つ。
struct BeamMetrics {
    double hpbwDeg = 0.0;   // −3 dB 全幅 [deg]
    double sllDb = 0.0;     // 主ビーム外の最大 (ピーク基準 = 負値)
    double sllDeg = 0.0;    // その方向
    bool   hasHpbw = false; // 両側で −3 dB を跨いだか
    bool   hasSll = false;  // 主ビームの外にローブが見つかったか
    // −6 dB 以上が及ぶ角度の下端・上端。**splay を付けたアレイでは主ローブの
    // −3 dB 幅は「客席のカバー範囲」ではない** (J カーブでも主ローブ自体は
    // 細いままで、カバーは複数ローブの包絡で作られる) ので、こちらを併記する。
    double coverageMinDeg = 0.0;
    double coverageMaxDeg = 0.0;
    bool   hasCoverage = false;
};

// 遠方界パターン。elementHeight_m = 0 なら素子は無指向性。
ArrayPattern beamPattern(const std::vector<ArrayElement> &els,
                         double freqHz, double soundSpeed,
                         double elementHeight_m,
                         double degMin = -90.0, double degMax = 90.0,
                         int nAngles = 361);

// パターンから −3 dB 幅とサイドローブレベルを読む (範囲の端では折り返さない)
BeamMetrics beamMetrics(const ArrayPattern &p);

// N 素子のラインアレイを作る。
//   spacing_m : 箱の高さ (= 素子間隔)
//   splayDeg  : 箱ごとの相対 splay 角 [deg] (足りない分は最後の値を繰り返す。
//               空なら全て 0 = ストレート)
//   steerDeg  : 電気ステアリング角 [deg] (下向き正)。遅延 τ_k = û_s·p_k/c
std::vector<ArrayElement> buildLineArray(int n, double spacing_m,
                                         const std::vector<double> &splayDeg,
                                         double steerDeg, double soundSpeed);

// グレーティングローブが現れ始める周波数 [Hz]:  f = c / (d·(1 + |sinθ_s|))
// (素子間隔が λ/(1+|sinθs|) を超えると可視領域に 2 本目の主極大が入る)
// spacing <= 0 なら 0 を返す。
double gratingLobeFreq(double spacing_m, double steerDeg, double soundSpeed);

// 2 素子サブアレイ: 前の箱を x = 0、後ろの箱を x = −d に置き、**後ろの箱に**
// 遅延 τ を与える (音源タブの「後方リア遅延」と同じ意味)。
// reversePolarity は後ろの箱の逆相 (「リアを逆相にする」チェック)。
//
//   前方 (+x): 後ろの箱は d/c 遅れて届く → 位相差 ω(d/c + τ)
//   後方 (−x): 後ろの箱は d/c 早く届く   → 位相差 ω(τ − d/c)
//   p = |1 + s·exp(−i·位相差)|,  s = 逆相なら −1
//
// 逆相 + τ = d/c で後方は**全周波数で**打ち消され (カージオイド)、
// 前方は 2|sin(2π·d/λ)| なので d = λ/4 で最大 (+6 dB) になる。
struct EndfireResult {
    bool   valid = false;
    double frontDb = 0.0;        // 前方の和 [dB] (素子 1 個を 0 dB とする)
    double backDb = 0.0;         // 後方の和 [dB]
    double frontBackDb = 0.0;    // 前後比 [dB]
    double optimumDelay_s = 0.0; // 後方を消す遅延 (逆相なら d/c)
    double bestFreqHz = 0.0;     // 前方が最大になる周波数 (d = λ/4)
};
EndfireResult endfire(double spacing_m, double delay_s, double freqHz,
                      double soundSpeed, bool reversePolarity);

} // namespace acoustics
} // namespace ofd

#endif // OFD_ACOUSTICS_ARRAYDIRECTIVITY_H
