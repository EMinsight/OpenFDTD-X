// NkCsv.h — 実測 n,k テーブル (CSV / TSV / 空白区切り) の読み込み。
//
// 材料Explorer の「n,k 取込」。内蔵データベースは公刊 Sellmeier 係数から
// 作った n(λ) しか持たないので、利用者の実測値を読んで
// `optics/DispersionFit` に当てられるようにする。
//
// ── 読める形 ──────────────────────────────────────────────────────────────
//   波長, n            (k 列なし)
//   波長, n, k
// 区切りは , / ; / TAB / 空白のどれでもよく、改行は CRLF / LF どちらでもよい。
// 先頭に数値で始まらない行があればヘッダとして読み飛ばす。`#` 始まりの行と
// 空行も読み飛ばす。
//
// ── 波長の単位を黙って決めない ────────────────────────────────────────────
// この種のファイルは nm 表記と μm 表記が混在して出回る (refractiveindex.info
// の書出は μm、測定器の書出は nm が多い)。**推測した単位は必ず報告する**:
//
//   1. ヘッダに単位語 (nm / um / μm / micron / m) があればそれに従う
//   2. 無ければ波長の中央値の桁で決める
//        中央値 < 0.01      → m   (例 5.5e-7)
//        0.01 ≦ 中央値 < 100 → μm  (例 0.55)
//        100 ≦ 中央値        → nm  (例 550)
//      光学の波長域ではこの 3 つは 4 桁以上離れているので取り違えない。
//
// どちらの規則で決めたかを `unitFromHeader` に入れて返すので、呼び出し側は
// 画面に「この単位と解釈した」と出す。**利用者が単位を上書きできること**が
// 前提の設計で、当てずっぽうを黙って通さない。
//
// ── k 列が無いときは 0 ではなく「データ無し」──────────────────────────────
// `optics::NkSample::k` は負値が「k のデータを持たない」を表す約束
// (0 を入れると「吸収ゼロを実測した」ことになってしまう)。k 列が無い
// ファイルは k = −1 のまま返す。
//
// ── 壊れた入力は部分的に受け入れない ──────────────────────────────────────
// 数値として読めない行が 1 行でもあれば、その行を読み飛ばした事実を
// `skipped` に数える。**有効な点が 2 点未満なら全体を失敗**にして points を
// 空にする (中途半端な曲線を返さない)。波長は昇順に並べ替え、重複した
// 波長は 1 点にまとめる (後勝ちではなく最初の値を採る)。
#pragma once
#include <QString>

#include <vector>

#include "../optics/DispersionFit.h"

namespace ofd {

struct NkTable {
    std::vector<optics::NkSample> points;  // 波長昇順。k < 0 は「データ無し」
    bool    ok = false;
    bool    hasK = false;            // k 列があったか
    QString unit;                    // 解釈した波長の単位 ("nm" / "um" / "m")
    bool    unitFromHeader = false;  // true = ヘッダの単位語に従った
    int     rows = 0;                // 数値として読めた行数
    int     skipped = 0;             // 読み飛ばした行数 (ヘッダ・コメント除く)
    int     duplicates = 0;          // 同じ波長が重複していた数
    QString error;                   // ok == false のときの理由

    double  minLambda_um() const {
        return points.empty() ? 0.0 : points.front().lambda_um;
    }
    double  maxLambda_um() const {
        return points.empty() ? 0.0 : points.back().lambda_um;
    }
};

// ファイルを読む。unitOverride が空でなければ ("nm"/"um"/"m") 桁の推測より
// 優先する (利用者が画面で選び直したとき)。
NkTable readNkCsv(const QString &path, const QString &unitOverride = QString());

// 文字列から読む (selftest 用。ファイル入出力を挟まずに判定できる)
NkTable parseNkCsv(const QString &text,
                   const QString &unitOverride = QString());

} // namespace ofd
