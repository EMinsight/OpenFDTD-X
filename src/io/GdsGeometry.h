// GdsGeometry.h — GDSII の多角形 → FDTD の直方体ジオメトリ
//
// レイアウトタブの「FDTD へ渡す」を実際に動かすための変換。
// `Project` の形状モデルは本家 .ofd の形状コード (直方体・楕円体・円柱 …) で、
// **任意多角形を受ける型が無い**。そこで多角形を軸平行な矩形の集合へ分解し、
// レイヤーごとの z 範囲で押し出して直方体 (shape = 1) にする。
//
// 分解は**水平スラブ法**:
//   1. 頂点の y 座標で区間 (スラブ) に切る
//   2. 各スラブの中央 y で多角形の辺との交点を求め、x 昇順に並べて
//      偶奇規則で内部区間にする
//   3. 上下で x 区間が一致するスラブは 1 つの矩形へ結合する
//
// **面積は厳密に保存される**。頂点 y の間では多角形の幅が y の 1 次式なので、
// 中点で測った幅の積分は厳密 (中点則が 1 次式に対して厳密であるため)。
// 一方 **形状**が厳密なのは全辺が軸平行 (Manhattan) のときだけで、斜辺は
// 階段近似になる。シリコンフォトニクスのレイアウトは通常 Manhattan なので
// 実用上は厳密だが、そうでない場合は `manhattan = false` で呼び出し側へ伝え、
// 画面に「階段近似です」と出す (絶対規則 5)。
#ifndef OFD_IO_GDSGEOMETRY_H
#define OFD_IO_GDSGEOMETRY_H

#include <QVector>

#include "GdsIO.h"
#include "../core/Geometry.h"

namespace ofd {

// 軸平行矩形 (単位は GdsPolygon と同じ meter)
struct GdsRect {
    double x0 = 0.0, x1 = 0.0, y0 = 0.0, y1 = 0.0;
    double area() const { return (x1 - x0) * (y1 - y0); }
};

struct GdsDecompose {
    bool             valid = false;
    bool             manhattan = true;  // 全辺が軸平行だったか
    QVector<GdsRect> rects;
    double           polygonArea = 0.0; // 靴紐公式による面積 (絶対値)
    double           rectArea = 0.0;    // 分解後の面積合計
};

// 多角形 1 個を軸平行矩形へ分解する。
// 頂点が 3 個未満、または面積が 0 のときは valid = false。
GdsDecompose decomposePolygon(const GdsPolygon &p);

// レイヤー 1 枚の押し出し条件
struct GdsLayerExtrude {
    int    layer = 0;        // GDS レイヤー番号
    double z0_m = 0.0;       // 下端 [m]
    double z1_m = 0.0;       // 上端 [m]
    int    materialId = 2;   // 材料番号 (0 = 空気, 1 = PEC, 2 以降がユーザー定義)
    QString name;            // 生成する形状の名前の接頭辞 (GUI 表示用)
};

struct GdsToGeometryResult {
    QVector<Geometry> units;
    int  polygons = 0;         // 対象になった多角形の数
    int  rects = 0;            // 生成した直方体の数
    int  skippedPolygons = 0;  // 面積 0 などで捨てた多角形
    int  nonManhattan = 0;     // 斜辺を含んでいた多角形の数 (階段近似)
    double totalArea_m2 = 0.0; // 生成した直方体の底面積の合計
};

// トップセル (空なら最初の構造) の中で、指定されたレイヤーの多角形を
// 直方体へ変換する。z1 <= z0 のレイヤーは厚みが無いので無視する。
GdsToGeometryResult gdsToGeometry(const GdsLibrary &lib,
                                  const QString &topCell,
                                  const QVector<GdsLayerExtrude> &layers);

} // namespace ofd

#endif // OFD_IO_GDSGEOMETRY_H
