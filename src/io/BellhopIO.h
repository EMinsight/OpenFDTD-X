// BellhopIO.h — 水中音響カーネル bellhopcxx (bellhopcuda) の入力生成。
//
// UnderwaterTab / OceanEnvironmentTab の設定 (UnderwaterOpts) から
// BELLHOP の .env テキストを生成する。書式は bellhopcuda 同梱の
// test/in/DickinsB.env と BELLHOP マニュアルの ENVFIL 仕様に従う:
//   TITLE / FREQ / NMEDIA / SSPOPT / 底深度 + SSP 点列 / 底面半無限層 /
//   音源深度 / 受波器深度 / 受波器距離 / RunType / ビーム角 / STEP-ZBOX-RBOX
// 実行は Runner (Kernel::Bellhop) が `bellhopcxx <basename>` で行い、
// 結果は <basename>.prt (ログ) と <basename>.shd (TL 音場) に出る。
//
// 海底地形 (.bty): UnderwaterOpts::bathymetry が非空のとき、底面オプションを
// 'A~' にして BTYFIL を併せて書き出す。'~' (または '*') が
// bellhopcuda src/module/boundary.hpp の IsFile() 判定で、これが無いと
// .bty が置いてあっても **黙って読まれず平坦海底になる**。
#pragma once
#include <QString>

namespace ofd {

class Project;
struct UnderwaterOpts;

class BellhopIO {
public:
    // .env テキストを生成する。設定が不足していても実行可能な既定値で
    // 埋める (SSP が 2 点未満なら等速 1500 m/s の 2 点プロファイル)。
    static QString envText(const Project &p);

    // .bty (BTYFIL) テキスト。地形断面が空なら空文字列を返す
    // (呼び出し側はその場合ファイルを書かない)。
    static QString btyText(const Project &p);

    // 実行ケース名 (FILEROOT)。.env / .bty / .prt / .shd の共通ベース名。
    static QString caseName(const Project &p);

    // ── 設定 → .env の値 (タブの表示と selftest が共有する純関数) ─────────
    // 海面の RMS 粗さ σ [m] = SSP 行の SIGMA。レイリー海面で σ = Hs/4。
    // 「鏡面」を選んでいる (既定) か「Bragg 散乱」が外れていれば 0。
    static double surfaceSigma(const UnderwaterOpts &u);
    // 射出角の扇 [deg]。指向性を選ぶと ±ビーム幅/2 と交差させる
    // (ray モデルでの指向性。**ビーム内の重み付けは一様**)。
    static void beamAngles(const UnderwaterOpts &u, double *a1, double *a2);
    // SSPOPT 文字列。体積吸収 (Thorp) を選ぶと 4 文字目 'T' が付く。
    static QString sspOption(const UnderwaterOpts &u);
};

} // namespace ofd
