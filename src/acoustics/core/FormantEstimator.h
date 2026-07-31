// FormantEstimator.h — LPC によるフォルマント周波数推定 (F1/F2/F3)。
// Qt 非依存 / C++14。
//
// 手順 (Markel & Gray 1976 の標準的な LPC フォルマント推定):
//   1. 反エイリアス FIR (ハミング窓 sinc) + 整数間引きで内部 fs ≈ 10 kHz
//      に落とす (48 kHz → 1/5 = 9.6 kHz)。フォルマント帯域 (< 5 kHz) に
//      対して LPC 次数を小さく保つため。
//   2. LPC 次数 p = 2 + round(内部 fs / 1000) (9.6 kHz → p = 12、上限 24)。
//   3. フレーム前処理: プリエンファシス (係数 0.97) + ハミング窓。
//      推定は呼び出し側 (YIN) が有声と判定したフレームのみ行う。
//   4. 自己相関 → Levinson-Durbin で LPC 係数 A(z) = 1 + Σ a_i z^-i。
//   5. A(z) の根を Durand-Kerner 法 (自前実装、決定的初期値 (0.4+0.9i)^k、
//      乱数不使用) で求め、上半平面の極 z から
//        F = arg(z)·fs/(2π) [Hz],  B = −ln|z|·fs/π [Hz]
//      を得る。F ≥ minFormantHz かつ 0 < B ≤ maxBandwidthHz の候補を
//      昇順に F1/F2/F3 とする。
//   6. 代表値は有効フレームの時間中央値 (MetricValue / AnalysisQuality の
//      流儀に従い、評価不能時は理由付き invalid)。
//
// 注意: 本モジュールは共鳴周波数という物理量の推定のみを行う。母音の
// 同定・声区の判定など診断的・教育的な結論は導かない (ADR-0006)。
#pragma once
#include <complex>
#include <cstddef>
#include <string>
#include <vector>

#include "AnalysisQuality.h"
#include "ArrayView.h"

namespace ofd {
namespace acoustics {

struct FormantEstimatorConfig {
    double targetInternalRateHz; // 内部 fs の目標 (既定 10 kHz)
    double preEmphasis;          // プリエンファシス係数 (既定 0.97)
    int maxLpcOrder;             // LPC 次数の上限 (既定 24)
    double minFormantHz;         // 候補の下限周波数 (既定 90 Hz)
    double maxBandwidthHz;       // 候補の帯域幅上限 (既定 400 Hz)

    FormantEstimatorConfig()
        : targetInternalRateHz(10000.0), preEmphasis(0.97), maxLpcOrder(24),
          minFormantHz(90.0), maxBandwidthHz(400.0) {}
};

// 1 フレームのフォルマント推定結果 (F0 軌跡と同じフレーム割り)
struct FormantFrame {
    double timeSeconds; // フレーム中心時刻 [s] (YIN と同じ (start + W/2)/fs)
    bool voiced;        // 入力の有声判定 (無声フレームは推定しない)
    bool valid;         // 有声かつ候補が 1 つ以上得られたか
    double f1Hz, f2Hz, f3Hz; // 0 = 候補なし
    double b1Hz, b2Hz, b3Hz; // 対応する帯域幅 [Hz] (候補なしは 0)

    FormantFrame()
        : timeSeconds(0.0), voiced(false), valid(false), f1Hz(0.0), f2Hz(0.0),
          f3Hz(0.0), b1Hz(0.0), b2Hz(0.0), b3Hz(0.0) {}
};

struct FormantTrackResult {
    std::vector<FormantFrame> frames; // 全フレーム分 (F0 軌跡と同数)
    double internalRateHz;            // 間引き後の内部 fs [Hz]
    std::size_t decimationFactor;     // 整数間引き率 (>= 1)
    int lpcOrder;                     // 実際に使った LPC 次数 p

    // 代表値: 各フォルマントが得られた有効フレームの時間中央値
    MetricValue f1MedianHz;
    MetricValue f2MedianHz;
    MetricValue f3MedianHz;

    std::string warning; // 全体が評価不能な場合の理由 (空 = 問題なし)

    FormantTrackResult()
        : frames(), internalRateHz(0.0), decimationFactor(1), lpcOrder(0),
          f1MedianHz(), f2MedianHz(), f3MedianHz(), warning() {}
};

// ── 数値部品 (単体テストのため公開) ──

// Levinson-Durbin 再帰: 自己相関 r[0..order] から LPC 係数を求める。
// 出力 lpc は長さ order+1、lpc[0] = 1 で A(z) = Σ lpc[i]·z^-i。
// predictionError には最終予測誤差 (> 0)。r[0] <= 0 や予測誤差の
// 非正化 (数値破綻) では false を返す。
bool levinsonDurbin(const std::vector<double> &autocorr, int order,
                    std::vector<double> &lpc, double &predictionError);

// Durand-Kerner 法による実係数多項式の全複素根。
// coeffs[0]·z^n + coeffs[1]·z^(n-1) + … + coeffs[n] = 0 (先頭の 0 係数は
// 除去する)。初期値は決定的な (0.4+0.9i)^(k+1) (乱数不使用)。
// 収束 (更新量 < tolerance) または残差が十分小さければ true。
// 次数 0 以下 (定数・空・全て 0) は false。
bool durandKernerRoots(const std::vector<double> &coeffs,
                       std::vector<std::complex<double> > &roots,
                       int maxIterations = 500, double tolerance = 1e-12);

class FormantEstimator {
public:
    explicit FormantEstimator(
        const FormantEstimatorConfig &config = FormantEstimatorConfig());

    // x: 元信号、sampleRateHz: 元 fs、voicedFlags: フレーム毎の有声判定
    // (YIN の判定をそのまま渡す)、frameLength / hopLength: 元 fs 基準の
    // フレーム長・ホップ [サンプル] (YIN と同一の値を渡すこと)。
    // フレーム i の開始は i·hopLength、中心時刻は (start + W/2)/fs。
    FormantTrackResult estimate(ArrayView<const double> x, double sampleRateHz,
                                const std::vector<unsigned char> &voicedFlags,
                                std::size_t frameLength,
                                std::size_t hopLength) const;

    const FormantEstimatorConfig &config() const { return m_config; }

private:
    FormantEstimatorConfig m_config;
};

} // namespace acoustics
} // namespace ofd
