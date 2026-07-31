// Geometry.h — one geometry unit, 1:1 with the OpenFDTD "geometry =" line.
//
// 本家の形状コード (sol/ingeometry.c):
//   1            直方体      g[0..5] = X1 X2 Y1 Y2 Z1 Z2
//   2            楕円体      g[0..5] = 外接直方体
//   11/12/13     円柱 X/Y/Z  g[0..5] = 外接直方体
//   31/32/33     三角柱      g[0..7]
//   41/42/43     角錐台      g[0..7]
//   51/52/53     円錐台      g[0..7]
// 重なった領域は後のユニットが優先 (ユニット番号 = リスト順)。
#pragma once
#include <QString>

namespace ofd {

enum class Axis { X, Y, Z };

struct Geometry {
    int     materialId = 2;   // index into the material table (0=air, 1=PEC)
    int     shape = 1;        // 本家 shape code
    double  g[8] = {0,0,0,0,0,0,0,0};
    QString name;             // GUI only (emitted as "name = " line, kernel ignores it)

    static int paramCount(int shape) {
        switch (shape) {
            case 1: case 2: case 11: case 12: case 13:
                return 6;
            case 31: case 32: case 33:
            case 41: case 42: case 43:
            case 51: case 52: case 53:
                return 8;
        }
        return 6;
    }

    // 軸 axis (0=x, 1=y, 2=z) の「座標」を保持する g[] インデックス対応表
    // (本家 sol/ingeometry.c 準拠)。平行移動・回転・ミラーが共用する。
    // idx に書き込んだ個数を返す (座標でないパラメータ = 寸法は含まない):
    //   1/2/11/12/13 : 軸ごとの区間ペア {2a, 2a+1}
    //   31/32/33     : 柱軸 {0,1}、断面第1軸 (柱軸+1)%3 = {2,3,4}、
    //                  第2軸 (柱軸+2)%3 = {5,6,7}   (inout3 の (u,v) 順)
    //   41..43/51..53: 錐軸 {0,1}、断面中心 (錐軸+1)%3 = {2}、
    //                  (錐軸+2)%3 = {3}   (g[4..7] は寸法なので対象外)
    static int coordIndices(int shape, int axis, int idx[3]) {
        switch (shape) {
            case 1: case 2: case 11: case 12: case 13:
                idx[0] = 2 * axis;
                idx[1] = 2 * axis + 1;
                return 2;
            case 31: case 32: case 33: {
                const int own = shape - 31;          // 柱軸
                if (axis == own) { idx[0] = 0; idx[1] = 1; return 2; }
                if (axis == (own + 1) % 3) {
                    idx[0] = 2; idx[1] = 3; idx[2] = 4;
                    return 3;
                }
                idx[0] = 5; idx[1] = 6; idx[2] = 7;
                return 3;
            }
            case 41: case 42: case 43:
            case 51: case 52: case 53: {
                const int own = shape % 10 - 1;      // 41/51→x, 42/52→y, …
                if (axis == own) { idx[0] = 0; idx[1] = 1; return 2; }
                idx[0] = (axis == (own + 1) % 3) ? 2 : 3;
                return 1;
            }
        }
        return 0;
    }
};

} // namespace ofd
