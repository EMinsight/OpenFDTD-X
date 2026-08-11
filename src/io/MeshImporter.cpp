// MeshImporter.cpp — STL / OBJ / PLY の取込 (仕様は MeshImporter.h)
#include "MeshImporter.h"

#include <QHash>

#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <cmath>
#include <cstring>
#include <limits>

using namespace ofd;

QStringList MeshImporter::supportedExtensions()
{
    return { QStringLiteral("stl"), QStringLiteral("obj"),
             QStringLiteral("ply") };
}

QString MeshImporter::fileDialogFilter()
{
    QStringList globs;
    for (const QString &e : supportedExtensions())
        globs << QStringLiteral("*.") + e;
    return QStringLiteral("Mesh (%1);;All files (*)").arg(globs.join(' '));
}

namespace {

// 三角形 1 枚を積む: 頂点・bbox・面積・枚数を同時に更新する。
// (bbox は「最初の 1 頂点で初期化してから min/max」— 0 始まりにすると
//  原点を含まないメッシュで箱が原点まで伸びる)
void accumulate(ImportedMesh &mesh, const float v[9])
{
    for (int i = 0; i < 9; ++i) mesh.vertices.push_back(v[i]);
    for (int k = 0; k < 3; ++k) {
        for (int c = 0; c < 3; ++c) {
            const double val = v[3*k + c];
            if (mesh.numTriangles == 0 && k == 0) {
                mesh.bbox[c] = mesh.bbox[3 + c] = val;
            } else {
                mesh.bbox[c]     = std::min(mesh.bbox[c], val);
                mesh.bbox[3 + c] = std::max(mesh.bbox[3 + c], val);
            }
        }
    }
    // area += |AB × AC| / 2
    const double ab[3] = { v[3]-v[0], v[4]-v[1], v[5]-v[2] };
    const double ac[3] = { v[6]-v[0], v[7]-v[1], v[8]-v[2] };
    const double cx = ab[1]*ac[2] - ab[2]*ac[1];
    const double cy = ab[2]*ac[0] - ab[0]*ac[2];
    const double cz = ab[0]*ac[1] - ab[1]*ac[0];
    mesh.surfaceArea += 0.5 * std::sqrt(cx*cx + cy*cy + cz*cz);
    ++mesh.numTriangles;
}

// 頂点配列 + 多角形の添字列 → 扇状に三角形化して積む。
// 添字が範囲外の面は **黙って捨てず** 1 枚も積まないで false を返す。
bool addPolygon(ImportedMesh &mesh, const QVector<float> &xyz,
                const QVector<int> &idx)
{
    if (idx.size() < 3) return true;                 // 点・線は無視 (面ではない)
    const int nv = xyz.size() / 3;
    for (int i : idx)
        if (i < 0 || i >= nv) return false;
    for (int k = 1; k + 1 < idx.size(); ++k) {
        const int a = idx[0], b = idx[k], c = idx[k + 1];
        const float v[9] = {
            xyz[3*a], xyz[3*a+1], xyz[3*a+2],
            xyz[3*b], xyz[3*b+1], xyz[3*b+2],
            xyz[3*c], xyz[3*c+1], xyz[3*c+2],
        };
        accumulate(mesh, v);
    }
    return true;
}

} // namespace

bool MeshImporter::load(const QString &path, ImportedMesh &mesh, QString *err)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (err) *err = f.errorString();
        return false;
    }
    const QByteArray data = f.readAll();
    if (data.isEmpty()) {
        if (err) *err = "empty file";
        return false;
    }

    mesh = ImportedMesh{};
    mesh.sourcePath = path;
    mesh.name = QFileInfo(path).completeBaseName();

    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == QLatin1String("obj")) return loadObj(data, mesh, err);
    if (ext == QLatin1String("ply")) return loadPly(data, mesh, err);
    if (ext == QLatin1String("stl")) return loadStl(data, mesh, err);

    // 拡張子が無い / 未知: 中身で判定する (PLY はマジック、OBJ は "v " 行)
    if (data.startsWith("ply")) return loadPly(data, mesh, err);
    return loadStl(data, mesh, err);
}

// ── STL ────────────────────────────────────────────────────────────────────
bool MeshImporter::loadStl(const QByteArray &data, ImportedMesh &mesh,
                           QString *err)
{
    if (data.size() < 84) {
        if (err) *err = "file too small for STL";
        return false;
    }
    // binary STL: 80-byte header + uint32 count + 50 bytes per triangle.
    // "solid" prefix alone is not reliable — check the size equation.
    quint32 count = 0;
    std::memcpy(&count, data.constData() + 80, 4);
    const bool sizeMatches =
        (qint64(data.size()) == 84 + qint64(count) * 50);

    if (sizeMatches)
        return loadStlBinary(data, mesh, err);
    return loadStlAscii(data, mesh, err);
}

