// test_clipping.cpp — クリッピング検出の陽性系ユニットテスト (負債 #8)。
//
// 従来はクリッピングが「起きていないこと」(RirAnalyzer の
// preprocess.clippingDetected == false) しか確認していなかった。
// 本テストは実際にクリップする入力を与えて検出されることを確認する。
//
// 実装仕様 (コードから確認したもの — テストはこの仕様に合わせる):
//   RirAnalyzer (src/acoustics/core/RirAnalyzer.cpp):
//     - 判定は **|x| > clipThreshold の厳密な不等号** (既定 0.999)。
//       ちょうど閾値と等しいサンプルは検出されない。
//     - clipRunLength (既定 3) サンプル **連続** で 1 区間と数える。
//       連続数がこれ未満の孤立サンプルは検出されない。
//     - clippedRunCount は連続区間の数 (サンプル数ではない)。
//     - DC 除去の **前** に生サンプルに対して判定する。
//   ConvolutionEngine (src/acoustics/core/ConvolutionEngine.cpp):
//     - 判定は |y| > clipThreshold (既定 1.0) の厳密な不等号。
//     - clippedSampleCount は全チャンネル合計の **サンプル数**
//       (連続数の概念は無い)。clipped = (clippedSampleCount > 0)。
#include <cmath>
#include <cstdio>
#include <vector>

#include "../../src/acoustics/core/ConvolutionEngine.h"
#include "../../src/acoustics/core/RirAnalyzer.h"
#include "test_common.h"

using namespace ofd::acoustics;

namespace {

// クリップ検出の土台となる正常な人工 RIR (RT 0.5 s / 48 kHz / 1.05 s)
std::vector<double> baseRir(unsigned seed) {
    testutil::SyntheticRirSpec spec;
    spec.rt60 = 0.5;
    spec.directAmplitude = 0.8; // 既定閾値 0.999 を超えないようにしておく
    spec.seed = seed;
    return testutil::makeSyntheticRir(spec);
}

// 指定位置に count サンプル連続で value を書き込む
void writeRun(std::vector<double> &h, std::size_t at, std::size_t count,
              double value) {
    for (std::size_t i = 0; i < count && at + i < h.size(); ++i)
        h[at + i] = value;
}

// RIR を解析し PreprocessInfo を返す (解析失敗時は ok=false)
struct AnalyzeOutcome {
    bool ok;
    PreprocessInfo pre;
    std::vector<std::string> warnings;

    AnalyzeOutcome() : ok(false), pre(), warnings() {}
};

AnalyzeOutcome analyze(const std::vector<double> &h,
                       const RirAnalyzerConfig &cfg) {
    AnalyzeOutcome out;
    RirAnalyzer analyzer(cfg);
    AcousticResult<RirAnalysisResult> r =
        analyzer.analyze(ArrayView<const double>(h.data(), h.size()), 48000.0);
    out.ok = r.success();
    if (r.success()) {
        out.pre = r.value().preprocess;
        out.warnings = r.value().warnings;
    } else {
        std::printf("    analyze failed: %s\n", r.message().c_str());
    }
    return out;
}

RirAnalyzerConfig fullBandConfig() {
    RirAnalyzerConfig cfg;
    cfg.bandSet = BandSet::FullBandOnly; // 帯域分割は本テストに無関係
    return cfg;
}

bool hasClipWarning(const std::vector<std::string> &warnings) {
    for (std::size_t i = 0; i < warnings.size(); ++i) {
        if (warnings[i].find("clipping detected") != std::string::npos)
            return true;
    }
    return false;
}

// モノラル AudioBuffer
AudioBuffer makeMono(const std::vector<double> &x, double fs) {
    AudioBuffer b;
    b.sampleRateHz = fs;
    b.channels.push_back(x);
    return b;
}

} // namespace

