// MeshDiagnostics.cpp — 取込メッシュ (STL) の位相・幾何検査
#include "MeshDiagnostics.h"

#include <QHash>
#include <cmath>

namespace ofd {
namespace {

// 頂点溶接用の格子キー (座標を許容差で量子化したもの)
struct VKey {
    qint64 x = 0, y = 0, z = 0;
    bool operator==(const VKey &o) const {
        return x == o.x && y == o.y && z == o.z;
    }
};

// QHash 用 (ADL で見つかるよう VKey と同じスコープに置く)
size_t qHash(const VKey &k, size_t seed = 0) noexcept
{
    return ::qHashMulti(seed, k.x, k.y, k.z);
}

// 辺の使用状況。undirected キー (小さい方の頂点番号→大きい方) にまとめ、
// 有向にたどられた回数を数えて法線の向きの不一致を検出する。
struct EdgeUse {
    int total = 0;      // この辺を使う三角形の枚数
    int forward = 0;    // (lo → hi) の向きにたどられた回数
};

// 三角形の面積 (外積の半分)
double triArea(const float *p)
{
    const double ux = double(p[3]) - p[0], uy = double(p[4]) - p[1],
                 uz = double(p[5]) - p[2];
    const double vx = double(p[6]) - p[0], vy = double(p[7]) - p[1],
                 vz = double(p[8]) - p[2];
    const double cx = uy * vz - uz * vy;
    const double cy = uz * vx - ux * vz;
    const double cz = ux * vy - uy * vx;
    return 0.5 * std::sqrt(cx * cx + cy * cy + cz * cz);
}

} // namespace

MeshDiagnostics analyzeMesh(const ImportedMesh &mesh, int maxTriangles)
{
    MeshDiagnostics d;
    const int n = qMin(mesh.numTriangles, int(mesh.vertices.size() / 9));
    if (n <= 0) return d;              // 未取込 / 空メッシュ → valid = false
    if (maxTriangles > 0 && n > maxTriangles) {
        d.skippedTooLarge = true;      // 大きすぎるので検査しない (偽の OK を出さない)
        d.triangles = n;
        return d;
    }

    d.valid = true;
    d.triangles = n;
    d.rawVertices = n * 3;

    // 溶接許容差: bbox 対角の 1e-6 (退化 bbox では 1e-12)。STL は頂点を共有
    // せず 3 頂点ずつ書き出す形式なので、位相を見るには溶接が必須。
    const double dx = mesh.bbox[3] - mesh.bbox[0];
    const double dy = mesh.bbox[4] - mesh.bbox[1];
    const double dz = mesh.bbox[5] - mesh.bbox[2];
    const double diag = std::sqrt(dx * dx + dy * dy + dz * dz);
    const double tol = diag > 0.0 ? diag * 1e-6 : 1e-12;
    d.weldTolerance = tol;

    // ── 1) 頂点溶接 (許容差セルへ量子化 + 近傍 27 セル探索) ──────────────
    // 丸めだけではセル境界をまたぐ近接点を取りこぼすため、近傍セルも見る。
    QHash<VKey, int> vmap;
    vmap.reserve(d.rawVertices);
    QVector<int> index(d.rawVertices, -1);
    auto quant = [tol](double v) { return qint64(std::llround(v / tol)); };

    for (int i = 0; i < d.rawVertices; ++i) {
        const float *p = mesh.vertices.constData() + i * 3;
        const VKey key{ quant(p[0]), quant(p[1]), quant(p[2]) };
        int found = -1;
        // まず同一セル (大半はここで当たる)、外れたときだけ近傍 26 セル
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
            found = d.uniqueVertices++;
            vmap.insert(key, found);
        }
        index[i] = found;
    }
    d.duplicateVertices = d.rawVertices - d.uniqueVertices;

    // ── 2) 辺の位相 (縮退三角形は除外して数える) ─────────────────────────
    QHash<qint64, EdgeUse> edges;
    edges.reserve(n * 3);
    const double areaEps = tol * tol;
    for (int t = 0; t < n; ++t) {
        const int v[3] = { index[t * 3], index[t * 3 + 1], index[t * 3 + 2] };
        const float *p = mesh.vertices.constData() + t * 9;
        if (v[0] == v[1] || v[1] == v[2] || v[2] == v[0]
            || triArea(p) <= areaEps) {
            ++d.degenerateTriangles;
            continue;      // 位相を汚さないよう辺は数えない
        }
        for (int e = 0; e < 3; ++e) {
            const int a = v[e], b = v[(e + 1) % 3];
            const int lo = qMin(a, b), hi = qMax(a, b);
            EdgeUse &u = edges[(qint64(lo) << 32) | qint64(hi)];
            ++u.total;
            if (a == lo) ++u.forward;
        }
    }

    // ── 2b) 符号付き体積 (発散定理) ──────────────────────────────────────
    // V = Σ (v0 · (v1 × v2)) / 6。縮退三角形は寄与 0 なので除外は不要。
    // 大きな座標での桁落ちを避けるため bbox 中心を原点にとる (平行移動で
    // 体積は変わらない)。閉じていないメッシュでは意味を持たない値になるが、
    // それは watertight() を見る側の責任 (ヘッダに明記)。
    {
        const double cx = 0.5 * (mesh.bbox[0] + mesh.bbox[3]);
        const double cy = 0.5 * (mesh.bbox[1] + mesh.bbox[4]);
        const double cz = 0.5 * (mesh.bbox[2] + mesh.bbox[5]);
        double vol6 = 0.0;
        for (int t = 0; t < n; ++t) {
            const float *p = mesh.vertices.constData() + t * 9;
            const double ax = p[0] - cx, ay = p[1] - cy, az = p[2] - cz;
            const double bx = p[3] - cx, by = p[4] - cy, bz = p[5] - cz;
            const double gx = p[6] - cx, gy = p[7] - cy, gz = p[8] - cz;
            vol6 += ax * (by * gz - bz * gy)
                  + ay * (bz * gx - bx * gz)
                  + az * (bx * gy - by * gx);
        }
        d.signedVolume = vol6 / 6.0;
    }

    for (auto it = edges.constBegin(); it != edges.constEnd(); ++it) {
        const EdgeUse &u = it.value();
        if (u.total == 1) {
            ++d.boundaryEdges;
        } else if (u.total >= 3) {
            ++d.nonManifoldEdges;
        } else if (u.forward != 1) {
            // 2 枚が共有しているのに同じ向き (forward = 0 or 2) →
            // 片方の面が裏返っている
            ++d.inconsistentEdges;
        }
    }
    return d;
}

} // namespace ofd