bool MeshImporter::loadStlBinary(const QByteArray &data, ImportedMesh &mesh,
                                 QString *err)
{
    quint32 count = 0;
    std::memcpy(&count, data.constData() + 80, 4);
    const char *p = data.constData() + 84;
    mesh.vertices.reserve(int(count) * 9);
    for (quint32 i = 0; i < count; ++i) {
        float v[12];                       // normal + 3 vertices
        std::memcpy(v, p, 48);
        accumulate(mesh, v + 3);
        p += 50;                           // 48 + 2 attribute bytes
    }
    if (mesh.numTriangles == 0 && err) *err = "no triangles";
    return mesh.numTriangles > 0;
}

bool MeshImporter::loadStlAscii(const QByteArray &data, ImportedMesh &mesh,
                                QString *err)
{
    QTextStream in(data);
    float v[9];
    int idx = 0;
    while (!in.atEnd()) {
        QString word;
        in >> word;
        if (word == "vertex") {
            in >> v[idx] >> v[idx+1] >> v[idx+2];
            idx += 3;
            if (idx == 9) {
                accumulate(mesh, v);
                idx = 0;
            }
        }
    }
    if (mesh.numTriangles == 0) {
        if (err) *err = "no triangles found (not an STL file?)";
        return false;
    }
    return true;
}

// ── OBJ ────────────────────────────────────────────────────────────────────
// `v x y z [w]` と `f …` だけを見る。面の添字は 1 始まりで、
// 負の値は「末尾からの相対」(-1 = 最後に読んだ頂点) — 仕様どおり実装する。
bool MeshImporter::loadObj(const QByteArray &data, ImportedMesh &mesh,
                           QString *err)
{
    QVector<float> xyz;
    QVector<int> idx;
    int badFaces = 0;

    // 部品分け: g / o / usemtl で切り替わる。名前ごとに 1 グループへまとめる
    // (同じ名前が離れて何度も出てくる OBJ があるため)。
    QHash<QString, int> groupOf;
    QString curGroup;                  // "" = 名前が付く前の面
    auto groupIndex = [&](const QString &n) {
        const auto it = groupOf.constFind(n);
        if (it != groupOf.constEnd()) return it.value();
        const int i = mesh.groupNames.size();
        mesh.groupNames << (n.isEmpty() ? QStringLiteral("(default)") : n);
        groupOf.insert(n, i);
        return i;
    };

    const QList<QByteArray> lines = data.split('\n');
    for (const QByteArray &raw : lines) {
        const QByteArray line = raw.trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;
        const QList<QByteArray> tok = line.simplified().split(' ');
        if (tok.isEmpty()) continue;

        if (tok[0] == "g" || tok[0] == "o" || tok[0] == "usemtl") {
            // 名前は残り全部 (空白を含む名前がある)
            QStringList parts;
            for (int i = 1; i < tok.size(); ++i)
                parts << QString::fromUtf8(tok[i]);
            curGroup = parts.join(QLatin1Char(' ')).trimmed();
            continue;
        }

        if (tok[0] == "v") {
            if (tok.size() < 4) continue;
            bool ok1 = false, ok2 = false, ok3 = false;
            const float x = tok[1].toFloat(&ok1);
            const float y = tok[2].toFloat(&ok2);
            const float z = tok[3].toFloat(&ok3);
            if (!(ok1 && ok2 && ok3)) continue;
            xyz << x << y << z;
        } else if (tok[0] == "f") {
            idx.clear();
            for (int i = 1; i < tok.size(); ++i) {
                // "12" / "12/7" / "12/7/3" / "12//3" — 先頭が頂点添字
                const QByteArray first = tok[i].split('/').value(0);
                bool ok = false;
                int v = first.toInt(&ok);
                if (!ok || v == 0) { idx.clear(); break; }
                // 1 始まり / 負は末尾からの相対
                v = (v > 0) ? (v - 1) : (xyz.size() / 3 + v);
                idx << v;
            }
            if (idx.size() >= 3 && !addPolygon(mesh, xyz, idx)) ++badFaces;
            // 追加された三角形 (多角形は扇状に分割されるので複数のことがある)
            // を現在のグループに属させる
            const int gi = groupIndex(curGroup);
            while (mesh.triGroup.size() < mesh.numTriangles)
                mesh.triGroup.push_back(gi);
        }
    }

    // 分かれていない (グループが 1 つ以下) なら「部品分けなし」に戻す。
    // 単一部品の OBJ を今までどおり扱うため (hasGroups() が false になる)。
    if (mesh.groupNames.size() < 2) {
        mesh.groupNames.clear();
        mesh.triGroup.clear();
    }

    if (mesh.numTriangles == 0) {
        if (err) {
            *err = badFaces > 0
                       ? QStringLiteral("OBJ: %1 face(s) reference vertices "
                                        "that do not exist").arg(badFaces)
                       : QStringLiteral("OBJ: no faces found");
        }
        return false;
    }
    // 一部の面だけが壊れている場合も**黙って通さない**
    if (badFaces > 0 && err)
        *err = QStringLiteral("OBJ: skipped %1 face(s) with out-of-range "
                              "vertex indices").arg(badFaces);
    return true;
}

