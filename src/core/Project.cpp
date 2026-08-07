// Project.cpp
#include "Project.h"
#include "../io/OfdIO.h"

#include <QFileInfo>
#include <cmath>

using namespace ofd;

// ── 光ドメイン: TPA / パワースイープ入力の妥当性 ─────────────────────────────
bool ofd::isValidTpaBeta(double beta_cmGW)
{
    return beta_cmGW > 0.0 && std::isfinite(beta_cmGW);
}

bool ofd::isValidPowerSweepRange(double pmin_W, double pmax_W)
{
    return pmin_W > 0.0 && pmax_W >= pmin_W &&
           std::isfinite(pmin_W) && std::isfinite(pmax_W);
}

// ── 光ドメイン: RCWA 層スタックの妥当性 ─────────────────────────────────────
bool ofd::isValidRcwaLayer(const RcwaLayer &layer)
{
    return std::isfinite(layer.eps1) && layer.eps1 > 0.0 &&
           std::isfinite(layer.eps2) && layer.eps2 > 0.0 &&
           std::isfinite(layer.fill) && layer.fill >= 0.0 && layer.fill <= 1.0 &&
           std::isfinite(layer.thickness_nm) && layer.thickness_nm >= 0.0;
}

bool ofd::isValidRcwaStack(const QVector<RcwaLayer> &layers)
{
    if (layers.isEmpty()) return false;
    for (const RcwaLayer &l : layers)
        if (!isValidRcwaLayer(l)) return false;
    return true;
}

// ── 室内音響: 騒音源内訳の既定 4 行 (mock room-acoustics.jsx:697-709) ────────
// 後ろ 2 行 (外部交通騒音 / 照明トランス) は既定でチェック外し。
QVector<NoiseSourceRow> ofd::defaultNoiseSources()
{
    auto row = [](bool on, const char *name, double dBA, const char *measure) {
        NoiseSourceRow r;
        r.enabled = on;
        r.name = QString::fromUtf8(name);
        r.level_dBA = dBA;
        r.measure = QString::fromUtf8(measure);
        return r;
    };
    return {
        row(true,  "空調吹出口",   28, "消音器追加"),
        row(true,  "ダクト気流音", 24, "風速低減"),
        row(false, "外部交通騒音", 19, "外皮遮音"),
        row(false, "照明トランス", 15, "—"),
    };
}

// ── 回路系電磁解析: ポート定義の既定 3 行 ───────────────────────────────────
// 降圧コンバータ基板を想定した初期値 (抽出結果ではない)。利用者は
// CircuitSolversTab の表で編集・追加・削除でき、変更すると .ofdx に保存される。
QVector<CircuitPortRow> ofd::defaultCircuitPorts()
{
    auto row = [](const char *name, int kind, const char *net, const char *ref) {
        CircuitPortRow r;
        r.name = QString::fromUtf8(name);
        r.kind = kind;
        r.net = QString::fromUtf8(net);
        r.ref = QString::fromUtf8(ref);
        return r;
    };
    return {
        row("VIN",     CircuitPortRow::Lumped, "NET_VBUS", "GND"),
        row("VOUT",    CircuitPortRow::Lumped, "NET_VOUT", "GND"),
        row("SW_NODE", CircuitPortRow::Probe,  "NET_SW",   "GND"),
    };
}

// ── フォトニック回路: ネットリストの既定 5 行 ───────────────────────────────
// 2 入力 MZI + リング共振器 + 2 光検出器の初期構成 (計算結果ではない)。
QVector<PhotonicNetRow> ofd::defaultPhotonicNetlist()
{
    auto row = [](const char *from, const char *to, const char *wl) {
        PhotonicNetRow r;
        r.from = QString::fromUtf8(from);
        r.to = QString::fromUtf8(to);
        r.wavelength = QString::fromUtf8(wl);
        return r;
    };
    return {
        row("LASER1.out", "MZI1.in1", "1530~1570nm"),
        row("LASER2.out", "MZI1.in2", "1530~1570nm"),
        row("MZI1.out1",  "RING1.in", "—"),
        row("RING1.drop", "PD1.in",   "—"),
        row("RING1.thru", "PD2.in",   "—"),
    };
}

Project::Project(QObject *parent) : QObject(parent)
{
    clear();
    // 編集は全て changed() を経由する (タブの apply() → touch())。
    // 個々の編集箇所に印を付けて回るのではなく、ここ 1 箇所で拾う。
    connect(this, &Project::changed, this, [this] { setModified(true); });
}

void Project::setModified(bool m)
{
    if (m_dirty == m) return;
    m_dirty = m;
    emit modifiedChanged(m_dirty);
}

