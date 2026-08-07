// CircuitIO.h — 回路パラメータ抽出ソルバ (OpenPEEC / OpenFEM) の入力生成。
//
// CircuitSolversTab の「抽出実行」の入口。Project の導体形状とポート定義から
//
//   OpenPEEC : <ケース名>.peec   (node / wire / bar / plate + port + frequency)
//   OpenFEM  : <ケース名>.ofe    (xmesh/ymesh/zmesh + material + conductor)
//
// を書き出す。実行は Runner (Kernel::PEEC / Kernel::FEM) が
// `peec <file>` / `ofe <file>` で行う。
//
// **導体の抜き出し方**: `Project::geometries()` のうち、材料が PEC (id 1) か
// 導電率 σ > 0 の形状を導体とみなす。shape 1 (直方体) は PEEC の `bar`
// (矩形断面の直線導体) へ写せるので、それだけを対象にする。他の形状は
// 落として **理由を warnings に積む** (黙って無視しない)。
//
// **ポート**: `CircuitPortRow` の端点座標。両端が同じ (未設定) の行と
// 無効行は除外し、その旨を warnings に出す。
#pragma once
#include <QString>
#include <QStringList>

namespace ofd {

class Project;

// 入力生成の結果。text が空 = 生成できなかった (reason に理由)。
struct CircuitInput {
    QString     text;        // 入力ファイルの中身
    QStringList warnings;    // 落とした形状・ポートなどの説明 (利用者向け)
    QString     reason;      // 生成できなかった理由 (text が空のときのみ)
    int         conductors = 0;
    int         ports = 0;

    bool isValid() const { return !text.isEmpty(); }
};

class CircuitIO {
public:
    // OpenPEEC の .peec を生成する
    static CircuitInput peecText(const Project &p);
    // OpenFEM の .ofe を生成する
    static CircuitInput femText(const Project &p);

    // 実行ケース名 (.peec / .ofe / ログの共通ベース名)
    static QString caseName(const Project &p);
};

} // namespace ofd
