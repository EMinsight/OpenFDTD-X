// DisplayMetrics.h — ディスプレイ / AR-VR 光学の解析式による評価量.
//
// Qt 非依存の純粋計算。いずれも教科書・公表論文の閉形式で、DisplayOpticsTab の
// 入力 (基板屈折率・格子周期・チップ寸法など) から直接計算できるものだけを
// 置く。実光線追跡・RCWA・Berreman 4x4 が要る量 (輝度均一性・迷光・視野角
// 特性など) はここには無い — タブ側で「—」と表示する。
//
// 出典:
//  [1] T. Levola, "Diffractive optics for virtual reality displays",
//      J. Soc. Inf. Display 14(5), 467-475 (2006).       … 導波路の FOV 帯域
//  [2] B. C. Kress, I. Chatterjee, "Waveguide combiners for mixed reality
//      headsets: a nanophotonics design perspective",
//      Opt. Eng. 60(8), 081807 (2021).                   … 瞳拡大とアイボックス
//  [3] E. F. Schubert, "Light-Emitting Diodes", 2nd ed., Cambridge (2006),
//      Ch. 9 (escape cone / extraction efficiency).
//  [4] N. C. Greenham, R. H. Friend, D. D. C. Bradley,
//      "Angular dependence of the emission from a conjugated polymer LED",
//      Adv. Mater. 6, 491 (1994).                        … OLED の 1/(2n^2)
//  [5] M. Born, E. Wolf, "Principles of Optics", 7th ed., §1.6 (平板の透過率)
//  [6] F. Olivier et al., "Influence of size-reduction on the performances of
//      GaN-based micro-LEDs for display application",
//      J. Lumin. 191, 112 (2017).                        … 側壁再結合の寸法則
//  [7] IEC 62341-6-1 / ISO 9241-305 (環境光下のコントラスト比の定義)
#pragma once

