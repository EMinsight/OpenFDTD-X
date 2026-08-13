// FieldCurvature.h — 像面湾曲 (サジタル / タンジェンシャル焦点のずれ) — Qt 非依存。
//
// レンズエディタタブの「Field Curvature」。視野角ごとに**光線が実際に交わる
// 位置**を求め、近軸像面からのずれを描く。
//
// ── 焦点探索は要らない ────────────────────────────────────────────────────
// 像空間では光線は直線なので、**像面を 2 枚置いて外挿すれば交点は閉形式で
// 厳密に解ける**。反復探索も刻み幅も要らない。
//
//   光線 i は  y_i(z) = y_i(0) + (y_i(Δ) − y_i(0))/Δ · z
//   2 本が交わる z は  z* = Δ·(y₂(0) − y₁(0)) / [(y₁(Δ) − y₁(0)) − (y₂(Δ) − y₂(0))]
//
// 分母が 0 のとき (2 本が平行) は交わらないので**無効を返す** — 無限遠を
// 0 や巨大な数にして絵を壊さない。
//
// ── サジタルとタンジェンシャル ────────────────────────────────────────────
// 視野を y 方向にとったとき、
//   タンジェンシャル (メリジオナル) = 瞳の y 方向の上下 2 本の交点 (y 座標で解く)
//   サジタル                        = 瞳の x 方向の左右 2 本の交点 (x 座標で解く)
// 2 つが離れているぶんが非点収差、両方が軸上からずれているぶんが像面湾曲。
//
// ── 3 次収差 (Seidel) とは混ぜない ────────────────────────────────────────
// `optics/SeidelAberration` も像面湾曲の係数 (S_III, S_IV) を持つが、あちらは
// **3 次の近似**で、こちらは実光線の交点。視野が大きいと両者は一致しない。
// 同じ図に混ぜず、実追跡の値だけを出す。
#ifndef OFD_OPTICS_FIELDCURVATURE_H
#define OFD_OPTICS_FIELDCURVATURE_H

#include <vector>

namespace ofd {
namespace optics {

// 2 本の光線が交わる z (像面 0 からの距離)。dz は 2 枚目の像面までの距離。
// a0/b0 = 像面 0 での 2 本の座標、aD/bD = 像面 dz での座標。
// 交わらない (平行) 場合は ok に false を入れて 0 を返す。
double crossingZ(double a0, double b0, double aD, double bD, double dz,
                 bool *ok = nullptr);

struct FieldCurvaturePoint {
    double field_deg = 0.0;
    double sagittal_mm = 0.0;      // 近軸像面からのずれ
    double tangential_mm = 0.0;
    bool   sagittalOk = false;
    bool   tangentialOk = false;
};

struct FieldCurvatureResult {
    std::vector<FieldCurvaturePoint> points;
    double maxAstigmatism_mm = 0.0;   // |T − S| の最大 (両方有効な点のみ)
    bool valid() const { return points.size() >= 2; }
};

// 像面 0 と像面 dz での「上下 2 本 / 左右 2 本」の座標を視野ごとに与える
// コールバック。tracer(field_deg, dz, out[4]) が
//   out[0], out[1] = タンジェンシャル 2 本の y 座標
//   out[2], out[3] = サジタル 2 本の x 座標
// を返す (追跡に失敗したら false)。
using PairTracer = bool (*)(double field_deg, double dz, double out[4],
                            void *user);

FieldCurvatureResult fieldCurvature(PairTracer tracer, void *user,
                                    double halfField_deg, int points,
                                    double dz_mm);

} // namespace optics
} // namespace ofd

#endif // OFD_OPTICS_FIELDCURVATURE_H
