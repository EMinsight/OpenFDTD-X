// KernelResultReader.h — カーネル出力の結果テキストを読む。
//
// 対象は OpenFDTD 系カーネル (ofd/orcwa/obpm) が作業ディレクトリへ書く:
//   - <kernel>.log : 給電点の周波数特性表
//       feed #N (Z0[ohm] = 50.00)
//         frequency[Hz] Rin[ohm] Xin[ohm] Gin[mS] Bin[mS] Ref[dB] VSWR
//         2.00000e+09  34.621  -104.556  ...
//   - far1d.log     : 遠方界パターン (plotfar1d)
//       #1 : X-plane, frequency[Hz] = 3.00000e+09
//         No. deg E-abs[dB] E-theta[dB] ...
// 書式はカーネル側 (OpenFDTD sol/・post/) が正 — GUI 側でフォーマットを
// 変えない。読めない行は黙って読み飛ばし、表が 1 つも無ければ空を返す。
#pragma once
#include <QString>
#include <QVector>

namespace ofd {

// 給電点 1 つ分の周波数掃引
struct FeedSweepPoint {
    double freqHz = 0;
    double rin = 0, xin = 0;    // 入力インピーダンス [Ω]
    double refDb = 0;           // 反射係数 [dB]
    double vswr = 0;
};
struct FeedSweep {
    int    feedIndex = 1;       // feed #N の N
    double z0 = 50.0;           // 基準インピーダンス [Ω]
    QVector<FeedSweepPoint> points;
};

// 遠方界パターン 1 面分 (far1d.log の 1 ブロック)
struct FarPattern {
    QString plane;              // "X-plane" / "Y-plane" / "Z-plane" / "V" / "H"
    double  freqHz = 0;
    QVector<double> deg;        // 角度 [deg]
    QVector<double> eAbsDb;     // E-abs [dB]
};

// 熱解析レイヤの診断 1 点 (sol/solve.c が周波数ごとに 1 行書く)。
//   Thermal: dissipated[0] = 1.234560e-03 (f=3.000000e+09 Hz)
// 値は **絶対的な W ではなく相対量** — 近傍界 DFT が入射スペクトルで
// 正規化されていないため (カーネル README の注記)。表示側は必ずその旨を
// 添えること (校正なしの絶対値を出さない)。
struct ThermalPoint {
    int    index = 0;        // dissipated[i] の i (frequency2 の並び)
    double freqHz = 0.0;
    double dissipated = 0.0; // 相対値
};

// 2 次元の場マップ (far2d.log / near2d.log)。ev2d / ev3d を使わずに
// アプリ内で描くための素データ。
//
// far2d.log : "No. No. theta[deg] phi[deg] E-abs[dB] …"
//             行頭の 2 つ組が (theta 番号, phi 番号)。
// near2d.log: "No. No. X[m] Y[m] Z[m] E[V/m] …"
//             2 つ組が面内の格子番号。3 座標のうち **変化しない 1 軸**が
//             断面の法線で、残り 2 軸が面内座標になる。
//
// どちらも 1 ブロック = 1 周波数。周波数見出しの行で区切られる。
struct FieldMap {
    QString label;                   // 見出し (周波数など)
    double  freqHz = 0.0;
    int     rows = 0, cols = 0;      // rows = 第 1 番号の数, cols = 第 2 番号
    QVector<double> values;          // rows*cols、行優先
    QString valueName;               // "E-abs[dB]" / "E[V/m]"
    QString rowAxis, colAxis;        // 軸名 ("theta[deg]" / "Y[m]" 等)
    double  rowMin = 0, rowMax = 0, colMin = 0, colMax = 0;