// ── PLY ────────────────────────────────────────────────────────────────────
namespace {

// PLY のスカラ型 → バイト数 (未知は 0)
int plyTypeSize(const QByteArray &t)
{
    if (t == "char"   || t == "int8"    || t == "uchar" || t == "uint8")  return 1;
    if (t == "short"  || t == "int16"   || t == "ushort"|| t == "uint16") return 2;
    if (t == "int"    || t == "int32"   || t == "uint"  || t == "uint32"
        || t == "float" || t == "float32")                                return 4;
    if (t == "double" || t == "float64")                                  return 8;
    return 0;
}

bool plyTypeIsFloat(const QByteArray &t)
{
    return t == "float" || t == "float32" || t == "double" || t == "float64";
}

bool plyTypeIsSigned(const QByteArray &t)
{
    return t == "char" || t == "int8" || t == "short" || t == "int16"
        || t == "int"  || t == "int32";
}

// バイナリのスカラを 1 個読む (成功したら pos を進める)
bool plyReadScalar(const QByteArray &d, int &pos, const QByteArray &type,
                   bool bigEndian, double &out)
{
    const int n = plyTypeSize(type);
    if (n == 0 || pos + n > d.size()) return false;
    unsigned char b[8];
    std::memcpy(b, d.constData() + pos, n);
    if (bigEndian)
        for (int i = 0; i < n / 2; ++i) std::swap(b[i], b[n - 1 - i]);
    pos += n;
    if (plyTypeIsFloat(type)) {
        if (n == 4) { float f; std::memcpy(&f, b, 4); out = f; }
        else        { double g; std::memcpy(&g, b, 8); out = g; }
    } else if (plyTypeIsSigned(type)) {
        qint64 v = 0;
        if (n == 1)      { qint8  t; std::memcpy(&t, b, 1); v = t; }
        else if (n == 2) { qint16 t; std::memcpy(&t, b, 2); v = t; }
        else             { qint32 t; std::memcpy(&t, b, 4); v = t; }
        out = double(v);
    } else {
        quint64 v = 0;
        if (n == 1)      { quint8  t; std::memcpy(&t, b, 1); v = t; }
        else if (n == 2) { quint16 t; std::memcpy(&t, b, 2); v = t; }
        else             { quint32 t; std::memcpy(&t, b, 4); v = t; }
        out = double(v);
    }
    return true;
}

struct PlyProp {
    QByteArray name;
    QByteArray type;        // スカラ型 (リストなら要素の型)
    QByteArray countType;   // リストの個数の型 (非リストなら空)
    bool isList() const { return !countType.isEmpty(); }
};

struct PlyElement {
    QByteArray name;
    qint64 count = 0;
    QVector<PlyProp> props;
};

} // namespace

