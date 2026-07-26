// test_estimator.cpp — AcousticFdtdEstimator (音響 FDTD 規模見積り) の検証。
// 既知ケース: 10×8×3 m, fmax = 1 kHz, N = 10, c = 343 m/s, T = 1 s
//   λ = 0.343 m, Δx = 0.0343 m, Δt = Δx/(343·√3) ≈ 5.7735e-5 s,
//   格子数 = ceil(L/Δx) = 292×234×88 (切り上げ — 実装仕様),
//   メモリ = 総セル × 4 field × 8 byte, ステップ数 = ceil(T/Δt) = 17321。
#include <cmath>
#include <cstdio>
#include <limits>

#include "../../src/acoustics/core/AcousticFdtdEstimator.h"
#include "test_common.h"

using namespace ofd::acoustics;

namespace {

FdtdEstimateInput knownInput() {
    FdtdEstimateInput in;
    in.lxMeters = 10.0;
    in.lyMeters = 8.0;
    in.lzMeters = 3.0;
    in.fmaxHz = 1000.0;
    in.cellsPerWavelength = 10.0;
    in.speedOfSound = 343.0;
    in.simulationTimeSeconds = 1.0;
    return in;
}

void expectInvalid(const FdtdEstimateInput &in, const char *what) {
    AcousticResult<FdtdEstimate> r = AcousticFdtdEstimator::estimate(in);
    CHECK(!r.success());
    CHECK(r.errorCode() == AcousticErrorCode::InvalidArgument);
    if (r.success()) std::printf("  expected failure for: %s\n", what);
}

} // namespace

int main() {
    // ── 既知ケース ──
    {
        AcousticResult<FdtdEstimate> r =
            AcousticFdtdEstimator::estimate(knownInput());
        CHECK(r.success());
        const FdtdEstimate &e = r.value();

        CHECK_NEAR(e.wavelengthMeters, 0.343, 1e-12);
        CHECK_NEAR(e.cellSizeMeters, 0.0343, 1e-12);
        // Δt = Δx/(c·√3): 数式そのもの + 数値 5.7735e-5 の両方で確認
        CHECK_NEAR(e.timeStepSeconds, 0.0343 / (343.0 * std::sqrt(3.0)), 1e-18);
        CHECK_NEAR(e.timeStepSeconds, 5.7735e-5, 1e-9);

        // 格子数は切り上げ: 10/0.0343 = 291.5… → 292 など
        CHECK(e.nx == 292u);
        CHECK(e.ny == 234u);
        CHECK(e.nz == 88u);
        CHECK(e.totalCells == 292ull * 234ull * 88ull); // 6,012,864

        // メモリ = 4 field (p + vx/vy/vz) × double × 総セル
        CHECK(e.memoryBytes == e.totalCells * 4ull * 8ull);
        CHECK(e.memoryBytes == 6012864ull * 32ull);

        // ステップ数 = ceil(T/Δt) = ceil(17320.5…) = 17321
        CHECK(e.totalSteps == 17321ull);
        CHECK_REL(e.cellUpdates,
                  static_cast<double>(e.totalCells) *
                      static_cast<double>(e.totalSteps), 1e-12);

        std::printf("  known case: dx=%.6g m dt=%.6g s grid=%llux%llux%llu "
                    "cells=%llu mem=%.1f MB steps=%llu updates=%.3g\n",
                    e.cellSizeMeters, e.timeStepSeconds,
                    (unsigned long long)e.nx, (unsigned long long)e.ny,
                    (unsigned long long)e.nz,
                    (unsigned long long)e.totalCells,
                    e.memoryBytes / 1048576.0,
                    (unsigned long long)e.totalSteps, e.cellUpdates);
    }

    // ── 既定値 (N = 10, c = 343) が適用されること ──
    {
        FdtdEstimateInput in; // 既定コンストラクタの N / c を使う
        in.lxMeters = 10.0;
        in.lyMeters = 8.0;
        in.lzMeters = 3.0;
        in.fmaxHz = 1000.0;
        in.simulationTimeSeconds = 1.0;
        CHECK_NEAR(in.cellsPerWavelength, 10.0, 0.0);
        CHECK_NEAR(in.speedOfSound, 343.0, 0.0);
        AcousticResult<FdtdEstimate> r = AcousticFdtdEstimator::estimate(in);
        CHECK(r.success());
        CHECK_NEAR(r.value().cellSizeMeters, 0.0343, 1e-12);
    }

    // ── スケーリング: fmax 2 倍 → Δx 半分・格子数ほぼ 2 倍/軸 ──
    {
        FdtdEstimateInput in = knownInput();
        in.fmaxHz = 2000.0;
        AcousticResult<FdtdEstimate> r = AcousticFdtdEstimator::estimate(in);
        CHECK(r.success());
        CHECK_NEAR(r.value().cellSizeMeters, 0.01715, 1e-12);
        CHECK(r.value().nx == 584u); // ceil(10/0.01715) = ceil(583.09) = 584
    }

    // ── 境界: 室寸法 < Δx でも各軸最低 1 セル ──
    {
        FdtdEstimateInput in = knownInput();
        in.lxMeters = 0.001;
        in.lyMeters = 0.001;
        in.lzMeters = 0.001;
        AcousticResult<FdtdEstimate> r = AcousticFdtdEstimator::estimate(in);
        CHECK(r.success());
        CHECK(r.value().nx == 1u && r.value().ny == 1u && r.value().nz == 1u);
        CHECK(r.value().totalCells == 1u);
        CHECK(r.value().memoryBytes == 32u);
    }

    // ── 境界: T < Δt でも最低 1 ステップ ──
    {
        FdtdEstimateInput in = knownInput();
        in.simulationTimeSeconds = 1e-9;
        AcousticResult<FdtdEstimate> r = AcousticFdtdEstimator::estimate(in);
        CHECK(r.success());
        CHECK(r.value().totalSteps == 1u);
    }

    // ── 無効入力: 非正・非有限はすべて InvalidArgument ──
    {
        FdtdEstimateInput in = knownInput();
        in.lxMeters = 0.0;
        expectInvalid(in, "Lx = 0");
    }
    {
        FdtdEstimateInput in = knownInput();
        in.lyMeters = -8.0;
        expectInvalid(in, "Ly < 0");
    }
    {
        FdtdEstimateInput in = knownInput();
        in.lzMeters = std::numeric_limits<double>::quiet_NaN();
        expectInvalid(in, "Lz = NaN");
    }
    {
        FdtdEstimateInput in = knownInput();
        in.fmaxHz = 0.0;
        expectInvalid(in, "fmax = 0");
    }
    {
        FdtdEstimateInput in = knownInput();
        in.fmaxHz = std::numeric_limits<double>::infinity();
        expectInvalid(in, "fmax = inf");
    }
    {
        FdtdEstimateInput in = knownInput();
        in.cellsPerWavelength = 0.0;
        expectInvalid(in, "N = 0");
    }
    {
        FdtdEstimateInput in = knownInput();
        in.speedOfSound = -343.0;
        expectInvalid(in, "c < 0");
    }
    {
        FdtdEstimateInput in = knownInput();
        in.simulationTimeSeconds = 0.0;
        expectInvalid(in, "T = 0");
    }

    return testutil::summary("test_estimator");
}
