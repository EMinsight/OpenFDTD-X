// SourceSpectrum.h — IlluminationOpts のスペクトルモデル → 測色量.
//
// IlluminationTab のフォーム値 (ガウシアンローブ / 黒体温度) から分光分布
// S(λ) を組み立て、`optics/Colorimetry` で色度・CCT・Duv・発光効率を計算する。
// GUI から切り離してあるので selftest から直接検証できる。
#pragma once
#include <functional>

#include "Colorimetry.h"

namespace ofd {

struct IlluminationOpts;

namespace optics {

// スペクトルモデル → 分光分布 S(λ) [相対単位]
std::function<double(double)> sourceSpectrum(const IlluminationOpts &o);

// 測色量 (すべて S(λ) から計算した値。サンプル値は一切含まない)
struct SourceColor {
    bool   valid = false;             // S(λ) が有効で三刺激値が出た
    colorimetry::Chromaticity chrom;  // (x,y) と (u',v')
    colorimetry::CctResult    cct;    // CCT [K] と Duv (定義できない場合 invalid)
    double efficacy_lm_W = 0;         // 放射束あたりの光束 K [lm/W]
    double peak_nm = 0;               // 分光分布のピーク波長
};

SourceColor evaluateSource(const IlluminationOpts &o);

} // namespace optics
} // namespace ofd
