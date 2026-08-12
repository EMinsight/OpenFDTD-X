// EyeDiagram.h — 線路の伝達関数を通したビット列のアイダイアグラム — Qt 非依存。
//
// 伝送線路タブの「アイダイアグラム」を実際に出すための部分。線路の
// S21(f) (= `core/TransmissionLine` の `sParameters`) を周波数ごとに掛けて
// 受信波形を作り、1 UI ごとに折り返す。
//
// ── 何を計算しているか ────────────────────────────────────────────────────
// 送信波形 x(t) は最長周期 PRBS (2^n − 1 ビット) を台形波にしたもの。
// これを **周期信号として** 扱い、伝達関数 H(f) = S21(f) を掛けて受信波形を
// 得る。周期信号の定常応答なので**立ち上がりの過渡が混じらない** — アイ
// ダイアグラムは定常状態の図なので、これが正しい。
//
//   y[n] = Σ_m h[m] · x[(n − m) mod N]      (N = PRBS 1 周期のサンプル数)
//
// 巡回畳み込みなのは x が周期 N で周期的だから。**近似ではなく厳密**である
// ことが重要 (打ち切り誤差を「たぶん小さい」で済ませない)。
//
// ── h[m] の作り方 ─────────────────────────────────────────────────────────
// H(f) を周波数格子で標本化し、逆 FFT して h を得る。FFT は 2 の冪長しか
// 扱えないが PRBS の周期 N は 2 の冪ではないので、**h は 2 の冪長 M ≥ N の
// 格子で作り、畳み込みは周期 N で巡回させる**。M は h が十分減衰する長さを
// とる (`Config::impulseSamples`)。h の裾がどれだけ残っているかは
// `Result::tailFraction` に出すので、黙って切ったことにはならない。
//
// H(f) は実信号を保つためエルミート対称 (H(−f) = conj H(f)) に組む。直流は
// 実数でなければならないので、最小周波数での値の実部を使う。
//
// ── 出す量 ────────────────────────────────────────────────────────────────
//   height_V   判定時刻 (アイの中央) での開口 = min(1 の値) − max(0 の値)
//              **負にもなる** (閉じたアイ)。閉じているものを 0 で止めない。
//   width_s    しきい値 (0 V) を横切る時刻の、最も遅い立上りから最も早い
//              立下りまで = ジッタで削られた後の開口幅
//   jitter_s   交差時刻のばらつき (peak-to-peak)。データ依存ジッタ (ISI)。
//   traces     折り返した波形そのもの (描画用)。
//
// ── 検証できること (selftest がこれを判定している) ───────────────────────
//   * H ≡ 1 なら開口は送信振幅そのもの、幅は 1 UI ちょうど、ジッタ 0
//   * 純遅延 exp(−j2πfτ) は形を変えない (開口・幅が不変、位置だけずれる)
//   * 1 次 RC は**閉形式の巡回定常解**と一致する (指数の等比級数で厳密に
//     書ける)。数値の畳み込みが正しいことをここで押さえる。
//   * 減衰を強くすると開口は単調に狭まる (**チャネルが標本化で解像できて
//     いる範囲で**。ナイキストに応答が残る粗い標本化では折り返しで単調性が
//     崩れる — これは手法の限界なので `nyquistMag` で表に出す)
#ifndef OFD_CORE_EYEDIAGRAM_H
#define OFD_CORE_EYEDIAGRAM_H

#include <complex>
#include <cstddef>
#include <functional>
#include <vector>

namespace ofd {
namespace eye {

// 伝達関数 H(f) [f は Hz、負の周波数では呼ばれない]
using Transfer = std::function<std::complex<double>(double)>;

struct Config {
    double bitRate_bps    = 1.0e9;  // ビットレート
    int    prbsOrder      = 7;      // PRBS の次数 (周期 2^n − 1 ビット)
    int    samplesPerBit  = 32;     // 1 UI あたりの標本数 (偶数を推奨)
    double amplitude_V    = 1.0;    // 振幅 (±amplitude の 2 値)
    double riseTime_s     = 0.0;    // 台形波の遷移時間 (0 = 矩形)
    int    impulseSamples = 4096;   // h の格子長 (2 の冪へ切り上げる)
    bool   valid() const {
        return bitRate_bps > 0.0 && prbsOrder >= 2 && prbsOrder <= 16
               && samplesPerBit >= 4 && amplitude_V > 0.0
               && riseTime_s >= 0.0 && impulseSamples >= 16;
    }
};

struct Result {
    double dt_s = 0.0;                         // 標本間隔
    int    samplesPerBit = 0;
    std::vector<std::vector<double>> traces;   // 折り返した波形 (各 2 UI 分)
    double height_V = 0.0;                     // 判定時刻での開口 (負もあり)
    double width_s  = 0.0;                     // 交差から交差までの開口幅
    double jitter_s = 0.0;                     // 交差時刻のばらつき (p-p)
    double tailFraction = 0.0;                 // h の格子外へ残った電力の割合
    // |H(ナイキスト)| / |H(0)|。**標本化がチャネルを解像できているかの指標**。
    // H(f) を格子で標本化して逆 FFT する方法は、ナイキストより上に応答が
    // 残っていると時間領域で折り返す (h が巡回して混じる)。伝送線路の S21 は
    // 高域で落ちるので実用上は小さいが、広帯域のチャネルを粗い標本化で
    // 見ると開口が実際より広く/狭く出る。**0.1 を超えたら 1 UI あたりの
    // 標本数を増やすこと** (画面にもそう出す)。
    double nyquistMag = 0.0;
    // 判定した位置 (ビット先頭からの標本数)。チャネルの遅延を含むので
    // 1 UI を超える。**遅延を無視すると別のビットを見てしまう**ので、
    // 主到達 (h の最大値の位置) に合わせ、その上で 1 UI 以内は開口が
    // 最大になる点を選んでいる (受信器の最良判定点)。
    std::size_t sampleIndex = 0;
    bool   ok() const { return !traces.empty() && dt_s > 0.0; }
};

// 最長周期 PRBS (LFSR)。長さ 2^order − 1 の 0/1 列。
// タップは原始多項式 (次数 2..16)。**周期が 2^order − 1 であることは
// selftest が実際に数えて確認している** (表を写し間違えれば落ちる)。
std::vector<int> prbs(int order);

// 送信波形 (±amplitude の台形波、周期 = PRBS 1 周期)
std::vector<double> transmit(const Config &c, const std::vector<int> &bits);

// H(f) から実インパルス応答 h を作る (長さは 2 の冪)。
// tailOut に「格子の後半 1/4 に残っている電力の割合」を返す (打ち切りの目安)。
std::vector<double> impulseResponse(const Transfer &H, double fs_Hz,
                                    int minLength, double *tailOut = nullptr);

// x を周期 N の巡回畳み込みで h に通す (厳密)。
std::vector<double> convolveCyclic(const std::vector<double> &x,
                                   const std::vector<double> &h);

// 一式まとめて。H が空なら理想チャネル (H ≡ 1) とみなす。
Result build(const Config &c, const Transfer &H);

} // namespace eye
} // namespace ofd

#endif // OFD_CORE_EYEDIAGRAM_H
