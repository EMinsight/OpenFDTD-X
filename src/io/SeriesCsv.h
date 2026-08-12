// SeriesCsv.h — (x, y) の 2 列 CSV を系列として読む。
//
// 参照データ (別のソルバー・実測・文献値・クラウドの結果) を突き合わせる
// (`core/SeriesCompare`) ための入口。**タブごとに書かない** — 検証タブと
// tidy3d タブが同じ規則で読むように 1 か所に置く
// (`.claude/rules/gui.md`「タブ間で共有できるヘルパーはコピーせず抽出」)。
//
// 読み方の規則:
//   * 空行と `#` で始まる行は読み飛ばす。
//   * 区切りはカンマ・セミコロン・空白のいずれか (混在可)。
//   * **数値に読めない行は捨てる** — 見出し行はこれで自然に落ちる。
//     足りない列を 0 で埋めることはしない。
//   * x が昇順でなければ**昇順に並べ替える** (補間が前提とするため)。
//     同じ x が複数あっても落とさない (呼び出し側が気づけるように残す)。
#pragma once
#include <QString>

#include "../core/SeriesCompare.h"

namespace ofd {
namespace io {

// 何列目を x / y にするか (0 起点)。既定は 1 列目と 2 列目。
struct SeriesCsvOptions {
    int xCol = 0, yCol = 1;
};

// 読めた点が 2 点未満なら valid() == false の系列を返す。
cmp::Series parseSeriesCsv(const QString &text,
                           const SeriesCsvOptions &opt = SeriesCsvOptions());
cmp::Series readSeriesCsv(const QString &path,
                          const SeriesCsvOptions &opt = SeriesCsvOptions());

} // namespace io
} // namespace ofd
