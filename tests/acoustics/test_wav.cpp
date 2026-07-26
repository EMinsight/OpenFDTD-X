// test_wav.cpp — WAV の書き込み → 読み込み ラウンドトリップ検証。
// PCM16 / float32 は WavWriter で、PCM24 / PCM32 はテスト内で生の
// バイト列を組み立てて WavReader の読み込みを検証する。
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "../../src/acoustics/io/WavReader.h"
#include "../../src/acoustics/io/WavWriter.h"
#include "test_common.h"

using namespace ofd::acoustics;

namespace {

std::string tempPath(const char *name) {
    // TMPDIR (POSIX/CI) → TEMP/TMP (Windows) → /tmp の順で解決する
    const char *dir = std::getenv("TMPDIR");
    if (dir == nullptr || dir[0] == '\0') dir = std::getenv("TEMP");
    if (dir == nullptr || dir[0] == '\0') dir = std::getenv("TMP");
    if (dir == nullptr || dir[0] == '\0') dir = "/tmp";
    return std::string(dir) + "/" + name;
}

// 決定的なテスト波形 (±1 近傍を含む)
std::vector<double> makeSamples(std::size_t n, double phase) {
    std::vector<double> v(n);
    for (std::size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(n);
        v[i] = 0.999 * std::sin(6.2831853 * (3.0 * t + phase));
    }
    if (n > 3) {
        v[0] = 0.999;
        v[1] = -0.999;
        v[2] = 0.0;
    }
    return v;
}

// ── 生バイトによる PCM24/32 wav の組み立て (リトルエンディアン) ──
void putU16(std::vector<unsigned char> &v, unsigned x) {
    v.push_back(static_cast<unsigned char>(x & 0xFF));
    v.push_back(static_cast<unsigned char>((x >> 8) & 0xFF));
}
void putU32(std::vector<unsigned char> &v, unsigned long x) {
    for (int k = 0; k < 4; ++k)
        v.push_back(static_cast<unsigned char>((x >> (8 * k)) & 0xFF));
}
void putTag(std::vector<unsigned char> &v, const char *t) {
    v.insert(v.end(), t, t + 4);
}

// bits = 24 or 32 の整数 PCM wav を組み立てる。
// 先頭に odd サイズ (3 バイト) の LIST チャンクを置き、パディング処理を試す。
std::vector<unsigned char> buildIntPcmWav(
    const std::vector<std::vector<double>> &chans, unsigned long fs,
    unsigned bits, bool oddChunkBefore) {
    const unsigned ch = static_cast<unsigned>(chans.size());
    const std::size_t frames = chans[0].size();
    const unsigned bps = bits / 8;
    const unsigned block = ch * bps;
    std::vector<unsigned char> body;

    if (oddChunkBefore) {
        putTag(body, "LIST");
        putU32(body, 3); // odd サイズ → 1 バイトのパディングが必要
        body.push_back('a');
        body.push_back('b');
        body.push_back('c');
        body.push_back(0); // パディング
    }
    putTag(body, "fmt ");
    putU32(body, 16);
    putU16(body, 1); // PCM
    putU16(body, ch);
    putU32(body, fs);
    putU32(body, fs * block);
    putU16(body, block);
    putU16(body, bits);
    putTag(body, "data");
    putU32(body, static_cast<unsigned long>(frames) * block);
    const double scale = (bits == 24) ? 8388608.0 : 2147483648.0;
    const double maxV = scale - 1.0;
    for (std::size_t fr = 0; fr < frames; ++fr) {
        for (unsigned c = 0; c < ch; ++c) {
            double x = chans[c][fr];
            double q = std::floor(x * scale + 0.5);
            if (q > maxV) q = maxV;
            if (q < -scale) q = -scale;
            long long v = static_cast<long long>(q);
            unsigned long long u =
                static_cast<unsigned long long>(v & 0xFFFFFFFFll);
            for (unsigned k = 0; k < bps; ++k)
                body.push_back(
                    static_cast<unsigned char>((u >> (8 * k)) & 0xFF));
        }
    }
    if ((body.size() & 1) != 0) body.push_back(0);

    std::vector<unsigned char> out;
    putTag(out, "RIFF");
    putU32(out, static_cast<unsigned long>(4 + body.size()));
    putTag(out, "WAVE");
    out.insert(out.end(), body.begin(), body.end());
    return out;
}

void checkBuffer(const AudioBuffer &got,
                 const std::vector<std::vector<double>> &want, double fs,
                 double tol) {
    CHECK(got.sampleRateHz == fs);
    CHECK(got.channelCount() == want.size());
    if (got.channelCount() != want.size()) return;
    CHECK(got.sampleCount() == want[0].size());
    if (got.sampleCount() != want[0].size()) return;
    double maxErr = 0.0;
    for (std::size_t c = 0; c < want.size(); ++c) {
        for (std::size_t i = 0; i < want[c].size(); ++i) {
            const double e = std::fabs(got.channels[c][i] - want[c][i]);
            if (e > maxErr) maxErr = e;
        }
    }
    CHECK(maxErr <= tol);
    if (maxErr > tol)
        std::printf("  max round-trip error %.3g > tol %.3g\n", maxErr, tol);
}

void testWriterRoundTrip(WavSampleFormat fmt, unsigned channels, double fs,
                         double tol, const char *name) {
    AudioBuffer buf;
    buf.sampleRateHz = fs;
    for (unsigned c = 0; c < channels; ++c)
        buf.channels.push_back(makeSamples(1000, 0.1 * c));

    const std::string path = tempPath(name);
    AcousticResult<bool> w = writeWavFile(path, buf, fmt);
    CHECK(w.success());
    if (!w.success()) {
        std::printf("  write failed: %s\n", w.message().c_str());
        return;
    }
    AcousticResult<AudioBuffer> r = readWavFile(path);
    CHECK(r.success());
    if (!r.success()) {
        std::printf("  read failed: %s\n", r.message().c_str());
        return;
    }
    checkBuffer(r.value(), buf.channels, fs, tol);
    std::remove(path.c_str());
}

void testRawIntRoundTrip(unsigned bits, unsigned channels, double fs,
                         double tol, bool oddChunk, const char *name) {
    std::vector<std::vector<double>> chans;
    // 奇数サンプル数 (24bit モノラルなら data サイズが奇数になる)
    for (unsigned c = 0; c < channels; ++c)
        chans.push_back(makeSamples(999, 0.2 * c));

    const std::vector<unsigned char> bytes = buildIntPcmWav(
        chans, static_cast<unsigned long>(fs), bits, oddChunk);
    const std::string path = tempPath(name);
    std::FILE *fp = std::fopen(path.c_str(), "wb");
    CHECK(fp != nullptr);
    if (fp == nullptr) return;
    std::fwrite(bytes.data(), 1, bytes.size(), fp);
    std::fclose(fp);

    AcousticResult<AudioBuffer> r = readWavFile(path);
    CHECK(r.success());
    if (!r.success()) {
        std::printf("  read failed: %s\n", r.message().c_str());
        return;
    }
    checkBuffer(r.value(), chans, fs, tol);
    std::remove(path.c_str());
}

} // namespace

