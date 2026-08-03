// AudioEditEngine.h — 音響編集・解析エンジン (元 mock: audio-editor.jsx /
// audio-editor-ext.jsx)。GUI 層 (C++17) だが Qt 非依存 — selftest から直接
// 検証できる。FFT / 畳み込み / WAV I/O は音響コア (ofd::acoustics) を再利用
// する (CLAUDE.md: 車輪を再発明しない)。
//
// 方針:
//   - すべて値渡しの純関数 (入力バッファを変更しない)。編集は新しい
//     AudioBuffer を返す (タブ側の undo スタックがバッファ全体を保持する)。
//   - 乱数を使う生成 (white/pink) と合成 IR は固定シードで決定的にする
//     (同一入力 → 同一出力。selftest で再現性を検証する)。
//   - 範囲 [a, z) は内部でクランプする (a >= z なら全範囲とみなす)。
#pragma once
#include <cstddef>
#include <string>
#include <vector>

#include "../acoustics/core/AudioBuffer.h"

namespace ofd {
namespace audioedit {

using acoustics::AudioBuffer;

// ── 窓関数ライブラリ (audio-editor-ext.jsx AE_WINDOWS / AE_WIN_INFO) ────────
enum class WindowKind {
    Rect, Hann, Hamming, Blackman, BlackmanHarris4, Nuttall, FlatTop,
    Bartlett, Welch, Gauss, Tukey, Kaiser, Cosine, Lanczos, Exponential
};

struct WindowInfo {
    WindowKind  kind;
    const char *id;        // 設定保存やコンボの userData に使う短い識別子
    const char *nameJa;    // 表示名 (日)
    const char *nameEn;    // 表示名 (英)
    const char *mainLobe;  // メインローブ幅 [bin] (文献値、"—" あり)
    const char *sideLobe;  // 最大サイドローブ [dB] (文献値、"—" あり)
    const char *useJa;     // 用途 (日)
    const char *useEn;     // 用途 (英)
};

// 15 種の窓の一覧 (表示順は mock と同じ)
const std::vector<WindowInfo> &windowInfos();

// 窓の値 w(i), i = 0..n-1
double windowValue(WindowKind w, std::size_t i, std::size_t n);

// ── 信号生成 (audio-editor.jsx generate) ────────────────────────────────────
enum class SignalKind {
    Sine,      // 正弦波 (f1 のみ使用)
    ExpSweep,  // 指数スイープ ESS (Farina) — IR 測定の標準
    LinSweep,  // 線形スイープ
    White,     // ホワイトノイズ (固定シード)
    Pink,      // ピンクノイズ (Paul Kellet 3 極フィルタ, 固定シード)
    Mls,       // MLS 擬似ランダム (17bit LFSR)
    Impulse,   // 単位インパルス
    Click      // クリック (1kHz 減衰正弦 — バルーン模擬)
};

AudioBuffer generateSignal(SignalKind kind, double f1Hz, double f2Hz,
                           double durationSec, double amp, double sampleRate);

// ── 基本編集 (すべて選択範囲 [a, z) に適用。a >= z は全範囲) ────────────────
AudioBuffer trimToRange(const AudioBuffer &in, std::size_t a, std::size_t z);
AudioBuffer deleteRange(const AudioBuffer &in, std::size_t a, std::size_t z);
AudioBuffer silenceRange(const AudioBuffer &in, std::size_t a, std::size_t z);
AudioBuffer reverseRange(const AudioBuffer &in, std::size_t a, std::size_t z);
// 範囲のピークを targetPeak (既定 0.98 ≒ -0.2 dBFS) へ。適用ゲイン dB を返す
AudioBuffer normalizeRange(const AudioBuffer &in, std::size_t a, std::size_t z,
                           double targetPeak, double *appliedGainDb);
AudioBuffer fadeRange(const AudioBuffer &in, std::size_t a, std::size_t z,
                      bool fadeIn);
AudioBuffer gainRange(const AudioBuffer &in, std::size_t a, std::size_t z,
                      double gainDb);
AudioBuffer removeDcRange(const AudioBuffer &in, std::size_t a, std::size_t z);
// クリック除去 (隣接 2 点平均の平滑)
AudioBuffer smoothRange(const AudioBuffer &in, std::size_t a, std::size_t z);

// ── エフェクト (全体に適用 — mock と同じ) ───────────────────────────────────
enum class BiquadKind { Peaking, HighPass, LowPass };

// RBJ Audio EQ Cookbook の biquad。gainDb は Peaking のみ使用。
AudioBuffer applyBiquad(const AudioBuffer &in, BiquadKind kind,
                        double freqHz, double q, double gainDb);

// フィードバックディレイ (末尾 2 秒のテールを付加)
AudioBuffer applyDelay(const AudioBuffer &in, double delayMs,
                       double feedback, double mix);

// フィードフォワードコンプレッサ (ピーク検波 attack/release、静的カーブ)
AudioBuffer applyCompressor(const AudioBuffer &in, double thresholdDb,
                            double ratio, double attackSec = 0.003,
                            double releaseSec = 0.25);

// 再生速度変更 (線形補間リサンプル — ピッチ連動)。長さは 1/rate 倍
AudioBuffer applyRate(const AudioBuffer &in, double rate);

// ホール残響 IR を合成 (指数減衰ノイズ + プリディレイ 12ms + 直接音)。
// 固定シードで決定的。2ch, 長さ = ceil(sr * rt60 * 1.3)
AudioBuffer synthesizeHallIr(double rt60Sec, double sampleRate);

// 畳み込みリバーブ: 合成 IR (または渡された IR) を ConvolutionEngine で
// 畳み込み、dry×(1-mix*0.6) + wet×mix。出力長 = 入力長 + IR 長
AudioBuffer applyReverb(const AudioBuffer &in, double rt60Sec, double mix);
AudioBuffer applyConvolution(const AudioBuffer &in, const AudioBuffer &ir,
                             double mix);

// ピッチシフト (グラニュラー OLA、長さ保持) / タイムストレッチ (OLA、ピッチ保持)
AudioBuffer pitchShift(const AudioBuffer &in, double semitones);
AudioBuffer timeStretch(const AudioBuffer &in, double factor);

// スペクトルノイズリダクション (スペクトルゲート、2048 点 / 75% OL)。
// noiseProfile: 選択範囲 [a, z) の平均振幅スペクトル (FFT/2+1 点)
std::vector<double> noiseProfile(const AudioBuffer &in, std::size_t a,
                                 std::size_t z);
AudioBuffer denoise(const AudioBuffer &in, const std::vector<double> &profile,
                    double reduceDb);

// ステレオ処理 (モノ入力は L=R として扱い、出力は常に 2ch)
enum class StereoOp { Mono, Swap, Side, Widen, InvertLeft };
AudioBuffer applyStereoOp(const AudioBuffer &in, StereoOp op);

// ── 解析 (audio-editor.jsx analyze + ext) ───────────────────────────────────
struct SpectrumPoint { double logF; double db; };  // x = log10(f [Hz])

// 選択開始位置から fftSize 点の窓付き FFT → 対数周波数間引き系列
std::vector<SpectrumPoint> spectrum(const AudioBuffer &in, std::size_t a,
                                    WindowKind window,
                                    std::size_t fftSize = 4096);

// レベル指標 + Schroeder 逆積分の残響指標 (選択範囲を IR とみなす)。
// 減衰が該当レベルへ達しない場合は has* = false (「それらしい値」を返さない)
struct LevelMetrics {
    double durationSec = 0.0;
    double peakDbfs    = -300.0;
    double rmsDbfs     = -300.0;
    double crestDb     = 0.0;
    double dcOffset    = 0.0;
    bool   hasEdt = false, hasT20 = false, hasT30 = false;
    double edtSec = 0.0, t20Sec = 0.0, t30Sec = 0.0;
};
LevelMetrics analyzeLevels(const AudioBuffer &in, std::size_t a,
                           std::size_t z);

// ITU-R BS.1770 ラウドネス (K 特性は任意 fs で係数を導出、400ms ブロック
// 75% OL、絶対 -70 / 相対 -10 ゲーティング) + True Peak (簡易 4x 線形補間)
struct LoudnessMetrics {
    double integratedLufs  = -70.0;
    double rangeLu         = 0.0;
    double momentaryMaxLufs = -70.0;
    double truePeakDbtp    = -300.0;
};
LoudnessMetrics analyzeLoudness(const AudioBuffer &in);

// 1/1 オクターブバンド相対レベル (31.5 Hz〜16 kHz、先頭 8192 点の FFT 集計)
struct OctaveBand { double fcHz; double db; };
std::vector<OctaveBand> octaveBands(const AudioBuffer &in);

// スペクトログラム表示用の強度マップ (rows×cols, 行 0 = 最高周波数側)。
// 値は 0..1 の正規化強度 (-84 dB..0 dB)。色変換は表示側で行う
std::vector<float> spectrogram(const AudioBuffer &in, int cols, int rows,
                               WindowKind window, std::size_t fftSize = 512);

} // namespace audioedit
} // namespace ofd
