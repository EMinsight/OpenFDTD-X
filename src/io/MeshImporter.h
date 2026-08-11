// MeshImporter.h — 3D メッシュの取込 (形状タブの「3Dモデル取込」の実体)
//
// 旧名 StlImporter。STL しか読めなかったところへ OBJ / PLY を足したので
// 名前を実態に合わせた (`StlImporter` は互換のため型別名として残してある)。
//
// 対応形式 (いずれも外部ライブラリ無しの自前パーサ — 絶対規則 4/5):
//   - STL  : バイナリ / ASCII (面の集合。頂点は共有されない)
//   - OBJ  : `v` / `f` のみ解釈。多角形は扇状に三角形化する。
//            `f a/b/c` `f a//c` の添字形式と負添字 (末尾からの相対) に対応。
//            マテリアル (mtllib/usemtl)・法線・UV は読み飛ばす
//   - PLY  : ascii / binary_little_endian / binary_big_endian。
//            頂点は x,y,z プロパティを名前で拾い、他のプロパティは
//            型のサイズぶん読み飛ばす。面はリストプロパティを扇状に三角形化
//
// **3MF / STEP / IGES は未対応。** 3MF は ZIP (deflate) の展開が要り、
// STEP / IGES は CAD カーネルが要る — どちらも依存を増やすので入れていない。
// 形状タブのバッジがこの対応状況を色分けして示す (対応済み = アクセント色)。
//
// 出力はどの形式でも同じ `ImportedMesh` (三角形ごとに 9 float)。下流の
// Voxelizer / MeshDiagnostics / MeshRepair / MeshAxes は形式を意識しない。
#pragma once
#include <QString>
#include <QStringList>
#include <QVector>

namespace ofd {

struct ImportedMesh {
    QString          name;
    QString          sourcePath;
    QVector<float>   vertices;     // 9 floats per triangle (x1 y1 z1 x2 ...)
    int              numTriangles = 0;
    double           bbox[6] = {0,0,0,0,0,0};   // xmin ymin zmin xmax ymax zmax
    double           surfaceArea = 0.0;

    // ── 部品分け (OBJ の g / o / usemtl) ────────────────────────────────
    // **2 つ以上に分かれたときだけ**埋まる。1 つしか無い (= 分かれていない)
    // ファイルでは両方とも空にする — 「グループがある」ことに意味を持たせ、
    // 単一部品のメッシュを扱う既存の経路を一切変えないため。
    QStringList      groupNames;   // 空 = 部品分けなし
    QVector<int>     triGroup;     // 三角形ごとのグループ添字 (空 = 同上)

    bool hasGroups() const
    {
        return groupNames.size() >= 2 && triGroup.size() == numTriangles;
    }
};

// group 番目のグループだけを取り出した新しいメッシュ (bbox / 面積は再計算)。
// 範囲外や部品分けの無いメッシュには空メッシュ (numTriangles = 0) を返す。
ImportedMesh subMeshOfGroup(const ImportedMesh &mesh, int group);

class MeshImporter {
public:
    // 拡張子で形式を選ぶ (拡張子が無い/未知なら中身から STL を推定する)。
    // 失敗したら false を返し、err に理由を入れる (黙って空メッシュにしない)。
    static bool load(const QString &path, ImportedMesh &mesh, QString *err = nullptr);

    // ファイルダイアログのフィルタ用 (小文字の拡張子、ドット無し)
    static QStringList supportedExtensions();
    // 「読める形式」のフィルタ文字列 (例: "Mesh (*.stl *.obj *.ply)")
    static QString fileDialogFilter();

private:
    static bool loadStl(const QByteArray &data, ImportedMesh &mesh, QString *err);
    static bool loadStlBinary(const QByteArray &data, ImportedMesh &mesh, QString *err);
    static bool loadStlAscii(const QByteArray &data, ImportedMesh &mesh, QString *err);
    static bool loadObj(const QByteArray &data, ImportedMesh &mesh, QString *err);
    static bool loadPly(const QByteArray &data, ImportedMesh &mesh, QString *err);
};

// 旧名 (呼び出し側の互換用)
using StlImporter = MeshImporter;

} // namespace ofd
