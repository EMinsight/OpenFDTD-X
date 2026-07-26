// AcousticFdtdEstimator.cpp — 音響 FDTD の実行前規模見積り。
// 式の定義と根拠はヘッダおよび ADR-0004 Decision 5 を参照。
#include "AcousticFdtdEstimator.h"

#include <cmath>

namespace ofd {
namespace acoustics {

namespace {

bool isPositiveFinite(double v) {
    return std::isfinite(v) && v > 0.0;
}

// 正の実数の切り上げ → 最低 1 (室寸法が Δx 未満でも 1 セルは確保する)
std::uint64_t ceilCells(double lengthMeters, double cellSizeMeters) {
    const double n = std::ceil(lengthMeters / cellSizeMeters);
    return (n < 1.0) ? 1u : static_cast<std::uint64_t>(n);
}

} // namespace

AcousticResult<FdtdEstimate>
AcousticFdtdEstimator::estimate(const FdtdEstimateInput &input) {
    typedef AcousticResult<FdtdEstimate> Result;

    if (!isPositiveFinite(input.lxMeters) || !isPositiveFinite(input.lyMeters) ||
        !isPositiveFinite(input.lzMeters)) {
        return Result::error(AcousticErrorCode::InvalidArgument,
                             "room dimensions must be positive finite [m]");
    }
    if (!isPositiveFinite(input.fmaxHz)) {
        return Result::error(AcousticErrorCode::InvalidArgument,
                             "fmax must be positive finite [Hz]");
    }
    if (!isPositiveFinite(input.cellsPerWavelength)) {
        return Result::error(AcousticErrorCode::InvalidArgument,
                             "cells per wavelength must be positive finite");
    }
    if (!isPositiveFinite(input.speedOfSound)) {
        return Result::error(AcousticErrorCode::InvalidArgument,
                             "speed of sound must be positive finite [m/s]");
    }
    if (!isPositiveFinite(input.simulationTimeSeconds)) {
        return Result::error(AcousticErrorCode::InvalidArgument,
                             "simulation time must be positive finite [s]");
    }

    FdtdEstimate e;
    // λ = c / f_max, Δx = λ / N, Δt = Δx / (c·√3)
    e.wavelengthMeters = input.speedOfSound / input.fmaxHz;
    e.cellSizeMeters = e.wavelengthMeters / input.cellsPerWavelength;
    e.timeStepSeconds = e.cellSizeMeters / (input.speedOfSound * std::sqrt(3.0));

    // 格子数は切り上げ (端数セルも 1 セルとして数える)
    e.nx = ceilCells(input.lxMeters, e.cellSizeMeters);
    e.ny = ceilCells(input.lyMeters, e.cellSizeMeters);
    e.nz = ceilCells(input.lzMeters, e.cellSizeMeters);
    e.totalCells = e.nx * e.ny * e.nz;

    // 圧力 + 粒子速度 3 成分 (4 field) × double
    e.memoryBytes = e.totalCells *
                    static_cast<std::uint64_t>(kFieldCount * kBytesPerField);

    e.totalSteps = static_cast<std::uint64_t>(
        std::ceil(input.simulationTimeSeconds / e.timeStepSeconds));
    if (e.totalSteps < 1) e.totalSteps = 1;

    e.cellUpdates = static_cast<double>(e.totalCells) *
                    static_cast<double>(e.totalSteps);

    return Result::ok(e);
}

} // namespace acoustics
} // namespace ofd
