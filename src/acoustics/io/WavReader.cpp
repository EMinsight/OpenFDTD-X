// WavReader.cpp — RIFF/WAVE パーサの実装 (エンディアン非依存のバイト合成)。
#include "WavReader.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace ofd {
namespace acoustics {

namespace {
typedef AcousticResult<AudioBuffer> Result;

// リトルエンディアンのバイト列から整数を組み立てる (ホスト非依存)
unsigned readU16(const unsigned char *p) {
    return static_cast<unsigned>(p[0]) | (static_cast<unsigned>(p[1]) << 8);
}
unsigned long readU32(const unsigned char *p) {
    return static_cast<unsigned long>(p[0]) |
           (static_cast<unsigned long>(p[1]) << 8) |
           (static_cast<unsigned long>(p[2]) << 16) |
           (static_cast<unsigned long>(p[3]) << 24);
}

bool tagEquals(const unsigned char *p, const char *tag) {
    return std::memcmp(p, tag, 4) == 0;
}

// 符号つき PCM のデコード (フルスケール ±1.0 へ変換するのみ。正規化しない)
double decodePcm16(const unsigned char *p) {
    int v = static_cast<int>(readU16(p));
    if (v >= 32768) v -= 65536;
    return static_cast<double>(v) / 32768.0;
}
double decodePcm24(const unsigned char *p) {
    long v = static_cast<long>(p[0]) | (static_cast<long>(p[1]) << 8) |
             (static_cast<long>(p[2]) << 16);
    if (v >= 8388608L) v -= 16777216L;
    return static_cast<double>(v) / 8388608.0;
}
double decodePcm32(const unsigned char *p) {
    unsigned long u = readU32(p);
    long long v = static_cast<long long>(u);
    if (v >= 2147483648LL) v -= 4294967296LL;
    return static_cast<double>(v) / 2147483648.0;
}
double decodeFloat32(const unsigned char *p) {
    unsigned long u = readU32(p);
    unsigned int u32 = static_cast<unsigned int>(u);
    float f;
    std::memcpy(&f, &u32, sizeof(f));
    return static_cast<double>(f);
}
} // namespace

AcousticResult<AudioBuffer> readWavFromMemory(const unsigned char *data,
                                              std::size_t size) {
    if (data == nullptr || size == 0)
        return Result::error(AcousticErrorCode::EmptyInput,
                             "no data to parse");
    if (size < 12 || !tagEquals(data, "RIFF") || !tagEquals(data + 8, "WAVE"))
        return Result::error(AcousticErrorCode::UnsupportedFormat,
                             "not a RIFF/WAVE file");

    // チャンク走査 (odd サイズは 1 バイトのパディングを飛ばす)
    bool haveFmt = false;
    unsigned audioFormat = 0, channels = 0, bitsPerSample = 0;
    unsigned long sampleRate = 0;
    const unsigned char *dataChunk = nullptr;
    std::size_t dataSize = 0;

    std::size_t pos = 12;
    while (pos + 8 <= size) {
        const unsigned char *hdr = data + pos;
        const std::size_t chunkSize =
            static_cast<std::size_t>(readU32(hdr + 4));
        const std::size_t body = pos + 8;
        if (body + chunkSize > size)
            return Result::error(AcousticErrorCode::FileReadError,
                                 "truncated chunk");
        if (tagEquals(hdr, "fmt ")) {
            if (chunkSize < 16)
                return Result::error(AcousticErrorCode::UnsupportedFormat,
                                     "fmt chunk too small");
            const unsigned char *f = data + body;
            audioFormat = readU16(f);
            channels = readU16(f + 2);
            sampleRate = readU32(f + 4);
            bitsPerSample = readU16(f + 14);
            // WAVE_FORMAT_EXTENSIBLE: SubFormat GUID の先頭 2 バイトが実形式
            if (audioFormat == 0xFFFE) {
                if (chunkSize < 40)
                    return Result::error(AcousticErrorCode::UnsupportedFormat,
                                         "extensible fmt chunk too small");
                audioFormat = readU16(f + 24);
            }
            haveFmt = true;
        } else if (tagEquals(hdr, "data")) {
            dataChunk = data + body;
            dataSize = chunkSize;
        }
        pos = body + chunkSize + (chunkSize & 1); // odd パディング対応
    }

    if (!haveFmt)
        return Result::error(AcousticErrorCode::UnsupportedFormat,
                             "missing fmt chunk");
    if (dataChunk == nullptr)
        return Result::error(AcousticErrorCode::UnsupportedFormat,
                             "missing data chunk");
    if (channels == 0)
        return Result::error(AcousticErrorCode::UnsupportedFormat,
                             "zero channels");
    if (sampleRate == 0)
        return Result::error(AcousticErrorCode::UnsupportedSampleRate,
                             "zero sample rate");

    // 形式チェック: PCM 16/24/32, IEEE float 32
    const bool isPcm = (audioFormat == 1);
    const bool isFloat = (audioFormat == 3);
    if (!isPcm && !isFloat)
        return Result::error(AcousticErrorCode::UnsupportedFormat,
                             "unsupported audio format tag");
    if (isPcm && bitsPerSample != 16 && bitsPerSample != 24 &&
        bitsPerSample != 32)
        return Result::error(AcousticErrorCode::UnsupportedFormat,
                             "unsupported PCM bit depth");
    if (isFloat && bitsPerSample != 32)
        return Result::error(AcousticErrorCode::UnsupportedFormat,
                             "unsupported float bit depth");

    const std::size_t bytesPerSample = bitsPerSample / 8;
    const std::size_t frameSize = bytesPerSample * channels;
    const std::size_t frames = dataSize / frameSize;

    AudioBuffer buf;
    buf.sampleRateHz = static_cast<double>(sampleRate);
    buf.channels.assign(channels, std::vector<double>(frames, 0.0));

    for (std::size_t fr = 0; fr < frames; ++fr) {
        const unsigned char *p = dataChunk + fr * frameSize;
        for (unsigned ch = 0; ch < channels; ++ch) {
            const unsigned char *sp = p + ch * bytesPerSample;
            double v;
            if (isFloat) {
                v = decodeFloat32(sp);
            } else if (bitsPerSample == 16) {
                v = decodePcm16(sp);
            } else if (bitsPerSample == 24) {
                v = decodePcm24(sp);
            } else {
                v = decodePcm32(sp);
            }
            buf.channels[ch][fr] = v;
        }
    }
    return Result::ok(buf);
}

AcousticResult<AudioBuffer> readWavFile(const std::string &path) {
    std::FILE *fp = std::fopen(path.c_str(), "rb");
    if (fp == nullptr)
        return Result::error(AcousticErrorCode::FileNotFound,
                             "cannot open: " + path);
    std::vector<unsigned char> bytes;
    unsigned char tmp[65536];
    std::size_t got;
    while ((got = std::fread(tmp, 1, sizeof(tmp), fp)) > 0)
        bytes.insert(bytes.end(), tmp, tmp + got);
    const bool readErr = (std::ferror(fp) != 0);
    std::fclose(fp);
    if (readErr)
        return Result::error(AcousticErrorCode::FileReadError,
                             "read error: " + path);
    if (bytes.empty())
        return Result::error(AcousticErrorCode::EmptyInput,
                             "empty file: " + path);
    return readWavFromMemory(bytes.data(), bytes.size());
}

} // namespace acoustics
} // namespace ofd
