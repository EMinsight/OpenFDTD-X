// Project.cpp
#include "Project.h"
#include "../io/OfdIO.h"

#include <QFileInfo>
#include <cmath>

using namespace ofd;

Project::Project(QObject *parent) : QObject(parent)
{
    clear();
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
        absRow(AbsorptionRow::Air,      "空気吸収", 0,
               { 0, 0, 0, 0, 0, 0 }, 38),
    };
    m_operaAcoustic = OperaAcousticSettings{};
    m_underwater = UnderwaterOpts{};
    m_underwater.ssp = { {0, 1525}, {100, 1510}, {500, 1490}, {1000, 1485},
                         {1500, 1488}, {3000, 1510}, {5000, 1540} };
    m_tidy3d = Tidy3dOpts{};
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
    return true;
}

bool Project::save(const QString &path, QString *err)
{
    if (!OfdIO::save(path, *this, err)) return false;

    const QString ofdx = QFileInfo(path).path() + "/" +
                         QFileInfo(path).completeBaseName() + ".ofdx";
    if (!OfdxIO::save(ofdx, *this, err)) return false;

    m_filePath = path;
    return true;
}
