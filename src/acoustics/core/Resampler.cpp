// Resampler.cpp — 有理比ポリフェーズ Kaiser 窓 sinc リサンプラの実装。
//
// ── 設計根拠 ────────────────────────────────────────────────────────────────
// 阻止域減衰 A = 90 dB を設計目標とする (16 bit PCM の量子化ノイズ床
// ≈ −96 dBFS に対し、折り返し成分を聴感上無視できる水準まで抑える)。
//
// Kaiser 窓の経験式 (J. F. Kaiser, "Nonrecursive digital filter design using
// the I0-sinh window function", Proc. IEEE ISCAS, 1974 /
// Oppenheim & Schafer, Discrete-Time Signal Processing, §7.5.3):
//   β = 0.1102 (A − 8.7)                (A > 50 dB)      → β ≈ 8.96
//   N ≈ (A − 7.95) / (2.285 Δω)        (Δω: 遷移帯域幅 [rad/sample])
//
// 遷移帯域は「目標ナイキスト (= min(fs_in, fs_out)/2) の 0.90–1.00 倍」に置く。
// 阻止域端をちょうど目標ナイキストに一致させるので、ダウンサンプリング時に
// 折り返して通過帯域へ入り得る成分 (目標ナイキスト以上) は全て A dB 以上
// 抑圧される。通過帯域は 0.90 × ナイキストまで平坦で、Kaiser 設計の通過帯域
// リプルは δ = 10^(−A/20) ≈ 3.2e-5 → ±0.00028 dB (0.1 dB 仕様に対し十分小)。
//
// タップ数はプロトタイプ全体で N ≈ 114.3 · max(L, M)。ポリフェーズ評価では
// 出力 1 サンプルあたり約 N/L タップ (≈ 115 MAC) なので、44.1k↔48k
// (L/M = 160/147) のような大きい比でも実用速度で動く。
//
// ── 群遅延補正 ──────────────────────────────────────────────────────────────
// プロトタイプ FIR は奇数長・偶対称 (線形位相) で、群遅延は高レート
// (L·fs_in) で整数 Nh = (N−1)/2。出力サンプル n の値を高レート時刻
// p = n·M を中心にフィルタを「引き戻して」評価する (y[n] = Σ x[j]·h[p − jL + Nh])
// ことで遅延は厳密に相殺され、出力の時間原点は入力と一致する。
// 端点は暗黙のゼロ詰め (係数範囲を入力範囲でクランプ)。
#include "Resampler.h"

#include <cmath>
#include <cstdio>

namespace ofd {
namespace acoustics {

namespace {

const double kPi = 3.14159265358979323846;

const double kStopbandDb = 90.0;   // 設計阻止域減衰 A [dB]
const double kPassbandEdge = 0.90; // 通過帯域端 (目標ナイキスト比)
const double kStopbandEdge = 1.00; // 阻止域端 (目標ナイキスト比)

// 約分後の L, M の上限。標準系サンプルレート (8k/11.025k/16k/22.05k/24k/
// 32k/44.1k/48k/88.2k/96k/176.4k/192k) の相互変換で必要な最大値は 640
// (11025 ↔ 48000)。上限はフィルタのメモリ暴走を防ぐ安全弁。
const long long kMaxFactor = 4096;

long long gcdll(long long a, long long b)
{
    while (b != 0) {
        const long long t = a % b;
        a = b;
        b = t;
    }
    return a;
}

// floor 除算 (負の被除数でも数学的な floor)
long long floorDiv(long long a, long long b)
{
    long long q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) --q;
    return q;
}

long long ceilDiv(long long a, long long b) { return -floorDiv(-a, b); }

// 第 1 種 0 次変形ベッセル関数 I0(x) の級数展開
// I0(x) = Σ_k ((x/2)^k / k!)^2。x ≤ β ≈ 9 では急速に収束する。
double besselI0(double x)
{
    double sum = 1.0;
    double term = 1.0;
    const double half = 0.5 * x;
    for (int k = 1; k <= 64; ++k) {
        term *= (half / static_cast<double>(k));
        const double add = term * term;
        sum += add;
        if (add < sum * 1e-17) break;
    }
    return sum;
}

// Kaiser 窓 sinc の線形位相プロトタイプ (奇数長 2·half+1、L 倍の振幅補償込み)
struct FirDesign {
    std::vector<double> h;
    long long half;

