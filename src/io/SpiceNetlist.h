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

class SpiceIO {
public:
    static SpiceNetlist parse(const QString &text);
    // ファイルから読む。開けなければ false (err に理由)。
    static bool read(const QString &path, SpiceNetlist &out, QString *err = nullptr);

    // SPICE の数値表記 → SI 値。解釈できなければ false。
    static bool parseValue(const QString &token, double *out);

    // ファイルダイアログのフィルタ
    static QString fileDialogFilter();
};

} // namespace ofd
