// FlankingTransmission.h — 側路伝搬 (flanking) の経路合成。
// Qt 非依存 / C++14 相当の素の C++17。SoundproofTab「側路伝搬」ページの
// 合成 R'w と改善案の効果の計算実体。
//
// ── 合成則 (EN 12354-1) ────────────────────────────────────────────────────
//
// 各経路の音響透過損失 R_i [dB] は透過率 τ_i = 10^(−R_i/10) に対応し、
// 経路は**透過率で加算**される:
//
//     R'w = −10·log10( Σ_i 10^(−(R_i + ΔR_i)/10) )
//
// ΔR_i は改善策による経路 i の改善量 [dB]。**合成は必ず最も弱い経路より
// 悪くなる** (透過率が足し合わさるため) ので、最も弱い経路を直さない限り
// 全体は良くならない — 改善案の効果はこの性質がそのまま出る。
//
// ── ここで扱わないもの (絶対規則 5) ────────────────────────────────────────
//
// 経路別の R_i そのものの予測 (EN 12354-1 の振動低減指数 Kij と要素の
// R、接合部の形状から出す) は**未実装**。R_i は利用者の入力値として受け取る。
#ifndef OFD_CORE_FLANKINGTRANSMISSION_H
#define OFD_CORE_FLANKINGTRANSMISSION_H

#include <vector>

namespace ofd {
namespace flanking {

// 1 経路。enabled = false の経路は合成に入らない。
struct Path {
    double R_dB = 0.0;        // 入力の音響透過損失
    double deltaR_dB = 0.0;   // 改善策による改善量 (>= 0)
    bool   enabled = true;
};

struct Combined {
    bool   valid = false;     // 有効な経路が 1 つも無ければ false
    double rw_dB = 0.0;       // 改善後の合成 R'w
    double base_dB = 0.0;     // 改善前 (ΔR をすべて 0 とした) 合成 R'w
    double gain_dB = 0.0;     // rw − base (改善量。0 以上)
    int    paths = 0;         // 合成に入った経路の数
    int    weakestIndex = -1; // 合成後に最も弱い (R+ΔR が最小の) 経路
};

Combined combine(const std::vector<Path> &paths);

} // namespace flanking
} // namespace ofd

#endif // OFD_CORE_FLANKINGTRANSMISSION_H
