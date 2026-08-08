// SweepDeconvolution.h — 指数掃引正弦波 (ESS) の生成と逆畳み込み。
// Qt 非依存 / C++14。
//
// 出典: A. Farina, "Simultaneous Measurement of Impulse Response and
//       Distortion with a Swept-Sine Technique", AES 108th Convention,
//       Preprint 5093 (2000)。
//
// 実測の現場では、インパルス応答そのものではなく **掃引正弦波を再生した
// 録音** が手元にある。ESS を逆フィルタで畳み込むと線形インパルス応答が
// 得られ、しかも非線形歪み (高調波) は線形応答より **前** の時刻へ分離して
// 現れる — これが ESS 法の要点で、インパルス加振より S/N が桁違いに良い。
//
// 規約:
//   - 掃引は x(t) = sin( (ω1·T/L)·(e^{t·L/T} − 1) )、L = ln(ω2/ω1)。
//     位相は t = 0 で 0 から始まる。
//   - 逆フィルタは時間反転 + 6 dB/oct の振幅補正 (ESS のピンク雑音的
//     スペクトルを白くする)。**掃引そのものを逆畳み込みすると振幅 1 の
//     単一インパルスになる**ようスカラー正規化してある (この性質を
//     ctest で検証する — 正規化を外すと復元したインパルス応答の絶対値が
//     狂う)。
//   - N 次高調波のインパルス応答は線形応答より
//       Δt_N = T·ln(N)/ln(f2/f1)
//     だけ **前** に現れる (Farina 2000 式 (9))。
#ifndef OFD_ACOUSTICS_SWEEPDECONVOLUTION_H
#define OFD_ACOUSTICS_SWEEPDECONVOLUTION_H

#include <cstddef>
#include <string>
#include <vector>

#include "AcousticError.h"
#include "ArrayView.h"
#include "AudioBuffer.h"

namespace ofd {
namespace acoustics {

// 掃引の仕様 (測定時に使ったものと同じ値を与える — ここが違うと
// 逆畳み込みは意味のある結果を返さない)
struct SweepSpec {
    double startHz;       // 開始周波数 f1 (> 0)
    double endHz;         // 終了周波数 f2 (> f1、< fs/2)
    double durationSec;   // 掃引長 T (> 0)
    double sampleRateHz;  // fs
    double amplitude;     // 生成する掃引の振幅 (逆畳み込みには影響しない)
    double fadeInSec;     // 生成時の立ち上がりフェード (クリック防止)
    double fadeOutSec;    // 同 立ち下がり

    SweepSpec()
        : startHz(20.0), endHz(20000.0), durationSec(5.0),
          sampleRateHz(48000.0), amplitude(0.5),
          fadeInSec(0.01), fadeOutSec(0.05) {}

    bool valid() const;
};

// ESS を生成する (1 ch)。spec が不正なら空のバッファを返す。
AudioBuffer generateSweep(const SweepSpec &spec);

// 逆フィルタ (時間反転 + 6 dB/oct 補正 + 正規化)。spec が不正なら空。
// 生成した掃引とこれを畳み込むと、振幅 1 の単一インパルスになる。
std::vector<double> sweepInverseFilter(const SweepSpec &spec);

// N 次高調波が線形応答より前に現れる時間 [s] (N >= 1、N = 1 は 0)。
// spec が不正なら 0。
double harmonicDelaySec(const SweepSpec &spec, int order);

// 分離された高調波成分 1 つ
struct HarmonicComponent {
    int         order;        // 2 以上
    std::size_t index;        // 逆畳み込み結果の中でのピーク位置
    double      peak;         // その周辺のピーク絶対値
    double      levelDbc;     // 線形応答のピークに対する相対レベル [dB]
    bool        separable;    // 前後の高調波と重ならず切り出せたか

    HarmonicComponent()
        : order(0), index(0), peak(0.0), levelDbc(-300.0), separable(false) {}
};

struct SweepDeconvolutionResult {
    bool                valid;
    std::vector<double> response;      // 逆畳み込みの全長 (高調波を含む)
    double              sampleRateHz;
    // response の中で線形インパルス応答が始まる位置。ここより前が高調波。
    std::size_t         linearIndex;
    std::vector<double> linear;        // linearIndex 以降 (= 線形 IR)
    std::vector<HarmonicComponent> harmonics;
    // 全高調波のエネルギー和 / 線形応答のエネルギー の平方根 [%]。
    // 高調波を 1 つも分離できなかった場合は thdValid = false
    bool                thdValid;
    double              thdPercent;
    std::string         warning;

    SweepDeconvolutionResult()
        : valid(false), response(), sampleRateHz(0.0), linearIndex(0),
          linear(), harmonics(), thdValid(false), thdPercent(0.0), warning() {}
};

// 録音 (掃引を再生して収録した信号) を逆畳み込みして線形インパルス応答と
// 高調波成分を分離する。maxHarmonic は分離を試みる最大次数 (2..8、
// 0 以下なら高調波分離を行わない)。
// 録音が空 / spec が不正 / 逆畳み込み結果が全 0 のときは valid = false。
AcousticResult<SweepDeconvolutionResult>
deconvolveSweep(ArrayView<const double> recorded, const SweepSpec &spec,
                int maxHarmonic = 5);

} // namespace acoustics
} // namespace ofd

#endif // OFD_ACOUSTICS_SWEEPDECONVOLUTION_H
