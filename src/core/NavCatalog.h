// NavCatalog.h — 左ナビの「タブキー → カテゴリキー」対応表 (app.jsx LeftDock 相当)。
//
// MainWindow::buildLeftNav の Def テーブルと selftest が共有する唯一の出所。
// Def 側にカテゴリを直書きすると、カテゴリ再編のたびに検証と実装が二重管理に
// なるため、対応表だけをここへ出してある (ComponentCatalog.h と同じ流儀)。
// Qt 非依存 (const char* の静的表) — selftest から直接検証できる。
//
// カテゴリの意味づけ:
//   cat_setup   … 形状/物性/領域/波源/観測 の入力 (全ドメイン共通の①〜⑤)
//   cat_library … 「そこから選んで使う」部品・素材のカタログ
//   cat_apps    … ドメイン固有の応用解析 (カタログを使って解く用途別ワークフロー)
//   cat_solve   … 実行と実行前後の設定
//   cat_post    … 結果の可視化・二次解析
//   cat_dom_*   … ドメイン専用の総合タブ
//
// 並び順もこの表の順 = ナビの表示順。TabNavigator::rebuild は「直前の項目と
// カテゴリキーが変わったところ」で見出しを挿入するので、**同じカテゴリの項目は
// 必ず連続させること** (連続性は selftest が検証する)。
#pragma once
#include <cstring>

namespace ofd {
namespace navcat {

struct Assign {
    const char *navKey;       // TabNavigator::Entry::key
    const char *categoryKey;  // I18n キー ("cat_setup" …)
};

// ナビの表示順 (MainWindow の Def テーブルもこの順に並べる)
inline const Assign *table(int *count)
{
    static const Assign t[] = {
        // セットアップ
        { "geometry",     "cat_setup" },
        { "material",     "cat_setup" },
        { "solverregion", "cat_setup" },
        { "source",       "cat_setup" },
        { "monitors",     "cat_setup" },
        { "general",      "cat_setup" },
        { "mesh",         "cat_setup" },
        { "perface",      "cat_setup" },
        // ライブラリ (部品・素材のカタログ)
        { "components",   "cat_library" },
        { "matexplorer",  "cat_library" },
        { "glasscatalog", "cat_library" },
        { "lens",         "cat_library" },
        { "layoutgds",    "cat_library" },
        { "schematic",    "cat_library" },
        // 応用 (光)
        { "photonics",    "cat_apps" },
        { "modesolver",   "cat_apps" },
        { "thinfilm",     "cat_apps" },
        { "illum",        "cat_apps" },
        { "displayopt",   "cat_apps" },
        // 応用 (音響 / 水中)
        { "acsource",     "cat_apps" },
        { "audioedit",    "cat_apps" },
        { "oceanenv",     "cat_apps" },
        { "roomac",       "cat_apps" },
        { "acsolver",     "cat_apps" },
        { "soundproof",   "cat_apps" },
        { "outdoor",      "cat_apps" },
        { "cabin",        "cat_apps" },
        { "ultrasound",   "cat_apps" },
        // 解析
        { "family",       "cat_solve" },
        { "solver",       "cat_solve" },
        { "verification", "cat_solve" },
        { "optimize",     "cat_solve" },
        { "tolerance",    "cat_solve" },
        { "scripts",      "cat_solve" },
        { "multiphysics", "cat_solve" },
        { "tidy3d",       "cat_solve" },
        // ポスト
        { "analysisgroups", "cat_post" },
        { "datasets",     "cat_post" },
        { "h5viewer",     "cat_post" },
        { "interop",      "cat_post" },
        { "antennachar",  "cat_post" },
        { "txline",       "cat_post" },
        { "scattering",   "cat_post" },
        { "circuit",      "cat_post" },
        { "emc",          "cat_post" },
        { "sar",          "cat_post" },
        { "channel",      "cat_post" },
        { "post1",        "cat_post" },
        { "post2",        "cat_post" },
        // ドメイン別
        { "optical",      "cat_dom_optical" },
        { "acoustic",     "cat_dom_acoustic" },
        { "riranalysis",  "cat_dom_acoustic" },
        { "vocalanalysis","cat_dom_acoustic" },
        { "auralization", "cat_dom_acoustic" },
        { "underwater",   "cat_dom_underwater" },
    };
    if (count) *count = int(sizeof(t) / sizeof(t[0]));
    return t;
}

// 未登録は nullptr (呼び手が「登録漏れ」として扱えるように)
inline const char *categoryFor(const char *navKey)
{
    int n = 0;
    const Assign *t = table(&n);
    for (int i = 0; i < n; ++i)
        if (std::strcmp(t[i].navKey, navKey) == 0) return t[i].categoryKey;
    return nullptr;
}

} // namespace navcat
} // namespace ofd
