// BellhopIO.h — 水中音響カーネル bellhopcxx (bellhopcuda) の入力生成。
//
// UnderwaterTab / OceanEnvironmentTab の設定 (UnderwaterOpts) から
// BELLHOP の .env テキストを生成する。書式は bellhopcuda 同梱の
// test/in/DickinsB.env と BELLHOP マニュアルの ENVFIL 仕様に従う:
//   TITLE / FREQ / NMEDIA / SSPOPT / 底深度 + SSP 点列 / 底面半無限層 /
//   音源深度 / 受波器深度 / 受波器距離 / RunType / ビーム角 / STEP-ZBOX-RBOX
// 実行は Runner (Kernel::Bellhop) が `bellhopcxx <basename>` で行い、
// 結果は <basename>.prt (ログ) と <basename>.shd (TL 音場) に出る。
#pragma once
#include <QString>

namespace ofd {

class Project;

class BellhopIO {
public:
    // .env テキストを生成する。設定が不足していても実行可能な既定値で
    // 埋める (SSP が 2 点未満なら等速 1500 m/s の 2 点プロファイル)。
    static QString envText(const Project &p);

    // 実行ケース名 (FILEROOT)。.env / .prt / .shd の共通ベース名。
    static QString caseName(const Project &p);
};

} // namespace ofd
