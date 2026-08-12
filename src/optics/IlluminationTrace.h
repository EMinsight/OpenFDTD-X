// IlluminationTrace.h — 非順次 (non-sequential) モンテカルロ・レイトレーサ。
// Qt 非依存 / C++17。IlluminationTab の配光量 (系の全光束・光学効率・
// ビーム角・照度均斉度) の計算実体。
//
// 順次追跡 (optics/RayTrace) との違い: あちらは面の並び順が既知の結像系を
// 追う。照明系は光線が面をどの順で何回通るか分からない (リフレクタで反射
// してから拡散板を通り、また戻ることもある) ので、**最近接交点を毎回探す**
// 非順次の追跡が要る。面の反射モデル (鏡面 / 拡散 / ABG) はここで初めて
// 結果に効く — 順次追跡は屈折しか扱わないため。
//
// ── 系のモデル (軸対称) ────────────────────────────────────────────────────
//
//   光源      原点。ランバート放射 (+z 半球) で全光束 Φ [lm]。
//             点光源、または一辺 s の正方チップ (z = 0 面上で一様)。
//   リフレクタ 焦点が原点にある回転放物面。開口半径 R で切る。
//                 x² + y² = 4f(z + f)      (頂点 z = −f, 焦点 z = 0)
//             原点からの極形式は r(θ) = 2f/(1 − cosθ)。
//   拡散板     z = z_d の円板 (半径 R_d, 法線 +z)。透過率 τ で散乱透過する。
//   評価面     z = D の正方形 (半幅 W) を cells×cells に分けた照度グリッド。
//
// ── 反射・散乱モデル ───────────────────────────────────────────────────────
//
//   鏡面 (Specular)    : D' = D − 2(D·n̂)n̂ / 透過は方向不変。
//   拡散 (Lambertian)  : 法線まわりの cos 分布。
//   ABG (Harvey–Shack) : 方向余弦 (β) 空間で鏡面方向のまわりに
//                            BSDF(Δβ) = A / (B + Δβ^g)
//                        で散乱する。散乱角の pdf は立体角要素を含めて
//                            p(Δβ) ∝ Δβ·A/(B + Δβ^g)
//                        で、累積分布を数表にして逆関数法で引く
//                        (g = 2 では F(Δβ) = ln(1+Δβ²/B)/ln(1+Δβmax²/B) の
//                        閉形式に一致する — selftest で検算している)。
//
// エネルギーは光線の重み w で運ぶ。反射率 ρ (透過率 τ) を掛けた残りを吸収と
// して積み上げるので、**Φ_out + Φ_abs = Φ_in が丸め誤差まで厳密に成り立つ**。
//
// ── 既知の近似・未実装 (絶対規則 5: 動作済みに見せない) ────────────────────
//
//   - **ABG は鏡面スパイクを分離しない**。反射エネルギー全体を ABG の分布に
//     従って散乱させる (TIS を別に持たない)。粗さの小さい面では B を小さく
//     すれば実質的に鏡面へ漸近する。
//   - **地平線を越える散乱 (|β| ≥ 1) は吸収として計上**する (BSDF の切り捨て)。
//     再サンプリングで押し戻すと分布が歪むため、こちらを選んだ。
//   - TIR レンズ・導光板・蛍光体散乱・実測 BSDF・実測レイデータは**未実装**。
//     Scene に入れる形が無いので、呼び出し側は該当する選択のときに
//     「この要素は追跡モデルに入っていない」と明示すること。
//   - 波長は追わない (分光量は optics/Colorimetry が別に担当する)。屈折率
//     分散・蛍光変換は扱わない。
//   - 遠方界強度 I(θ) は軸対称を仮定して θ ビンへ集める。
//
// ── 乱数 ───────────────────────────────────────────────────────────────────
// 放射方向は Halton 列 (基数 2, 3) の準乱数で、散乱は光線番号から決まる
// ハッシュ乱数。**状態を持たないので同じ入力からは必ずビット単位で同じ結果**
// が出る (selftest で検証)。
#ifndef OFD_OPTICS_ILLUMINATIONTRACE_H
#define OFD_OPTICS_ILLUMINATIONTRACE_H

#include <vector>

