// WavWriter.h — RIFF/WAVE 書き出し (PCM16 / IEEE float32)。
// Qt 非依存 / C++14。自動正規化はしない (PCM16 は ±1.0 を超える値を
// クランプして量子化、float32 は値をそのまま書き出す)。
#pragma once
#include <string>

#include "../core/AcousticError.h"
#include "../core/AudioBuffer.h"

namespace ofd {
namespace acoustics {

enum class WavSampleFormat {
    Pcm16,   // 16 bit 整数 PCM
    Float32  // 32 bit IEEE float
};

// buffer の全チャンネルをインターリーブして書き出す。
// 全チャンネルのサンプル数は一致している必要がある。
AcousticResult<bool> writeWavFile(const std::string &path,
                                  const AudioBuffer &buffer,
                                  WavSampleFormat format =
                                      WavSampleFormat::Float32);

} // namespace acoustics
} // namespace ofd
