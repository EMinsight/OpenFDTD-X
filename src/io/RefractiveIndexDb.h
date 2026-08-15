// RefractiveIndexDb.h — refractiveindex.info データベースの読み手
//
// 対象は公開データベース (CC0 1.0 / パブリックドメイン) の 2 種類のファイル:
//
//   catalog-nk.yml … shelf / book / page の木。page ごとに data: の相対パス
//   <data>.yml     … DATA: の下に「表」か「式」で n,k が入っている
//
// **YAML 汎用パーサではない。** 依存を増やさない方針 (Qt6 Widgets のみ) なので、
// このデータベースが実際に使っている書き方だけを読む。実ファイルで確認した形:
//
//   DATA:
//     - type: tabulated nk        … data: | の後にインデントされた「λ n k」行
//     - type: tabulated k         … 「λ k」行 (式と組で使われる。実例 N-BK7)
//     - type: formula 2           … wavelength_range: と coefficients:
//
// 注意: DATA: の外にも `- type: formula A` (PROPERTIES の熱分散) が現れるので、
// **列 0 の次のキーで DATA: 節を打ち切る**こと (実ファイル N-BK7.yml で確認)。
//
// 式は Sellmeier の 1 と 2 だけ対応する。どちらも公表値と照合済み:
//   formula 1 : 溶融石英 (Malitson) n(0.5876/1.0/0.3 µm) = 1.45846/1.45042/1.48779
//   formula 2 : N-BK7 n(0.5876/0.6563/0.4861 µm) = 1.51680/1.51432/1.52238
// 3〜9 は対応しない (式の定義を実データで検証できていないため — 当てずっぽうで
// 実装して静かに誤った屈折率を出すより、対応外と言う方がよい)。
#ifndef OFD_IO_REFRACTIVEINDEXDB_H
#define OFD_IO_REFRACTIVEINDEXDB_H

#include <QByteArray>
#include <QString>
#include <QVector>

#include <utility>
#include <vector>

#include "NkCsv.h"

namespace ofd {

struct RiEntry {
    QString shelf, book, page;     // 識別子 (URL 組み立てにも使う)
    QString bookName, pageName;    // 画面表示用の名前
    QString dataPath;              // 例 "main/Ag/nk/Johnson.yml"

    QString label() const;         // "Ag (Silver) / Johnson and Christy 1972…"
};

// カタログ (catalog-nk.yml) を読む。壊れていれば空を返す。
QVector<RiEntry> parseRiCatalog(const QByteArray &yaml);

struct RiData {
    int     formula = 0;                     // 0 = 式は無い
    double  fMin_um = 0.0, fMax_um = 0.0;    // 式の有効範囲
    QVector<double> coeff;

    std::vector<optics::NkSample>         nkTable;  // tabulated nk / n
    std::vector<std::pair<double, double>> kTable;  // tabulated k (λ_um, k)

    QString reference;      // REFERENCES (出典表示に使う)
    QString comments;       // COMMENTS
    QString error;          // ok == false のときの理由
    bool    ok = false;

    bool hasFormula()   const { return formula > 0; }
    bool hasTable()     const { return !nkTable.empty(); }
};

// データファイル 1 個を読む。
RiData parseRiData(const QByteArray &yaml);

// 対応している式か (1, 2 のみ)。
bool riFormulaSupported(int formula);

// 式から n を求める。範囲外・対応外は false。
bool riEvalN(const RiData &d, double lambda_um, double *n);

// REFERENCES / COMMENTS を平文にする。
// このフィールドには HTML (<a href> / <i> / <b>) が入っており、2026-06 の
// 上流通知で **Markdown も入る**ことになった。画面へそのまま流すと QLabel の
// 既定 (Qt::AutoText) が HTML と解釈して表示が崩れるので、タグと Markdown の
// 装飾記号を落として平文にしてから出す。
QString riPlainText(const QString &s);

// 取り込み用の n,k 表にする。
// - 表があればそれを使う (k の表があれば重ねる)
// - 表が無く式だけなら、有効範囲を samples 点で刻む
//   (k の表があればその波長で n を求め、k と対にする)
NkTable riToNkTable(const RiData &d, int samples = 200);

} // namespace ofd

#endif // OFD_IO_REFRACTIVEINDEXDB_H
