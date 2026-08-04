// ParaxialTrace.h — 順次光学系の近軸光線追跡 (y-nu 追跡).
//
// Qt 非依存の純粋計算。LensEditorTab の面テーブルから焦点距離・主点・
// バックフォーカス・F 値・近軸像高といった **近軸量** を求める。
// 収差 (球面収差・コマ・非点・歪曲・スポット径・MTF) は実光線追跡が必要で
// あり、ここには含めない (タブ側で「—」と表示する)。
//
// 定式化 (標準の近軸追跡。W. J. Smith, "Modern Optical Engineering",
// 4th ed., §2 / R. Kingslake, "Lens Design Fundamentals" §2 と同じ):
//   屈折 : n'u' = n·u − y·φ,   φ = (n' − n)/R   (平面は φ = 0)
//   転送 : y_next = y + u'·t
#pragma once
#include <vector>

namespace ofd {
namespace paraxial {

// 追跡に使う 1 面 (物体面と像面は含めない — 屈折面だけを順に並べる)
struct Surface {
    double R = 0.0;        // 曲率半径 [mm] (0 = 平面)
    double thickness = 0;  // この面から次の面までの距離 [mm]
    double nAfter = 1.0;   // この面の後ろ側の屈折率
    double semiD = 0.0;    // 有効半径 [mm] (0 = 不明)
    bool   stop = false;   // 絞り面
};

struct SystemData {
    bool   valid = false;
    double efl = 0;           // 有効焦点距離 f' [mm]
    double bfl = 0;           // 最終面頂点 → 後側焦点 [mm]
    double ffl = 0;           // 前側焦点 → 第1面頂点 [mm]
    double backPrincipal = 0; // 最終面頂点 → 後側主点 H' [mm] (= bfl − f')
    double frontPrincipal = 0;// 第1面頂点 → 前側主点 H  [mm] (= f' − ffl … 符号は下記)
    double fnumber = 0;       // 像側 F 値 = f' / EPD (EPD > 0 のとき)
    double imageHeight = 0;   // 近軸像高 f'·tan(視野半角) [mm]
    double totalTrack = 0;    // 第1面 → 最後の面 までの距離 [mm]
    // 面テーブル上の像面位置と近軸焦点位置のずれ [mm]
    // (+ は近軸焦点が像面より後ろ = 像面が手前にある)
    bool   hasImagePlane = false;
    double defocus = 0;
};

// surfaces: 屈折面の並び。imageDistance: 最後の面から像面までの距離 [mm]
// (負値なら像面情報なしとして defocus を計算しない)。
// epd: 入射瞳径 [mm] (<=0 なら F 値を出さない)、fieldHalf_deg: 視野半角 [deg]。
SystemData analyze(const std::vector<Surface> &surfaces, double imageDistance,
                   double epd, double fieldHalf_deg);

} // namespace paraxial
} // namespace ofd
