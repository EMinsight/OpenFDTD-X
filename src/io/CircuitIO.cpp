// CircuitIO.cpp
#include "CircuitIO.h"
#include "../core/Project.h"

#include <QFileInfo>
#include <QTextStream>
#include <algorithm>
#include <array>
#include <cmath>

using namespace ofd;

namespace {

QString num(double v) { return QString::number(v, 'g', 10); }

// 節点表: 座標で重複を潰して 1 始まりの ID を振る。
// PEEC は導体端点と port の節点を **座標一致** で繋ぐので、丸め差で
// 別節点になると回路が開いてしまう。1 μm 単位に量子化して照合する。
class NodeTable {
public:
    int idFor(double x, double y, double z)
    {
        const auto key = quantize(x, y, z);
        for (int i = 0; i < m_key.size(); ++i)
            if (m_key[i] == key) return i + 1;
        m_key.push_back(key);
        m_pos.push_back({ x, y, z });
        return int(m_key.size());
    }
    QString text() const
    {
        QString s;
        QTextStream out(&s);
        for (int i = 0; i < m_pos.size(); ++i)
            out << "node = " << (i + 1) << " " << num(m_pos[i][0]) << " "
                << num(m_pos[i][1]) << " " << num(m_pos[i][2]) << "\n";
        return s;
    }
    int count() const { return int(m_key.size()); }

private:
    using Key = std::array<long long, 3>;
    static Key quantize(double x, double y, double z)
    {
        const double q = 1e6;   // 1 μm
        return { llround(x * q), llround(y * q), llround(z * q) };
    }
    QVector<Key> m_key;
    QVector<std::array<double, 3>> m_pos;
};

// 材料が導体か (PEC = id 1、または導電率 σ > 0)
bool isConductor(const Project &p, int materialId)
{
    if (materialId == 1) return true;   // PEC
    const int idx = materialId - 2;     // material テーブルは id 2 から
    if (idx < 0 || idx >= p.materials().size()) return false;
    return p.materials()[idx].esgm > 0.0;
}

// 直方体 (shape 1) の 6 パラメータ = x1 x2 y1 y2 z1 z2
struct Box { double lo[3], hi[3]; };

bool boxOf(const Geometry &g, Box *b)
{
    if (g.shape != 1) return false;
    for (int a = 0; a < 3; ++a) {
        b->lo[a] = std::min(g.g[2 * a], g.g[2 * a + 1]);
        b->hi[a] = std::max(g.g[2 * a], g.g[2 * a + 1]);
    }
    return true;
}

} // namespace

QString CircuitIO::caseName(const Project &p)
{
    const QString path = p.filePath();
    return path.isEmpty() ? QStringLiteral("circuit")
                          : QFileInfo(path).completeBaseName();
}