void Project::clear()
{
    m_general = GeneralOpts{};
    for (int a = 0; a < 3; ++a) {
        m_mesh[a].nodes = { -0.05, 0.05 };
        m_mesh[a].divs  = { 20 };
    }
    m_materials.clear();
    m_loads.clear();
    m_geometries.clear();
    m_feeds.clear();
    m_planewave = PlaneWave{};
    m_probes.clear();
    m_post = PostOpts{};
    m_optical = OpticalOpts{};
    m_displayOptics = DisplayOpticsOpts{};
    m_illumination = IlluminationOpts{};
    m_acoustic = AcousticOpts{};
    // 既定の吸音バジェット (コンサートホール例, α は 125..4k Hz)
    auto absRow = [](int role, const char *name, double area,
                     std::initializer_list<double> a, double airA = 0) {
        AbsorptionRow r;
        r.role = role;
        r.name = QString::fromUtf8(name);
        r.area = area;
        int i = 0;
        for (double v : a) { if (i < 6) r.alpha[i] = v; ++i; }
        r.airA = airA;
        return r;
    };
    m_acoustic.absorption = {
        absRow(AbsorptionRow::Audience, "客席(満席)", 680,
               { 0.50, 0.65, 0.75, 0.80, 0.82, 0.83 }),
        absRow(AbsorptionRow::Ceiling,  "天井(音響)", 900,
               { 0.20, 0.25, 0.30, 0.35, 0.38, 0.40 }),
        absRow(AbsorptionRow::SideWall, "側壁(木)", 620,
               { 0.18, 0.16, 0.15, 0.15, 0.13, 0.10 }),
        absRow(AbsorptionRow::RearWall, "後壁(拡散)", 180,
               { 0.20, 0.22, 0.24, 0.25, 0.26, 0.28 }),
        absRow(AbsorptionRow::Floor,    "床(板)", 420,
               { 0.15, 0.12, 0.10, 0.10, 0.08, 0.07 }),
        // オルガン・反響板 (方向情報なしの Other 行 — Fitzroy では面積比配分)
        absRow(AbsorptionRow::Other,    "オルガン・反響板", 150,
               { 0.12, 0.12, 0.14, 0.15, 0.16, 0.18 }),
        absRow(AbsorptionRow::Air,      "空気吸収", 0,
               { 0, 0, 0, 0, 0, 0 }, 38),
    };
    m_operaAcoustic = OperaAcousticSettings{};
    m_underwater = UnderwaterOpts{};
    m_underwater.ssp = { {0, 1525}, {100, 1510}, {500, 1490}, {1000, 1485},
                         {1500, 1488}, {3000, 1510}, {5000, 1540} };
    m_tidy3d = Tidy3dOpts{};
    // 細分化領域は既定で無し (利用者が GeometryTab で定義する)
    m_refineRegions.clear();
    // ポート定義 / フォトニックネットリストは既定行に戻す
    m_circuitPorts = defaultCircuitPorts();
    m_photonicNet = defaultPhotonicNetlist();
    m_extraLines.clear();
    m_filePath.clear();
}

void Project::setActiveDomain(Domain d)
{
    if (m_domain == d) return;
    m_domain = d;
    emit domainChanged(d);
    emit changed();
}

qint64 Project::totalCells() const
{
    qint64 n = 1;
    for (int a = 0; a < 3; ++a) n *= qMax(1, m_mesh[a].totalCells());
    return n;
}

double Project::estimatedMemoryMB() const
{
    // E/H 6成分 (double) + 媒質ID等の補助配列 ≈ 60 byte/cell
    return totalCells() * 60.0 / (1024.0 * 1024.0);
}

double Project::courantDt() const
{
    const double c0 = 2.99792458e8;
    double s = 0;
    for (int a = 0; a < 3; ++a) {
        const double d = m_mesh[a].minSpacing();
        if (d <= 0 || d >= 1e308) return 0;
        s += 1.0 / (d * d);
    }
    return (s > 0) ? 1.0 / (c0 * std::sqrt(s)) : 0;
}

bool Project::load(const QString &path, QString *err)
{
    clear();
    if (!OfdIO::load(path, *this, err)) return false;

    const QString ofdx = QFileInfo(path).path() + "/" +
                         QFileInfo(path).completeBaseName() + ".ofdx";
    if (QFileInfo::exists(ofdx))
        OfdxIO::load(ofdx, *this, nullptr);   // sidecar is optional

    m_filePath = path;
    emit loaded();
    emit domainChanged(m_domain);
    emit changed();
    // 読み込み直後は未変更 (上の changed() で立った印を下ろす)
    setModified(false);
    return true;
}

bool Project::save(const QString &path, QString *err)
{
    if (!OfdIO::save(path, *this, err)) return false;

    const QString ofdx = QFileInfo(path).path() + "/" +
                         QFileInfo(path).completeBaseName() + ".ofdx";
    if (!OfdxIO::save(ofdx, *this, err)) return false;

    m_filePath = path;
    setModified(false);
    return true;
}
