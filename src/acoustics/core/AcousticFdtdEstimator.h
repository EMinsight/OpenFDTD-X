// AcousticFdtdEstimator.h — 音響 FDTD の実行前規模見積り (フェーズ5)。
// Qt 非依存 / C++14。
//
// 電磁用ステータスバーの見積り式の流用ではなく独立実装
// (ADR-0004 Decision 5、docs/opera-acoustics-assumptions.md §23):
//   波長          λ  = c / f_max
//   セル寸法      Δx = λ / N            (既定 N = 10 — λ/10 規則)
//   時間刻み      Δt = Δx / (c·√3)      (3D CFL 上限をそのまま採用)
//   格子数        n{x,y,z} = ceil(L{x,y,z} / Δx)   (端数セルは切り上げ)
//   総セル数      nx·ny·nz
//   メモリ        総セル数 × 4 field × 8 byte
//                 (圧力 p + 粒子速度 vx/vy/vz の 4 スカラー場, double)
//   総ステップ数  ceil(T / Δt)
//   概算演算量    総セル数 × 総ステップ数 (セル更新回数)
//
// 実ソルバーの離散化 (高次スキーム・非一様格子・PML 層) によって実所要量は
// 係数倍変わる。見積りは「実行可否の事前警告」用であり正確な予約量ではない。
#pragma once
#include <cstdint>

#include "AcousticError.h"

namespace ofd {
namespace acoustics {

// 見積り入力。寸法・周波数・時間はすべて正の有限値であること。
struct FdtdEstimateInput {
    double lxMeters;            // 室寸法 X [m]
    double lyMeters;            // 室寸法 Y [m]
    double lzMeters;            // 室寸法 Z [m]
    double fmaxHz;              // 解析上限周波数 [Hz]
    double cellsPerWavelength;  // 1 波長あたり格子数 N (既定 10)
    double speedOfSound;        // 音速 c [m/s] (既定 343 — 仮定 §11)
    double simulationTimeSeconds; // シミュレーション時間 T [s]

    FdtdEstimateInput()
        : lxMeters(0.0), lyMeters(0.0), lzMeters(0.0), fmaxHz(0.0),
          cellsPerWavelength(10.0), speedOfSound(343.0),
          simulationTimeSeconds(0.0) {}
};

// 見積り結果 (ヘッダ冒頭の式を参照)
struct FdtdEstimate {
    double wavelengthMeters;      // λ = c / f_max
    double cellSizeMeters;        // Δx = λ / N
    double timeStepSeconds;       // Δt = Δx / (c·√3)
    std::uint64_t nx;             // ceil(Lx / Δx)
    std::uint64_t ny;             // ceil(Ly / Δx)
    std::uint64_t nz;             // ceil(Lz / Δx)
    std::uint64_t totalCells;     // nx·ny·nz
    std::uint64_t memoryBytes;    // totalCells × 4 field × sizeof(double)
    std::uint64_t totalSteps;     // ceil(T / Δt)
    double cellUpdates;           // totalCells × totalSteps (概算演算量。
                                  // uint64 溢れ回避のため double で保持)

    FdtdEstimate()
        : wavelengthMeters(0.0), cellSizeMeters(0.0), timeStepSeconds(0.0),
          nx(0), ny(0), nz(0), totalCells(0), memoryBytes(0), totalSteps(0),
          cellUpdates(0.0) {}
};

class AcousticFdtdEstimator {
public:
    // メモリ見積りのフィールド数: 圧力 + 粒子速度 3 成分
    static const int kFieldCount = 4;
    // 1 フィールド 1 セルあたりのバイト数 (double)
    static const int kBytesPerField = 8;

    // 入力が不正 (非正・非有限) の場合は InvalidArgument を返す。
    static AcousticResult<FdtdEstimate> estimate(const FdtdEstimateInput &input);
};

} // namespace acoustics
} // namespace ofd
