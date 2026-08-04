// SourceSpectrum.cpp
#include "SourceSpectrum.h"
#include "../core/Project.h"

#include <vector>

namespace ofd {
namespace optics {

using colorimetry::GaussLobe;

std::function<double(double)> sourceSpectrum(const IlluminationOpts &o)
{
    switch (o.spectrum) {
    case 1: {   // RGB 3 チップ
        std::vector<GaussLobe> lobes = {
            { o.rPeak_nm, o.rFwhm_nm, o.rRatio },
            { o.gPeak_nm, o.gFwhm_nm, o.gRatio },
            { o.bPeak_nm, o.bFwhm_nm, o.bRatio },
        };
        return [lobes](double l) { return colorimetry::lobeSpectrum(lobes, l); };
    }
    case 2: {   // フルスペクトル (黒体放射で近似)
        const double T = o.blackbody_K;
        return [T](double l) { return colorimetry::planckSpectrum(l, T); };
    }
    case 3: {   // 単色 (ガウシアン 1 本)
        std::vector<GaussLobe> lobes = {
            { o.monoPeak_nm, o.monoFwhm_nm, 1.0 },
        };
        return [lobes](double l) { return colorimetry::lobeSpectrum(lobes, l); };
    }
    case 0:
    default: {  // 白色 LED = 青 LED ピーク + 蛍光体 broad band
        std::vector<GaussLobe> lobes = {
            { o.bluePeak_nm, o.blueFwhm_nm, 1.0 },
            { o.phosPeak_nm, o.phosFwhm_nm, o.phosRatio },
        };
        return [lobes](double l) { return colorimetry::lobeSpectrum(lobes, l); };
    }
    }
}

SourceColor evaluateSource(const IlluminationOpts &o)
{
    SourceColor r;
    const std::function<double(double)> spd = sourceSpectrum(o);
    const colorimetry::XYZ xyz = colorimetry::integrate(spd);
    r.chrom = colorimetry::chromaticity(xyz);
    if (!r.chrom.valid) return r;
    r.cct = colorimetry::correlatedColorTemperature(r.chrom);
    r.efficacy_lm_W = colorimetry::luminousEfficacyOfRadiation(spd);
    r.peak_nm = colorimetry::peakWavelength(spd);
    r.valid = true;
    return r;
}

} // namespace optics
} // namespace ofd
