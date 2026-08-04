// MaterialDispersion.cpp — 公刊 Sellmeier 係数と熱光学係数のテーブル。
//
// 分散式 (λ は μm、C/E は μm²):
//     n²(λ) = A + Σᵢ Bᵢ·λ²/(λ² − Cᵢ) + D/(λ² − E)
// 第 3 項は DeVore 型 (λ² に比例しない加算極) 用で、D = 0 なら未使用。
//
// 係数の出典は各行のコメントに記す。有効範囲は出典が測定 / フィットした
// 波長域であり、この範囲外では評価しない (外挿しない)。
#include "optics/MaterialDispersion.h"

#include <cmath>
#include <cstring>

namespace ofd {
namespace optics {
namespace {

struct Entry {
    MaterialInfo info;
    double A;
    double B[3], C[3];   // Sellmeier 項
    double D, E;         // 加算極 (DeVore 型)。D = 0 なら未使用
};

// ── 熱光学係数 dn/dT (公刊値) ───────────────────────────────────────────────
// Si    : +1.86e-4 /K — Cocorullo & Rendina, Electron. Lett. 28, 83 (1992)
//         λ = 1.523 μm, T ≈ 300 K (室温近傍 283–333 K で概ね一定)。
// SiO2  : +1.0e-5 /K — Leviton & Frey, Proc. SPIE 6273, 62732K (2006) の
//         合成石英 (近赤外・室温近傍の値。0.95–1.1e-5 /K の範囲で報告される)。
//         Arbabi & Goddard, Opt. Lett. 38, 3878 (2013) の SiO2 実測とも整合。
// Si3N4 : +2.45e-5 /K — Arbabi & Goddard, Opt. Lett. 38, 3878 (2013)
//         LPCVD Si3N4、λ = 1.55 μm、室温近傍 (マイクロリング共振の温度掃引)。
// Al2O3 : +1.3e-5 /K — Tapping & Reilly, J. Opt. Soc. Am. A 3, 610 (1986)
//         サファイア常光線、633/799 nm、室温近傍 (高温側で増大する)。
// LiNbO3: +3.3e-5 /K — Moretti et al., J. Appl. Phys. 98, 036101 (2005)
//         異常光線 (ne)、λ ≈ 1.5 μm、T ≈ 300 K。
// TiO2 / PMMA は文献値のばらつきが大きく単一の公刊値を特定できないため
// 「未定義」(hasDnDt = false) とする。0 で埋めない。
// (PMMA は負の dn/dT を持つことが知られているが、報告値が −1.0e-4 〜 −1.3e-4 /K
//  と広く、どれを採るか根拠を示せないので値を与えない。)

const Entry kEntries[] = {
    // ── 空気 / 真空 ────────────────────────────────────────────────────────
    // FDTD の背景媒質と同じく n = 1 (無分散) として扱う。標準空気の実測は
    // n ≈ 1.00027 (Ciddor 1996) だが、本 GUI の背景は真空 n = 1 が既定。
    { { "Air", "Air / vacuum (n = 1, 無分散)", 0.1, 100.0, false, 0.0, 20.0 },
      1.0, { 0, 0, 0 }, { 1, 1, 1 }, 0, 0 },

    // ── SiO2 — Malitson, J. Opt. Soc. Am. 55, 1205 (1965) (合成石英) ───────
    { { "SiO2", "SiO2 (fused silica, Malitson 1965)", 0.21, 3.71,
        true, 1.0e-5, 20.0 },
      1.0, { 0.6961663, 0.4079426, 0.8974794 },
      { 0.004679148, 0.013512063, 97.934003 }, 0, 0 },

    // ── Si3N4 — Luke et al., Opt. Lett. 40, 4823 (2015) (LPCVD 化学量論組成) ─
    { { "Si3N4", "Si3N4 (Luke 2015)", 0.31, 5.5, true, 2.45e-5, 25.0 },
      1.0, { 3.0249, 40314.0, 0.0 },
      { 0.018317068, 1537208.2, 1.0 }, 0, 0 },

    // ── Al2O3 — Malitson, J. Opt. Soc. Am. 52, 1377 (1962) (サファイア常光) ─
    { { "Al2O3", "Al2O3 (sapphire, ordinary ray, Malitson 1962)", 0.2, 5.0,
        true, 1.3e-5, 25.0 },
      1.0, { 1.4313493, 0.65054713, 5.3414021 },
      { 0.005279925, 0.014238264, 325.01783 }, 0, 0 },

    // ── Si — Salzberg & Villa, J. Opt. Soc. Am. 47, 244 (1957) (赤外透明域) ─
    // dn/dT の基準温度は Cocorullo & Rendina の 300 K = 26.85 degC。
    { { "Si", "Si (Salzberg-Villa 1957)", 1.36, 11.0, true, 1.86e-4, 26.85 },
      1.0, { 10.6684293, 0.0030434748, 1.54133408 },
      { 0.090912190, 1.2876602, 1218816.0 }, 0, 0 },

    // ── TiO2 — DeVore, J. Opt. Soc. Am. 41, 416 (1951) (ルチル常光) ────────
    //   n² = 5.913 + 0.2441/(λ² − 0.0803)
    { { "TiO2", "TiO2 (rutile, ordinary ray, DeVore 1951)", 0.43, 1.5,
        false, 0.0, 25.0 },
      5.913, { 0, 0, 0 }, { 1, 1, 1 }, 0.2441, 0.0803 },

    // ── PMMA — Sultanova et al., Acta Phys. Pol. A 116, 585 (2009) ─────────
    { { "PMMA", "PMMA (Sultanova 2009)", 0.44, 1.05, false, 0.0, 20.0 },
      1.0, { 1.1819, 0, 0 }, { 0.011313, 1.0, 1.0 }, 0, 0 },

    // ── LiNbO3 (異常光線 ne) — Zelmon, Small & Jundt, ───────────────────────
    //    J. Opt. Soc. Am. B 14, 3319 (1997) (コングルエント組成, 21 degC)
    { { "LiNbO3_e", "LiNbO3 (extraordinary ray nₑ, Zelmon 1997)", 0.4, 5.0,
        true, 3.3e-5, 26.85 },
      1.0, { 2.9804, 0.5981, 8.9543 },
      { 0.02047, 0.0666, 416.08 }, 0, 0 },
};

const Entry *findEntry(const char *id)
{
    if (!id) return nullptr;
    for (const Entry &e : kEntries)
        if (std::strcmp(e.info.id, id) == 0) return &e;
    return nullptr;
}

double evalN(const Entry &e, double lambda_um)
{
    const double l2 = lambda_um * lambda_um;
    double n2 = e.A;
    for (int i = 0; i < 3; ++i)
        if (e.B[i] != 0.0) n2 += e.B[i] * l2 / (l2 - e.C[i]);
    if (e.D != 0.0) n2 += e.D / (l2 - e.E);
    return (n2 > 0.0) ? std::sqrt(n2) : 0.0;
}

} // namespace

const std::vector<MaterialInfo> &materials()
{
    static const std::vector<MaterialInfo> list = [] {
        std::vector<MaterialInfo> v;
        v.reserve(sizeof(kEntries) / sizeof(kEntries[0]));
        for (const Entry &e : kEntries) v.push_back(e.info);
        return v;
    }();
    return list;
}

const MaterialInfo *findMaterial(const char *id)
{
    const Entry *e = findEntry(id);
    return e ? &e->info : nullptr;
}

bool refractiveIndex(const char *id, double lambda_um, double &value)
{
    const Entry *e = findEntry(id);
    if (!e) return false;
    if (!(lambda_um >= e->info.lmin_um) || !(lambda_um <= e->info.lmax_um))
        return false;              // 範囲外 / NaN は評価しない
    value = evalN(*e, lambda_um);
    return true;
}

bool refractiveIndexAt(const char *id, double lambda_um, double temp_C,
                       double &value, bool &tempApplied)
{
    double n = 0.0;
    if (!refractiveIndex(id, lambda_um, n)) return false;
    const MaterialInfo *m = findMaterial(id);
    if (m->hasDnDt) {
        n += m->dnDt_perK * (temp_C - m->tRef_C);
        tempApplied = true;
    } else {
        tempApplied = false;       // dn/dT 未定義 — 温度は反映しない
    }
    value = n;
    return true;
}

} // namespace optics
} // namespace ofd
