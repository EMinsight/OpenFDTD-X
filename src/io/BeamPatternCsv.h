// BeamPatternCsv.h — 計測した送波器の指向パターン (CSV) の読み込み。
//
// `core/SourceDirectivity` は**閉形式**のパターン (ガウス開口 / 直線開口) を
// 作るが、実際の送波器は水槽で測った表を持っていることが多い。その表を
// 取り込んで BELLHOP の `.sbp` として渡すための入口がここ。
//
// ── 受け付ける書式 ────────────────────────────────────────────────────────
// 1 行 1 点の「角度, レベル」。区切りはカンマ / タブ / 空白のいずれでもよい
// (計測器の書出しが揃っていないため)。
//   * `#` `!` `;` で始まる行と空行は註釈として読み飛ばす
//   * 数値として読めない行が先頭に 1 行だけあるときは見出し行とみなす
//     (`angle,level` のような列名)。2 行目以降に現れたら誤りとして弾く
//   * 角度は [deg]、レベルは [dB] (相対)。順序は問わない (昇順に並べ替える)
//
// ── 正規化 (ここで決めていること) ─────────────────────────────────────────
// **最大値が 0 dB になるよう平行移動する。** BELLHOP は .sbp の dB を
// 10^(dB/20) の**相対振幅**として使うので、表全体の定数オフセットは全ビームを
// 一様に増減させ、TL の絶対値をずらしてしまう。計測値は校正状態がまちまち
// (器差・距離補正の有無) なので、そのまま渡すと「知らないうちに音源レベルを
// 変えた」ことになる。ピークを 0 dB に揃えるのは指向パターンとしての定義
// (b(0) = 1) に沿うし、閉形式の側とも一致する。**平行移動量は呼び手へ返す**
// ので、画面に出して黙って変えていないことを示せる。
//
// ── 弾くもの (静かに通さない) ─────────────────────────────────────────────
//   * 点が 2 個未満 / 角度が重複 / 角度が [-180, 180] の外
//   * 数値でない列、列数が 2 に満たない行
//   * 有限でない値 (inf / nan)
// いずれも理由を err に入れて空を返す。**部分的に読めた分だけ使うことはしない**
// — 欠けた表は指向パターンとしては別物になるため。
#ifndef OFD_IO_BEAMPATTERNCSV_H
#define OFD_IO_BEAMPATTERNCSV_H

#include <QString>
#include <QVector>
#include "../core/Project.h"

namespace ofd {
namespace beamcsv {

struct Result {
    QVector<BeamPatternPoint> points;   // 角度昇順・ピーク 0 dB
    double shift_dB = 0.0;              // 正規化で引いた量 (= 元のピーク)
    int    skipped = 0;                 // 読み飛ばした註釈・見出しの行数 (空行は数えない)
    bool   ok() const { return points.size() >= 2; }
};

// CSV 本文を読む。失敗したら points は空で err に理由 (I18n 済みではない —
// 呼び手が I18n のテンプレートへ差し込む)。
Result parse(const QString &text, QString *err = nullptr);

// ファイルから読む (読めない/大きすぎる場合も err に理由)。
Result load(const QString &path, QString *err = nullptr);

// 取り込んだ表を .sbp の行へ整形する (BellhopIO::sbpText が使う)。
// floorDb より下はクリップする (.sbp は −∞ を持てない)。
QVector<BeamPatternPoint> clampToFloor(const QVector<BeamPatternPoint> &pts,
                                       double floorDb);

} // namespace beamcsv
} // namespace ofd

#endif // OFD_IO_BEAMPATTERNCSV_H
