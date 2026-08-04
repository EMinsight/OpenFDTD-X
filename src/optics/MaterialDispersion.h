// MaterialDispersion.h — 実材料の屈折率分散 (公刊 Sellmeier 係数) と熱光学係数。
//
// MaterialExplorerTab の file-local テーブルから抽出した Qt 非依存の共有モジュール。
// モードソルバ FDE / 材料エクスプローラなど、実材料の n(λ, T) を必要とする
// すべての箇所から使う。
//
// 方針:
//   - 係数は公刊値のみ (出典を .cpp のコメントに著者・年で明記)。
//   - 有効範囲外は評価しない (範囲外へ外挿した「それらしい値」を返さない)。
//   - 熱光学係数 dn/dT は信頼できる出典で確認できた材料にのみ与え、
//     確認できない材料は 0 で埋めず「未定義」(hasDnDt = false) として扱う。
#pragma once
#include <vector>

namespace ofd {
namespace optics {

// 材料の書誌情報 (係数そのものは .cpp の内部テーブルが持つ)
struct MaterialInfo {
    const char *id;          // "Si" / "SiO2" / "Si3N4" / "Air" ...
    const char *label;       // 表示名 (出典付き)
    double lmin_um, lmax_um; // Sellmeier の有効範囲 [μm]
    bool   hasDnDt;          // 熱光学係数が公刊値で定義されているか
    double dnDt_perK;        // hasDnDt が false のとき意味を持たない [1/K]
    double tRef_C;           // dn/dT の基準温度 [degC]
};

// 内蔵材料の一覧 (登録順)
const std::vector<MaterialInfo> &materials();

// id で検索。見つからなければ nullptr (id == nullptr も nullptr)
const MaterialInfo *findMaterial(const char *id);

// λ [μm] における屈折率。範囲外・未知 id では false を返し value を書き換えない
// (範囲外へ外挿した「それらしい値」を返さない)。
bool refractiveIndex(const char *id, double lambda_um, double &value);

// 温度 T [degC] を考慮した屈折率。n(T) = n(λ) + (dn/dT)·(T − T_ref)。
// dn/dT 未定義の材料では温度補正を行わず tempApplied = false を返す
// (value には温度補正なしの n が入り、戻り値は true)。
// λ が範囲外・未知 id のときは false を返し value / tempApplied を書き換えない。
bool refractiveIndexAt(const char *id, double lambda_um, double temp_C,
                       double &value, bool &tempApplied);

} // namespace optics
} // namespace ofd
