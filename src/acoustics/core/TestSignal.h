// TestSignal.h — 可聴化用の試験信号 (帯域制限クリック)。Qt 非依存 / C++14。
//
// 可聴化のドライ音源として最も素直なのは「クリック」で、これを室の
// インパルス応答に畳み込むと **その部屋の響きそのもの** が聴こえる
// (無響の音声・音楽素材は第三者の録音なので同梱できない — クリックは
//  こちらで作れる)。
//
// ただし 1 サンプルだけ 1 を立てた「デルタ」は使わない。デルタは
// ナイキストまで一様なスペクトルを持つので、再生系の帯域外成分が
// そのまま折り返し歪みとして乗り、聴感上「パチッ」ではなく「ジッ」と
// 濁る。ここでは **帯域制限インパルス** を作る:
//
//   h_ideal[n] = 2·fh/fs·sinc(2·fh·t) − 2·fl/fs·sinc(2·fl·t),  t = (n−c)/fs
//   h[n]       = w[n]·h_ideal[n] − k·w[n]       (w = Hann 窓, N タップ)
//
// 最後の項は直流除去で、k = Σ(w·h_ideal)/Σw。窓長が有限なので下側遮断 fl が
// 遷移帯域幅より狭いと直流が残る (既定の fl = 20 Hz はその状態) — 窓そのものの
// 形で引けば Σh = 0 を厳密に満たしたまま対称性と両端の 0 が保たれ、
// 引いた分のスペクトルは直流まわり ±Δf に集中するので通過域は動かない。
//
// これは理想帯域通過フィルタのインパルス応答を Hann 窓で切り出した
// 窓関数法 (windowed-sinc) そのもので、次の性質を持つ:
//
//   - **線形位相** — h は中心 c について厳密に対称。位相歪みが無いので
//     畳み込んだ結果の時間波形が RIR の形をそのまま保つ。
//   - **通過域が平坦** — Hann 窓のリップルは ±0.06 dB 程度。
//   - **阻止域 −40 dB 級** — Hann 窓の最大サイドローブ (−31 dB) と
//     6 dB/oct のロールオフから決まる。
//   - **直流成分が無い** (fl > 0 のとき) — スピーカーを傷めない。
//
// 遷移帯域の幅は窓長で決まり、Hann ではおよそ Δf ≈ 3.1·fs/N。
// これらは検証 (tests/acoustics/test_testsignal.cpp) で実測している。
#ifndef OFD_ACOUSTICS_TESTSIGNAL_H
#define OFD_ACOUSTICS_TESTSIGNAL_H

#include "AudioBuffer.h"

namespace ofd {
namespace acoustics {

// 帯域制限クリックの仕様
struct ClickSpec {
    double sampleRateHz;  // fs
    double lowHz;         // 下側遮断 fl (0 以上。0 なら低域通過になる)
    double highHz;        // 上側遮断 fh (fl < fh < fs/2)
    double durationSec;   // 窓長 (タップ数は奇数に丸める。8 タップ以上)
    double amplitude;     // 生成後のピーク振幅 (0 < a <= 1)

    ClickSpec()
        : sampleRateHz(48000.0), lowHz(20.0), highHz(20000.0),
          durationSec(0.02), amplitude(0.5) {}

    bool valid() const;

    // タップ数 (奇数)。valid() でなければ 0。
    std::size_t tapCount() const;

    // Hann 窓の遷移帯域幅の目安 [Hz] (Δf ≈ 3.1·fs/N)。
    // 平坦とみなせるのは [lowHz + Δf, highHz − Δf] の範囲。
    double transitionWidthHz() const;
};

// 帯域制限クリックを生成する (1 ch)。spec が不正なら空のバッファを返す。
// ピークは中央サンプルにあり、値はちょうど spec.amplitude になる。
AudioBuffer generateClick(const ClickSpec &spec);

} // namespace acoustics
} // namespace ofd

#endif // OFD_ACOUSTICS_TESTSIGNAL_H