int main() {
    // WavWriter → WavReader (PCM16 / float32, mono / stereo, 各種 fs)
    testWriterRoundTrip(WavSampleFormat::Pcm16, 1, 48000.0, 1.0e-4,
                        "t_wav_pcm16_mono.wav");
    testWriterRoundTrip(WavSampleFormat::Pcm16, 2, 44100.0, 1.0e-4,
                        "t_wav_pcm16_stereo.wav");
    testWriterRoundTrip(WavSampleFormat::Float32, 1, 96000.0, 1.0e-6,
                        "t_wav_f32_mono.wav");
    testWriterRoundTrip(WavSampleFormat::Float32, 2, 12345.0, 1.0e-6,
                        "t_wav_f32_stereo.wav");

    // 生バイト組み立てによる PCM24 / PCM32 (odd チャンクパディング込み)
    testRawIntRoundTrip(24, 1, 48000.0, 3.0e-7, true, "t_wav_pcm24_mono.wav");
    testRawIntRoundTrip(24, 2, 44100.0, 3.0e-7, false, "t_wav_pcm24_stereo.wav");
    testRawIntRoundTrip(32, 1, 48000.0, 2.0e-9, false, "t_wav_pcm32_mono.wav");
    testRawIntRoundTrip(32, 2, 32000.0, 2.0e-9, true, "t_wav_pcm32_stereo.wav");

    // エラー系: 存在しないファイル / WAV でないデータ
    {
        AcousticResult<AudioBuffer> r =
            readWavFile(tempPath("t_wav_missing_file.wav"));
        CHECK(!r.success());
        CHECK(r.errorCode() == AcousticErrorCode::FileNotFound);
    }
    {
        const unsigned char junk[16] = {'n', 'o', 't', 'a', 'w', 'a', 'v', '!',
                                        0, 1, 2, 3, 4, 5, 6, 7};
        AcousticResult<AudioBuffer> r = readWavFromMemory(junk, sizeof(junk));
        CHECK(!r.success());
        CHECK(r.errorCode() == AcousticErrorCode::UnsupportedFormat);
    }

    return testutil::summary("test_wav");
}