bool MeshImporter::loadPly(const QByteArray &data, ImportedMesh &mesh,
                           QString *err)
{
    if (!data.startsWith("ply")) {
        if (err) *err = "PLY: missing the 'ply' magic";
        return false;
    }
    // ヘッダは常に ASCII 行。end_header の直後からが本体。
    const int hdrEnd = data.indexOf("end_header");
    if (hdrEnd < 0) {
        if (err) *err = "PLY: no end_header";
        return false;
    }
    int bodyStart = data.indexOf('\n', hdrEnd);
    if (bodyStart < 0) { if (err) *err = "PLY: truncated header"; return false; }
    ++bodyStart;

    enum Fmt { Ascii, LittleEndian, BigEndian } fmt = Ascii;
    bool haveFmt = false;
    QVector<PlyElement> elems;
    for (const QByteArray &raw : data.left(hdrEnd).split('\n')) {
        const QList<QByteArray> t = raw.trimmed().simplified().split(' ');
        if (t.isEmpty()) continue;
        if (t[0] == "format" && t.size() >= 2) {
            if      (t[1] == "ascii")                 { fmt = Ascii; }
            else if (t[1] == "binary_little_endian")   { fmt = LittleEndian; }
            else if (t[1] == "binary_big_endian")      { fmt = BigEndian; }
            else {
                if (err) *err = QStringLiteral("PLY: unsupported format '%1'")
                                    .arg(QString::fromLatin1(t[1]));
                return false;
            }
            haveFmt = true;
        } else if (t[0] == "element" && t.size() >= 3) {
            PlyElement e;
            e.name = t[1];
            e.count = t[2].toLongLong();
            elems << e;
        } else if (t[0] == "property" && !elems.isEmpty()) {
            PlyProp p;
            if (t.size() >= 5 && t[1] == "list") {
                p.countType = t[2];
                p.type = t[3];
                p.name = t[4];
            } else if (t.size() >= 3) {
                p.type = t[1];
                p.name = t[2];
            } else {
                continue;
            }
            if (plyTypeSize(p.type) == 0) {
                if (err) *err = QStringLiteral("PLY: unknown property type '%1'")
                                    .arg(QString::fromLatin1(p.type));
                return false;
            }
            elems.last().props << p;
        }
    }
    if (!haveFmt) { if (err) *err = "PLY: no format line"; return false; }

    QVector<float> xyz;
    QVector<int> idx;
    int badFaces = 0;
    int pos = bodyStart;
    QTextStream ascii(data.mid(bodyStart));

    for (const PlyElement &e : elems) {
        const bool isVertex = (e.name == "vertex");
        const bool isFace   = (e.name == "face");
        // x/y/z がどのプロパティかを名前で決める (並び順に依存しない)
        int ix = -1, iy = -1, iz = -1;
        for (int i = 0; i < e.props.size(); ++i) {
            if (e.props[i].name == "x") ix = i;
            if (e.props[i].name == "y") iy = i;
            if (e.props[i].name == "z") iz = i;
        }
        if (isVertex && (ix < 0 || iy < 0 || iz < 0)) {
            if (err) *err = "PLY: the vertex element has no x/y/z";
            return false;
        }
        if (isVertex) xyz.reserve(int(e.count) * 3);

        for (qint64 n = 0; n < e.count; ++n) {
            QVector<double> scalar(e.props.size(), 0.0);
            idx.clear();
            for (int i = 0; i < e.props.size(); ++i) {
                const PlyProp &p = e.props[i];
                if (fmt == Ascii) {
                    if (p.isList()) {
                        int cnt = 0;
                        ascii >> cnt;
                        for (int k = 0; k < cnt; ++k) {
                            int v = 0;
                            ascii >> v;
                            if (isFace && p.name.contains("indices")) idx << v;
                        }
                    } else {
                        double v = 0;
                        ascii >> v;
                        scalar[i] = v;
                    }
                    if (ascii.status() != QTextStream::Ok) {
                        if (err) *err = "PLY: truncated ascii body";
                        return false;
                    }
                } else {
                    const bool be = (fmt == BigEndian);
                    if (p.isList()) {
                        double cntD = 0;
                        if (!plyReadScalar(data, pos, p.countType, be, cntD)) {
                            if (err) *err = "PLY: truncated list count";
                            return false;
                        }
                        const int cnt = int(cntD);
                        for (int k = 0; k < cnt; ++k) {
                            double v = 0;
                            if (!plyReadScalar(data, pos, p.type, be, v)) {
                                if (err) *err = "PLY: truncated list data";
                                return false;
                            }
                            if (isFace && p.name.contains("indices"))
                                idx << int(v);
                        }
                    } else {
                        double v = 0;
                        if (!plyReadScalar(data, pos, p.type, be, v)) {
                            if (err) *err = "PLY: truncated element data";
                            return false;
                        }
                        scalar[i] = v;
                    }
                }
            }
            if (isVertex)
                xyz << float(scalar[ix]) << float(scalar[iy])
                    << float(scalar[iz]);
            else if (isFace && idx.size() >= 3 && !addPolygon(mesh, xyz, idx))
                ++badFaces;
        }
    }

    if (mesh.numTriangles == 0) {
        if (err) {
            *err = badFaces > 0
                       ? QStringLiteral("PLY: %1 face(s) reference vertices "
                                        "that do not exist").arg(badFaces)
                       : QStringLiteral("PLY: no faces found");
        }
        return false;
    }
    if (badFaces > 0 && err)
        *err = QStringLiteral("PLY: skipped %1 face(s) with out-of-range "
                              "vertex indices").arg(badFaces);
    return true;
}

// ── グループの取り出し ─────────────────────────────────────────────────────
// (accumulate は上の無名 name space のもの — ここからも見える)
ImportedMesh ofd::subMeshOfGroup(const ImportedMesh &mesh, int group)
{
    ImportedMesh out;
    if (!mesh.hasGroups() || group < 0 || group >= mesh.groupNames.size())
        return out;
    out.name = mesh.groupNames[group];
    out.sourcePath = mesh.sourcePath;
    for (int t = 0; t < mesh.numTriangles; ++t) {
        if (mesh.triGroup[t] != group) continue;
        float v[9];
        for (int k = 0; k < 9; ++k) v[k] = mesh.vertices[t * 9 + k];
        accumulate(out, v);      // bbox と面積もここで積む
    }
    return out;
}
