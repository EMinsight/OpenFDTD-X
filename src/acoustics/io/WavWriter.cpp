// WavWriter.cpp — RIFF/WAVE 書き出しの実装 (エンディアン非依存)。
#include "WavWriter.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace ofd {
namespace acoustics {

namespace {
typedef AcousticResult<bool> Result;

void putU16(std::vector<unsigned char> &v, unsigned x) {
    v.push_back(static_cast<unsigned char>(x & 0xFF));
    v.push_back(static_cast<unsigned char>((x >> 8) & 0xFF));
}
void putU32(std::vector<unsigned char> &v, unsigned long x) {
    v.push_back(static_cast<unsigned char>(x & 0xFF));
    v.push_back(static_cast<unsigned char>((x >> 8) & 0xFF));
    v.push_back(static_cast<unsigned char>((x >> 16) & 0xFF));
    v.push_back(static_cast<unsigned char>((x >> 24) & 0xFF));
}
void putTag(std::vector<unsigned char> &v, const char *tag) {
    v.insert(v.end(), tag, tag + 4);
}

// PCM16 量子化: ±1.0 を超える値はクランプ (自動正規化はしない)
int quantizePcm16(double x) {
    if (x > 1.0) x = 1.0;
    if (x < -1.0) x = -1.0;
    double v = std::floor(x * 32767.0 + 0.5);
    if (v > 32767.0) v = 32767.0;
    if (v < -32768.0) v = -32768.0;
    return static_cast<int>(v);
}
} // namespace

AcousticResult<bool> writeWavFile(const std::string &path,
                                  const AudioBuffer &buffer,
                                  WavSampleFormat format) {
    if (buffer.channelCount() == 0)
        return Result::error(AcousticErrorCode::EmptyInput, "no channels");
    if (buffer.sampleRateHz <= 0.0)
        return Result::error(AcousticErrorCode::UnsupportedSampleRate,
                             "sample rate must be positive");
    const std::size_t frames = buffer.sampleCount();
    for (std::size_t ch = 0; ch < buffer.channelCount(); ++ch) {
        if (buffer.channels[ch].size() != frames)
            return Result::error(AcousticErrorCode::InvalidArgument,
                                 "channel lengths differ");
    }

    const unsigned channels = static_cast<unsigned>(buffer.channelCount());
    const unsigned bits = (format == WavSampleFormat::Pcm16) ? 16u : 32u;
    const unsigned bytesPerSample = bits / 8u;
    const unsigned blockAlign = channels * bytesPerSample;
    const unsigned long sampleRate =
        static_cast<unsigned long>(buffer.sampleRateHz + 0.5);
    const unsigned long dataSize =
        static_cast<unsigned long>(frames) * blockAlign;
    const unsigned formatTag = (format == WavSampleFormat::Pcm16) ? 1u : 3u;

    std::vector<unsigned char> out;
    out.reserve(44 + dataSize + 1);
    putTag(out, "RIFF");
    putU32(out, 36 + dataSize + (dataSize & 1)); // odd データはパディング込み
    putTag(out, "WAVE");
    putTag(out, "fmt ");
    putU32(out, 16);
    putU16(out, formatTag);
    putU16(out, channels);
    putU32(out, sampleRate);
    putU32(out, sampleRate * blockAlign); // byte rate
    putU16(out, blockAlign);
    putU16(out, bits);
    putTag(out, "data");
    putU32(out, dataSize);

    for (std::size_t fr = 0; fr < frames; ++fr) {
        for (unsigned ch = 0; ch < channels; ++ch) {
            const double x = buffer.channels[ch][fr];
            if (format == WavSampleFormat::Pcm16) {
                int v = quantizePcm16(x);
                if (v < 0) v += 65536;
                putU16(out, static_cast<unsigned>(v));
            } else {
                const float f = static_cast<float>(x);
                unsigned int u32;
                std::memcpy(&u32, &f, sizeof(u32));
                putU32(out, static_cast<unsigned long>(u32));
            }
        }
    }
    if ((dataSize & 1) != 0) out.push_back(0); // odd チャンクのパディング

    std::FILE *fp = std::fopen(path.c_str(), "wb");
    if (fp == nullptr)
        return Result::error(AcousticErrorCode::FileWriteError,
                             "cannot open for write: " + path);
    const std::size_t wrote = std::fwrite(out.data(), 1, out.size(), fp);
    const int closeErr = std::fclose(fp);
    if (wrote != out.size() || closeErr != 0)
        return Result::error(AcousticErrorCode::FileWriteError,
                             "write failed: " + path);
    return Result::ok(true);
}

} // namespace acoustics
} // namespace ofd
