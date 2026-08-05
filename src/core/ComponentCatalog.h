// ComponentCatalog.h — コンポーネントライブラリの部品表とドメイン許可表。
// ComponentsTab (カード表示) と Viewport3D (ドロップ判定) と selftest が共有する。
// 部品 → ドメイン対応の二重管理を避けるため、許可表はこのヘッダだけに置く。
//
// domains はドメイン許可フラグ文字列:
//   e = 電磁 (em) / o = 光 (optical) / a = 室内音響 (acoustic) /
//   u = 水中音響 (underwater)
// 水中音響 (BELLHOP) は海洋環境タブ (SSP・海底・ソナー) だけから .env を
// 生成し、形状・波源・モニターの配置を一切使わないため、'u' を持つ部品は
// 存在しない (io/BellhopIO.cpp 参照)。
#pragma once
#include <QString>

namespace ofd {
namespace ComponentCatalog {

struct Component { const char *cat, *icon, *name, *sub, *domains; };

// 部品表 — Ansys Lumerical FDTD が同梱するものに基づく (mock 同値)。
// ドメイン許可は 2026-08 の監査結果:
//   - basic は形状なので e/o/a (水中は形状を使わない)
//   - photonic/grating/lens/metal はナノ〜μm スケールの光学構造なので o のみ
//     (metal のプラズモニクスはマイクロ波 EM では意味を持たない)
//   - antenna は e、acoustic 部材は a
//   - source: Mode source / Gaussian beam は光導波路・ビーム光学専用 (o)。
//     音響の点音源は acoustic カテゴリの Loudspeaker が担うので source は
//     音響では全滅 (e/o のみ)
//   - monitor: Mode expansion は光導波路専用 (o)、Flux (Poynting) は EM/光
//     のみ (e/o)。点/線/面/体積/動画/時間は e/o/a
//   - imported: STL/OBJ は形状取込なので e/o/a、GDSII レイアウトは
//     LayoutGDS タブが光専用なので o
inline constexpr Component kComponents[] = {
    // Basic shapes
    { "basic",    "▭",  "Rectangle",            "直方体",              "eoa" },
    { "basic",    "⬭",  "Circle/Disk",          "円柱",                "eoa" },
    { "basic",    "○",  "Sphere",               "球",                  "eoa" },
    { "basic",    "▱",  "Pyramid",              "四角錐",              "eoa" },
    { "basic",    "⏃",  "Triangle",             "三角形",              "eoa" },
    { "basic",    "⏆",  "Polygon",              "多角形",              "eoa" },
    { "basic",    "⌒",  "Spline",               "自由曲線",            "eoa" },
    // Photonics
    { "photonic", "▬",  "Waveguide (rib)",      "リブ導波路 Si/SiO₂",  "o" },
    { "photonic", "⌑",  "Ring resonator",       "リング共振器",        "o" },
    { "photonic", "≡",  "Bragg grating (DBR)",  "分布Bragg反射器",     "o" },
    { "photonic", "⫝̸",  "Y-branch splitter",    "光分波器",            "o" },
    { "photonic", "⋊",  "Directional coupler",  "方向性結合器",        "o" },
    { "photonic", "⨯",  "MMI splitter",         "多モード干渉計",      "o" },
    { "photonic", "◇",  "Photonic crystal",     "フォトニック結晶",    "o" },
    { "photonic", "◈",  "Grating coupler",      "格子結合器",          "o" },
    { "photonic", "▷◁", "MZI",                  "Mach-Zehnder干渉計",  "o" },
    { "photonic", "⊙",  "Quantum dot",          "量子ドット波源",      "o" },
    // Metal / plasmonics (プラズモニクス = 光専用。EM では意味が薄い)
    { "metal",    "◉",  "Nanoparticle (Au/Ag)", "プラズモニックNP",    "o" },
    { "metal",    "⫾",  "Nanorod",              "ナノロッド",          "o" },
    { "metal",    "◫",  "Nanowire grid",        "ワイヤグリッド偏光子","o" },
    { "metal",    "⊞",  "Bow-tie antenna",      "光アンテナ",          "o" },
    // Gratings / periodic
    { "grating",  "▦",  "1D Grating",           "1次元格子",           "o" },
    { "grating",  "⬚",  "2D Grating",           "2次元格子",           "o" },
    { "grating",  "⌗",  "Metasurface unit",     "メタサーフェス単位胞","o" },
    { "grating",  "⌖",  "Blazed grating",       "ブレーズド格子",      "o" },
    { "grating",  "⎈",  "Polarization grating", "偏光格子",            "o" },
    // Lens
    { "lens",     "◐",  "Plano-convex lens",    "平凸レンズ",          "o" },
    { "lens",     "◑",  "Biconvex lens",        "両凸レンズ",          "o" },
    { "lens",     "◖",  "Aspheric lens",        "非球面",              "o" },
    { "lens",     "⏥",  "Metalens",             "メタレンズ",          "o" },
    { "lens",     "⌧",  "GRIN lens",            "屈折率分布レンズ",    "o" },
    { "lens",     "╲",  "Mirror",               "反射鏡",              "o" },
    { "lens",     "⨀",  "Aperture / Stop",      "絞り",                "o" },
    // Antenna
    { "antenna",  "⊥",  "Dipole",               "ダイポール",          "e" },
    { "antenna",  "▥",  "Patch antenna",        "パッチアンテナ",      "e" },
    { "antenna",  "▽",  "Horn",                 "ホーンアンテナ",      "e" },
    { "antenna",  "⌬",  "Helix",                "ヘリカル",            "e" },
    { "antenna",  "⊟",  "Yagi-Uda",             "八木宇田",            "e" },
    { "antenna",  "▣",  "Array (8×8)",          "アレイアンテナ",      "e" },
    // Acoustic
    { "acoustic", "♫",  "Loudspeaker",          "スピーカー",          "a" },
    { "acoustic", "⌖",  "Microphone",           "マイクロホン",        "a" },
    { "acoustic", "▙",  "Absorber panel",       "吸音パネル",          "a" },
    { "acoustic", "⫽",  "Diffuser (QRD)",       "拡散体",              "a" },
    { "acoustic", "▓",  "Audience block",       "客席ブロック",        "a" },
    // Sources (音響の点音源は Loudspeaker が担うので source は e/o のみ)
    { "source",   "⚡", "Dipole source",        "電気/磁気/光双極子",  "eo" },
    { "source",   "⫴",  "Mode source",          "モード波源",          "o" },
    { "source",   "⤓",  "Plane wave",           "平面波",              "eo" },
    { "source",   "☼",  "Gaussian beam",        "ガウシアンビーム",    "o" },
    { "source",   "⌖",  "TFSF (全/散乱場)",     "TFSF波源",            "eo" },
    { "source",   "⮃",  "Import source",        "スペクトル取込",      "eo" },
    // Monitors
    { "monitor",  "⊙",  "Point monitor",        "点モニター",          "eoa" },
    { "monitor",  "━",  "Line monitor",         "線モニター",          "eoa" },
    { "monitor",  "▭",  "Plane monitor",        "面モニター",          "eoa" },
    { "monitor",  "▦",  "Volume monitor",       "体積モニター",        "eoa" },
    { "monitor",  "⊛",  "Mode expansion",       "モード展開モニター",  "o" },
    { "monitor",  "▶",  "Movie monitor",        "動画",                "eoa" },
    { "monitor",  "≡",  "Flux monitor",         "電力 (Poynting)",     "eo" },
    { "monitor",  "⌛", "Time monitor",         "時間応答",            "eoa" },
    // Imported models — 取込モデル (GeometryTab の STL/OBJ 取込 と LayoutGDS)
    { "imported", "⧉",  "Imported mesh",        "取込3Dモデル (STL/OBJ)", "eoa" },
    { "imported", "▤",  "GDSII layout",         "レイアウト取込 (GDS)",   "o" },
};

// ドメインキー ("em"/"optical"/"acoustic"/"underwater") → 許可フラグ文字。
// 未知のキーは '\0' (どの部品にも一致しない)。
inline char domainFlag(const QString &domain)
{
    if (domain == QLatin1String("em"))         return 'e';
    if (domain == QLatin1String("optical"))    return 'o';
    if (domain == QLatin1String("acoustic"))   return 'a';
    if (domain == QLatin1String("underwater")) return 'u';
    return '\0';
}

// 名前 → 部品定義 (表に無ければ nullptr)。
// 名前には非 ASCII を含むもの ("TFSF (全/散乱場)" 等) があるので
// UTF-8 で比較する。
inline const Component *findByName(const QString &name)
{
    for (const Component &c : kComponents)
        if (name == QString::fromUtf8(c.name)) return &c;
    return nullptr;
}

// 部品がそのドメインで意味を持つか (上の許可表)。
// 表に無い名前・未知のドメインは不許可。水中音響は常に false になる
// (どの部品も 'u' を持たない)。
inline bool allowedInDomain(const QString &name, const QString &domain)
{
    const Component *c = findByName(name);
    const char f = domainFlag(domain);
    if (!c || f == '\0') return false;
    for (const char *p = c->domains; *p; ++p)
        if (*p == f) return true;
    return false;
}

} // namespace ComponentCatalog
} // namespace ofd
