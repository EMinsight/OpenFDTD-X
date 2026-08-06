// ArrReader.h — BELLHOP の到達ファイル (<ケース名>.arr) の読み取りと、
// そこからの受信インパルス応答 (IR) の合成。
//
// 「計算モード = 到達時間」(RunType 'A') で実行すると、bellhopcxx は受波器
// ごとの到達を ASCII で書き出す (bellhopcuda src/mode/arr.cpp WriteOutArrivals)。
// 2D の並びは:
//
//   '2D'
//   freq0
//   NSz  Sz[0..NSz-1]
//   NRz  Rz[0..NRz-1]          … [m]
//   NRr  Rr[0..NRr-1]          … [m] (.env は km だが実行時に m へ変換される)
//   音源ごと: maxn
//     受波器ごと (深度 iz → 距離 ir): narr
//       到達ごと: a  phase[deg]  delay_re[s]  delay_im[s]
//                 SrcDeclAngle  RcvrDeclAngle  NTopBnc  NBotBnc
//
// 到達は「振幅・位相・遅延」の組なので、そのまま **疎なインパルス応答**
// として使える。合成した IR を WAV にすれば、既存の畳み込み
// (acoustics/core/ConvolutionEngine) と可聴化タブがそのまま使える。
//
// **重要な制限**: 到達の振幅は .env の FREQ 1 波数で計算された値なので、
// 合成した IR は **その周波数の近傍でしか正しくない**。広帯域の音を作るには
// 周波数ごとに実行して合成する必要がある (未実装)。呼び出し側は必ずこの旨を
// 利用者に表示すること。
#pragma once
#include <QString>
#include <QVector>

namespace ofd {

// 到達 1 本
struct ArrArrival {
    double amp = 0.0;           // 振幅 (絶対値)
    double phaseDeg = 0.0;      // 位相 [deg]
    double delayS = 0.0;        // 到達時刻 (複素遅延の実部) [s]
    double delayImagS = 0.0;    // 複素遅延の虚部 (吸収媒質)
    double srcAngleDeg = 0.0;   // 射出角
    double rcvAngleDeg = 0.0;   // 到来角
    int    nTop = 0, nBot = 0;  // 海面・海底の反射回数
};

// .arr のヘッダ (受波器格子)
struct ArrHeader {
    double freqHz = 0.0;
    QVector<double> sz;   // 音源深度 [m]
    QVector<double> rz;   // 受波器深度 [m]
    QVector<double> rr;   // 受波器距離 [m]
};

// 合成した IR の付帯情報 (利用者への説明に使う)
struct IrSynthInfo {
    int    arrivals = 0;        // 使った到達の本数
    double firstDelayS = 0.0;   // 最初の到達 (直接波) の時刻
    double lastDelayS = 0.0;    // 最後の到達
    double peak = 0.0;          // 波形の最大絶対値
    int    length = 0;          // サンプル数
    double dropped = 0;         // 範囲外で捨てた到達の本数
};

class ArrReader {
public:
    // ヘッダ (周波数と受波器格子) だけを読む
    static bool readHeader(const QString &path, ArrHeader &out,
                           QString *err = nullptr);

    // 受波器 1 点 (深度 index iz, 距離 index ir) の到達列を取り出す。
    // 巨大なファイル (受波器 101×201 で 140 MB 級) になるため全体は
    // 保持せず、目的の受波器まで読み飛ばす。
    static bool readArrivals(const QString &path, int iz, int ir,
                             ArrHeader &header, QVector<ArrArrival> &out,
                             QString *err = nullptr);
};

// 到達列 → 実数インパルス応答。
// 到達を「振幅 a・位相 φ」の複素振幅 A = a·e^{iφ} とみなし、その実部
// (a·cos φ) を遅延位置へ置く。サンプル格子に丸めると櫛形フィルタの
// 位置がずれるので、**Hann 窓付き sinc の分数遅延** で補間する。
// tailS は最後の到達の後に付ける余白 [s]。
QVector<double> synthesizeIr(const QVector<ArrArrival> &arrivals, double fsHz,
                             double tailS = 0.05, IrSynthInfo *info = nullptr);

} // namespace ofd
