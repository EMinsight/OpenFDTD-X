// AbsorptionCsv.h — 吸音率 α のオクターブバンド表 (CSV / TSV) の読み込みと NRC。
//
// 室内音響タブの「材質 DB 取込」。内蔵の 9 材質は代表値の例示なので、
// 実測値やメーカー公表値を読み込んで表に足せるようにする。
//
// ── 読める形 ──────────────────────────────────────────────────────────────
//   名称, α125, α250, α500, α1k, α2k, α4k
// 区切りは , / ; / TAB。**名称に空白が入りうるので空白は区切りにしない**
// (n,k テーブルの `io/NkCsv` と違う点)。改行は CRLF / LF どちらでもよい。
// 先頭に数値列を持たない行があればヘッダとして読み飛ばす。`#` 始まりの行と
// 空行も読み飛ばす。
//
// ── NRC は表に書かせず、必ずここで計算する ────────────────────────────────
// NRC (Noise Reduction Coefficient) は ASTM C423 の定義で
//
//     NRC = round0.05( (α250 + α500 + α1k + α2k) / 4 )
//
// (250/500/1k/2k の 4 バンドの算術平均を 0.05 刻みへ丸める。125 Hz と 4 kHz は
// 入らない)。**定義を 1 箇所に閉じ込めるための関数**で、表へ直接書いた値と
// 計算値が食い違う事故を防ぐ (内蔵表で実際に 1 行食い違っていた —
// コンクリートだけ平均を小数 2 桁へ丸めた別の流儀の値が入っていた)。
//
// ── α > 1 は捨てない ──────────────────────────────────────────────────────
// 残響室法 (ISO 354) の吸音率は試料端部の回折で 1 を超えることが普通にある。
// **測定値として正当**なので弾かず、そのまま保持して「1 を超える値がある」と
// 数だけ報告する。負値と非有限値は物理的にありえないので行ごと捨てる。
#pragma once
#include <QString>
#include <QStringList>

#include <vector>

namespace ofd {

// オクターブバンド 125 / 250 / 500 / 1k / 2k / 4k Hz の吸音率
struct AbsorptionMaterial {
    QString name;
    double  alpha[6] = { 0, 0, 0, 0, 0, 0 };
};

struct AbsorptionTable {
    std::vector<AbsorptionMaterial> materials;
    bool    ok = false;
    int     rows = 0;         // 数値として読めた行数
    int     skipped = 0;      // 読み飛ばした行数 (ヘッダ・コメント除く)
    int     overUnity = 0;    // α > 1 の値を含む材質の数 (残響室法では正常)
    QString error;
};

// ASTM C423 の NRC。250/500/1k/2k の平均を 0.05 刻みへ丸める
// (ちょうど半分は上側。0.05 で割らず 20 を掛ける — 実装のコメント参照)。
double nrcFromAlpha(const double alpha[6]);

AbsorptionTable readAbsorptionCsv(const QString &path);
AbsorptionTable parseAbsorptionCsv(const QString &text);   // selftest 用

} // namespace ofd
