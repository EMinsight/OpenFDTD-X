// WaveformSpectrum.h — 時間波形 → 窓関数つきスペクトル (Qt 非依存 / C++17)
//
// カーネルが出す時間波形 (ofd_post の feed.log = 給電点の V/I 波形、
// point.log = 観測点の波形、音響の RIR など) を、GUI 側で周波数領域へ移す。
// ev2d / ev3d を経由しないポスト表示の一部で、外部ソルバーは要らない。
//
// **窓は 2 段構え**にしてある。意味が違うので混ぜない:
//
//   1. apodization (モニタータブの「開始時 / 終了時 / 両端」)
//      記録の**端の過渡**を落とすためのテーパ。Tukey (raised-cosine) を
//      指定した側にだけ掛ける。掛けた側以外のサンプルは 1 倍のまま。
//   2. 解析窓 (Hann / Hamming / Blackman …)
//      DFT の漏れ (leakage) を抑えるための窓。全長に掛かる。
//      実体は audio/AudioEditEngine の窓関数ライブラリを使う (再実装しない)。
//
// 出力は片側スペクトル (0 … fs/2) の振幅を最大 0 dB で正規化したもの。
// 絶対値を出さないのは、窓とゼロ詰めで振幅の意味が変わるため
// (校正のない絶対値を表示しない、という本プロジェクトの方針にも沿う)。
#ifndef OFD_CORE_WAVEFORMSPECTRUM_H
#define OFD_CORE_WAVEFORMSPECTRUM_H

#include <vector>

#include "../audio/AudioEditEngine.h"

namespace ofd {
namespace wavespec {

// 記録の端の過渡を落とすテーパ (モニタータブの apodization と同じ意味)
enum class Apodization { Off = 0, Start = 1, End = 2, Both = 3 };

struct Result {
    bool   valid = false;
    std::vector<double> freqHz;   // 0 … fs/2
    std::vector<double> db;       // 最大を 0 dB とした相対振幅
    double dtSec = 0.0;           // 標本間隔 (時間列から求めた)
    double fsHz = 0.0;            // 標本化周波数 1/dt
    double dfHz = 0.0;            // 周波数分解能 fs/nFft
    int    nUsed = 0;             // 使った標本数
    int    nFft = 0;              // ゼロ詰め後の長さ (2 の冪)
    double coherentGain = 1.0;    // 窓の直流利得 Σw/N (Rect で 1)
    double enbwBins = 1.0;        // 等価雑音帯域幅 [bin] (Rect で 1)
    double peakFreqHz = 0.0;      // 最大となる周波数
    bool   hasPeak = false;
};

// t: 時間列 [s] (等間隔・昇順)、y: 値。両者は同じ長さで 4 点以上。
// window: 解析窓、apod: 端のテーパ、taperFrac: テーパ長 / 全長 (0〜0.5)。
// minFftSize: ゼロ詰めの下限 (0 なら nUsed 以上の最小の 2 の冪)。
//
// 時間列が等間隔でない (隣り合う差が平均から 2 % を超えてずれる) 場合は
// valid = false。「それらしいスペクトル」を作らない。
// 2 % と広いのは、ofd_post の time 列が 6 桁の指数表記で、丸めだけで
// 0.1 % ほど揺れるため (厳しくすると実データが全て弾かれる)。
Result waveformSpectrum(const std::vector<double> &t,
                        const std::vector<double> &y,
                        audioedit::WindowKind window = audioedit::WindowKind::Hann,
                        Apodization apod = Apodization::Off,
                        double taperFrac = 0.1,
                        int minFftSize = 0);

// テーパ係数そのもの (検証と表示用)。i = 0..n-1。
// Off なら常に 1。Start/End/Both は Tukey の該当側だけを使う。
double taperValue(Apodization apod, double taperFrac, std::size_t i,
                  std::size_t n);

} // namespace wavespec
} // namespace ofd

#endif // OFD_CORE_WAVEFORMSPECTRUM_H
