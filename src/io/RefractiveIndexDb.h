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
// 式は 1〜9 に対応する。定義は上流リポジトリ同梱の公式仕様書
// database/doc/"Dispersion formulas.pdf" (RefractiveIndex.INFO, 2014-06-29):
//   1: Sellmeier      n²−1 = C1 + Σ C(2i)·λ²/(λ²−C(2i+1)²)
//   2: Sellmeier-2    n²−1 = C1 + Σ C(2i)·λ²/(λ²−C(2i+1))
//   3: Polynomial     n²   = C1 + Σ C(2i)·λ^C(2i+1)
//   4: RefractiveIndex.INFO
//                     n²   = C1 + C2·λ^C3/(λ²−C4^C5) + C6·λ^C7/(λ²−C8^C9)
//                               + Σ_{i≥10} C(2i)·λ^C(2i+1)
//   5: Cauchy         n    = C1 + Σ C(2i)·λ^C(2i+1)
//   6: Gases          n−1  = C1 + Σ C(2i)/(C(2i+1) − λ⁻²)
//   7: Herzberger     n    = C1 + C2/(λ²−0.028) + C3·(1/(λ²−0.028))²
//                               + C4λ² + C5λ⁴ + C6λ⁶
//   8: Retro          (n²−1)/(n²+2) = C1 + C2·λ²/(λ²−C3) + C4λ² (→ n² に逆変換)
//   9: Exotic         n²   = C1 + C2/(λ²−C3) + C4(λ−C5)/((λ−C5)²+C6)
// 実データのアンカー (selftest):
//   formula 1 : 溶融石英 (Malitson) n(0.5876/1.0/0.3 µm) = 1.45846/1.45042/1.48779
//   formula 2 : N-BK7 n(0.5876/0.6563/0.4861 µm) = 1.51680/1.51432/1.52238
//   formula 4 : Si (Chandler-Horowitz) n(10.6 µm) = 3.4179 (CO2 レーザーの定番値)
//   formula 6 : 空気 (Ciddor) n−1 (633 nm) = 2.765e-4 (公表の空気屈折率)
//   formula 3 : CCl4 (Moutzouris) n(589 nm) ≈ 1.457 (CRC n_D 1.4601、温度差込み)
// 5/7/8/9 は仕様書どおりの厳密な恒等式 (係数の極限・特別な λ) で判定する。
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

// 対応している式か (1〜9)。
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