namespace ofd {
namespace displayoptics {

// ── 平板導波路コンバイナ (SRG) の視野角帯域 ────────────────────────────────
// 格子式 n·sinθg = sinθair + λ/Λ (1 次回折) と、導波条件
// θc = asin(1/n) <= θg <= θgMax から、空気側の角度帯域を求める [1]。
// θgMax は実装上の導波角上限 (掠め入射に近づくほど伝搬距離が延びるため、
// 実機では 80° 程度に制限する)。
struct WaveguideFov {
    bool   valid = false;      // 帯域が存在する (|sinθair| <= 1 の解がある)
    double critAngle_deg = 0;  // 全反射臨界角 θc
    double fovMin_deg = 0;     // 空気側の下端 (θg = θc に対応)
    double fovMax_deg = 0;     // 空気側の上端 (θg = θgMax に対応)
    double fov_deg = 0;        // 帯域幅 (= fovMax - fovMin)
};
WaveguideFov waveguideFov(double period_nm, double lambda_nm, double nSub,
                          double guideMax_deg);

// 瞳拡大後のアイボックス幅 [mm]。
// 出射格子の長さ L から、視野端の光線がアイレリーフ ER だけ離れた瞳面で
// 削る分を引く: W = L - 2·ER·tan(FOV/2) [2]。負なら 0 を返す。
double eyeboxWidth_mm(double outcouplerLen_mm, double eyeRelief_mm,
                      double fov_deg);

// アイレリーフを掃引したときのアイボックス幅 (作図用)。
// W(ER) = L − 2·ER·tan(FOV/2) なので **ER に対して厳密に直線**で、
// ER0 = L / (2·tan(FOV/2)) で 0 になる (それ以上離れると瞳が視野を外れる)。
// 幅そのものは eyeboxWidth_mm を呼んで作る (同じ式を 2 度書かない)。
struct EyeboxSweep {
    bool   valid = false;
    double zeroEyeRelief_mm = 0;    // W = 0 になるアイレリーフ ER0
    double slope_mm_per_mm = 0;     // dW/dER = −2·tan(FOV/2)
    // eyeRelief[i] と eyebox[i] が対になる (i = 0..n-1、eyeRelief は昇順)
    // ※ Qt 非依存にするため呼び出し側が配列を確保して渡す
};
// er[] / w[] に n 点を書く (er は 0 から erMax_mm まで等間隔)。
// 戻り値の valid が false なら配列は触らない。
EyeboxSweep eyeboxVsEyeRelief(double outcouplerLen_mm, double fov_deg,
                              double erMax_mm, int n,
                              double *er, double *w);

// FOV が最小になる格子周期 [nm] (閉形式)。
// fov(Λ) = asin(u + c) − asin(u),  u = 1 − λ/Λ,  c = n·sinθg,max − 1 なので
// d fov/du = 0 は u = −c/2、すなわち **Λ = 2λ / (1 + n·sinθg,max)**。
// **FOV は周期に対して単調ではなく、ここに極小がある** — つまり同じ FOV を
// 与える周期が 2 つありうる。トレードオフ図を「周期の順」に線で結ぶと折り
// 返すので、図は FOV の順に並べ替えてから結ぶこと。
// 帯域が成立しない (|sin| > 1) 場合もあるので、値は目安として使う。
double minFovPeriod_nm(double lambda_nm, double nSub, double guideMax_deg);

// 格子周期を掃引したときの「FOV とアイボックスのトレードオフ」(作図用)。
// 周期を変えると導波できる角度帯域 (FOV) が変わり、FOV が広がるほど
// 視野端の光線が瞳面で余計に削るのでアイボックスは狭くなる。
// fov[] / eyebox[] / ok[] に n 点を書く (ok[i] = その周期で帯域が成立するか)。
// 戻り値は帯域が成立した点の数。
int fovEyeboxTradeoff(double periodMin_nm, double periodMax_nm, int n,
                      double lambda_nm, double nSub, double guideMax_deg,
                      double outcouplerLen_mm, double eyeRelief_mm,
                      double *period, double *fov, double *eyebox, bool *ok);

// 無コート平板の垂直入射透過率 (2 界面 + 多重反射, 無吸収) T = (1-R)/(1+R) [5]
double slabTransmittance(double nSub);

// 単一界面の垂直入射フレネル透過率 T = 4n/(n+1)^2
double fresnelNormalTransmittance(double n);

// 全反射臨界角 [deg] (n > 1 のとき。それ以外は 90)
double criticalAngle_deg(double n);

// 等方双極子の射出円錐割合 (1 面) (1 - cosθc)/2 [3]
double escapeConeFraction(double n);

// OLED の古典的な光取り出し効率 η = 1/(2 n^2)
// (背面に反射電極がある平面構造の ray-optics 近似) [4]
double oledOutcoupling(double nOrganic);

// LED チップの取り出し効率の 2 つの古典的な目安 [3]:
//  - 上面 1 面のみ (吸収基板)     : (1-cosθc)/2 · T_fresnel
//  - 立方体 6 面 (透明基板 上限)  : 3/(2 n^2)
double ledExtractionTopFace(double n);
double ledExtractionCube(double n);

// 側壁再結合による実効内部量子効率の低下 [6]。
// 表面再結合速度 S [cm/s]、バルク実効寿命 τ [ns]、正方チップ辺長 L [μm] に
// 対し、周長/面積 = 4/L のスケーリングから  η_eff = η0 / (1 + 4·S·τ / L)。
double sidewallDeratedIqe(double iqe0, double chipSize_um,
                          double surfaceVelocity_cm_s, double lifetime_ns);

// 環境光下のコントラスト比 [7]。
// 拡散反射する画面の環境光輝度 L_amb = R·E/π [cd/m^2] を白/黒輝度に足す:
//   CR_amb = (L_white + L_amb) / (L_black + L_amb),  L_black = L_white / CR0
struct AmbientContrast {
    bool   valid = false;
    double ambientLuminance_cdm2 = 0;
    double blackLuminance_cdm2 = 0;
    double contrast = 0;
};
AmbientContrast ambientContrast(double peakLuminance_cdm2, double darkroomCr,
                                double ambient_lx, double reflectance);

// ランバート配光の半値角 [deg]: I(θ) = I0·cosθ → θ = 60°
double lambertianHalfAngle_deg();

} // namespace displayoptics
} // namespace ofd
