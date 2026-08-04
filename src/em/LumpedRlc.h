// LumpedRlc.h — 集中定数 RLC のインピーダンス (Qt 非依存 / C++17)
//
// 回路系電磁解析タブ (src/tabs/CircuitSolversTab.cpp) の「結果」ページで
// |Z(f)| 曲線と各周波数の値を出すための解析式。PEEC / FEM による寄生抽出は
// 未実装なので、**抽出結果ではなく利用者が与えた集中定数モデル**の
// インピーダンスであることを GUI 側で明示すること。
//
// 定義 (時間因子 e^{jωt}):
//   直列 RLC : Z = R + jωL + 1/(jωC)
//              |Z| = √(R² + (ωL − 1/ωC)²)
//   並列 RLC : Y = 1/R + 1/(jωL) + jωC
//              |Z| = 1/√((1/R)² + (ωC − 1/ωL)²)
//   共振周波数 f0 = 1/(2π√(LC))
// いずれも教科書式 (例: Pozar, "Microwave Engineering", 4th ed., §6.1)。
//
// 素子の「不在」の扱い:
//   直列回路で C ≤ 0 / L ≤ 0 は「その素子が無い = 短絡 (0 Ω)」、
//   並列回路で L ≤ 0 / C ≤ 0 は「その素子が無い = 開放 (アドミタンス 0)」。
//   R は直列では 0 Ω (短絡)、並列では G = 0 (開放) として扱う。
#ifndef OFD_EM_LUMPEDRLC_H
#define OFD_EM_LUMPEDRLC_H

namespace ofd {
namespace em {

enum class RlcTopology { Series = 0, Parallel = 1 };

struct RlcModel {
    double r_ohm = 0;
    double l_H = 0;
    double c_F = 0;
    RlcTopology topology = RlcTopology::Series;
};

struct RlcImpedance {
    double xL_ohm = 0;         // ωL  (L ≤ 0 なら 0)
    double xC_ohm = 0;         // 1/ωC (C ≤ 0 なら 0)
    double magnitude_ohm = 0;  // |Z|
    bool   valid = false;      // f > 0 かつ素子が 1 つ以上あるか
};

// 周波数 f_Hz における |Z|。f_Hz ≤ 0 は valid=false。
RlcImpedance rlcImpedance(const RlcModel &m, double f_Hz);

// LC 共振周波数 f0 = 1/(2π√(LC)) [Hz]。L または C が 0 以下なら 0。
double rlcResonanceHz(double l_H, double c_F);

} // namespace em
} // namespace ofd

#endif // OFD_EM_LUMPEDRLC_H
