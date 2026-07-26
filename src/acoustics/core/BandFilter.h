// BandFilter.h — 4次バターワース帯域通過フィルタによる帯域分割。
// Qt 非依存 / C++14。係数は双一次変換 (プリワーピングつき) で自前計算する。
#pragma once
#include <cstddef>
#include <string>
#include <vector>

#include "AcousticError.h"
#include "ArrayView.h"

namespace ofd {
namespace acoustics {

// 分析帯域の定義
struct Band {
    std::string label;
    double centerHz; // 中心周波数 (幾何平均)
    double lowHz;    // 下側エッジ
    double highHz;   // 上側エッジ
    bool fullBand;   // true なら全帯域 (フィルタを適用しない)

    Band() : label(), centerHz(0.0), lowHz(0.0), highHz(0.0), fullBand(false) {}
    Band(const std::string &l, double c, double lo, double hi, bool full = false)
        : label(l), centerHz(c), lowHz(lo), highHz(hi), fullBand(full) {}
};

// 帯域セット
enum class BandSet {
    Compat6,           // 既存互換 6 帯域 {125, 250, 500, 1k, 2k, 4k}
    FullBandOnly,      // 全帯域 (フィルタなし) のみ
    Octave63To8k,      // 1 オクターブ {63 .. 8k}
    ThirdOctave100To5k,// 1/3 オクターブ {100 .. 5k}
    SingerFormant      // 歌手フォルマント帯域 {2.0-2.5k, 2.5-3.15k, 3.15-4.0k, 2.0-4.0k}
};

std::vector<Band> makeBands(BandSet set);

// 4次バターワース帯域通過フィルタ (双一次変換による IIR、z^-1 4次)。
class BandFilter {
public:
    BandFilter(); // 無効なフィルタ

    // lowHz〜highHz の帯域通過を設計する。highHz はナイキスト未満であること。
    static AcousticResult<BandFilter> design(double lowHz, double highHz,
                                             double sampleRateHz);

    bool valid() const { return m_valid; }

    // zeroPhase = true で前後方向フィルタリング (filtfilt 相当、ゼロ位相)。
    // その場合、振幅特性は 2 乗 (実効 8 次) になる。
    std::vector<double> apply(ArrayView<const double> x,
                              bool zeroPhase = false) const;

private:
    double m_b[5]; // 分子係数 (z^-1 の昇冪)
    double m_a[5]; // 分母係数 (a[0] = 1)
    bool m_valid;
};

// band.fullBand の場合は入力の複製を返す。
AcousticResult<std::vector<double>> filterBand(ArrayView<const double> x,
                                               double sampleRateHz,
                                               const Band &band,
                                               bool zeroPhase = false);

} // namespace acoustics
} // namespace ofd
