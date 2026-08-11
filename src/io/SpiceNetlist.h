// SpiceNetlist.h — SPICE ネットリスト (.cir / .sp / .net) の読み取り
//
// 回路系電磁解析タブ「SPICE 連成」の取込の実体。外部で描いた回路図の
// ネットリストから **集中定数素子 (R / L / C)** を取り出し、
//   - 素子表として見せる
//   - `.ofd` の `load = <dir> <x> <y> <z> <R|L|C> <value>` 行として
//     プロジェクトへ入れる (配置は利用者が与える)
// のに使う。`load` は本家 OpenFDTD の入力キーそのものなので、取り込んだ
// 素子はカーネルまで届く (GUI 内で完結しない)。
//
// ── 対応範囲 ────────────────────────────────────────────────────────────────
//   - 2 端子の R / L / C 行: `Rname n1 n2 <value>`
//   - 先頭行はタイトル (SPICE の慣習。回路行として解釈しない)
//   - コメント: 行頭 `*`、および行中の `;` 以降
//   - 継続行: 行頭 `+` は直前の行へ連結する
//   - `.subckt` … `.ends` の中身は**展開せずに読み飛ばす** (件数を warnings へ)
//   - その他の `.` ディレクティブ (.model/.tran/.ac/.end …) は読み飛ばす
//   - R/L/C 以外の素子行 (V, I, D, Q, M, X …) は読み飛ばし、種類ごとに集計
//
// **読み飛ばしたものは必ず warnings に残す。** 黙って落とすと「取り込んだ
// つもりの素子が計算に入っていない」事故になる (絶対規則 5)。
//
// ── 数値の書式 (SPICE の接尾辞) ─────────────────────────────────────────────
//   T=1e12  G=1e9  MEG=1e6  K=1e3  M=1e-3  MIL=25.4e-6
//   U=1e-6  N=1e-9  P=1e-12  F=1e-15
// **`M` はミリ、`MEG` がメガ** (SPICE 固有の落とし穴)。大文字小文字は区別
// しない。接尾辞の後ろの文字は単位とみなして読み捨てる (`4.7kOhm` = 4700)。
#pragma once
#include <QChar>
#include <QString>
#include <QStringList>
#include <QVector>

namespace ofd {

// 2 端子の集中定数素子 1 個
struct SpiceElement {
    QChar   type = 'R';     // 'R' / 'L' / 'C'
    QString name;           // "R1" など (ネットリストの表記のまま)
    QString node1, node2;   // 接続ノード名
    double  value = 0.0;    // SI 単位 (Ω / H / F)
};

struct SpiceNetlist {
    QString               title;        // 先頭行
    QVector<SpiceElement> elements;     // R / L / C のみ
    QStringList           nodes;        // 出現順のノード名 (重複なし)
    QStringList           warnings;     // 読み飛ばしたものの説明
    int                   skippedSubckts = 0;
    int                   skippedElements = 0;   // R/L/C 以外の素子行

    bool isValid() const { return !elements.isEmpty(); }
    int count(QChar type) const;        // 種別ごとの個数
};

// 書き出す等価回路 1 個 (抽出結果 → SPICE サブサーキット)。
// 直列 / 並列の別と R / L / C の値だけを持つ — 抽出が出すのはこれだけで、
// それ以上を「それらしく」書き足さない。
struct SpiceSubckt {
    QString name = QStringLiteral("EXTRACTED");
    bool    series = true;      // true = R-L-C 直列、false = 並列
    double  r_ohm = 0.0;        // 0 以下の素子は書かない (存在しない扱い)
    double  l_h = 0.0;
    double  c_f = 0.0;
    QString comment;            // 冒頭のコメント行 (出所を残す)

    // 書ける素子が 1 つ以上あるか
    bool hasAny() const { return r_ohm > 0.0 || l_h > 0.0 || c_f > 0.0; }
};

class SpiceIO {
public:
    static SpiceNetlist parse(const QString &text);
    // ファイルから読む。開けなければ false (err に理由)。
    static bool read(const QString &path, SpiceNetlist &out, QString *err = nullptr);

    // SPICE の数値表記 → SI 値。解釈できなければ false。
    static bool parseValue(const QString &token, double *out);

    // ファイルダイアログのフィルタ
    static QString fileDialogFilter();

    // ── 書き出し ────────────────────────────────────────────────────────
    // 抽出した等価回路を 2 端子の .subckt として組み立てる。
    // 端子は 1 (入力) と 2 (出力)。直列は 1 → 内部ノード → 2 と数珠つなぎ、
    // 並列は全素子を 1-2 間に並べる。値は %.6g の指数表記で書く
    // (SPICE の接尾辞は使わない — M がミリかメガかで事故るため)。
    // hasAny() が false のときは空文字列を返す (空の subckt を作らない)。
    static QString buildSubckt(const SpiceSubckt &s);
    // 上を .cir として書き出す。開けなければ false (err に理由)。
    static bool writeSubckt(const QString &path, const SpiceSubckt &s,
                            QString *err = nullptr);
};

} // namespace ofd