namespace ofd {
namespace illum {

// 面の反射 / 散乱モデル (IlluminationOpts::surface の 0/1/3 に対応。
// 2 = BSDF 実測は測定データが無いのでここには無い)
enum class Scatter { Specular = 0, Lambertian = 1, ABG = 2 };

// Harvey–Shack ABG モデルの係数。BSDF(Δβ) = A/(B + Δβ^g)
struct AbgParams {
    double A = 0.02;
    double B = 1.0e-4;
    double g = 2.0;
};

// ABG の散乱角サンプラ (累積分布の数表 + 逆関数法)。
// cdf() は検証のために公開してある (g = 2 の閉形式と突き合わせる)。
class AbgSampler {
public:
    AbgSampler(const AbgParams &p, double dbetaMax = 2.0, int nodes = 4096);
    double sample(double u) const;   // u ∈ [0,1) → Δβ
    double cdf(double dbeta) const;  // Δβ → F (数表からの線形補間)
    bool   valid() const { return m_valid; }

private:
    std::vector<double> m_cdf;  // 節点上の F
    double m_max = 2.0;
    double m_step = 0.0;
    bool   m_valid = false;
};

struct Source {
    enum Kind { Point = 0, Chip = 1 };
    Kind   kind = Point;
    double size_mm = 1.0;      // Chip の一辺
    double flux_lm = 1000.0;   // 全光束 Φ
};

struct Reflector {
    bool      enabled = false;
    double    focal_mm = 5.0;    // 焦点距離 f (光源は焦点にある)
    double    radius_mm = 20.0;  // 開口半径 R
    double    reflectance = 0.9; // ρ
    Scatter   model = Scatter::Specular;
    AbgParams abg;
};

struct Diffuser {
    bool      enabled = false;
    double    z_mm = 25.0;         // 板の位置
    double    radius_mm = 25.0;    // 板の半径
    double    transmittance = 0.85;// τ
    Scatter   model = Scatter::Lambertian;
    AbgParams abg;
};

struct TargetPlane {
    double distance_mm = 1000.0;  // z = D
    double half_mm = 500.0;       // 半幅 W
    int    cells = 21;            // 片側の分割数 (cells × cells)
};

struct Scene {
    Source      source;
    Reflector   reflector;
    Diffuser    diffuser;
    TargetPlane target;
    int         angleBins = 180;  // 遠方界の θ ビン数 (0..180°)
};

// ── 追跡の打ち切りと放射方向のサンプリング ─────────────────────────────────
//
// 放射方向の引き方は 4 通りある。**どれも推定量としては不偏**で、違うのは
// 分散だけ (光線 1 本 1 本の重みの与え方が変わる):
//
//   Qmc        : Halton 列で cos 分布から引く。ランバート光源では cos 分布
//                そのものが重要度サンプリングなので、**全ての光線が等しい
//                重み Φ/N** を持つ。既定 (従来の挙動と完全に一致)。
//   Jittered   : √N × √N の層に分けて層内を乱数でずらす。重みは Qmc と同じ。
//   Uniform    : 立体角に一様に引き、重みで cos を補う (w = 2cosθ·Φ/N)。
//                素朴な方式で、**分散が最も大きい**。比較の基準として置く。
//   Importance : 評価面を見込む円錐 (θ < θ_c) の内と外に光線を配り分ける。
//                内側へ配る割合 p を実際の光束比 F = sin²θ_c より大きく
//                (最低でも半分) 取り、重みで割り戻す:
//                    内 w = Φ·F/n_in,  外 w = Φ·(1−F)/(n_out)
//                本数を**あらかじめ決めて配る** (層化) ので、初期重みの
//                総和は厳密に Φ のままで、光束収支も閉じたままになる。
enum class Sampling { Uniform = 0, Jittered = 1, Qmc = 2, Importance = 3 };

struct TraceOptions {
    Sampling sampling = Sampling::Qmc;
    int      maxBounces = 64;        // これを超えたら打ち切って吸収に計上
    double   minEnergy_dB = -90.0;   // 初期重みに対する打ち切り閾値 (電力比)
    int      maxDiffuse = 0;         // 非鏡面の散乱回数の上限 (0 = 制限なし)
};

struct Result {
    bool   valid = false;
    long long rays = 0;

    // 光束収支 [lm] — out + absorbed = in (丸め誤差まで)
    double fluxIn_lm = 0.0;
    // 実際に放射した重みの合計。Qmc / Jittered / Importance では厳密に
    // fluxIn と一致し、Uniform だけモンテカルロ誤差ぶんずれる (重みが
    // 2cosθ で揺らぐため)。**光束収支はこちらに対して厳密に閉じる**。
    double fluxEmitted_lm = 0.0;
    double fluxOut_lm = 0.0;       // 系の外へ出た
    double fluxAbsorbed_lm = 0.0;  // 面で吸収 + 打ち切り + 地平線越え
    double fluxTarget_lm = 0.0;    // 評価面に当たった
    double efficiency = 0.0;       // fluxOut / fluxIn
    double targetEfficiency = 0.0; // fluxTarget / fluxIn

    // 配光
    std::vector<double> intensity_cd;   // angleBins 個 (θ = 0..180°)
    double axialIntensity_cd = 0.0;     // I(0) (先頭ビン)
    double beamAngleFwhm_deg = 0.0;     // 全角
    bool   beamValid = false;           // 半値へ落ちる点が見つかったか

    // 照度 [lx]
    int    cells = 0;                   // 実際に使った分割数 (偶数指定は +1 して奇数化)
    std::vector<double> illuminance_lx; // cells × cells (行 = y)
    double illumCenter_lx = 0.0;
    double illumAvg_lx = 0.0;
    double illumMin_lx = 0.0;
    double illumMax_lx = 0.0;
    double uniformityMinAvg = 0.0;      // U1 = Emin / Eav
    bool   uniformityValid = false;     // 統計が足りているか

    long long raysOnTarget = 0;
    long long raysTrapped = 0;          // 反射回数の上限で打ち切った本数
    long long raysDiffuseCut = 0;       // 拡散次数の上限で打ち切った本数
};

// nRays 本を追跡する。nRays <= 0 や幾何が不正 (f <= 0, R <= 2f, D が系の
// 内側など) のときは valid = false で返す (「それらしい値」を返さない)。
Result trace(const Scene &s, long long nRays, const TraceOptions &opt = TraceOptions());

// 追跡が成り立たない理由を返す (成り立つときは nullptr)。
// 呼び出し側がそのまま利用者へ出せるよう、理由の識別子を返す:
//   "rays" / "flux" / "focal" / "radius" / "target" / "cells"
const char *traceBlocker(const Scene &s, long long nRays);

} // namespace illum
} // namespace ofd

#endif // OFD_OPTICS_ILLUMINATIONTRACE_H
