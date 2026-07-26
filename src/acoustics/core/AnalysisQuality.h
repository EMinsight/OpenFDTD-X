// AnalysisQuality.h — 分析品質と指標値の共通表現。Qt 非依存 / C++14。
#pragma once
#include <string>

namespace ofd {
namespace acoustics {

// 分析結果の信頼度区分
enum class AnalysisQuality {
    Valid,   // 有効 (基準を満たす)
    Warning, // 参考値 (品質低下の可能性: 決定係数不足など)
    Invalid  // 無効 (動的範囲不足などで評価不能)
};

// 各音響指標の値と品質。自動正規化は行わず、value は計算値そのまま。
struct MetricValue {
    double value;            // 指標値 (単位は指標ごとに定義)
    bool valid;              // 評価可能か
    AnalysisQuality quality; // 品質区分
    std::string warning;     // 品質低下・無効の理由 (空 = 問題なし)

    MetricValue()
        : value(0.0), valid(false), quality(AnalysisQuality::Invalid), warning() {}
};

inline MetricValue makeValidMetric(double v) {
    MetricValue m;
    m.value = v;
    m.valid = true;
    m.quality = AnalysisQuality::Valid;
    return m;
}

inline MetricValue makeWarningMetric(double v, const std::string &why) {
    MetricValue m;
    m.value = v;
    m.valid = true;
    m.quality = AnalysisQuality::Warning;
    m.warning = why;
    return m;
}

inline MetricValue makeInvalidMetric(const std::string &why) {
    MetricValue m;
    m.valid = false;
    m.quality = AnalysisQuality::Invalid;
    m.warning = why;
    return m;
}

} // namespace acoustics
} // namespace ofd
