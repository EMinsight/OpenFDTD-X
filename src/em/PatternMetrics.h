// PatternMetrics.h — 遠方界パターン 1 面から読み取れる指標 (Qt 非依存 / C++17)
//
// アンテナ特性タブの「放射」項目のうち、**far1d.log の 1 切断面だけで確定する
// もの**をここで計算する。指向性・放射効率・全放射電力は全球積分が要るので
// ここには入れない (far1d は切断面しか持たない — 画面ではその旨を出す)。
//
// 入力は角度 [deg] と E-abs [dB] の対 (`io/KernelResultReader` の FarPattern)。
// 角度は昇順で、0〜360 を 1 周する前提 (OpenFDTD の出力がそうなっている)。
//
// 定義 (いずれも IEEE Std 145-2013 の用語):
//   ピーク       最大値とその角度
//   3 dB 幅      主ビームのピークから両側 −3 dB へ落ちる点の間隔 (HPBW)
//   SLL          主ビームの外側で最も高いローブの、ピークからの差 [dB] (負値)
//   F/B          ピーク方向と、その 180° 逆方向の差 [dB]
//
// **主ビームの境界は「ピークから両側へ下って最初に上向きへ転じる点」**とする
// (最初のヌル)。そこから外側だけを SLL の探索範囲にする。ヌルが見つからない
// (単調に下がり続ける) 場合は SLL を求めない — 無理に値を出さない。
#ifndef OFD_EM_PATTERNMETRICS_H
#define OFD_EM_PATTERNMETRICS_H

#include <vector>

namespace ofd {
namespace em {

struct PatternMetrics {
    double peakDb = 0.0;      // ピーク値 [dB] (給電問題では dBi)
    double peakDeg = 0.0;     // ピーク方向 [deg]
    double hpbwDeg = 0.0;     // 3 dB 幅 [deg]。両側が取れなければ 0
    double sllDb = 0.0;       // サイドローブレベル [dB] (ピーク基準の負値)
    double fbDb = 0.0;        // 前後比 [dB]
    bool   hasPeak = false;
    bool   hasHpbw = false;   // 両側で −3 dB を跨いだか
    bool   hasSll = false;    // 主ビームの外にローブが見つかったか
    bool   hasFb = false;     // 逆方向のデータがあるか
};

// 角度 [deg] と値 [dB] の対から指標を求める。
// 点数が 3 未満、または長さが違えば hasPeak = false の空を返す。
PatternMetrics patternMetrics(const std::vector<double> &deg,
                              const std::vector<double> &db);

} // namespace em
} // namespace ofd

#endif // OFD_EM_PATTERNMETRICS_H
