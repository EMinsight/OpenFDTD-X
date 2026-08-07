// PhotonicCircuit.h — フォトニック集積回路 (PIC) の回路レベル解析。
// Qt 非依存 / C++17。selftest から解析解と直接突き合わせる。
//
// PIC 設計は 2 階層になる:
//   ① 素子レベル — FDTD / FDE / RCWA で個々の素子を解いて S パラメータを得る
//   ② 回路レベル — S パラメータを配線図どおりに接続して回路応答を出す
// ここは **②** を担う。① は既存のカーネル (ofd / orcwa / obpm) と
// `optics/FdeModeSolver` の役目で、その結果 (neff, κ, 損失) を入力にする。
//
// 規約:
//   - 時間因子は exp(+jωt)。伝搬は exp(-jβL)、損失は exp(-αL/2) (振幅)
//   - β = 2π·neff/λ。群屈折率 ng を与えると分散 (FSR) が正しく出る:
//       neff(λ) = neff(λ0) + (neff(λ0) - ng)·(λ - λ0)/λ0
//   - 損失は [dB/cm] で与える (業界慣用)。α[1/m] = ln(10)/10 · (dB/cm)·100
//   - 方向性結合器は無損失の 2×2:  through t = sqrt(1-κ²), cross = -j·κ
//     (この符号でユニタリになり、リングの閉形式と整合する)
#pragma once
#include <complex>
#include <string>
#include <vector>

namespace ofd {
namespace optics {

using cplx = std::complex<double>;

// 導波路 1 本の素子パラメータ
struct Waveguide {
    double neff = 2.44;        // 実効屈折率 (λ0 での値)
    double ng = 4.2;           // 群屈折率 (0 以下なら分散なし = neff 一定)
    double lambda0_nm = 1550.0;// neff / ng を与えた基準波長
    double loss_dBcm = 2.0;    // 伝搬損失 [dB/cm]

    // 波長 λ [nm] での実効屈折率
    double neffAt(double lambda_nm) const;
    // 振幅伝達 exp(-jβL - αL/2)。length_um は [μm]
    cplx transfer(double lambda_nm, double length_um) const;
};

// リング共振器 (全域通過 = through のみ / アド・ドロップ = 2 結合器)
struct RingResonator {
    Waveguide wg;
    double radius_um = 5.0;     // 半径 (周長 L = 2πR)
    double kappa1 = 0.2;        // 入力側の結合率 κ (振幅)
    double kappa2 = 0.0;        // ドロップ側 (0 = 全域通過)
    double couplerLoss_dB = 0.0;// 結合器 1 個あたりの過剰損失 [dB]

    double circumference_um() const;
    // through / drop の振幅伝達
    cplx through(double lambda_nm) const;
    cplx drop(double lambda_nm) const;
};

// マッハツェンダ干渉計 (2 本のアーム長差 ΔL、両端 50:50 とは限らない結合器)
struct MachZehnder {
    Waveguide wg;
    double length1_um = 100.0;  // アーム 1
    double length2_um = 150.0;  // アーム 2 (差が ΔL)
    double kappa1 = 0.7071067811865476;   // 入力結合器 (既定 50:50)
    double kappa2 = 0.7071067811865476;   // 出力結合器
    double phaseShift_rad = 0.0;          // アーム 1 への追加位相 (熱光学等)

    cplx bar(double lambda_nm) const;    // 入力 1 → 出力 1
    cplx cross(double lambda_nm) const;  // 入力 1 → 出力 2
};

// 波長掃引の 1 点
struct SweepPoint {
    double lambda_nm = 0.0;
    double through_dB = 0.0;   // 10 log10 |t|²
    double drop_dB = 0.0;
};

// 共振器スペクトルから読める設計量
struct ResonatorMetrics {
    bool   valid = false;
    double fsr_nm = 0.0;          // 自由スペクトル間隔
    double fwhm_nm = 0.0;         // 共振の半値全幅
    double qFactor = 0.0;         // Q = λ/FWHM
    double finesse = 0.0;         // F = FSR/FWHM
    double extinction_dB = 0.0;   // 消光比 (最大 - 最小)
    double resonance_nm = 0.0;    // 掃引内で最も深い共振の波長
    std::string note;             // 読めなかった理由など
};

// 掃引 (lambda1 → lambda2 を points 点)
std::vector<SweepPoint> sweepRing(const RingResonator &ring, double lambda1_nm,
                                  double lambda2_nm, int points);
std::vector<SweepPoint> sweepMzi(const MachZehnder &mzi, double lambda1_nm,
                                 double lambda2_nm, int points);

// 掃引結果から FSR / FWHM / Q / フィネス / 消光比を読む。
// 共振が 2 本以上入っていないと FSR が出せないので、その場合は
// valid = false + note を返す (数字をでっち上げない)。
ResonatorMetrics analyseSweep(const std::vector<SweepPoint> &sweep);

// 解析式による FSR [nm] — FSR = λ²/(ng·L)。検証と設計目安に使う
double analyticFsr_nm(double lambda_nm, double ng, double length_um);

// ── 熱光学 (thermo-optic) ───────────────────────────────────────────────────
// Si は dn/dT ≈ 1.86e-4 /K (室温、1550 nm)。温度が上がると屈折率が上がり、
// 共振は長波長側へ動く。
//   neff(T) = neff(T0) + (dn/dT)·(T − T0)
//   Δλ_res  = λ_res · Δn_eff / n_g        (共振条件 m·λ = n_eff·L の微分)
// ng で割るのは群屈折率が波長依存を含むため (位相屈折率で割ると過大評価)。
double thermoOpticNeff(double neff0, double dndT_perK, double T_C,
                       double T0_C = 25.0);
double thermoOpticShift_nm(double lambda_nm, double dndT_perK, double dT_K,
                           double ng);

// ── ネットリストの経路解決 ─────────────────────────────────────────────────
// SchematicTab のネットリスト表 ("LASER1.out" → "MZI1.in1") から、素子の
// つながりを辿って 1 本の経路にする。回路レベル解析は経路が決まって初めて
// 掛け算できるので、まずここを解く。
//
// 分岐 (同じ素子から 2 本以上出る) は 1 本の経路にできないので、そこで
// 打ち切って理由を返す — 勝手にどちらかを選んで「つながっている」ことに
// しない。
struct NetLink {
    std::string fromNode, fromPort;   // "LASER1", "out"
    std::string toNode,   toPort;     // "MZI1",   "in1"
};

struct NetPath {
    std::vector<std::string> nodes;   // 通過する素子名 (始点から順)
    bool        complete = false;     // 終端まで一意に辿れたか
    std::string note;                 // 辿れなかった理由
};

// "LASER1.out" のような端子名を素子名とポート名へ割る。
// ドットが無ければ全体を素子名とし、ポートは空。
NetLink parseLink(const std::string &from, const std::string &to);

// 入次数 0 の素子 (どこからも入力されない = 始点候補) を返す。
std::vector<std::string> sourceNodes(const std::vector<NetLink> &links);

// start から辿れるだけ辿る。閉路は検出して打ち切る (無限ループにしない)。
NetPath tracePath(const std::vector<NetLink> &links, const std::string &start);

} // namespace optics
} // namespace ofd
