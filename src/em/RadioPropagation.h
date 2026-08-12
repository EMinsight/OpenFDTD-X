// RadioPropagation.h — 電波伝搬リンクバジェットの確立モデル (Qt 非依存 / C++17)
//
// ChannelTab (電波伝搬・チャネル解析) が表示する数値の計算実体。
// **見通し内 (LOS) の解析モデルだけ**を扱う。建物透過・散乱・多重反射を含む
// チャネル (RMS 遅延スプレッド・角度スプレッドなど) はレイトレース /  FDTD の
// 実行が必要で、ここでは扱わない (GUI 側で「未計算」と表示する)。
//
// 出典 (式はすべて公刊のもの):
//   [1] H. T. Friis, "A note on a simple transmission formula",
//       Proc. IRE 34, 254-256 (1946)。自由空間基本伝送損
//       L_fs = (4πd/λ)²。
//   [2] T. S. Rappaport, "Wireless Communications: Principles and Practice",
//       2nd ed., Prentice Hall (2002), §4.6 (two-ray ground reflection model)
//       と §4.11.3 (経路損失指数)。
//   [3] ITU-R P.525-4 (2019) — 自由空間伝搬の基本式
//       L[dB] = 32.44 + 20log10(f[MHz]) + 20log10(d[km])。
//   [4] C. E. Shannon, "A mathematical theory of communication",
//       Bell Syst. Tech. J. 27, 379-423 (1948)。C = B·log2(1+S/N)。
//   [5] IEEE Std 1149 系で慣用の標準雑音温度 T0 = 290 K。
//       雑音電力 N = kT0B (k = 1.380649e-23 J/K, CODATA 2018)。
//   [6] C. A. Balanis, "Antenna Theory: Analysis and Design", 4th ed.,
//       Wiley (2016), §6.3 (N 素子等間隔アレイの指向性)。
//   [7] I. E. Telatar, "Capacity of multi-antenna Gaussian channels",
//       Eur. Trans. Telecommun. 10(6), 585-595 (1999)。
//   [8] G. J. Foschini and M. J. Gans, "On limits of wireless communications
//       in a fading environment when using multiple antennas",
//       Wireless Pers. Commun. 6, 311-335 (1998)。
//
// 適用範囲 (GUI にも明示すること):
//   - two-ray モデルは「平面大地・完全反射 (Γ = −1)・等方アンテナ」を仮定した
//     2 波の干渉。市街地の建物回折や屋内の壁透過は含まない。
//   - 干渉のヌル点では損失が発散するので、損失には上限 (kMaxPathLossDb) を置く。
#ifndef OFD_EM_RADIOPROPAGATION_H
#define OFD_EM_RADIOPROPAGATION_H

#include <vector>

