// AudioBuffer.h — 多チャンネル音声バッファ (指示書 §9)。Qt 非依存 / C++14。
// 内部表現は double。自動正規化は行わない (読み込み時のスケールを保持する)。
#pragma once
#include <cstddef>
#include <vector>

namespace ofd {
namespace acoustics {

struct AudioBuffer {
    double sampleRateHz;                        // サンプリング周波数 [Hz]
    std::vector<std::vector<double>> channels;  // channels[ch][sample]

    AudioBuffer() : sampleRateHz(0.0), channels() {}

    std::size_t channelCount() const { return channels.size(); }

    // 先頭チャンネルのサンプル数 (全チャンネル同数を前提とする)
    std::size_t sampleCount() const {
        return channels.empty() ? 0 : channels[0].size();
    }

    double durationSeconds() const {
        return (sampleRateHz > 0.0)
                   ? static_cast<double>(sampleCount()) / sampleRateHz
                   : 0.0;
    }
};

} // namespace acoustics
} // namespace ofd
