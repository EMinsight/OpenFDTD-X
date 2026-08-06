// Resampler.h — 有理比ポリフェーズ窓関数 sinc リサンプラ (Kaiser 窓)。
// Qt 非依存 / C++14。
//
// 方式 (既知の負債 #12 「リサンプリング未実装」の解消):
//   - 変換比は有理数 L/M (gcd で約分)。WAV の fs は整数なので無理比は扱わない。
//   - 概念上は「L 倍アップサンプル → 線形位相 FIR 低域通過 → M 分の 1 間引き」
//     で、実装は必要な出力サンプルだけをポリフェーズ評価する
//     (出力 1 サンプルあたり約 N/L タップ — L, M の大きさに依らずほぼ一定)。
//   - プロトタイプ FIR は Kaiser 窓 sinc。設計目標は阻止域減衰 ~90 dB
//     (β = 0.1102(A−8.7), Kaiser 1974)。設計根拠は Resampler.cpp 冒頭を参照。
//   - フィルタ中心 (群遅延) を出力時刻に一致させて評価するため、出力の
//     時間原点は入力とずれない (RIR の直接音到達時刻 → ITDG 等の解析を守る)。
//   - 決定的: 乱数・時刻に依存しない。同じ入力は常にビット一致の出力。
//     内部精度は double。同一 fs は入力をそのままコピー (ビット一致)。
//   - L または M が 1 段の上限 (4096) を超える比 (FDTD ソルバーが格子刻みから
//     決める端数 fs — 例 1201 Hz → 48000 Hz は 48000/1201) は、L・M を上限
//     以下の因子へ分けた**多段カスケード**で実現する。中間レートは必ず
//     min(fs_in, fs_out) 〜 max(fs_in, fs_out) に収まるので、通過帯域端
//     (0.90 × min ナイキスト) と阻止域 (~90 dB/段) は 1 段の場合と同じ。
//     分割できない比 (上限超えの素因数を持つ) は従来どおりエラーにする
//     (黙って劣化させない)。1 段で足りる比の出力は従来とビット一致。
#pragma once
#include <cstddef>
#include <vector>

#include "AcousticError.h"
#include "ArrayView.h"
#include "AudioBuffer.h"

namespace ofd {
namespace acoustics {

// リサンプリングの設計情報 (呼び出し側の表示・検証用)
struct ResampleInfo {
    long long upFactor;        // 補間比 L (約分後)
    long long downFactor;      // 間引き比 M (約分後)
    std::size_t filterLength;  // プロトタイプ FIR 長の合計 (奇数長の和)。
                               // 恒等変換では 0
    double cutoffHz;           // 設計カットオフ (−6 dB 点) [Hz]
    double stopbandDb;         // 設計阻止域減衰 [dB] (~90 dB。多段でも 1 段
                               // ごとにこの値なのでカスケードは更に急峻)
    bool identity;             // fs 一致 (フィルタ不使用、入力のコピー)
    std::size_t stageCount;    // カスケード段数 (通常 1。恒等変換では 0)

    ResampleInfo()
        : upFactor(1), downFactor(1), filterLength(0), cutoffHz(0.0),
          stopbandDb(0.0), identity(false), stageCount(0) {}
};

// 1 チャンネルのリサンプリング。
// fs は正の整数値であること (WAV 前提 — 無理比は非対応)。
// 出力長は round(入力長 · L / M)。srcRateHz == dstRateHz は入力とビット一致。
AcousticResult<std::vector<double>>
resampleSignal(ArrayView<const double> x, double srcRateHz, double dstRateHz,
               ResampleInfo *outInfo = nullptr);

// 全チャンネルのリサンプリング (全チャンネル同一の設計を適用)。
// 出力の sampleRateHz は dstRateHz になる。
AcousticResult<AudioBuffer>
resampleBuffer(const AudioBuffer &in, double dstRateHz,
               ResampleInfo *outInfo = nullptr);

} // namespace acoustics
} // namespace ofd