namespace ofd {
namespace em {
namespace propagation {

constexpr double kC0 = 2.99792458e8;        // 真空中の光速 [m/s]
constexpr double kBoltzmann = 1.380649e-23; // ボルツマン定数 [J/K] (CODATA 2018)
constexpr double kT0 = 290.0;               // 標準雑音温度 [K] ([5])
constexpr double kMaxPathLossDb = 300.0;    // 干渉ヌルの発散を切る上限

// 波長 λ = c/f [m]。f ≤ 0 なら 0。
double wavelength(double freq_hz);

// 自由空間基本伝送損 L = 20·log10(4πd/λ) [dB] ([1][3])。
// 距離・周波数が非正なら 0。d < λ/4π (近傍界) でも式は評価するが、
// 遠方界の式であることに注意 (GUI 側で距離の下限を案内する)。
double freeSpacePathLossDb(double dist_m, double freq_hz);

// 2 波モデル (直接波 + 大地反射波) の経路損失 [dB] ([2] §4.6)。
//   P_r/P_t = (λ/4π)²·|e^{-jk·d1}/d1 + Γ·e^{-jk·d2}/d2|²
//   d1 = sqrt(d² + (ht−hr)²)、d2 = sqrt(d² + (ht+hr)²)
// reflection は反射係数の**大きさ** (完全反射 = 1)。gammaSign は反射係数の
// 符号で、既定の −1 は位相 π の反転 (水平偏波、あるいは grazing 入射)。
// **完全導体面では水平偏波が Γ = −1、垂直偏波が Γ = +1** になる (境界条件
// から厳密にそうなる) ので、垂直偏波は gammaSign = +1 を渡す。
// アンテナ利得は 0 dBi。
double twoRayPathLossDb(double dist_m, double hTx_m, double hRx_m,
                        double freq_hz, double reflection = 1.0,
                        double gammaSign = -1.0);

// ブレークポイント距離 d_bp = 4·ht·hr/λ [m] ([2] §4.6)。
// これより遠方では 2 波モデルの損失が距離の 4 乗 (n = 4) で増える。
double breakpointDistance(double hTx_m, double hRx_m, double freq_hz);

// 2 点の経路損失から求める局所の経路損失指数
//   n = (L2 − L1) / (10·log10(d2/d1))
// 自由空間なら厳密に 2。d1, d2 が非正または等しいときは 0。
double pathLossExponent(double loss1Db, double d1_m, double loss2Db, double d2_m);

// 2 波モデルのライシアン K ファクタ [dB]
//   K = (直接波電力)/(反射波電力) = (d2/(d1·|Γ|))²
// 完全反射・遠距離では d1 ≈ d2 なので 0 dB に漸近する。
double twoRayKFactorDb(double dist_m, double hTx_m, double hRx_m,
                       double reflection = 1.0);

// 2 波モデルの遅延差 τ = (d2 − d1)/c [s]。
// これは 2 波だけの余剰遅延であり、実チャネルの RMS 遅延スプレッドではない。
double twoRayExcessDelay(double dist_m, double hTx_m, double hRx_m);

// 受信電力 [dBm] = EIRP[dBm] − 経路損失[dB] + 受信利得[dBi]
double receivedPowerDbm(double eirpDbm, double pathLossDb, double rxGainDbi);

// 熱雑音電力 [dBm] = 10·log10(k·T0·B / 1 mW) + NF ([5])
// B ≤ 0 なら 0。
double thermalNoiseDbm(double bandwidth_hz, double noiseFigureDb);

// Shannon 容量 [bit/s] = B·log2(1 + SNR) ([4])。B ≤ 0 なら 0。
// SISO (単一入出力) の上限であり、MIMO の多重利得は含まない。
double shannonCapacity(double bandwidth_hz, double snrDb);

// N 素子アレイの最大アレイ利得 [dB] = 10·log10(N) ([6] §6.3)。
// 素子が無損失・等間隔・同振幅で、ボアサイト方向へ同相合成した場合の上限。
// **単一素子の利得に対する増分**なので、EIRP に既にアレイ分が入っていれば
// 二重計上になる (呼び出し側で明示すること)。N < 1 なら 0。
double arrayGainDb(int elements);

// 空間多重 MIMO の容量 [bit/s] ([7] eq.(7), [8])。
//   C = min(Nt, Nr)·B·log2(1 + SNR/Nt)
// 送信電力を Nt 本へ等分し、min(Nt,Nr) 本の等利得な固有モードが立つと
// 仮定した**上限**。実チャネルの相関・ランク落ちは含まない。
// Nt = Nr = 1 なら shannonCapacity と一致する。
double mimoCapacity(double bandwidth_hz, double snrDb, int nTx, int nRx);

// ── 環境別の経験式 (見通し外を含む) ────────────────────────────────────────
//
// 上の Friis / 2 波は見通し内の解析モデルで、市街地の建物回折や屋内の壁透過は
// 表せない。ここでは**公刊の経験式**を実装する。経験式は測定データの当てはめ
// なので、**適用範囲の外では使ってはならない** — 範囲判定を別関数で出し、
// GUI 側で警告できるようにしてある (黙って外挿しない)。
//
//   [9]  Y. Okumura et al., Rev. Elec. Commun. Lab. 16, 825 (1968) と
//        M. Hata, "Empirical formula for propagation loss in land mobile
//        radio services", IEEE Trans. Veh. Technol. 29(3), 317-325 (1980)。
//        奥村-秦 (Okumura-Hata) の市街地・郊外・開放地の式。
//   [10] COST Action 231 Final Report, "Digital mobile radio towards future
//        generation systems", EUR 18957 (1999), §4.1.2 (COST-231 Hata)。
//        1500-2000 MHz への拡張。
//   [11] ITU-R P.1238-11 (2023) — 屋内の距離損失係数モデル
//        L = 20log10(f[MHz]) + N·log10(d[m]) − 28 + Lf。

// 対数距離モデル L = L_fs(d0) + 10·n·log10(d/d0) [dB] ([2] §4.11.3)。
// n = 2 なら自由空間と厳密に一致する (この恒等式を selftest で検証している)。
double logDistancePathLossDb(double dist_m, double freq_hz, double exponent,
                             double d0_m = 1.0);

// ITU-R P.1238 屋内 ([11])。distCoef は距離損失係数 N (自由空間は 20、
// 屋内オフィスの代表値は 30)。floorLossDb は階層貫通損 Lf [dB]。
double indoorP1238PathLossDb(double dist_m, double freq_hz, double distCoef,
                             double floorLossDb = 0.0);

// 奥村-秦 ([9])。hb = 基地局高 [m]、hm = 移動局高 [m]。
// largeCity = true で大都市の移動局高補正 a(hm) を使う。
double hataUrbanPathLossDb(double dist_m, double freq_hz,
                           double hb_m, double hm_m, bool largeCity = false);
// 郊外  = 市街地 − 2·[log10(f_MHz/28)]² − 5.4        ([9] 定義式)
double hataSuburbanPathLossDb(double dist_m, double freq_hz,
                              double hb_m, double hm_m);
// 開放地 = 市街地 − 4.78·(log10 f)² + 18.33·log10 f − 40.94   ([9] 定義式)
double hataOpenPathLossDb(double dist_m, double freq_hz,
                          double hb_m, double hm_m);
// COST-231 Hata ([10])。cityCorrectionDb は都市補正 C (中小都市/郊外 = 0、
// 大都市 = 3 dB)。
double cost231HataPathLossDb(double dist_m, double freq_hz,
                             double hb_m, double hm_m,
                             double cityCorrectionDb = 0.0,
                             bool largeCity = false);

// 適用範囲の判定 (経験式の外挿を黙って行わないため)。
//   奥村-秦    : f 150-1500 MHz, hb 30-200 m, hm 1-10 m, d 1-20 km
//   COST-231   : f 1500-2000 MHz, 他は同じ
bool hataApplicable(double dist_m, double freq_hz, double hb_m, double hm_m);
bool cost231Applicable(double dist_m, double freq_hz, double hb_m, double hm_m);

// 受信電力のカバレッジ格子 (受信点を「格子」にしたときの表示の実体)。
//
// 送信点を格子の中心に置き、受信点を水平面 (x, y) 上へ並べて 2 波モデルで
// 受信電力 [dBm] を求める。**アンテナ指向性は含まない** (等方 + 受信利得の
// 一定加算)。したがって等方円対称になり、遮蔽物の影は出ない — 見通し内の
// 距離依存を見るための図であることを画面に明示すること。
struct CoverageGrid {
    int n = 0;                  // 1 辺の点数 (n×n)
    double halfSpan_m = 0.0;    // 中心からの半幅 [m]
    std::vector<double> dbm;    // n*n, row-major。[0] は (x,y) = (−half, −half)
    double minDbm = 0.0, maxDbm = 0.0;

