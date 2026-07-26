// WavReader.h — RIFF/WAVE 読み込み。Qt 非依存 / C++14。
//
// 対応: PCM 16/24/32 bit, IEEE float 32 bit, 任意チャンネル数 / 任意 fs、
// WAVE_FORMAT_EXTENSIBLE、odd サイズチャンクのパディング。
// 自動正規化はしない (整数 PCM はフルスケール ±1.0 への変換のみ、
// float はそのままの値を保持する)。
#pragma once
#include <cstddef>
#include <string>

#include "../core/AcousticError.h"
#include "../core/AudioBuffer.h"

namespace ofd {
namespace acoustics {

AcousticResult<AudioBuffer> readWavFile(const std::string &path);
AcousticResult<AudioBuffer> readWavFromMemory(const unsigned char *data,
                                              std::size_t size);

} // namespace acoustics
} // namespace ofd
