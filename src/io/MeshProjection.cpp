// MeshProjection.cpp
#include "MeshProjection.h"

#include <cmath>

namespace ofd {

namespace {
// Qt を include しないので M_PI は使わない (MSVC で未定義 — cpp-qt.md)
const double kPi = 3.14159265358979323846;
} // namespace

void projectPoint(double x, double y, double z, double azDeg, double elDeg,
                  double *u, double *v, double *depth)
{
    const double a = azDeg * kPi / 180.0;
    const double e = elDeg * kPi / 180.0;
    const double ca = std::cos(a), sa = std::sin(a);
    const double ce = std::cos(e), se = std::sin(e);
    if (u)     *u = -x * sa + y * ca;
    if (v)     *v = -x * ca * se - y * sa * se + z * ce;
    if (depth) *depth = x * ca * ce + y * sa * ce + z * se;
}

QVector<ProjectedTri> projectMesh(const ImportedMesh &mesh,
                                  double azDeg, double elDeg)
{
    QVector<ProjectedTri> out;
    if (mesh.numTriangles <= 0) return out;
    out.reserve(mesh.numTriangles);

    const double a = azDeg * kPi / 180.0;
    const double e = elDeg * kPi / 180.0;
    // 視線ベクトル (depth が増える向き)。陰影はこれと面法線の角度で決める
    const double vx = std::cos(a) * std::cos(e);
    const double vy = std::sin(a) * std::cos(e);
    const double vz = std::sin(e);

    const float *V = mesh.vertices.constData();
    for (int t = 0; t < mesh.numTriangles; ++t) {
        const float *p = V + t * 9;
        ProjectedTri pt;
        double dsum = 0.0;
        for (int k = 0; k < 3; ++k) {
            double d = 0.0;
            projectPoint(p[k * 3], p[k * 3 + 1], p[k * 3 + 2], azDeg, elDeg,
                         &pt.u[k], &pt.v[k], &d);
            dsum += d;
        }
        pt.depth = dsum / 3.0;

        // 面法線 (外積) と視線の内積 → 陰影。裏返っていても暗くならないよう
        // 絶対値を取る (法線の向きが揃っていないメッシュでも見える)
        const double ax = p[3] - p[0], ay = p[4] - p[1], az2 = p[5] - p[2];
        const double bx = p[6] - p[0], by = p[7] - p[1], bz = p[8] - p[2];
        double nx = ay * bz - az2 * by;
        double ny = az2 * bx - ax * bz;
        double nz = ax * by - ay * bx;
        const double nl = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (nl > 0.0) { nx /= nl; ny /= nl; nz /= nl; }
        const double dot = std::fabs(nx * vx + ny * vy + nz * vz);
        // 真っ黒にしない (輪郭だけの面が消えないよう下駄を履かせる)
        pt.shade = 0.25 + 0.75 * (dot > 1.0 ? 1.0 : dot);
        out.push_back(pt);
    }
    return out;
}

} // namespace ofd