int main() {
    const RirAnalyzerConfig defCfg = fullBandConfig();
    CHECK_NEAR(defCfg.clipThreshold, 0.999, 1e-12); // 仕様の固定
    CHECK(defCfg.clipRunLength == 3);

    // ── (1) 陽性: 閾値超えが 3 サンプル連続 → 検出される ──
    std::printf("== (1) positive: one clipped run ==\n");
    {
        std::vector<double> h = baseRir(101u);
        writeRun(h, 5000, 3, 1.2);
        const AnalyzeOutcome o = analyze(h, defCfg);
        CHECK(o.ok);
        CHECK(o.pre.clippingDetected);
        CHECK(o.pre.clippedRunCount == 1);
        CHECK(hasClipWarning(o.warnings));
        std::printf("    detected=%d runs=%d warning=%d\n",
                    o.pre.clippingDetected ? 1 : 0, o.pre.clippedRunCount,
                    hasClipWarning(o.warnings) ? 1 : 0);
    }

    // ── (2) 陽性: 負側 (絶対値で判定される) ──
    std::printf("== (2) positive: negative-going clip ==\n");
    {
        std::vector<double> h = baseRir(102u);
        writeRun(h, 5000, 4, -1.5);
        const AnalyzeOutcome o = analyze(h, defCfg);
        CHECK(o.ok);
        CHECK(o.pre.clippingDetected);
        CHECK(o.pre.clippedRunCount == 1);
    }

    // ── (3) 陽性: 独立した 3 区間 → clippedRunCount == 3 ──
    std::printf("== (3) positive: three separate runs ==\n");
    {
        std::vector<double> h = baseRir(103u);
        writeRun(h, 3000, 3, 1.1);
        writeRun(h, 6000, 8, 1.1);   // 長い区間でも 1 区間
        writeRun(h, 9000, 3, -1.1);
        const AnalyzeOutcome o = analyze(h, defCfg);
        CHECK(o.ok);
        CHECK(o.pre.clippingDetected);
        CHECK(o.pre.clippedRunCount == 3);
        std::printf("    runs=%d (expected 3)\n", o.pre.clippedRunCount);
    }

    // ── (4) 陰性: 連続数が clipRunLength 未満 (2 サンプル) → 検出しない ──
    std::printf("== (4) negative: run shorter than clipRunLength ==\n");
    {
        std::vector<double> h = baseRir(104u);
        writeRun(h, 5000, 2, 1.5);
        writeRun(h, 7000, 1, 2.0);
        const AnalyzeOutcome o = analyze(h, defCfg);
        CHECK(o.ok);
        CHECK(!o.pre.clippingDetected);
        CHECK(o.pre.clippedRunCount == 0);
        CHECK(!hasClipWarning(o.warnings));
    }

    // ── (5) 境界値: ちょうど閾値 (0.999) は検出しない (厳密な > 判定) ──
    std::printf("== (5) boundary: |x| == clipThreshold ==\n");
    {
        std::vector<double> h = baseRir(105u);
        writeRun(h, 5000, 8, defCfg.clipThreshold);
        writeRun(h, 7000, 8, -defCfg.clipThreshold);
        const AnalyzeOutcome o = analyze(h, defCfg);
        CHECK(o.ok);
        CHECK(!o.pre.clippingDetected);
        CHECK(o.pre.clippedRunCount == 0);
    }

    // ── (6) 境界値: 閾値の 1 ulp 上 → 検出する ──
    std::printf("== (6) boundary: clipThreshold + 1 ulp ==\n");
    {
        const double justAbove =
            std::nextafter(defCfg.clipThreshold, 2.0); // 0.999 の直上
        CHECK(justAbove > defCfg.clipThreshold);
        std::vector<double> h = baseRir(106u);
        writeRun(h, 5000, 3, justAbove);
        const AnalyzeOutcome o = analyze(h, defCfg);
        CHECK(o.ok);
        CHECK(o.pre.clippingDetected);
        CHECK(o.pre.clippedRunCount == 1);
        std::printf("    threshold=%.17g justAbove=%.17g detected=%d\n",
                    defCfg.clipThreshold, justAbove,
                    o.pre.clippingDetected ? 1 : 0);
    }

    // ── (7) 境界値: フルスケール 1.0 (既定閾値 0.999 < 1.0) → 検出する ──
    std::printf("== (7) boundary: full scale 1.0 ==\n");
    {
        std::vector<double> h = baseRir(107u);
        writeRun(h, 5000, 3, 1.0);
        const AnalyzeOutcome o = analyze(h, defCfg);
        CHECK(o.ok);
        CHECK(o.pre.clippingDetected);
        CHECK(o.pre.clippedRunCount == 1);
    }

    // ── (8) 設定の反映: 閾値と連続数を変えると判定も変わる ──
    std::printf("== (8) configurable threshold / run length ==\n");
    {
        std::vector<double> h = baseRir(108u);
        writeRun(h, 5000, 2, 0.6); // 既定では閾値未満・連続数不足

        RirAnalyzerConfig loose = fullBandConfig();
        const AnalyzeOutcome o1 = analyze(h, loose);
        CHECK(o1.ok);
        CHECK(!o1.pre.clippingDetected);

        RirAnalyzerConfig strict = fullBandConfig();
        strict.clipThreshold = 0.5;
        strict.clipRunLength = 2;
        const AnalyzeOutcome o2 = analyze(h, strict);
        CHECK(o2.ok);
        CHECK(o2.pre.clippingDetected);
        CHECK(o2.pre.clippedRunCount >= 1);
        std::printf("    thr=0.5 runLen=2 -> detected=%d runs=%d\n",
                    o2.pre.clippingDetected ? 1 : 0, o2.pre.clippedRunCount);
    }

    // ── (9) DC 除去より前に生サンプルで判定される ──
    // DC +0.4 を足すと生値は 1.05 (> 0.999) だが、DC 除去後の値は 0.65。
    // 検出は DC 除去前なので clippingDetected は true になる。
    std::printf("== (9) detection happens before DC removal ==\n");
    {
        std::vector<double> h = baseRir(109u);
        for (std::size_t i = 0; i < h.size(); ++i) h[i] = 0.5 * h[i] + 0.4;
        writeRun(h, 5000, 3, 1.05);
        RirAnalyzerConfig cfg = fullBandConfig();
        cfg.removeDc = true;
        const AnalyzeOutcome o = analyze(h, cfg);
        CHECK(o.ok);
        CHECK(o.pre.dcRemoved);
        CHECK(o.pre.dcOffset > 0.3); // 実際に DC が乗っていた
        CHECK(o.pre.clippingDetected);
        std::printf("    dcOffset=%.4f detected=%d\n", o.pre.dcOffset,
                    o.pre.clippingDetected ? 1 : 0);
    }

    // ── (10) 陰性の回帰確認: クリップの無い RIR では検出しない ──
    std::printf("== (10) negative: clean RIR ==\n");
    {
        const std::vector<double> h = baseRir(110u);
        const AnalyzeOutcome o = analyze(h, defCfg);
        CHECK(o.ok);
        CHECK(!o.pre.clippingDetected);
        CHECK(o.pre.clippedRunCount == 0);
        CHECK(!hasClipWarning(o.warnings));
    }

    // ── (11) ConvolutionEngine: サンプル数の陽性カウント ──
    // dry = 定数 0.9 (1000 サンプル) × RIR = 単一デルタ (ゲイン 2)
    // → 出力は 1000 サンプル全てが 1.8。
    std::printf("== (11) ConvolutionEngine: clipped sample count ==\n");
    {
        const std::vector<double> x(1000, 0.9);
        const std::vector<double> hDelta(1, 2.0);
        ConvolutionEngine engine; // 既定 clipThreshold = 1.0
        AcousticResult<ConvolvedAudio> r =
            engine.convolve(makeMono(x, 48000.0), makeMono(hDelta, 48000.0));
        CHECK(r.success());
        if (r.success()) {
            const ConvolutionInfo &info = r.value().info;
            CHECK(info.clipped);
            CHECK(info.clippedSampleCount == 1000);
            CHECK_NEAR(info.outputPeak, 1.8, 1e-9);
            std::printf("    count=%zu peak=%.4f gain=%.2f dB\n",
                        info.clippedSampleCount, info.outputPeak,
                        info.suggestedGainDb);
        }
    }

    // ── (12) ConvolutionEngine: 閾値を上げると検出されない ──
    std::printf("== (12) ConvolutionEngine: threshold gating ==\n");
    {
        const std::vector<double> x(1000, 0.9);
        const std::vector<double> hDelta(1, 2.0);
        ConvolutionEngineConfig cfg;
        cfg.clipThreshold = 2.0; // ピーク 1.8 < 2.0
        ConvolutionEngine engine(cfg);
        AcousticResult<ConvolvedAudio> r =
            engine.convolve(makeMono(x, 48000.0), makeMono(hDelta, 48000.0));
        CHECK(r.success());
        if (r.success()) {
            const ConvolutionInfo &info = r.value().info;
            CHECK(!info.clipped);
            CHECK(info.clippedSampleCount == 0);
            // 正規化はしないので出力ピークは 1.8 のまま
            CHECK_NEAR(info.outputPeak, 1.8, 1e-9);
        }
    }

    // ── (13) ConvolutionEngine: 全チャンネル合計で数える ──
    std::printf("== (13) ConvolutionEngine: multi-channel count ==\n");
    {
        const std::vector<double> x(1000, 0.9);
        AudioBuffer rir;
        rir.sampleRateHz = 48000.0;
        rir.channels.push_back(std::vector<double>(1, 2.0));
        rir.channels.push_back(std::vector<double>(1, 2.0));
        ConvolutionEngine engine;
        AcousticResult<ConvolvedAudio> r =
            engine.convolve(makeMono(x, 48000.0), rir);
        CHECK(r.success());
        if (r.success()) {
            const ConvolutionInfo &info = r.value().info;
            CHECK(r.value().audio.channelCount() == 2);
            CHECK(info.clipped);
            CHECK(info.clippedSampleCount == 2000); // 1000 × 2ch
            std::printf("    count=%zu (2ch)\n", info.clippedSampleCount);
        }
    }

    // ── (14) ConvolutionEngine: 一部サンプルだけがクリップする場合 ──
    std::printf("== (14) ConvolutionEngine: partial clipping ==\n");
    {
        // dry のうち 10 サンプルだけ振幅 0.9、残りは 0.1。RIR = デルタ(2.0)
        // → 出力で 1.0 を超えるのは 10 サンプルのみ。
        std::vector<double> x(500, 0.1);
        for (std::size_t i = 100; i < 110; ++i) x[i] = 0.9;
        const std::vector<double> hDelta(1, 2.0);
        ConvolutionEngine engine;
        AcousticResult<ConvolvedAudio> r =
            engine.convolve(makeMono(x, 48000.0), makeMono(hDelta, 48000.0));
        CHECK(r.success());
        if (r.success()) {
            const ConvolutionInfo &info = r.value().info;
            CHECK(info.clipped);
            CHECK(info.clippedSampleCount == 10);
            CHECK_NEAR(info.outputPeak, 1.8, 1e-9);
            std::printf("    count=%zu (expected 10)\n",
                        info.clippedSampleCount);
        }
    }

    // ── (15) ConvolutionEngine: クリップしない入力では警告も出ない ──
    std::printf("== (15) ConvolutionEngine: no clipping ==\n");
    {
        const std::vector<double> x(1000, 0.4);
        const std::vector<double> hDelta(1, 1.0);
        ConvolutionEngine engine;
        AcousticResult<ConvolvedAudio> r =
            engine.convolve(makeMono(x, 48000.0), makeMono(hDelta, 48000.0));
        CHECK(r.success());
        if (r.success()) {
            const ConvolutionInfo &info = r.value().info;
            CHECK(!info.clipped);
            CHECK(info.clippedSampleCount == 0);
            CHECK(info.warnings.empty());
            CHECK(info.suggestedGainDb > 0.0); // 上げる余地がある
        }
    }

    return testutil::summary("test_clipping");
}
