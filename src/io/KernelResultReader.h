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

namespace KernelResultReader {

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

} // namespace KernelResultReader
} // namespace ofd