CircuitInput CircuitIO::peecText(const Project &p)
{
    CircuitInput r;
    const CircuitOpts &c = p.circuit();

    // ── 導体: 直方体の形状を、最も長い軸に沿った bar (矩形断面) へ写す ──
    struct Bar { double a[3], b[3], w, t, sigma; int ndiv; };
    QVector<Bar> bars;
    NodeTable nodes;
    int skippedShape = 0, skippedMaterial = 0;

    for (const Geometry &g : p.geometries()) {
        if (!isConductor(p, g.materialId)) { ++skippedMaterial; continue; }
        Box box;
        if (!boxOf(g, &box)) { ++skippedShape; continue; }
        double len[3];
        for (int a = 0; a < 3; ++a) len[a] = box.hi[a] - box.lo[a];
        const int axis = int(std::max_element(len, len + 3) - len);
        if (!(len[axis] > 0.0)) { ++skippedShape; continue; }
        // 断面は残り 2 軸。厚み 0 の板は PEEC が弾くので下限を入れる
        const int u = (axis + 1) % 3, v = (axis + 2) % 3;
        const double w = std::max(len[u], 1e-9);
        const double t = std::max(len[v], 1e-9);

        Bar bar;
        for (int a = 0; a < 3; ++a) {
            const double mid = 0.5 * (box.lo[a] + box.hi[a]);
            bar.a[a] = (a == axis) ? box.lo[a] : mid;
            bar.b[a] = (a == axis) ? box.hi[a] : mid;
        }
        bar.w = w;
        bar.t = t;
        // PEC は有限導電率にしないと表皮効果・抵抗が計算できないので、
        // 設定の導電率 (既定 銅 5.8e7 S/m) を充てる。この置き換えは
        // 利用者へ出す (黙って PEC を銅にしない)。
        bar.sigma = c.peecSigma_Spm;
        // 分割数: 設定のメッシュ幅 [mm] で割る (最低 1)
        const double mesh_m = std::max(1e-6, c.peecMesh_mm * 1e-3);
        bar.ndiv = std::max(1, int(std::ceil(len[axis] / mesh_m)));
        bars.push_back(bar);
        nodes.idFor(bar.a[0], bar.a[1], bar.a[2]);
        nodes.idFor(bar.b[0], bar.b[1], bar.b[2]);
        if (g.materialId == 1)
            r.warnings << QStringLiteral(
                "%1: PEC の形状に導電率 %2 S/m を充てました "
                "(PEEC は有限導電率を要求します)")
                .arg(g.name.isEmpty() ? QStringLiteral("(無名)") : g.name)
                .arg(num(c.peecSigma_Spm));
    }
    if (skippedShape > 0)
        r.warnings << QStringLiteral(
            "直方体でない形状 %1 個を除外しました (PEEC の bar へ写せません)")
            .arg(skippedShape);
    if (skippedMaterial > 0)
        r.warnings << QStringLiteral(
            "導体でない形状 %1 個を除外しました (材料が PEC でも σ>0 でもない)")
            .arg(skippedMaterial);

    // ── ポート ──
    struct Port { int n1, n2; double z0; QString name; };
    QVector<Port> ports;
    for (const CircuitPortRow &row : p.circuitPorts()) {
        if (!row.enabled) continue;
        if (row.kind != CircuitPortRow::Lumped) continue;   // Probe は励振しない
        if (!row.hasEndpoints()) {
            r.warnings << QStringLiteral(
                "ポート「%1」は端点が未設定なので除外しました")
                .arg(row.name.isEmpty() ? QStringLiteral("(無名)") : row.name);
            continue;
        }
        Port pt;
        pt.n1 = nodes.idFor(row.x1_m, row.y1_m, row.z1_m);
        pt.n2 = nodes.idFor(row.x2_m, row.y2_m, row.z2_m);
        pt.z0 = row.z0_ohm;
        pt.name = row.name;
        ports.push_back(pt);
    }

    if (bars.isEmpty()) {
        r.reason = QStringLiteral(
            "導体形状がありません。ジオメトリタブで直方体を置き、材料を "
            "PEC か導電率 σ>0 のものにしてください。");
        return r;
    }
    if (ports.isEmpty()) {
        r.reason = QStringLiteral(
            "有効なポートがありません。回路ソルバタブのポート表で、"
            "端子 A / B の座標を入れてください (両端が同じ行は無効です)。");
        return r;
    }

    // ── 出力 ──
    QString text;
    QTextStream out(&text);
    out << "OpenPEEC 1 0\n";
    out << "title = " << (p.general().title.isEmpty()
                              ? QStringLiteral("OpenFDTD-X circuit extraction")
                              : p.general().title) << "\n";
    // 任意機能 (キー省略時はカーネル既定 = 無効なので、有効なときだけ書く)
    if (c.peecCapacitance) out << "capacitance = 1\n";
    if (c.peecSkinEffect)  out << "skineffect = 1\n";
    if (c.peecRetardation) out << "retardation = 1\n";
    out << nodes.text();
    for (const Bar &b : bars)
        out << "bar = " << num(b.a[0]) << " " << num(b.a[1]) << " " << num(b.a[2])
            << " " << num(b.b[0]) << " " << num(b.b[1]) << " " << num(b.b[2])
            << " " << num(b.w) << " " << num(b.t) << " " << num(b.sigma)
            << " " << b.ndiv << "\n";
    for (const Port &pt : ports)
        out << "port = " << pt.n1 << " " << pt.n2 << " " << num(pt.z0) << "\n";
    // frequency = f0 f1 ndiv (ndiv = 0 で単一周波数)
    const double f0 = std::max(1e-30, c.fmin_Hz);
    const double f1 = std::max(f0, c.fmax_Hz);
    out << "frequency = " << num(f0) << " " << num(f1) << " "
        << std::max(0, c.fdiv) << "\n";
    out << "end\n";

    r.text = text;
    r.conductors = int(bars.size());
    r.ports = int(ports.size());
    return r;
}

CircuitInput CircuitIO::femText(const Project &p)
{
    CircuitInput r;
    r.reason = QStringLiteral("OpenFEM (.ofe) の入力生成は未実装です");
    Q_UNUSED(p);
    return r;
}
