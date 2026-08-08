// MeshRepair.cpp — 取込メッシュ (STL) の修復
#include "MeshRepair.h"

#include <QHash>
#include <QVector>

#include <cmath>
#include <vector>

namespace ofd {
namespace {

// 頂点溶接用の格子キー (MeshDiagnostics と同じ量子化)
struct VKey {
    qint64 x = 0, y = 0, z = 0;
    bool operator==(const VKey &o) const {
        return x == o.x && y == o.y && z == o.z;
    }
};

size_t qHash(const VKey &k, size_t seed = 0) noexcept
{
    return ::qHashMulti(seed, k.x, k.y, k.z);
}

double triArea(const double *a, const double *b, const double *c)
{
    const double ux = b[0] - a[0], uy = b[1] - a[1], uz = b[2] - a[2];
    const double vx = c[0] - a[0], vy = c[1] - a[1], vz = c[2] - a[2];
    const double cx = uy * vz - uz * vy;
    const double cy = uz * vx - ux * vz;
    const double cz = ux * vy - uy * vx;
    return 0.5 * std::sqrt(cx * cx + cy * cy + cz * cz);
}

// 原点を基準にした四面体の符号付き体積 × 6 (閉じた面の総和が正 = 外向き)
double signedVolume6(const double *a, const double *b, const double *c)
{
    return a[0] * (b[1] * c[2] - b[2] * c[1])
         - a[1] * (b[0] * c[2] - b[2] * c[0])
         + a[2] * (b[0] * c[1] - b[1] * c[0]);
}

} // namespace

bool repairMesh(const ImportedMesh &mesh, const RepairOptions &opt,
                ImportedMesh &out, RepairReport &report, int maxTriangles)
{
    report = RepairReport();
    const int n = qMin(mesh.numTriangles, int(mesh.vertices.size() / 9));
    if (n <= 0) return false;
    if (maxTriangles > 0 && n > maxTriangles) {
        report.skippedTooLarge = true;
        return false;                       // 中途半端に触らない
    }

    report.before = analyzeMesh(mesh, maxTriangles);

    // 溶接許容差 (MeshDiagnostics と同じ既定)
    const double dx = mesh.bbox[3] - mesh.bbox[0];
    const double dy = mesh.bbox[4] - mesh.bbox[1];
    const double dz = mesh.bbox[5] - mesh.bbox[2];
    const double diag = std::sqrt(dx * dx + dy * dy + dz * dz);
    double tol = (opt.weldTolerance > 0.0) ? opt.weldTolerance
                                           : (diag > 0.0 ? diag * 1e-6 : 1e-12);

    // ── ① 頂点溶接 ────────────────────────────────────────────────────────
    // 位相を作るのに必須なので常に行う。代表点の座標をそのまま出力に使う
    // (これが「溶接した」ということ)。
    const int rawVerts = n * 3;
    QHash<VKey, int> vmap;
    vmap.reserve(rawVerts);
    std::vector<int> index(std::size_t(rawVerts), -1);
    std::vector<double> pos;                // 代表点の座標 (溶接後)
    pos.reserve(std::size_t(rawVerts) * 3);
    auto quant = [tol](double v) { return qint64(std::llround(v / tol)); };

    for (int i = 0; i < rawVerts; ++i) {
        const float *p = mesh.vertices.constData() + i * 3;
        const VKey key{ quant(p[0]), quant(p[1]), quant(p[2]) };
        int found = -1;
        const auto hit = vmap.constFind(key);
        if (hit != vmap.constEnd()) found = hit.value();
        for (int a = -1; a <= 1 && found < 0; ++a)
            for (int b = -1; b <= 1 && found < 0; ++b)
                for (int c = -1; c <= 1 && found < 0; ++c) {
                    const auto it =
                        vmap.constFind(VKey{ key.x + a, key.y + b, key.z + c });
                    if (it != vmap.constEnd()) found = it.value();
                }
        if (found < 0) {
            found = int(pos.size() / 3);
            vmap.insert(key, found);
            pos.push_back(p[0]);
            pos.push_back(p[1]);
            pos.push_back(p[2]);
        }
        index[std::size_t(i)] = found;
    }
    report.weldedVertices = rawVerts - int(pos.size() / 3);

    // 三角形 → 溶接後の頂点番号
    std::vector<int> tri;                   // 3 × 残す三角形数
    tri.reserve(std::size_t(n) * 3);
    const double areaEps = tol * tol;
    for (int t = 0; t < n; ++t) {
        const int v[3] = { index[std::size_t(t) * 3],
                           index[std::size_t(t) * 3 + 1],
                           index[std::size_t(t) * 3 + 2] };
        // ── ② 縮退三角形の除去 ──
        const bool degenerate =
            (v[0] == v[1] || v[1] == v[2] || v[2] == v[0])
            || triArea(&pos[std::size_t(v[0]) * 3], &pos[std::size_t(v[1]) * 3],
                       &pos[std::size_t(v[2]) * 3]) <= areaEps;
        if (degenerate) {
            if (opt.dropDegenerate) { ++report.removedTriangles; continue; }
        }
        tri.push_back(v[0]);
        tri.push_back(v[1]);
        tri.push_back(v[2]);
    }
    const int m = int(tri.size() / 3);
    if (m <= 0) return false;               // 全部消えるなら修復しない

    // ── ③ 法線の統一 ──────────────────────────────────────────────────────
    std::vector<bool> flip(std::size_t(m), false);
    if (opt.unifyNormals) {
        // 無向辺 → その辺を使う面 (最大 2 枚まで記録。3 枚以上 = 非多様体は
        // 一意に辿れないので、その辺では伝播しない)
        struct EdgeFaces { int f[2] = { -1, -1 }; int count = 0; };
        QHash<qint64, EdgeFaces> edges;
        edges.reserve(m * 3);
        for (int t = 0; t < m; ++t)
            for (int e = 0; e < 3; ++e) {
                const int a = tri[std::size_t(t) * 3 + e];
                const int b = tri[std::size_t(t) * 3 + (e + 1) % 3];
                const int lo = qMin(a, b), hi = qMax(a, b);
                EdgeFaces &ef = edges[(qint64(lo) << 32) | qint64(hi)];
                if (ef.count < 2) ef.f[ef.count] = t;
                ++ef.count;
            }

        // 連結成分ごとに幅優先で向きを伝播する。
        // 辺 (a,b) を共有する 2 枚は、正しく揃っていれば逆向きに辿る。
        std::vector<int> comp(std::size_t(m), -1);
        std::vector<int> stack;
        int ncomp = 0;
        for (int seed = 0; seed < m; ++seed) {
            if (comp[std::size_t(seed)] >= 0) continue;
            const int cid = ncomp++;
            comp[std::size_t(seed)] = cid;
            stack.clear();
            stack.push_back(seed);
            std::vector<int> members;
            while (!stack.empty()) {
                const int t = stack.back();
                stack.pop_back();
                members.push_back(t);
                for (int e = 0; e < 3; ++e) {
                    int a = tri[std::size_t(t) * 3 + e];
                    int b = tri[std::size_t(t) * 3 + (e + 1) % 3];
                    if (flip[std::size_t(t)]) std::swap(a, b);
                    const int lo = qMin(a, b), hi = qMax(a, b);
                    const auto it =
                        edges.constFind((qint64(lo) << 32) | qint64(hi));
                    if (it == edges.constEnd() || it->count != 2) continue;
                    const int u = (it->f[0] == t) ? it->f[1] : it->f[0];
                    if (u < 0 || u == t) continue;
                    if (comp[std::size_t(u)] >= 0) continue;
                    // u が同じ辺を (a→b) の向きに辿るなら裏返す
                    bool sameDir = false;
                    for (int k = 0; k < 3; ++k) {
                        int ua = tri[std::size_t(u) * 3 + k];
                        int ub = tri[std::size_t(u) * 3 + (k + 1) % 3];
                        if (flip[std::size_t(u)]) std::swap(ua, ub);
                        if (ua == a && ub == b) { sameDir = true; break; }
                    }
                    if (sameDir) flip[std::size_t(u)] = true;
                    comp[std::size_t(u)] = cid;
                    stack.push_back(u);
                }
            }

            // 成分が閉じていれば符号付き体積で外向きへ揃える。
            // 開いた成分 (境界がある) は基準が無いので触らない。
            double vol6 = 0.0;
            bool closed = true;
            for (int t : members) {
                int v0 = tri[std::size_t(t) * 3];
                int v1 = tri[std::size_t(t) * 3 + 1];
                int v2 = tri[std::size_t(t) * 3 + 2];
                if (flip[std::size_t(t)]) std::swap(v1, v2);
                vol6 += signedVolume6(&pos[std::size_t(v0) * 3],
                                      &pos[std::size_t(v1) * 3],
                                      &pos[std::size_t(v2) * 3]);
                for (int e = 0; e < 3 && closed; ++e) {
                    const int a = tri[std::size_t(t) * 3 + e];
                    const int b = tri[std::size_t(t) * 3 + (e + 1) % 3];
                    const int lo = qMin(a, b), hi = qMax(a, b);
                    const auto it =
                        edges.constFind((qint64(lo) << 32) | qint64(hi));
                    if (it == edges.constEnd() || it->count != 2) closed = false;
                }
            }
            if (closed && vol6 < 0.0) {
                for (int t : members) flip[std::size_t(t)] = !flip[std::size_t(t)];
                ++report.componentsFlipped;
            }
        }
        for (int t = 0; t < m; ++t)
            if (flip[std::size_t(t)]) ++report.flippedTriangles;
    }

    // ── 出力メッシュの組み立て ────────────────────────────────────────────
    out = ImportedMesh();
    out.name = mesh.name;
    out.sourcePath = mesh.sourcePath;
    out.numTriangles = m;
    out.vertices.resize(m * 9);
    double bb[6] = { 0, 0, 0, 0, 0, 0 };
    double area = 0.0;
    for (int t = 0; t < m; ++t) {
        int v[3] = { tri[std::size_t(t) * 3], tri[std::size_t(t) * 3 + 1],
                     tri[std::size_t(t) * 3 + 2] };
        if (flip[std::size_t(t)]) std::swap(v[1], v[2]);
        for (int k = 0; k < 3; ++k) {
            const double *p = &pos[std::size_t(v[k]) * 3];
            float *dst = out.vertices.data() + (t * 9 + k * 3);
            dst[0] = float(p[0]);
            dst[1] = float(p[1]);
            dst[2] = float(p[2]);
            if (t == 0 && k == 0) {
                bb[0] = bb[3] = p[0];
                bb[1] = bb[4] = p[1];
                bb[2] = bb[5] = p[2];
            } else {
                for (int a = 0; a < 3; ++a) {
                    if (p[a] < bb[a]) bb[a] = p[a];
                    if (p[a] > bb[a + 3]) bb[a + 3] = p[a];
                }
            }
        }
        area += triArea(&pos[std::size_t(v[0]) * 3], &pos[std::size_t(v[1]) * 3],
                        &pos[std::size_t(v[2]) * 3]);
    }
    for (int i = 0; i < 6; ++i) out.bbox[i] = bb[i];
    out.surfaceArea = area;

    report.after = analyzeMesh(out, maxTriangles);
    report.boundaryEdgesLeft = report.after.boundaryEdges;
    report.valid = true;
    return true;
}

} // namespace ofd
