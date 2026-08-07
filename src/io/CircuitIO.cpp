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

// ── OpenFEM (.ofe) ─────────────────────────────────────────────────────────
// .ofe は .ofd と同じメッシュ記法を使う準静的 FEM の入力。断面 2 次元の
// 伝送線路解析 (analysis = C L) を主用途にする。
//   xmesh/ymesh/zmesh   = .ofd と同じ (節点と分割数の交互列)
//   material = <epsr> <sigma>
//   geometry = <matid> <shape> <g0..g5>       (誘電体)
//   conductor = <id> <shape> <g0..g5>         (id = 0 が基準導体)
//   analysis / tline / voltage / solver
//
// **導体 id の決め方**: ポート表の端子 B (基準側) を含む導体を 0 番
// (基準導体) とし、残りを 1 番から振る。どの形状にも当たらない場合は
// 最初の導体を基準にして、その旨を warnings に出す (黙って決めない)。
CircuitInput CircuitIO::femText(const Project &p)
{
    CircuitInput r;
    const CircuitOpts &c = p.circuit();

    // 導体 / 誘電体の仕分け
    struct Shape { const Geometry *g; Box box; };
    QVector<Shape> conductors, dielectrics;
    int skippedShape = 0;
    for (const Geometry &g : p.geometries()) {
        Box b;
        if (!boxOf(g, &b)) { ++skippedShape; continue; }
        (isConductor(p, g.materialId) ? conductors : dielectrics)
            .push_back({ &g, b });
    }
    if (skippedShape > 0)
        r.warnings << QStringLiteral(
            "直方体でない形状 %1 個を除外しました (.ofe の直方体指定へ写せません)")
            .arg(skippedShape);
    if (conductors.isEmpty()) {
        r.reason = QStringLiteral(
            "導体形状がありません。ジオメトリタブで直方体を置き、材料を "
            "PEC か導電率 σ>0 のものにしてください。");
        return r;
    }

    // 基準導体 (id 0) を決める — 端子 B を含む導体
    int refIndex = -1;
    for (const CircuitPortRow &row : p.circuitPorts()) {
        if (!row.enabled || !row.hasEndpoints()) continue;
        for (int i = 0; i < conductors.size() && refIndex < 0; ++i) {
            const Box &b = conductors[i].box;
            const double q[3] = { row.x2_m, row.y2_m, row.z2_m };
            bool inside = true;
            for (int a = 0; a < 3; ++a)
                inside = inside && (q[a] >= b.lo[a] - 1e-12)
                                && (q[a] <= b.hi[a] + 1e-12);
            if (inside) refIndex = i;
        }
        if (refIndex >= 0) break;
    }
    if (refIndex < 0) {
        refIndex = 0;
        r.warnings << QStringLiteral(
            "基準導体を特定できなかったので最初の導体を基準 (id 0) にしました。"
            "ポート表の端子 B を基準導体の内側に置くと確定します。");
    }

    QString text;
    QTextStream out(&text);
    out << "OpenFEM 1 0\n";
    out << "title = " << (p.general().title.isEmpty()
                              ? QStringLiteral("OpenFDTD-X circuit extraction")
                              : p.general().title) << "\n";
    // メッシュ (.ofd と同じ書式)
    static const char *meshKey[3] = { "xmesh", "ymesh", "zmesh" };
    for (int a = 0; a < 3; ++a) {
        const MeshAxis &ax = p.mesh(a);
        if (ax.nodes.size() < 2) {
            r.reason = QStringLiteral("メッシュが未設定です (%1)")
                           .arg(QLatin1String(meshKey[a]));
            return CircuitInput{ QString(), r.warnings, r.reason, 0, 0 };
        }
        out << meshKey[a] << " = " << num(ax.nodes[0]);
        for (int i = 0; i < ax.divs.size(); ++i)
            out << " " << ax.divs[i] << " " << num(ax.nodes[i + 1]);
        out << "\n";
    }
    // 材料 (.ofe は epsr と sigma の 2 値)。
    // σ を読むのは analysis R / A / E / F だけで、C・L 解析に σ を渡すと
    // 「読まれないキー」警告が出る。導体は conductor 行で与えるので、
    // σ を読まない解析では 0 にして出す (その旨は warnings に出す)。
    const QString an = c.femAnalysis.toUpper();
    const bool usesSigma = an.contains(QLatin1Char('R')) || an.contains(QLatin1Char('A'))
                        || an.contains(QLatin1Char('E')) || an.contains(QLatin1Char('F'));
    bool zeroed = false;
    for (const Material &m : p.materials()) {
        const double sg = usesSigma ? m.esgm : 0.0;
        if (!usesSigma && m.esgm > 0.0) zeroed = true;
        out << "material = " << num(m.epsr) << " " << num(sg) << "\n";
    }
    if (zeroed)
        r.warnings << QStringLiteral(
            "解析 %1 は導電率を読まないため、材料の σ は 0 として書き出しました "
            "(導体は conductor 行で与えます)。").arg(c.femAnalysis);
    auto emitBox = [&](const char *key, int id, const Box &b) {
        out << key << " = " << id << " 1"
            << " " << num(b.lo[0]) << " " << num(b.hi[0])
            << " " << num(b.lo[1]) << " " << num(b.hi[1])
            << " " << num(b.lo[2]) << " " << num(b.hi[2]) << "\n";
    };
    for (const Shape &s : dielectrics)
        emitBox("geometry", s.g->materialId, s.box);
    int nextId = 1;
    for (int i = 0; i < conductors.size(); ++i)
        emitBox("conductor", (i == refIndex) ? 0 : nextId++, conductors[i].box);

    out << "analysis = " << c.femAnalysis << "\n";
    // 伝送線路解析 (C / L / Z0) は線路軸 tline を要求する。
    // 断面 2 次元モデルなので「分割が 1 の軸」= 線路軸。
    if (an.contains(QLatin1Char('C')) || an.contains(QLatin1Char('L'))) {
        static const char kAxis[3] = { 'X', 'Y', 'Z' };
        int lineAxis = -1;
        for (int a = 0; a < 3; ++a) {
            int total = 0;
            for (const int d : p.mesh(a).divs) total += d;
            if (total == 1) { lineAxis = a; break; }
        }
        if (lineAxis < 0) {
            lineAxis = 2;
            r.warnings << QStringLiteral(
                "線路軸を特定できなかったので Z 軸としました "
                "(断面 2 次元モデルでは分割数 1 の軸が線路軸です)。");
        }
        out << "tline = " << kAxis[lineAxis] << "\n";
    }
    out << "voltage = " << num(c.femVoltage_V) << "\n";
    out << "end\n";

    r.text = text;
    r.conductors = int(conductors.size());
    r.ports = std::max(0, nextId - 1);
    return r;
}