    bool valid() const
    {
        return n > 0 && halfSpan_m > 0.0
            && dbm.size() == std::size_t(n) * std::size_t(n);
    }
    // (ix, iy) の座標 [m] (中心が 0)
    double coord(int i) const
    {
        return (n > 1) ? (-halfSpan_m + 2.0 * halfSpan_m * i / (n - 1)) : 0.0;
    }
};

// カバレッジ格子を作る。距離は minDistance_m で下限を切る (原点は距離 0 で
// 発散するため、また 2 波モデルは遠方界の式なので近すぎる点は意味を持たない)。
// n ≤ 0 / halfSpan ≤ 0 / 周波数 ≤ 0 なら valid() == false の空を返す。
CoverageGrid coverageMap(double halfSpan_m, int n,
                         double hTx_m, double hRx_m, double freq_hz,
                         double eirpDbm, double rxGainDbi,
                         double reflection = 1.0,
                         double minDistance_m = 1.0);

// ── 複数 AP の配置とカバレッジ ─────────────────────────────────────────────
//
// 1 局のカバレッジ図が距離依存を見るためのものだったのに対し、複数局では
// **どの局に繋ぐか (最良サーバ)** と**他局が干渉になること**が本題になる。
// 各点で
//   受信電力 C   = max_k P_k        (最も強い局に繋ぐ)
//   干渉電力 I   = Σ_{k≠best} P_k   (残りは同一チャネル干渉)
//   SINR        = C / (I + N)
// を取る。**電力の和は dBm ではなく真値で足す** (dB のまま足すのは誤り)。
//
// 経路損失は 1 局のときと同じ 2 波モデルを使う (同じ図の延長として読めるように
// するため)。局ごとに高さと EIRP を変えられる。
struct AccessPoint {
    double x_m = 0.0, y_m = 0.0;   // 水平位置 (格子の中心が原点)
    double h_m = 10.0;             // 空中線高
    double eirpDbm = 30.0;
};

// count 局を半径 radius_m の円周上に等間隔で置く (1 局のときは中心)。
// 等間隔なので配置は回転対称で、カバレッジ図もその対称性を持つ。
// count <= 0 なら空。
std::vector<AccessPoint> apRing(int count, double radius_m, double h_m,
                                double eirpDbm);

struct MultiCoverage {
    int n = 0;
    double halfSpan_m = 0.0;
    std::vector<double> bestDbm;    // n*n: 最良サーバの受信電力 [dBm]
    std::vector<int>    server;     // n*n: 最良サーバの AP 番号 (-1 = 無し)
    std::vector<double> sinrDb;     // n*n: SINR [dB]
    double minDbm = 0.0, maxDbm = 0.0;
    double coveredFraction = 0.0;   // 閾値以上の点の割合
    bool valid() const
    {
        return n > 0 && halfSpan_m > 0.0
            && bestDbm.size() == std::size_t(n) * std::size_t(n);
    }
    double coord(int i) const
    {
        return (n > 1) ? (-halfSpan_m + 2.0 * halfSpan_m * i / (n - 1)) : 0.0;
    }
};

// 複数 AP のカバレッジ格子。noiseDbm は熱雑音電力 (thermalNoiseDbm で作る)、
// thresholdDbm はカバー率の判定閾値。
// **AP が 1 局で原点にあるとき、bestDbm は coverageMap の dbm と厳密に一致**
// する (同じ経路損失を使っているため — selftest で判定している)。
MultiCoverage coverageMapMulti(const std::vector<AccessPoint> &aps,
                               double halfSpan_m, int n,
                               double hRx_m, double freq_hz, double rxGainDbi,
                               double noiseDbm, double thresholdDbm,
                               double reflection = 1.0,
                               double minDistance_m = 1.0);

} // namespace propagation
} // namespace em
} // namespace ofd

#endif // OFD_EM_RADIOPROPAGATION_H
