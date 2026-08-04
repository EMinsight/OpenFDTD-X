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

namespace KernelResultReader {

// <kernel>.log から給電点表を読む (見つからなければ空)
QVector<FeedSweep> readFeedSweeps(const QString &logPath);
QVector<FeedSweep> parseFeedSweeps(const QString &text);

// far1d.log から遠方界パターンを読む (見つからなければ空)
QVector<FarPattern> readFar1d(const QString &path);
QVector<FarPattern> parseFar1d(const QString &text);

} // namespace KernelResultReader
} // namespace ofd
