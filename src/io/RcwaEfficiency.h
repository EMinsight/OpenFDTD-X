// RcwaEfficiency.h — orcwa が出す rcwa_efficiency.csv を読む。
//
// 書式はカーネル側 (`OpenRCWA/sol/rcwa_bridge.cpp`) の書き出しが正:
//
//     frequency[Hz],lambda[m],R_TE,T_TE,R_TM,T_TM
//
// R / T は**電力の反射率・透過率** (回折次数を全部足したもの) で、無次元の
// 0..1。dB ではない。
//
// **給電点の反射 Ref[dB] とは別物である。** `<kernel>.log` の Ref[dB] は
// 給電点から見た整合の良さ (ポート反射) で、こちらは平面波の電力反射率。
// 定義も規格化も違うので、**この 2 つを突き合わせてはいけない**。
#pragma once
#include <QString>
#include <QVector>

namespace ofd {
namespace rcwa {

struct EfficiencyPoint {
    double freqHz = 0.0, lambda_m = 0.0;
    double rTE = 0.0, tTE = 0.0, rTM = 0.0, tTM = 0.0;
};

struct Efficiency {
    QVector<EfficiencyPoint> points;
    bool valid() const { return points.size() >= 1; }
    // 無損失なら R + T = 1。**最大のずれ**を返す (偏波の悪い方)。
    // 点が無ければ 0。
    double worstEnergyError() const;
    // そのずれが起きた周波数 [Hz] (点が無ければ 0)
    double worstEnergyFreqHz() const;
};

// ファイルから読む (無ければ空)。見出し行は読み飛ばし、数値が 6 個そろって
// いない行は**捨てる** (足りない列を 0 で埋めない)。
Efficiency read(const QString &csvPath);
Efficiency parse(const QString &text);

} // namespace rcwa
} // namespace ofd
