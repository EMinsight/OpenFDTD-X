// generate_synthetic_rir.cpp — 既知 RT の人工 RIR を生成する CLI。
//
// モデル:
//   h(t) = δ(t-t0) + Σ a_i δ(t-t0-τ_i)
//          + n(t)·exp(-6.91 (t-t0) / RT)   (指数減衰ホワイトノイズ)
//          + noiseFloor·w(t)               (定常ノイズフロア)
// exp(-6.91 t / RT) は t = RT でエネルギー -60 dB に対応する
// (20·log10(e^-6.91) ≈ -60 dB)。
//
// 設計方針: 生成した wav ファイルはリポジトリにコミットしない。
// テストは必要な RIR を実行時にその場で生成する (tests/acoustics/ 参照)。
// tests/acoustic_data/metadata.json に標準ケースの生成条件を記録してあり、
// 本 CLI で同一条件の wav を再現できる。
//
// 使い方:
//   generate_synthetic_rir --out rir.wav [--rt 1.0] [--fs 48000]
//       [--duration 1.8] [--direct-delay-ms 10] [--decay-rms 0.3]
//       [--noise-db -60] [--reflection MS:DB]... [--seed 20260716]
//       [--metadata-out meta.json]
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "../../src/acoustics/core/AudioBuffer.h"
#include "../../src/acoustics/io/WavWriter.h"

namespace {

// 決定的 LCG (tests/acoustics/test_common.h と同一)
unsigned lcgNext(unsigned &state) {
    state = state * 1664525u + 1013904223u;
    return state;
}
double lcgUniform(unsigned &state) {
    return (static_cast<double>(lcgNext(state) >> 8) / 8388608.0) - 1.0;
}

struct Options {
    double rt60;
    double fs;
    double duration;       // <= 0 で自動 (1.5·RT + 0.3)
    double directDelayMs;
    double decayRms;
    double noiseDb;        // <= -900 で無効
    unsigned seed;
    std::string outPath;
    std::string metadataPath;
    std::vector<std::pair<double, double>> reflections; // {ms, dB}

    Options()
        : rt60(1.0), fs(48000.0), duration(0.0), directDelayMs(10.0),
          decayRms(0.3), noiseDb(-1000.0), seed(20260716u), outPath(),
          metadataPath(), reflections() {}
};

void usage() {
    std::printf(
        "usage: generate_synthetic_rir --out FILE [options]\n"
        "  --rt SEC             残響時間 RT60 (既定 1.0; 0.5/1.0/1.5/2.0/3.0 が標準)\n"
        "  --fs HZ              サンプリング周波数 (既定 48000)\n"
        "  --duration SEC       信号長 (既定 1.5*RT + 0.3)\n"
        "  --direct-delay-ms MS 直接音の位置 (既定 10)\n"
        "  --decay-rms V        減衰雑音の初期 RMS (既定 0.3)\n"
        "  --noise-db DB        ノイズフロア [dBFS rms] (既定 なし)\n"
        "  --reflection MS:DB   反射を追加 (直接音基準の遅延と相対レベル、複数可)\n"
        "  --seed N             乱数シード (既定 20260716)\n"
        "  --metadata-out FILE  生成条件を JSON で書き出す\n");
}

bool parseArgs(int argc, char **argv, Options &opt) {
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        const bool hasNext = (i + 1 < argc);
        if (a == "--rt" && hasNext) {
            opt.rt60 = std::atof(argv[++i]);
        } else if (a == "--fs" && hasNext) {
            opt.fs = std::atof(argv[++i]);
        } else if (a == "--duration" && hasNext) {
            opt.duration = std::atof(argv[++i]);
        } else if (a == "--direct-delay-ms" && hasNext) {
            opt.directDelayMs = std::atof(argv[++i]);
        } else if (a == "--decay-rms" && hasNext) {
            opt.decayRms = std::atof(argv[++i]);
        } else if (a == "--noise-db" && hasNext) {
            opt.noiseDb = std::atof(argv[++i]);
        } else if (a == "--seed" && hasNext) {
            opt.seed = static_cast<unsigned>(std::strtoul(argv[++i], nullptr, 10));
        } else if (a == "--out" && hasNext) {
            opt.outPath = argv[++i];
        } else if (a == "--metadata-out" && hasNext) {
            opt.metadataPath = argv[++i];
        } else if (a == "--reflection" && hasNext) {
            const char *s = argv[++i];
            const char *colon = std::strchr(s, ':');
            if (colon == nullptr) return false;
            opt.reflections.push_back(
                std::make_pair(std::atof(s), std::atof(colon + 1)));
        } else {
            return false;
        }
    }
    return !opt.outPath.empty() && opt.rt60 > 0.0 && opt.fs > 0.0;
}