    FirDesign() : h(), half(0) {}
};

FirDesign designKaiserLowpass(long long L, long long M)
{
    const long long F = (L > M) ? L : M;
    // 高レート (L·fs_in) での正規化角周波数。目標ナイキスト wn = π/F。
    const double wn = kPi / static_cast<double>(F);
    const double wc = 0.5 * (kPassbandEdge + kStopbandEdge) * wn; // −6 dB 点
    const double dw = (kStopbandEdge - kPassbandEdge) * wn;       // 遷移帯域幅
    const double beta = 0.1102 * (kStopbandDb - 8.7);             // Kaiser 1974

    // N ≈ (A − 7.95)/(2.285 Δω) → 奇数長 2·half+1 に切り上げ
    long long half = static_cast<long long>(
        std::ceil(((kStopbandDb - 7.95) / (2.285 * dw) - 1.0) / 2.0));
    if (half < 1) half = 1;

    FirDesign d;
    d.half = half;
    d.h.assign(static_cast<std::size_t>(2 * half + 1), 0.0);
    const double i0b = besselI0(beta);
    for (long long k = -half; k <= half; ++k) {
        const double r = static_cast<double>(k) / static_cast<double>(half);
        const double win = besselI0(beta * std::sqrt(1.0 - r * r)) / i0b;
        const double ideal =
            (k == 0) ? wc / kPi
                     : std::sin(wc * static_cast<double>(k)) /
                           (kPi * static_cast<double>(k));
        // L 倍はアップサンプリング (ゼロ挿入) の振幅補償
        d.h[static_cast<std::size_t>(k + half)] =
            static_cast<double>(L) * ideal * win;
    }
    return d;
}

// ポリフェーズ評価。出力 n の高レート時刻 p = n·M を中心に評価する
// (群遅延補正済み — 冒頭コメント参照)。範囲外の入力はゼロ詰め扱い。
std::vector<double> applyPolyphase(const FirDesign &d, long long L,
                                   long long M, ArrayView<const double> x,
                                   std::size_t nOut)
{
    std::vector<double> y(nOut, 0.0);
    const long long nIn = static_cast<long long>(x.size());
    for (std::size_t n = 0; n < nOut; ++n) {
        const long long p = static_cast<long long>(n) * M;
        long long j0 = ceilDiv(p - d.half, L);
        long long j1 = floorDiv(p + d.half, L);
        if (j0 < 0) j0 = 0;
        if (j1 > nIn - 1) j1 = nIn - 1;
        double acc = 0.0;
        for (long long j = j0; j <= j1; ++j) {
            acc += x[static_cast<std::size_t>(j)] *
                   d.h[static_cast<std::size_t>(p - j * L + d.half)];
        }
        y[n] = acc;
    }
    return y;
}

// fs の妥当性検査と L/M の決定。成功時は true を返し L, M を埋める。
bool resolveRatio(double srcRateHz, double dstRateHz, long long *L,
                  long long *M, AcousticErrorCode *code, std::string *msg)
{
    if (!(srcRateHz > 0.0) || !(dstRateHz > 0.0)) {
        *code = AcousticErrorCode::UnsupportedSampleRate;
        *msg = "sample rate must be positive";
        return false;
    }
    // WAV の fs は整数 — 無理比 (非整数 fs) は対象外 (Resampler.h 参照)
    if (std::floor(srcRateHz) != srcRateHz ||
        std::floor(dstRateHz) != dstRateHz || srcRateHz > 1.0e9 ||
        dstRateHz > 1.0e9) {
        *code = AcousticErrorCode::UnsupportedSampleRate;
        *msg = "sample rate must be an integer number of Hz (WAV)";
        return false;
    }
    const long long src = static_cast<long long>(srcRateHz);
    const long long dst = static_cast<long long>(dstRateHz);
    const long long g = gcdll(dst, src);
    *L = dst / g;
    *M = src / g;
    if (*L > kMaxFactor || *M > kMaxFactor) {
        char buf[128];
        std::snprintf(buf, sizeof(buf),
                      "conversion ratio %lld/%lld exceeds the supported "
                      "factor limit (%lld)",
                      static_cast<long long>(*L), static_cast<long long>(*M),
                      kMaxFactor);
        *code = AcousticErrorCode::UnsupportedSampleRate;
        *msg = buf;
        return false;
    }
    return true;
}

// 出力長 round(nIn·L/M) を整数演算で決定的に求める (四捨五入)
std::size_t outputLength(std::size_t nIn, long long L, long long M)
{
    const long long num = static_cast<long long>(nIn) * L + M / 2;
    return static_cast<std::size_t>(num / M);
}

ResampleInfo makeInfo(long long L, long long M, const FirDesign &d,
                      double srcRateHz, double dstRateHz)
{
    ResampleInfo info;
    info.upFactor = L;
    info.downFactor = M;
    info.filterLength = d.h.size();
    // カットオフ (−6 dB 点) = 0.95 × min(fs_in, fs_out)/2 [Hz]
    const double fmin = (srcRateHz < dstRateHz) ? srcRateHz : dstRateHz;
    info.cutoffHz = 0.5 * (kPassbandEdge + kStopbandEdge) * 0.5 * fmin;
    info.stopbandDb = kStopbandDb;
    info.identity = false;
    return info;
}

} // namespace

AcousticResult<std::vector<double>>
resampleSignal(ArrayView<const double> x, double srcRateHz, double dstRateHz,
               ResampleInfo *outInfo)
{
    typedef AcousticResult<std::vector<double>> Result;
    if (outInfo) *outInfo = ResampleInfo();
    if (x.empty())
        return Result::error(AcousticErrorCode::EmptyInput,
                             "input signal is empty");

    // 同一 fs は恒等変換: 入力をそのままコピー (ビット一致保証)
    if (srcRateHz > 0.0 && srcRateHz == dstRateHz) {
        if (outInfo) {
            outInfo->identity = true;
            outInfo->cutoffHz = 0.5 * srcRateHz;
        }
        return Result::ok(std::vector<double>(x.begin(), x.end()));
    }

    long long L = 1, M = 1;
    AcousticErrorCode code = AcousticErrorCode::Ok;
    std::string msg;
    if (!resolveRatio(srcRateHz, dstRateHz, &L, &M, &code, &msg))
        return Result::error(code, msg);

    const FirDesign d = designKaiserLowpass(L, M);
    if (outInfo) *outInfo = makeInfo(L, M, d, srcRateHz, dstRateHz);
    return Result::ok(applyPolyphase(d, L, M, x, outputLength(x.size(), L, M)));
}

AcousticResult<AudioBuffer>
resampleBuffer(const AudioBuffer &in, double dstRateHz, ResampleInfo *outInfo)
{
    typedef AcousticResult<AudioBuffer> Result;
    if (outInfo) *outInfo = ResampleInfo();
    if (in.channelCount() == 0 || in.sampleCount() == 0)
        return Result::error(AcousticErrorCode::EmptyInput,
                             "input buffer is empty");
    for (std::size_t c = 1; c < in.channelCount(); ++c) {
        if (in.channels[c].size() != in.channels[0].size())
            return Result::error(AcousticErrorCode::InvalidArgument,
                                 "channel lengths differ within a buffer");
    }

    AudioBuffer out;
    out.sampleRateHz = dstRateHz;

    // 恒等変換 (ビット一致コピー)
    if (in.sampleRateHz > 0.0 && in.sampleRateHz == dstRateHz) {
        out.channels = in.channels;
        if (outInfo) {
            outInfo->identity = true;
            outInfo->cutoffHz = 0.5 * in.sampleRateHz;
        }
        return Result::ok(std::move(out));
    }

    long long L = 1, M = 1;
    AcousticErrorCode code = AcousticErrorCode::Ok;
    std::string msg;
    if (!resolveRatio(in.sampleRateHz, dstRateHz, &L, &M, &code, &msg))
        return Result::error(code, msg);

    // フィルタは 1 回だけ設計し全チャンネルに適用する
    const FirDesign d = designKaiserLowpass(L, M);
    const std::size_t nOut = outputLength(in.sampleCount(), L, M);
    out.channels.reserve(in.channelCount());
    for (std::size_t c = 0; c < in.channelCount(); ++c) {
        out.channels.push_back(applyPolyphase(
            d, L, M,
            ArrayView<const double>(in.channels[c].data(),
                                    in.channels[c].size()),
            nOut));
    }
    if (outInfo) *outInfo = makeInfo(L, M, d, in.sampleRateHz, dstRateHz);
    return Result::ok(std::move(out));
}

} // namespace acoustics
} // namespace ofd
