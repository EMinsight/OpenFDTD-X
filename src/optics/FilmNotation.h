// FilmNotation.h — 多層膜の周期記法 (Macleod 記法) のパーサ — Qt 非依存
//
// 薄膜設計の分野で標準的に使われる略記を層列へ展開する。
//
//   Air | (H L)^12 H | Sub    H=Si3N4 L=SiO2 @ 1550nm
//   ↑入射媒質  ↑層構成        ↑基板   ↑記号への材料割当  ↑設計波長
//
// 記法 (H. A. Macleod, "Thin-Film Optical Filters" 4th ed. §2.6 / §5.2 の
// 慣用表記):
//   - 記号 1 文字 (A-Z / a-z) が 1 層。**大小は区別する** (H と h は別)。
//     連結して書ける (`HL` = H の次に L の 2 層)。
//   - 記号の前の数は光学膜厚の係数で、1 = 四分の一波長 (QWOT)。
//     `2L` は半波長層、`0.5H` は八分の一波長層。省略時は 1。
//   - `( … )^N` / `[ … ]^N` は括弧内の繰り返し。入れ子にできる。
//     `^` は省略できない (`(HL)12` は誤り)。
//   - `|` で 入射媒質 | 層構成 | 基板 に区切る。区切りは 0 個または 2 個。
//     0 個なら全体が層構成で、媒質は指定なし (呼び出し側の現状維持)。
//   - 基板の後ろ (3 番目の `|` 以降が無い場合は最後の区画の後ろ) に
//     `記号=材料名` の割当と `@ 波長` を空白区切りで書ける。
//     波長の単位は nm / um / µm / m を付けられる (既定 nm)。
//
// **展開できない入力は必ず false を返す** — 部分的に解釈した結果を返さない
// (誤った層構成を静かに作ると設計そのものが狂う)。
#ifndef OFD_OPTICS_FILMNOTATION_H
#define OFD_OPTICS_FILMNOTATION_H

#include <map>
#include <string>
#include <vector>

namespace ofd {
namespace optics {

// 展開後の 1 層
struct NotationLayer {
    char   symbol = 'H';   // 由来の記号
    double qwot   = 1.0;   // 光学膜厚 n·d/λ₀ の 1/4 波長単位 (1 = QWOT)
};

struct NotationResult {
    bool ok = false;
    std::string error;                       // ok = false のときの理由 (英語)

    std::vector<NotationLayer> layers;       // 入射側 → 基板側
    std::string incident;                    // 入射媒質の記述 (空 = 指定なし)
    std::string substrate;                   // 基板の記述 (空 = 指定なし)
    std::map<char, std::string> assign;      // 記号 → 材料名
    double lambda0_nm = 0.0;                 // `@ …` の設計波長 (0 = 指定なし)
};

// 周期記法を展開する。展開後の層数が maxLayers を超えたら失敗させる
// (`(HL)^100000` のような入力で GUI が固まらないようにするため)。
NotationResult parseNotation(const std::string &text, int maxLayers = 512);

} // namespace optics
} // namespace ofd

#endif // OFD_OPTICS_FILMNOTATION_H