std::vector<double> generate(const Options &opt) {
    const double fs = opt.fs;
    const double dur =
        (opt.duration > 0.0) ? opt.duration : 1.5 * opt.rt60 + 0.3;
    const std::size_t n = static_cast<std::size_t>(dur * fs + 0.5);
    const std::size_t d0 =
        static_cast<std::size_t>(opt.directDelayMs * 0.001 * fs + 0.5);
    std::vector<double> h(n, 0.0);
    unsigned st = opt.seed;

    // 指数減衰ホワイトノイズ e^(-6.91 t / RT)·n(t)
    if (opt.decayRms > 0.0) {
        const double k = 6.91 / (opt.rt60 * fs);
        const double amp = opt.decayRms * std::sqrt(3.0); // 一様乱数→RMS 換算
        for (std::size_t i = d0; i < n; ++i)
            h[i] += amp * lcgUniform(st) *
                    std::exp(-k * static_cast<double>(i - d0));
    }
    // 直接音デルタ
    if (d0 < n) h[d0] += 1.0;
    // 反射 (遅延 / レベル指定)
    for (std::size_t r = 0; r < opt.reflections.size(); ++r) {
        const std::size_t idx =
            d0 + static_cast<std::size_t>(
                     opt.reflections[r].first * 0.001 * fs + 0.5);
        if (idx < n)
            h[idx] += std::pow(10.0, opt.reflections[r].second / 20.0);
    }
    // ノイズフロア付加
    if (opt.noiseDb > -900.0) {
        const double amp = std::pow(10.0, opt.noiseDb / 20.0) * std::sqrt(3.0);
        for (std::size_t i = 0; i < n; ++i) h[i] += amp * lcgUniform(st);
    }
    return h;
}

bool writeMetadata(const Options &opt, std::size_t sampleCount) {
    std::FILE *fp = std::fopen(opt.metadataPath.c_str(), "wb");
    if (fp == nullptr) return false;
    std::fprintf(fp, "{\n");
    std::fprintf(fp, "  \"_comment\": \"人工 RIR の生成条件。wav はコミットせず"
                     "テスト実行時に都度生成する。\",\n");
    std::fprintf(fp, "  \"generator\": \"tests/generators/generate_synthetic_rir.cpp\",\n");
    std::fprintf(fp, "  \"outFile\": \"%s\",\n", opt.outPath.c_str());
    std::fprintf(fp, "  \"rt60Seconds\": %.6g,\n", opt.rt60);
    std::fprintf(fp, "  \"sampleRateHz\": %.6g,\n", opt.fs);
    std::fprintf(fp, "  \"durationSeconds\": %.6g,\n",
                 (opt.duration > 0.0) ? opt.duration : 1.5 * opt.rt60 + 0.3);
    std::fprintf(fp, "  \"sampleCount\": %lu,\n",
                 static_cast<unsigned long>(sampleCount));
    std::fprintf(fp, "  \"directDelayMs\": %.6g,\n", opt.directDelayMs);
    std::fprintf(fp, "  \"decayNoiseRms\": %.6g,\n", opt.decayRms);
    if (opt.noiseDb > -900.0)
        std::fprintf(fp, "  \"noiseFloorDb\": %.6g,\n", opt.noiseDb);
    else
        std::fprintf(fp, "  \"noiseFloorDb\": null,\n");
    std::fprintf(fp, "  \"reflections\": [");
    for (std::size_t i = 0; i < opt.reflections.size(); ++i) {
        std::fprintf(fp, "%s{\"delayMs\": %.6g, \"levelDb\": %.6g}",
                     (i == 0) ? "" : ", ", opt.reflections[i].first,
                     opt.reflections[i].second);
    }
    std::fprintf(fp, "],\n");
    std::fprintf(fp, "  \"seed\": %u\n", opt.seed);
    std::fprintf(fp, "}\n");
    return std::fclose(fp) == 0;
}

} // namespace

int main(int argc, char **argv) {
    Options opt;
    if (!parseArgs(argc, argv, opt)) {
        usage();
        return 2;
    }

    ofd::acoustics::AudioBuffer buf;
    buf.sampleRateHz = opt.fs;
    buf.channels.push_back(generate(opt));

    ofd::acoustics::AcousticResult<bool> r = ofd::acoustics::writeWavFile(
        opt.outPath, buf, ofd::acoustics::WavSampleFormat::Float32);
    if (!r.success()) {
        std::fprintf(stderr, "error: %s (%s)\n", r.message().c_str(),
                     ofd::acoustics::acousticErrorCodeName(r.errorCode()));
        return 1;
    }
    if (!opt.metadataPath.empty() &&
        !writeMetadata(opt, buf.sampleCount())) {
        std::fprintf(stderr, "error: cannot write metadata: %s\n",
                     opt.metadataPath.c_str());
        return 1;
    }
    std::printf("wrote %s (RT=%.3g s, fs=%.0f Hz, %lu samples)\n",
                opt.outPath.c_str(), opt.rt60, opt.fs,
                static_cast<unsigned long>(buf.sampleCount()));
    return 0;
}
