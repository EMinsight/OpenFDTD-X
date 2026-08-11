// LevelSum.h — デシベル量のエネルギー加算と寄与分析 (Qt 非依存 / C++17)
//
// 「複数の騒音源が同時に鳴っているとき、合計は何 dB で、どれを潰せば
// 何 dB 下がるか」を出す。車内音響 (NVH) タブの騒音源の寄与分析が用途だが、
// 式そのものは非干渉なパワー量の加算なので EMC の合成にも使える。
//
//   合計       L = 10·log10( Σ 10^(Lᵢ/10) )
//   寄与率     pᵢ = 10^(Lᵢ/10) / Σ 10^(Lⱼ/10)
//   除去効果   ΔLᵢ = L − 10·log10( Σ_{j≠i} 10^(Lⱼ/10) )  (= −10·log10(1 − pᵢ))
//
// **前提は「音源同士が無相関」**であること。相関があると干渉で ±6 dB まで
// ずれる (同相なら +6 dB、逆相なら打ち消し)。エンジン・ロードノイズ・風切り音
// のような独立な発生源には妥当だが、同一源の多重経路には使えない。
//
// レベルは利用者が入力した値で、校正の無い計算結果ではない (絶対規則 6)。
// この関数は入力された dB を足すだけで、SPL の校正には関与しない。
#ifndef OFD_CORE_LEVELSUM_H
#define OFD_CORE_LEVELSUM_H

#include <vector>

namespace ofd {
namespace levelsum {

struct Contribution {
    double level_db = 0.0;       // 入力レベル (そのまま返す)
    double share = 0.0;          // 寄与率 0..1 (エネルギー比)
    double removalGain_db = 0.0; // この源を completely 消したときの低減量 [dB]
};

struct Result {
    bool   valid = false;
    double total_db = 0.0;       // 合計レベル
    int    dominantIndex = -1;   // 最大寄与の添字
    // 支配的な源を消しても合計が下がりにくいか (2 番目以降が近いとき) の
    // 目安として、上位 2 つの差 [dB] を返す。要素が 1 つなら 0。
    double topTwoGap_db = 0.0;
    std::vector<Contribution> parts;
};

// levels_db: 各源のレベル [dB]。空なら valid = false。
// 有限でない値が混ざっていたら valid = false (「それらしい合計」を作らない)。
Result energySum(const std::vector<double> &levels_db);

// **同相で重なったときの上限** (最悪ケース)。振幅で足すので
//   L = 20·log10( Σ 10^(Lᵢ/20) )
// 等レベル 2 源で +6.02 dB (無相関なら +3.01 dB)。クリップ余裕の見積りは
// こちらで取る — 無相関の合計だけを見ていると、たまたま位相が揃ったときに
// 割れる。
struct SumResult {
    bool   valid = false;
    double total_db = 0.0;
};
SumResult coherentSum(const std::vector<double> &levels_db);

// 自由音場の点音源の距離減衰 (逆二乗則): 1 m 基準からの差 [dB]
//   ΔL(d) = −20·log10(d / 1 m)
// 距離が 2 倍で −6.02 dB。d <= 0 は 0 を返す (減衰させない)。
double spreadingLoss_db(double distance_m);

} // namespace levelsum
} // namespace ofd

#endif // OFD_CORE_LEVELSUM_H