    bool isValid() const
    {
        return rows > 0 && cols > 0
            && values.size() == qsizetype(rows) * qsizetype(cols);
    }
};

// 散乱断面積 (RCS) の 1 周波数分。平面波入射の問題で `sol/outputChars.c` が
// `<kernel>.log` へ書く:
//
//   === cross section ===
//     frequency[Hz] backward[m*m]  forward[m*m]
//       3.00000e+09    1.2594e-02    1.9587e-01
//
// **単位は m² の実値**で、給電電力ではなく入射平面波で正規化されている
// (sol/farfield.c の farfactor が平面波分岐を持つ)。したがってそのまま
// dBsm へ換算できる。給電のある問題では書かれない
// (`sol/outputChars.c:37` — IPlanewave && NFreq2 のときだけ)。
struct CrossSectionPoint {
    double freqHz = 0.0;
    double backward_m2 = 0.0;   // 後方散乱 (モノスタティック RCS)
    double forward_m2 = 0.0;    // 前方散乱
};

// ofd_post が書く「番号付きの表」1 ブロック分 (ev2d を介さないポスト表示)。
//
// ofd_post は作図 (ev.ev2 / ev.ev3) と**同じ内容をテキスト表**にも書く:
//
//   feed.log   : feed #N (waveform) / (spectrum)   post/plot2dFeed.c
//   point.log  : point #N (waveform) / (spectrum)  post/plot2dPoint.c
//   far0d.log  : theta=… phi=… の周波数特性        post/outputFar0d.c
//   near1d.log : #N : frequency[Hz] = … の線上分布 post/outputNear1d.c
//
// どれも「見出し行 → 列見出し行 → 先頭が通し番号の数値行」という同じ形を
// している (先頭列は No.)。したがって 1 つのパーサで全部読める。列の**意味**
// はカーネル側が正なので、GUI は列名をそのまま持ち回り、解釈を足さない。
//
// 横軸は「表の中で値が変化する最初の列」を採る。near1d.log は X/Y/Z の 3 列を
// 持ち、線に沿って動くのはそのうち 1 つだけなので、先頭列固定では線上分布に
// ならない。値が変わらなかった列は `fixed` に文字列として退避する
// (捨てずに画面へ出す — 断面位置の情報そのものなので)。
struct PostTable {
    QString     sourceFile;    // "feed.log" 等 (どのファイル由来かを画面に出す)
    QString     title;         // 見出し行そのまま ("feed #1 (waveform)")
    QString     xName;         // 横軸に選んだ列名 ("time[sec]" / "frequency[Hz]")
    QStringList yNames;        // 残りの列名 ("V[V]", "I[A]" …)
    QString     fixed;         // 表の中で値が変わらなかった列 ("X[m]=0.000 …")
    QVector<double>          x;
    QVector<QVector<double>> y;   // yNames と同じ本数、各要素は x と同じ長さ
    // 元の行数。間引いた場合だけ x.size() より大きくなる (下記 kMaxTableRows)。
    // 画面に「N 行中 M 行」と出すためのもの — 黙って切り捨てない。
    int totalRows = 0;
    bool decimated() const { return totalRows > x.size(); }

    bool isValid() const
    {
        if (x.isEmpty() || y.isEmpty() || y.size() != yNames.size())
            return false;
        for (const QVector<double> &c : y)
            if (c.size() != x.size()) return false;
        return true;
    }
};

namespace KernelResultReader {

// 1 ブロックあたりに保持する最大行数 (大規模データ対策)。
// 実際の反復回数は普通これより桁で小さいので通常は効かないが、効いたときは
// `PostTable::totalRows` に元の行数が残り、画面に間引きを明示する。
constexpr int kMaxTableRows = 200000;

// <kernel>.log から給電点表を読む (見つからなければ空)
QVector<FeedSweep> readFeedSweeps(const QString &logPath);
QVector<FeedSweep> parseFeedSweeps(const QString &text);

// far2d.log / near2d.log を 2 次元マップとして読む (見つからなければ空)。
// 周波数ブロックごとに 1 つ返す。
QVector<FieldMap> readFar2d(const QString &path);
QVector<FieldMap> parseFar2d(const QString &text);
QVector<FieldMap> readNear2d(const QString &path);
QVector<FieldMap> parseNear2d(const QString &text);

// <kernel>.log から熱解析の診断行を読む (見つからなければ空)
QVector<ThermalPoint> readThermal(const QString &logPath);
QVector<ThermalPoint> parseThermal(const QString &text);

// <kernel>.log の "=== cross section ===" を読む (見つからなければ空)
QVector<CrossSectionPoint> readCrossSection(const QString &logPath);
QVector<CrossSectionPoint> parseCrossSection(const QString &text);

// far1d.log から遠方界パターンを読む (見つからなければ空)
QVector<FarPattern> readFar1d(const QString &path);
QVector<FarPattern> parseFar1d(const QString &text);

// ofd_post の番号付き表 (feed.log / point.log / far0d.log / near1d.log) を
// 読む。sourceFile には見出し用の表示名 (既定はファイル名) が入る。
QVector<PostTable> readPostTables(const QString &path);
QVector<PostTable> parsePostTables(const QString &text,
                                   const QString &sourceFile = QString());

} // namespace KernelResultReader
} // namespace ofd
