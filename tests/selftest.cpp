// selftest.cpp — .ofd ラウンドトリップ検証.
//
// data/sample/*.ofd を全件ロード → シリアライズ → 再パースし、
// 構造 (メッシュ/材質/形状/波源/周波数/ポスト設定) が一致することを確認する。
// 失敗が1件でもあれば非0で終了 (CI 用)。
//
//   ./ofdx_selftest [sample_dir]    (default: ../../data/sample)
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include "audio/AudioEditEngine.h"
#include "core/Project.h"
#include "core/ProjectTemplates.h"
#include "io/ActivationCurve.h"
#include "io/BellhopIO.h"
#include "io/H5Reader.h"
#include "io/KernelResultReader.h"
#include "io/OfdIO.h"
#include "optics/FdeModeSolver.h"
#include "kernel/Runner.h"
#include "io/MeshDiagnostics.h"
#include "io/StlImporter.h"
#include "io/Voxelizer.h"
#include "core/GlassCatalog.h"
#include "optics/MaterialDispersion.h"
#include "optics/ThinFilmStack.h"
#include "core/RoomAcoustics.h"
#include "core/FdtdVerification.h"
#include "core/ToleranceStats.h"
#include "acoustics/core/SoundInsulation.h"
#include "acoustics/core/RoomModes.h"
#include "acoustics/core/EnvironmentalNoise.h"
#include "acoustics/core/FocusedField.h"
#include "optics/PlasmaDispersion.h"
#include "optics/DispersionFit.h"
#include "optics/BendWaveguide.h"
#include "optics/Colorimetry.h"
#include "optics/SourceSpectrum.h"
#include "optics/DisplayMetrics.h"
#include "optics/ParaxialTrace.h"
#include "core/SolverSelection.h"
#include "em/SarMetrics.h"
#include "em/RadioPropagation.h"
#include "em/EmcStandards.h"
#include "em/LumpedRlc.h"
#include "acoustics/qt/QtAcousticAdapter.h"
#include "acoustics/qt/AcousticReportBuilder.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>

#ifdef OFD_USE_HDF5
#include <hdf5.h>
#endif
#include <QTemporaryFile>

using namespace ofd;

static int g_checks = 0;
static int g_failures = 0;
static QString g_file;

static void check(bool cond, const char *what)
{
    ++g_checks;
    if (!cond) {
        ++g_failures;
        std::fprintf(stderr, "FAIL %s: %s\n", qPrintable(g_file), what);
    }
}

static bool nearlyEq(double a, double b)
{
    const double m = std::max(std::fabs(a), std::fabs(b));
    return std::fabs(a - b) <= 1e-9 * std::max(m, 1.0);
}

// Append one triangle (3 vertices) to an ImportedMesh.
static void addTri(ImportedMesh &m,
                   double ax, double ay, double az,
                   double bx, double by, double bz,
                   double cx, double cy, double cz)
{
    const float v[9] = { float(ax), float(ay), float(az),
                         float(bx), float(by), float(bz),
                         float(cx), float(cy), float(cz) };
    for (float f : v) m.vertices.push_back(f);
    ++m.numTriangles;
}

// Build a closed axis-aligned box [x0,x1]×[y0,y1]×[z0,z1] (12 triangles).
static ImportedMesh boxMesh(double x0, double y0, double z0,
                            double x1, double y1, double z1)
{
    ImportedMesh m;
    m.name = "cube";
    auto quad = [&](double a[3], double b[3], double c[3], double d[3]) {
        addTri(m, a[0],a[1],a[2], b[0],b[1],b[2], c[0],c[1],c[2]);
        addTri(m, a[0],a[1],a[2], c[0],c[1],c[2], d[0],d[1],d[2]);
    };
    double p[8][3] = {
        {x0,y0,z0},{x1,y0,z0},{x1,y1,z0},{x0,y1,z0},
        {x0,y0,z1},{x1,y0,z1},{x1,y1,z1},{x0,y1,z1} };
    quad(p[0],p[1],p[2],p[3]);   // z0
    quad(p[4],p[5],p[6],p[7]);   // z1
    quad(p[0],p[1],p[5],p[4]);   // y0
    quad(p[3],p[2],p[6],p[7]);   // y1
    quad(p[0],p[3],p[7],p[4]);   // x0
    quad(p[1],p[2],p[6],p[5]);   // x1
    m.bbox[0]=x0; m.bbox[1]=y0; m.bbox[2]=z0;
    m.bbox[3]=x1; m.bbox[4]=y1; m.bbox[5]=z1;
    return m;
}

// Voxelize a cube on a known uniform grid and check the occupancy is exact.
static void testVoxelizer()
{
    g_file = "voxelizer";

    // 10 cells per axis over [-1,1] → centers at ±0.9,±0.7,...,±0.1.
    MeshAxis ax;
    ax.nodes = { -1.0, 1.0 };
    ax.divs  = { 10 };
    // cube [-0.55,0.55]³ — centers with |c| ≤ 0.5 are strictly inside:
    // {-0.5,-0.3,-0.1,0.1,0.3,0.5} = 6 per axis → 6³ = 216 cells, 36 X-runs.
    const ImportedMesh cube = boxMesh(-0.55, -0.55, -0.55, 0.55, 0.55, 0.55);

    const VoxelResult r = Voxelizer::voxelize(cube, ax, ax, ax, 2);
    check(r.ok, "voxelize ok");
    check(r.nx == 10 && r.ny == 10 && r.nz == 10, "voxel grid dims");
    check(r.occupied == 216, "voxel occupied count (expected 216)");
    check(r.bricks.size() == 36, "voxel brick runs (expected 36)");
    if (r.occupied != 216)
        std::fprintf(stderr, "  (got occupied=%lld bricks=%lld)\n",
                     (long long)r.occupied, (long long)r.bricks.size());

    bool covers = false;
    for (const Geometry &g : r.bricks)
        if (g.g[0] <= 0.0 && 0.0 <= g.g[1] &&
            g.g[2] <= 0.0 && 0.0 <= g.g[3] &&
            g.g[4] <= 0.0 && 0.0 <= g.g[5]) { covers = true; break; }
    check(covers, "origin covered by a voxel brick");
}

static void compareProjects(const Project &a, const Project &b)
{
    check(a.general().title == b.general().title, "title");
    check(a.general().maxiter == b.general().maxiter, "solver maxiter");
    check(a.general().nout == b.general().nout, "solver nout");
    check(nearlyEq(a.general().converg, b.general().converg), "solver converg");
    check(a.general().abc == b.general().abc, "abc");
    check(a.general().pbcX == b.general().pbcX &&
          a.general().pbcY == b.general().pbcY &&
          a.general().pbcZ == b.general().pbcZ, "pbc");
    check(nearlyEq(a.general().f1min, b.general().f1min) &&
          nearlyEq(a.general().f1max, b.general().f1max) &&
          a.general().f1div == b.general().f1div, "frequency1");
    check(nearlyEq(a.general().f2min, b.general().f2min) &&
          nearlyEq(a.general().f2max, b.general().f2max) &&
          a.general().f2div == b.general().f2div, "frequency2");

    for (int ax = 0; ax < 3; ++ax) {
        const MeshAxis &ma = a.mesh(ax), &mb = b.mesh(ax);
        check(ma.nodes.size() == mb.nodes.size(), "mesh node count");
        check(ma.divs == mb.divs, "mesh divisions");
        for (int i = 0; i < qMin(ma.nodes.size(), mb.nodes.size()); ++i)
            check(nearlyEq(ma.nodes[i], mb.nodes[i]), "mesh node value");
    }

    check(a.materials().size() == b.materials().size(), "material count");
    for (int i = 0; i < qMin(a.materials().size(), b.materials().size()); ++i) {
        const Material &x = a.materials()[i], &y = b.materials()[i];
        check(x.type == y.type, "material type");
        check(nearlyEq(x.epsr, y.epsr) && nearlyEq(x.esgm, y.esgm) &&
              nearlyEq(x.amur, y.amur) && nearlyEq(x.msgm, y.msgm), "material values");
        check(nearlyEq(x.einf, y.einf) && nearlyEq(x.ae, y.ae) &&
              nearlyEq(x.be, y.be) && nearlyEq(x.ce, y.ce), "dispersive values");
    }

    check(a.geometries().size() == b.geometries().size(), "geometry count");
    for (int i = 0; i < qMin(a.geometries().size(), b.geometries().size()); ++i) {
        const Geometry &x = a.geometries()[i], &y = b.geometries()[i];
        check(x.materialId == y.materialId, "geometry material");
        check(x.shape == y.shape, "geometry shape");
        for (int k = 0; k < Geometry::paramCount(x.shape); ++k)
            check(nearlyEq(x.g[k], y.g[k]), "geometry coords");
    }

    check(a.feeds().size() == b.feeds().size(), "feed count");
    for (int i = 0; i < qMin(a.feeds().size(), b.feeds().size()); ++i) {
        const Feed &x = a.feeds()[i], &y = b.feeds()[i];
        check(x.dir == y.dir, "feed dir");
        check(nearlyEq(x.x, y.x) && nearlyEq(x.y, y.y) && nearlyEq(x.z, y.z),
              "feed position");
        check(nearlyEq(x.volt, y.volt) && nearlyEq(x.delay, y.delay) &&
              nearlyEq(x.z0, y.z0), "feed params");
    }
    check(a.planewave().enabled == b.planewave().enabled, "planewave");
    if (a.planewave().enabled && b.planewave().enabled) {
        check(nearlyEq(a.planewave().theta, b.planewave().theta) &&
              nearlyEq(a.planewave().phi, b.planewave().phi) &&
              a.planewave().pol == b.planewave().pol, "planewave params");
    }
    check(a.probes().size() == b.probes().size(), "point count");
    check(a.loads().size() == b.loads().size(), "load count");

    const PostOpts &pa = a.post(), &pb = b.post();
    check(pa.plotiter == pb.plotiter, "plotiter");
    check(pa.plotsmith == pb.plotsmith, "plotsmith");
    check(pa.zin.enabled == pb.zin.enabled, "plotzin");
    check(pa.ref.enabled == pb.ref.enabled, "plotref");
    check(pa.far1d.size() == pb.far1d.size(), "plotfar1d count");
    check(pa.far2d == pb.far2d, "plotfar2d");
    check(pa.near1d.size() == pb.near1d.size(), "plotnear1d count");
    check(pa.near2d.size() == pb.near2d.size(), "plotnear2d count");
    check(pa.far1dDb == pb.far1dDb, "far1ddb");
    check(a.extraLines() == b.extraLines(), "extra lines round-trip");

    // ONN 光活性化 (tpa / powersweep) — .ofd キーの往復
    const OpticalOpts &oa = a.optical(), &ob = b.optical();
    check(oa.tpaEnabled == ob.tpaEnabled &&
          oa.tpaMaterialId == ob.tpaMaterialId &&
          nearlyEq(oa.tpaBeta_cmGW, ob.tpaBeta_cmGW), "tpa round-trip");
    check(oa.powerSweepEnabled == ob.powerSweepEnabled &&
          nearlyEq(oa.psPmin_W, ob.psPmin_W) &&
          nearlyEq(oa.psPmax_W, ob.psPmax_W) &&
          oa.psPoints == ob.psPoints && oa.psLog == ob.psLog,
          "powersweep round-trip");

    // RCWA コア設定 (rcwa / rcwalayer) — .ofd キーの往復
    check(oa.solver == ob.solver, "optical solver round-trip");
    check(oa.rcwaLayerList.size() == ob.rcwaLayerList.size(),
          "rcwalayer count round-trip");
    for (int i = 0; i < qMin(oa.rcwaLayerList.size(), ob.rcwaLayerList.size());
         ++i) {
        const RcwaLayer &x = oa.rcwaLayerList[i], &y = ob.rcwaLayerList[i];
        check(nearlyEq(x.eps1, y.eps1) && nearlyEq(x.eps2, y.eps2) &&
              nearlyEq(x.fill, y.fill) &&
              nearlyEq(x.thickness_nm, y.thickness_nm),
              "rcwalayer params round-trip");
    }
}


// Sellmeier: N-BK7 の d線 (587.56nm) 屈折率が nd と一致するか + AGF 取込。
static void testGlassCatalog()
{
    g_file = "glass";

    const auto &all = GlassCatalog::all();
    check(all.size() >= 19, "builtin catalog size");

    const Glass *bk7 = nullptr;
    for (const Glass &g : all)
        if (g.name == "N-BK7") { bk7 = &g; break; }
    check(bk7 != nullptr, "N-BK7 present");
    if (bk7) {
        const double n_d = bk7->n(0.58756);
        check(std::fabs(n_d - 1.5168) < 5e-4, "N-BK7 Sellmeier nd@587.56nm");
        const double n1550 = bk7->n(1.55);
        check(n1550 > 1.49 && n1550 < 1.51, "N-BK7 n@1550nm plausible");
        check(bk7->n(0.4) > bk7->n(1.0), "normal dispersion (n falls with λ)");
    }

    // 不変条件: Sellmeier 係数を持つ全銘柄は自身の nd を再現する
    // (レビューで発覚した mock 由来の不整合データの回帰テスト)
    for (const Glass &g : all) {
        if (!g.hasSellmeier()) continue;
        check(std::fabs(g.n(0.58756) - g.nd) < 2e-3,
              qPrintable(QStringLiteral("%1 Sellmeier reproduces nd").arg(g.name)));
    }
    // 係数なし銘柄 (ZERODUR 等) は nd/vd フォールバックで nd 近傍を返す
    for (const Glass &g : all) {
        if (g.hasSellmeier()) continue;
        check(std::fabs(g.n(0.58756) - g.nd) < 1e-6,
              qPrintable(QStringLiteral("%1 fallback reproduces nd").arg(g.name)));
    }

    // CSV: ヘッダより短い行でもクラッシュせず skip する (範囲外アクセス回帰)
    QTemporaryFile csv;
    csv.setFileTemplate(QDir::tempPath() + "/ofdx_test_XXXXXX.csv");
    if (csv.open()) {
        QTextStream out(&csv);
        out << "name,maker,nd,vd\n";
        out << "SHORTROW,Schott\n";              // nd 列が欠けた行
        out << "GOODGLAS,Test,1.5000,60.0\n";
        out.flush();
        const GlassImportResult r = GlassCatalog::importCsv(csv.fileName());
        check(r.ok && r.imported == 1, "CSV short row skipped, good row imported");
    }

    // AGF import round-trip (Sellmeier1 = formula 2)
    QTemporaryFile agf;
    agf.setFileTemplate(QDir::tempPath() + "/ofdx_test_XXXXXX.agf");
    if (agf.open()) {
        QTextStream out(&agf);
        out << "CC test catalog\n";
        out << "NM TESTGLAS 2 0 1.5168 64.17 0\n";
        out << "CD 1.03961212 0.00600069867 0.231792344 0.0200179144 "
               "1.01046945 103.560653\n";
        out.flush();
        const int before = GlassCatalog::all().size();
        const GlassImportResult r = GlassCatalog::importAgf(agf.fileName());
        check(r.ok && r.imported == 1, "AGF import ok");
        check(GlassCatalog::all().size() == before + 1, "AGF glass appended");
        const Glass &g = GlassCatalog::all().last();
        check(g.name == "TESTGLAS", "AGF glass name");
        check(std::fabs(g.n(0.58756) - 1.5168) < 5e-4, "AGF Sellmeier eval");
    }
}

// Sabine / NC / Barron / エコーグラムの数値健全性。
static void testRoomAcoustics()
{
    using namespace roomac;
    g_file = "roomac";

    // Sabine 既知値: V=12000, A=1200 → RT = 0.161*12000/1200 = 1.61 s
    AcousticOpts a;
    a.volume = 12000;
    a.surface = 3800;
    a.occupancy = 2;
    AbsorptionRow row;
    row.role = AbsorptionRow::Other;
    row.area = 1200;
    for (double &al : row.alpha) al = 1.0;
    a.absorption = { row };
    check(std::fabs(rt60(a, 3, 0) - 1.61) < 1e-6, "Sabine RT=0.161V/A");
    // Eyring は同一 A で Sabine より長くならない… ではなく短くなる
    check(rt60(a, 3, 1) < rt60(a, 3, 0), "Eyring < Sabine for same budget");

    // Barron: 距離が伸びると C80 と D50 は低下、G も低下
    const SeatMetrics near = seatMetrics(8.0, 1.6, 12000);
    const SeatMetrics far = seatMetrics(28.0, 1.6, 12000);
    check(near.C80 > far.C80, "C80 falls with distance");
    check(near.D50 > far.D50, "D50 falls with distance");
    check(near.G > far.G, "G falls with distance");
    check(near.STI > 0 && near.STI <= 1, "STI in [0,1]");

    // NC: 全帯域 0dB → NC 0 近傍、NC-25 曲線ちょうど → 25
    const double quiet[7] = { 0, 0, 0, 0, 0, 0, 0 };
    check(ncRating(quiet) <= 5, "silence rates ~NC-0");
    const double nc25[7] = { 54, 44, 37, 31, 27, 24, 22 };
    check(ncRating(nc25) == 25, "NC-25 curve rates NC-25");
    const double loud[7] = { 90, 90, 90, 90, 90, 90, 90 };
    check(ncRating(loud) == 70, "very loud clamps to NC-70");

    // エコーグラム: 直接音 + 6面の1次反射、反射は全て遅れて弱い
    AcousticOpts b;   // 既定値では absorption が空 → face α は既定 0.2
    b.roomL = 30; b.roomW = 20; b.roomH = 12;
    const double src[3] = { 1.5, 10.0, 1.5 };
    const double rcv[3] = { 9.0, 10.0, 1.2 };
    const QVector<Reflection> refl = echogram(b, src, rcv);
    check(refl.size() == 7, "echogram: direct + 6 reflections");
    check(refl[0].surface.isEmpty() && refl[0].timeMs == 0.0,
          "echogram: direct first");
    for (int i = 1; i < refl.size(); ++i) {
        check(refl[i].timeMs > 0, "reflection arrives after direct");
        check(refl[i].levelDb < 0, "reflection weaker than direct");
    }
    check(itdgMs(refl) > 0, "ITDG positive");

    // .ofdx ラウンドトリップ (室モデル永続化)
    Project p1;
    AcousticOpts &pa = p1.acoustic();
    pa.roomL = 42; pa.volume = 9999; pa.occupancy = 0; pa.rtFormula = 0;
    pa.noiseLevels[0] = 55;
    pa.absorption[0].area = 777;
    QTemporaryFile ofdx;
    ofdx.setFileTemplate(QDir::tempPath() + "/ofdx_test_XXXXXX.ofdx");
    if (ofdx.open()) {
        check(OfdxIO::save(ofdx.fileName(), p1), "ofdx save");
        Project p2;
        check(OfdxIO::load(ofdx.fileName(), p2), "ofdx load");
        const AcousticOpts &qa = p2.acoustic();
        check(qa.roomL == 42 && qa.volume == 9999, "ofdx room round-trip");
        check(qa.occupancy == 0 && qa.rtFormula == 0, "ofdx occ/formula");
        check(qa.noiseLevels[0] == 55, "ofdx noise round-trip");
        check(!qa.absorption.isEmpty() && qa.absorption[0].area == 777,
              "ofdx absorption round-trip");
    }

    // Fitzroy (rtFormula=2): 既知値 (10m 立方体) — 詳細は ctest roomac.fitzroy
    {
        AcousticOpts f;
        f.volume = 1000;
        f.surface = 600;
        auto faceRow = [](int role, double area, double alpha) {
            AbsorptionRow r;
            r.role = role;
            r.area = area;
            for (double &al : r.alpha) al = alpha;
            return r;
        };
        f.absorption = {
            faceRow(AbsorptionRow::Floor,    100, 0.8),
            faceRow(AbsorptionRow::Ceiling,  100, 0.8),
            faceRow(AbsorptionRow::SideWall, 200, 0.1),
            faceRow(AbsorptionRow::RearWall, 200, 0.1) };
        check(std::fabs(rt60(f, 3, 2) - 1.7534) < 1e-3,
              "Fitzroy cube expected 1.7534 s");
        // 旧 0/1 の挙動は不変
        check(std::fabs(rt60(f, 3, 0) - 0.161 * 1000 / 200) < 1e-9,
              "Sabine unchanged with Fitzroy present");
        check(rt60(f, 3, 2) > rt60(f, 3, 1),
              "Fitzroy > Eyring for non-uniform absorption");
    }

    // 騒音源内訳: 既定 4 行 (mock room-acoustics.jsx:697-709)
    {
        const AcousticOpts d;
        check(d.noiseSources.size() == 4, "noise sources: default 4 rows");
        if (d.noiseSources.size() == 4) {
            check(d.noiseSources[0].enabled &&
                  d.noiseSources[0].name == QString::fromUtf8("空調吹出口") &&
                  d.noiseSources[0].level_dBA == 28 &&
                  d.noiseSources[0].measure == QString::fromUtf8("消音器追加"),
                  "noise sources: row 1 defaults");
            check(d.noiseSources[1].enabled &&
                  d.noiseSources[1].name == QString::fromUtf8("ダクト気流音") &&
                  d.noiseSources[1].level_dBA == 24,
                  "noise sources: row 2 defaults");
            check(!d.noiseSources[2].enabled &&
                  d.noiseSources[2].level_dBA == 19 &&
                  !d.noiseSources[3].enabled &&
                  d.noiseSources[3].level_dBA == 15,
                  "noise sources: rows 3/4 unchecked");
        }
    }

    // 騒音源内訳: .ofdx ラウンドトリップ + 既存キー保全
    {
        Project ps;
        auto &ns = ps.acoustic().noiseSources;
        ns.clear();
        NoiseSourceRow r;
        r.enabled = false;
        r.name = QString::fromUtf8("換気ファン");
        r.level_dBA = 31.5;
        r.measure = QString::fromUtf8("防振ゴム");
        ns.push_back(r);
        ps.acoustic().rtFormula = 2;   // Fitzroy も往復する

        QTemporaryFile f2;
        f2.setFileTemplate(QDir::tempPath() + "/ofdx_ns_XXXXXX.ofdx");
        if (f2.open()) {
            check(OfdxIO::save(f2.fileName(), ps), "noise sources ofdx save");
            Project pl;
            check(OfdxIO::load(f2.fileName(), pl), "noise sources ofdx load");
            const auto &qs = pl.acoustic().noiseSources;
            check(qs.size() == 1, "noise sources count round-trip");
            if (qs.size() == 1)
                check(!qs[0].enabled &&
                      qs[0].name == QString::fromUtf8("換気ファン") &&
                      nearlyEq(qs[0].level_dBA, 31.5) &&
                      qs[0].measure == QString::fromUtf8("防振ゴム"),
                      "noise sources row round-trip");
            check(pl.acoustic().rtFormula == 2, "rt_formula=2 round-trip");

            QFile jf(f2.fileName());
            check(jf.open(QIODevice::ReadOnly), "noise sources ofdx reopen");
            const QJsonObject ac = QJsonDocument::fromJson(jf.readAll())
                                       .object().value("acoustic").toObject();
            check(ac.contains("noise_levels") && ac.contains("absorption") &&
                  ac.contains("rt_formula"),
                  "noise sources json keeps existing acoustic keys");
            check(ac.value("noise_sources").toArray().size() == 1,
                  "noise sources json key");
        }
    }

    // 受音点リスト (AcousticTab の受音点表): 既定値と受音点数との一致
    {
        const AcousticOpts d;
        check(d.receivers.size() == 1 && d.micCount == 1,
              "receivers: default row count matches mic count");
        check(d.receivers[0].enabled && d.receivers[0].type == 0 &&
              d.receivers[0].name == QString::fromUtf8("P1 中央") &&
              nearlyEq(d.receivers[0].x, 0.0) &&
              nearlyEq(d.receivers[0].y, 1.2) &&
              nearlyEq(d.receivers[0].z, 8.0),
              "receivers: default row 1 values");
        const QVector<ReceiverRow> six = defaultReceivers(6);
        check(six.size() == 6, "receivers: defaultReceivers(6) size");
        check(six[3].type == 1 && six[3].name == QString::fromUtf8("P4 後方"),
              "receivers: 4th default is the stereo rear point");
        check(six[5].name == QStringLiteral("P6") && nearlyEq(six[5].z, 18.0),
              "receivers: 5th+ defaults continue backwards");
    }

    // 受音点リスト: .ofdx ラウンドトリップ + 既存キー保全
    {
        Project ps;
        auto &rc = ps.acoustic().receivers;
        rc = defaultReceivers(3);
        rc[0].enabled = false;
        rc[0].x = -1.25; rc[0].y = 1.7; rc[0].z = 6.5;
        rc[0].type = 2;
        rc[0].name = QString::fromUtf8("指揮者位置");
        ps.acoustic().micCount = rc.size();

        QTemporaryFile f4;
        f4.setFileTemplate(QDir::tempPath() + "/ofdx_rcv_XXXXXX.ofdx");
        if (f4.open()) {
            check(OfdxIO::save(f4.fileName(), ps), "receivers ofdx save");
            Project pl;
            check(OfdxIO::load(f4.fileName(), pl), "receivers ofdx load");
            const auto &qs = pl.acoustic().receivers;
            check(qs.size() == 3, "receivers count round-trip");
            if (qs.size() == 3) {
                check(!qs[0].enabled && qs[0].type == 2 &&
                      qs[0].name == QString::fromUtf8("指揮者位置") &&
                      nearlyEq(qs[0].x, -1.25) && nearlyEq(qs[0].y, 1.7) &&
                      nearlyEq(qs[0].z, 6.5),
                      "receivers row round-trip");
                check(qs[2].enabled && nearlyEq(qs[2].x, 2.0),
                      "receivers remaining rows round-trip");
            }
            check(pl.acoustic().micCount == 3,
                  "receivers: mic count follows row count");

            QFile jf(f4.fileName());
            check(jf.open(QIODevice::ReadOnly), "receivers ofdx reopen");
            const QJsonObject ac = QJsonDocument::fromJson(jf.readAll())
                                       .object().value("acoustic").toObject();
            check(ac.value("receivers").toArray().size() == 3,
                  "receivers json key");
            check(ac["receivers"].toArray()[0].toObject()
                      .value("pos_m").toArray().size() == 3,
                  "receivers json pos array");
            check(ac.contains("mic_count") && ac.contains("noise_sources") &&
                  ac.contains("absorption"),
                  "receivers json keeps existing acoustic keys");
        }
    }

    // 旧 .ofdx (receivers 無し): mic_count 個の既定点で埋める / 範囲外はクランプ
    {
        QTemporaryFile old;
        old.setFileTemplate(QDir::tempPath() + "/ofdx_rcv_old_XXXXXX.ofdx");
        if (old.open()) {
            const QByteArray legacy =
                "{ \"schemaVersion\": \"1.0\", \"domain\": \"acoustic\","
                "  \"acoustic\": { \"mic_count\": 4 } }";
            old.write(legacy);
            old.flush();
            Project p4;
            check(OfdxIO::load(old.fileName(), p4), "legacy rcv ofdx load");
            const auto &qs = p4.acoustic().receivers;
            check(qs.size() == 4 && p4.acoustic().micCount == 4,
                  "legacy ofdx fills receivers from mic_count");
            check(qs[3].name == QString::fromUtf8("P4 後方"),
                  "legacy ofdx receiver defaults");
        }
        QTemporaryFile bad;
        bad.setFileTemplate(QDir::tempPath() + "/ofdx_rcv_bad_XXXXXX.ofdx");
        if (bad.open()) {
            const QByteArray broken =
                "{ \"schemaVersion\": \"1.0\", \"domain\": \"acoustic\","
                "  \"acoustic\": { \"mic_count\": 9,"
                "    \"receivers\": [ { \"type\": 7, \"name\": \"X\" } ] } }";
            bad.write(broken);
            bad.flush();
            Project p5;
            check(OfdxIO::load(bad.fileName(), p5), "broken rcv ofdx load");
            const auto &qs = p5.acoustic().receivers;
            check(qs.size() == 1 && p5.acoustic().micCount == 1,
                  "broken ofdx: mic count follows receivers");
            check(qs[0].type == 2, "broken ofdx: receiver type clamped");
        }
    }

    // 旧 .ofdx (noise_sources 無し): 既定 4 行のまま (旧ファイル互換)
    {
        QTemporaryFile old;
        old.setFileTemplate(QDir::tempPath() + "/ofdx_ns_old_XXXXXX.ofdx");
        if (old.open()) {
            const QByteArray legacy =
                "{ \"schemaVersion\": \"1.0\", \"domain\": \"acoustic\","
                "  \"acoustic\": { \"room_l\": 25.5, \"rt_formula\": 1 } }";
            old.write(legacy);
            old.flush();
            Project p3;
            check(OfdxIO::load(old.fileName(), p3), "legacy ns ofdx load");
            const auto &qs = p3.acoustic().noiseSources;
            check(qs.size() == 4 &&
                  qs[0].name == QString::fromUtf8("空調吹出口"),
                  "legacy ofdx keeps default noise sources");
            // AcousticTab 追加設定: キー無しの旧ファイルは既定値のまま
            const AcousticOpts &la = p3.acoustic();
            check(!la.lf && la.analysisType == 0 &&
                  la.thirdOctave && la.bandRange == 2,
                  "legacy ofdx keeps AcousticTab defaults");
            check(nearlyEq(la.srcX_m, -3.0) && nearlyEq(la.srcY_m, 1.6) &&
                  nearlyEq(la.srcZ_m, 5.0) &&
                  nearlyEq(la.srcAimTheta_deg, 90.0) &&
                  nearlyEq(la.srcAimPhi_deg, 0.0),
                  "legacy ofdx keeps default source pos/aim");
        }
    }

    // AcousticTab 追加設定 (LF / 音源位置・向き / 解析タイプ / 帯域):
    // .ofdx ラウンドトリップ + 既存キー保全
    {
        Project ps;
        AcousticOpts &a = ps.acoustic();
        a.lf = true;
        a.srcX_m = 1.5; a.srcY_m = 2.5; a.srcZ_m = -3.5;
        a.srcAimTheta_deg = 45.0; a.srcAimPhi_deg = 30.0;
        a.analysisType = 2;
        a.thirdOctave = false;
        a.bandRange = 1;
        QTemporaryFile f3;
        f3.setFileTemplate(QDir::tempPath() + "/ofdx_actab_XXXXXX.ofdx");
        if (f3.open()) {
            check(OfdxIO::save(f3.fileName(), ps), "actab ofdx save");
            Project pl;
            check(OfdxIO::load(f3.fileName(), pl), "actab ofdx load");
            const AcousticOpts &q = pl.acoustic();
            check(q.lf, "actab lf round-trip");
            check(nearlyEq(q.srcX_m, 1.5) && nearlyEq(q.srcY_m, 2.5) &&
                  nearlyEq(q.srcZ_m, -3.5), "actab src pos round-trip");
            check(nearlyEq(q.srcAimTheta_deg, 45.0) &&
                  nearlyEq(q.srcAimPhi_deg, 30.0), "actab src aim round-trip");
            check(q.analysisType == 2, "actab analysis type round-trip");
            check(!q.thirdOctave && q.bandRange == 1, "actab band round-trip");

            // 保存 JSON に新キーが在り、既存 acoustic キーも保全されること
            QFile jf(f3.fileName());
            check(jf.open(QIODevice::ReadOnly), "actab ofdx reopen");
            const QJsonObject ac = QJsonDocument::fromJson(jf.readAll())
                                       .object().value("acoustic").toObject();
            check(ac.contains("lf") && ac.contains("analysis_type") &&
                  ac.contains("third_octave") && ac.contains("band_range"),
                  "actab json keys present");
            check(ac.value("src_pos_m").toArray().size() == 3 &&
                  ac.value("src_aim_deg").toArray().size() == 2,
                  "actab json src pos/aim arrays");
            check(ac.contains("rt60") && ac.contains("mic_count") &&
                  ac.contains("noise_levels"),
                  "actab json keeps existing acoustic keys");
        }
    }

    // ── 音源リスト (AcousticSourceTab の音源一覧) ────────────────────────────
    // 既定値 (新規プロジェクト): 室内 3 本 / 水中 2 本
    {
        const AcousticOpts da;
        const UnderwaterOpts du;
        check(da.sources.size() == 3, "acoustic sources: default 3 rows");
        if (da.sources.size() == 3) {
            check(da.sources[0].enabled &&
                  da.sources[0].name == QStringLiteral("L_main") &&
                  da.sources[0].kind == AcousticSourceRow::Cardioid &&
                  nearlyEq(da.sources[0].x_m, -3.0) &&
                  nearlyEq(da.sources[0].y_m, 4.5) &&
                  nearlyEq(da.sources[0].z_m, 5.0) &&
                  nearlyEq(da.sources[0].level_dB, 94.0),
                  "acoustic sources: row 1 defaults");
            // 同梱していない音源ファイル名を既定に入れない (信号欄は空)
            check(da.sources[0].signal.isEmpty() &&
                  da.sources[2].name == QStringLiteral("C_voice") &&
                  nearlyEq(da.sources[2].level_dB, 88.0),
                  "acoustic sources: no fabricated signal file");
        }
        check(du.sources.size() == 2, "sonar sources: default 2 rows");
        if (du.sources.size() == 2)
            check(du.sources[0].name == QStringLiteral("TX_sonar") &&
                  du.sources[0].kind == AcousticSourceRow::Directional &&
                  nearlyEq(du.sources[0].level_dB, 220.0) &&
                  !du.sources[1].enabled,
                  "sonar sources: row defaults");
    }

    // 音源リスト: .ofdx ラウンドトリップ (室内 / 水中の両方) + 既存キー保全
    {
        Project ps;
        auto &sl = ps.acoustic().sources;
        sl.clear();
        AcousticSourceRow r;
        r.enabled = false;
        r.name = QString::fromUtf8("舞台上手");
        r.kind = AcousticSourceRow::Bipolar;
        r.x_m = 1.25; r.y_m = -2.5; r.z_m = 3.75;
        r.aim = QString::fromUtf8("-Z 15°");
        r.signal = QStringLiteral("speech_48k.wav");
        r.level_dB = 91.5;
        sl.push_back(r);
        auto &ul = ps.underwater().sources;
        ul.clear();
        AcousticSourceRow u;
        u.name = QStringLiteral("TX_A");
        u.kind = AcousticSourceRow::Omni;
        u.x_m = -10; u.y_m = 20; u.z_m = -30;
        u.signal = QStringLiteral("chirp 1-2kHz");
        u.level_dB = 205.0;
        ul.push_back(u);

        QTemporaryFile fs;
        fs.setFileTemplate(QDir::tempPath() + "/ofdx_srclist_XXXXXX.ofdx");
        if (fs.open()) {
            check(OfdxIO::save(fs.fileName(), ps), "source list ofdx save");
            Project pl;
            check(OfdxIO::load(fs.fileName(), pl), "source list ofdx load");
            const auto &qs = pl.acoustic().sources;
            check(qs.size() == 1, "source list count round-trip");
            if (qs.size() == 1)
                check(!qs[0].enabled &&
                      qs[0].name == QString::fromUtf8("舞台上手") &&
                      qs[0].kind == AcousticSourceRow::Bipolar &&
                      nearlyEq(qs[0].x_m, 1.25) &&
                      nearlyEq(qs[0].y_m, -2.5) &&
                      nearlyEq(qs[0].z_m, 3.75) &&
                      qs[0].aim == QString::fromUtf8("-Z 15°") &&
                      qs[0].signal == QStringLiteral("speech_48k.wav") &&
                      nearlyEq(qs[0].level_dB, 91.5),
                      "source list row round-trip");
            const auto &us = pl.underwater().sources;
            check(us.size() == 1, "sonar source list count round-trip");
            if (us.size() == 1)
                check(us[0].name == QStringLiteral("TX_A") &&
                      us[0].kind == AcousticSourceRow::Omni &&
                      nearlyEq(us[0].z_m, -30.0) &&
                      us[0].signal == QStringLiteral("chirp 1-2kHz") &&
                      nearlyEq(us[0].level_dB, 205.0),
                      "sonar source list row round-trip");

            QFile jf(fs.fileName());
            check(jf.open(QIODevice::ReadOnly), "source list ofdx reopen");
            const QJsonObject root =
                QJsonDocument::fromJson(jf.readAll()).object();
            const QJsonObject ac = root.value("acoustic").toObject();
            const QJsonObject uw = root.value("underwater").toObject();
            check(ac.value("sources").toArray().size() == 1 &&
                  uw.value("sources").toArray().size() == 1,
                  "source list json key");
            check(ac.contains("noise_sources") && ac.contains("receivers") &&
                  ac.contains("mic_count") &&
                  uw.contains("sonar_sl_db") && uw.contains("ssp"),
                  "source list json keeps existing keys");
        }
    }

    // 旧 .ofdx (sources 無し): 既定 3 行 / 2 行のまま (旧ファイル互換)。
    // 壊れた種別 (範囲外) はクランプされる。
    {
        QTemporaryFile old;
        old.setFileTemplate(QDir::tempPath() + "/ofdx_srclist_old_XXXXXX.ofdx");
        if (old.open()) {
            const QByteArray legacy =
                "{ \"schemaVersion\": \"1.0\", \"domain\": \"acoustic\","
                "  \"acoustic\": { \"room_l\": 25.5 },"
                "  \"underwater\": { \"sonar_sl_db\": 210.0 } }";
            old.write(legacy);
            old.flush();
            Project p4;
            check(OfdxIO::load(old.fileName(), p4), "legacy src ofdx load");
            check(p4.acoustic().sources.size() == 3 &&
                  p4.acoustic().sources[0].name == QStringLiteral("L_main"),
                  "legacy ofdx keeps default acoustic sources");
            check(p4.underwater().sources.size() == 2 &&
                  p4.underwater().sources[0].name == QStringLiteral("TX_sonar"),
                  "legacy ofdx keeps default sonar sources");
        }
        QTemporaryFile bad2;
        bad2.setFileTemplate(QDir::tempPath() + "/ofdx_srclist_bad_XXXXXX.ofdx");
        if (bad2.open()) {
            const QByteArray broken =
                "{ \"schemaVersion\": \"1.0\", \"domain\": \"acoustic\","
                "  \"acoustic\": { \"sources\": [ { \"name\": \"X\","
                "      \"kind\": 9, \"pos_m\": [1,2] } ] } }";
            bad2.write(broken);
            bad2.flush();
            Project p5;
            check(OfdxIO::load(bad2.fileName(), p5), "broken src ofdx load");
            const auto &bs = p5.acoustic().sources;
            check(bs.size() == 1 && bs[0].kind == AcousticSourceRow::Directional,
                  "source list out-of-range kind clamped");
            // 要素数の足りない pos_m は既定値のまま (壊れた座標を作らない)
            if (bs.size() == 1)
                check(nearlyEq(bs[0].x_m, 0.0) && nearlyEq(bs[0].y_m, 0.0) &&
                      nearlyEq(bs[0].z_m, 0.0) && bs[0].enabled,
                      "source list short pos_m keeps defaults");
        }
    }

    // 壊れた .ofdx の範囲外 int はクランプされ不正な選択を作らない
    {
        QTemporaryFile bad;
        bad.setFileTemplate(QDir::tempPath() + "/ofdx_actab_bad_XXXXXX.ofdx");
        if (bad.open()) {
            const QByteArray broken =
                "{ \"schemaVersion\": \"1.0\", \"domain\": \"acoustic\","
                "  \"acoustic\": { \"analysis_type\": 9,"
                "                  \"band_range\": -2 } }";
            bad.write(broken);
            bad.flush();
            Project pb;
            check(OfdxIO::load(bad.fileName(), pb), "actab broken ofdx load");
            check(pb.acoustic().analysisType == 2 &&
                  pb.acoustic().bandRange == 0,
                  "actab out-of-range ints clamped");
        }
    }

    // ── ISO 3382-1 の減衰時間 (回帰) ─────────────────────────────────────────
    // 期待値は実装と独立に「傾き −60/T の直線」から与える。
    {
        const double T = 2.0;
        QVector<QPointF> line;
        for (int i = 0; i <= 2000; ++i)
            line.push_back({ i * 0.001, -60.0 * (i * 0.001) / T });
        check(std::fabs(decayTimeFromCurve(line, 0, -10) - T) < 1e-6,
              "EDT from an exact -60dB/T line = T");
        check(std::fabs(decayTimeFromCurve(line, -5, -25) - T) < 1e-6,
              "T20 from an exact -60dB/T line = T");
        check(std::fabs(decayTimeFromCurve(line, -5, -35) - T) < 1e-6,
              "T30 from an exact -60dB/T line = T");
        check(decayTimes(line).valid, "decayTimes valid on a full line");
        // 傾きを半分 (−30 dB/s) にすれば減衰時間は 2 倍
        QVector<QPointF> slow;
        for (int i = 0; i <= 4000; ++i)
            slow.push_back({ i * 0.001, -30.0 * (i * 0.001) });
        check(std::fabs(decayTimeFromCurve(slow, -5, -25) - 2.0) < 1e-6,
              "T20 = 60/|slope|");
        // −35 dB まで届かない曲線は T30 が算出不能 (0)
        QVector<QPointF> shortC;
        for (int i = 0; i <= 500; ++i)
            shortC.push_back({ i * 0.001, -60.0 * (i * 0.001) / T });
        check(decayTimeFromCurve(shortC, -5, -35) == 0.0,
              "T30 unavailable when the curve never reaches -35 dB");
        check(!decayTimes(shortC).valid, "decayTimes invalid on a short curve");
    }

    // Barron モデルの Schroeder 曲線: 後期の傾きは −60/T なので T20=T30=RT60、
    // 直接音の段差があるぶん EDT は RT60 以下になる。
    {
        const double Tb = 1.8;
        const QVector<QPointF> c = schroederCurve(15.0, Tb, 12000, 3.0, 601);
        check(c.size() == 601 && c.first().x() == 0.0 && c.first().y() == 0.0,
              "Schroeder curve starts at (0, 0 dB)");
        check(c[1].y() < 0.0 && c.last().y() < c[1].y(),
              "Schroeder curve is decreasing");
        const DecayTimes d = decayTimes(c);
        // 減衰係数に 13.8 (≈ 6·ln10 = 13.8155 の慣用丸め) を使うので
        // 傾きは −59.93/T となり、T20/T30 は RT60 より 0.11% 長く出る。
        check(std::fabs(d.T20 - Tb) < 0.005 * Tb, "Barron curve: T20 = RT60");
        check(std::fabs(d.T30 - Tb) < 0.005 * Tb, "Barron curve: T30 = RT60");
        check(d.EDT > 0 && d.EDT < Tb,
              "Barron curve: EDT < RT60 (direct sound steepens 0..-10 dB)");
    }

    // 吸音力 → ∞ で RT60 → 0 (Sabine)
    {
        AcousticOpts z;
        z.volume = 12000; z.surface = 3800;
        AbsorptionRow rw;
        rw.role = AbsorptionRow::Other;
        rw.area = 100000;
        for (double &al : rw.alpha) al = 1.0;
        z.absorption = { rw };
        check(rt60(z, 3, 0) < 0.05, "RT60 -> 0 as absorption -> inf");
        // 吸音力を 10 倍にすると Sabine の RT は 1/10 になる (RT = 0.161V/A)
        AbsorptionRow tenth = rw;
        tenth.area = 10000;
        AcousticOpts z10 = z;
        z10.absorption = { tenth };
        check(std::fabs(rt60(z, 3, 0) * 10.0 - rt60(z10, 3, 0)) < 1e-9,
              "Sabine RT is inversely proportional to A");
    }

    // Ts / G_late (Barron 閉形式) の極限
    {
        // 残響が消える極限 → C80 → +∞, D50 → 1, Ts → 0
        const SeatMetrics dry = seatMetrics(10.0, 0.1, 1e9);
        check(dry.C80 > 60.0, "C80 -> +inf without reverberation");
        check(dry.D50 > 0.999, "D50 -> 1 without reverberation");
        check(dry.Ts < 1.0, "Ts -> 0 ms without reverberation");
        // 直接音が無視できる極限 → Ts → T/13.8 [s]
        const double T = 2.0;
        const SeatMetrics rev = seatMetrics(30.0, T, 100.0);
        check(std::fabs(rev.Ts - 1000.0 * T / 13.8) < 0.5,
              "Ts -> T/13.8 when the direct sound is negligible");
        // G_late は常に G より小さく、距離とともに下がる
        const SeatMetrics n1 = seatMetrics(8.0, 1.6, 12000);
        const SeatMetrics f1 = seatMetrics(28.0, 1.6, 12000);
        check(n1.Glate < n1.G, "G_late < G");
        check(n1.Glate > f1.Glate, "G_late falls with distance");
        check(n1.Ts < f1.Ts, "Ts grows with distance");
    }

    // 複数音源 (拡声系) の席指標
    {
        const double rr[2] = { 12.0, 12.0 };
        const double gg[2] = { 0.0, 0.0 };
        const SeatMetrics one = seatMetrics(12.0, 1.5, 8000);
        const SeatMetrics two = seatMetrics(rr, gg, 2, 1.5, 8000);
        check(std::fabs((two.G - one.G) - 10.0 * std::log10(2.0)) < 1e-9,
              "2 identical sources = +3.01 dB");
        check(std::fabs(two.C80 - one.C80) < 1e-9,
              "C80 unchanged when the source is duplicated");
        check(std::fabs(two.STI - one.STI) < 1e-9,
              "STI unchanged when the source is duplicated");
        const double r1[1] = { 12.0 }, g1[1] = { 0.0 };
        check(std::fabs(seatMetrics(r1, g1, 1, 1.5, 8000).G - one.G) < 1e-12,
              "multi-source with n=1 degenerates to the single-source form");
        // 近い音源を足すと直接音が増え C80/STI が上がる
        const double rn[2] = { 12.0, 4.0 };
        const SeatMetrics near2 = seatMetrics(rn, gg, 2, 1.5, 8000);
        check(near2.C80 > one.C80 && near2.STI > one.STI,
              "a nearer loudspeaker raises C80/STI");
    }

    // 初期側方エネルギー比 LF / LFC (1次鏡像法 + ISO 3382-1 A.2.6)
    {
        AcousticOpts g;
        g.roomL = 30; g.roomW = 20; g.roomH = 12;
        const double s0[3] = { 1.5, 10.0, 1.5 };
        const double r0[3] = { 12.0, 10.0, 1.2 };
        const LateralEnergy le = lateralEnergy(g, s0, r0);
        check(le.valid && le.LF >= 0.0 && le.LF <= 1.0, "LF in [0,1]");
        check(le.LFC >= le.LF - 1e-12, "LFC >= LF (|cos| >= cos^2)");
        check(le.nEarly > 0, "LF counts early reflections");
        // 側壁を吸音すると側方エネルギーが減る
        AcousticOpts h = g;
        AbsorptionRow side;
        side.role = AbsorptionRow::SideWall;
        side.area = 100;
        for (double &al : side.alpha) al = 0.9;
        h.absorption = { side };
        check(lateralEnergy(h, s0, r0).LF < le.LF,
              "LF falls when the side walls are made absorptive");
    }

    // 音速 (ISO 9613-1) と整合遅延
    {
        check(std::fabs(soundSpeed(0.0) - 331.3) < 1e-9, "c(0 C) = 331.3 m/s");
        check(std::fabs(soundSpeed(20.0) - 343.2) < 0.1, "c(20 C) = 343.2 m/s");
        check(soundSpeed(30.0) > soundSpeed(20.0), "c grows with temperature");
        check(std::fabs(alignmentDelayMs(20.0, 10.0, 20.0)
                        - 10.0 / soundSpeed(20.0) * 1000.0) < 1e-9,
              "delay = (dFar - dNear)/c");
        check(alignmentDelayMs(5.0, 10.0) == 0.0, "no negative delay");
    }

    // PAG / NAG (Davis & Patronis) — 手計算と一致すること
    {
        // D0=32, D1=2, D2=8, Ds=16, NOM=1, EAD=2, FSM=0
        //   NAG = 20log10(32/2)          = 24.0824 dB
        //   PAG = 20log10(32*16/(2*8))   = 20log10(32) = 30.1030 dB
        const GainBeforeFeedback g = pagNag(32, 2, 8, 16, 1, 2.0, 0.0);
        check(g.valid, "pagNag valid for positive distances");
        check(std::fabs(g.NAG - 24.0824) < 1e-3, "NAG = 20log10(D0/EAD)");
        check(std::fabs(g.PAG - 30.1030) < 1e-3,
              "PAG = 20log10(D0*Ds/(D1*D2))");
        check(std::fabs(g.margin - (g.PAG - g.NAG)) < 1e-12,
              "margin = PAG - NAG");
        check(std::fabs((g.PAG - pagNag(32, 2, 8, 16, 2, 2.0, 0.0).PAG)
                        - 10.0 * std::log10(2.0)) < 1e-9,
              "doubling NOM costs 3.01 dB");
        check(std::fabs((g.PAG - pagNag(32, 2, 8, 16, 1, 2.0, 6.0).PAG) - 6.0)
                  < 1e-9, "FSM is subtracted from PAG");
        check(pagNag(32, 1, 8, 16).margin > pagNag(32, 2, 8, 16).margin,
              "a mic closer to the talker gives more margin");
        check(pagNag(32, 2, 8, 24).margin > pagNag(32, 2, 8, 16).margin,
              "a loudspeaker farther from the mic gives more margin");
        check(!pagNag(0, 2, 8, 16).valid, "pagNag rejects zero distances");
    }
}

// 実測 RIR 分析設定 (OperaAcousticSettings) の既定値と .ofdx 永続化。
static void testOperaAcousticSettings()
{
    g_file = "opera";

    // 1) 既定値 (指示仕様)
    {
        const OperaAcousticSettings s;
        check(s.enabled == false, "opera default enabled=false");
        check(s.rirPath.isEmpty() && s.voicePath.isEmpty(),
              "opera default paths empty");
        check(s.voiceType == 6, "opera default voiceType=Unknown");
        check(s.calibrationState == 2, "opera default calibration=Uncalibrated");
        check(s.calibrationOffsetDb == 0.0, "opera default calibOffset=0dB");
        check(s.directSoundMethod == 1, "opera default directSound=Envelope");
        check(s.bandMode == 0, "opera default bandMode=Compat6");
        check(s.noiseCorrection == true, "opera default noiseCorrection=true");
        check(s.minimumDynamicRangeDb == 35.0, "opera default minDR=35dB");
        check(s.channelMode == 2, "opera default channelMode=mono");
        // 可聴化 (フェーズ4) / 歌声分析 (フェーズ3) の既定値
        check(s.auralizationDryFile.isEmpty() &&
              s.auralizationOutputFile.isEmpty(),
              "opera default auralization paths empty");
        check(s.auralizationGainMode == 0, "opera default gainMode=as-is");
        check(s.vocalF0MinHz == 0.0 && s.vocalF0MaxHz == 0.0,
              "opera default vocal F0 override=auto(0)");
        // 音響ソルバー連携 (AcousticSolverTab) の既定値
        check(s.solverBackend == 3, "opera default solver backend=ExternalFDTD");
        check(s.solverExecutable.isEmpty(),
              "opera default solver executable empty");
        check(s.solverThreads == 4 && s.solverProcesses == 1,
              "opera default solver threads=4 processes=1");
    }

    // 2) .ofdx 往復 (設定変更 → save → load → 一致)
    {
        Project p1;
        OperaAcousticSettings &s = p1.operaAcoustic();
        s.enabled = true;
        s.rirPath = "/tmp/hall_stage.wav";
        s.voicePath = "/tmp/aria.wav";
        s.voiceType = 0;
        s.calibrationState = 1;
        s.calibrationOffsetDb = 93.5;
        s.directSoundMethod = 2;
        s.bandMode = 3;
        s.noiseCorrection = false;
        s.minimumDynamicRangeDb = 42.5;
        s.channelMode = 0;
        s.auralizationDryFile = "/tmp/aria_dry.wav";
        s.auralizationOutputFile = "/tmp/aria_wet.wav";
        s.auralizationGainMode = 1;
        s.vocalF0MinHz = 200.0;
        s.vocalF0MaxHz = 1200.0;
        s.solverBackend = 4;
        s.solverExecutable = "/opt/acoustic/solver";
        s.solverThreads = 8;
        s.solverProcesses = 2;

        QTemporaryFile ofdx;
        ofdx.setFileTemplate(QDir::tempPath() + "/ofdx_opera_XXXXXX.ofdx");
        if (ofdx.open()) {
            check(OfdxIO::save(ofdx.fileName(), p1), "opera ofdx save");
            Project p2;
            check(OfdxIO::load(ofdx.fileName(), p2), "opera ofdx load");
            const OperaAcousticSettings &q = p2.operaAcoustic();
            check(q.enabled == true, "opera rt enabled");
            check(q.rirPath == "/tmp/hall_stage.wav", "opera rt rirPath");
            check(q.voicePath == "/tmp/aria.wav", "opera rt voicePath");
            check(q.voiceType == 0, "opera rt voiceType");
            check(q.calibrationState == 1, "opera rt calibrationState");
            check(nearlyEq(q.calibrationOffsetDb, 93.5),
                  "opera rt calibrationOffsetDb");
            check(q.directSoundMethod == 2, "opera rt directSoundMethod");
            check(q.bandMode == 3, "opera rt bandMode");
            check(q.noiseCorrection == false, "opera rt noiseCorrection");
            check(nearlyEq(q.minimumDynamicRangeDb, 42.5), "opera rt minDR");
            check(q.channelMode == 0, "opera rt channelMode");
            check(q.auralizationDryFile == "/tmp/aria_dry.wav",
                  "opera rt auralization dryFile");
            check(q.auralizationOutputFile == "/tmp/aria_wet.wav",
                  "opera rt auralization outputFile");
            check(q.auralizationGainMode == 1, "opera rt auralization gainMode");
            check(nearlyEq(q.vocalF0MinHz, 200.0), "opera rt vocal f0Min");
            check(nearlyEq(q.vocalF0MaxHz, 1200.0), "opera rt vocal f0Max");
            check(q.solverBackend == 4, "opera rt solver backend");
            check(q.solverExecutable == "/opt/acoustic/solver",
                  "opera rt solver executable");
            check(q.solverThreads == 8 && q.solverProcesses == 2,
                  "opera rt solver threads/processes");

            // 4) 保存 JSON に既存 acoustic キーが残ること
            QFile jf(ofdx.fileName());
            check(jf.open(QIODevice::ReadOnly), "opera ofdx reopen");
            const QJsonObject root =
                QJsonDocument::fromJson(jf.readAll()).object();
            const QJsonObject ac = root.value("acoustic").toObject();
            check(ac.contains("rt60") && ac.contains("c80") &&
                  ac.contains("d50") && ac.contains("sti") &&
                  ac.contains("edt"), "opera json keeps metric flags");
            check(ac.contains("sample_rate") && ac.contains("src_directivity") &&
                  ac.contains("mic_count"), "opera json keeps fdtd keys");
            check(ac.contains("room_l") && ac.contains("volume") &&
                  ac.contains("absorption") && ac.contains("noise_levels"),
                  "opera json keeps hall keys");
            const QJsonObject oa = ac.value("opera_analysis").toObject();
            check(oa.value("rir_file").toString() == "/tmp/hall_stage.wav",
                  "opera json rir_file key");
            check(oa.value("analysis_settings").toObject()
                      .value("minimum_dynamic_range_db").toDouble() == 42.5,
                  "opera json nested analysis_settings");
            // docs 予約キー名 (負債 #1) — この名前でなければならない
            check(oa.contains("calibration_offset_db") &&
                  oa.value("calibration_offset_db").toDouble() == 93.5,
                  "opera json calibration_offset_db key");
            // docs §2.1 / 指示書: auralization と vocal のネスト
            const QJsonObject au = oa.value("auralization").toObject();
            check(au.value("dry_file").toString() == "/tmp/aria_dry.wav",
                  "opera json auralization dry_file");
            check(au.value("output_file").toString() == "/tmp/aria_wet.wav",
                  "opera json auralization output_file");
            check(au.value("gain_mode").toInt() == 1,
                  "opera json auralization gain_mode");
            const QJsonObject vo = oa.value("vocal").toObject();
            check(vo.value("f0_min_hz").toDouble() == 200.0 &&
                  vo.value("f0_max_hz").toDouble() == 1200.0,
                  "opera json vocal f0 range");
            // 音響ソルバー連携のネスト (AcousticSolverTab)
            const QJsonObject so = oa.value("solver").toObject();
            check(so.value("backend").toInt() == 4 &&
                  so.value("executable").toString() == "/opt/acoustic/solver" &&
                  so.value("threads").toInt() == 8 &&
                  so.value("processes").toInt() == 2,
                  "opera json solver nest");
        }
    }

    // 3) 旧 .ofdx (opera_analysis 無し): 既定値のまま + 既存キーは読める
    {
        QTemporaryFile old;
        old.setFileTemplate(QDir::tempPath() + "/ofdx_old_XXXXXX.ofdx");
        if (old.open()) {
            const QByteArray legacy =
                "{ \"schemaVersion\": \"1.0\", \"domain\": \"acoustic\","
                "  \"acoustic\": { \"rt60\": false, \"sample_rate\": 96000,"
                "                  \"room_l\": 25.5, \"occupancy\": 1 } }";
            old.write(legacy);
            old.flush();
            Project p3;
            // ロード前に非既定値を入れ、旧ファイルで上書きされないことを確認
            p3.operaAcoustic().bandMode = 2;
            check(OfdxIO::load(old.fileName(), p3), "legacy ofdx load");
            const OperaAcousticSettings &q = p3.operaAcoustic();
            check(q.bandMode == 2 && q.calibrationState == 2 &&
                  q.minimumDynamicRangeDb == 35.0 && !q.enabled,
                  "legacy ofdx leaves opera settings untouched");
            check(q.auralizationDryFile.isEmpty() &&
                  q.auralizationOutputFile.isEmpty() &&
                  q.auralizationGainMode == 0,
                  "legacy ofdx leaves auralization defaults");
            check(q.vocalF0MinHz == 0.0 && q.vocalF0MaxHz == 0.0,
                  "legacy ofdx leaves vocal defaults");
            check(q.solverBackend == 3 && q.solverExecutable.isEmpty() &&
                  q.solverThreads == 4 && q.solverProcesses == 1,
                  "legacy ofdx leaves solver defaults");
            check(q.calibrationOffsetDb == 0.0,
                  "legacy ofdx leaves calibrationOffsetDb=0");
            const AcousticOpts &a = p3.acoustic();
            check(a.rt60 == false && a.sampleRate == 96000,
                  "legacy ofdx acoustic keys still load");
            check(nearlyEq(a.roomL, 25.5) && a.occupancy == 1,
                  "legacy ofdx hall keys still load");
        }
    }

    // 4) 旧 .ofdx (opera_analysis はあるが calibration_offset_db 無し):
    //    オフセットだけ既定 0.0 に落ち、他のキーは通常どおり読める。
    {
        QTemporaryFile old;
        old.setFileTemplate(QDir::tempPath() + "/ofdx_nooffset_XXXXXX.ofdx");
        if (old.open()) {
            const QByteArray legacy =
                "{ \"schemaVersion\": \"1.1\", \"domain\": \"acoustic\","
                "  \"acoustic\": { \"opera_analysis\": {"
                "      \"enabled\": true, \"calibration_state\": 0,"
                "      \"band_mode\": 1 } } }";
            old.write(legacy);
            old.flush();
            Project p4;
            p4.operaAcoustic().calibrationOffsetDb = 77.0;  // 上書きされる想定
            check(OfdxIO::load(old.fileName(), p4), "no-offset ofdx load");
            const OperaAcousticSettings &q = p4.operaAcoustic();
            check(q.enabled && q.calibrationState == 0 && q.bandMode == 1,
                  "no-offset ofdx reads sibling opera keys");
            check(q.calibrationOffsetDb == 77.0,
                  "missing calibration_offset_db leaves current value");
            Project p5;
            check(OfdxIO::load(old.fileName(), p5), "no-offset ofdx load (fresh)");
            check(p5.operaAcoustic().calibrationOffsetDb == 0.0,
                  "missing calibration_offset_db defaults to 0.0");
        }
    }
}

// ── 音響テンプレートの吸音バジェット期待値 ────────────────────────────────
// 中音域 (500 Hz / 1 kHz の平均) RT60 [s] の期待レンジ。
// **この表はテンプレート実装から読み取った値ではなく、用途ごとの目標残響時間
// から独立に決めた期待値である** (ProjectTemplates.cpp 側は「この目標に入る
// ように」吸音バジェットを組んである)。根拠はテンプレート側のコメント参照。
//   ac_hall/ac_raytrace : 交響楽ホール V≈7200m³ の推奨 1.6〜2.1 s
//   ac_office           : 事務室・教室の慣行 0.4〜0.6 s (STI 確保)
//   ac_studio           : EBU Tech 3276 / ITU-R BS.1116 の 0.25·(V/100)^(1/3)
//                         = 0.21 s ± 0.05 s (V = 60 m³)
//   ac_aural            : 多目的小ホール V≈900m³ の 1.0 s 級
//   ac_imagesource      : 剛壁シューボックス (設計値ではなく検証用の長残響)
//   ac_noise            : 住宅居室 0.4〜0.6 s
//   ac_cinema           : SMPTE ST 202 系の劇場 V≈3200m³ で 0.5 s 前後
//   ac_livehouse        : 拡声音楽会場 0.6〜1.0 s
//   ac_gym              : **未対策の現状** を再現するケース (推奨値ではない)
//   ac_church           : オルガン/聖歌隊のための礼拝堂 2.0〜3.0 s
//   ac_restaurant       : 飲食店 0.6 s 以下 (会話明瞭度)
// ac_outdoor は屋外伝搬で室内残響の概念を持たない (吸音バジェット未設定)
// ため対象外。
struct AcRtExpect { const char *id; double lo; double hi; };
static const AcRtExpect kAcRtExpect[] = {
    { "ac_hall",         1.70, 2.00 },
    { "ac_raytrace",     1.70, 2.00 },   // ac_hall と同一の室構成
    { "ac_office",       0.40, 0.60 },
    { "ac_studio",       0.16, 0.26 },
    { "ac_aural",        0.90, 1.20 },
    { "ac_imagesource",  2.40, 3.40 },
    { "ac_noise",        0.35, 0.55 },
    { "ac_cinema",       0.45, 0.60 },
    { "ac_livehouse",    0.65, 0.95 },
    { "ac_gym",          3.00, 4.20 },
    { "ac_church",       2.20, 3.00 },
    { "ac_restaurant",   0.35, 0.55 },
};

// 音響テンプレートの吸音バジェット自己整合:
//   1) 各テンプレートが吸音バジェットを持つこと
//   2) roomac::rt60() の中音域が上表の期待レンジに入ること
//   3) 面の合計面積が室の総表面積 S と桁で乖離していないこと
//      (V/S を上書きして budget だけ既定のまま、という取り違えの検出)
static void testAcousticBudgets()
{
    for (const AcRtExpect &e : kAcRtExpect) {
        g_file = QStringLiteral("acbudget:%1").arg(QLatin1String(e.id));
        Project p;
        if (!templates::apply(p, "acoustic", QLatin1String(e.id))) {
            check(false, "acoustic template applies");
            continue;
        }
        const AcousticOpts &a = p.acoustic();
        check(!a.absorption.isEmpty(), "template has an absorption budget");
        if (a.absorption.isEmpty()) continue;

        const double t500 = roomac::rt60(a, 2, a.rtFormula);
        const double t1k  = roomac::rt60(a, 3, a.rtFormula);
        const double mid  = 0.5 * (t500 + t1k);
        const bool inRange = (mid >= e.lo && mid <= e.hi);
        if (!inRange)
            std::fprintf(stderr,
                         "  (%s: mid RT60 = %.3f s, expected %.2f..%.2f)\n",
                         e.id, mid, e.lo, e.hi);
        check(inRange, "mid-band RT60 within the intended range");
        // 帯域ごとに有限かつ正であること (バジェット行の抜けを検出)
        bool allPositive = true;
        for (int b = 0; b < 6; ++b) {
            const double t = roomac::rt60(a, b, a.rtFormula);
            allPositive = allPositive && (t > 0.0) && (t < 30.0);
        }
        check(allPositive, "all six bands give a finite positive RT60");

        // 面の合計面積 vs 室の総表面積 (Air 行と Other 行は面積に数えるが、
        // Other は付加物なので上振れを許す)
        double areaSum = 0;
        for (const AbsorptionRow &r : a.absorption)
            if (r.enabled && r.role != AbsorptionRow::Air) areaSum += r.area;
        check(areaSum > 0.5 * a.surface && areaSum < 2.0 * a.surface,
              "absorption areas are consistent with the room surface");
    }
}

// プロジェクトテンプレート (core/ProjectTemplates — 応用ギャラリー)。
// 全テンプレートが「保存すればカーネルへ渡せる」状態を作ることを検証する:
// メッシュ妥当・波源あり・周波数正・.ofd 往復一致 + ドメイン設定の抜き取り。
static void testProjectTemplates()
{
    const char *domains[] = { "em", "optical", "acoustic", "underwater",
                              "tidy3d" };
    int total = 0;
    for (const char *d : domains) {
        g_file = QStringLiteral("templates:%1").arg(QLatin1String(d));
        const QStringList ids = templates::idsFor(QLatin1String(d));
        check(!ids.isEmpty(), "domain has templates");
        for (const QString &id : ids) {
            ++total;
            g_file = QStringLiteral("template:%1").arg(id);
            Project p;
            check(templates::apply(p, QLatin1String(d), id),
                  "template applies");
            bool meshOk = true;
            for (int a = 0; a < 3; ++a)
                meshOk = meshOk && p.mesh(a).isValid();
            check(meshOk, "template mesh valid");
            check(p.totalCells() > 0, "template has cells");
            check(!p.general().title.isEmpty(), "template sets title");
            const GeneralOpts &g = p.general();
            check(g.f1min <= g.f1max && g.f1div >= 1 && g.f2min > 0,
                  "template frequencies sane");
            // 波源: 給電 / 平面波 / RCWA 層スタックのいずれかを必ず持つ
            check(!p.feeds().isEmpty() || p.planewave().enabled ||
                  isValidRcwaStack(p.optical().rcwaLayerList),
                  "template has a source");
            // 障害物ジオメトリはメッシュ領域の内側に収まっていること。
            // (領域外へはみ出した形状はボクセル化で丸ごと落ちるため無意味。
            //  室内音響の障害物 = 客席ブロック・間仕切り等でとくに起きやすい)
            bool geomInside = true;
            for (const Geometry &gm : p.geometries()) {
                if (Geometry::paramCount(gm.shape) != 6) continue;
                for (int ax = 0; ax < 3; ++ax) {
                    const double lo = p.mesh(ax).min(), hi = p.mesh(ax).max();
                    const double tol = 1e-9 * std::max(hi - lo, 1e-30);
                    const double g1 = std::min(gm.g[2 * ax], gm.g[2 * ax + 1]);
                    const double g2 = std::max(gm.g[2 * ax], gm.g[2 * ax + 1]);
                    if (g1 < lo - tol || g2 > hi + tol) {
                        geomInside = false;
                        std::fprintf(stderr,
                                     "  (%s: geometry \"%s\" axis %d "
                                     "[%g,%g] outside mesh [%g,%g])\n",
                                     qPrintable(id), qPrintable(gm.name), ax,
                                     g1, g2, lo, hi);
                    }
                }
            }
            check(geomInside, "template geometry fits inside the mesh");
            // 実保存経路の往復 (.ofd + .ofdx — 光ソルバ種別等はサイドカー側)
            QTemporaryDir dir;
            if (dir.isValid()) {
                const QString path = dir.filePath(id + ".ofd");
                QString err;
                bool ok = p.save(path, &err);
                check(ok, "template saves");
                Project p2;
                if (ok && p2.load(path, &err)) {
                    compareProjects(p, p2);
                } else if (ok) {
                    ++g_failures;
                    std::fprintf(stderr, "FAIL %s: reload: %s\n",
                                 qPrintable(g_file), qPrintable(err));
                }
            }
        }
    }
    g_file = "templates";
    // 8 EM + 10 光 + 13 音響 + 7 水中 + 4 tidy3d
    check(total == 42, "42 templates registered");

    // 室内音響テンプレートの障害物ジオメトリ (会場の実構成が入っていること)。
    // ac_imagesource だけは鏡像法の解析解が空の直方体でしか成立しないため
    // 意図的にジオメトリを持たない — 逆にジオメトリが増えたら失敗させる。
    {
        const char *withGeom[] = { "ac_hall", "ac_office", "ac_studio",
                                   "ac_aural", "ac_cinema", "ac_livehouse",
                                   "ac_gym", "ac_church", "ac_restaurant",
                                   "ac_noise", "ac_outdoor" };
        for (const char *id : withGeom) {
            g_file = QStringLiteral("acgeom:%1").arg(QLatin1String(id));
            Project p;
            check(templates::apply(p, "acoustic", QLatin1String(id)),
                  "acoustic template applies");
            check(!p.geometries().isEmpty(),
                  "acoustic template has obstacle geometry");
            check(!p.materials().isEmpty(),
                  "acoustic obstacles reference materials");
        }
        g_file = "acgeom:ac_imagesource";
        Project p;
        check(templates::apply(p, "acoustic", "ac_imagesource"),
              "imagesource template applies");
        check(p.geometries().isEmpty(),
              "image-source template stays an empty shoebox");
        g_file = "templates";
    }

    // ドメイン設定の抜き取り検証 (シナリオが実際に反映されていること)
    {
        Project p;
        check(templates::apply(p, "acoustic", "ac_raytrace"),
              "raytrace template applies");
        check(p.activeDomain() == Domain::Acoustic &&
              p.operaAcoustic().solverBackend == 4,
              "raytrace sets ExternalGeometric backend");
        check(templates::apply(p, "underwater", "uw_sofar"),
              "sofar template applies");
        check(p.activeDomain() == Domain::Underwater &&
              p.underwater().sofar,
              "sofar template enables SOFAR");
        check(templates::apply(p, "optical", "opt_nonlinear"),
              "tpa template applies");
        check(p.optical().tpaEnabled && p.optical().powerSweepEnabled &&
              p.optical().solver == OpticalSolver::BPM,
              "tpa template wires BPM + powersweep");
        check(templates::apply(p, "tidy3d", "t3_large"),
              "tidy3d template applies");
        check(p.activeDomain() == Domain::Optical &&
              p.tidy3d().resolution == "fine",
              "tidy3d template targets optical cloud");
        check(templates::apply(p, "em", "em_rcs"), "rcs template applies");
        check(p.planewave().enabled, "rcs template uses plane wave");
        check(!templates::apply(p, "em", "no_such_template"),
              "unknown template id rejected");
    }
}

// カーネル結果リーダ (io/KernelResultReader) — 実行後の結果反映の入口。
// 実カーネル出力から転記した固定文字列で、給電点表と far1d.log の
// パースを検証する (書式はカーネル側が正 — GUI で変えない)。
static void testKernelResultReader()
{
    g_file = "kernel_result";

    // ofd.log の給電点表 (dipole 実行の実出力から抜粋)
    const QString feedLog = QStringLiteral(
        "Iterations = 1000, Convergence = 1.000e-03\n"
        "\n"
        "feed #1 (Z0[ohm] = 50.00)\n"
        "  frequency[Hz] Rin[ohm]   Xin[ohm]    Gin[mS]    Bin[mS]"
        "    Ref[dB]       VSWR\n"
        "  2.00000e+09     34.621   -104.556      2.854      8.619"
        "     -2.095      8.332\n"
        "  2.45000e+09     66.295     -9.256     14.796      2.066"
        "    -15.883      1.383\n"
        "  3.00000e+09    122.845    103.541      4.771     -4.021"
        "     -4.653      3.851\n"
        "\n"
        "=== output files ===\n");
    const QVector<FeedSweep> sweeps =
        KernelResultReader::parseFeedSweeps(feedLog);
    check(sweeps.size() == 1, "feed table found");
    if (!sweeps.isEmpty()) {
        const FeedSweep &s = sweeps.first();
        check(s.feedIndex == 1 && nearlyEq(s.z0, 50.0), "feed header parsed");
        check(s.points.size() == 3, "feed rows parsed");
        check(nearlyEq(s.points[0].freqHz, 2.0e9) &&
              nearlyEq(s.points[0].rin, 34.621) &&
              nearlyEq(s.points[0].xin, -104.556),
              "feed first row values");
        check(nearlyEq(s.points[1].refDb, -15.883) &&
              nearlyEq(s.points[1].vswr, 1.383),
              "feed ref/vswr columns");
    }
    check(KernelResultReader::parseFeedSweeps(
              QStringLiteral("no tables here\n")).isEmpty(),
          "no feed table -> empty");

    // far1d.log (dipole 実行の実出力から抜粋 — 2 面)
    const QString farLog = QStringLiteral(
        "#1 : X-plane, frequency[Hz] = 3.00000e+09\n"
        "  No.   deg    E-abs[dB]  E-theta[dB] E-theta[deg]    E-phi[dB]"
        "   E-phi[deg]\n"
        "   0    0.0    -240.0000    -240.0000    -148.6233    -240.0000"
        "     131.3390\n"
        "   1    5.0     -22.3709     -22.3709     136.8919    -240.0000"
        "      -5.4757\n"
        "   2   10.0     -16.2784     -16.2784     136.8768    -240.0000"
        "    -159.8874\n"
        "#2 : Y-plane, frequency[Hz] = 3.00000e+09\n"
        "  No.   deg    E-abs[dB]\n"
        "   0    0.0      -8.0000\n"
        "   1  180.0      -9.0000\n");
    const QVector<FarPattern> pats = KernelResultReader::parseFar1d(farLog);
    check(pats.size() == 2, "far1d blocks found");
    if (pats.size() == 2) {
        check(pats[0].plane == QStringLiteral("X-plane") &&
              nearlyEq(pats[0].freqHz, 3.0e9),
              "far1d header parsed");
        check(pats[0].deg.size() == 3 &&
              nearlyEq(pats[0].deg[1], 5.0) &&
              nearlyEq(pats[0].eAbsDb[1], -22.3709),
              "far1d rows parsed");
        check(pats[1].plane == QStringLiteral("Y-plane") &&
              pats[1].deg.size() == 2,
              "far1d second block parsed");
    }
    check(KernelResultReader::parseFar1d(QStringLiteral("---\n")).isEmpty(),
          "no far1d block -> empty");
}

// 音響編集エンジン (AudioEditorTab の DSP — src/audio/AudioEditEngine)。
// 生成の決定性・編集の恒等式・K 特性ラウドネス (1kHz 正弦で LUFS ≒ dBFS)・
// Schroeder RT (既知の指数減衰で T=RT60) を合成信号で検証する。
static void testAudioEditEngine()
{
    using namespace ofd::audioedit;
    g_file = "audioedit";
    const double sr = 48000.0;

    // ── 窓関数: Hann は中央 1 / 両端 0、Flat-top は中央 ≒ 1 ────────────────
    check(std::fabs(windowValue(WindowKind::Hann, 512, 1025) - 1.0) < 1e-12,
          "hann window center=1");
    check(windowValue(WindowKind::Hann, 0, 1025) < 1e-12,
          "hann window edge=0");
    check(std::fabs(windowValue(WindowKind::FlatTop, 512, 1025) - 1.0) < 1e-3,
          "flattop window center~1");
    check(windowInfos().size() == 15, "15 window kinds");

    // ── 生成: 正弦のピーク / 長さ、ノイズの決定性、MLS の 2 値性 ────────────
    const AudioBuffer sine =
        generateSignal(SignalKind::Sine, 1000, 0, 1.0, 0.5, sr);
    check(sine.sampleCount() == 48000, "sine length 1s");
    {
        double pk = 0;
        for (double v : sine.channels[0]) pk = std::max(pk, std::fabs(v));
        check(std::fabs(pk - 0.5) < 1e-3, "sine peak = amp");
    }
    {
        const AudioBuffer w1 =
            generateSignal(SignalKind::White, 0, 0, 0.2, 0.7, sr);
        const AudioBuffer w2 =
            generateSignal(SignalKind::White, 0, 0, 0.2, 0.7, sr);
        check(w1.channels[0] == w2.channels[0],
              "white noise deterministic (fixed seed)");
        const AudioBuffer mls =
            generateSignal(SignalKind::Mls, 0, 0, 0.1, 0.6, sr);
        bool binary = true;
        for (double v : mls.channels[0])
            if (std::fabs(std::fabs(v) - 0.6) > 1e-12) binary = false;
        check(binary, "mls is two-valued ±amp");
        const AudioBuffer imp =
            generateSignal(SignalKind::Impulse, 0, 0, 0.01, 1.0, sr);
        check(imp.channels[0][0] == 1.0 && imp.channels[0][1] == 0.0,
              "impulse only at t=0");
    }

    // ── 編集: 恒等式と長さ ──────────────────────────────────────────────────
    {
        const AudioBuffer rev2 =
            reverseRange(reverseRange(sine, 0, 0), 0, 0);
        check(rev2.channels[0] == sine.channels[0],
              "reverse twice = identity");
        const AudioBuffer trimmed = trimToRange(sine, 1000, 5000);
        check(trimmed.sampleCount() == 4000, "trim length");
        check(trimmed.channels[0][0] == sine.channels[0][1000],
              "trim copies from range start");
        const AudioBuffer del = deleteRange(sine, 1000, 5000);
        check(del.sampleCount() == 48000 - 4000, "delete length");
        check(del.channels[0][1000] == sine.channels[0][5000],
              "delete joins across range");
        double gainDb = 0.0;
        const AudioBuffer norm = normalizeRange(sine, 0, 0, 0.98, &gainDb);
        double pk = 0;
        for (double v : norm.channels[0]) pk = std::max(pk, std::fabs(v));
        check(std::fabs(pk - 0.98) < 1e-3, "normalize peak=0.98");
        check(std::fabs(gainDb - 20.0 * std::log10(0.98 / 0.5)) < 0.05,
              "normalize reported gain");
        const AudioBuffer g6 = gainRange(sine, 0, 0, 6.0);
        check(std::fabs(g6.channels[0][12] /
                        sine.channels[0][12] - std::pow(10.0, 0.3)) < 1e-6,
              "gain +6dB factor");
        const AudioBuffer fi = fadeRange(sine, 0, 0, true);
        check(fi.channels[0][0] == 0.0, "fade-in starts at 0");
        const AudioBuffer sil = silenceRange(sine, 100, 200);
        check(sil.channels[0][150] == 0.0 && sil.channels[0][99] != 0.0,
              "silence only in range");
    }

    // ── biquad: LP 1kHz は 100Hz を通し 8kHz を強く減衰 ─────────────────────
    {
        auto rmsOf = [](const AudioBuffer &b) {
            double s = 0;
            for (double v : b.channels[0]) s += v * v;
            return std::sqrt(s / b.sampleCount());
        };
        const AudioBuffer lo =
            generateSignal(SignalKind::Sine, 100, 0, 0.5, 0.5, sr);
        const AudioBuffer hi =
            generateSignal(SignalKind::Sine, 8000, 0, 0.5, 0.5, sr);
        const double loRatio =
            rmsOf(applyBiquad(lo, BiquadKind::LowPass, 1000, 0.707, 0)) /
            rmsOf(lo);
        const double hiRatio =
            rmsOf(applyBiquad(hi, BiquadKind::LowPass, 1000, 0.707, 0)) /
            rmsOf(hi);
        check(std::fabs(loRatio - 1.0) < 0.02, "LP passes 100Hz");
        check(hiRatio < 0.05, "LP attenuates 8kHz > 26dB");
    }

    // ── レベル指標: 単位正弦の RMS = -3.01 dBFS / crest = 3.01 dB ──────────
    {
        const AudioBuffer u =
            generateSignal(SignalKind::Sine, 1000, 0, 1.0, 1.0, sr);
        const LevelMetrics m = analyzeLevels(u, 0, 0);
        check(std::fabs(m.rmsDbfs + 3.01) < 0.05, "sine RMS -3.01 dBFS");
        check(std::fabs(m.crestDb - 3.01) < 0.05, "sine crest 3.01 dB");
        check(std::fabs(m.peakDbfs) < 0.05, "sine peak 0 dBFS");
    }

    // ── 減衰が -25dB に達しない短い定常区間では T20/T30 を返さない ─────────
    // (一定振幅 N=100: 逆積分の最終値は 10log10(1/N) = -20dB — 規則 6 の流儀)
    {
        AudioBuffer flat;
        flat.sampleRateHz = sr;
        flat.channels.assign(1, std::vector<double>(100, 1.0));
        const LevelMetrics m = analyzeLevels(flat, 0, 0);
        check(!m.hasT20 && !m.hasT30,
              "insufficient decay yields no T20/T30");
        check(m.hasEdt, "-10dB (EDT) still reachable at N=100");
    }

    // ── Schroeder RT: 既知の指数減衰 (RT60 = 0.5s) で T20/T30 ≒ 0.5 ───────
    {
        const double rt = 0.5;
        AudioBuffer ir;
        ir.sampleRateHz = sr;
        ir.channels.assign(1, std::vector<double>(std::size_t(sr * 1.5), 0.0));
        for (std::size_t i = 0; i < ir.channels[0].size(); ++i) {
            const double t = i / sr;
            ir.channels[0][i] = std::pow(10.0, -3.0 * t / rt)
                              * std::sin(2 * 3.14159265358979 * 1000 * t);
        }
        const LevelMetrics m = analyzeLevels(ir, 0, 0);
        check(m.hasEdt && m.hasT20 && m.hasT30, "exp decay has EDT/T20/T30");
        check(std::fabs(m.t20Sec - rt) < rt * 0.05, "T20 within 5% of RT60");
        check(std::fabs(m.t30Sec - rt) < rt * 0.05, "T30 within 5% of RT60");
        check(std::fabs(m.edtSec - rt) < rt * 0.10, "EDT within 10% of RT60");
    }

    // ── ラウドネス: 1kHz 正弦 (フルスケール) は LUFS ≒ RMS dBFS ────────────
    // K 特性の -0.691 dB 補正は 997Hz 正弦で LUFS = dBFS になる較正
    // (EBU Tech 3341)。任意 fs の係数導出を 48k / 44.1k の両方で確認する。
    {
        for (double fs : { 48000.0, 44100.0 }) {
            const AudioBuffer u =
                generateSignal(SignalKind::Sine, 1000, 0, 3.0, 1.0, fs);
            const LoudnessMetrics lm = analyzeLoudness(u);
            check(std::fabs(lm.integratedLufs + 3.01) < 0.3,
                  fs == 48000.0 ? "LUFS = dBFS for 1kHz sine (48k)"
                                : "LUFS = dBFS for 1kHz sine (44.1k)");
            check(lm.truePeakDbtp >= -0.05, "true peak >= sample peak");
        }
    }

    // ── オクターブバンド: 1kHz 正弦は 1k 帯域が最大 ─────────────────────────
    {
        const std::vector<OctaveBand> bands = octaveBands(sine);
        check(bands.size() == 10, "10 octave bands");
        std::size_t best = 0;
        for (std::size_t i = 1; i < bands.size(); ++i)
            if (bands[i].db > bands[best].db) best = i;
        check(bands[best].fcHz == 1000.0, "1kHz sine peaks in 1k band");
    }

    // ── スペクトル: 1kHz 正弦のピークが log10(1000) 近傍 ────────────────────
    {
        const std::vector<SpectrumPoint> sp =
            spectrum(sine, 0, WindowKind::Hann);
        check(!sp.empty(), "spectrum non-empty");
        std::size_t best = 0;
        for (std::size_t i = 1; i < sp.size(); ++i)
            if (sp[i].db > sp[best].db) best = i;
        check(std::fabs(sp[best].logF - 3.0) < 0.05,
              "spectrum peak at 1kHz");
    }

    // ── 時間軸: 長さの規約 ──────────────────────────────────────────────────
    {
        const AudioBuffer sl = timeStretch(sine, 1.5);
        check(std::fabs(double(sl.sampleCount()) - 48000 * 1.5) < 2,
              "time stretch length ×1.5");
        const AudioBuffer ps = pitchShift(sine, 5.0);
        check(ps.sampleCount() == sine.sampleCount(),
              "pitch shift keeps length");
        const AudioBuffer rt2 = applyRate(sine, 2.0);
        check(rt2.sampleCount() == 24000, "rate ×2 halves length");
    }

    // ── リバーブ: テール付加 + 決定性 ───────────────────────────────────────
    {
        const AudioBuffer clickBuf =
            generateSignal(SignalKind::Click, 0, 0, 0.2, 0.8, sr);
        const AudioBuffer r1 = applyReverb(clickBuf, 0.3, 0.35);
        const AudioBuffer r2 = applyReverb(clickBuf, 0.3, 0.35);
        check(r1.sampleCount() > clickBuf.sampleCount(),
              "reverb adds IR tail");
        check(r1.channels[0] == r2.channels[0],
              "reverb deterministic (fixed-seed IR)");
    }

    // ── ノイズリダクション: 自己プロファイルで白色雑音の RMS が下がる ───────
    {
        const AudioBuffer noise =
            generateSignal(SignalKind::White, 0, 0, 1.0, 0.3, sr);
        const std::vector<double> prof = noiseProfile(noise, 0, 0);
        const AudioBuffer den = denoise(noise, prof, 12.0);
        auto rmsOf = [](const AudioBuffer &b) {
            double s = 0;
            for (double v : b.channels[0]) s += v * v;
            return std::sqrt(s / b.sampleCount());
        };
        check(rmsOf(den) < rmsOf(noise) * 0.5,
              "denoise reduces noise floor > 6dB");
        // 2048 サンプル未満の選択は学習失敗 (空プロファイル) を返す
        check(noiseProfile(noise, 0, 1000).empty(),
              "noise profile fails on short selection");
    }

    // ── Nyquist 超のオクターブ帯域は出力しない (fs=8kHz → 8k/16k 帯を除外) ──
    {
        const AudioBuffer lowFs =
            generateSignal(SignalKind::Sine, 1000, 0, 1.0, 0.5, 8000.0);
        const std::vector<OctaveBand> bands = octaveBands(lowFs);
        bool hasAbove = false;
        for (const OctaveBand &b : bands)
            if (b.fcHz / std::sqrt(2.0) >= 4000.0) hasAbove = true;
        check(!hasAbove && bands.size() < 10,
              "octave bands above Nyquist excluded");
    }

    // ── ステレオ: モノ化で L=R、Side 抽出で L=-R ────────────────────────────
    {
        AudioBuffer st;
        st.sampleRateHz = sr;
        st.channels.assign(2, std::vector<double>(100));
        for (int i = 0; i < 100; ++i) {
            st.channels[0][i] = 0.5;
            st.channels[1][i] = -0.25;
        }
        const AudioBuffer mono = applyStereoOp(st, StereoOp::Mono);
        check(mono.channels[0] == mono.channels[1] &&
              std::fabs(mono.channels[0][0] - 0.125) < 1e-12,
              "stereo mono = (L+R)/2");
        const AudioBuffer side = applyStereoOp(st, StereoOp::Side);
        check(std::fabs(side.channels[0][0] - 0.375) < 1e-12 &&
              std::fabs(side.channels[1][0] + 0.375) < 1e-12,
              "stereo side extraction");
    }
}

// 校正オフセットのゲート規則 (負債 #1):
// QtAcousticAdapter は calibrationState==Absolute のときだけオフセットを
// 分析コアへ渡す。Relative / Uncalibrated では 0 (未校正なのに SPL が
// ずれるのを防ぐ — CLAUDE.md 絶対規則 6)。RIR / 歌声の両経路で同じ規則。
static void testCalibrationOffsetGate()
{
    g_file = "calib-offset";

    OperaAcousticSettings s;
    s.calibrationOffsetDb = 94.0;

    s.calibrationState = 0;   // Absolute
    {
        const acoustics::RirAnalyzerConfig rc =
            QtAcousticAdapter::toAnalyzerConfig(s);
        const acoustics::VocalAnalyzerConfig vc =
            QtAcousticAdapter::toVocalConfig(s);
        check(rc.calibration == acoustics::CalibrationState::Absolute,
              "gate: rir calibration=Absolute");
        check(nearlyEq(rc.calibrationOffsetDb, 94.0),
              "gate: rir offset passed when Absolute");
        check(vc.calibration == acoustics::CalibrationState::Absolute,
              "gate: vocal calibration=Absolute");
        check(nearlyEq(vc.calibrationOffsetDb, 94.0),
              "gate: vocal offset passed when Absolute");
    }

    s.calibrationState = 1;   // Relative
    {
        const acoustics::RirAnalyzerConfig rc =
            QtAcousticAdapter::toAnalyzerConfig(s);
        const acoustics::VocalAnalyzerConfig vc =
            QtAcousticAdapter::toVocalConfig(s);
        check(rc.calibration == acoustics::CalibrationState::Relative,
              "gate: rir calibration=Relative");
        check(rc.calibrationOffsetDb == 0.0,
              "gate: rir offset forced to 0 when Relative");
        check(vc.calibrationOffsetDb == 0.0,
              "gate: vocal offset forced to 0 when Relative");
    }

    s.calibrationState = 2;   // Uncalibrated
    {
        const acoustics::RirAnalyzerConfig rc =
            QtAcousticAdapter::toAnalyzerConfig(s);
        const acoustics::VocalAnalyzerConfig vc =
            QtAcousticAdapter::toVocalConfig(s);
        check(rc.calibration == acoustics::CalibrationState::Uncalibrated,
              "gate: rir calibration=Uncalibrated");
        check(rc.calibrationOffsetDb == 0.0,
              "gate: rir offset forced to 0 when Uncalibrated");
        check(vc.calibration == acoustics::CalibrationState::Uncalibrated,
              "gate: vocal calibration=Uncalibrated");
        check(vc.calibrationOffsetDb == 0.0,
              "gate: vocal offset forced to 0 when Uncalibrated");
    }

    // 既定 (オフセット未設定) は従来どおり 0
    {
        const OperaAcousticSettings d;
        check(QtAcousticAdapter::toAnalyzerConfig(d).calibrationOffsetDb == 0.0,
              "gate: default settings keep offset 0");
    }
}

// ONN 光活性化関数 (TPA / powersweep) — .ofd/.ofdx 永続化 + CSV パーサ。
// 出典: Honda, Shoji, Amemiya, Opt. Lett. 49, 5811 (2024).
static void testOnnActivation()
{
    g_file = "onn";

    // 1) 既定値 (無効): .ofd 出力に tpa/powersweep が現れず、
    //    有効化→無効化で従来出力とバイト一致 (後方互換)
    {
        Project p;
        const QString base = OfdIO::serialize(p);
        check(!base.contains("tpa"), "onn: no tpa line by default");
        check(!base.contains("powersweep"), "onn: no powersweep by default");

        OpticalOpts &o = p.optical();
        o.tpaEnabled = true;
        o.powerSweepEnabled = true;
        const QString on = OfdIO::serialize(p);
        check(on.contains("\ntpa = 2 424\n"), "onn: tpa line emitted");
        check(on.contains("\npowersweep = 0.001 10 41 log\n"),
              "onn: powersweep line emitted");

        o.tpaEnabled = false;
        o.powerSweepEnabled = false;
        check(OfdIO::serialize(p) == base,
              "onn: disabled output byte-identical to legacy");
    }

    // 2) .ofd 往復: tpa/powersweep 行 → 構造 → 再シリアライズ
    {
        const QString text =
            "OpenFDTD 4 2\n"
            "tpa = 3 250.5\n"
            "powersweep = 0.01 5 21 lin\n"
            "end\n";
        Project p;
        QString err;
        check(OfdIO::parse(text, p, &err), "onn: parse tpa/powersweep");
        const OpticalOpts &o = p.optical();
        check(o.tpaEnabled && o.tpaMaterialId == 3 &&
              nearlyEq(o.tpaBeta_cmGW, 250.5), "onn: tpa parsed");
        check(o.powerSweepEnabled && nearlyEq(o.psPmin_W, 0.01) &&
              nearlyEq(o.psPmax_W, 5.0) && o.psPoints == 21 && !o.psLog,
              "onn: powersweep parsed (lin)");
        check(p.extraLines().isEmpty(),
              "onn: tpa keys not duplicated into extraLines");
        const QString out = OfdIO::serialize(p);
        check(out.contains("\ntpa = 3 250.5\n") &&
              out.contains("\npowersweep = 0.01 5 21 lin\n"),
              "onn: reserialize keeps tpa/powersweep");
    }

    // 3) .ofdx 往復 + 既存 optical キーが保存されること
    {
        Project p1;
        OpticalOpts &o = p1.optical();
        o.solver = OpticalSolver::BPM;
        o.tpaEnabled = true;
        o.tpaMaterialId = 4;
        o.tpaBeta_cmGW = 500.0;
        o.powerSweepEnabled = true;
        o.psPmin_W = 0.002;
        o.psPmax_W = 20.0;
        o.psPoints = 33;
        o.psLog = false;

        QTemporaryFile ofdx;
        ofdx.setFileTemplate(QDir::tempPath() + "/ofdx_onn_XXXXXX.ofdx");
        if (ofdx.open()) {
            check(OfdxIO::save(ofdx.fileName(), p1), "onn ofdx save");
            Project p2;
            check(OfdxIO::load(ofdx.fileName(), p2), "onn ofdx load");
            const OpticalOpts &q = p2.optical();
            check(q.tpaEnabled && q.tpaMaterialId == 4 &&
                  nearlyEq(q.tpaBeta_cmGW, 500.0), "onn ofdx tpa round-trip");
            check(q.powerSweepEnabled && nearlyEq(q.psPmin_W, 0.002) &&
                  nearlyEq(q.psPmax_W, 20.0) && q.psPoints == 33 && !q.psLog,
                  "onn ofdx powersweep round-trip");

            QFile jf(ofdx.fileName());
            check(jf.open(QIODevice::ReadOnly), "onn ofdx reopen");
            const QJsonObject root =
                QJsonDocument::fromJson(jf.readAll()).object();
            const QJsonObject opt = root.value("optical").toObject();
            check(opt.contains("solver") && opt.contains("mode") &&
                  opt.contains("wavelength") && opt.contains("rcwa") &&
                  opt.contains("bpm") && opt.contains("fmm") &&
                  opt.contains("bpf") && opt.contains("ring"),
                  "onn json keeps existing optical keys");
            check(opt.value("tpa").toObject().value("beta_cm_gw")
                      .toDouble() == 500.0, "onn json tpa key");
            check(opt.value("powersweep").toObject().value("scale")
                      .toString() == "lin", "onn json powersweep key");
        }
    }

    // 4) 旧 .ofdx (tpa/powersweep 無し): 既定値のまま (旧ファイル互換)
    {
        QTemporaryFile old;
        old.setFileTemplate(QDir::tempPath() + "/ofdx_onn_old_XXXXXX.ofdx");
        if (old.open()) {
            const QByteArray legacy =
                "{ \"schemaVersion\": \"1.0\", \"domain\": \"optical\","
                "  \"optical\": { \"solver\": 2 } }";
            old.write(legacy);
            old.flush();
            Project p;
            check(OfdxIO::load(old.fileName(), p), "onn legacy ofdx load");
            const OpticalOpts &q = p.optical();
            check(!q.tpaEnabled && q.tpaMaterialId == 2 &&
                  q.tpaBeta_cmGW == 424.0,
                  "onn legacy ofdx leaves tpa defaults");
            check(!q.powerSweepEnabled && q.psPmin_W == 0.001 &&
                  q.psPmax_W == 10.0 && q.psPoints == 41 && q.psLog,
                  "onn legacy ofdx leaves powersweep defaults");
        }
    }

    // 5) activation_curve.csv パーサ
    {
        const QString csv =
            "P_in_W,P_out_W,transmission\n"
            "0.001,0.000999,0.999\n"
            "\n"
            "# comment line\n"
            "10,3.2,0.32\n";
        QVector<ActivationPoint> pts;
        QString err;
        check(ActivationCurve::parse(csv, pts, &err), "onn csv parse ok");
        check(pts.size() == 2, "onn csv row count");
        check(nearlyEq(pts[0].pin, 0.001) && nearlyEq(pts[0].pout, 0.000999) &&
              nearlyEq(pts[0].T, 0.999), "onn csv first row");
        check(nearlyEq(pts[1].pin, 10.0) && nearlyEq(pts[1].T, 0.32),
              "onn csv last row");
        check(!ActivationCurve::parse("P_in_W,P_out_W,transmission\n",
                                      pts, &err),
              "onn csv header-only fails");
        check(!ActivationCurve::load(QDir::tempPath() +
                  "/ofdx_no_such_dir/activation_curve.csv", pts),
              "onn csv missing file fails");

        // カーネルログからの A_eff 抽出
        check(nearlyEq(ActivationCurve::aeffFromLogLine(
                  "ONN: A_eff = 2.5e-13 [m^2]"), 2.5e-13),
              "onn aeff extracted from log");
        check(ActivationCurve::aeffFromLogLine(
                  "ONN: P_in=1 -> P_out=0.5 (T=0.5)") == 0.0,
              "onn non-aeff log line ignored");
    }

    // 6) 入力バリデーション (OpticalTab と共用の判定):
    //    不正値では有効フラグを落とし、カーネルへ渡さない
    {
        check(isValidTpaBeta(424.0), "onn beta 424 valid");
        check(!isValidTpaBeta(QString().toDouble()),
              "onn beta empty text invalid");
        check(!isValidTpaBeta(0.0), "onn beta zero invalid");
        check(!isValidTpaBeta(-1.0), "onn beta negative invalid");
        check(isValidPowerSweepRange(0.001, 10.0), "onn sweep range valid");
        check(isValidPowerSweepRange(2.0, 2.0), "onn sweep pmin==pmax valid");
        check(!isValidPowerSweepRange(0.0, 10.0), "onn sweep pmin zero invalid");
        check(!isValidPowerSweepRange(-1.0, 10.0),
              "onn sweep pmin negative invalid");
        check(!isValidPowerSweepRange(5.0, 1.0), "onn sweep pmax<pmin invalid");

        // 不正入力に対する apply() 相当の結果: 有効フラグが落ちるので
        // .ofd 出力は従来 (無効時) とバイト一致 = 424 では走らない
        Project p;
        const QString base = OfdIO::serialize(p);
        OpticalOpts &o = p.optical();
        o.tpaEnabled = /* checkbox on */ true && isValidTpaBeta(0.0);
        o.powerSweepEnabled = true && isValidPowerSweepRange(5.0, 1.0);
        check(!o.tpaEnabled && !o.powerSweepEnabled,
              "onn invalid input drops enable flags");
        check(OfdIO::serialize(p) == base,
              "onn invalid input leaves kernel input unchanged");
    }

    // 7) 解析解 T = 1/(1+β(P/A_eff)L) (β は [cm/GW] → ×1e-11 で [m/W])
    {
        const double beta = 424.0, aeff = 1e-13, L = 1e-4;
        check(nearlyEq(ActivationCurve::analyticTransmission(0.0, beta,
                                                             aeff, L), 1.0),
              "onn analytic T(0 W) = 1");
        // 424 cm/GW = 4.24e-9 m/W, P/A_eff = 1e13 W/m², L = 1e-4 m
        check(nearlyEq(ActivationCurve::analyticTransmission(1.0, beta,
                                                             aeff, L),
                       1.0 / (1.0 + 4.24)),
              "onn analytic T(1 W) matches hand calculation");
        check(ActivationCurve::analyticTransmission(1.0, beta, aeff, L) >
              ActivationCurve::analyticTransmission(2.0, beta, aeff, L),
              "onn analytic decreases with input power");
        check(ActivationCurve::analyticTransmission(1.0, 0.0, aeff, L) == 0.0,
              "onn analytic without beta -> 0 (no overlay)");
        check(ActivationCurve::analyticTransmission(1.0, beta, 0.0, L) == 0.0,
              "onn analytic without A_eff -> 0 (no overlay)");
        check(ActivationCurve::analyticTransmission(1.0, beta, aeff, 0.0) == 0.0,
              "onn analytic without L -> 0 (no overlay)");
        // β のスナップショット差が解析解に効く (実行中の UI 編集で
        // 別カーブになることの裏返し — だから実行開始時に控える)
        check(!nearlyEq(ActivationCurve::analyticTransmission(1.0, beta,
                                                              aeff, L),
                        ActivationCurve::analyticTransmission(1.0, beta * 2,
                                                              aeff, L)),
              "onn analytic depends on the beta snapshot");
    }
}

// RCWA コア設定 (OpenRCWA の rcwa / rcwalayer キー) — .ofd/.ofdx 永続化。
// キー仕様は OpenRCWA/sol/input_data.c が正: 単位は m、GUI は nm で保持する。
static void testRcwaCore()
{
    g_file = "rcwa";

    // 層スタックの妥当性判定 (OpticalTab と共用)
    {
        check(isValidRcwaLayer(RcwaLayer{}), "rcwa default layer valid");
        check(!isValidRcwaLayer(RcwaLayer{ 0.0, 1.0, 0.5, 0.0 }),
              "rcwa eps1 zero invalid");
        check(!isValidRcwaLayer(RcwaLayer{ -1.0, 1.0, 0.5, 0.0 }),
              "rcwa eps1 negative invalid");
        check(!isValidRcwaLayer(RcwaLayer{ 1.0, 0.0, 0.5, 0.0 }),
              "rcwa eps2 zero invalid");
        check(!isValidRcwaLayer(RcwaLayer{ 1.0, 1.0, 1.5, 0.0 }),
              "rcwa fill > 1 invalid");
        check(!isValidRcwaLayer(RcwaLayer{ 1.0, 1.0, -0.1, 0.0 }),
              "rcwa fill < 0 invalid");
        check(isValidRcwaLayer(RcwaLayer{ 1.0, 1.0, 0.0, 0.0 }),
              "rcwa fill 0 valid");
        check(isValidRcwaLayer(RcwaLayer{ 1.0, 1.0, 1.0, 0.0 }),
              "rcwa fill 1 valid");
        check(!isValidRcwaLayer(RcwaLayer{ 1.0, 1.0, 0.5, -1.0 }),
              "rcwa negative thickness invalid");
        check(!isValidRcwaStack({}), "rcwa empty stack invalid");
        check(isValidRcwaStack({ RcwaLayer{ 1.0, 1.0, 0.5, 0.0 },
                                 RcwaLayer{ 2.25, 2.25, 0.5, 0.0 } }),
              "rcwa two-layer stack valid");
        check(!isValidRcwaStack({ RcwaLayer{ 1.0, 1.0, 0.5, 0.0 },
                                  RcwaLayer{ 0.0, 2.25, 0.5, 0.0 } }),
              "rcwa stack with one bad layer invalid");
    }

    // (b) solver が RCWA 以外 / 層リストが空 → 出力は従来とバイト一致
    {
        Project p;
        const QString base = OfdIO::serialize(p);
        check(!base.contains("rcwa"), "rcwa: no rcwa line by default");
        check(base.startsWith("OpenFDTD 4 2\n"), "rcwa: default header kept");

        OpticalOpts &o = p.optical();
        // 層だけ入れても solver が FDTD なら出力は変わらない
        o.rcwaLayerList = { RcwaLayer{ 1.0, 1.0, 0.5, 0.0 },
                            RcwaLayer{ 2.25, 2.25, 0.5, 0.0 } };
        check(OfdIO::serialize(p) == base,
              "rcwa: layers without RCWA solver keep output byte-identical");

        // solver だけ RCWA にしても層が空なら出力は変わらない
        o.rcwaLayerList.clear();
        o.solver = OpticalSolver::RCWA;
        check(OfdIO::serialize(p) == base,
              "rcwa: RCWA solver with empty stack keeps output byte-identical");

        // 不正な層が混ざっていても出力は変わらない (カーネルへ渡さない)
        o.rcwaLayerList = { RcwaLayer{ 1.0, 1.0, 0.5, 0.0 },
                            RcwaLayer{ -1.0, 2.25, 0.5, 0.0 } };
        check(OfdIO::serialize(p) == base,
              "rcwa: invalid layer keeps output byte-identical");

        // 元に戻せば完全にバイト一致 (後方互換)
        o.rcwaLayerList.clear();
        o.solver = OpticalSolver::FDTD;
        check(OfdIO::serialize(p) == base,
              "rcwa: disabled output byte-identical to legacy");
    }

    // (d) 出力書式と nm → m 換算 (600 nm → 6e-07)
    {
        Project p;
        OpticalOpts &o = p.optical();
        o.solver = OpticalSolver::RCWA;
        o.rcwaNx = 5;
        o.rcwaPeriodX = 300.0;          // nm → 3e-07 m
        o.rcwaPeriodY = 600.0;          // 無視される (orcwa は 1D)
        o.rcwaLayerList = { RcwaLayer{ 1.0, 1.0, 0.5, 0.0 },
                            RcwaLayer{ 4.0, 1.0, 0.5, 200.0 },
                            RcwaLayer{ 2.25, 2.25, 0.5, 0.0 } };
        const QString out = OfdIO::serialize(p);
        check(out.startsWith("OpenRCWA 4 2\n"),
              "rcwa: header switched to OpenRCWA");
        check(out.contains("\nrcwa = 5 3e-07\n"), "rcwa: rcwa line emitted");
        check(out.contains("\nrcwalayer = 1 1 0.5 0\n"),
              "rcwa: incident layer emitted");
        check(out.contains("\nrcwalayer = 4 1 0.5 2e-07\n"),
              "rcwa: grating layer thickness nm -> m");
        check(out.contains("\nrcwalayer = 2.25 2.25 0.5 0\n"),
              "rcwa: exit layer emitted");
        // 600 nm → 6e-07 m
        o.rcwaPeriodX = 600.0;
        check(OfdIO::serialize(p).contains("\nrcwa = 5 6e-07\n"),
              "rcwa: 600 nm period serializes as 6e-07 m");
    }

    // (e) FMM は RCWA と同一手法 (Fourier Modal Method) — orcwa を共用し、
    //     調和次数だけ fmmHarmonics を使う。カーネル選択も RCWA と同じ。
    {
        Project p;
        OpticalOpts &o = p.optical();
        o.solver = OpticalSolver::FMM;
        o.fmmHarmonics = 9;
        o.rcwaNx = 5;                   // FMM 選択時は使われない
        o.rcwaPeriodX = 300.0;
        o.rcwaLayerList = { RcwaLayer{ 1.0, 1.0, 0.5, 0.0 },
                            RcwaLayer{ 4.0, 1.0, 0.5, 200.0 },
                            RcwaLayer{ 2.25, 2.25, 0.5, 0.0 } };
        const QString out = OfdIO::serialize(p);
        check(out.startsWith("OpenRCWA 4 2\n"),
              "fmm: header switched to OpenRCWA");
        check(out.contains("\nrcwa = 9 3e-07\n"),
              "fmm: harmonics taken from fmmHarmonics");
        p.setActiveDomain(Domain::Optical);
        check(Runner::kernelForProject(p) == Kernel::RCWA,
              "fmm: kernel resolves to orcwa (RCWA)");

        // 層スタックが空なら従来出力とバイト一致 (実行前ゲートは
        // MainWindow 側で警告するため、書き出しは何も加えない)
        o.rcwaLayerList.clear();
        Project legacy;
        legacy.optical().solver = OpticalSolver::FMM;
        legacy.optical().fmmHarmonics = 9;
        check(OfdIO::serialize(p) == OfdIO::serialize(legacy),
              "fmm: empty stack keeps output byte-identical");
    }

    // (a) 層あり .ofd のラウンドトリップ
    {
        const QString text =
            "OpenRCWA 4 2\n"
            "rcwa = 7 3.0e-7\n"
            "rcwalayer = 1.0 1.0 0.5 0\n"
            "rcwalayer = 4.0 1.0 0.4 2.0e-7\n"
            "rcwalayer = 2.25 2.25 0.5 0\n"
            "end\n";
        Project p;
        QString err;
        check(OfdIO::parse(text, p, &err), "rcwa: parse OpenRCWA header file");
        const OpticalOpts &o = p.optical();
        check(o.solver == OpticalSolver::RCWA, "rcwa: solver switched to RCWA");
        check(o.rcwaNx == 7, "rcwa: harmonics parsed");
        check(nearlyEq(o.rcwaPeriodX, 300.0), "rcwa: period m -> nm");
        check(o.rcwaLayerList.size() == 3, "rcwa: three layers parsed");
        if (o.rcwaLayerList.size() == 3) {
            check(nearlyEq(o.rcwaLayerList[1].eps1, 4.0) &&
                  nearlyEq(o.rcwaLayerList[1].eps2, 1.0) &&
                  nearlyEq(o.rcwaLayerList[1].fill, 0.4) &&
                  nearlyEq(o.rcwaLayerList[1].thickness_nm, 200.0),
                  "rcwa: grating layer parsed (thickness m -> nm)");
            check(nearlyEq(o.rcwaLayerList[2].eps1, 2.25),
                  "rcwa: exit layer parsed");
        }
        check(p.extraLines().isEmpty(),
              "rcwa: rcwa keys not duplicated into extraLines");

        // 再シリアライズ → 再パースで一致
        const QString out = OfdIO::serialize(p);
        check(out.contains("\nrcwa = 7 3e-07\n") &&
              out.contains("\nrcwalayer = 4 1 0.4 2e-07\n"),
              "rcwa: reserialize keeps rcwa/rcwalayer");
        Project p2;
        check(OfdIO::parse(out, p2, &err), "rcwa: reparse");
        compareProjects(p, p2);
    }

    // (c) .ofdx ラウンドトリップ + 旧ファイル互換
    {
        Project p1;
        OpticalOpts &o = p1.optical();
        o.solver = OpticalSolver::RCWA;
        o.rcwaLayerList = { RcwaLayer{ 1.0, 1.0, 0.5, 0.0 },
                            RcwaLayer{ 12.25, 2.1, 0.35, 220.0 },
                            RcwaLayer{ 2.25, 2.25, 0.5, 0.0 } };

        QTemporaryFile ofdx;
        ofdx.setFileTemplate(QDir::tempPath() + "/ofdx_rcwa_XXXXXX.ofdx");
        if (ofdx.open()) {
            check(OfdxIO::save(ofdx.fileName(), p1), "rcwa ofdx save");
            Project p2;
            check(OfdxIO::load(ofdx.fileName(), p2), "rcwa ofdx load");
            const OpticalOpts &q = p2.optical();
            check(q.solver == OpticalSolver::RCWA, "rcwa ofdx solver");
            check(q.rcwaLayerList.size() == 3, "rcwa ofdx layer count");
            if (q.rcwaLayerList.size() == 3)
                check(nearlyEq(q.rcwaLayerList[1].eps1, 12.25) &&
                      nearlyEq(q.rcwaLayerList[1].eps2, 2.1) &&
                      nearlyEq(q.rcwaLayerList[1].fill, 0.35) &&
                      nearlyEq(q.rcwaLayerList[1].thickness_nm, 220.0),
                      "rcwa ofdx layer round-trip");

            QFile jf(ofdx.fileName());
            check(jf.open(QIODevice::ReadOnly), "rcwa ofdx reopen");
            const QJsonObject rc = QJsonDocument::fromJson(jf.readAll())
                                       .object().value("optical").toObject()
                                       .value("rcwa").toObject();
            check(rc.contains("nx") && rc.contains("ny") &&
                  rc.contains("period_x_nm") && rc.contains("period_y_nm") &&
                  rc.contains("layers"),
                  "rcwa json keeps existing rcwa keys");
            check(rc.value("layer_list").toArray().size() == 3,
                  "rcwa json layer_list key");
        }

        // 旧 .ofdx (layer_list 無し): 空リストのまま (旧ファイル互換)
        QTemporaryFile old;
        old.setFileTemplate(QDir::tempPath() + "/ofdx_rcwa_old_XXXXXX.ofdx");
        if (old.open()) {
            const QByteArray legacy =
                "{ \"schemaVersion\": \"1.0\", \"domain\": \"optical\","
                "  \"optical\": { \"solver\": 1,"
                "     \"rcwa\": { \"nx\": 9, \"period_x_nm\": 450 } } }";
            old.write(legacy);
            old.flush();
            Project p3;
            check(OfdxIO::load(old.fileName(), p3), "rcwa legacy ofdx load");
            const OpticalOpts &q = p3.optical();
            check(q.rcwaNx == 9 && nearlyEq(q.rcwaPeriodX, 450.0),
                  "rcwa legacy ofdx keys still load");
            check(q.rcwaLayerList.isEmpty(),
                  "rcwa legacy ofdx leaves layer list empty");
            check(OfdIO::serialize(p3) == OfdIO::serialize(Project()),
                  "rcwa legacy project serializes byte-identically");
        }
    }
}

// 光解析モード別設定 (BPF 設計目標 / Ring ポート / 導波路 / MZI /
// メタサーフェス / PhC / NF2FF / S パラメータ) の .ofdx 永続化。
// これらは .ofd (カーネル入力) には出力しない — 出力バイト不変を併せて検証。
// ── 実材料分散 (src/optics/MaterialDispersion) ──────────────────────────────
// 公刊 Sellmeier 係数が既知の実測値を再現すること、有効範囲外を評価しないこと、
// 熱光学補正が線形であること、MaterialExplorerTab からの抽出前後で
// 同じ λ に対し同じ n を返すことを検証する。
static void testOpticsMaterials()
{
    using namespace ofd::optics;
    g_file = "optics-dispersion";

    // 抽出前 (MaterialExplorerTab の file-local テーブル) と同じ評価式を
    // テスト側に独立に書き、同一 λ で一致することを確認する
    struct OldEntry {
        const char *id;
        double A, B[3], C[3], D, E;
    };
    const OldEntry kOld[] = {
        { "SiO2", 1.0, { 0.6961663, 0.4079426, 0.8974794 },
          { 0.004679148, 0.013512063, 97.934003 }, 0, 0 },
        { "Si3N4", 1.0, { 3.0249, 40314.0, 0.0 },
          { 0.018317068, 1537208.2, 1.0 }, 0, 0 },
        { "Al2O3", 1.0, { 1.4313493, 0.65054713, 5.3414021 },
          { 0.005279925, 0.014238264, 325.01783 }, 0, 0 },
        { "Si", 1.0, { 10.6684293, 0.0030434748, 1.54133408 },
          { 0.090912190, 1.2876602, 1218816.0 }, 0, 0 },
        { "TiO2", 5.913, { 0, 0, 0 }, { 1, 1, 1 }, 0.2441, 0.0803 },
        { "PMMA", 1.0, { 1.1819, 0, 0 }, { 0.011313, 1.0, 1.0 }, 0, 0 },
    };
    auto oldN = [](const OldEntry &e, double um) {
        const double l2 = um * um;
        double n2 = e.A;
        for (int i = 0; i < 3; ++i)
            if (e.B[i] != 0.0) n2 += e.B[i] * l2 / (l2 - e.C[i]);
        if (e.D != 0.0) n2 += e.D / (l2 - e.E);
        return (n2 > 0.0) ? std::sqrt(n2) : 0.0;
    };

    check(materials().size() >= 7, "material table populated");
    check(findMaterial("SiO2") != nullptr, "findMaterial(SiO2)");
    check(findMaterial("NoSuchMaterial") == nullptr, "findMaterial(unknown)");
    check(findMaterial(nullptr) == nullptr, "findMaterial(nullptr)");

    // ── 既知の実測値との一致 ────────────────────────────────────────────
    double n = 0.0;
    // SiO2 (合成石英) d線 587.56 nm: 文献値 n = 1.45846 (Malitson 1965)
    check(refractiveIndex("SiO2", 0.58756, n), "SiO2 in range");
    check(std::fabs(n - 1.45846) < 2e-4, "SiO2 n=1.4585 @589nm");
    // Si 1550 nm: Salzberg-Villa の式は n = 3.4777
    check(refractiveIndex("Si", 1.55, n), "Si in range");
    check(std::fabs(n - 3.4777) < 2e-3, "Si n=3.478 @1550nm");
    // Si3N4 1550 nm: Luke (2015) の式は n = 1.9963
    check(refractiveIndex("Si3N4", 1.55, n), "Si3N4 in range");
    check(std::fabs(n - 1.9963) < 1e-3, "Si3N4 n=1.996 @1550nm");
    // Al2O3 (サファイア常光) 587.56 nm: 文献値 n = 1.7682
    check(refractiveIndex("Al2O3", 0.58756, n), "Al2O3 in range");
    check(std::fabs(n - 1.7682) < 5e-4, "Al2O3 n=1.768 @589nm");
    // PMMA d線: 文献値 nd = 1.4906
    check(refractiveIndex("PMMA", 0.58756, n), "PMMA in range");
    check(std::fabs(n - 1.4906) < 5e-4, "PMMA nd=1.4906");
    // LiNbO3 異常光線 1550 nm: Zelmon (1997) の式は ne = 2.1376
    // (文献の実測 ne ≈ 2.138 と 3 桁目まで一致)
    check(refractiveIndex("LiNbO3_e", 1.55, n), "LiNbO3_e in range");
    check(std::fabs(n - 2.1376) < 5e-4, "LiNbO3 ne=2.138 @1550nm");
    // 空気は n = 1 (無分散)
    double nAir1 = 0.0, nAir2 = 0.0;
    check(refractiveIndex("Air", 0.5, nAir1) && refractiveIndex("Air", 5.0, nAir2),
          "Air in range");
    check(nAir1 == 1.0 && nAir2 == 1.0, "Air n=1 (no dispersion)");
    // 正常分散 (短波長ほど n が大きい)
    double na = 0.0, nb = 0.0;
    check(refractiveIndex("SiO2", 0.4, na) && refractiveIndex("SiO2", 1.5, nb)
          && na > nb, "SiO2 normal dispersion");

    // ── 有効範囲外は false を返し、渡した変数を書き換えない ──────────────
    const double kSentinel = -12345.0;
    double v = kSentinel;
    check(!refractiveIndex("SiO2", 0.1, v), "SiO2 below range -> false");
    check(v == kSentinel, "value untouched below range");
    check(!refractiveIndex("SiO2", 5.0, v), "SiO2 above range -> false");
    check(v == kSentinel, "value untouched above range");
    check(!refractiveIndex("Si", 1.0, v), "Si below range -> false");
    check(v == kSentinel, "value untouched (Si)");
    check(!refractiveIndex("NoSuchMaterial", 1.55, v), "unknown id -> false");
    check(v == kSentinel, "value untouched (unknown id)");
    bool applied = true;
    check(!refractiveIndexAt("SiO2", 0.1, 25.0, v, applied),
          "refractiveIndexAt out of range -> false");
    check(v == kSentinel && applied, "value/flag untouched out of range");

    // ── 温度補正: n(T) − n(T_ref) = dn/dT·(T − T_ref) ────────────────────
    for (const MaterialInfo &m : materials()) {
        if (!m.hasDnDt) continue;
        const double lam = 0.5 * (std::max(m.lmin_um, 0.3)
                                  + std::min(m.lmax_um, 2.0));
        double n0 = 0.0, nT = 0.0;
        bool a0 = false, aT = false;
        const bool ok0 = refractiveIndexAt(m.id, lam, m.tRef_C, n0, a0);
        const bool okT = refractiveIndexAt(m.id, lam, m.tRef_C + 40.0, nT, aT);
        check(ok0 && okT && a0 && aT, "dn/dT material: tempApplied");
        double nRef = 0.0;
        check(refractiveIndex(m.id, lam, nRef) && std::fabs(n0 - nRef) < 1e-15,
              "n(T_ref) == n(λ)");
        check(std::fabs((nT - n0) - m.dnDt_perK * 40.0) < 1e-12,
              "linear thermo-optic shift");
    }
    // Si: dn/dT = 1.86e-4 /K (Cocorullo & Rendina 1992)
    const MaterialInfo *si = findMaterial("Si");
    check(si && si->hasDnDt && std::fabs(si->dnDt_perK - 1.86e-4) < 1e-12,
          "Si dn/dT = 1.86e-4 /K");
    double nSi25 = 0.0, nSi75 = 0.0;
    bool aSi = false;
    check(refractiveIndexAt("Si", 1.55, si->tRef_C, nSi25, aSi)
          && refractiveIndexAt("Si", 1.55, si->tRef_C + 50.0, nSi75, aSi),
          "Si temp eval");
    check(std::fabs((nSi75 - nSi25) - 50.0 * 1.86e-4) < 1e-12, "Si Δn @+50K");

    // ── dn/dT 未定義の材料は温度を反映しない (0 で埋めない) ───────────────
    const MaterialInfo *tio2 = findMaterial("TiO2");
    check(tio2 && !tio2->hasDnDt, "TiO2 dn/dT undefined");
    double nT0 = 0.0, nT1 = 0.0;
    bool aT0 = true, aT1 = true;
    check(refractiveIndexAt("TiO2", 0.8, 25.0, nT0, aT0)
          && refractiveIndexAt("TiO2", 0.8, 125.0, nT1, aT1),
          "TiO2 eval with temperature");
    check(!aT0 && !aT1, "TiO2 tempApplied=false");
    check(nT0 == nT1, "TiO2 n unchanged by temperature");
    const MaterialInfo *air = findMaterial("Air");
    check(air && !air->hasDnDt, "Air dn/dT undefined");

    // ── 抽出前後の一致 (同じ λ で同じ n) ─────────────────────────────────
    for (const OldEntry &o : kOld) {
        const MaterialInfo *m = findMaterial(o.id);
        check(m != nullptr, "extracted material present");
        if (!m) continue;
        for (int i = 0; i < 5; ++i) {
            const double lam = m->lmin_um
                + (m->lmax_um - m->lmin_um) * (i + 0.5) / 5.0;
            double got = 0.0;
            const bool ok = refractiveIndex(o.id, lam, got);
            check(ok && std::fabs(got - oldN(o, lam)) < 1e-12,
                  "extraction preserves n(λ)");
        }
    }
}

// ── 多層薄膜 特性行列法 (src/optics/ThinFilmStack) ──────────────────────────
// 期待値はすべて解析解としてこのテスト側に独立に書く:
//   - 膜なし界面 = フレネルの式 (垂直/斜入射, s/p 両偏波)
//   - ブルースター角で Rp = 0
//   - 単層無反射膜 n_f = √(n0·ns) の λ/4 で R = 0 (厳密)
//   - 半波長 (absentee) 膜は基板だけの R に一致
//   - 四分の一波長積層 (HL)^N H は Y = (nH/nL)^{2N}·nH²/ns の閉形式
//   - 全反射条件で R = 1, T = 0
//   - 無損失系で R + T = 1 (エネルギー保存)
static void testThinFilmStack()
{
    using namespace ofd::optics;
    g_file = "thinfilm-tmm";
    const double kPi = 3.14159265358979323846;

    // ── (1) 膜なし界面 = フレネル (垂直入射) ────────────────────────────
    {
        const double n0 = 1.0, ns = 1.52;
        const FilmResponse r = filmResponse(n0, {}, ns, 0.0, 550.0, 0.0, Pol::S);
        const double rf = (n0 - ns) / (n0 + ns);
        check(r.valid, "tmm: bare interface valid");
        check(std::fabs(r.R - rf * rf) < 1e-12, "tmm: bare R = Fresnel");
        check(std::fabs(r.R + r.T - 1.0) < 1e-12, "tmm: bare R+T = 1");
        check(r.A < 1e-12, "tmm: bare A = 0 (lossless)");
    }

    // ── (2) 斜入射フレネル (s / p) + エネルギー保存 ──────────────────────
    for (double aoi : { 15.0, 45.0, 70.0 }) {
        const double n0 = 1.0, ns = 1.52;
        const double th = aoi * kPi / 180.0;
        const double c0 = std::cos(th);
        const double ct = std::sqrt(1.0 - std::pow(n0 * std::sin(th) / ns, 2));
        const double rs = (n0 * c0 - ns * ct) / (n0 * c0 + ns * ct);
        const double rp = (ns * c0 - n0 * ct) / (ns * c0 + n0 * ct);
        const FilmResponse s = filmResponse(n0, {}, ns, 0.0, 550.0, aoi, Pol::S);
        const FilmResponse p = filmResponse(n0, {}, ns, 0.0, 550.0, aoi, Pol::P);
        check(std::fabs(s.R - rs * rs) < 1e-12, "tmm: oblique Rs = Fresnel");
        check(std::fabs(p.R - rp * rp) < 1e-12, "tmm: oblique Rp = Fresnel");
        check(std::fabs(s.R + s.T - 1.0) < 1e-12, "tmm: oblique s energy");
        check(std::fabs(p.R + p.T - 1.0) < 1e-12, "tmm: oblique p energy");
    }

    // ── (3) ブルースター角 θ_B = atan(ns/n0) で Rp = 0 ───────────────────
    {
        const double n0 = 1.0, ns = 1.52;
        const double b = std::atan(ns / n0) * 180.0 / kPi;
        const FilmResponse p = filmResponse(n0, {}, ns, 0.0, 550.0, b, Pol::P);
        check(p.valid && p.R < 1e-20, "tmm: Rp = 0 at Brewster angle");
        const FilmResponse s = filmResponse(n0, {}, ns, 0.0, 550.0, b, Pol::S);
        check(s.R > 0.1, "tmm: Rs finite at Brewster angle");
    }

    // ── (4) 単層無反射膜 n_f = √(n0·ns) を λ/4 → R = 0 (厳密) ────────────
    {
        const double n0 = 1.0, ns = 2.25, lam = 550.0;
        const double nf = std::sqrt(n0 * ns);
        const std::vector<FilmLayer> ls{ { nf, 0.0, lam / (4.0 * nf) } };
        const FilmResponse r = filmResponse(n0, ls, ns, 0.0, lam, 0.0, Pol::S);
        check(r.valid && r.R < 1e-20, "tmm: quarter-wave AR gives R = 0");
        check(std::fabs(r.T - 1.0) < 1e-12, "tmm: quarter-wave AR gives T = 1");
        // 設計波長から外すと R は増える (最小値であることの確認)
        const FilmResponse off = filmResponse(n0, ls, ns, 0.0, lam * 1.1, 0.0,
                                              Pol::S);
        check(off.valid && off.R > r.R, "tmm: AR is a minimum at λ0");
    }

    // ── (5) 半波長 (absentee) 膜は基板だけの R に一致 ─────────────────────
    //      斜入射では傾斜光学膜厚 d·√(n² − n0²sin²θ) = λ/2
    for (double aoi : { 0.0, 30.0 }) {
        const double n0 = 1.0, ns = 1.52, lam = 632.8, nf = 2.3;
        const double q = std::sqrt(nf * nf
                                   - std::pow(n0 * std::sin(aoi * kPi / 180.0), 2));
        const std::vector<FilmLayer> ls{ { nf, 0.0, lam / (2.0 * q) } };
        for (Pol pol : { Pol::S, Pol::P }) {
            const FilmResponse a = filmResponse(n0, ls, ns, 0.0, lam, aoi, pol);
            const FilmResponse b = filmResponse(n0, {}, ns, 0.0, lam, aoi, pol);
            check(a.valid && b.valid && std::fabs(a.R - b.R) < 1e-12,
                  "tmm: half-wave layer is absentee");
        }
    }

    // ── (6) 四分の一波長積層 (HL)^N H の閉形式 ───────────────────────────
    //      Y = (nH/nL)^{2N}·nH²/ns,  R = ((n0 − Y)/(n0 + Y))²
    {
        const double n0 = 1.0, ns = 1.52, lam = 1550.0, nH = 2.0, nL = 1.46;
        const int N = 6;
        std::vector<FilmLayer> ls;
        for (int i = 0; i < N; ++i) {
            ls.push_back({ nH, 0.0, lam / (4.0 * nH) });
            ls.push_back({ nL, 0.0, lam / (4.0 * nL) });
        }
        ls.push_back({ nH, 0.0, lam / (4.0 * nH) });
        const FilmResponse r = filmResponse(n0, ls, ns, 0.0, lam, 0.0, Pol::S);
        const double Y = std::pow(nH / nL, 2 * N) * nH * nH / ns;
        const double Rex = std::pow((n0 - Y) / (n0 + Y), 2);
        check(r.valid && std::fabs(r.R - Rex) < 1e-10,
              "tmm: quarter-wave stack matches closed form");
        check(std::fabs(r.R + r.T - 1.0) < 1e-12, "tmm: stack energy R+T=1");
        check(r.R > 0.96, "tmm: 13-layer QW stack is a high reflector");
    }

    // ── (7) 全反射 (n0 > ns、臨界角超) → R = 1, T = 0 ────────────────────
    {
        const double n0 = 1.5, ns = 1.0;
        const double crit = std::asin(ns / n0) * 180.0 / kPi;
        for (Pol pol : { Pol::S, Pol::P }) {
            const FilmResponse r = filmResponse(n0, {}, ns, 0.0, 550.0,
                                                crit + 10.0, pol);
            check(r.valid && std::fabs(r.R - 1.0) < 1e-12, "tmm: TIR R = 1");
            // 厳密に 0 であること (処理系の丸め残差に依存しない実装を要求する
            // — Apple clang で Re(ηs) に微小な非零が残り macOS CI が落ちた)
            check(r.T == 0.0, "tmm: TIR T = 0");
            check(r.A == 0.0, "tmm: TIR は吸収も 0");
            // 臨界角の直上でも同じ (境界付近で分岐が崩れないこと)
            const FilmResponse e = filmResponse(n0, {}, ns, 0.0, 550.0,
                                                crit + 1e-6, pol);
            check(e.valid && e.T == 0.0, "tmm: 臨界角直上でも T = 0");
        }
        // 臨界角未満では透過する
        const FilmResponse t = filmResponse(n0, {}, ns, 0.0, 550.0,
                                            crit - 10.0, Pol::S);
        check(t.valid && t.T > 0.0, "tmm: below critical angle T > 0");
    }

    // ── (8) 吸収層: A > 0 かつ R + T + A = 1 ─────────────────────────────
    {
        const std::vector<FilmLayer> ls{ { 2.0, 0.05, 200.0 } };
        const FilmResponse r = filmResponse(1.0, ls, 1.52, 0.0, 550.0, 20.0,
                                            Pol::P);
        check(r.valid && r.A > 1e-3, "tmm: absorbing layer A > 0");
        check(std::fabs(r.R + r.T + r.A - 1.0) < 1e-12, "tmm: R+T+A = 1");
        // k を厚くするほど吸収は増える (単調性)
        const std::vector<FilmLayer> ls2{ { 2.0, 0.10, 200.0 } };
        const FilmResponse r2 = filmResponse(1.0, ls2, 1.52, 0.0, 550.0, 20.0,
                                             Pol::P);
        check(r2.A > r.A, "tmm: absorptance increases with k");
    }

    // ── (9) 不正入力は valid = false ─────────────────────────────────────
    {
        check(!filmResponse(1.0, {}, 1.5, 0.0, 0.0, 0.0, Pol::S).valid,
              "tmm: λ = 0 rejected");
        check(!filmResponse(1.0, {}, 1.5, 0.0, 550.0, 90.0, Pol::S).valid,
              "tmm: grazing incidence rejected");
        check(!filmResponse(1.0, {}, 1.5, 0.0, 550.0, -1.0, Pol::S).valid,
              "tmm: negative angle rejected");
        check(!filmResponse(0.0, {}, 1.5, 0.0, 550.0, 0.0, Pol::S).valid,
              "tmm: n0 = 0 rejected");
    }

    // ── (10) spectrum(): 点数・群遅延・範囲外 λ の除外 ───────────────────
    {
        const StackAtLambda bare = [](double lam, StackSample &s) {
            (void)lam;
            s.n0 = 1.0; s.nsub = 1.52; s.ksub = 0.0;
            return true;
        };
        const std::vector<SpectrumPoint> sp = spectrum(bare, 500, 600, 11, 0.0,
                                                       true);
        check(sp.size() == 11, "tmm: spectrum point count");
        check(sp.front().lambda_nm == 500.0 && sp.back().lambda_nm == 600.0,
              "tmm: spectrum endpoints");
        bool gdZero = true, ascending = true;
        for (size_t i = 0; i < sp.size(); ++i) {
            if (!sp[i].gdValid || std::fabs(sp[i].gds_ps) > 1e-9) gdZero = false;
            if (i && sp[i].lambda_nm <= sp[i - 1].lambda_nm) ascending = false;
        }
        // 膜が無ければ反射位相は波長に依らないので群遅延は 0
        check(gdZero, "tmm: bare interface group delay = 0");
        check(ascending, "tmm: spectrum λ ascending");

        // 材料データの有効範囲外は外挿せず除外する
        const StackAtLambda limited = [](double lam, StackSample &s) {
            if (lam < 550.0) return false;
            s.n0 = 1.0; s.nsub = 1.52;
            return true;
        };
        const std::vector<SpectrumPoint> lim = spectrum(limited, 500, 600, 11,
                                                        0.0, false);
        check(lim.size() == 6, "tmm: out-of-range λ dropped, not extrapolated");
        check(lim.front().lambda_nm == 550.0, "tmm: first in-range λ");
    }

    // ── (11) angleSweep(): filmResponse と一致 ───────────────────────────
    {
        const StackAtLambda bare = [](double lam, StackSample &s) {
            (void)lam;
            s.n0 = 1.0; s.nsub = 1.52;
            return true;
        };
        const std::vector<AnglePoint> ap = angleSweep(bare, 550.0, 0.0, 60.0, 61);
        check(ap.size() == 61, "tmm: angle sweep point count");
        if (ap.size() == 61) {
            const FilmResponse r0 = filmResponse(1.0, {}, 1.52, 0.0, 550.0, 30.0,
                                                 Pol::S);
            check(std::fabs(ap[30].aoi_deg - 30.0) < 1e-12
                  && std::fabs(ap[30].Rs - r0.R) < 1e-12,
                  "tmm: angle sweep matches filmResponse");
            check(ap.back().Rs > ap.front().Rs,
                  "tmm: s-reflectance grows with angle");
        }
    }

    // ── (12) メリット関数 ────────────────────────────────────────────────
    {
        const double n0 = 1.0, ns = 2.25, lam = 550.0;
        const double nf = std::sqrt(n0 * ns);
        const StackAtLambda ar = [=](double l, StackSample &s) {
            (void)l;
            s.n0 = n0; s.nsub = ns;
            s.layers.push_back({ nf, 0.0, lam / (4.0 * nf) });
            return true;
        };
        std::vector<TargetBand> t(1);
        t[0].lam0_nm = lam; t[0].lam1_nm = lam; t[0].samples = 1;
        t[0].q = Quantity::R; t[0].goal = 0.0; t[0].tol = 0.005; t[0].weight = 1.0;
        const MeritResult m = merit(ar, t, 0.0);
        // 設計波長で R = 0 (目標一致) なので F = 0
        check(m.valid && m.used == 1 && m.merit < 1e-9,
              "tmm: merit = 0 when the goal is met exactly");

        // 目標を 100 % にすると F = |0 − 1|/tol = 1/0.005 = 200
        t[0].goal = 1.0;
        const MeritResult m2 = merit(ar, t, 0.0);
        check(m2.valid && std::fabs(m2.merit - 200.0) < 1e-6,
              "tmm: merit = |Q−goal|/tol");
        // 許容差を 2 倍にすると F は半分
        t[0].tol = 0.010;
        const MeritResult m3 = merit(ar, t, 0.0);
        check(m3.valid && std::fabs(m3.merit - 100.0) < 1e-6,
              "tmm: merit halves when the tolerance doubles");
        // 評価できる λ が無ければ valid = false (0 を返さない)
        const StackAtLambda none = [](double, StackSample &) { return false; };
        const MeritResult m4 = merit(none, t, 0.0);
        check(!m4.valid && m4.skipped == 1, "tmm: merit invalid when no λ usable");
    }

    // ── (13) 膜厚感度 ────────────────────────────────────────────────────
    {
        // 入射媒質・層・基板がすべて n = 1 なら、膜厚を変えても R は変わらない
        const StackAtLambda flat = [](double l, StackSample &s) {
            (void)l;
            s.n0 = 1.0; s.nsub = 1.0;
            s.layers.push_back({ 1.0, 0.0, 100.0 });
            return true;
        };
        std::vector<TargetBand> t(1);
        t[0].lam0_nm = 500; t[0].lam1_nm = 600; t[0].samples = 5;
        t[0].goal = 0.0; t[0].tol = 0.01; t[0].weight = 1.0;
        const SensitivityResult s0 = thicknessSensitivity(flat, t, 0.0, 0.5);
        check(s0.valid && s0.dQ_pctPerNm.size() == 1
              && s0.dQ_pctPerNm[0] < 1e-12,
              "tmm: index-matched layer has zero thickness sensitivity");

        // 高屈折率層 + 「基板と同じ屈折率の層」の 2 層。後者は基板の一部と
        // 等価なので、厚みを変えても R は変わらない (感度 0)
        const StackAtLambda two = [](double l, StackSample &s) {
            (void)l;
            s.n0 = 1.0; s.nsub = 1.5;
            s.layers.push_back({ 2.35, 0.0, 60.0 });   // 高屈折率
            s.layers.push_back({ 1.5,  0.0, 80.0 });   // 基板と同じ n
            return true;
        };
        const SensitivityResult s1 = thicknessSensitivity(two, t, 0.0, 0.5);
        check(s1.valid && s1.dQ_pctPerNm.size() == 2, "tmm: sensitivity size");
        if (s1.dQ_pctPerNm.size() == 2) {
            check(s1.dQ_pctPerNm[0] > 1e-3, "tmm: high-index layer is sensitive");
            check(s1.dQ_pctPerNm[1] < 1e-12,
                  "tmm: index-matched layer is insensitive");
            check(s1.worst == 0, "tmm: worst layer identified");
        }
    }

    // ── (14) 製造誤差モンテカルロ ────────────────────────────────────────
    {
        const double lam = 550.0, ns = 2.25;
        const double nf = std::sqrt(ns);
        const StackAtLambda ar = [=](double l, StackSample &s) {
            (void)l;
            s.n0 = 1.0; s.nsub = ns;
            s.layers.push_back({ nf, 0.0, lam / (4.0 * nf) });
            return true;
        };
        std::vector<TargetBand> t(1);
        t[0].lam0_nm = 540; t[0].lam1_nm = 560; t[0].samples = 5;
        t[0].goal = 0.0; t[0].tol = 0.005; t[0].weight = 1.0;

        ToleranceOptions o;
        o.trials = 200;
        o.sigmaRel = 0.0;
        const ToleranceResult r0 = monteCarlo(ar, t, 0.0, o);
        check(r0.valid && r0.trials == 200 && r0.passed == 200,
              "tmm: σ = 0 passes every trial");
        check(r0.yield == 1.0, "tmm: σ = 0 yield = 1");
        check(std::fabs(r0.meritMean - r0.meritNominal) < 1e-12,
              "tmm: σ = 0 mean merit equals nominal");

        o.sigmaRel = 0.05;
        const ToleranceResult r1 = monteCarlo(ar, t, 0.0, o);
        const ToleranceResult r2 = monteCarlo(ar, t, 0.0, o);
        check(r1.valid && r1.yield == r2.yield
              && r1.meritMean == r2.meritMean,
              "tmm: Monte Carlo is deterministic for a fixed seed");
        check(r1.meritMean >= r1.meritNominal,
              "tmm: perturbation cannot improve the mean merit");
        o.sigmaRel = 0.20;
        const ToleranceResult r3 = monteCarlo(ar, t, 0.0, o);
        check(r3.valid && r3.yield <= r1.yield,
              "tmm: yield decreases as the thickness error grows");
        check(r3.meritP90 >= r1.meritP90, "tmm: 90th percentile merit grows");
        // 評価できる λ が無ければ valid = false
        const StackAtLambda none = [](double, StackSample &) { return false; };
        check(!monteCarlo(none, t, 0.0, o).valid,
              "tmm: Monte Carlo invalid when no λ usable");
    }

    // ── (15) 多層・斜入射でもエネルギー保存 (無損失) ─────────────────────
    {
        std::vector<FilmLayer> ls;
        const double lam = 620.0;
        for (int i = 0; i < 5; ++i) {
            ls.push_back({ 2.35, 0.0, 30.0 + 7.0 * i });
            ls.push_back({ 1.46, 0.0, 55.0 + 3.0 * i });
        }
        for (double aoi : { 0.0, 25.0, 55.0, 80.0 })
            for (Pol pol : { Pol::S, Pol::P }) {
                const FilmResponse r = filmResponse(1.0, ls, 1.52, 0.0, lam,
                                                    aoi, pol);
                check(r.valid && std::fabs(r.R + r.T - 1.0) < 1e-12,
                      "tmm: lossless multilayer conserves energy");
                check(r.R >= 0.0 && r.R <= 1.0 && r.T >= 0.0 && r.T <= 1.0,
                      "tmm: R, T in [0,1]");
            }
    }
}

static void testOpticalModeSettings()
{
    g_file = "optical-modes";

    // 全フィールドを非既定値にした Project を作るヘルパー
    auto setNonDefaults = [](Project &p) {
        OpticalOpts &o = p.optical();
        o.bpfIL_dB = 1.5;
        o.bpfStop_dB = 55.0;
        o.ringThruPort = false;
        o.ringDropPort = false;
        o.wgTE0 = false; o.wgTE1 = true;
        o.wgTM0 = true;  o.wgTM1 = true;
        o.wgLoss_dBcm = 1.2;
        o.mziDeltaL_um = 75.5;
        o.mziThermo = false;
        o.mziElectro = true;
        o.metaPeriod_nm = 520.0;
        o.metaShape = 2;
        o.metaPhase = 1;
        o.phcLattice = 1;
        o.phcA_nm = 390.0;
        o.phcRoverA = 0.25;
        o.phcBand = false;
        o.phcDefect = true;
        o.nfffSurface = 1;
        o.nfffDistance_lambda = 250.0;
        o.spPorts = 4;
        o.spPortIn = 2;
        o.spPortOut = 4;
        o.spS11 = false;
        o.spS21 = false;
        o.spPhase = false;
        o.spGroupDelay = true;
    };

    // 1) これらの設定は .ofd (カーネル入力) を 1 バイトも変えない
    {
        Project p;
        const QString base = OfdIO::serialize(p);
        setNonDefaults(p);
        check(OfdIO::serialize(p) == base,
              "optmode: settings keep .ofd output byte-identical");
    }

    // 2) .ofdx ラウンドトリップ (a: 新キーの往復)
    {
        Project p1;
        setNonDefaults(p1);
        QTemporaryFile ofdx;
        ofdx.setFileTemplate(QDir::tempPath() + "/ofdx_optmode_XXXXXX.ofdx");
        if (ofdx.open()) {
            check(OfdxIO::save(ofdx.fileName(), p1), "optmode ofdx save");
            Project p2;
            check(OfdxIO::load(ofdx.fileName(), p2), "optmode ofdx load");
            const OpticalOpts &q = p2.optical();
            check(nearlyEq(q.bpfIL_dB, 1.5) && nearlyEq(q.bpfStop_dB, 55.0),
                  "optmode bpf il/stop round-trip");
            check(!q.ringThruPort && !q.ringDropPort,
                  "optmode ring ports round-trip");
            check(!q.wgTE0 && q.wgTE1 && q.wgTM0 && q.wgTM1 &&
                  nearlyEq(q.wgLoss_dBcm, 1.2),
                  "optmode waveguide round-trip");
            check(nearlyEq(q.mziDeltaL_um, 75.5) && !q.mziThermo &&
                  q.mziElectro, "optmode mzi round-trip");
            check(nearlyEq(q.metaPeriod_nm, 520.0) && q.metaShape == 2 &&
                  q.metaPhase == 1, "optmode metasurface round-trip");
            check(q.phcLattice == 1 && nearlyEq(q.phcA_nm, 390.0) &&
                  nearlyEq(q.phcRoverA, 0.25) && !q.phcBand && q.phcDefect,
                  "optmode phc round-trip");
            check(q.nfffSurface == 1 &&
                  nearlyEq(q.nfffDistance_lambda, 250.0),
                  "optmode nf2ff round-trip");
            check(q.spPorts == 4 && q.spPortIn == 2 && q.spPortOut == 4 &&
                  !q.spS11 && !q.spS21 && !q.spPhase && q.spGroupDelay,
                  "optmode sparam round-trip");

            // JSON: 既存キーが残り、新キーが追加されていること
            QFile jf(ofdx.fileName());
            check(jf.open(QIODevice::ReadOnly), "optmode ofdx reopen");
            const QJsonObject opt = QJsonDocument::fromJson(jf.readAll())
                                        .object().value("optical").toObject();
            const QJsonObject bpf = opt.value("bpf").toObject();
            check(bpf.contains("band_nm") && bpf.contains("Q"),
                  "optmode json keeps existing bpf keys");
            check(bpf.contains("il_db") && bpf.contains("stop_db"),
                  "optmode json bpf il/stop keys");
            const QJsonObject ring = opt.value("ring").toObject();
            check(ring.contains("radius_um") && ring.contains("gap_nm"),
                  "optmode json keeps existing ring keys");
            check(ring.contains("thru_port") && ring.contains("drop_port"),
                  "optmode json ring port keys");
            check(opt.contains("waveguide") && opt.contains("mzi") &&
                  opt.contains("metasurface") && opt.contains("phc") &&
                  opt.contains("nf2ff") && opt.contains("sparam"),
                  "optmode json mode-section keys present");
        }
    }

    // 3) 旧 .ofdx (新キー無し): 既定値のまま (旧ファイル互換, b)
    {
        QTemporaryFile old;
        old.setFileTemplate(QDir::tempPath() + "/ofdx_optmode_old_XXXXXX.ofdx");
        if (old.open()) {
            const QByteArray legacy =
                "{ \"schemaVersion\": \"1.0\", \"domain\": \"optical\","
                "  \"optical\": { \"solver\": 0,"
                "     \"bpf\": { \"band_nm\": [1530, 1570], \"Q\": 5000 },"
                "     \"ring\": { \"radius_um\": 8, \"gap_nm\": 150 } } }";
            old.write(legacy);
            old.flush();
            Project p;
            check(OfdxIO::load(old.fileName(), p), "optmode legacy ofdx load");
            const OpticalOpts &q = p.optical();
            // 既存キーは読み込まれる
            check(nearlyEq(q.bpfBandMin, 1530.0) &&
                  nearlyEq(q.bpfBandMax, 1570.0) && nearlyEq(q.bpfQ, 5000.0),
                  "optmode legacy bpf keys still load");
            check(nearlyEq(q.ringRadius_um, 8.0) &&
                  nearlyEq(q.ringGap_nm, 150.0),
                  "optmode legacy ring keys still load");
            // 新キーは既定値のまま
            check(q.bpfIL_dB == 0.5 && q.bpfStop_dB == 40.0,
                  "optmode legacy leaves bpf il/stop defaults");
            check(q.ringThruPort && q.ringDropPort,
                  "optmode legacy leaves ring port defaults");
            check(q.wgTE0 && !q.wgTE1 && !q.wgTM0 && !q.wgTM1 &&
                  q.wgLoss_dBcm == 0.3,
                  "optmode legacy leaves waveguide defaults");
            check(q.mziDeltaL_um == 50.0 && q.mziThermo && !q.mziElectro,
                  "optmode legacy leaves mzi defaults");
            check(q.metaPeriod_nm == 400.0 && q.metaShape == 0 &&
                  q.metaPhase == 0,
                  "optmode legacy leaves metasurface defaults");
            check(q.phcLattice == 0 && q.phcA_nm == 430.0 &&
                  q.phcRoverA == 0.30 && q.phcBand && !q.phcDefect,
                  "optmode legacy leaves phc defaults");
            check(q.nfffSurface == 0 && q.nfffDistance_lambda == 1000.0,
                  "optmode legacy leaves nf2ff defaults");
            check(q.spPorts == 2 && q.spPortIn == 1 && q.spPortOut == 2 &&
                  q.spS11 && q.spS21 && q.spPhase && !q.spGroupDelay,
                  "optmode legacy leaves sparam defaults");
        }
    }

    // 4) 壊れたファイルの範囲外値はコンボ index の範囲へクランプされる
    {
        QTemporaryFile bad;
        bad.setFileTemplate(QDir::tempPath() + "/ofdx_optmode_bad_XXXXXX.ofdx");
        if (bad.open()) {
            const QByteArray broken =
                "{ \"schemaVersion\": \"1.0\", \"domain\": \"optical\","
                "  \"optical\": {"
                "     \"metasurface\": { \"shape\": 99, \"phase\": -3 },"
                "     \"phc\": { \"lattice\": 7 },"
                "     \"nf2ff\": { \"surface\": 5 },"
                "     \"sparam\": { \"ports\": 0, \"port_in\": 99 } } }";
            bad.write(broken);
            bad.flush();
            Project p;
            check(OfdxIO::load(bad.fileName(), p), "optmode broken ofdx load");
            const OpticalOpts &q = p.optical();
            check(q.metaShape == 2 && q.metaPhase == 0,
                  "optmode broken metasurface clamped");
            check(q.phcLattice == 2, "optmode broken phc lattice clamped");
            check(q.nfffSurface == 1, "optmode broken nf2ff surface clamped");
            check(q.spPorts == 1 && q.spPortIn == 1,
                  "optmode broken sparam clamped");
        }
    }
}

// 実行結果の表示ゲート: ONN 活性化カーブは「その実行が生成したもの」だけ。
static void testRunGating()
{
    g_file = "run-gating";

    Project p;
    p.setActiveDomain(Domain::Optical);
    OpticalOpts &o = p.optical();
    o.solver = OpticalSolver::BPM;
    o.powerSweepEnabled = true;

    RunConfig bpm;
    bpm.kernel = Kernel::BPM;
    bpm.mode   = RunMode::Both;
    check(Runner::producesActivationCurve(p, bpm),
          "gate: bpm + powersweep expects an ONN result");

    RunConfig solverOnly = bpm; solverOnly.mode = RunMode::Solver;
    check(Runner::producesActivationCurve(p, solverOnly),
          "gate: solver-only bpm run expects an ONN result");

    RunConfig postOnly = bpm; postOnly.mode = RunMode::Post;
    check(!Runner::producesActivationCurve(p, postOnly),
          "gate: post-only run generates no activation curve");

    RunConfig fdtd = bpm; fdtd.kernel = Kernel::FDTD;
    check(!Runner::producesActivationCurve(p, fdtd),
          "gate: fdtd run must not show a stale activation curve");
    RunConfig rcwa = bpm; rcwa.kernel = Kernel::RCWA;
    check(!Runner::producesActivationCurve(p, rcwa),
          "gate: rcwa run must not show a stale activation curve");

    o.powerSweepEnabled = false;
    check(!Runner::producesActivationCurve(p, bpm),
          "gate: bpm without powersweep expects nothing");
    o.powerSweepEnabled = true;

    o.solver = OpticalSolver::FDTD;
    check(!Runner::producesActivationCurve(p, bpm),
          "gate: optical FDTD solver expects nothing");
    o.solver = OpticalSolver::BPM;

    p.setActiveDomain(Domain::Acoustic);
    check(!Runner::producesActivationCurve(p, bpm),
          "gate: non-optical domain expects nothing");
    p.setActiveDomain(Domain::Optical);

    // 実行前クリーンアップ用の作業ディレクトリ解決 (start() と同じ規則)
    RunConfig explicitWd;
    explicitWd.workingDir = QDir::tempPath() + "/ofdx_gate_explicit";
    check(Runner::resolveWorkingDir(&p, explicitWd) == explicitWd.workingDir,
          "wd: explicit workingDir wins");

    RunConfig autoWd;
    p.setFilePath(QDir::tempPath() + "/ofdx_gate/project.ofd");
    check(Runner::resolveWorkingDir(&p, autoWd) ==
              QDir::tempPath() + "/ofdx_gate",
          "wd: derived from the project file location");
    p.setFilePath(QString());
    check(Runner::resolveWorkingDir(&p, autoWd).endsWith("/openfdtd-x"),
          "wd: unsaved project falls back to the temp location");
    check(Runner::resolveWorkingDir(nullptr, autoWd).isEmpty(),
          "wd: no project and no explicit dir -> empty");

    // ── カーネルバイナリの探索 (resolveBinary) ──────────────────────────────
    // README は OPENFDTD_HOME にリポジトリルートを指定する案内なので、
    // ディレクトリ直下だけでなく bin/ も探索されること。
    check(qstrcmp(Runner::homeVarFor(Kernel::FDTD), "OPENFDTD_HOME") == 0,
          "bin: FDTD home var");
    check(qstrcmp(Runner::homeVarFor(Kernel::RCWA), "OPENRCWA_HOME") == 0,
          "bin: RCWA home var");
    check(qstrcmp(Runner::homeVarFor(Kernel::BPM), "OPENBPM_HOME") == 0,
          "bin: BPM home var");
    check(qstrcmp(Runner::homeVarFor(Kernel::Bellhop), "BELLHOPCUDA_HOME") == 0,
          "bin: Bellhop home var");

    QTemporaryDir kdir;
    if (kdir.isValid()) {
        // Windows は resolveBinary が .exe を付けるので両方の名前を置く
        const auto touch = [](const QString &path) {
            QDir().mkpath(QFileInfo(path).path());
            QFile f(path);
            f.open(QIODevice::WriteOnly);
        };
        RunConfig bc;
        bc.binaryDir = kdir.path();

        // (a) 何も無ければ素の名前 (PATH 解決に委ねる)
        const QString bare = Runner::resolveBinary(bc, "ofd");
        check(!bare.contains('/') || bare == "ofd",
              "bin: nothing found -> bare name for PATH");

        // (b) bin/ 配下だけにあるとき: リポジトリルート指定で見つかる
        touch(kdir.path() + "/bin/ofd");
        touch(kdir.path() + "/bin/ofd.exe");
        check(Runner::resolveBinary(bc, "ofd").contains("/bin/"),
              "bin: found under <dir>/bin");

        // (c) 直下にもあるときは直下が優先 (探索順の保証)
        touch(kdir.path() + "/ofd");
        touch(kdir.path() + "/ofd.exe");
        const QString direct = Runner::resolveBinary(bc, "ofd");
        check(direct.startsWith(kdir.path()) && !direct.contains("/bin/"),
              "bin: direct entry wins over bin/");

        // (d) GUI 設定 (QSettings "OpenFDTD/Kernels") のディレクトリも
        //     探索される (binaryDir 未指定でも見つかる)。既存の設定値は
        //     退避して必ず復元する。
        const QString prev = Runner::kernelDirSetting(Kernel::FDTD);
        Runner::setKernelDirSetting(Kernel::FDTD, kdir.path());
        check(Runner::kernelDirSetting(Kernel::FDTD) == kdir.path(),
              "bin: QSettings kernel dir round-trips");
        RunConfig noDir;   // binaryDir 未指定
        check(Runner::resolveBinary(noDir, "ofd").startsWith(kdir.path()),
              "bin: QSettings kernel dir searched");
        Runner::setKernelDirSetting(Kernel::FDTD, QString());
        check(Runner::kernelDirSetting(Kernel::FDTD).isEmpty(),
              "bin: QSettings kernel dir cleared");
        Runner::setKernelDirSetting(Kernel::FDTD, prev);

        // (e) resolvedSolverPath: 見つかれば実在する絶対パス、それが
        //     solverBinary の解決結果と一致する (PATH 環境に依存しない
        //     肯定側のみ検証 — CI はカーネル入りでも走る)
        const QString resolved = Runner::resolvedSolverPath(bc);
        check(!resolved.isEmpty() && QFileInfo::exists(resolved),
              "bin: resolvedSolverPath returns an existing path");
        check(resolved.startsWith(kdir.path()),
              "bin: resolvedSolverPath honours binaryDir");
    }
}


// 水中音響 (bellhopcxx) — .env 生成と Runner のカーネル解決。
// 環境変数 OFDX_BELLHOP_BIN が指す実カーネルがあれば、生成した .env を
// 実際に実行して .shd (TL 音場) が生成されることまで検証する。
static void testBellhop()
{
    g_file = "bellhop";

    Project p;
    p.setActiveDomain(Domain::Underwater);
    UnderwaterOpts &u = p.underwater();
    u.sonarFreq_kHz = 0.23;                 // 230 Hz (DickinsB と同帯域)
    u.rangeMax_km = 10.0;
    u.bottomC_mps = 1550.0;
    u.bottomRho_kgm3 = 1500.0;
    u.ssp = { { 0.0, 1476.7 }, { 100.0, 1467.2 }, { 3000.0, 1506.5 } };

    // (a) カーネル解決: 水中音響 → bellhopcxx
    check(Runner::kernelForProject(p) == Kernel::Bellhop,
          "bellhop: underwater resolves to Kernel::Bellhop");

    // (b) .env 生成: 主要行が BELLHOP の ENVFIL 仕様どおり並ぶこと
    const QString env = BellhopIO::envText(p);
    const QStringList lines = env.split('\n');
    check(lines[0].startsWith("'OpenFDTD-X underwater"),
          "bellhop: TITLE line");
    check(lines[1].startsWith("230\t"), "bellhop: FREQ 230 Hz");
    check(lines[2].startsWith("1\t"), "bellhop: NMEDIA = 1");
    check(lines[3].startsWith("'CVW'"), "bellhop: SSPOPT");
    check(lines[4].startsWith("0 0.0 3000"), "bellhop: bottom depth line");
    check(env.contains("\n0 1476.7 /\n"), "bellhop: first SSP point");
    check(env.contains("\n3000 1506.5 /\n"), "bellhop: last SSP point");
    check(env.contains("\n'A' 0.0\n3000 1550 0.0 1.5 0.5 /\n"),
          "bellhop: acousto-elastic halfspace (rho kg/m3 -> g/cm3)");
    check(env.contains("\n'C'"), "bellhop: coherent TL run type");
    check(env.contains("0.0 3100 11"), "bellhop: STEP/ZBOX/RBOX line");

    // (c) SSP 2 点未満でも実行可能な既定プロファイルで埋める
    {
        Project q;
        q.setActiveDomain(Domain::Underwater);
        q.underwater().ssp.clear();     // 既定プロファイルを外す
        const QString e2 = BellhopIO::envText(q);
        check(e2.contains("\n0 1500 /\n") && e2.contains("\n100 1500 /\n"),
              "bellhop: default iso-velocity profile when SSP missing");
    }

    // (c2) 底質吸収係数 α [dB/λ] (bottomAlpha_dBlambda):
    //   既定値 0.5 (従来のハードコード値) のままなら .env は従来とバイト一致、
    //   指定時はハーフスペース行の減衰へ反映される。
    {
        Project q0;                          // 既定 (α キーに一切触らない)
        q0.setActiveDomain(Domain::Underwater);
        Project q1;                          // 既定値 0.5 を明示指定
        q1.setActiveDomain(Domain::Underwater);
        q1.underwater().bottomAlpha_dBlambda = 0.5;
        check(BellhopIO::envText(q0) == BellhopIO::envText(q1),
              "bellhop: default alpha keeps .env byte-identical");
        // 既定プロジェクト (既定 SSP は 5000 m まで、c 1650, rho 1900) の
        // ハーフスペース行
        check(BellhopIO::envText(q0).contains("\n5000 1650 0.0 1.9 0.5 /\n"),
              "bellhop: default halfspace line unchanged (0.5 dB/lambda)");
        q1.underwater().bottomAlpha_dBlambda = 1.25;
        check(BellhopIO::envText(q1).contains("\n5000 1650 0.0 1.9 1.25 /\n"),
              "bellhop: alpha propagates to halfspace attenuation");
    }

    // (c3) .ofdx 永続化: bottom_alpha_db_lambda のラウンドトリップと
    //      旧ファイル (キー無し) の既定値 0.5 (旧ファイル互換)。
    {
        Project ps;
        ps.setActiveDomain(Domain::Underwater);
        ps.underwater().bottomAlpha_dBlambda = 1.25;
        QTemporaryFile f;
        f.setFileTemplate(QDir::tempPath() + "/ofdx_uw_alpha_XXXXXX.ofdx");
        if (f.open()) {
            check(OfdxIO::save(f.fileName(), ps), "uw alpha ofdx save");
            Project pl;
            check(OfdxIO::load(f.fileName(), pl), "uw alpha ofdx load");
            check(nearlyEq(pl.underwater().bottomAlpha_dBlambda, 1.25),
                  "uw alpha ofdx round-trip");
            check(nearlyEq(pl.underwater().bottomRho_kgm3, 1900.0),
                  "uw alpha ofdx keeps sibling keys");
            // JSON にキー名どおり書かれていること (改名検知)
            QFile jf(f.fileName());
            check(jf.open(QIODevice::ReadOnly), "uw alpha ofdx reopen");
            const QJsonObject uw = QJsonDocument::fromJson(jf.readAll())
                                       .object()["underwater"].toObject();
            check(nearlyEq(uw.value("bottom_alpha_db_lambda").toDouble(), 1.25),
                  "uw alpha ofdx key name");
        }
        QTemporaryFile old;
        old.setFileTemplate(QDir::tempPath() + "/ofdx_uw_alpha_old_XXXXXX.ofdx");
        if (old.open()) {
            old.write(QByteArray(
                "{ \"domain\": \"underwater\",\n"
                "  \"underwater\": { \"bottom_type\": \"mud\",\n"
                "                    \"bottom_c_mps\": 1520 } }\n"));
            old.flush();
            Project p3;
            check(OfdxIO::load(old.fileName(), p3), "uw alpha legacy ofdx load");
            check(nearlyEq(p3.underwater().bottomAlpha_dBlambda, 0.5),
                  "uw alpha legacy ofdx keeps default 0.5");
            check(p3.underwater().bottomType == "mud",
                  "uw alpha legacy ofdx reads sibling keys");
        }
    }

    // (d) 統合: 実カーネルがあれば .env を実行して .shd 生成まで確認
    const QString bin = qEnvironmentVariable("OFDX_BELLHOP_BIN");
    if (bin.isEmpty() || !QFileInfo::exists(bin)) {
        std::printf("  (bellhop integration skipped: "
                    "set OFDX_BELLHOP_BIN to run)\n");
        return;
    }
    QTemporaryDir dir;
    check(dir.isValid(), "bellhop: temp dir");
    const QString base = QStringLiteral("uwcase");
    {
        QFile f(dir.filePath(base + ".env"));
        check(f.open(QIODevice::WriteOnly | QIODevice::Text),
              "bellhop: write .env");
        f.write(env.toUtf8());
    }
    QProcess proc;
    proc.setWorkingDirectory(dir.path());
    proc.start(bin, { base });
    check(proc.waitForFinished(120000), "bellhop: kernel finished in time");
    check(proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0,
          "bellhop: kernel exit code 0");
    const QFileInfo shd(dir.filePath(base + ".shd"));
    check(shd.exists() && shd.size() > 0, "bellhop: .shd generated");
}

// ── HDF5 結果リーダー (io/H5Reader) ─────────────────────────────────────────
// USE_HDF5 ビルドのみ実行 (CI Linux が担う)。既知の値で 2D/3D データセットを
// 書き、列挙・2D 読み・フレーム読み・次元不一致の拒否を検証する。
static void testH5Reader()
{
    g_file = "h5_reader";
#ifndef OFD_USE_HDF5
    check(!H5Reader::available(), "h5: reader reports unavailable");
    std::printf("  (h5 reader tests skipped: built without USE_HDF5)\n");
#else
    check(H5Reader::available(), "h5: reader available");
    QTemporaryDir dir;
    check(dir.isValid(), "h5: temp dir");
    const QString path = dir.filePath("t.h5");

    // /field/Ixz (3×4, v = i*10+j) と /field/frames (2×3×4, v = f*100+i*10+j)
    {
        const hid_t file = H5Fcreate(path.toLocal8Bit().constData(),
                                     H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
        check(file >= 0, "h5: fixture file created");
        const hid_t grp = H5Gcreate2(file, "/field", H5P_DEFAULT, H5P_DEFAULT,
                                     H5P_DEFAULT);
        float m2[3][4];
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 4; ++j) m2[i][j] = float(i * 10 + j);
        const hsize_t d2[2] = { 3, 4 };
        hid_t sp = H5Screate_simple(2, d2, nullptr);
        hid_t ds = H5Dcreate2(grp, "Ixz", H5T_NATIVE_FLOAT, sp, H5P_DEFAULT,
                              H5P_DEFAULT, H5P_DEFAULT);
        H5Dwrite(ds, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, m2);
        H5Dclose(ds); H5Sclose(sp);

        float m3[2][3][4];
        for (int f = 0; f < 2; ++f)
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 4; ++j)
                    m3[f][i][j] = float(f * 100 + i * 10 + j);
        const hsize_t d3[3] = { 2, 3, 4 };
        sp = H5Screate_simple(3, d3, nullptr);
        ds = H5Dcreate2(grp, "frames", H5T_NATIVE_FLOAT, sp, H5P_DEFAULT,
                        H5P_DEFAULT, H5P_DEFAULT);
        H5Dwrite(ds, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, m3);
        H5Dclose(ds); H5Sclose(sp);
        H5Gclose(grp);
        H5Fclose(file);
    }

    QVector<H5DatasetInfo> list;
    check(H5Reader::listDatasets(path, list), "h5: listDatasets ok");
    check(list.size() == 2, "h5: two datasets listed");
    bool foundIxz = false;
    for (const H5DatasetInfo &d : list)
        if (d.path == QLatin1String("/field/Ixz")) {
            foundIxz = true;
            check(d.dims == (QVector<qlonglong>{ 3, 4 }), "h5: Ixz dims");
            check(d.typeName == QLatin1String("float32"), "h5: Ixz type");
        }
    check(foundIxz, "h5: Ixz found in listing");

    QVector<double> m;
    int r = 0, c = 0;
    check(H5Reader::read2D(path, "/field/Ixz", m, r, c), "h5: read2D ok");
    check(r == 3 && c == 4, "h5: read2D dims");
    check(m[1 * 4 + 2] == 12.0, "h5: read2D value (float→double 変換込み)");

    check(H5Reader::readFrame(path, "/field/frames", 1, m, r, c),
          "h5: readFrame ok");
    check(r == 3 && c == 4 && m[2 * 4 + 3] == 123.0, "h5: readFrame value");

    check(!H5Reader::read2D(path, "/field/frames", m, r, c),
          "h5: read2D rejects 3-D dataset");
    check(!H5Reader::readFrame(path, "/field/frames", 9, m, r, c),
          "h5: readFrame rejects out-of-range frame");

    // readAll: スカラーと 2D (float→double 変換込み)
    QVector<double> flat;
    QVector<qlonglong> dims;
    check(H5Reader::readAll(path, "/field/Ixz", flat, dims),
          "h5: readAll 2D ok");
    check(dims == (QVector<qlonglong>{ 3, 4 }) && flat.size() == 12 &&
          flat[6] == 12.0, "h5: readAll dims/values");

    // ofd レイアウトの空間再構成: 2×2×2 セル (ノード 3×3×3, 余白なし)。
    // node = 9i + 3j + k。全 6 成分 = node → |E| = node·√6。
    // /data000000 はゼロ、/data000100 に実値 — 最終グループが選ばれること
    const QString ofdPath = dir.filePath("ofd.h5");
    {
        const hid_t file = H5Fcreate(ofdPath.toLocal8Bit().constData(),
                                     H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
        check(file >= 0, "h5: ofd fixture created");
        auto writeScalarInt = [&](const char *name, long long v) {
            const hid_t sp = H5Screate(H5S_SCALAR);
            const hid_t ds = H5Dcreate2(file,
                (QByteArray("/metadata/") + name).constData(),
                H5T_NATIVE_LLONG, sp, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
            H5Dwrite(ds, H5T_NATIVE_LLONG, H5S_ALL, H5S_ALL, H5P_DEFAULT, &v);
            H5Dclose(ds); H5Sclose(sp);
        };
        H5Gclose(H5Gcreate2(file, "/metadata", H5P_DEFAULT, H5P_DEFAULT,
                            H5P_DEFAULT));
        writeScalarInt("Nx", 2); writeScalarInt("Ny", 2);
        writeScalarInt("Nz", 2);
        writeScalarInt("Ni", 9); writeScalarInt("Nj", 3);
        writeScalarInt("Nk", 1); writeScalarInt("N0", 0);
        writeScalarInt("NN", 27);
        // 節点座標 [m] — 旧レイアウトは /metadata/Xn,Yn,Zn に native double
        // の 1 次元配列で入る (実 ofd 出力で確認済み: Xn {Nx+1})
        auto writeCoordsD = [&](const char *name,
                                const std::vector<double> &v) {
            const hsize_t d1[1] = { hsize_t(v.size()) };
            const hid_t sp = H5Screate_simple(1, d1, nullptr);
            const hid_t ds = H5Dcreate2(file,
                (QByteArray("/metadata/") + name).constData(),
                H5T_NATIVE_DOUBLE, sp, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
            H5Dwrite(ds, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT,
                     v.data());
            H5Dclose(ds); H5Sclose(sp);
        };
        writeCoordsD("Xn", { 0.0, 0.5, 1.0 });
        writeCoordsD("Yn", { -1.0, 0.0, 1.0 });
        writeCoordsD("Zn", { 0.0, 1.0, 2.0 });
        auto writeE = [&](const char *group, bool zeros) {
            H5Gclose(H5Gcreate2(file, group, H5P_DEFAULT, H5P_DEFAULT,
                                H5P_DEFAULT));
            QVector<double> e(27 * 6);
            for (int n = 0; n < 27; ++n)
                for (int cc = 0; cc < 6; ++cc)
                    e[n * 6 + cc] = zeros ? 0.0 : double(n);
            const hsize_t d4[4] = { 1, 1, 27, 6 };
            const hid_t sp = H5Screate_simple(4, d4, nullptr);
            const hid_t ds = H5Dcreate2(file,
                (QByteArray(group) + "/E").constData(),
                H5T_NATIVE_DOUBLE, sp, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
            H5Dwrite(ds, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT,
                     e.constData());
            H5Dclose(ds); H5Sclose(sp);
        };
        writeE("/data000000", true);
        writeE("/data000100", false);
        H5Fclose(file);
    }
    {
        QVector<double> cells;
        int rows = 0, cols = 0;
        QString group;
        check(H5Reader::readOfdMidSlice(ofdPath, cells, rows, cols, &group),
              "h5: ofd mid-slice ok");
        check(rows == 3 && cols == 3, "h5: ofd slice dims (Ny+1, Nx+1)");
        check(group == QLatin1String("data000100"),
              "h5: ofd latest group selected");
        // k = Nz/2 = 1。行 0 = +y (j=2)。|E|(i,j) = (9i+3j+1)·√6
        const double s6 = std::sqrt(6.0);
        check(std::fabs(cells[2 * 3 + 0] - (9 * 0 + 3 * 0 + 1) * s6) < 1e-9,
              "h5: ofd slice value (i=0,j=0)");
        check(std::fabs(cells[0 * 3 + 2] - (9 * 2 + 3 * 2 + 1) * s6) < 1e-9,
              "h5: ofd slice value (i=2,j=2 → row 0)");
        check(std::fabs(cells[1 * 3 + 1] - (9 * 1 + 3 * 1 + 1) * s6) < 1e-9,
              "h5: ofd slice center value");
    }

    // 旧レイアウトの伝搬時系列: /data000000 (ゼロ) → /data000100 (実値) の
    // 2 フレーム列として読めること
    {
        H5OfdSeriesInfo info;
        check(H5Reader::ofdSeriesInfo(ofdPath, "E", info),
              "h5: old-layout series info");
        check(info.frames == 2 && info.rows == 3 && info.cols == 3 &&
              !info.instantaneous, "h5: old-layout series shape");
        QVector<double> cells;
        int rows = 0, cols = 0;
        QString label;
        check(H5Reader::readOfdSeriesFrame(ofdPath, "E", 0, 2, -1, cells, rows, cols,
                                           &label),
              "h5: old-layout series frame 0");
        double vmax = 0.0;
        for (double v : cells) vmax = std::max(vmax, v);
        check(vmax == 0.0 && label == QLatin1String("data000000"),
              "h5: old-layout frame 0 is the zero snapshot");
        check(H5Reader::readOfdSeriesFrame(ofdPath, "E", 1, 2, -1, cells, rows, cols,
                                           &label),
              "h5: old-layout series frame 1");
        const double s6b = std::sqrt(6.0);
        check(std::fabs(cells[1 * 3 + 1] - (9 + 3 + 1) * s6b) < 1e-9 &&
              label == QLatin1String("data000100"),
              "h5: old-layout frame 1 values");
        check(!H5Reader::readOfdSeriesFrame(ofdPath, "E", 2, 2, -1, cells, rows, cols),
              "h5: old-layout series rejects out-of-range frame");
        // 断面軸の切替: YZ 面 (X=1 固定, node = 9·1+3j+k)。
        // 列 = Y (u=j), 行 = Z (v=k, 行 0 = k=2)
        check(H5Reader::readOfdSeriesFrame(ofdPath, "E", 1, 0, 1, cells,
                                           rows, cols),
              "h5: old-layout YZ slice");
        check(rows == 3 && cols == 3 &&
              std::fabs(cells[0 * 3 + 1] - (9 + 3 + 2) * s6b) < 1e-9 &&
              std::fabs(cells[2 * 3 + 1] - (9 + 3 + 0) * s6b) < 1e-9,
              "h5: old-layout YZ slice values");
        // XZ 面 (Y=0 固定, node = 9i+k)。列 = X, 行 = Z
        check(H5Reader::readOfdSeriesFrame(ofdPath, "E", 1, 1, 0, cells,
                                           rows, cols) &&
              std::fabs(cells[1 * 3 + 2] - (18 + 1) * s6b) < 1e-9,
              "h5: old-layout XZ slice values");
    }

    // 節点座標 (旧レイアウト /metadata/Xn,Yn,Zn) — 3D 表示で断面を実寸法・
    // 実位置に置くために要る
    {
        QVector<double> xs, ys, zs;
        check(H5Reader::ofdGridCoords(ofdPath, xs, ys, zs),
              "h5: old-layout grid coords");
        check(xs.size() == 3 && ys.size() == 3 && zs.size() == 3,
              "h5: old-layout coord sizes (Nx+1 …)");
        check(xs[0] == 0.0 && xs[2] == 1.0 && ys[0] == -1.0 &&
              ys[2] == 1.0 && zs[0] == 0.0 && zs[2] == 2.0,
              "h5: old-layout coord values [m]");
    }

    // 新レイアウト (OpenFDTD sol/outputHdf5.c): /freqdomain/E
    // {F, Nx+1, Ny+1, Nz+1, 3, 2}。値は全 6 成分 = 9i+3j+k → |E| = v·√6。
    // あわせて /timeseries/E {2, 3, 3, 3, 3} (瞬時値 3 成分 = 同値 →
    // |E| = v·√3。フレーム 0 はゼロ) と /timeseries/time {2} を書く
    const QString newPath = dir.filePath("ofd_new.h5");
    {
        const hid_t file = H5Fcreate(newPath.toLocal8Bit().constData(),
                                     H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
        check(file >= 0, "h5: freqdomain fixture created");
        H5Gclose(H5Gcreate2(file, "/freqdomain", H5P_DEFAULT, H5P_DEFAULT,
                            H5P_DEFAULT));
        QVector<float> e(3 * 3 * 3 * 6);
        int m6 = 0;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                for (int k = 0; k < 3; ++k)
                    for (int cc = 0; cc < 6; ++cc)
                        e[m6++] = float(9 * i + 3 * j + k);
        const hsize_t d6[6] = { 1, 3, 3, 3, 3, 2 };
        hid_t sp = H5Screate_simple(6, d6, nullptr);
        hid_t ds = H5Dcreate2(file, "/freqdomain/E", H5T_NATIVE_FLOAT,
                              sp, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
        H5Dwrite(ds, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT,
                 e.constData());
        H5Dclose(ds); H5Sclose(sp);

        // /timeseries: 瞬時値スナップショット 2 フレーム + 時刻
        H5Gclose(H5Gcreate2(file, "/timeseries", H5P_DEFAULT, H5P_DEFAULT,
                            H5P_DEFAULT));
        QVector<float> ts(2 * 3 * 3 * 3 * 3, 0.0f);
        int m5 = 3 * 3 * 3 * 3;    // フレーム 1 の先頭 (フレーム 0 はゼロ)
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                for (int k = 0; k < 3; ++k)
                    for (int cc = 0; cc < 3; ++cc)
                        ts[m5++] = float(9 * i + 3 * j + k);
        const hsize_t d5[5] = { 2, 3, 3, 3, 3 };
        sp = H5Screate_simple(5, d5, nullptr);
        ds = H5Dcreate2(file, "/timeseries/E", H5T_NATIVE_FLOAT, sp,
                        H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
        H5Dwrite(ds, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT,
                 ts.constData());
        H5Dclose(ds); H5Sclose(sp);
        const double times[2] = { 0.0, 1.5e-9 };
        const hsize_t d1[1] = { 2 };
        sp = H5Screate_simple(1, d1, nullptr);
        ds = H5Dcreate2(file, "/timeseries/time", H5T_NATIVE_DOUBLE, sp,
                        H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
        H5Dwrite(ds, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, times);
        H5Dclose(ds); H5Sclose(sp);

        // 節点座標 [m] — 新レイアウトは /geometry/Xn,Yn,Zn。float32 で書いて
        // double へ変換されること (型変換は HDF5 任せ) もあわせて確認する
        H5Gclose(H5Gcreate2(file, "/geometry", H5P_DEFAULT, H5P_DEFAULT,
                            H5P_DEFAULT));
        auto writeCoordsF = [&](const char *name,
                                const std::vector<float> &v) {
            const hsize_t d1[1] = { hsize_t(v.size()) };
            const hid_t s1 = H5Screate_simple(1, d1, nullptr);
            const hid_t d = H5Dcreate2(file,
                (QByteArray("/geometry/") + name).constData(),
                H5T_NATIVE_FLOAT, s1, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
            H5Dwrite(d, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT,
                     v.data());
            H5Dclose(d); H5Sclose(s1);
        };
        writeCoordsF("Xn", { 0.0f, 0.25f, 0.5f });
        writeCoordsF("Yn", { -0.5f, 0.0f, 0.5f });
        writeCoordsF("Zn", { 1.0f, 1.5f, 2.0f });
        H5Fclose(file);
    }
    {
        QVector<double> cells;
        int rows = 0, cols = 0;
        QString group;
        check(H5Reader::readOfdMidSlice(newPath, cells, rows, cols, &group),
              "h5: freqdomain mid-slice ok");
        check(rows == 3 && cols == 3, "h5: freqdomain slice dims");
        check(group == QLatin1String("freqdomain"),
              "h5: freqdomain layout detected");
        const double s6 = std::sqrt(6.0);
        check(std::fabs(cells[2 * 3 + 0] - (9 * 0 + 3 * 0 + 1) * s6) < 1e-5,
              "h5: freqdomain value (i=0,j=0)");
        check(std::fabs(cells[0 * 3 + 2] - (9 * 2 + 3 * 2 + 1) * s6) < 1e-5,
              "h5: freqdomain value (i=2,j=2 → row 0)");
        check(std::fabs(cells[1 * 3 + 1] - (9 * 1 + 3 * 1 + 1) * s6) < 1e-5,
              "h5: freqdomain center value");
    }

    // 新レイアウトの伝搬時系列 (/timeseries/E 瞬時値) の再生
    {
        H5OfdSeriesInfo info;
        check(H5Reader::ofdSeriesInfo(newPath, "E", info),
              "h5: timeseries info");
        check(info.frames == 2 && info.rows == 3 && info.cols == 3 &&
              info.instantaneous, "h5: timeseries shape (instantaneous)");
        QVector<double> cells;
        int rows = 0, cols = 0;
        QString label;
        check(H5Reader::readOfdSeriesFrame(newPath, "E", 0, 2, -1, cells, rows, cols,
                                           &label),
              "h5: timeseries frame 0");
        double vmax = 0.0;
        for (double v : cells) vmax = std::max(vmax, v);
        check(vmax == 0.0, "h5: timeseries frame 0 is zero");
        check(label.startsWith(QStringLiteral("t = 0")),
              "h5: timeseries frame 0 time label");
        check(H5Reader::readOfdSeriesFrame(newPath, "E", 1, 2, -1, cells, rows, cols,
                                           &label),
              "h5: timeseries frame 1");
        const double s3 = std::sqrt(3.0);
        // k = (Nz+1-1)/2 = 1、行 0 = +y。|E|(i,j) = (9i+3j+1)·√3
        check(std::fabs(cells[1 * 3 + 1] - (9 + 3 + 1) * s3) < 1e-5,
              "h5: timeseries frame 1 center value");
        // 断面軸の切替: YZ 面 (X=2 固定)。列 = Y, 行 = Z (行 0 = k=2)
        check(H5Reader::readOfdSeriesFrame(newPath, "E", 1, 0, 2, cells,
                                           rows, cols),
              "h5: timeseries YZ slice");
        check(rows == 3 && cols == 3 &&
              std::fabs(cells[0 * 3 + 1] - (18 + 3 + 2) * s3) < 1e-5,
              "h5: timeseries YZ slice value");
        check(label.contains(QStringLiteral("1.5e-09")),
              "h5: timeseries frame 1 time label");
        check(!H5Reader::readOfdSeriesFrame(newPath, "E", 5, 2, -1, cells, rows,
                                            cols),
              "h5: timeseries rejects out-of-range frame");
    }

    // 節点座標 (新レイアウト /geometry/Xn,Yn,Zn。float32 → double 変換込み)
    {
        QVector<double> xs, ys, zs;
        check(H5Reader::ofdGridCoords(newPath, xs, ys, zs),
              "h5: new-layout grid coords (/geometry)");
        check(xs.size() == 3 && ys.size() == 3 && zs.size() == 3,
              "h5: new-layout coord sizes");
        check(std::fabs(xs[1] - 0.25) < 1e-6 &&
              std::fabs(ys[0] + 0.5) < 1e-6 &&
              std::fabs(zs[2] - 2.0) < 1e-6,
              "h5: new-layout coord values [m]");
    }

    // 座標が無いファイル / 片方の軸だけ欠けたファイル。
    // 取れない軸は空ベクタで返る (呼び出し側が「座標不明」を判断できること)
    {
        QVector<double> xs, ys, zs;
        check(!H5Reader::ofdGridCoords(path, xs, ys, zs),
              "h5: grid coords absent → false");
        check(xs.isEmpty() && ys.isEmpty() && zs.isEmpty(),
              "h5: grid coords absent → all empty");

        // /geometry/Xn と /metadata/Zn だけを持つファイル (Yn は欠落)。
        // 軸ごとに 新 → 旧 の順で探すので混在でも拾えること
        const QString partial = dir.filePath("coords_partial.h5");
        {
            const hid_t file = H5Fcreate(partial.toLocal8Bit().constData(),
                                         H5F_ACC_TRUNC, H5P_DEFAULT,
                                         H5P_DEFAULT);
            check(file >= 0, "h5: partial coords fixture created");
            H5Gclose(H5Gcreate2(file, "/geometry", H5P_DEFAULT, H5P_DEFAULT,
                                H5P_DEFAULT));
            H5Gclose(H5Gcreate2(file, "/metadata", H5P_DEFAULT, H5P_DEFAULT,
                                H5P_DEFAULT));
            auto write1D = [&](const char *name, const std::vector<double> &v) {
                const hsize_t d1[1] = { hsize_t(v.size()) };
                const hid_t sp = H5Screate_simple(1, d1, nullptr);
                const hid_t ds = H5Dcreate2(file, name, H5T_NATIVE_DOUBLE, sp,
                                            H5P_DEFAULT, H5P_DEFAULT,
                                            H5P_DEFAULT);
                H5Dwrite(ds, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT,
                         v.data());
                H5Dclose(ds); H5Sclose(sp);
            };
            write1D("/geometry/Xn", { 1.0, 3.0 });
            write1D("/metadata/Zn", { 5.0, 7.0 });
            H5Fclose(file);
        }
        check(H5Reader::ofdGridCoords(partial, xs, ys, zs),
              "h5: partial grid coords ok");
        check(xs.size() == 2 && ys.isEmpty() && zs.size() == 2,
              "h5: partial grid coords — missing axis is empty");
        check(xs[1] == 3.0 && zs[1] == 7.0,
              "h5: partial grid coords values (新旧レイアウト混在)");
    }
#endif
}

// ── OpenFDTD (基幹カーネル) 統合 ────────────────────────────────────────────
// 環境変数 OFDX_OFD_BIN が指す実カーネルがあれば、同梱サンプル dipole.ofd を
// 実行して正常終了 (ofd.log の "normal end") まで検証する。GUI → subprocess
// 連携の回帰検出 (bellhop 統合と同じゲート方式。CI Linux が実ビルドを渡す)。
static void testOfdIntegration(const QString &sampleDir)
{
    g_file = "ofd_integration";
    const QString bin = qEnvironmentVariable("OFDX_OFD_BIN");
    if (bin.isEmpty() || !QFileInfo::exists(bin)) {
        std::printf("  (ofd integration skipped: "
                    "set OFDX_OFD_BIN to run)\n");
        return;
    }
    QTemporaryDir dir;
    check(dir.isValid(), "ofd: temp dir");
    check(QFile::copy(QDir(sampleDir).filePath("dipole.ofd"),
                      dir.filePath("dipole.ofd")),
          "ofd: copy dipole.ofd");
    QProcess proc;
    proc.setWorkingDirectory(dir.path());
    proc.start(bin, { QStringLiteral("-n"), QStringLiteral("2"),
                      QStringLiteral("dipole.ofd") });
    check(proc.waitForFinished(300000), "ofd: kernel finished in time");
    check(proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0,
          "ofd: kernel exit code 0");
    QFile log(dir.filePath("ofd.log"));
    check(log.open(QIODevice::ReadOnly | QIODevice::Text)
              && QString::fromUtf8(log.readAll()).contains("normal end"),
          "ofd: log reports normal end");
    // 実カーネルの ofd.log から給電点表が読めること (結果反映の入口)。
    // dipole.ofd は frequency1 = 21 点
    const QVector<FeedSweep> sweeps =
        KernelResultReader::readFeedSweeps(dir.filePath("ofd.log"));
    check(sweeps.size() == 1 && sweeps.first().points.size() == 21,
          "ofd: feed sweep parsed from real log");
#ifdef OFD_USE_HDF5
    // 実カーネルの time_series_data.h5 から z 中央断面が再構成できること。
    // dipole は 30×30×31 セル → 断面は 31×31 ノード
    {
        QVector<double> cells;
        int rows = 0, cols = 0;
        QString group;
        check(H5Reader::readOfdMidSlice(dir.filePath("time_series_data.h5"),
                                        cells, rows, cols, &group),
              "ofd: h5 mid-slice from real kernel output");
        check(rows == 31 && cols == 31, "ofd: h5 slice dims 31x31");
        double vmax = 0.0;
        for (double v : cells) vmax = std::max(vmax, v);
        check(vmax > 0.0, "ofd: h5 slice has non-zero field");
        // 伝搬時系列 (新 /timeseries、旧 /data%06d のどちらでも) が
        // フレーム列として読めること
        H5OfdSeriesInfo sinfo;
        check(H5Reader::ofdSeriesInfo(dir.filePath("time_series_data.h5"),
                                      "E", sinfo) && sinfo.frames > 0,
              "ofd: propagation series available");
        QVector<double> fcells;
        int fr = 0, fc = 0;
        check(H5Reader::readOfdSeriesFrame(
                  dir.filePath("time_series_data.h5"), "E",
                  sinfo.frames - 1, 2, -1, fcells, fr, fc),
              "ofd: propagation series last frame readable");
        check(fr == 31 && fc == 31, "ofd: series frame dims 31x31");
        // 断面軸の切替 (XZ 面 = Y 固定): dipole は 30×30×31 セル →
        // 行 = Nz+1 = 32, 列 = Nx+1 = 31
        check(H5Reader::readOfdSeriesFrame(
                  dir.filePath("time_series_data.h5"), "E",
                  sinfo.frames - 1, 1, -1, fcells, fr, fc),
              "ofd: XZ slice readable");
        check(fr == 32 && fc == 31, "ofd: XZ slice dims 32x31");
    }
#endif

    // テンプレート E2E: ギャラリーの EM テンプレートが生成する .ofd を
    // 実カーネルがそのまま解けること (テンプレートの「実シチュエーション」保証)
    {
        g_file = "template_e2e:em_antenna";
        Project tp;
        check(templates::apply(tp, "em", "em_antenna"), "e2e: apply");
        // E2E はステップ数を落として短時間で判定する (物理検証は本体の
        // dipole 回帰が担う — ここでは入力受理と正常終了のみを見る)
        tp.general().maxiter = 200;
        QTemporaryDir tdir;
        check(tdir.isValid(), "e2e: temp dir");
        QString err;
        check(tp.save(tdir.filePath("template.ofd"), &err), "e2e: save");
        QProcess tproc;
        tproc.setWorkingDirectory(tdir.path());
        tproc.start(bin, { QStringLiteral("-n"), QStringLiteral("2"),
                           QStringLiteral("template.ofd") });
        check(tproc.waitForFinished(300000), "e2e: kernel finished in time");
        check(tproc.exitStatus() == QProcess::NormalExit &&
              tproc.exitCode() == 0, "e2e: kernel exit code 0");
        QFile tlog(tdir.filePath("ofd.log"));
        check(tlog.open(QIODevice::ReadOnly | QIODevice::Text) &&
              QString::fromUtf8(tlog.readAll()).contains("normal end"),
              "e2e: log reports normal end");
    }
}

// ── オペラ音響の一括レポート (AcousticReportBuilder) ────────────────────────
// GUI を介さず、手作りの分析結果からレポート文字列を生成して検証する。
//   (a) 未実行の明示 — 空欄でなく「未実行」トークンが出ること (絶対規則 5)
//   (b) 決定性 — 同一入力から同一バイト列 (時刻・乱数を含まない)
//   (c) HTML エスケープ — 警告文の '<' がタグとして混入しないこと
//   (d) 校正ゲート — 校正オフセットは Absolute のときだけ載ること
static void testAcousticReport()
{
    g_file = "acoustic_report";
    using acoustics::AnalysisQuality;
    using acoustics::MetricValue;

    // 最小の RIR 分析結果 (Full band 1 帯域 + 反射 1 件 + 警告 1 件)
    acoustics::RirAnalysisResult rir;
    rir.overallQuality = AnalysisQuality::Warning;
    rir.preprocess.noiseFloorDb = -65.0;
    rir.preprocess.peakDb = -1.0;
    rir.preprocess.dynamicRangeDb = 64.0;
    rir.directSound.found = true;
    rir.directSound.timeSeconds = 0.0125;
    rir.directSound.quality = AnalysisQuality::Valid;
    acoustics::BandMetricsResult bm;
    bm.band = acoustics::Band("Full band", 0.0, 0.0, 0.0, true);
    bm.filterOk = true;
    bm.metrics.t30 = acoustics::makeValidMetric(1.82);
    bm.metrics.c80 = acoustics::makeValidMetric(2.4);
    // T20 は無効 (動的レンジ不足を模擬) — 「算出不可」経路の検証
    bm.metrics.t20.valid = false;
    bm.metrics.t20.quality = AnalysisQuality::Invalid;
    bm.metrics.t20.warning = "dynamic range < 30 dB";
    rir.bands.push_back(bm);
    acoustics::ReflectionEvent ev;
    ev.arrivalTime = 0.030;
    ev.delayFromDirect = 0.0175;
    ev.relativeLevelDb = -6.5;
    ev.confidence = 0.9;
    rir.reflections.push_back(ev);
    rir.warnings.push_back("noise floor high & tail short <check>");

    AcousticReportInput in;
    in.projectTitle = "Report <Test> & Co.";
    in.rirFile = "hall.wav";
    in.calibrationState = 2;   // Uncalibrated
    in.hasRir = true;
    in.rir = rir;

    // (a) 歌声分析は未実行 → CSV に not_run が明示される
    const QString csv = AcousticReportBuilder::buildCsv(in);
    check(csv.startsWith("source,section,metric,band,value,unit,"
                         "valid,quality,warning\n"),
          "report: csv header");
    check(csv.contains("meta,status,rir_analysis,,done"),
          "report: csv rir status done");
    check(csv.contains("meta,status,vocal_analysis,,not_run"),
          "report: csv vocal status not_run");
    check(csv.contains("rir,metrics,T30,Full band,1.82,s,1,valid,"),
          "report: csv T30 row");
    check(!csv.contains("vocal,metrics,"), "report: csv has no vocal rows");

    // (d) Uncalibrated ではオフセット行を出さない
    check(!csv.contains("calibration_offset_db"),
          "report: csv no offset when uncalibrated");

    // (b) 決定性: 同一入力 → 同一バイト列
    check(AcousticReportBuilder::buildCsv(in) == csv,
          "report: csv deterministic");
    const QString html = AcousticReportBuilder::buildHtml(in);
    check(AcousticReportBuilder::buildHtml(in) == html,
          "report: html deterministic");

    // (c) HTML: タイトル・警告のメタ文字がエスケープされる
    check(html.contains("Report &lt;Test&gt; &amp; Co."),
          "report: html escapes project title");
    check(html.contains("&lt;check&gt;"), "report: html escapes warning text");
    check(!html.contains("<check>"), "report: no raw tag from warning");
    // 無効な T20 は数値でなく「算出不可」になる
    check(html.contains("dynamic range &lt; 30 dB"),
          "report: html carries invalid reason");
    // 未実行セクションの明示 (ja 既定文言)
    check(html.contains(QString::fromUtf8("未実行")),
          "report: html marks vocal section not run");
    check(html.startsWith("<!DOCTYPE html>"), "report: html doctype");

    // (d) Absolute にするとオフセットが載る
    in.calibrationState = 0;
    in.calibrationOffsetDb = 94.0;
    check(AcousticReportBuilder::buildCsv(in)
              .contains("meta,info,calibration_offset_db,,94.0"),
          "report: csv offset present when absolute");

    // 両方未実行なら hasAnyResult が false (空レポートを出さない判断に使う)
    AcousticReportInput none;
    check(!AcousticReportBuilder::hasAnyResult(none),
          "report: hasAnyResult false when nothing run");
    check(AcousticReportBuilder::hasAnyResult(in),
          "report: hasAnyResult true with rir");

    // 歌声分析を追加すると vocal 行が現れる
    acoustics::VocalAnalysisResult voc;
    voc.overallQuality = AnalysisQuality::Valid;
    voc.totalFrameCount = 100;
    voc.voicedFrameCount = 80;
    voc.voicedRatio = 0.8;
    voc.f0SearchMinHz = 80.0;
    voc.f0SearchMaxHz = 1000.0;
    voc.f0MedianHz = acoustics::makeValidMetric(440.0);
    in.hasVocal = true;
    in.vocal = voc;
    const QString csv2 = AcousticReportBuilder::buildCsv(in);
    check(csv2.contains("meta,status,vocal_analysis,,done"),
          "report: csv vocal status done");
    check(csv2.contains("vocal,metrics,F0 median,Full band,440.0,Hz,1,valid,"),
          "report: csv vocal F0 row");
}

// ── 光導波路 断面 FDE ソルバ (src/optics/FdeModeSolver) ─────────────────────
//
// 検証の出所をソルバー本体から独立させる: 対称 3 層スラブ導波路は超越方程式
// という厳密解を持つので、それをここで二分法により解いて基準にする。
// ソルバー側の式・関数は一切再利用しない。
//
// 2D ソルバーを 1D スラブへ帰着させるやり方:
//   屈折率を y にだけ依存させると離散問題は x と y に厳密に分離する。
//   x 方向は Dirichlet 窓の離散ラプラシアン (閉形式の固有値 μ_x) なので、
//   β²_2D = μ_x + β²_1D から β²_1D を厳密に取り出せる。この引き算をすると
//   残る差は y 方向の離散化誤差だけになり、格子細分化で 2 次収束するかどうかで
//   「離散化誤差」と「差分スキームのバグ」を区別できる。

namespace fdetest {

const double kPi = 3.14159265358979323846;

// 対称スラブ導波路の厳密 neff。分散方程式は
//     κ t = m π + 2 atan(ρ γ / κ),  κ = √(k0²n1² − β²), γ = √(β² − k0²n2²)
//     ρ = 1 (TE: E は界面に平行) / (n1/n2)² (TM: E は界面に垂直)
// 左辺 − 右辺は neff について単調減少なので二分法で解ける。
// V = k0 t √(n1²−n2²) > mπ のときだけ m 次モードが存在する (無ければ -1)。
double slabExact(double n1, double n2, double t, double lam, int m, bool tm)
{
    const double k0 = 2.0 * kPi / lam;
    const double rho = tm ? (n1 * n1) / (n2 * n2) : 1.0;
    auto f = [&](double ne) {
        const double b2 = k0 * k0 * ne * ne;
        const double kap = std::sqrt(std::max(k0 * k0 * n1 * n1 - b2, 0.0));
        const double gam = std::sqrt(std::max(b2 - k0 * k0 * n2 * n2, 0.0));
        return kap * t - m * kPi
             - 2.0 * std::atan(rho * gam / std::max(kap, 1e-300));
    };
    double lo = n2 + 1e-12, hi = n1 - 1e-12;
    if (f(lo) < 0.0) return -1.0;             // このモードは存在しない
    for (int i = 0; i < 200; ++i) {
        const double mid = 0.5 * (lo + hi);
        if (f(mid) > 0.0) lo = mid; else hi = mid;
    }
    return 0.5 * (lo + hi);
}

// 節点 nn 個・間隔 h の 1 次元 Dirichlet 離散ラプラシアンの m 次固有値
// (m = 1..nn)。−(4/h²) sin²(mπ / (2(nn+1)))。
double discLapEig(int nn, double h, int m)
{
    const double s = std::sin(m * kPi / (2.0 * (nn + 1)));
    return -(4.0 / (h * h)) * s * s;
}

// y 方向にだけ層構造を持つ (x には一様な) 断面
ofd::optics::CrossSection slabSection(double n1, double n2, double t, double dy,
                                      double clad, int nx, double dx)
{
    const int nct = int(std::lround(t / dy));
    const int ncl = int(std::lround(clad / dy));
    const int ny = nct + 2 * ncl;
    ofd::optics::CrossSection cs;
    cs.nx = nx; cs.ny = ny; cs.dx_um = dx; cs.dy_um = dy;
    cs.n.assign(size_t(nx) * ny, n2);
    cs.core.assign(size_t(nx) * ny, 0);
    for (int iy = 0; iy < ny; ++iy) {
        const bool inCore = (iy >= ncl && iy < ncl + nct);
        for (int ix = 0; ix < nx; ++ix) {
            cs.n[size_t(iy) * nx + ix] = inCore ? n1 : n2;
            cs.core[size_t(iy) * nx + ix] = inCore ? 1 : 0;
        }
    }
    return cs;
}

} // namespace fdetest

// ── 建築遮音コア (src/acoustics/core/SoundInsulation) ───────────────────────
// 期待値は実装から読まず、規格の手順・解析解・極限値から独立に立てる。
static void testSoundInsulation()
{
    namespace ins = ofd::acoustics::insulation;
    g_file = "sound-insulation";

    // 帯域テーブル (1/3 oct 50..5000 Hz)
    check(ins::kNumBands == 21, "insul: 21 third-octave bands");
    check(ins::kThirdOctaveHz[ins::kIsoFirst] == 100,
          "insul: ISO 717 starts at 100 Hz");
    check(ins::kThirdOctaveHz[ins::kIsoFirst + ins::kIsoCount - 1] == 3150,
          "insul: ISO 717 ends at 3150 Hz");
    check(ins::kThirdOctaveHz[ins::kAstmFirst] == 125,
          "insul: ASTM E413 starts at 125 Hz");
    check(ins::kThirdOctaveHz[ins::kAstmFirst + ins::kAstmCount - 1] == 4000,
          "insul: ASTM E413 ends at 4000 Hz");

    // ── ISO 717-1 の基準曲線 (規格 表 1) を独立に書き下す ──────────────
    const double isoRef[16] = { 33, 36, 39, 42, 45, 48, 51, 52,
                                53, 54, 55, 56, 56, 56, 56, 56 };
    double R[ins::kNumBands];
    for (int i = 0; i < ins::kNumBands; ++i) R[i] = 0;
    for (int i = 0; i < 16; ++i) R[ins::kIsoFirst + i] = isoRef[i];
    {
        // R が基準曲線そのものなら、基準曲線を s dB 上げたときの不利偏差は
        // ちょうど 16·s。上限 32 dB なので s = 2 → Rw = 52 + 2 = 54。
        const ins::RatingResult rw = ins::weightedReduction(R);
        check(rw.valid, "insul: Rw of the ISO reference curve is computable");
        check(rw.value == 54, "insul: Rw(reference curve) = 52 + 32/16 = 54");
        check(std::fabs(rw.sumDeficiency - 32.0) < 1e-6,
              "insul: the unfavourable deviations reach exactly 32 dB");
        check(rw.shift == 2, "insul: contour shift is +2 dB");
    }
    // 平行移動不変性: R を一様に k dB 上げれば Rw も k dB 上がる
    for (int k : { -7, 3, 11 }) {
        double Rk[ins::kNumBands];
        for (int i = 0; i < ins::kNumBands; ++i) Rk[i] = R[i] + k;
        check(ins::weightedReduction(Rk).value == 54 + k,
              "insul: Rw is translation-invariant");
        check(ins::soundTransmissionClass(Rk).value
                  == ins::soundTransmissionClass(R).value + k,
              "insul: STC is translation-invariant");
    }

    // ── ASTM E413 の基準等級曲線 (規格) を独立に書き下す ────────────────
    const double astm[16] = { -16, -13, -10, -7, -4, -1, 0, 1,
                              2, 3, 4, 4, 4, 4, 4, 4 };
    {
        double Ra[ins::kNumBands];
        for (int i = 0; i < ins::kNumBands; ++i) Ra[i] = 0;
        for (int i = 0; i < 16; ++i) Ra[ins::kAstmFirst + i] = astm[i] + 45;
        // 基準曲線そのもの → 16·s <= 32 で s = 2 → STC = 45 + 2 = 47
        check(ins::soundTransmissionClass(Ra).value == 47,
              "insul: STC(contour + 45) = 47");
        // 1 帯域だけ 30 dB 落とすと「1 帯域 8 dB 以下」の規定で頭打ちになる:
        // 315 Hz の基準値 −4、測定値 45−4−30 = 11 →
        // (−4 + s) − 11 <= 8  →  s <= 23  →  STC = 23
        Ra[ins::kAstmFirst + 4] -= 30;
        const ins::RatingResult stc = ins::soundTransmissionClass(Ra);
        check(stc.value == 23, "insul: STC is capped by the 8 dB single-band rule");
        check(stc.maxDeficiency <= 8.0 + 1e-9,
              "insul: no single deficiency exceeds 8 dB");
        // ISO 717-1 には 1 帯域の制限が無いので Rw のほうが高く出る
        check(ins::weightedReduction(Ra).value > stc.value,
              "insul: Rw has no single-band cap, so it exceeds STC here");
    }

    // ── ISO 717-2 の基準曲線 (規格 表 1) ───────────────────────────────
    {
        const double lnRef[16] = { 62, 62, 62, 62, 62, 62, 61, 60,
                                   59, 58, 57, 54, 51, 48, 45, 42 };
        double Ln[ins::kNumBands];
        for (int i = 0; i < ins::kNumBands; ++i) Ln[i] = 0;
        for (int i = 0; i < 16; ++i) Ln[ins::kIsoFirst + i] = lnRef[i];
        // 床衝撃音は「低いほど良い」ので基準曲線を下げる: 16·|s| <= 32,
        // s = −2 → Ln,w = 60 − 2 = 58
        check(ins::weightedImpact(Ln).value == 58,
              "insul: Ln,w(reference curve) = 60 - 32/16 = 58");
        for (int i = 0; i < 16; ++i) Ln[ins::kIsoFirst + i] = lnRef[i] - 5;
        check(ins::weightedImpact(Ln).value == 53,
              "insul: Ln,w drops 1 dB per 1 dB of impact level");
    }

    // ── C / Ctr: ISO 717-1 のスペクトルは Σ10^(Li/10) = 1 に正規化されている
    // ので、R が全帯域で一定 K のときは X_A = K、すなわち C = Ctr = K − Rw。
    {
        double Rf[ins::kNumBands];
        for (int i = 0; i < ins::kNumBands; ++i) Rf[i] = 40;
        const ins::RatingResult rw = ins::weightedReduction(Rf);
        bool okC = false, okCtr = false;
        const int C = ins::spectrumAdaptation(Rf, ins::SpectrumPink,
                                              rw.value, &okC);
        const int Ctr = ins::spectrumAdaptation(Rf, ins::SpectrumTraffic,
                                                rw.value, &okCtr);
        check(okC && okCtr, "insul: C / Ctr are computable");
        check(rw.value + C == 40,
              "insul: for a flat R the pink spectrum gives X_A = R");
        check(rw.value + Ctr == 40,
              "insul: for a flat R the traffic spectrum gives X_A = R");
    }

    // ── 質量則: 面密度 2 倍 / 周波数 2 倍で +6 dB (20log10 2) ────────────
    {
        const double m = 20.0;
        const double a = ins::fieldIncidenceMassLaw(500, m);
        check(std::fabs(ins::fieldIncidenceMassLaw(500, 2 * m) - a
                        - 20.0 * std::log10(2.0)) < 1e-9,
              "insul: mass law gains 6 dB per doubling of surface mass");
        check(std::fabs(ins::fieldIncidenceMassLaw(1000, m) - a
                        - 20.0 * std::log10(2.0)) < 1e-9,
              "insul: mass law gains 6 dB per octave");
    }

    // ── 限界周波数: fc = c²/(2π)·√(12ρ(1−ν²)/(E h²)) を独立に評価する ──
    {
        const double E = 30e9, nu = 0.2, rho = 2400, h = 0.15, c = 343.0;
        const double want = c * c / (2.0 * 3.14159265358979323846)
                          * std::sqrt(12.0 * rho * (1.0 - nu * nu) / (E * h * h));
        const double got = ins::criticalFrequency(E, nu, rho, h);
        check(std::fabs(got - want) < 1e-6 * want,
              "insul: critical frequency matches the closed form");
        // fc ∝ 1/h (同一材料) — 厚さ半分で 2 倍
        check(std::fabs(ins::criticalFrequency(E, nu, rho, h / 2) - 2 * got)
                  < 1e-6 * got,
              "insul: fc is inversely proportional to thickness");
        check(ins::criticalFrequency(0, nu, rho, h) == 0.0,
              "insul: no Young's modulus -> no critical frequency");
    }

    // ── 単一壁: 0.5·fc 未満は質量則そのもの ─────────────────────────────
    {
        std::vector<ins::Layer> layers;
        ins::Layer L;
        L.thicknessM = 0.150; L.densityKgM3 = 2400; L.youngsPa = 30e9;
        L.poisson = 0.2; L.lossFactor = 0.006;
        layers.push_back(L);
        const ins::TlResult tl = ins::transmissionLoss(layers);
        check(tl.valid && tl.model == ins::ModelSingleLeaf,
              "insul: a single solid layer uses the single-leaf model");
        check(std::fabs(tl.surfaceMass - 360.0) < 1e-9,
              "insul: surface mass = thickness x density");
        int belowChecked = 0;
        for (int i = 0; i < ins::kNumBands; ++i) {
            if (ins::kThirdOctaveHz[i] >= 0.5 * tl.leafCriticalHz[0]) continue;
            ++belowChecked;
            check(std::fabs(tl.R[i]
                            - ins::fieldIncidenceMassLaw(ins::kThirdOctaveHz[i],
                                                         tl.surfaceMass)) < 1e-9,
                  "insul: below 0.5 fc the single leaf follows the mass law");
        }
        check(belowChecked > 0, "insul: some bands lie below 0.5 fc");
        // 質量を倍にすれば Rw は上がる (単調性)
        layers[0].thicknessM = 0.300;
        check(ins::weightedReduction(ins::transmissionLoss(layers).R).value
                  > ins::weightedReduction(tl.R).value,
              "insul: doubling the slab thickness raises Rw");
    }

    // ── 二重壁 vs 同質量の単一壁、および構造結合時のフォールバック ──────
    {
        std::vector<ins::Layer> stack;
        ins::Layer board;
        board.thicknessM = 0.0125; board.densityKgM3 = 720;
        board.youngsPa = 2.5e9; board.poisson = 0.3; board.lossFactor = 0.015;
        ins::Layer gap;
        gap.thicknessM = 0.065; gap.densityKgM3 = 32;
        gap.cavity = true; gap.porousFill = true;
        stack.push_back(board);
        stack.push_back(gap);
        stack.push_back(board);

        const ins::TlResult dbl = ins::transmissionLoss(stack, true);
        check(dbl.valid && dbl.model == ins::ModelDoubleLeaf,
              "insul: a cavity between two boards gives the double-leaf model");
        check(dbl.leafCount == 2, "insul: two leaves detected");
        check(dbl.cavityAbsorbed, "insul: the porous fill is detected");
        check(std::fabs(dbl.cavityDepthM - 0.065) < 1e-12,
              "insul: cavity depth comes from the gap layers");
        // f0 = (1/2pi)*sqrt(1.4e5*(m1+m2)/(d*m1*m2)) を独立に評価する
        const double m1 = 0.0125 * 720, m2 = m1;
        const double f0 = 1.0 / (2.0 * 3.14159265358979323846)
                        * std::sqrt(1.4e5 * (m1 + m2) / (0.065 * m1 * m2));
        check(std::fabs(dbl.massAirMassHz - f0) < 1e-6 * f0,
              "insul: mass-air-mass resonance matches the closed form");
        check(std::fabs(dbl.limitingHz - 55.0 / 0.065) < 1e-9,
              "insul: limiting frequency fl = 55/d");

        const ins::TlResult rigid = ins::transmissionLoss(stack, false);
        check(rigid.valid && rigid.model == ins::ModelSingleLeaf,
              "insul: rigid ties collapse the stack to a single leaf");
        check(std::fabs(rigid.surfaceMass - (m1 + m2)) < 1e-9,
              "insul: the collapsed leaf keeps the combined surface mass");
        // 分離された二重壁は同じ質量の単一壁より Rw が高い
        check(ins::weightedReduction(dbl.R).value
                  > ins::weightedReduction(rigid.R).value,
              "insul: a decoupled double leaf beats a single leaf of equal mass");
        // 吸音材の無い空隙は cavityAbsorbed = false になる
        stack[1].densityKgM3 = 0;
        stack[1].porousFill = false;
        check(!ins::transmissionLoss(stack, true).cavityAbsorbed,
              "insul: an empty cavity reports no absorptive fill");
        // 葉が 3 枚あるときは最大空隙で 2 葉に集約したことを申告する
        std::vector<ins::Layer> triple = stack;
        ins::Layer wide = gap;
        wide.thicknessM = 0.200;
        triple.push_back(wide);
        triple.push_back(board);
        const ins::TlResult tri = ins::transmissionLoss(triple, true);
        check(tri.leafCount == 3 && tri.reducedToTwoLeaves,
              "insul: three leaves are reduced to two and flagged");
        check(std::fabs(tri.cavityDepthM - 0.200) < 1e-12,
              "insul: the reduction splits at the widest cavity");
        // 有効な層が無ければ「計算しない」
        check(!ins::transmissionLoss(std::vector<ins::Layer>()).valid,
              "insul: an empty build-up yields no prediction");
    }

    // ── 複合壁 / 現場の標準式 ───────────────────────────────────────────
    {
        const double areas[2] = { 50.0, 50.0 };
        const double same[2] = { 45.0, 45.0 };
        check(std::fabs(ins::compositeReduction(areas, same, 2) - 45.0) < 1e-9,
              "insul: equal panels give the same composite R");
        // 開口 (R = 0 → τ = 1) 1 m² と遮音壁 99 m²:
        //   tau = (99*10^-6 + 1)/100 → R = -10log10(tau)
        const double a2[2] = { 99.0, 1.0 };
        const double r2[2] = { 60.0, 0.0 };
        const double want = -10.0 * std::log10((99.0 * 1e-6 + 1.0) / 100.0);
        check(std::fabs(ins::compositeReduction(a2, r2, 2) - want) < 1e-9,
              "insul: a 1% opening dominates the composite R");
        // Lp2 = Lp1 - R + 10log10(S/A)
        check(std::fabs(ins::receivingLevel(75, 40, 12.15, 10.0)
                        - (75 - 40 + 10.0 * std::log10(12.15 / 10.0))) < 1e-9,
              "insul: receiving level follows Lp1 - R + 10log10(S/A)");
        check(std::fabs(ins::sabineAbsorption(100, 0.5) - 0.161 * 100 / 0.5)
                  < 1e-12,
              "insul: A = 0.161 V / T");
        // DnT,w = Rw + 10log10(0.32 V/S); 0.32V = S のとき DnT,w = Rw
        check(std::fabs(ins::standardizedLevelDifference(50, 100.0, 32.0) - 50.0)
                  < 1e-9,
              "insul: DnT,w equals Rw when 0.32 V = S");
        // 囲い: 開口ゼロ・内部吸音 1.0 なら IL = R、alpha = 0.1 なら R - 10
        check(std::fabs(ins::enclosureInsertionLoss(30, 10, 0, 1.0) - 30.0) < 1e-9,
              "insul: IL = R for a fully absorptive enclosure with no openings");
        check(std::fabs(ins::enclosureInsertionLoss(30, 10, 0, 0.1) - 20.0) < 1e-9,
              "insul: IL loses 10 dB when the interior absorption drops to 0.1");
        // 開口があると IL は開口率で頭打ちになる (壁 R を無限に上げても)
        const double capped = ins::enclosureInsertionLoss(200, 99, 1, 1.0);
        check(std::fabs(capped - 20.0) < 0.01,
              "insul: a 1% opening caps the enclosure IL at ~20 dB");
    }

    // ── ダクト (ASHRAE) ────────────────────────────────────────────────
    {
        // Sabine の式: alpha = 1, P/A = 1 → 1.05 dB/m。alpha に単調増加。
        check(std::fabs(ins::linedDuctAttenuation(1.0, 1.0, 1.0) - 1.05) < 1e-9,
              "insul: Sabine duct attenuation is 1.05 dB/m at alpha = 1");
        check(ins::linedDuctAttenuation(0.3, 1.3, 0.1)
                  < ins::linedDuctAttenuation(0.6, 1.3, 0.1),
              "insul: duct lining attenuation grows with alpha");
        check(ins::linedDuctAttenuation(0.0, 1.3, 0.1) == 0.0,
              "insul: no absorption -> no lining attenuation");
        // 分岐: 半分の断面へ入るなら 3.01 dB
        check(std::fabs(ins::branchAttenuation(0.5, 1.0)
                        - 10.0 * std::log10(2.0)) < 1e-9,
              "insul: halving the branch area costs 3 dB");
        check(ins::branchAttenuation(1.0, 1.0) == 0.0,
              "insul: no branching -> no loss");
        // 開口端反射: ka >= 1 で 0、フランジなしはフランジ付き +3.01 dB
        const double area = 0.1;
        check(ins::endReflectionLoss(125, area, true)
                  > ins::endReflectionLoss(500, area, true),
              "insul: end reflection loss falls with frequency");
        check(std::fabs(ins::endReflectionLoss(125, area, false)
                        - ins::endReflectionLoss(125, area, true)
                        - 10.0 * std::log10(2.0)) < 1e-9,
              "insul: an unflanged end reflects 3 dB more");
        check(ins::endReflectionLoss(4000, area, true) == 0.0,
              "insul: no end reflection loss once ka >= 1");
        // エルボ: ASHRAE の表は f·w (w はインチ) で区切られる。
        // f·w < 48 in·Hz (= 63 Hz で幅 19 mm 未満) は 0 dB。
        check(ins::elbowAttenuation(63, 0.015, false) == 0.0,
              "insul: a small f*w elbow gives no attenuation");
        check(ins::elbowAttenuation(63, 0.030, false) > 0.0,
              "insul: crossing f*w = 48 in.Hz starts to attenuate");
        check(ins::elbowAttenuation(1000, 0.4, false) > 0.0,
              "insul: a large f*w elbow attenuates");
        // 拡散音場: Lp = LW + 10log10(4/A)
        check(std::fabs(ins::reverberantLevel(80, 40.0)
                        - (80 + 10.0 * std::log10(4.0 / 40.0))) < 1e-9,
              "insul: reverberant level follows LW + 10log10(4/A)");
    }

    // ── STI (IEC 60268-16 MTF 法) ──────────────────────────────────────
    {
        // 残響ゼロ・十分な S/N なら MTF = 1 → STI = Sum(alpha) - Sum(beta) = 1
        check(ins::sti(0.0, 50.0) > 0.999,
              "insul: STI tends to 1 as RT60 -> 0 with a high S/N");
        check(ins::sti(0.0, 50.0) <= 1.0, "insul: STI never exceeds 1");
        // S/N が十分低ければ STI = 0
        check(ins::sti(1.0, -30.0) < 0.001,
              "insul: STI collapses to 0 at a very low S/N");
        // RT60 に対して単調減少、S/N に対して単調増加
        double prev = 2.0;
        for (double rt : { 0.2, 0.5, 1.0, 2.0, 4.0 }) {
            const double s = ins::sti(rt, 20.0);
            check(s < prev, "insul: STI decreases monotonically with RT60");
            prev = s;
        }
        double prevS = -1.0;
        for (double snr : { -10.0, 0.0, 10.0, 20.0 }) {
            const double s = ins::sti(0.8, snr);
            check(s > prevS, "insul: STI increases monotonically with S/N");
            prevS = s;
        }
        // 帯域別 API に同じ値を渡せば一括版と一致する
        double rts[7], snrs[7];
        for (int i = 0; i < 7; ++i) { rts[i] = 0.8; snrs[i] = 12.0; }
        check(ins::stiBands(rts, snrs) == ins::sti(0.8, 12.0),
              "insul: stiBands with uniform inputs equals sti()");
        // 決定性 (同一入力 → 同一ビット列)
        check(ins::sti(0.73, 7.5) == ins::sti(0.73, 7.5),
              "insul: sti is deterministic");
    }
}

static void testFdeModeSolver()
{
    using namespace ofd::optics;
    using fdetest::slabExact;
    using fdetest::discLapEig;
    using fdetest::slabSection;
    g_file = "fde";

    const double lam = 1.55;                       // λ = 1.55 um
    const double k0 = 2.0 * fdetest::kPi / lam;
    const double nSi = 3.476, nOx = 1.444;         // Si / SiO2 @1550nm
    const int    nx = 24;                          // x 窓 1.2 um
    const double dx = 0.05;
    const double muX = discLapEig(nx, dx, 1);      // x 方向の離散量子化分

    // ── (1) 対称スラブ TE0/TM0 が厳密解と一致すること
    //        + (2) 格子細分化で誤差が 1/4 になること (= 2 次の離散化誤差)
    //
    // 実測 (t=0.22um, Si/SiO2, λ=1.55um):
    //     TE0 : dy=10nm → +1.0214e-3, dy=5nm → +2.5555e-4  (比 0.250)
    //     TM0 : dy=10nm → +1.8604e-3, dy=5nm → +4.6484e-4  (比 0.250)
    // 許容差は実測値の約 1.4 倍に置く (厳密解 2.8477822434 / 2.0533196788)。
    // 比が 4 に乗ることが「差分スキームのバグではなく離散化誤差」の証拠。
    for (int tm = 0; tm < 2; ++tm) {
        const double exact = slabExact(nSi, nOx, 0.22, lam, 0, tm != 0);
        check(exact > nOx && exact < nSi, tm ? "fde: TM0 exact in range"
                                             : "fde: TE0 exact in range");
        double err[2] = { 0.0, 0.0 };
        const double dys[2] = { 0.01, 0.005 };
        for (int s = 0; s < 2; ++s) {
            CrossSection cs = slabSection(nSi, nOx, 0.22, dys[s], 1.5, nx, dx);
            SolveOptions o;
            o.modes = 1;
            o.pol = tm ? Polarization::SemiVecTM : Polarization::SemiVecTE;
            const std::vector<ModeResult> r = solveModes(cs, lam, o);
            if (r.size() != 1) {
                check(false, tm ? "fde: TM0 slab solved" : "fde: TE0 slab solved");
                continue;
            }
            check(r[0].guided, tm ? "fde: TM0 guided" : "fde: TE0 guided");
            // β²_1D = β²_2D − μ_x (x 方向の離散量子化を厳密に除去)
            const double b2 = k0 * k0 * r[0].neff * r[0].neff;
            const double neff1d = std::sqrt(b2 - muX) / k0;
            err[s] = std::fabs(neff1d - exact);

            // 閉込め係数と実効断面積の妥当域
            check(r[0].gamma > 0.0 && r[0].gamma < 1.0,
                  tm ? "fde: TM0 gamma in (0,1)" : "fde: TE0 gamma in (0,1)");
            check(r[0].aeff_um2 > 0.0, tm ? "fde: TM0 aeff positive"
                                          : "fde: TE0 aeff positive");
            // 場の離散 L2 ノルムは 1、強度の最大値は 1
            double nn = 0.0, imax = 0.0;
            for (size_t i = 0; i < r[0].field.size(); ++i) {
                nn += r[0].field[i] * r[0].field[i];
                imax = std::max(imax, r[0].intensity[i]);
            }
            check(std::fabs(nn - 1.0) < 1e-9,
                  tm ? "fde: TM0 field normalised" : "fde: TE0 field normalised");
            check(std::fabs(imax - 1.0) < 1e-12,
                  tm ? "fde: TM0 intensity peak 1" : "fde: TE0 intensity peak 1");

            // 半ベクトルの不連続扱いが効く向きの確認:
            // 界面に平行な TE のほうがコアへよく閉じ込められる (Γ_TE > Γ_TM)
            if (s == 0) {
                check(tm ? (r[0].gamma < 0.30) : (r[0].gamma > 0.70),
                      tm ? "fde: TM0 weakly confined" : "fde: TE0 strongly confined");
            }
        }
        check(err[0] < (tm ? 2.5e-3 : 1.5e-3),
              tm ? "fde: TM0 matches exact slab (dy=10nm)"
                 : "fde: TE0 matches exact slab (dy=10nm)");
        check(err[1] < (tm ? 7.0e-4 : 4.0e-4),
              tm ? "fde: TM0 matches exact slab (dy=5nm)"
                 : "fde: TE0 matches exact slab (dy=5nm)");
        // 2 次収束 (実測 0.250)。0.30 を切れば 1 次以下ではないと言える。
        check(err[1] < 0.30 * err[0],
              tm ? "fde: TM0 error is 2nd-order discretisation"
                 : "fde: TE0 error is 2nd-order discretisation");
    }

    // ── (1b) 屈折率が x に一様なら半ベクトル TE はスカラーに厳密一致する
    //         (調和平均の係数が通常の 2 階中心差分へ縮退することの検査)
    //         実測差 1.8e-15。
    {
        CrossSection cs = slabSection(nSi, nOx, 0.22, 0.01, 1.5, nx, dx);
        SolveOptions o; o.modes = 1;
        o.pol = Polarization::SemiVecTE;
        const std::vector<ModeResult> a = solveModes(cs, lam, o);
        o.pol = Polarization::Scalar;
        const std::vector<ModeResult> b = solveModes(cs, lam, o);
        check(a.size() == 1 && b.size() == 1, "fde: scalar/TE both solved");
        if (a.size() == 1 && b.size() == 1)
            check(std::fabs(a[0].neff - b[0].neff) < 1e-12,
                  "fde: semi-vector TE degenerates to scalar when n(x) is flat");
    }

    // ── (3) 高次モード: V 数から決まる本数だけ立ち、neff は降順
    //        + (4) モード同士が離散内積で直交すること
    //
    // t=1.0um の対称スラブは V/π = 4.08 → TE0..TE4 の 5 本 (y 方向の次数)。
    // 断面は x に一様なので離散問題は厳密に分離し、固有値は全組合せ
    //     β²(m,p) = μ_x(m) + k0² neff_slab(p)²    (m = x の次数, p = y の次数)
    // になる。x 窓を 0.5um と狭くしてあるので μ_x(m) の間隔が大きく、導波条件
    // (neff > クラッド) を満たすのは (1,0)(1,1)(1,2)(1,3)(2,0) の 5 個だけ。
    // 6 本要求してもこの 5 本しか返らないことと、値・順序が一致することを見る。
    // 実測: 最大差 2.006e-3 (dy=10nm)、モード間内積の最大 5.4e-16。
    {
        const int mx = 25;
        const double mdx = 0.02, mdy = 0.01, t = 1.0;
        // スラブ次数の本数は V 数で決まる: V = k0 t √(n1²−n2²) = 12.82,
        // ⌈V/π⌉ = 5 → TE0..TE4 の 5 本。
        std::vector<double> slabNe;
        for (int p = 0; p < 12; ++p) {
            const double ne = slabExact(nSi, nOx, t, lam, p, false);
            if (ne < 0.0) break;
            slabNe.push_back(ne);
        }
        const double V = k0 * t * std::sqrt(nSi * nSi - nOx * nOx);
        check(int(slabNe.size()) == int(std::ceil(V / fdetest::kPi)),
              "fde: slab order count follows V-number");
        check(slabNe.size() == 5, "fde: 1.0um Si slab has 5 TE orders");

        // 期待スペクトルは分離解の全組合せ β²(m,p) = μ_x(m) + k0² neff_slab(p)²
        // のうち導波条件 (neff > クラッド屈折率) を満たすもの。降順に並べる。
        std::vector<double> pred;
        for (int m = 1; m <= 4; ++m)
            for (size_t p = 0; p < slabNe.size(); ++p) {
                const double b2 = discLapEig(mx, mdx, m)
                                + k0 * k0 * slabNe[p] * slabNe[p];
                if (b2 <= 0.0) continue;
                const double ne = std::sqrt(b2) / k0;
                if (ne > nOx) pred.push_back(ne);
            }
        std::sort(pred.begin(), pred.end(),
                  [](double a, double b) { return a > b; });
        check(pred.size() == 5, "fde: separable spectrum predicts 5 guided states");

        CrossSection cs = slabSection(nSi, nOx, t, mdy, 1.0, mx, mdx);
        SolveOptions o; o.modes = 6; o.pol = Polarization::Scalar;
        const std::vector<ModeResult> r = solveModes(cs, lam, o);
        // 6 本要求しても存在するのは 5 本 — 無いモードを作らないことの検査
        check(r.size() == pred.size(),
              "fde: solver returns exactly the predicted guided states");
        double worst = 0.0;
        bool desc = true;
        for (size_t i = 0; i < r.size() && i < pred.size(); ++i) {
            worst = std::max(worst, std::fabs(r[i].neff - pred[i]));
            if (i > 0 && !(r[i].neff < r[i - 1].neff)) desc = false;
        }
        check(desc, "fde: modes sorted by descending neff");
        check(worst < 3.0e-3, "fde: higher-order neff match separable spectrum");

        double ortho = 0.0;
        for (size_t i = 0; i < r.size(); ++i)
            for (size_t j = i + 1; j < r.size(); ++j) {
                double s = 0.0;
                for (size_t q = 0; q < r[i].field.size(); ++q)
                    s += r[i].field[q] * r[j].field[q];
                ortho = std::max(ortho, std::fabs(s));
            }
        check(ortho < 1e-10, "fde: modes mutually orthogonal");

        // Γ は 0..1。先頭 4 本は同じ x 次数で y の次数だけが上がる列なので、
        // 高次ほどコアからしみ出す = Γ は単調減少になる
        // (実測 0.9938 → 0.9738 → 0.9339 → 0.8508)。
        // 5 本目は x 次数が上がったもので y 分布は基本モードと同じため、
        // Γ もほぼ同じ値に戻る — ここに単調性を期待してはいけない。
        for (size_t i = 0; i < r.size(); ++i)
            check(r[i].gamma >= 0.0 && r[i].gamma <= 1.0, "fde: gamma in [0,1]");
        for (size_t i = 1; i < r.size() && i < 4; ++i)
            check(r[i].gamma < r[i - 1].gamma,
                  "fde: higher y-order leaks out of the core more");
    }

    // ── (5) 2D 矩形コア: 実効屈折率法 (EIM) と突き合わせ + 場の左右対称性
    //
    // EIM は独立な準解析基準 (y 方向スラブ → その neff をコアとする x 方向
    // スラブ。x へ移ると界面に対する電界の向きが入れ替わるので偏波も入れ替える)。
    // Si 導波路では数 % の近似なので、一致は「桁と傾向」の検査として使う。
    // 実測差: 450x220 TE −0.0010 / TM +0.0016、900x220 TE +0.0039。
    {
        const struct { double w; bool tm; int nGuided; } kCase[] = {
            { 0.45, false, 1 }, { 0.45, true, 1 }, { 0.90, false, 2 }
        };
        for (const auto &cse : kCase) {
            CrossSection cs = makeRectangularCore(cse.w, 0.22, 0.0,
                                                  nSi, nOx, nOx, 0.02, 1.0);
            check(cs.nx > 0 && cs.ny > 0 && int(cs.n.size()) == cs.nx * cs.ny,
                  "fde: makeRectangularCore builds a consistent grid");
            SolveOptions o; o.modes = 4;
            o.pol = cse.tm ? Polarization::SemiVecTM : Polarization::SemiVecTE;
            const std::vector<ModeResult> r = solveModes(cs, lam, o);
            check(int(r.size()) == cse.nGuided,
                  "fde: rectangular core guided-mode count");
            if (r.empty()) continue;

            const double nSlab = slabExact(nSi, nOx, 0.22, lam, 0, cse.tm);
            const double eim = slabExact(nSlab, nOx, cse.w, lam, 0, !cse.tm);
            check(eim > 0.0, "fde: EIM reference exists");
            check(std::fabs(r[0].neff - eim) < 0.01,
                  "fde: 2D neff agrees with effective-index method");
            check(r[0].neff > nOx && r[0].neff < nSi,
                  "fde: 2D neff between cladding and core index");
            check(r[0].guided, "fde: fundamental 2D mode is guided");

            // 左右対称な断面 → 強度分布も左右対称 (実測 ≤ 4.5e-5)
            double sym = 0.0;
            for (int iy = 0; iy < cs.ny; ++iy)
                for (int ix = 0; ix < cs.nx; ++ix)
                    sym = std::max(sym, std::fabs(
                        r[0].intensity[size_t(iy) * cs.nx + ix]
                      - r[0].intensity[size_t(iy) * cs.nx + (cs.nx - 1 - ix)]));
            check(sym < 2e-3, "fde: intensity is left-right symmetric");
        }
    }

    // ── リブ (スラブ付き) 断面も解けること。コアより neff が下がり、
    //    ストリップより閉込めが弱くなる。
    {
        CrossSection strip = makeRectangularCore(0.50, 0.22, 0.00,
                                                 nSi, nOx, nOx, 0.02, 1.0);
        CrossSection rib   = makeRectangularCore(0.50, 0.22, 0.09,
                                                 nSi, nOx, nOx, 0.02, 1.0);
        SolveOptions o; o.modes = 2; o.pol = Polarization::SemiVecTE;
        const std::vector<ModeResult> a = solveModes(strip, lam, o);
        const std::vector<ModeResult> b = solveModes(rib, lam, o);
        check(!a.empty() && !b.empty(), "fde: strip and rib both solved");
        if (!a.empty() && !b.empty()) {
            // スラブが付くと側方のクラッドが Si に置き換わるので neff は上がる
            check(b[0].neff > a[0].neff, "fde: rib raises neff over strip");
            check(b[0].gamma > 0.0 && b[0].gamma < 1.0, "fde: rib gamma in (0,1)");
        }
    }

    // ── (6) 決定性: 同じ入力を 2 回解いて完全一致 (乱数を使っていないこと)
    {
        CrossSection cs = makeRectangularCore(0.45, 0.22, 0.0,
                                              nSi, nOx, nOx, 0.02, 1.0);
        SolveOptions o; o.modes = 2; o.pol = Polarization::SemiVecTE;
        const std::vector<ModeResult> a = solveModes(cs, lam, o);
        const std::vector<ModeResult> b = solveModes(cs, lam, o);
        bool same = (a.size() == b.size());
        for (size_t i = 0; same && i < a.size(); ++i) {
            same = same && a[i].neff == b[i].neff
                        && a[i].gamma == b[i].gamma
                        && a[i].aeff_um2 == b[i].aeff_um2
                        && a[i].guided == b[i].guided
                        && a[i].field.size() == b[i].field.size();
            for (size_t q = 0; same && q < a[i].field.size(); ++q)
                same = same && a[i].field[q] == b[i].field[q]
                            && a[i].intensity[q] == b[i].intensity[q];
        }
        check(!a.empty() && same, "fde: solver is bit-for-bit deterministic");
    }

    // ── 異常入力: 解けないものは «作らない» (空を返す)
    {
        CrossSection empty;
        check(solveModes(empty, lam, SolveOptions()).empty(),
              "fde: empty cross-section yields no modes");
        CrossSection cs = makeRectangularCore(0.45, 0.22, 0.0,
                                              nSi, nOx, nOx, 0.02, 1.0);
        SolveOptions o; o.modes = 0;
        check(solveModes(cs, lam, o).empty(), "fde: modes=0 yields no modes");
        o.modes = 2;
        check(solveModes(cs, -1.0, o).empty(),
              "fde: non-positive wavelength yields no modes");
        check(makeRectangularCore(0.0, 0.22, 0.0, nSi, nOx, nOx, 0.02, 1.0)
                  .nx == 0,
              "fde: degenerate core size yields empty section");
    }
}

// ── 直方体室の音響モード (src/acoustics/core/RoomModes) ─────────────────────
// 期待値は実装から読まず、1 次元極限・既知の閉形式・単調性から独立に立てる。
static void testRoomModes()
{
    namespace rm = ofd::acoustics::roommodes;
    g_file = "room-modes";

    // ── 音速 (ISO 9613-1): 0 ℃ で 331.3 m/s、20 ℃ で 343.2 m/s ─────────
    check(std::fabs(rm::soundSpeed(0.0) - 331.3) < 1e-9,
          "modes: c(0 °C) = 331.3 m/s");
    check(std::fabs(rm::soundSpeed(20.0) - 343.2) < 0.1,
          "modes: c(20 °C) ≈ 343.2 m/s");
    check(rm::soundSpeed(-300.0) == 0.0,
          "modes: below absolute zero yields no sound speed");

    const double c = 343.0;

    // ── 1 次元極限: 他の 2 辺を非常に長くすると f(1,0,0) → c/(2L) ───────
    {
        const double L = 2.4;
        const double f100 = rm::modeFrequency(1, 0, 0, L, 1000.0, 1000.0, c);
        check(std::fabs(f100 - c / (2.0 * L)) < 1e-9,
              "modes: axial mode (1,0,0) equals c/(2L)");
        // 次数 n の軸モードは基本波の n 倍 (弦・気柱と同じ調和列)
        for (int n = 2; n <= 5; ++n) {
            const double fn = rm::modeFrequency(n, 0, 0, L, 3.0, 2.0, c);
            check(std::fabs(fn - n * c / (2.0 * L)) < 1e-9,
                  "modes: axial series is harmonic (n·c/2L)");
        }
    }

    // ── 立方体の (1,1,0) / (1,1,1) は解析的に √2, √3 倍 ─────────────────
    {
        const double a = 3.0;
        const double f1 = rm::modeFrequency(1, 0, 0, a, a, a, c);
        const double f2 = rm::modeFrequency(1, 1, 0, a, a, a, c);
        const double f3 = rm::modeFrequency(1, 1, 1, a, a, a, c);
        check(std::fabs(f2 - std::sqrt(2.0) * f1) < 1e-9,
              "modes: cube tangential = √2 × axial");
        check(std::fabs(f3 - std::sqrt(3.0) * f1) < 1e-9,
              "modes: cube oblique = √3 × axial");
    }

    // ── 種別の分類と昇順・網羅性 ────────────────────────────────────────
    {
        const double L = 2.4, W = 1.45, H = 1.15, fmax = 200.0;
        const std::vector<rm::Mode> v =
            rm::rectangularModes(L, W, H, c, fmax, 0);
        check(!v.empty(), "modes: rectangular cabin has modes below 200 Hz");

        bool sorted = true, inRange = true, kindOk = true, hasZero = false;
        for (size_t i = 0; i < v.size(); ++i) {
            if (i > 0 && v[i].freqHz < v[i - 1].freqHz) sorted = false;
            if (v[i].freqHz <= 0.0 || v[i].freqHz > fmax) inRange = false;
            const int nz = (v[i].nx > 0 ? 1 : 0) + (v[i].ny > 0 ? 1 : 0)
                           + (v[i].nz > 0 ? 1 : 0);
            if (v[i].kind != nz) kindOk = false;
            if (v[i].nx == 0 && v[i].ny == 0 && v[i].nz == 0) hasZero = true;
        }
        check(sorted, "modes: list is sorted by frequency");
        check(inRange, "modes: every mode lies in (0, fmax]");
        check(kindOk, "modes: kind = number of non-zero orders");
        check(!hasZero, "modes: the (0,0,0) trivial solution is excluded");

        // 最低次は最長辺の軸モード (2.4 m → c/(2·2.4))
        check(v[0].kind == rm::ModeAxial && v[0].nx == 1
                  && v[0].ny == 0 && v[0].nz == 0,
              "modes: lowest mode is the axial mode along the longest side");
        check(std::fabs(v[0].freqHz - c / (2.0 * L)) < 1e-9,
              "modes: lowest mode frequency = c/(2·Lmax)");

        // 総数は独立な素朴列挙と一致する
        int brute = 0;
        for (int ix = 0; ix <= 20; ++ix)
            for (int iy = 0; iy <= 20; ++iy)
                for (int iz = 0; iz <= 20; ++iz) {
                    if (!ix && !iy && !iz) continue;
                    const double fx = ix / L, fy = iy / W, fz = iz / H;
                    const double f = 0.5 * c
                                     * std::sqrt(fx * fx + fy * fy + fz * fz);
                    if (f > 0.0 && f <= fmax) ++brute;
                }
        check(int(v.size()) == brute,
              "modes: enumeration matches an independent brute-force count");

        // maxModes は低次側からの打ち切り
        const std::vector<rm::Mode> top5 =
            rm::rectangularModes(L, W, H, c, fmax, 5);
        check(top5.size() == 5, "modes: maxModes truncates the list");
        bool sameHead = true;
        for (size_t i = 0; i < top5.size(); ++i)
            if (top5[i].freqHz != v[i].freqHz) sameHead = false;
        check(sameHead, "modes: truncation keeps the lowest modes");

        // 上限周波数を上げるとモードは増える一方 (単調)
        check(rm::rectangularModes(L, W, H, c, 2 * fmax, 0).size() > v.size(),
              "modes: raising fmax can only add modes");
        // 室を大きくするとモード周波数は下がる (総数は増える)
        check(rm::rectangularModes(2 * L, 2 * W, 2 * H, c, fmax, 0).size()
                  > v.size(),
              "modes: a larger room has more modes below a given frequency");
    }

    // ── 異常入力は «計算しない» (0 / 空を返す) ──────────────────────────
    check(rm::modeFrequency(0, 0, 0, 3, 3, 3, c) == 0.0,
          "modes: (0,0,0) has no frequency");
    check(rm::modeFrequency(1, 0, 0, -1, 3, 3, c) == 0.0,
          "modes: negative dimension yields no frequency");
    check(rm::rectangularModes(0, 1, 1, c, 200, 0).empty(),
          "modes: zero dimension yields no modes");
    check(rm::rectangularModes(3, 3, 3, c, 0, 0).empty(),
          "modes: non-positive fmax yields no modes");
    check(rm::rectangularModes(3, 3, 3, 0, 200, 0).empty(),
          "modes: non-positive sound speed yields no modes");
}

// ── 屋外騒音伝搬 (src/acoustics/core/EnvironmentalNoise) ────────────────────
// 期待値は実装から読まず、規格の式・前川の原典・極限値から独立に立てる。
static void testEnvironmentalNoise()
{
    namespace en = ofd::acoustics::outdoor;
    g_file = "environmental-noise";

    // ── 幾何拡散 (ISO 9613-2 §7.1) ──────────────────────────────────────
    // A_div = 20 lg(d) + 11 は 10 lg(4πd²) と同じ (4π = 11.0 dB)
    {
        const double d = 7.5;
        const double expect = 10.0 * std::log10(4.0 * 3.14159265358979323846
                                                * d * d);
        check(std::fabs(en::divergencePoint(d) - expect) < 0.01,
              "outdoor: A_div(point) = 10 lg(4πd²)");
        // 線音源は 10 lg(2πd)
        const double expectL = 10.0 * std::log10(2.0 * 3.14159265358979323846
                                                 * d);
        check(std::fabs(en::divergenceLine(d) - expectL) < 0.02,
              "outdoor: A_div(line) = 10 lg(2πd)");
        check(en::divergencePoint(0.0) == 0.0 && en::divergenceLine(-1.0) == 0.0,
              "outdoor: non-positive distance yields no divergence term");
    }
    // 距離 2 倍で点音源 −6.02 dB / 線音源 −3.01 dB
    {
        const double a = en::divergenceRelative(50.0, 25.0, false);
        const double b = en::divergenceRelative(50.0, 25.0, true);
        check(std::fabs(a - 6.0206) < 1e-3,
              "outdoor: point source loses 6.02 dB per distance doubling");
        check(std::fabs(b - 3.0103) < 1e-3,
              "outdoor: line source loses 3.01 dB per distance doubling");
        check(std::fabs(en::divergenceRelative(25.0, 25.0, false)) < 1e-12,
              "outdoor: no attenuation at the reference distance");
        // 10 倍距離: 点 20 dB / 線 10 dB
        check(std::fabs(en::divergenceRelative(100.0, 10.0, false) - 20.0)
                  < 1e-9,
              "outdoor: ×10 distance = 20 dB (point)");
        check(std::fabs(en::divergenceRelative(100.0, 10.0, true) - 10.0)
                  < 1e-9,
              "outdoor: ×10 distance = 10 dB (line)");
    }
    // PWL → 1 m の音圧レベル
    check(std::fabs(en::pointSourceLevelAt1m(105.0) - 94.0) < 1e-9,
          "outdoor: Lp(1 m) = PWL − 11 dB");

    // ── 前川チャート ────────────────────────────────────────────────────
    // N → 0 の極限は 10 lg 3 = 4.77 dB (≈ 5 dB)
    {
        const double d0 = en::maekawaAttenuation(1e-9);
        check(std::fabs(d0 - 10.0 * std::log10(3.0)) < 1e-6,
              "outdoor: Maekawa → 10 lg 3 = 4.77 dB as N → 0");
        check(d0 > 4.7 && d0 < 4.8, "outdoor: Maekawa at N→0 is about 5 dB");
        // 代表点: N = 1 で 10 lg 23 = 13.6 dB
        check(std::fabs(en::maekawaAttenuation(1.0)
                        - 10.0 * std::log10(23.0)) < 1e-9,
              "outdoor: Maekawa at N = 1 is 10 lg 23 dB");
        // 単調増加、24 dB で頭打ち
        double prev = 0;
        bool mono = true;
        for (int i = 1; i <= 200; ++i) {
            const double v = en::maekawaAttenuation(i * 0.25);
            if (v < prev - 1e-12) mono = false;
            prev = v;
        }
        check(mono, "outdoor: Maekawa is monotonic in N");
        check(en::maekawaAttenuation(1e6) == en::kMaekawaMaxDb,
              "outdoor: Maekawa saturates at the chart limit");
        check(en::maekawaAttenuation(0.0) == 0.0
                  && en::maekawaAttenuation(-1.0) == 0.0,
              "outdoor: no diffraction loss without shadowing");
    }
    // フレネル数 N = 2δ/λ
    {
        const double c = 343.2, f = 500.0;
        const double lambda = c / f;
        check(std::fabs(en::fresnelNumber(lambda, f, c) - 2.0) < 1e-9,
              "outdoor: N = 2 when the path difference equals λ");
        check(en::fresnelNumber(1.0, 0.0, c) == 0.0,
              "outdoor: no Fresnel number without a frequency");
    }

    // ── 遮蔽幾何 ────────────────────────────────────────────────────────
    {
        const double c = 343.2;
        en::BarrierGeometry g;
        g.srcHeightM = 0.5; g.barDistM = 5.0; g.barHeightM = 3.0;
        g.recvDistM = 25.0; g.recvHeightM = 1.2;
        const en::BarrierResult r = en::barrierDiffraction(g, 500.0, c);
        check(r.valid && r.shadow, "outdoor: a 3 m wall shadows the receiver");
        // 経路差を独立に計算する
        const double a = std::sqrt(5.0 * 5.0 + 2.5 * 2.5);
        const double b = std::sqrt(20.0 * 20.0 + 1.8 * 1.8);
        const double d = std::sqrt(25.0 * 25.0 + 0.7 * 0.7);
        check(std::fabs(r.pathDiffM - (a + b - d)) < 1e-9,
              "outdoor: path difference matches the geometry");
        check(std::fabs(r.fresnelN - 2.0 * r.pathDiffM / (c / 500.0)) < 1e-9,
              "outdoor: N = 2δ/λ from the geometry");
        check(std::fabs(r.attenDb - en::maekawaAttenuation(r.fresnelN)) < 1e-12,
              "outdoor: ΔL comes from the Maekawa chart");
        // 高い周波数ほど N が大きく ΔL も大きい (λ が短い)
        const en::BarrierResult hi = en::barrierDiffraction(g, 2000.0, c);
        check(hi.fresnelN > r.fresnelN && hi.attenDb > r.attenDb,
              "outdoor: higher frequency gives more diffraction loss");
        // 壁を高くすると経路差が増える
        en::BarrierGeometry g2 = g;
        g2.barHeightM = 5.0;
        check(en::barrierDiffraction(g2, 500.0, c).pathDiffM > r.pathDiffM,
              "outdoor: a taller wall increases the path difference");
        // 見通しがあるときは ΔL = 0
        en::BarrierGeometry g3 = g;
        g3.barHeightM = 0.6;   // 音源 0.5 m と受音点 1.2 m を結ぶ線より低い
        const en::BarrierResult los = en::barrierDiffraction(g3, 500.0, c);
        check(los.valid && !los.shadow && los.attenDb == 0.0,
              "outdoor: no loss when the wall does not break the sight line");
        // 幾何が成立しない入力
        en::BarrierGeometry g4 = g;
        g4.barDistM = 30.0;    // 受音点より遠い
        check(!en::barrierDiffraction(g4, 500.0, c).valid,
              "outdoor: a wall beyond the receiver is not a valid section");
    }

    // ── 環境基準 (平成10年環境庁告示第64号) ─────────────────────────────
    // 期待値は告示の表そのもの (実装とは独立に書き下す)
    {
        const double day[7]   = { 50, 55, 55, 60, 60, 65, 70 };
        const double night[7] = { 40, 45, 45, 50, 55, 60, 65 };
        bool ok = true;
        for (int i = 0; i < 7; ++i) {
            const en::EnvStandard s = en::environmentalStandardJp(i);
            if (!s.valid || s.dayDb != day[i] || s.nightDb != night[i])
                ok = false;
        }
        check(ok, "outdoor: JP environmental standards match Notification 64");
        check(!en::environmentalStandardJp(-1).valid
                  && !en::environmentalStandardJp(7).valid,
              "outdoor: unknown area category has no limit value");
        // 夜間の基準値は必ず昼間以下
        bool nightLower = true;
        for (int i = 0; i < 7; ++i) {
            const en::EnvStandard s = en::environmentalStandardJp(i);
            if (s.nightDb > s.dayDb) nightLower = false;
        }
        check(nightLower, "outdoor: the night limit never exceeds the day one");
    }

    // ── 断面予測 ────────────────────────────────────────────────────────
    {
        en::SiteModel m;
        m.refLevelDb = 80.0;
        m.refDistM   = 10.0;
        m.lineSource = false;
        m.srcHeightM = 0.5;
        m.evalFreqHz = 500.0;
        m.soundSpeedMs = 343.2;

        // 壁なし: 距離 2 倍でちょうど 6.02 dB 下がる
        const double l10 = en::predictLevel(m, 10.0, 1.2).levelDb;
        const double l20 = en::predictLevel(m, 20.0, 1.2).levelDb;
        check(std::fabs(l10 - 80.0) < 1e-9,
              "outdoor: level equals the reference level at the reference "
              "distance");
        check(std::fabs((l10 - l20) - 6.0206) < 1e-3,
              "outdoor: −6.02 dB per doubling without a barrier");

        // 線音源にすると 3.01 dB
        en::SiteModel ml = m;
        ml.lineSource = true;
        check(std::fabs((en::predictLevel(ml, 10.0, 1.2).levelDb
                         - en::predictLevel(ml, 20.0, 1.2).levelDb) - 3.0103)
                  < 1e-3,
              "outdoor: −3.01 dB per doubling for a line source");

        // 壁を入れると必ず下がる (回折減衰の分だけ)
        en::SiteModel mb = m;
        mb.barrierEnabled = true;
        mb.barDistM = 5.0;
        mb.barHeightM = 3.0;
        const en::PredictionResult pb = en::predictLevel(mb, 25.0, 1.2);
        const en::PredictionResult pn = en::predictLevel(m, 25.0, 1.2);
        check(pb.valid && pb.aBarDb > 0.0,
              "outdoor: the barrier contributes an attenuation");
        check(std::fabs((pn.levelDb - pb.levelDb) - pb.aBarDb) < 1e-9,
              "outdoor: the barrier lowers the level by exactly ΔL");

        // A_div を外すと距離に依らず基準レベルのまま (項を計上しない)
        en::SiteModel md = m;
        md.divergenceEnabled = false;
        check(std::fabs(en::predictLevel(md, 500.0, 1.2).levelDb - 80.0) < 1e-12,
              "outdoor: disabling A_div removes the divergence term");

        // 逆問題: L(d) = target の距離を求め、そこで実際に target になる
        const double d55 = en::distanceForLevel(m, 55.0, 1.2, 0.5, 5000.0);
        check(d55 > 0.0, "outdoor: the 55 dB distance is found");
        check(std::fabs(en::predictLevel(m, d55, 1.2).levelDb - 55.0) < 1e-6,
              "outdoor: the level at that distance is indeed the target");
        // 点音源 80 dB @ 10 m → 55 dB は 10·10^(25/20) = 177.8 m
        check(std::fabs(d55 - 10.0 * std::pow(10.0, 25.0 / 20.0)) < 1e-3,
              "outdoor: the 55 dB distance matches the closed-form inverse");
        // 高いレベルほど近い距離になる (単調)
        check(en::distanceForLevel(m, 60.0, 1.2, 0.5, 5000.0) < d55,
              "outdoor: a higher target level lies closer to the source");
        // 区間内に解が無ければ «作らない»
        check(en::distanceForLevel(m, 200.0, 1.2, 0.5, 5000.0) == 0.0,
              "outdoor: an unreachable target yields no distance");
        check(!en::predictLevel(m, -1.0, 1.2).valid,
              "outdoor: a non-positive receiver distance is not computable");
    }
}

// ── 集束超音波の軸上音場 (src/acoustics/core/FocusedField) ──────────────────
// 期待値はすべてテスト側に独立に書く:
//   - 幾何焦点の音圧 = ρ c u0 k h  (O'Neil 1949 の閉形式)
//   - 軸上閉形式は焦点で上の値に連続的に一致する
//   - 弱集束 (a ≪ R) の極限で h → a²/(2R)、利得 kh → k a²/(2R)
//   - 電力 → 速度の換算は W = ½ρc u0² S を厳密に満たす
//   - 強度 I = p²/(2ρc)、減衰は α0 f^y·距離 [dB]
//   - MI は IEC 62359 の定義 (0.3 dB/cm/MHz デレーティング)
//   - Gol'dberg 数 Γ = 1/(α x_sh), x_sh = 1/(βεk), β = 1+B/2A
static void testFocusedField()
{
    using namespace ofd::acoustics::ultrasound;
    g_file = "focused-ultrasound";
    const double kPi = 3.14159265358979323846;

    Medium water;                 // 水 (無吸収にして解析解と厳密比較する)
    water.rho = 1000.0;
    water.c = 1500.0;
    water.alpha0_dBcmMHz = 0.0;
    water.alphaExponent = 2.0;
    water.bOverA = 5.0;

    FocusedSource src;
    src.apertureRadius_m = 0.032;
    src.focalLength_m = 0.0626;
    src.frequency_Hz = 1.0e6;
    src.power_W = 150.0;

    // ── (1) 幾何量 ───────────────────────────────────────────────────────
    const double a = src.apertureRadius_m, R = src.focalLength_m;
    const double hExact = R - std::sqrt(R * R - a * a);
    check(std::fabs(capHeight(a, R) - hExact) < 1e-15,
          "focus: cap height h = R − √(R²−a²)");
    check(std::fabs(capArea(a, R) - 2.0 * kPi * R * hExact) < 1e-15,
          "focus: cap area S = 2πRh");
    check(capHeight(R, R * 0.5) == 0.0, "focus: a ≥ R is rejected");

    // ── (2) 電力 → 法線速度: W = ½ρc u0² S ──────────────────────────────
    const double u0 = surfaceVelocity(src, water);
    const double S = capArea(a, R);
    check(std::fabs(0.5 * water.rho * water.c * u0 * u0 * S - src.power_W)
              < 1e-9 * src.power_W,
          "focus: u0 reproduces the radiated power");

    // ── (3) 焦点音圧 = ρ c u0 k h (O'Neil) ──────────────────────────────
    const double k = 2.0 * kPi * src.frequency_Hz / water.c;
    const double pExact = water.rho * water.c * u0 * k * hExact;
    check(std::fabs(focalPressureLossless(src, water, u0) - pExact)
              < 1e-9 * pExact,
          "focus: focal pressure = rho c u0 k h");
    // 軸上閉形式が焦点で同じ値に一致する (数値的な連続性も確認)
    check(std::fabs(axialPressureAmplitude(src, water, u0, R) - pExact)
              < 1e-6 * pExact,
          "focus: axial closed form equals the focal value at z = R");
    check(std::fabs(axialPressureAmplitude(src, water, u0, R - 1e-7) - pExact)
              < 1e-4 * pExact,
          "focus: axial closed form is continuous at the focus");

    // ── (4) 弱集束の極限: h → a²/(2R), 利得 → k a²/(2R) ─────────────────
    {
        FocusedSource weak = src;
        weak.apertureRadius_m = 1.0e-4;      // a/R = 1.6e-3
        const double hp = weak.apertureRadius_m * weak.apertureRadius_m
                          / (2.0 * weak.focalLength_m);
        check(std::fabs(capHeight(weak.apertureRadius_m, weak.focalLength_m)
                        - hp) < 1e-5 * hp,
              "focus: h → a²/(2R) for a << R");
        const FocusedFieldResult wr = evaluateFocus(weak, water);
        const double kw = 2.0 * kPi * weak.frequency_Hz / water.c;
        check(std::fabs(wr.pressureGain - kw * hp) < 1e-5 * kw * hp,
              "focus: pressure gain → k a²/(2R) in the paraxial limit");
    }

    // ── (5) 軸上分布の性質 ──────────────────────────────────────────────
    {
        // 焦点が軸上最大 (音源〜2R の範囲を走査)
        double maxV = 0.0, maxZ = 0.0;
        for (int i = 1; i <= 2000; ++i) {
            const double z = 2.0 * R * i / 2000.0;
            const double v = axialPressureAmplitude(src, water, u0, z);
            if (v > maxV) { maxV = v; maxZ = z; }
        }
        // 有限開口では軸上最大は幾何焦点よりわずかに音源側へ寄る
        // (焦点シフト。O'Neil 1949)。値は幾何焦点の閉形式とほぼ一致する。
        check(maxZ <= R && std::fabs(maxZ - R) < 0.02 * R,
              "focus: the axial maximum sits just before the geometric focus");
        check(maxV >= pExact && (maxV - pExact) < 0.01 * pExact,
              "focus: the axial maximum is within 1 % of the focal value");
        // 音圧は u0 に比例する (線形性)
        check(std::fabs(axialPressureAmplitude(src, water, 2.0 * u0, 0.04)
                        - 2.0 * axialPressureAmplitude(src, water, u0, 0.04))
                  < 1e-9 * pExact,
              "focus: the axial field is linear in the surface velocity");
        check(axialPressureAmplitude(src, water, u0, 0.0) == 0.0,
              "focus: z = 0 is not evaluated");
    }

    // ── (6) 強度・減衰・ビーム幅 ────────────────────────────────────────
    {
        const FocusedFieldResult r = evaluateFocus(src, water);
        check(r.valid, "focus: the reference configuration is valid");
        check(std::fabs(r.attenuation_dB) < 1e-15,
              "focus: a lossless medium gives no attenuation");
        check(std::fabs(r.focalPressure_Pa - pExact) < 1e-9 * pExact,
              "focus: no derating without absorption");
        const double iExact = pExact * pExact / (2.0 * water.rho * water.c);
        check(std::fabs(r.focalIntensity_Wm2 - iExact) < 1e-9 * iExact,
              "focus: I = p^2/(2 rho c)");
        check(std::fabs(r.fNumber - R / (2.0 * a)) < 1e-15,
              "focus: F# = R/(2a)");
        // −6 dB (強度) 幅は 1.028·λ·F# (Airy パターン。文献値と 1 % 以内)
        const double lambda = water.c / src.frequency_Hz;
        check(std::fabs(r.beamWidth6dB_m - 1.028 * lambda * r.fNumber)
                  < 0.01 * 1.028 * lambda * r.fNumber,
              "focus: -6 dB width is 1.028 lambda F#");
        // λ を半分にすると幅も半分 (スケーリング)
        FocusedSource hi = src;
        hi.frequency_Hz = 2.0e6;
        const FocusedFieldResult rh = evaluateFocus(hi, water);
        check(std::fabs(rh.beamWidth6dB_m - 0.5 * r.beamWidth6dB_m)
                  < 1e-9 * r.beamWidth6dB_m,
              "focus: the -6 dB width scales with the wavelength");
    }

    // ── (7) べき乗則吸収: α(f) = α0 f^y ─────────────────────────────────
    {
        Medium m = water;
        m.alpha0_dBcmMHz = 0.5;
        m.alphaExponent = 1.0;
        // 0.5 dB/cm/MHz を 2 MHz で 1 cm → 1.0 dB/cm = 100 dB/m
        check(std::fabs(attenuation_dB_per_m(m, 2.0e6) - 100.0) < 1e-9,
              "focus: power-law absorption at y = 1");
        m.alphaExponent = 2.0;
        // 0.5 × 2² = 2 dB/cm = 200 dB/m
        check(std::fabs(attenuation_dB_per_m(m, 2.0e6) - 200.0) < 1e-9,
              "focus: power-law absorption at y = 2");
        // Np/m は dB/m ÷ 8.6859
        check(std::fabs(attenuation_Np_per_m(m, 2.0e6)
                        - 200.0 / 8.685889638065035) < 1e-9,
              "focus: dB/m to Np/m conversion");
        // 5 cm を 1 MHz で伝搬 (y=1, 0.5 dB/cm) → 2.5 dB 減衰
        Medium m2 = water;
        m2.alpha0_dBcmMHz = 0.5;
        m2.alphaExponent = 1.0;
        FocusedSource s2 = src;
        s2.focalLength_m = 0.05;
        const FocusedFieldResult r2 = evaluateFocus(s2, m2);
        check(std::fabs(r2.attenuation_dB - 2.5) < 1e-9,
              "focus: attenuation over the focal distance");
        check(std::fabs(r2.focalPressure_Pa
                        - r2.focalPressureLossless_Pa
                              * std::pow(10.0, -2.5 / 20.0))
                  < 1e-9 * r2.focalPressureLossless_Pa,
              "focus: the derated pressure applies the dB attenuation");
    }

    // ── (8) MI (IEC 62359) ──────────────────────────────────────────────
    {
        const FocusedFieldResult r = evaluateFocus(src, water);
        const double fMHz = 1.0;
        const double derate = std::pow(10.0, -0.3 * fMHz * (R * 100.0) / 20.0);
        const double miExact = pExact * derate * 1e-6 / std::sqrt(fMHz);
        check(std::fabs(r.mechanicalIndex - miExact) < 1e-9 * miExact,
              "focus: MI = derated p_r [MPa] / sqrt(f [MHz])");
        // 出力を 4 倍にすると音圧は 2 倍 → MI も 2 倍
        FocusedSource p4 = src;
        p4.power_W = 4.0 * src.power_W;
        const FocusedFieldResult r4 = evaluateFocus(p4, water);
        check(std::fabs(r4.mechanicalIndex - 2.0 * r.mechanicalIndex)
                  < 1e-9 * r.mechanicalIndex,
              "focus: MI doubles when the power is quadrupled");
    }

    // ── (9) 非線形指標 (Gol'dberg) ──────────────────────────────────────
    {
        Medium m = water;
        m.alpha0_dBcmMHz = 0.002;     // 水 (y = 2)
        const FocusedFieldResult r = evaluateFocus(src, m);
        check(r.nonlinearValid, "focus: B/A known → nonlinear metrics");
        check(std::fabs(r.betaNonlinear - (1.0 + 5.0 / 2.0)) < 1e-15,
              "focus: beta = 1 + B/2A");
        const double eps = r.focalPressure_Pa
                           / (m.rho * m.c * m.c);
        check(std::fabs(r.machNumber - eps) < 1e-12 * eps,
              "focus: Mach number = p/(rho c^2)");
        const double xsh = 1.0 / (r.betaNonlinear * eps * k);
        check(std::fabs(r.shockDistance_m - xsh) < 1e-9 * xsh,
              "focus: shock distance = 1/(beta eps k)");
        const double alphaNp = attenuation_Np_per_m(m, src.frequency_Hz);
        check(std::fabs(r.goldberg - 1.0 / (alphaNp * xsh))
                  < 1e-9 * r.goldberg,
              "focus: Goldberg number = 1/(alpha x_sh)");
        check(r.regime == RegimeShock,
              "focus: 150 W HIFU in water is shock-dominated");
        // 出力を下げれば準線形へ (単調性)
        FocusedSource lo = src;
        lo.power_W = 1e-9;
        const FocusedFieldResult rl = evaluateFocus(lo, m);
        check(rl.goldberg < r.goldberg,
              "focus: the Goldberg number falls with the drive power");
        check(rl.regime == RegimeQuasiLinear,
              "focus: a very weak drive is quasi-linear");
        // 吸収ゼロは Γ = ∞ (負値で表現)
        const FocusedFieldResult rw = evaluateFocus(src, water);
        check(rw.goldberg < 0.0 && rw.regime == RegimeShock,
              "focus: no absorption means an infinite Goldberg number");
        // B/A 不明の媒質では非線形指標を出さない
        Medium bone = bioMedium(4).medium;
        check(bone.bOverA < 0.0, "focus: cortical bone has no B/A entry");
        check(!evaluateFocus(src, bone).nonlinearValid,
              "focus: unknown B/A yields no nonlinear metrics");
    }

    // ── (10) 不正入力 ───────────────────────────────────────────────────
    {
        FocusedSource bad = src;
        bad.apertureRadius_m = src.focalLength_m;   // a = R
        check(!evaluateFocus(bad, water).valid, "focus: a = R is invalid");
        bad = src;
        bad.frequency_Hz = 0.0;
        check(!evaluateFocus(bad, water).valid, "focus: f = 0 is invalid");
        Medium bm;
        bm.rho = 0.0;
        check(!evaluateFocus(src, bm).valid, "focus: rho = 0 is invalid");
    }

    // ── (11) 文献値データベース ─────────────────────────────────────────
    {
        check(bioMediumCount() == 5 && ndtMediumCount() == 4,
              "focus: medium database sizes");
        for (int i = 0; i < bioMediumCount(); ++i) {
            const Medium &m = bioMedium(i).medium;
            check(m.rho > 0.0 && m.c > 0.0 && m.alpha0_dBcmMHz >= 0.0
                      && m.alphaExponent > 0.0,
                  "focus: bio medium entries are physical");
        }
        // 水の特性インピーダンス ≈ 1.48 MRayl (文献値)
        const Medium w = bioMedium(0).medium;
        check(std::fabs(acousticImpedance(w) * 1e-6 - 1.48) < 0.01,
              "focus: water impedance is about 1.48 MRayl");
        // 鋼の縦波音速 ≈ 5900 m/s、Z ≈ 46 MRayl
        const Medium st = ndtMedium(0).medium;
        check(std::fabs(st.c - 5900.0) < 1.0,
              "focus: steel longitudinal speed");
        check(std::fabs(acousticImpedance(st) * 1e-6 - 46.3) < 0.5,
              "focus: steel impedance is about 46 MRayl");
        // 水は f² 則、軟組織はほぼ f¹ (Szabo Table 4.1)
        check(std::fabs(w.alphaExponent - 2.0) < 1e-12,
              "focus: water absorption follows f^2");
        check(bioMedium(1).medium.alphaExponent > 1.0
                  && bioMedium(1).medium.alphaExponent < 1.5,
              "focus: soft tissue absorption is nearly linear in f");
    }
}

// ── 自由キャリア分散 (src/optics/PlasmaDispersion) ──────────────────────────
// 期待値はテスト側に独立に書く:
//   - ω_p = √(Ne²/(ε0 m*)) を定数から直接計算した値と一致
//   - γ = 0 の Drude は ω = ω_p で ε = 0 (プラズマ端)
//   - Δn = −ω_p²/(2nω²) (小摂動) — Drude 実装と一致
//   - Soref-Bennett の Si フィットは論文の係数どおり
//   - ΔN = ΔP = 0 で Δn = Δα = 0、キャリアに対し単調
static void testPlasmaDispersion()
{
    using namespace ofd::optics;
    g_file = "plasma-dispersion";
    const double kPi = 3.14159265358979323846;
    // CODATA 2018 (テスト側に独立に書く)
    const double e = 1.602176634e-19;
    const double m0 = 9.1093837015e-31;
    const double eps0 = 8.8541878128e-12;
    const double c0 = 2.99792458e8;

    // ── (1) プラズマ周波数 ──────────────────────────────────────────────
    {
        const double N = 1.0e24;      // m^-3 (= 1e18 cm^-3)
        const double meff = 0.26;
        const double wp = std::sqrt(N * e * e / (eps0 * meff * m0));
        check(std::fabs(plasmaAngularFrequency(N, meff) - wp) < 1e-9 * wp,
              "plasma: omega_p = sqrt(N e^2 /(eps0 m*))");
        // 密度 4 倍で ω_p は 2 倍
        check(std::fabs(plasmaAngularFrequency(4.0 * N, meff) - 2.0 * wp)
                  < 1e-9 * wp,
              "plasma: omega_p scales as sqrt(N)");
        check(plasmaAngularFrequency(0.0, meff) == 0.0,
              "plasma: no carriers means no plasma frequency");
    }

    // ── (2) Drude の誘電率: ω = ω_p で ε = 0 ────────────────────────────
    {
        const double wp = 1.0e14;
        const ComplexEps at = drudePermittivity(1.0, wp, wp, 0.0);
        check(std::fabs(at.re) < 1e-9 && std::fabs(at.im) < 1e-30,
              "plasma: eps = 0 at the plasma frequency (gamma = 0)");
        const ComplexEps below = drudePermittivity(1.0, 0.5 * wp, wp, 0.0);
        check(below.re < 0.0, "plasma: eps < 0 below the plasma frequency");
        const ComplexEps above = drudePermittivity(1.0, 2.0 * wp, wp, 0.0);
        check(above.re > 0.0 && above.re < 1.0,
              "plasma: 0 < eps < eps_inf above the plasma frequency");
        // 損失があると虚部は正 (exp(-iwt) 規約)
        const ComplexEps lossy = drudePermittivity(1.0, wp, wp, 1e12);
        check(lossy.im > 0.0, "plasma: damping gives a positive Im(eps)");
    }

    // ── (3) Drude の Δn = −ω_p²/(2nω²) ─────────────────────────────────
    {
        CarrierState cs;
        cs.deltaN_cm3 = 1.0e18;
        cs.deltaP_cm3 = 0.0;
        const double n = 3.48, lambda = 1550.0;
        const PlasmaResult r = drudeFreeCarrier(lambda, n, cs);
        check(r.valid, "plasma: Drude evaluation is valid");
        const double omega = 2.0 * kPi * c0 / (lambda * 1e-9);
        const double wp = std::sqrt(1.0e24 * e * e / (eps0 * 0.26 * m0));
        const double dnExact = -wp * wp / (2.0 * n * omega * omega);
        check(std::fabs(r.deltaN_index - dnExact) < 1e-9 * std::fabs(dnExact),
              "plasma: Drude index change matches -wp^2/(2 n w^2)");
        // Soref-Bennett の実測値 (-8.8e-4) と同じ桁・同符号
        check(r.deltaN_index < 0.0, "plasma: free carriers lower the index");
        check(std::fabs(r.deltaN_index) > 5e-4
                  && std::fabs(r.deltaN_index) < 2e-3,
              "plasma: Drude dn is the same order as the measured fit");
        // キャリア密度に比例
        CarrierState c2 = cs;
        c2.deltaN_cm3 = 2.0e18;
        check(std::fabs(drudeFreeCarrier(lambda, n, c2).deltaN_index
                        - 2.0 * r.deltaN_index) < 1e-9 * std::fabs(dnExact),
              "plasma: Drude dn is linear in the carrier density");
        // キャリアゼロで変化なし
        CarrierState zero;
        const PlasmaResult z = drudeFreeCarrier(lambda, n, zero);
        check(z.valid && z.deltaN_index == 0.0 && z.deltaAlpha_per_cm == 0.0,
              "plasma: no carriers means no index or loss change");
        // 不正入力
        check(!drudeFreeCarrier(0.0, n, cs).valid, "plasma: lambda > 0 required");
        check(!drudeFreeCarrier(lambda, 0.0, cs).valid, "plasma: n > 0 required");
    }

    // ── (4) Soref-Bennett の Si フィット ────────────────────────────────
    {
        // λ = 1.55 μm, ΔN = 1e18 cm^-3, ΔP = 0 → Δn = −8.8e-4, Δα = 8.5 cm^-1
        const PlasmaResult r = sorefBennettSilicon(1550.0, 1.0e18, 0.0);
        check(r.valid, "plasma: Soref-Bennett evaluation is valid");
        check(std::fabs(r.deltaN_index + 8.8e-4) < 1e-12,
              "plasma: SB electron index change at 1.55 um");
        check(std::fabs(r.deltaAlpha_per_cm - 8.5) < 1e-9,
              "plasma: SB electron absorption at 1.55 um");
        // 正孔は指数 0.8: ΔP = 1e18 → 8.5e-18·(1e18)^0.8
        const PlasmaResult h = sorefBennettSilicon(1550.0, 0.0, 1.0e18);
        check(std::fabs(h.deltaN_index
                        + 8.5e-18 * std::pow(1.0e18, 0.8)) < 1e-12,
              "plasma: SB hole index change uses the 0.8 exponent");
        check(std::fabs(h.deltaAlpha_per_cm - 6.0) < 1e-9,
              "plasma: SB hole absorption at 1.55 um");
        // λ = 1.31 μm の係数
        const PlasmaResult s13 = sorefBennettSilicon(1310.0, 1.0e18, 0.0);
        check(std::fabs(s13.deltaN_index + 6.2e-4) < 1e-12,
              "plasma: SB electron index change at 1.31 um");
        check(std::fabs(s13.deltaAlpha_per_cm - 6.0) < 1e-9,
              "plasma: SB electron absorption at 1.31 um");
        // 帯の選択と適用範囲
        check(nearestSorefBennettBand(1550.0) == SorefBennettBand::Lambda1550nm,
              "plasma: 1550 nm selects the 1.55 um fit");
        check(nearestSorefBennettBand(1310.0) == SorefBennettBand::Lambda1310nm,
              "plasma: 1310 nm selects the 1.31 um fit");
        check(sorefBennettApplicable(1550.0) && sorefBennettApplicable(1310.0),
              "plasma: the fitted bands are in range");
        check(!sorefBennettApplicable(1000.0),
              "plasma: 1000 nm is outside the fitted bands");
        // ゼロ入力・単調性
        const PlasmaResult z = sorefBennettSilicon(1550.0, 0.0, 0.0);
        check(z.deltaN_index == 0.0 && z.deltaAlpha_per_cm == 0.0,
              "plasma: SB with no carriers changes nothing");
        check(sorefBennettSilicon(1550.0, 2.0e18, 0.0).deltaN_index
                  < r.deltaN_index,
              "plasma: SB index change decreases monotonically with dN");
        check(sorefBennettSilicon(1550.0, 2.0e18, 0.0).deltaAlpha_per_cm
                  > r.deltaAlpha_per_cm,
              "plasma: SB absorption grows monotonically with dN");
        // dB/cm 換算は 10·log10(e)·α
        check(std::fabs(r.deltaAlpha_dB_per_cm
                        - r.deltaAlpha_per_cm * 4.3429448190325175) < 1e-9,
              "plasma: cm^-1 to dB/cm conversion");
        check(!sorefBennettSilicon(1550.0, -1.0, 0.0).valid,
              "plasma: negative carrier density is rejected");
    }
}

// ── SAR の定義式と指針値 (src/em/SarMetrics) ────────────────────────────────
// 期待値はテスト側に独立に書く (手計算値・規格の条文値):
//   - SAR = σ|E|²/(2ρ): σ=0.9, |E|=100 V/m, ρ=1000 → 4.5 W/kg
//   - E_rms 61.4 V/m ↔ S = 10 W/m² (ICNIRP の参考レベルの対応)
//   - ICNIRP 2020 / IEEE C95.1-2019 / FCC の基本制限
//   - 算出値が無いときに「適合」を返さない
static void testSarMetrics()
{
    using namespace ofd::em;
    g_file = "sar-metrics";

    // ── (1) 定義式 ──────────────────────────────────────────────────────
    check(std::fabs(sarFromPeakField(0.9, 100.0, 1000.0) - 4.5) < 1e-12,
          "sar: SAR = sigma |E|^2 /(2 rho)");
    check(std::fabs(sarFromRmsField(0.9, 100.0, 1000.0) - 9.0) < 1e-12,
          "sar: SAR = sigma |E_rms|^2 / rho");
    // ピーク = √2 × 実効 の整合
    check(std::fabs(sarFromPeakField(0.9, 100.0 * std::sqrt(2.0), 1000.0)
                    - sarFromRmsField(0.9, 100.0, 1000.0)) < 1e-12,
          "sar: peak and rms forms agree");
    check(sarFromPeakField(0.9, 100.0, 0.0) == 0.0,
          "sar: zero density is rejected");
    // 電界 2 倍で SAR 4 倍
    check(std::fabs(sarFromRmsField(0.9, 200.0, 1000.0)
                    - 4.0 * sarFromRmsField(0.9, 100.0, 1000.0)) < 1e-12,
          "sar: SAR is quadratic in the field");

    // ── (2) 平面波の電力密度 ────────────────────────────────────────────
    {
        // 61.4 V/m (rms) ↔ 10 W/m^2 (ICNIRP の対応表)
        const double s = planeWavePowerDensityFromRms(61.4);
        check(std::fabs(s - 10.0) < 0.02,
              "sar: 61.4 V/m rms is about 10 W/m^2");
        // 逆算の往復
        check(std::fabs(rmsFieldFromPowerDensity(s) - 61.4) < 1e-9,
              "sar: power density inverts back to the field");
        check(std::fabs(planeWavePowerDensityFromRms(1.0)
                        - 1.0 / 376.730313668) < 1e-15,
              "sar: S = E_rms^2 / Z0");
        check(rmsFieldFromPowerDensity(-1.0) == 0.0,
              "sar: negative power density yields no field");
    }

    // ── (3) 断熱温度上昇 ────────────────────────────────────────────────
    {
        // 2 W/kg を 6 分、c_p = 3600 J/(kg K) → 2*360/3600 = 0.2 K
        check(std::fabs(adiabaticTemperatureRise(2.0, 360.0, 3600.0) - 0.2)
                  < 1e-12,
              "sar: adiabatic dT = SAR t / c_p");
        check(adiabaticTemperatureRise(0.0, 360.0, 3600.0) == 0.0,
              "sar: no SAR means no temperature rise");
        check(adiabaticTemperatureRise(2.0, 360.0, 0.0) == 0.0,
              "sar: zero specific heat is rejected");
    }

    // ── (4) 指針値 (規格の条文値) ───────────────────────────────────────
    {
        const double f = 1950.0e6;    // 1950 MHz
        const ExposureLimit l10 = exposureLimit(Standard::Icnirp2020,
                                                Category::GeneralPublic,
                                                Metric::LocalSar10g, f);
        check(l10.defined && l10.applicable, "sar: ICNIRP 10 g applies at 2 GHz");
        check(std::fabs(l10.value - 2.0) < 1e-12,
              "sar: ICNIRP general-public local SAR is 2 W/kg");
        check(std::fabs(l10.averagingMass_g - 10.0) < 1e-12
                  && std::fabs(l10.averagingTime_s - 360.0) < 1e-12,
              "sar: ICNIRP local SAR averages 10 g over 6 min");
        const ExposureLimit l10o = exposureLimit(Standard::Icnirp2020,
                                                 Category::Occupational,
                                                 Metric::LocalSar10g, f);
        check(std::fabs(l10o.value - 10.0) < 1e-12,
              "sar: ICNIRP occupational local SAR is 10 W/kg");
        check(std::fabs(l10o.value / l10.value - 5.0) < 1e-12,
              "sar: the occupational/public ratio is 5");

        const ExposureLimit wb = exposureLimit(Standard::Icnirp2020,
                                               Category::GeneralPublic,
                                               Metric::WholeBodySar, f);
        check(std::fabs(wb.value - 0.08) < 1e-12,
              "sar: ICNIRP whole-body SAR is 0.08 W/kg");
        check(std::fabs(wb.averagingTime_s - 1800.0) < 1e-12,
              "sar: whole-body SAR averages over 30 min");

        // FCC の 1 g
        const ExposureLimit fcc1 = exposureLimit(Standard::Fcc47Cfr,
                                                 Category::GeneralPublic,
                                                 Metric::LocalSar1g, f);
        check(fcc1.defined && std::fabs(fcc1.value - 1.6) < 1e-12,
              "sar: FCC uncontrolled 1 g SAR is 1.6 W/kg");
        check(std::fabs(exposureLimit(Standard::Fcc47Cfr,
                                      Category::Occupational,
                                      Metric::LocalSar1g, f).value - 8.0)
                  < 1e-12,
              "sar: FCC controlled 1 g SAR is 8 W/kg");
        // ICNIRP は 1 g 平均を採用していない
        check(!exposureLimit(Standard::Icnirp2020, Category::GeneralPublic,
                             Metric::LocalSar1g, f).defined,
              "sar: ICNIRP defines no 1 g limit");

        // IEEE C95.1-2019
        check(std::fabs(exposureLimit(Standard::IeeeC95_1_2019,
                                      Category::GeneralPublic,
                                      Metric::LocalSar10g, f).value - 2.0)
                  < 1e-12,
              "sar: IEEE unrestricted local SAR is 2 W/kg");
    }

    // ── (5) 周波数による適用範囲 ────────────────────────────────────────
    {
        const double f2g = 1950.0e6, f28g = 28.0e9;
        check(exposureLimit(Standard::Icnirp2020, Category::GeneralPublic,
                            Metric::LocalSar10g, f2g).applicable,
              "sar: SAR limits apply below 6 GHz");
        check(!exposureLimit(Standard::Icnirp2020, Category::GeneralPublic,
                             Metric::LocalSar10g, f28g).applicable,
              "sar: SAR limits do not apply at 28 GHz");
        const ExposureLimit pd = exposureLimit(Standard::Icnirp2020,
                                               Category::GeneralPublic,
                                               Metric::AbsorbedPowerDensity,
                                               f28g);
        check(pd.defined && pd.applicable && std::fabs(pd.value - 20.0) < 1e-12,
              "sar: ICNIRP absorbed power density is 20 W/m^2 above 6 GHz");
        check(!exposureLimit(Standard::Icnirp2020, Category::GeneralPublic,
                             Metric::AbsorbedPowerDensity, f2g).applicable,
              "sar: the absorbed power density does not apply at 2 GHz");
        const ExposureLimit inc = exposureLimit(Standard::Icnirp2020,
                                                Category::GeneralPublic,
                                                Metric::IncidentPowerDensity,
                                                f28g);
        check(inc.applicable && std::fabs(inc.value - 10.0) < 1e-12,
              "sar: ICNIRP whole-body reference level is 10 W/m^2");
    }

    // ── (6) 温度上昇は「根拠値」であって限度値ではない ──────────────────
    {
        const ExposureLimit t = exposureLimit(Standard::IeeeC95_1_2019,
                                              Category::GeneralPublic,
                                              Metric::LocalTemperatureRise,
                                              1950.0e6);
        check(t.defined && t.isBasis, "sar: the temperature rise is a basis");
        check(std::fabs(t.value - 1.0) < 1e-12,
              "sar: the local temperature-rise basis is 1 K");
    }

    // ── (7) 判定は算出値が無いときに「適合」を作らない ──────────────────
    {
        const ExposureLimit l = exposureLimit(Standard::Icnirp2020,
                                              Category::GeneralPublic,
                                              Metric::LocalSar10g, 1950.0e6);
        check(evaluate(l, 0.0, false) == Verdict::NotEvaluated,
              "sar: no computed value means no verdict");
        check(evaluate(l, 1.4, true) == Verdict::Compliant,
              "sar: 1.4 W/kg is below the 2 W/kg limit");
        check(evaluate(l, 2.5, true) == Verdict::NonCompliant,
              "sar: 2.5 W/kg exceeds the 2 W/kg limit");
        const ExposureLimit off = exposureLimit(Standard::Icnirp2020,
                                                Category::GeneralPublic,
                                                Metric::LocalSar10g, 28.0e9);
        check(evaluate(off, 1.0, true) == Verdict::NotApplicable,
              "sar: an out-of-range metric is not applicable");
    }
}

// ── FDTD 設定の検証計算 (src/core/FdtdVerification) ─────────────────────────
// VerificationTab が表示する「実計算」の検算。期待値はすべてこのテスト側に
// 独立に書く (解析解・極限値・公表式の手計算):
//   - メッシュ解像度: 分割数を r 倍すると λ/Δx は r 倍、セル数は r³ 倍
//   - PML 設計反射率: R(θ)=R0^cosθ → θ=0 で R0、cosθ=1/2 で √R0 (dB は半分)
//   - 1 次 Mur: θ=0 で無反射、θ=90° で全反射、θ=60° で 1/3
//   - Courant 数: 等方格子 Δ で Δt = Δ/(c√3) のとき S = 1 (安定限界)
//   - 各判定関数の閾値 (既知の設定 → 期待どおりの合否)
static void testFdtdVerification()
{
    using namespace ofd::verify;
    g_file = "fdtd-verification";

    // ── (1) メッシュ解像度の計画値 ──────────────────────────────────────
    {
        Grid g;
        g.axis[0] = { 0.01,  0.01,  10 };
        g.axis[1] = { 0.005, 0.005, 20 };
        g.axis[2] = { 0.02,  0.02,   4 };
        const double lambda = 1.0;               // λ = 1 m
        const std::vector<MeshLevel> lv =
            meshConvergenceLevels(g, lambda, { 0.5, 1.0, 2.0 });
        check(lv.size() == 3, "meshlv: three levels");
        if (lv.size() == 3) {
            // ×1: 10*20*4 = 800 セル、Δx_max = 0.02 → λ/Δx = 50
            check(lv[1].cells == 800, "meshlv: x1 cell count");
            check(std::fabs(lv[1].dxMax_m - 0.02) < 1e-15, "meshlv: x1 dx_max");
            check(std::fabs(lv[1].lambdaOverDx - 50.0) < 1e-12,
                  "meshlv: x1 lambda/dx = 50");
            // ×2: 20*40*8 = 6400 セル (= 8 倍)、λ/Δx = 100 (= 2 倍)
            check(lv[2].cells == 6400, "meshlv: x2 cell count is 8x");
            check(std::fabs(lv[2].lambdaOverDx - 100.0) < 1e-12,
                  "meshlv: x2 lambda/dx doubles");
            // ×0.5: 5*10*2 = 100 セル、λ/Δx = 25
            check(lv[0].cells == 100, "meshlv: x0.5 cell count");
            check(std::fabs(lv[0].lambdaOverDx - 25.0) < 1e-12,
                  "meshlv: x0.5 lambda/dx halves");
            // メモリはセル数 × 60 byte
            check(std::fabs(lv[1].memoryMB - 800.0 * 60.0 / (1024.0 * 1024.0))
                      < 1e-12, "meshlv: memory = cells * 60 byte");
            // 単調性: 細かくするほどセル数も λ/Δx も増える
            check(lv[0].cells < lv[1].cells && lv[1].cells < lv[2].cells,
                  "meshlv: cell count is monotonic");
        }
        // 波長が不明なら λ/Δx は計算しない (0 = 未計算)
        const std::vector<MeshLevel> nolam = meshConvergenceLevels(g, 0.0, { 1.0 });
        check(nolam.size() == 1 && nolam[0].lambdaOverDx == 0.0,
              "meshlv: no wavelength -> lambda/dx not computed");
        // 格子が空なら行を作らない
        Grid empty;
        check(meshConvergenceLevels(empty, 1.0, { 1.0 }).empty(),
              "meshlv: empty grid yields no levels");
    }

    // ── (2) PML の設計反射率 R(θ) = R0^cosθ ─────────────────────────────
    {
        const double r0 = 1e-5;
        check(std::fabs(pmlDesignReflection(r0, 0.0) - r0) < 1e-18,
              "pml: normal incidence returns R0");
        check(std::fabs(toDb(pmlDesignReflection(r0, 0.0)) + 100.0) < 1e-9,
              "pml: R0 = 1e-5 is -100 dB");
        // cos 60° = 1/2 → R = √R0、dB はちょうど半分
        check(std::fabs(pmlDesignReflection(r0, 60.0) - std::sqrt(r0)) < 1e-15,
              "pml: 60 deg gives sqrt(R0)");
        check(std::fabs(toDb(pmlDesignReflection(r0, 60.0)) + 50.0) < 1e-9,
              "pml: 60 deg is half the dB");
        // 接線入射は吸収されない (R = 1)
        check(std::fabs(pmlDesignReflection(r0, 90.0) - 1.0) < 1e-15,
              "pml: grazing incidence reflects fully");
        // 角度に対して単調増加
        double prev = pmlDesignReflection(r0, 0.0);
        bool mono = true;
        for (double th = 5.0; th <= 85.0; th += 5.0) {
            const double v = pmlDesignReflection(r0, th);
            if (!(v > prev)) mono = false;
            prev = v;
        }
        check(mono, "pml: reflection grows monotonically with angle");
    }

    // ── (3) 1 次 Mur の残留反射 R(θ) = (1−cosθ)/(1+cosθ) ────────────────
    {
        check(murDesignReflection(0.0) == 0.0, "mur: normal incidence is exact");
        check(toDb(murDesignReflection(0.0)) <= -300.0,
              "mur: zero reflection clamps to the dB floor");
        check(std::fabs(murDesignReflection(60.0) - 1.0 / 3.0) < 1e-12,
              "mur: 60 deg gives 1/3");
        check(std::fabs(toDb(murDesignReflection(60.0)) + 9.542425094) < 1e-6,
              "mur: 60 deg is -9.54 dB");
        check(std::fabs(murDesignReflection(90.0) - 1.0) < 1e-15,
              "mur: grazing incidence reflects fully");
        // 同じ角度で PML (R0=1e-5) より Mur の方がはるかに反射が大きい
        check(murDesignReflection(45.0) > pmlDesignReflection(1e-5, 45.0),
              "mur: worse than PML at 45 deg");
    }

    // ── (4) dB 変換 ─────────────────────────────────────────────────────
    {
        check(std::fabs(toDb(1.0)) < 1e-15, "todb: 1 -> 0 dB");
        check(std::fabs(toDb(0.1) + 20.0) < 1e-12, "todb: 0.1 -> -20 dB");
        check(toDb(0.0) == -300.0, "todb: 0 -> floor");
        check(toDb(0.0, -120.0) == -120.0, "todb: explicit floor honoured");
    }

    // ── (5) 実行ログの収束履歴の抽出 ────────────────────────────────────
    {
        const std::string log =
            "OpenFDTD Version 4.2.0\n"
            "     Nx=10 Ny=10 Nz=10\n"
            "      1 1.000000e+00 9.000000e-01\n"
            "    100 1.000000e-02 9.000000e-03\n"
            "   1000 5.000000e-04 4.000000e-04\n"
            "     50 1.000000e-01 1.000000e-01\n"   // step が戻る = 別表
            "      1 2 3 4\n"                        // 4 トークン = 対象外
            "   2000 abc 1.0\n"                      // 数値でない
            "cpu time = 12.3\n"
            "normal end\n";
        const std::vector<ConvergencePoint> h = parseConvergenceLog(log);
        check(h.size() == 3, "convlog: three convergence rows");
        if (h.size() == 3) {
            check(h[0].step == 1 && std::fabs(h[0].e - 1.0) < 1e-12,
                  "convlog: first row");
            check(h[2].step == 1000 && std::fabs(h[2].e - 5.0e-4) < 1e-16 &&
                      std::fabs(h[2].h - 4.0e-4) < 1e-16,
                  "convlog: last row");
        }
        check(parseConvergenceLog("").empty(), "convlog: empty text");
        // 上限を超える分は捨てる
        check(parseConvergenceLog(log, 2).size() == 2, "convlog: maxPoints");

        // 収束判定 (未実行 = Unknown。OK を捏造しない)
        check(convergenceVerdict({}, 1e-3) == Verdict::Unknown,
              "convverdict: no history -> unknown");
        check(convergenceVerdict(h, 1e-3) == Verdict::Ok,
              "convverdict: last point below threshold -> ok");
        check(convergenceVerdict(h, 1e-6) == Verdict::Warn,
              "convverdict: last point above threshold -> warn");
    }

    // ── (6) Courant 数と安定条件 ────────────────────────────────────────
    {
        const double c = 2.99792458e8;
        const double d = 1e-3;
        const double dxs[3] = { d, d, d };
        // 等方格子の安定限界 Δt = Δ/(c√3) でちょうど S = 1
        const double dtLimit = d / (c * std::sqrt(3.0));
        check(std::fabs(courantNumber(dtLimit, c, dxs) - 1.0) < 1e-12,
              "courant: isotropic grid limit gives S = 1");
        check(std::fabs(courantNumber(0.5 * dtLimit, c, dxs) - 0.5) < 1e-12,
              "courant: S is proportional to dt");
        check(courantNumber(0.0, c, dxs) == 0.0,
              "courant: dt = 0 (auto) is not evaluated");
        // 音響 (343 m/s) でも同じ形
        const double da[3] = { 0.01, 0.01, 0.01 };
        check(std::fabs(courantNumber(0.01 / (343.0 * std::sqrt(3.0)), 343.0, da)
                        - 1.0) < 1e-12, "courant: works for sound speed too");

        check(courantVerdict(0.5) == Verdict::Ok, "courant: S = 0.5 is ok");
        check(courantVerdict(0.99) == Verdict::Ok, "courant: S = 0.99 is ok");
        check(courantVerdict(0.995) == Verdict::Warn, "courant: S = 0.995 warns");
        check(courantVerdict(1.0) == Verdict::Warn, "courant: S = 1 warns");
        check(courantVerdict(1.05) == Verdict::Ng, "courant: S > 1 is unstable");
        check(courantVerdict(0.0) == Verdict::Unknown, "courant: S = 0 unknown");
    }

    // ── (7) 分解能の判定 (既知の設定 → 期待どおりの合否) ────────────────
    {
        // 2.5 GHz (λ = 119.9 mm) を Δx = 5 mm で切ると λ/Δx ≈ 24 → OK
        const double lambda = 2.99792458e8 / 2.5e9;
        Grid g;
        g.axis[0] = { 0.005, 0.005, 40 };
        g.axis[1] = { 0.005, 0.005, 40 };
        g.axis[2] = { 0.005, 0.005, 40 };
        std::vector<MeshLevel> lv = meshConvergenceLevels(g, lambda, { 1.0 });
        check(lv.size() == 1 && resolutionVerdict(lv[0].lambdaOverDx) == Verdict::Ok,
              "resolution: 5 mm at 2.5 GHz is ok");
        // Δx = 15 mm → λ/Δx ≈ 8.0 → 注意
        g.axis[0] = g.axis[1] = g.axis[2] = AxisGrid{ 0.015, 0.015, 10 };
        lv = meshConvergenceLevels(g, lambda, { 1.0 });
        check(lv.size() == 1 && resolutionVerdict(lv[0].lambdaOverDx) == Verdict::Warn,
              "resolution: 15 mm at 2.5 GHz warns");
        // Δx = 20 mm → λ/Δx ≈ 6.0 (しきい値 6 のわずかに下) → NG
        g.axis[0] = g.axis[1] = g.axis[2] = AxisGrid{ 0.02, 0.02, 10 };
        lv = meshConvergenceLevels(g, lambda, { 1.0 });
        check(lv.size() == 1 && resolutionVerdict(lv[0].lambdaOverDx) == Verdict::Ng,
              "resolution: 20 mm at 2.5 GHz is just below the 6 cell/lambda line");
        // Δx = 30 mm → λ/Δx ≈ 4.0 → NG
        g.axis[0] = g.axis[1] = g.axis[2] = AxisGrid{ 0.03, 0.03, 10 };
        lv = meshConvergenceLevels(g, lambda, { 1.0 });
        check(lv.size() == 1 && resolutionVerdict(lv[0].lambdaOverDx) == Verdict::Ng,
              "resolution: 30 mm at 2.5 GHz is too coarse");

        check(resolutionVerdict(20.0) == Verdict::Ok, "resolution: 20 ok");
        check(resolutionVerdict(10.0) == Verdict::Ok, "resolution: 10 ok");
        check(resolutionVerdict(8.0) == Verdict::Warn, "resolution: 8 warns");
        check(resolutionVerdict(5.0) == Verdict::Ng, "resolution: 5 ng");
        check(resolutionVerdict(0.0) == Verdict::Unknown, "resolution: 0 unknown");
    }

    // ── (8) 吸収境界・配置の判定 ────────────────────────────────────────
    {
        check(absorbingBoundaryVerdict(true, 10) == Verdict::Ok, "abc: PML 10 ok");
        check(absorbingBoundaryVerdict(true, 8) == Verdict::Ok, "abc: PML 8 ok");
        check(absorbingBoundaryVerdict(true, 5) == Verdict::Warn, "abc: PML 5 warns");
        check(absorbingBoundaryVerdict(true, 4) == Verdict::Ng, "abc: PML 4 ng");
        check(absorbingBoundaryVerdict(false, 32) == Verdict::Warn,
              "abc: Mur warns regardless of layer count");

        check(separationVerdict(4.0) == Verdict::Ok, "sep: 4 lambda ok");
        check(separationVerdict(1.0) == Verdict::Ok, "sep: 1 lambda ok");
        check(separationVerdict(0.5) == Verdict::Warn, "sep: 0.5 lambda warns");
        // 反応性近傍界の境界 λ/2π のすぐ下は NG
        check(separationVerdict(0.9 / (2.0 * 3.14159265358979323846))
                  == Verdict::Ng, "sep: inside the reactive near field is ng");
        check(separationVerdict(0.0) == Verdict::Unknown, "sep: 0 unknown");

        check(marginVerdict(0.5) == Verdict::Ok, "margin: lambda/2 ok");
        check(marginVerdict(0.25) == Verdict::Ok, "margin: lambda/4 ok");
        check(marginVerdict(0.2) == Verdict::Warn, "margin: below lambda/4 warns");
        check(marginVerdict(0.05) == Verdict::Ng, "margin: too close is ng");
        check(marginVerdict(-0.1) == Verdict::Ng,
              "margin: geometry outside the grid is ng");
    }
}

// ── 製造ばらつきの入力分布 (src/core/ToleranceStats) ────────────────────────
// 期待値はこのテスト側に独立に書く (公表されている分布の性質):
//   - 正規: ピーク 1/(σ√2π)、±kσ の被覆 = erf(k/√2)、モーメント
//   - 一様: 密度 1/(2a)、標準偏差 a/√3 (GUM §4.3.7)
//   - レイリー: 最頻値 σ、平均 σ√(π/2)、標準偏差 σ√(2−π/2)
//   - 被覆区間は数値積分した確率が normalCoverage(k) に一致する
static void testToleranceStats()
{
    using namespace ofd::tolstat;
    g_file = "tolerance-stats";
    const double kPi = 3.14159265358979323846;

    // 台形則で ∫ w(x)·f(x) dx を数値積分する (期待値をテスト側で作るため)
    const auto integrate = [](const Variable &v, double lo, double hi, int n,
                              double (*w)(double)) {
        double s = 0.0;
        const double dx = (hi - lo) / n;
        for (int i = 0; i <= n; ++i) {
            const double x = lo + dx * i;
            const double f = pdf(v, x) * (w ? w(x) : 1.0);
            s += (i == 0 || i == n) ? 0.5 * f : f;
        }
        return s * dx;
    };
    const auto one = [](double) { return 1.0; };

    // ── (1) 被覆確率 erf(k/√2) の既知値 ─────────────────────────────────
    check(std::fabs(normalCoverage(1.0) - 0.6826894921) < 1e-9,
          "cover: k=1 is 68.27%");
    check(std::fabs(normalCoverage(2.0) - 0.9544997361) < 1e-9,
          "cover: k=2 is 95.45%");
    check(std::fabs(normalCoverage(3.0) - 0.9973002039) < 1e-9,
          "cover: k=3 is 99.73%");
    check(normalCoverage(0.0) == 0.0, "cover: k=0 is 0");

    // ── (2) 正規分布 ────────────────────────────────────────────────────
    {
        Variable v{ Dist::Normal, 1.0, 2.0 };
        check(isContinuous(v), "normal: continuous");
        check(std::fabs(pdf(v, 1.0) - 1.0 / (2.0 * std::sqrt(2.0 * kPi))) < 1e-15,
              "normal: peak is 1/(sigma*sqrt(2pi))");
        check(std::fabs(pdf(v, 1.0 + 1.7) - pdf(v, 1.0 - 1.7)) < 1e-18,
              "normal: symmetric about the mean");
        check(std::fabs(stdDev(v) - 2.0) < 1e-15, "normal: sigma");
        check(std::fabs(mean(v) - 1.0) < 1e-15, "normal: mean");
        // 全確率 1 (±10σ で十分)
        check(std::fabs(integrate(v, 1.0 - 20.0, 1.0 + 20.0, 20000, one) - 1.0)
                  < 1e-9, "normal: pdf integrates to 1");
        // ±kσ の被覆が erf(k/√2) に一致する
        for (double k : { 1.0, 2.0, 3.0 }) {
            const Interval iv = coverageInterval(v, k);
            check(std::fabs(iv.lo - (1.0 - k * 2.0)) < 1e-15 &&
                      std::fabs(iv.hi - (1.0 + k * 2.0)) < 1e-15,
                  "normal: interval is mean +- k*sigma");
            check(std::fabs(integrate(v, iv.lo, iv.hi, 20000, one)
                            - normalCoverage(k)) < 1e-7,
                  "normal: interval holds the expected probability");
        }
        // 密度曲線は μ±4σ を等間隔に刻む
        const std::vector<Point> c = pdfCurve(v, 121);
        check(c.size() == 121, "normal: curve size");
        if (c.size() == 121) {
            check(std::fabs(c.front().x - (1.0 - 8.0)) < 1e-12 &&
                      std::fabs(c.back().x - (1.0 + 8.0)) < 1e-12,
                  "normal: curve spans mean +- 4 sigma");
            check(std::fabs(c[60].y - pdf(v, 1.0)) < 1e-15,
                  "normal: curve centre equals the peak");
        }
    }

    // ── (3) 一様分布 (GUM §4.3.7: σ = a/√3) ─────────────────────────────
    {
        Variable v{ Dist::Uniform, 50.0, 20.0 };
        check(std::fabs(pdf(v, 50.0) - 1.0 / 40.0) < 1e-15,
              "uniform: density is 1/(2a)");
        check(pdf(v, 50.0 - 20.0 - 1e-9) == 0.0, "uniform: zero below support");
        check(pdf(v, 50.0 + 20.0 + 1e-9) == 0.0, "uniform: zero above support");
        check(std::fabs(stdDev(v) - 20.0 / std::sqrt(3.0)) < 1e-15,
              "uniform: sigma = a/sqrt(3)");
        check(std::fabs(mean(v) - 50.0) < 1e-15, "uniform: mean = centre");
        // 中央被覆区間の確率が erf(k/√2) に一致する (CDF が線形なので厳密)
        const Interval iv = coverageInterval(v, 3.0);
        check(std::fabs((iv.hi - iv.lo) / 40.0 - normalCoverage(3.0)) < 1e-12,
              "uniform: interval width is P * support");
        check(iv.lo > 50.0 - 20.0 && iv.hi < 50.0 + 20.0,
              "uniform: interval stays inside the support");
        // 分散 = ∫(x-μ)² f dx = a²/3
        double s2 = 0.0;
        const int n = 200000;
        const double lo = 30.0, hi = 70.0, dx = (hi - lo) / n;
        for (int i = 0; i <= n; ++i) {
            const double x = lo + dx * i;
            const double t = (x - 50.0) * (x - 50.0) * pdf(v, x);
            s2 += (i == 0 || i == n) ? 0.5 * t : t;
        }
        s2 *= dx;
        check(std::fabs(s2 - 400.0 / 3.0) < 1e-3, "uniform: variance = a^2/3");
    }

    // ── (4) レイリー分布 ────────────────────────────────────────────────
    {
        const double sg = 2.5;
        Variable v{ Dist::Rayleigh, 0.0, sg };
        check(pdf(v, -1.0) == 0.0, "rayleigh: zero below the location");
        check(pdf(v, 0.0) == 0.0, "rayleigh: zero at the location");
        // 最頻値は x = σ
        check(pdf(v, sg) > pdf(v, sg - 0.05) && pdf(v, sg) > pdf(v, sg + 0.05),
              "rayleigh: mode at sigma");
        // 台形則の離散化誤差 (~1e-8) を見込んだ許容値
        check(std::fabs(integrate(v, 0.0, 40.0 * sg, 200000, one) - 1.0) < 1e-7,
              "rayleigh: pdf integrates to 1");
        // 平均 σ√(π/2)、標準偏差 σ√(2−π/2)
        check(std::fabs(mean(v) - sg * std::sqrt(kPi / 2.0)) < 1e-15,
              "rayleigh: mean = sigma*sqrt(pi/2)");
        check(std::fabs(stdDev(v) - sg * std::sqrt(2.0 - kPi / 2.0)) < 1e-15,
              "rayleigh: sigma = scale*sqrt(2-pi/2)");
        double m1 = 0.0;
        const int n = 400000;
        const double hi = 40.0 * sg, dx = hi / n;
        for (int i = 0; i <= n; ++i) {
            const double x = dx * i;
            const double t = x * pdf(v, x);
            m1 += (i == 0 || i == n) ? 0.5 * t : t;
        }
        m1 *= dx;
        check(std::fabs(m1 - sg * std::sqrt(kPi / 2.0)) < 1e-6,
              "rayleigh: numeric mean matches the closed form");
        // 被覆区間の確率が normalCoverage(3) に一致 (裾は非対称でも確率は同じ)
        const Interval iv = coverageInterval(v, 3.0);
        check(iv.lo > 0.0 && iv.hi > iv.lo, "rayleigh: interval is positive");
        check(std::fabs(integrate(v, iv.lo, iv.hi, 200000, one)
                        - normalCoverage(3.0)) < 1e-6,
              "rayleigh: interval holds the expected probability");
        check(std::fabs(integrate(v, 0.0, iv.lo, 200000, one)
                        - 0.5 * (1.0 - normalCoverage(3.0))) < 1e-7,
              "rayleigh: lower tail holds half the residual");
        // 位置パラメータは分布ごと平行移動する
        Variable shifted{ Dist::Rayleigh, 3.0, sg };
        check(std::fabs(pdf(shifted, 3.0 + sg) - pdf(v, sg)) < 1e-15,
              "rayleigh: location shifts the density");
    }

    // ── (5) 離散 / 不正な入力は「数値を出さない」 ───────────────────────
    {
        Variable d{ Dist::Discrete, 0.0, 1.0 };
        check(!isContinuous(d), "discrete: not continuous");
        check(pdf(d, 0.0) == 0.0, "discrete: no density");
        check(pdfCurve(d).empty(), "discrete: no curve");
        const Interval iv = coverageInterval(d, 3.0);
        check(iv.lo == 0.0 && iv.hi == 0.0, "discrete: no interval");

        Variable bad{ Dist::Normal, 0.0, 0.0 };   // σ = 0
        check(!isContinuous(bad), "normal: sigma = 0 is not continuous");
        check(pdfCurve(bad).empty(), "normal: sigma = 0 yields no curve");
        check(stdDev(bad) == 0.0 && mean(bad) == 0.0,
              "normal: sigma = 0 yields no moments");
    }
}

// ── ソルバ選定の目安 (src/core/SolverSelection) ─────────────────────────────
// 期待値はテスト側に独立に書く (定義式・手計算・極限値):
//   λ = v/f、L/λ、λ/Δx、Q ≤ f·T (DFT 分解能 1/T)、
//   Schroeder f_c = 2000√(T/V)、Thorp の吸収係数、球面拡散 20log10(r)
static void testSolverSelection()
{
    using namespace ofd::selsolver;
    g_file = "solver-selection";

    // ── 波長・電気サイズ・分解能 (定義そのもの) ─────────────────────────
    check(std::fabs(wavelength(3.0e8, 3.0e8) - 1.0) < 1e-15,
          "sel: lambda = v/f");
    check(std::fabs(wavelength(343.0, 1000.0) - 0.343) < 1e-15,
          "sel: 1 kHz in air is 0.343 m");
    check(wavelength(0.0, 1.0e9) == 0.0 && wavelength(3.0e8, 0.0) == 0.0,
          "sel: no speed or no frequency yields 0 (not computed)");

    check(std::fabs(electricalSize(8.4, 1.0) - 8.4) < 1e-15,
          "sel: L/lambda");
    check(electricalSize(1.0, 0.0) == 0.0, "sel: no wavelength yields 0");
    check(std::fabs(cellsPerWavelength(0.1, 0.005) - 20.0) < 1e-12,
          "sel: 5 mm cells give 20 per 100 mm wavelength");
    check(cellsPerWavelength(0.1, 0.0) == 0.0, "sel: no cell size yields 0");

    // ── 分解できる Q の上限 (Q ≤ f·T) ────────────────────────────────────
    // 3 GHz を 1 µs 走らせれば線幅 1 MHz まで分解できる → Q = 3e9/1e6 = 3000
    check(std::fabs(maxResolvableQ(3.0e9, 1.0e-6) - 3000.0) < 1e-9,
          "sel: Q <= f*T equals 3000 for 3 GHz over 1 us");
    check(maxResolvableQ(3.0e9, 2.0e-6) > maxResolvableQ(3.0e9, 1.0e-6),
          "sel: longer runs resolve sharper resonances");
    check(maxResolvableQ(0.0, 1.0) == 0.0, "sel: no frequency yields 0");

    // ── Schroeder 周波数 ─────────────────────────────────────────────────
    // T = 2 s, V = 12000 m³ → 2000·sqrt(2/12000) = 2000/sqrt(6000) = 25.8199 Hz
    {
        const double expect = 2000.0 / std::sqrt(6000.0);
        check(std::fabs(schroederFrequency(2.0, 12000.0) - expect) < 1e-9,
              "sel: Schroeder frequency of a 12000 m3 hall with T60 = 2 s");
        check(std::fabs(expect - 25.81988897) < 1e-6,
              "sel: that value is 25.82 Hz");
        // 大きな室ほど低く、残響が長いほど高い
        check(schroederFrequency(2.0, 24000.0) < schroederFrequency(2.0, 12000.0),
              "sel: larger rooms have a lower Schroeder frequency");
        check(schroederFrequency(3.0, 12000.0) > schroederFrequency(2.0, 12000.0),
              "sel: longer reverberation raises the Schroeder frequency");
        check(schroederFrequency(0.0, 12000.0) == 0.0,
              "sel: no reverberation time yields 0 (not computed)");
    }

    // ── Thorp の吸収係数 ─────────────────────────────────────────────────
    {
        // f → 0 では定数項 0.003 dB/km だけが残る
        check(std::fabs(thorpAbsorption_dBkm(0.0) - 0.003) < 1e-12,
              "sel: Thorp tends to 0.003 dB/km at DC");
        // 10 kHz の手計算: 0.11·100/101 + 44·100/4200 + 2.75e-4·100 + 0.003
        const double hand = 0.11 * 100.0 / 101.0 + 44.0 * 100.0 / 4200.0
                          + 2.75e-4 * 100.0 + 0.003;
        check(std::fabs(thorpAbsorption_dBkm(10.0) - hand) < 1e-12,
              "sel: Thorp at 10 kHz matches the hand calculation");
        // 文献 (Urick §5.3) の桁: 10 kHz でおよそ 1 dB/km
        check(thorpAbsorption_dBkm(10.0) > 0.8 && thorpAbsorption_dBkm(10.0) < 1.5,
              "sel: Thorp at 10 kHz is of order 1 dB/km");
        // 周波数とともに単調増加
        double prev = -1.0;
        for (double f = 0.1; f <= 60.0; f *= 1.5) {
            const double a = thorpAbsorption_dBkm(f);
            check(a > prev, "sel: Thorp absorption increases with frequency");
            prev = a;
        }
    }

    // ── 球面拡散 + 吸収 ──────────────────────────────────────────────────
    check(std::fabs(sphericalTransmissionLoss_dB(1.0, 0.0) - 60.0) < 1e-12,
          "sel: 1 km of spherical spreading is 60 dB");
    check(std::fabs(sphericalTransmissionLoss_dB(10.0, 0.5)
                    - (20.0 * std::log10(10000.0) + 5.0)) < 1e-12,
          "sel: absorption adds alpha*range");
    check(sphericalTransmissionLoss_dB(0.0, 0.5) == 0.0,
          "sel: zero range yields 0 (not computed)");
}

// ── 電波伝搬モデル (src/em/RadioPropagation) ────────────────────────────────
// 期待値はテスト側に独立に書く:
//   ITU-R P.525 の 32.44 + 20log10(f[MHz]) + 20log10(d[km])、
//   距離 2 倍で 6.02 dB、2 波モデルの遠方漸近 40log10(d) − 20log10(h_t·h_r)、
//   kT0B (T0 = 290 K)、Shannon C = B·log2(1+SNR)
static void testRadioPropagation()
{
    namespace pr = ofd::em::propagation;
    g_file = "radio-propagation";

    // ── 自由空間損失 ─────────────────────────────────────────────────────
    {
        // ITU-R P.525-4 の実用形 (定数 32.44 は 2·log10(4π/c)+120 の丸め)
        const double itu = 32.44 + 20.0 * std::log10(2400.0)
                                 + 20.0 * std::log10(1.0);
        check(std::fabs(pr::freeSpacePathLossDb(1000.0, 2.4e9) - itu) < 0.02,
              "prop: FSPL at 2.4 GHz / 1 km matches ITU-R P.525");
        // 距離 2 倍で 6.0206 dB 増える (逆二乗則)
        const double a = pr::freeSpacePathLossDb(100.0, 1.0e9);
        const double b = pr::freeSpacePathLossDb(200.0, 1.0e9);
        check(std::fabs((b - a) - 20.0 * std::log10(2.0)) < 1e-12,
              "prop: doubling the distance adds 6.02 dB");
        // 周波数 2 倍でも 6.0206 dB 増える
        const double c = pr::freeSpacePathLossDb(100.0, 2.0e9);
        check(std::fabs((c - a) - 20.0 * std::log10(2.0)) < 1e-12,
              "prop: doubling the frequency adds 6.02 dB");
        check(pr::freeSpacePathLossDb(0.0, 1.0e9) == 0.0,
              "prop: no distance yields 0 (not computed)");
        // 波長 = c/f
        check(std::fabs(pr::wavelength(1.0e9) - 0.299792458) < 1e-12,
              "prop: 1 GHz is 29.98 cm");
    }

    // ── 経路損失指数 ─────────────────────────────────────────────────────
    {
        const double l1 = pr::freeSpacePathLossDb(100.0, 3.5e9);
        const double l2 = pr::freeSpacePathLossDb(200.0, 3.5e9);
        check(std::fabs(pr::pathLossExponent(l1, 100.0, l2, 200.0) - 2.0) < 1e-9,
              "prop: free space gives exactly n = 2");
        check(pr::pathLossExponent(l1, 0.0, l2, 200.0) == 0.0,
              "prop: invalid distances yield 0");
    }

    // ── 2 波モデル ───────────────────────────────────────────────────────
    {
        const double f = 3.5e9, ht = 10.0, hr = 1.5;
        // ブレークポイント d_bp = 4·h_t·h_r/λ
        const double lam = 2.99792458e8 / f;
        const double dbp = 4.0 * ht * hr / lam;
        check(std::fabs(pr::breakpointDistance(ht, hr, f) - dbp) < 1e-9,
              "prop: breakpoint distance is 4*ht*hr/lambda");
        // 遠方では平面大地の漸近形 40log10(d) − 20log10(ht·hr) に一致する
        const double d = 100.0 * dbp;
        const double plane = 40.0 * std::log10(d)
                           - 20.0 * std::log10(ht * hr);
        check(std::fabs(pr::twoRayPathLossDb(d, ht, hr, f) - plane) < 0.1,
              "prop: two-ray tends to the plane-earth asymptote");
        // その領域の経路損失指数は 4 に近い
        const double n = pr::pathLossExponent(
            pr::twoRayPathLossDb(d, ht, hr, f), d,
            pr::twoRayPathLossDb(2.0 * d, ht, hr, f), 2.0 * d);
        check(std::fabs(n - 4.0) < 0.05, "prop: the exponent tends to 4");
        // 反射が無ければ (|Γ| = 0) 自由空間損失そのもの
        check(std::fabs(pr::twoRayPathLossDb(500.0, ht, hr, f, 0.0)
                        - pr::freeSpacePathLossDb(
                              std::sqrt(500.0 * 500.0 + (ht - hr) * (ht - hr)),
                              f)) < 1e-9,
              "prop: without a reflection the two-ray model is Friis on d_los");
        // K ファクタ = (d_ref/d_los)²、遅延差 = (d_ref − d_los)/c
        const double dist = 100.0;
        const double d1 = std::sqrt(dist * dist + (ht - hr) * (ht - hr));
        const double d2 = std::sqrt(dist * dist + (ht + hr) * (ht + hr));
        check(std::fabs(pr::twoRayKFactorDb(dist, ht, hr)
                        - 20.0 * std::log10(d2 / d1)) < 1e-12,
              "prop: two-ray K factor is the ray-length ratio");
        check(std::fabs(pr::twoRayExcessDelay(dist, ht, hr)
                        - (d2 - d1) / 2.99792458e8) < 1e-18,
              "prop: two-ray excess delay is the path difference over c");
        // 遠距離では 2 波の電力が等しくなり K → 0 dB
        check(std::fabs(pr::twoRayKFactorDb(1.0e6, ht, hr)) < 1e-3,
              "prop: K tends to 0 dB at long range");
        // 損失は上限で頭打ちにする (干渉ヌルで発散させない)
        check(pr::twoRayPathLossDb(100.0, ht, hr, f) <= pr::kMaxPathLossDb,
              "prop: path loss is capped at the null");
    }

    // ── 雑音と容量 ───────────────────────────────────────────────────────
    {
        // N = kT0B: 1 Hz・NF 0 dB で −173.98 dBm/Hz (教科書値)
        const double n0 = pr::thermalNoiseDbm(1.0, 0.0);
        check(std::fabs(n0 + 173.9754) < 1e-3,
              "prop: thermal noise density is -173.98 dBm/Hz");
        // 帯域 1e6 倍で 60 dB 増える。NF はそのまま加算される
        check(std::fabs(pr::thermalNoiseDbm(1.0e6, 0.0) - (n0 + 60.0)) < 1e-9,
              "prop: bandwidth scales the noise power by 10log10(B)");
        check(std::fabs(pr::thermalNoiseDbm(1.0e6, 7.0)
                        - (pr::thermalNoiseDbm(1.0e6, 0.0) + 7.0)) < 1e-12,
              "prop: the noise figure adds directly");
        check(pr::thermalNoiseDbm(0.0, 0.0) == 0.0,
              "prop: no bandwidth yields 0 (not computed)");
        // Shannon: SNR = 0 dB なら 1 bit/s/Hz
        check(std::fabs(pr::shannonCapacity(1.0e6, 0.0) - 1.0e6) < 1e-6,
              "prop: capacity at 0 dB SNR is 1 bit/s/Hz");
        check(std::fabs(pr::shannonCapacity(1.0e6, 30.0)
                        - 1.0e6 * std::log2(1001.0)) < 1e-3,
              "prop: capacity follows log2(1+SNR)");
        check(pr::shannonCapacity(1.0e6, -300.0) < 1.0,
              "prop: capacity vanishes without signal");
        // 受信電力 = EIRP − 損失 + 利得
        check(std::fabs(pr::receivedPowerDbm(30.0, 100.0, 3.0) + 67.0) < 1e-12,
              "prop: received power is EIRP - loss + gain");
    }
}

// ── 分散モデルのフィット (src/optics/DispersionFit) ─────────────────────────
// 期待値はテスト側に独立に書く: 参照データを Sellmeier / Drude の**解析式**で
// 作り、同じ形のモデルを当てれば残差はゼロに収束し、係数も元の値に戻るはず。
static void testDispersionFit()
{
    using namespace ofd::optics;
    g_file = "dispersion-fit";

    // λ [µm] 範囲を等間隔にサンプルする (k は「データ無し」= 負)
    const auto sample = [](double lo, double hi, int n,
                           double (*nf)(double)) {
        std::vector<NkSample> v;
        for (int i = 0; i < n; ++i) {
            const double l = lo + (hi - lo) * i / (n - 1);
            NkSample s;
            s.lambda_um = l;
            s.n = nf(l);
            s.k = -1.0;
            v.push_back(s);
        }
        return v;
    };

    // ── (1) 単極 Sellmeier を単極 Lorentz で当てる → 係数が元に戻る ─────
    // n² = 1 + 1.0·λ²/(λ² − 0.01)  (極は λ0 = 0.1 µm、強度 Δε = 1)
    {
        struct F { static double n(double l) {
            return std::sqrt(1.0 + 1.0 * l * l / (l * l - 0.01)); } };
        const std::vector<NkSample> s = sample(0.4, 1.6, 48, &F::n);
        FitOptions o;
        o.model = FitModel::Lorentz;
        o.rmsTol = 1e-12;
        o.iterations = 40;
        const FitReport r = fitDispersion(s, o);
        check(r.status == FitStatus::Ok, "fit: single Lorentz solves");
        check(r.poles == 1, "fit: Lorentz uses exactly one pole");
        check(r.rmsN < 1e-6, "fit: single-pole data is reproduced");
        check(std::fabs(r.epsInf - 1.0) < 1e-3, "fit: recovers eps_inf = 1");
        check(!r.lambda0_um.empty()
              && std::fabs(r.lambda0_um[0] - 0.1) < 1e-3,
              "fit: recovers the pole wavelength 0.1 um");
        check(!r.deltaEps.empty() && std::fabs(r.deltaEps[0] - 1.0) < 1e-3,
              "fit: recovers the oscillator strength 1.0");
        // モデル評価は参照データに一致する (サンプル点の間でも)
        const double lam = 0.777;
        check(std::fabs(modelIndex(r, lam) - F::n(lam)) < 1e-5,
              "fit: the fitted model reproduces n between the samples");
        check(r.passivityOk, "fit: a passive Sellmeier stays passive");
        check(r.nMin > 1.0, "fit: n stays above 1 for this dielectric");
    }

    // ── (2) 3 項 Sellmeier (Malitson 1965 の SiO2) — 極を増やすほど良くなる ─
    {
        struct F { static double n(double l) {
            const double l2 = l * l;
            return std::sqrt(1.0 + 0.6961663 * l2 / (l2 - 0.004679148)
                                 + 0.4079426 * l2 / (l2 - 0.013512063)
                                 + 0.8974794 * l2 / (l2 - 97.934003)); } };
        const std::vector<NkSample> s = sample(0.4, 1.6, 64, &F::n);
        FitOptions o1;
        o1.maxPoles = 1; o1.rmsTol = 0.0; o1.iterations = 12;
        FitOptions o3 = o1;
        o3.maxPoles = 3;
        const FitReport r1 = fitDispersion(s, o1);
        const FitReport r3 = fitDispersion(s, o3);
        check(r1.status == FitStatus::Ok && r3.status == FitStatus::Ok,
              "fit: multi-pole fits solve");
        check(r3.rmsN <= r1.rmsN,
              "fit: more coefficients cannot make the residual worse");
        check(r3.rmsN < 1e-5, "fit: three poles reproduce fused silica");
        check(r1.points == 64 && r3.points == 64, "fit: all samples are used");
        // 参照データは公刊値そのものなので n(0.5876 µm) = 1.4585 (Malitson)
        check(std::fabs(F::n(0.58756) - 1.45846) < 1e-4,
              "fit: the reference data is the published Malitson curve");
        check(std::fabs(modelIndex(r3, 0.58756) - F::n(0.58756)) < 1e-4,
              "fit: the fit follows the published curve at the d line");
        // 許容値に達したら極を増やさない (係数の数の制御が効く)
        FitOptions oTol = o3;
        oTol.rmsTol = 1e-2;
        const FitReport rTol = fitDispersion(s, oTol);
        check(rTol.poles <= r3.poles,
              "fit: a loose tolerance stops adding poles");
    }

    // ── (3) Drude 参照データを Drude で当てる ────────────────────────────
    // ε = 4 − (λ/2)² → n = sqrt(4 − λ²/4)
    {
        struct F { static double n(double l) {
            return std::sqrt(4.0 - 0.25 * l * l); } };
        const std::vector<NkSample> s = sample(0.4, 1.2, 32, &F::n);
        FitOptions o;
        o.model = FitModel::Drude;
        const FitReport r = fitDispersion(s, o);
        check(r.status == FitStatus::Ok, "fit: Drude solves");
        check(r.rmsN < 1e-9, "fit: Drude data is reproduced exactly");
        check(std::fabs(r.epsInf - 4.0) < 1e-6, "fit: recovers eps_inf = 4");
        check(!r.lambda0_um.empty()
              && std::fabs(r.lambda0_um[0] - 2.0) < 1e-6,
              "fit: recovers the plasma wavelength 2 um");
        check(r.passivityOk, "fit: eps_inf >= 1 and wp^2 >= 0 is passive");
    }

    // ── (4) FDTD 安定性: n < 1 の帯域を持つモデルを検出する ──────────────
    {
        // ε = 1.2 − λ² → λ = 1 µm で ε = 0.2 (n = 0.447 < 1)
        struct F { static double n(double l) {
            return std::sqrt(1.2 - l * l); } };
        const std::vector<NkSample> s = sample(0.3, 1.0, 24, &F::n);
        FitOptions o;
        o.model = FitModel::Drude;
        const FitReport r = fitDispersion(s, o);
        check(r.status == FitStatus::Ok, "fit: sub-unity index model solves");
        check(r.nMin < 1.0, "fit: n_min below 1 is reported (Courant limit)");
        check(std::fabs(r.nMin - std::sqrt(0.2)) < 1e-3,
              "fit: n_min equals the analytic minimum");
    }

    // ── (5) 因果律の必要条件 (透明域で dε/dω ≥ 0 = λ について非増加) ─────
    {
        // 正常分散 (λ が伸びると n が下がる)
        struct Norm { static double n(double l) { return 1.5 - 0.05 * l; } };
        const FitReport ok = fitDispersion(sample(0.4, 1.6, 16, &Norm::n),
                                           FitOptions());
        check(ok.causalityEvaluable, "fit: transparent data can be judged");
        check(ok.causalityChecks == 15, "fit: judges every sample interval");
        check(ok.causalityViolations == 0,
              "fit: normal dispersion satisfies the requirement");
        // 異常分散 (吸収を伴わずに n が上がる = 因果律の必要条件に反する)
        struct Anom { static double n(double l) { return 1.5 + 0.05 * l; } };
        const FitReport ng = fitDispersion(sample(0.4, 1.6, 16, &Anom::n),
                                           FitOptions());
        check(ng.causalityEvaluable, "fit: the same judgement applies");
        check(ng.causalityViolations == ng.causalityChecks,
              "fit: anomalous dispersion violates it on every interval");
        // 無分散 (真空・n 一定) は境界例として満足側に入れる
        struct Flat { static double n(double) { return 1.0; } };
        const FitReport flat = fitDispersion(sample(0.4, 1.6, 16, &Flat::n),
                                             FitOptions());
        check(flat.causalityViolations == 0,
              "fit: a dispersionless medium is not flagged");
        // 吸収域を含むデータは必要条件では判定しない (評価対象外)
        {
            std::vector<NkSample> s = sample(0.4, 1.6, 16, &Norm::n);
            for (NkSample &p : s) p.k = 0.1;
            const FitReport r = fitDispersion(s, FitOptions());
            check(!r.causalityEvaluable,
                  "fit: absorbing data is out of scope for the requirement");
            check(r.hasK, "fit: k data is recognised when present");
            check(std::fabs(r.rmsK - 0.1) < 1e-9,
                  "fit: a lossless model leaves the whole k as residual");
        }
    }

    // ── (6) データが無い / 補間モデル ────────────────────────────────────
    {
        const FitReport none = fitDispersion(std::vector<NkSample>(),
                                             FitOptions());
        check(none.status == FitStatus::NoData, "fit: no data is reported");
        check(!none.causalityEvaluable && none.points == 0,
              "fit: nothing is judged without data");
        check(modelIndex(none, 1.0) == 0.0,
              "fit: a failed fit evaluates to 0 (no fabricated index)");

        struct F { static double n(double l) { return 1.5 - 0.05 * l; } };
        FitOptions o;
        o.model = FitModel::Sampled;
        const FitReport r = fitDispersion(sample(0.4, 1.6, 16, &F::n), o);
        check(r.status == FitStatus::Ok && r.interpolation,
              "fit: Sampled is flagged as interpolation");
        check(r.rmsN == 0.0 && r.poles == 0,
              "fit: interpolation has no residual and no poles");
        check(modelIndex(r, 1.0) == 0.0,
              "fit: interpolation has no pole model to evaluate");
    }
}

// ── 曲げ導波路の共形変換 (src/optics/BendWaveguide) ─────────────────────────
// 期待値はテスト側に独立に書く: 変換式そのもの、重なり積分の定義、
// R → ∞ の極限 (直線に戻る) と単調性。
static void testBendWaveguide()
{
    using namespace ofd::optics;
    g_file = "bend-waveguide";

    // ── (1) 共形変換 n_eq(x) = n(x)(1 + x/R) ─────────────────────────────
    {
        CrossSection cs = makeRectangularCore(0.45, 0.22, 0.0,
                                              3.4764, 1.4440, 1.4440, 0.025);
        check(cs.nx > 0 && cs.ny > 0, "bend: the test cross-section is built");
        const double R = 5.0;
        const CrossSection b = bendEquivalent(cs, R);
        check(b.nx == cs.nx && b.ny == cs.ny && b.n.size() == cs.n.size(),
              "bend: the transform keeps the grid");
        bool exact = true;
        for (int ix = 0; ix < cs.nx; ++ix) {
            const double x = (ix + 0.5 - 0.5 * cs.nx) * cs.dx_um;
            const double f = 1.0 + x / R;
            for (int iy = 0; iy < cs.ny; iy += 7) {
                const std::size_t i = std::size_t(iy) * cs.nx + ix;
                if (std::fabs(b.n[i] - cs.n[i] * f) > 1e-12) exact = false;
            }
        }
        check(exact, "bend: every cell is scaled by 1 + x/R");
        // 外周側は屈折率が上がり、内周側は下がる
        check(b.n[cs.nx - 1] > cs.n[cs.nx - 1] && b.n[0] < cs.n[0],
              "bend: the outer side gains index and the inner side loses it");
        // R → ∞ で直線に戻る
        const CrossSection s = bendEquivalent(cs, 1.0e9);
        double worst = 0.0;
        for (std::size_t i = 0; i < cs.n.size(); ++i)
            worst = std::max(worst, std::fabs(s.n[i] - cs.n[i]));
        check(worst < 1e-6, "bend: an infinite radius reproduces the straight guide");
        check(bendEquivalent(cs, 0.0).n == cs.n,
              "bend: a non-positive radius leaves the guide unchanged");
        // 妥当性指標 |x|/R
        const double xEdge = (cs.nx - 0.5 - 0.5 * cs.nx) * cs.dx_um;
        check(std::fabs(conformalRatio(cs, R) - xEdge / R) < 1e-12,
              "bend: the conformal ratio is |x_edge|/R");
        check(conformalRatio(cs, 20.0) < conformalRatio(cs, 5.0),
              "bend: larger radii are better approximated");
    }

    // ── (2) 重なり積分と接続損 (定義そのもの) ────────────────────────────
    {
        const int n = 64;
        std::vector<double> a(n), b(n), c(n);
        const double pi = 3.14159265358979323846;
        for (int i = 0; i < n; ++i) {
            a[i] = std::sin(pi * (i + 1) / (n + 1));
            b[i] = 2.0 * a[i];                       // 振幅だけ違う
            c[i] = std::sin(2.0 * pi * (i + 1) / (n + 1));   // 直交モード
        }
        check(std::fabs(overlapEfficiency(a, a) - 1.0) < 1e-12,
              "bend: a mode overlaps itself perfectly");
        check(std::fabs(overlapEfficiency(a, b) - 1.0) < 1e-12,
              "bend: the overlap is normalised (amplitude does not matter)");
        check(overlapEfficiency(a, c) < 1e-6,
              "bend: orthogonal modes do not overlap");
        check(overlapEfficiency(a, std::vector<double>()) == 0.0,
              "bend: mismatched sizes yield 0");
        check(std::fabs(mismatchLossDb(1.0)) < 1e-12,
              "bend: a perfect overlap costs 0 dB");
        check(std::fabs(mismatchLossDb(0.5) - 10.0 * std::log10(2.0)) < 1e-12,
              "bend: half the power is 3.01 dB");
        check(mismatchLossDb(0.0) >= 300.0,
              "bend: a vanishing overlap is capped instead of diverging");
    }

    // ── (3) 放射カウスティック x_c = R(neff/n_clad − 1) ──────────────────
    {
        check(std::fabs(radiationCaustic(5.0, 2.4, 1.44) - 5.0 * (2.4 / 1.44 - 1.0))
                  < 1e-12,
              "bend: the caustic follows the definition");
        check(std::fabs(radiationCaustic(5.0, 2.4, 1.44) - 3.3333333333) < 1e-9,
              "bend: 5 um radius with neff 2.4 gives 3.33 um");
        check(radiationCaustic(10.0, 2.4, 1.44) > radiationCaustic(5.0, 2.4, 1.44),
              "bend: a larger radius pushes the caustic further out");
        check(radiationCaustic(5.0, 1.4, 1.44) == 0.0,
              "bend: a non-guided index yields no caustic");
        check(radiationCaustic(0.0, 2.4, 1.44) == 0.0,
              "bend: a non-positive radius yields no caustic");
    }

    // ── (4) FDE と組んだ接続損: 半径が大きいほど小さく、直線で 0 になる ──
    {
        CrossSection cs = makeRectangularCore(0.45, 0.22, 0.0,
                                              3.4764, 1.4440, 1.4440, 0.025);
        SolveOptions o;
        o.pol = Polarization::SemiVecTE;
        o.modes = 1;
        const std::vector<ModeResult> st = solveModes(cs, 1.55, o);
        check(!st.empty(), "bend: the straight mode is found");
        if (!st.empty()) {
            double prev = 1e9;
            for (double R : { 3.0, 10.0 }) {
                const std::vector<ModeResult> bm =
                    solveModes(bendEquivalent(cs, R), 1.55, o);
                check(!bm.empty(), "bend: the equivalent bent mode is found");
                if (bm.empty()) continue;
                const double eta = overlapEfficiency(st[0].field, bm[0].field);
                const double loss = mismatchLossDb(eta);
                check(eta > 0.9 && eta <= 1.0,
                      "bend: the bent mode still resembles the straight one");
                check(loss < prev, "bend: mismatch loss falls as R grows");
                prev = loss;
                // 曲げると外周側へ寄るので実効屈折率は上がる
                check(bm[0].neff > st[0].neff,
                      "bend: the bend raises neff (the mode shifts outwards)");
            }
            check(prev < 0.05, "bend: a 10 um bend costs well under 0.05 dB");
            // R → ∞ は完全一致 (同じ固有値問題)
            const std::vector<ModeResult> sm =
                solveModes(bendEquivalent(cs, 1.0e9), 1.55, o);
            check(!sm.empty() && overlapEfficiency(st[0].field, sm[0].field)
                                     > 1.0 - 1e-9,
                  "bend: an infinite radius costs nothing");
        }
    }
}

// ── 取込メッシュの位相・幾何検査 (io/MeshDiagnostics) ───────────────────────
// GeometryTab「ジオメトリ検査」節の検出値の実体。期待値はコードからではなく
// 立方体の位相 (頂点 8・辺 18・面 12 三角形) から手で導ける値を使う。
static void testMeshDiagnostics()
{
    g_file = "mesh-diagnostics";

    // 外向き法線で首尾一貫させた閉じた立方体 (12 三角形)。
    // 面ごとの 4 頂点を外から見て CCW に並べる = 全体で coherent orientation。
    auto orientedCube = [](double a) {
        ImportedMesh m;
        m.name = "cube";
        const double h = a / 2.0;
        double p[8][3] = {
            {-h,-h,-h},{ h,-h,-h},{ h, h,-h},{-h, h,-h},
            {-h,-h, h},{ h,-h, h},{ h, h, h},{-h, h, h} };
        auto quad = [&](int i0, int i1, int i2, int i3) {
            addTri(m, p[i0][0],p[i0][1],p[i0][2],
                      p[i1][0],p[i1][1],p[i1][2],
                      p[i2][0],p[i2][1],p[i2][2]);
            addTri(m, p[i0][0],p[i0][1],p[i0][2],
                      p[i2][0],p[i2][1],p[i2][2],
                      p[i3][0],p[i3][1],p[i3][2]);
        };
        quad(0, 3, 2, 1);   // z− 面
        quad(4, 5, 6, 7);   // z+ 面
        quad(0, 1, 5, 4);   // y− 面
        quad(3, 7, 6, 2);   // y+ 面
        quad(0, 4, 7, 3);   // x− 面
        quad(1, 2, 6, 5);   // x+ 面
        m.bbox[0] = m.bbox[1] = m.bbox[2] = -h;
        m.bbox[3] = m.bbox[4] = m.bbox[5] =  h;
        return m;
    };

    // 1) 健全な閉多様体: 穴なし・非多様体なし・向き一致・重複頂点 36−8=28
    {
        const ImportedMesh cube = orientedCube(0.02);
        const MeshDiagnostics d = analyzeMesh(cube);
        check(d.valid && !d.skippedTooLarge, "meshdiag: cube analyzed");
        check(d.triangles == 12, "meshdiag: cube triangle count");
        check(d.rawVertices == 36, "meshdiag: cube raw vertex count");
        check(d.uniqueVertices == 8, "meshdiag: cube welds to 8 vertices");
        check(d.duplicateVertices == 28, "meshdiag: cube duplicate vertices");
        check(d.degenerateTriangles == 0, "meshdiag: cube has no degenerates");
        check(d.boundaryEdges == 0, "meshdiag: cube has no boundary edges");
        check(d.nonManifoldEdges == 0, "meshdiag: cube is manifold");
        check(d.inconsistentEdges == 0, "meshdiag: cube orientation coherent");
        check(d.watertight(), "meshdiag: cube watertight");
        check(d.weldTolerance > 0.0, "meshdiag: weld tolerance positive");
    }

    // 2) 三角形を 1 枚落とす → その 3 辺が境界エッジになり水密でなくなる
    {
        ImportedMesh holed = orientedCube(0.02);
        holed.vertices.resize(holed.vertices.size() - 9);
        --holed.numTriangles;
        const MeshDiagnostics d = analyzeMesh(holed);
        check(d.triangles == 11, "meshdiag: holed triangle count");
        check(d.boundaryEdges == 3, "meshdiag: hole gives 3 boundary edges");
        check(d.nonManifoldEdges == 0, "meshdiag: holed still manifold");
        check(!d.watertight(), "meshdiag: holed not watertight");
    }

    // 3) 三角形を 1 枚複製 → その 3 辺が 3 枚共有 = 非多様体
    {
        ImportedMesh dup = orientedCube(0.02);
        for (int i = 0; i < 9; ++i) dup.vertices.push_back(dup.vertices[i]);
        ++dup.numTriangles;
        const MeshDiagnostics d = analyzeMesh(dup);
        check(d.nonManifoldEdges == 3, "meshdiag: duplicated face is non-manifold");
        check(d.boundaryEdges == 0, "meshdiag: duplicated face has no boundary");
        check(!d.watertight(), "meshdiag: non-manifold is not watertight");
    }

    // 4) 三角形を 1 枚裏返す → その 3 辺で向きが一致しなくなる
    {
        ImportedMesh flip = orientedCube(0.02);
        for (int k = 0; k < 3; ++k)      // 頂点 1 と 2 を入れ替える
            std::swap(flip.vertices[3 + k], flip.vertices[6 + k]);
        const MeshDiagnostics d = analyzeMesh(flip);
        check(d.inconsistentEdges == 3, "meshdiag: flipped face gives 3 bad edges");
        check(d.boundaryEdges == 0 && d.nonManifoldEdges == 0,
              "meshdiag: flipped face keeps the topology closed");
        // 位相としては閉じている (向きの不一致は watertight の判定に含めない)
        check(d.watertight(), "meshdiag: flipped face still closed");
    }

    // 5) 縮退三角形 (3 頂点が同一) は数えられ、辺の位相を汚さない
    {
        ImportedMesh deg = orientedCube(0.02);
        addTri(deg, 0.001, 0.001, 0.001, 0.001, 0.001, 0.001,
                    0.001, 0.001, 0.001);
        const MeshDiagnostics d = analyzeMesh(deg);
        check(d.degenerateTriangles == 1, "meshdiag: degenerate triangle counted");
        check(d.boundaryEdges == 0 && d.nonManifoldEdges == 0,
              "meshdiag: degenerate triangle excluded from edge topology");
    }

    // 6) 空メッシュ / 上限超過は「検査していない」ことを返す (偽の OK を出さない)
    {
        const ImportedMesh empty;
        const MeshDiagnostics d = analyzeMesh(empty);
        check(!d.valid && !d.watertight(), "meshdiag: empty mesh is not valid");

        const ImportedMesh cube = orientedCube(0.02);
        const MeshDiagnostics s = analyzeMesh(cube, 4);   // 上限 4 三角形
        check(!s.valid && s.skippedTooLarge && s.triangles == 12,
              "meshdiag: oversized mesh is skipped, not silently OK");
        check(!s.watertight(), "meshdiag: skipped mesh never reports watertight");
    }
}

// ── EMC 規格の公表限度値と対策効果の古典式 (em/EmcStandards) ────────────────
// 期待値はコードと独立な出所 (規格の表 / 手計算) から取る。
static void testEmcStandards()
{
    using namespace ofd::em::emc;
    g_file = "emc-standards";

    auto approx = [](double a, double b, double tol) {
        return std::fabs(a - b) <= tol;
    };

    // 1) 放射妨害波の限度値 (CISPR 32:2015 Table A.3/A.4 の公表値)
    {
        LimitSegment seg[kMaxLimitSegments];
        int n = radiatedLimits(Standard::Cispr32, EmClass::A, seg,
                               kMaxLimitSegments);
        check(n == 2, "emc: CISPR 32 Class A has two segments");
        if (n == 2) {
            check(approx(seg[0].f1_MHz, 30.0, 0) &&
                  approx(seg[0].f2_MHz, 230.0, 0) &&
                  approx(seg[0].limit_dBuVm, 40.0, 0) &&
                  approx(seg[0].refDist_m, 10.0, 0),
                  "emc: CISPR 32 Class A 30-230 MHz = 40 dBuV/m @10 m");
            check(approx(seg[1].limit_dBuVm, 47.0, 0) &&
                  approx(seg[1].f2_MHz, 1000.0, 0),
                  "emc: CISPR 32 Class A 230-1000 MHz = 47 dBuV/m");
        }
        n = radiatedLimits(Standard::Cispr32, EmClass::B, seg,
                           kMaxLimitSegments);
        check(n == 2 && approx(seg[0].limit_dBuVm, 30.0, 0) &&
              approx(seg[1].limit_dBuVm, 37.0, 0),
              "emc: CISPR 32 Class B = 30 / 37 dBuV/m @10 m");

        // FCC 47 CFR §15.109: μV/m 規定 → dBμV/m (100 μV/m = 40 dBμV/m)
        n = radiatedLimits(Standard::Fcc15, EmClass::B, seg, kMaxLimitSegments);
        check(n == 4, "emc: FCC Part 15 Class B has four segments");
        if (n == 4) {
            check(approx(seg[0].limit_dBuVm, 40.0, 1e-9) &&
                  approx(seg[0].refDist_m, 3.0, 0),
                  "emc: FCC Class B 30-88 MHz = 100 uV/m (40 dBuV/m) @3 m");
            check(approx(seg[1].limit_dBuVm, 43.5218, 1e-3) &&
                  approx(seg[2].limit_dBuVm, 46.0206, 1e-3) &&
                  approx(seg[3].limit_dBuVm, 53.9794, 1e-3),
                  "emc: FCC Class B 150/200/500 uV/m in dBuV/m");
        }
        n = radiatedLimits(Standard::Fcc15, EmClass::A, seg, kMaxLimitSegments);
        check(n == 4 && approx(seg[0].limit_dBuVm, 39.0849, 1e-3) &&
              approx(seg[0].refDist_m, 10.0, 0),
              "emc: FCC Class A 30-88 MHz = 90 uV/m @10 m");

        // 収載していない規格は 0 件 (推定値を作らない)
        check(radiatedLimits(Standard::None, EmClass::A, seg,
                             kMaxLimitSegments) == 0,
              "emc: unlisted standard yields no limits");
    }

    // 2) 逆距離則による距離換算と区間検索
    {
        LimitSegment seg[kMaxLimitSegments];
        const int n = radiatedLimits(Standard::Cispr32, EmClass::B, seg,
                                     kMaxLimitSegments);
        // 30 dBμV/m @10 m → 3 m では +20log10(10/3) = +10.4576 dB
        check(approx(limitAtDistance(seg[0], 3.0), 40.4576, 1e-3),
              "emc: inverse-distance extrapolation 10 m -> 3 m");
        check(approx(limitAtDistance(seg[0], 10.0), 30.0, 1e-12),
              "emc: same distance leaves the limit unchanged");
        check(approx(limitAtDistance(seg[0], 0.0), 30.0, 1e-12),
              "emc: invalid distance falls back to the reference value");
        check(limitSegmentIndex(seg, n, 30.0) == 0 &&
              limitSegmentIndex(seg, n, 230.0) == 0 &&
              limitSegmentIndex(seg, n, 230.1) == 1 &&
              limitSegmentIndex(seg, n, 1500.0) == -1,
              "emc: limit segment lookup honours the band edges");
    }

    // 3) シールド遮蔽効果 SE = A + R + B (Schelkunoff / Ott)
    {
        // 銅 1 mm @500 MHz: δ = 1/√(πfμσ) = 2.955 μm
        const ShieldSE se = shieldEffectiveness(5.0e8, 1.0e-3, 1.0, 1.0);
        check(se.valid, "emc: shield SE is valid for a positive thickness");
        check(approx(se.skinDepth_m * 1e6, 2.9554, 1e-3),
              "emc: copper skin depth at 500 MHz = 2.955 um");
        check(approx(se.absorption_dB, 8.686 * 1.0e-3 / se.skinDepth_m, 1e-6),
              "emc: absorption loss A = 8.686 t/delta");
        // R = 168 + 10log10(sigma_r/(mu_r f)) = 168 - 86.9897
        check(approx(se.reflection_dB, 81.0103, 1e-3),
              "emc: plane-wave reflection loss of copper at 500 MHz");
        check(approx(se.multiRefl_dB, 0.0, 1e-6),
              "emc: multiple-reflection term vanishes for a thick shield");
        check(approx(se.total_dB,
                     se.absorption_dB + se.reflection_dB + se.multiRefl_dB, 1e-9),
              "emc: SE is the sum A + R + B");
        // 鋼は導電率が低く透磁率が高い → 吸収損は増え反射損は減る
        const ShieldSE st = shieldEffectiveness(5.0e8, 1.0e-3, 0.10, 1000.0);
        check(st.absorption_dB > se.absorption_dB &&
              st.reflection_dB < se.reflection_dB,
              "emc: steel absorbs more and reflects less than copper");
        // 無効入力では計算しない (0 を返し valid=false)
        const ShieldSE bad = shieldEffectiveness(5.0e8, 0.0, 1.0, 1.0);
        check(!bad.valid && bad.total_dB == 0.0,
              "emc: zero thickness gives no shielding effectiveness");
        // 材料表 (Ott Table 6-1)
        check(approx(shieldMaterial(0).sigmaRel, 1.0, 0) &&
              approx(shieldMaterial(1).sigmaRel, 0.61, 0) &&
              approx(shieldMaterial(2).muRel, 1000.0, 0),
              "emc: shield material constants (Cu / Al / steel)");
    }

    // 4) 開口 (スリット) の遮蔽効果 SE = 20log10(lambda/2L) - 10log10(n)
    {
        // 500 MHz: λ = 0.59958 m、L = 50 mm → 20log10(5.9958) = 15.556 dB
        check(approx(apertureSE_dB(5.0e8, 0.050, 1), 15.5561, 1e-3),
              "emc: aperture SE of a 50 mm slot at 500 MHz");
        check(approx(apertureSE_dB(5.0e8, 0.050, 4),
                     15.5561 - 6.0206, 1e-3),
              "emc: four apertures lose 10log10(4) dB");
        check(apertureSE_dB(5.0e8, 0.40, 1) == 0.0,
              "emc: an aperture at or above lambda/2 gives no shielding");
        check(apertureSE_dB(5.0e8, 0.0, 1) == 0.0,
              "emc: invalid aperture input yields 0");
        // 幅を半分にすると 20log10(2) = 6.02 dB 改善
        check(approx(apertureShrinkGain_dB(0.5), 6.0206, 1e-3),
              "emc: halving the slot width gains 6 dB");
        check(apertureShrinkGain_dB(1.0) == 0.0 &&
              apertureShrinkGain_dB(0.0) == 0.0,
              "emc: no gain when the slot is not shrunk");
    }

    // 5) 挿入損失 / ESD 電流 / 電力密度
    {
        // Z = 300 Ω を 150 Ω 回路へ直列挿入 → 20log10(3) = 9.542 dB
        check(approx(insertionLoss_dB(300.0, 150.0), 9.5424, 1e-3),
              "emc: insertion loss of a 300 ohm series element in 150 ohm");
        check(insertionLoss_dB(300.0, 0.0) == 0.0,
              "emc: insertion loss needs a positive circuit impedance");
        // 1 μH の理想インダクタ @500 MHz = 3141.6 Ω
        check(approx(inductiveReactance(5.0e8, 1.0e-6), 3141.5927, 1e-3),
              "emc: inductive reactance 2*pi*f*L");
        // IEC 61000-4-2 Table 2: レベル 4 (8 kV) = 30 / 16 / 8 A
        const EsdContactCurrent c = esdContactCurrent(8.0);
        check(approx(c.firstPeak_A, 30.0, 1e-9) &&
              approx(c.at30ns_A, 16.0, 1e-9) &&
              approx(c.at60ns_A, 8.0, 1e-9),
              "emc: IEC 61000-4-2 contact discharge currents at 8 kV");
        check(approx(esdContactCurrent(2.0).firstPeak_A, 7.5, 1e-9),
              "emc: IEC 61000-4-2 level 1 (2 kV) first peak = 7.5 A");
        // S = E^2/Z0 (10 V/m → 0.26544 W/m^2)、80% AM の尖頭は 1.8 倍
        check(approx(powerDensity_Wm2(10.0), 0.2654419, 1e-6),
              "emc: plane-wave power density S = E^2/Z0");
        check(approx(amModulatedPeakField(10.0), 18.0, 1e-9),
              "emc: 80% AM peak envelope = 1.8 x test level");
    }
}

// ── 集中定数 RLC の |Z(f)| (em/LumpedRlc) ───────────────────────────────────
static void testLumpedRlc()
{
    using namespace ofd::em;
    g_file = "lumped-rlc";

    auto approx = [](double a, double b, double tol) {
        return std::fabs(a - b) <= tol;
    };

    RlcModel m;
    m.r_ohm = 0.01;
    m.l_H = 50e-9;
    m.c_F = 200e-12;

    // 共振周波数 f0 = 1/(2*pi*sqrt(LC)) = 50.329 MHz
    const double f0 = rlcResonanceHz(m.l_H, m.c_F);
    check(approx(f0 * 1e-6, 50.3292, 1e-3), "rlc: f0 = 1/(2 pi sqrt(LC))");
    check(rlcResonanceHz(0.0, 200e-12) == 0.0 &&
          rlcResonanceHz(50e-9, 0.0) == 0.0,
          "rlc: no resonance without both L and C");

    // 直列: 1 MHz では容量性 1/(wC) = 795.77 ohm が支配的
    {
        const RlcImpedance z = rlcImpedance(m, 1.0e6);
        check(z.valid, "rlc: series impedance is valid at 1 MHz");
        check(approx(z.xL_ohm, 0.314159, 1e-6) &&
              approx(z.xC_ohm, 795.7747, 1e-3),
              "rlc: reactances wL and 1/wC at 1 MHz");
        check(approx(z.magnitude_ohm, 795.4606, 1e-3),
              "rlc: |Z| = sqrt(R^2 + (wL - 1/wC)^2)");
    }
    // 直列: 共振で |Z| = R (最小)
    {
        const RlcImpedance z = rlcImpedance(m, f0);
        check(approx(z.magnitude_ohm, m.r_ohm, 1e-9),
              "rlc: series |Z| falls to R at resonance");
        check(rlcImpedance(m, f0 * 2.0).magnitude_ohm > m.r_ohm,
              "rlc: series |Z| rises away from resonance");
    }
    // 並列: 共振で |Z| = R (最大)
    {
        RlcModel p = m;
        p.topology = RlcTopology::Parallel;
        const RlcImpedance z = rlcImpedance(p, f0);
        check(approx(z.magnitude_ohm, p.r_ohm, 1e-9),
              "rlc: parallel |Z| peaks at R at resonance");
        check(rlcImpedance(p, f0 * 2.0).magnitude_ohm < p.r_ohm,
              "rlc: parallel |Z| falls away from resonance");
    }
    // 素子の不在: 直列は短絡 (0 ohm)、並列は開放 (アドミタンス 0)
    {
        RlcModel s;
        s.r_ohm = 3.0;
        s.l_H = 50e-9;                       // C 無し
        const RlcImpedance z = rlcImpedance(s, 1.0e8);
        check(z.xC_ohm == 0.0 &&
              approx(z.magnitude_ohm,
                     std::sqrt(9.0 + z.xL_ohm * z.xL_ohm), 1e-9),
              "rlc: a missing series capacitor is a short");
        RlcModel p;
        p.r_ohm = 3.0;
        p.topology = RlcTopology::Parallel;  // R のみ
        check(approx(rlcImpedance(p, 1.0e8).magnitude_ohm, 3.0, 1e-9),
              "rlc: a parallel R alone gives |Z| = R");
        RlcModel none;
        check(!rlcImpedance(none, 1.0e8).valid,
              "rlc: an empty model is not valid");
        check(!rlcImpedance(m, 0.0).valid, "rlc: f <= 0 is not valid");
    }
}

// ── 回路ポート定義 (.ofdx "circuit.ports") ──────────────────────────────────
static void testCircuitPorts()
{
    g_file = "circuit-ports";

    // 1) 既定は 3 行 — .ofd も .ofdx も従来の出力のまま (追加キーを書かない)
    {
        Project p;
        check(p.circuitPorts().size() == 3, "cirport: default has three ports");
        check(p.circuitPorts()[0].name == "VIN" &&
              p.circuitPorts()[0].kind == CircuitPortRow::Lumped &&
              p.circuitPorts()[2].kind == CircuitPortRow::Probe,
              "cirport: default rows are the documented initial values");
        const QString base = OfdIO::serialize(p);
        CircuitPortRow extra;
        extra.name = QStringLiteral("PORT4");
        p.circuitPorts().push_back(extra);
        check(OfdIO::serialize(p) == base,
              "cirport: ports keep .ofd output byte-identical");

        QTemporaryFile def;
        def.setFileTemplate(QDir::tempPath() + "/ofdx_port_def_XXXXXX.ofdx");
        if (def.open()) {
            Project q;   // 既定のまま
            check(OfdxIO::save(def.fileName(), q), "cirport default ofdx save");
            QFile jf(def.fileName());
            check(jf.open(QIODevice::ReadOnly), "cirport default ofdx reopen");
            const QJsonObject root =
                QJsonDocument::fromJson(jf.readAll()).object();
            check(!root.contains("circuit"),
                  "cirport: no circuit key while the table is unedited");
        }
    }

    // 2) .ofdx ラウンドトリップ (a)
    {
        Project p1;
        QVector<CircuitPortRow> &ports = p1.circuitPorts();
        ports[0].name = QStringLiteral("VBUS_IN");
        ports[0].net = QStringLiteral("NET_A");
        ports[1].enabled = false;
        ports[2].kind = CircuitPortRow::Probe;
        CircuitPortRow added;
        added.name = QStringLiteral("PORT4");
        added.kind = CircuitPortRow::Probe;
        added.net = QStringLiteral("NET_D");
        added.ref = QStringLiteral("AGND");
        ports.push_back(added);

        QTemporaryFile ofdx;
        ofdx.setFileTemplate(QDir::tempPath() + "/ofdx_port_XXXXXX.ofdx");
        if (ofdx.open()) {
            check(OfdxIO::save(ofdx.fileName(), p1), "cirport ofdx save");
            Project p2;
            check(OfdxIO::load(ofdx.fileName(), p2), "cirport ofdx load");
            const QVector<CircuitPortRow> &r = p2.circuitPorts();
            check(r.size() == 4, "cirport: four ports round-trip");
            if (r.size() == 4) {
                check(r[0].name == "VBUS_IN" && r[0].net == "NET_A" &&
                      r[0].enabled && r[0].ref == "GND",
                      "cirport: first port fields round-trip");
                check(!r[1].enabled, "cirport: disabled flag round-trips");
                check(r[3].name == "PORT4" &&
                      r[3].kind == CircuitPortRow::Probe &&
                      r[3].ref == "AGND",
                      "cirport: appended port round-trips");
            }
            QFile jf(ofdx.fileName());
            check(jf.open(QIODevice::ReadOnly), "cirport ofdx reopen");
            const QJsonObject root =
                QJsonDocument::fromJson(jf.readAll()).object();
            check(root.value("circuit").toObject()
                      .value("ports").toArray().size() == 4,
                  "cirport: circuit.ports json array length");
            check(root.contains("optical") && root.contains("acoustic"),
                  "cirport: existing ofdx sections untouched");
        }
    }

    // 3) 旧 .ofdx (circuit キー無し): 既定 3 行のまま (b)
    {
        QTemporaryFile old;
        old.setFileTemplate(QDir::tempPath() + "/ofdx_port_old_XXXXXX.ofdx");
        if (old.open()) {
            const QByteArray legacy =
                "{ \"schemaVersion\": \"1.0\", \"domain\": \"em\","
                "  \"optical\": { \"solver\": 0 } }";
            old.write(legacy);
            old.flush();
            Project p;
            check(OfdxIO::load(old.fileName(), p), "cirport legacy ofdx load");
            check(p.circuitPorts().size() == 3 &&
                  p.circuitPorts()[0].name == "VIN",
                  "cirport: legacy file keeps the default ports");
        }
        Project p;
        p.circuitPorts().clear();
        p.clear();
        check(p.circuitPorts().size() == 3,
              "cirport: clear() restores the default ports");
    }
}

// ── フォトニック回路ネットリスト (.ofdx "schematic.netlist") ────────────────
static void testPhotonicNetlist()
{
    g_file = "photonic-netlist";

    // 1) 既定は 5 行 — 追加キーを書かない (旧 .ofdx とバイト一致)
    {
        Project p;
        check(p.photonicNetlist().size() == 5,
              "phnet: default netlist has five connections");
        check(p.photonicNetlist()[0].from == "LASER1.out" &&
              p.photonicNetlist()[0].to == "MZI1.in1",
              "phnet: default rows are the documented initial values");
        const QString base = OfdIO::serialize(p);
        p.photonicNetlist().push_back(PhotonicNetRow{});
        check(OfdIO::serialize(p) == base,
              "phnet: netlist keeps .ofd output byte-identical");

        QTemporaryFile def;
        def.setFileTemplate(QDir::tempPath() + "/ofdx_net_def_XXXXXX.ofdx");
        if (def.open()) {
            Project q;
            check(OfdxIO::save(def.fileName(), q), "phnet default ofdx save");
            QFile jf(def.fileName());
            check(jf.open(QIODevice::ReadOnly), "phnet default ofdx reopen");
            const QJsonObject root =
                QJsonDocument::fromJson(jf.readAll()).object();
            check(!root.contains("schematic"),
                  "phnet: no schematic key while the table is unedited");
        }
    }

    // 2) .ofdx ラウンドトリップ (a)
    {
        Project p1;
        QVector<PhotonicNetRow> &net = p1.photonicNetlist();
        net.remove(4);
        net[0].to = QStringLiteral("MMI1.in");
        net[1].enabled = false;
        PhotonicNetRow added;
        added.from = QStringLiteral("PD1.out");
        added.to = QStringLiteral("TIA1.in");
        added.wavelength = QStringLiteral("DC");
        net.push_back(added);

        QTemporaryFile ofdx;
        ofdx.setFileTemplate(QDir::tempPath() + "/ofdx_net_XXXXXX.ofdx");
        if (ofdx.open()) {
            check(OfdxIO::save(ofdx.fileName(), p1), "phnet ofdx save");
            Project p2;
            check(OfdxIO::load(ofdx.fileName(), p2), "phnet ofdx load");
            const QVector<PhotonicNetRow> &r = p2.photonicNetlist();
            check(r.size() == 5, "phnet: five connections round-trip");
            if (r.size() == 5) {
                check(r[0].from == "LASER1.out" && r[0].to == "MMI1.in" &&
                      r[0].wavelength == "1530~1570nm",
                      "phnet: first connection round-trips");
                check(!r[1].enabled, "phnet: disabled flag round-trips");
                check(r[4].from == "PD1.out" && r[4].to == "TIA1.in" &&
                      r[4].wavelength == "DC",
                      "phnet: appended connection round-trips");
            }
            QFile jf(ofdx.fileName());
            check(jf.open(QIODevice::ReadOnly), "phnet ofdx reopen");
            const QJsonObject root =
                QJsonDocument::fromJson(jf.readAll()).object();
            check(root.value("schematic").toObject()
                      .value("netlist").toArray().size() == 5,
                  "phnet: schematic.netlist json array length");
        }
    }

    // 3) 旧 .ofdx (schematic キー無し): 既定 5 行のまま (b)
    {
        QTemporaryFile old;
        old.setFileTemplate(QDir::tempPath() + "/ofdx_net_old_XXXXXX.ofdx");
        if (old.open()) {
            const QByteArray legacy =
                "{ \"schemaVersion\": \"1.0\", \"domain\": \"optics\","
                "  \"optical\": { \"solver\": 0 } }";
            old.write(legacy);
            old.flush();
            Project p;
            check(OfdxIO::load(old.fileName(), p), "phnet legacy ofdx load");
            check(p.photonicNetlist().size() == 5 &&
                  p.photonicNetlist()[2].to == "RING1.in",
                  "phnet: legacy file keeps the default netlist");
        }
        Project p;
        p.photonicNetlist().clear();
        p.clear();
        check(p.photonicNetlist().size() == 5,
              "phnet: clear() restores the default netlist");
    }
}

// ── モニター定義 (.ofdx "monitors") ─────────────────────────────────────────
static void testMonitorList()
{
    g_file = "monitors";

    // 1) 既定はドメイン別の初期行 — .ofd も .ofdx も従来の出力のまま
    {
        Project p;
        check(p.monitors().size() == 4, "mon: default EM list has four rows");
        check(p.monitors()[0].type == "point" &&
              p.monitors()[0].name == "E_probe" &&
              !p.monitors()[3].enabled,
              "mon: default rows are the documented initial values");
        check(defaultMonitors(Domain::Optical).size() == 5 &&
              defaultMonitors(Domain::Acoustic).size() == 4,
              "mon: per-domain defaults have the documented row counts");
        check(isDefaultMonitorSet(p.monitors()),
              "mon: an untouched list is recognised as a default set");
        const QString base = OfdIO::serialize(p);
        MonitorRow extra;
        extra.type = QStringLiteral("flux");
        extra.name = QStringLiteral("P_out");
        p.monitors().push_back(extra);
        check(!isDefaultMonitorSet(p.monitors()),
              "mon: an edited list is no longer a default set");
        check(OfdIO::serialize(p) == base,
              "mon: monitors keep .ofd output byte-identical");

        QTemporaryFile def;
        def.setFileTemplate(QDir::tempPath() + "/ofdx_mon_def_XXXXXX.ofdx");
        if (def.open()) {
            Project q;   // 既定のまま
            check(OfdxIO::save(def.fileName(), q), "mon default ofdx save");
            QFile jf(def.fileName());
            check(jf.open(QIODevice::ReadOnly), "mon default ofdx reopen");
            const QJsonObject root =
                QJsonDocument::fromJson(jf.readAll()).object();
            check(!root.contains("monitors"),
                  "mon: no monitors key while the list is unedited");
        }
        // ドメインを変えても「そのドメインの既定」ならキーを書かない
        QTemporaryFile defOpt;
        defOpt.setFileTemplate(QDir::tempPath() + "/ofdx_mon_opt_XXXXXX.ofdx");
        if (defOpt.open()) {
            Project q;
            q.setActiveDomain(Domain::Optical);
            q.monitors() = defaultMonitors(Domain::Optical);
            check(OfdxIO::save(defOpt.fileName(), q), "mon optical ofdx save");
            QFile jf(defOpt.fileName());
            check(jf.open(QIODevice::ReadOnly), "mon optical ofdx reopen");
            const QJsonObject root =
                QJsonDocument::fromJson(jf.readAll()).object();
            check(!root.contains("monitors"),
                  "mon: no monitors key for the optical default list");
        }
    }

    // 2) .ofdx ラウンドトリップ (a)
    {
        Project p1;
        QVector<MonitorRow> &mons = p1.monitors();
        mons[0].name = QStringLiteral("probe_A");
        mons[0].region = QStringLiteral("(1, 2, 3)");
        mons[1].enabled = false;
        MonitorRow added;
        added.type = QStringLiteral("spara");
        added.name = QStringLiteral("S21");
        added.region = QStringLiteral("port1→port2");
        added.band = QStringLiteral("2~3 GHz");
        mons.push_back(added);

        QTemporaryFile ofdx;
        ofdx.setFileTemplate(QDir::tempPath() + "/ofdx_mon_XXXXXX.ofdx");
        if (ofdx.open()) {
            check(OfdxIO::save(ofdx.fileName(), p1), "mon ofdx save");
            Project p2;
            check(OfdxIO::load(ofdx.fileName(), p2), "mon ofdx load");
            const QVector<MonitorRow> &r = p2.monitors();
            check(r.size() == 5, "mon: five monitors round-trip");
            if (r.size() == 5) {
                check(r[0].name == "probe_A" && r[0].region == "(1, 2, 3)" &&
                      r[0].type == "point" && r[0].enabled,
                      "mon: first monitor fields round-trip");
                check(!r[1].enabled, "mon: disabled flag round-trips");
                check(r[4].type == "spara" && r[4].name == "S21" &&
                      r[4].band == "2~3 GHz",
                      "mon: appended monitor round-trips");
            }
            QFile jf(ofdx.fileName());
            check(jf.open(QIODevice::ReadOnly), "mon ofdx reopen");
            const QJsonObject root =
                QJsonDocument::fromJson(jf.readAll()).object();
            check(root.value("monitors").toArray().size() == 5,
                  "mon: monitors json array length");
            check(root.contains("optical") && root.contains("acoustic"),
                  "mon: existing ofdx sections untouched");
        }
    }

    // 3) 旧 .ofdx (monitors キー無し): 読み込んだドメインの既定行 (b)
    {
        QTemporaryFile old;
        old.setFileTemplate(QDir::tempPath() + "/ofdx_mon_old_XXXXXX.ofdx");
        if (old.open()) {
            const QByteArray legacy =
                "{ \"schemaVersion\": \"1.0\", \"domain\": \"optical\","
                "  \"optical\": { \"solver\": 0 } }";
            old.write(legacy);
            old.flush();
            Project p;
            check(OfdxIO::load(old.fileName(), p), "mon legacy ofdx load");
            check(p.monitors() == defaultMonitors(p.activeDomain()),
                  "mon: legacy file falls back to the domain default list");
            check(p.monitors().size() >= 4 && p.monitors()[0].name == "T_drop",
                  "mon: legacy optical file gets the optical defaults");
        }
    }

    // 4) サイドカーの無い .ofd を開いたら前のプロジェクトの一覧を持ち越さない
    {
        Project p;
        p.monitors()[0].name = QStringLiteral("carried_over");
        MonitorRow extra;
        extra.type = QStringLiteral("flux");
        p.monitors().push_back(extra);
        p.analysisGroups()[0].name = QStringLiteral("carried_over");
        QString err;
        const QString text = OfdIO::serialize(Project());   // 素の .ofd
        check(OfdIO::parse(text, p, &err), "mon: plain .ofd parse");
        check(p.monitors() == defaultMonitors(p.activeDomain()),
              "mon: a sidecar-less .ofd resets the list to the domain default");
        check(p.analysisGroups() == defaultAnalysisGroups(p.activeDomain()),
              "aggrp: a sidecar-less .ofd resets the groups to the default");
    }
}

// ── 解析グループ (.ofdx "analysis_groups") ──────────────────────────────────
static void testAnalysisGroups()
{
    g_file = "analysis-groups";

    // 1) 既定はドメイン別の初期行 — 出力は従来のまま
    {
        Project p;
        check(p.analysisGroups().size() == 4,
              "aggrp: default EM list has four groups");
        check(p.analysisGroups()[0].name == "Antenna patterns" &&
              p.analysisGroups()[0].enabled &&
              !p.analysisGroups()[2].enabled,
              "aggrp: default rows are the documented initial values");
        check(defaultAnalysisGroups(Domain::Optical).size() == 6 &&
              defaultAnalysisGroups(Domain::Underwater).size() == 3,
              "aggrp: per-domain defaults have the documented row counts");
        const QString base = OfdIO::serialize(p);
        AnalysisGroupRow extra;
        extra.name = QStringLiteral("my_analysis");
        p.analysisGroups().push_back(extra);
        check(OfdIO::serialize(p) == base,
              "aggrp: groups keep .ofd output byte-identical");

        QTemporaryFile def;
        def.setFileTemplate(QDir::tempPath() + "/ofdx_agrp_def_XXXXXX.ofdx");
        if (def.open()) {
            Project q;
            check(OfdxIO::save(def.fileName(), q), "aggrp default ofdx save");
            QFile jf(def.fileName());
            check(jf.open(QIODevice::ReadOnly), "aggrp default ofdx reopen");
            const QJsonObject root =
                QJsonDocument::fromJson(jf.readAll()).object();
            check(!root.contains("analysis_groups"),
                  "aggrp: no analysis_groups key while the table is unedited");
        }
    }

    // 2) .ofdx ラウンドトリップ (a)
    {
        Project p1;
        QVector<AnalysisGroupRow> &grps = p1.analysisGroups();
        grps[0].name = QStringLiteral("patterns_v2");
        grps[0].output = QStringLiteral("gain, efficiency");
        grps[1].enabled = false;
        AnalysisGroupRow added;
        added.enabled = true;
        added.name = QStringLiteral("my_analysis");
        added.monitors = QStringLiteral("E_probe (point), far_field (ntff)");
        added.output = QStringLiteral("—");
        grps.push_back(added);

        QTemporaryFile ofdx;
        ofdx.setFileTemplate(QDir::tempPath() + "/ofdx_agrp_XXXXXX.ofdx");
        if (ofdx.open()) {
            check(OfdxIO::save(ofdx.fileName(), p1), "aggrp ofdx save");
            Project p2;
            check(OfdxIO::load(ofdx.fileName(), p2), "aggrp ofdx load");
            const QVector<AnalysisGroupRow> &r = p2.analysisGroups();
            check(r.size() == 5, "aggrp: five groups round-trip");
            if (r.size() == 5) {
                check(r[0].name == "patterns_v2" &&
                      r[0].output == "gain, efficiency" && r[0].enabled,
                      "aggrp: first group fields round-trip");
                check(!r[1].enabled, "aggrp: disabled flag round-trips");
                check(r[4].name == "my_analysis" &&
                      r[4].monitors == "E_probe (point), far_field (ntff)",
                      "aggrp: appended group round-trips");
            }
            QFile jf(ofdx.fileName());
            check(jf.open(QIODevice::ReadOnly), "aggrp ofdx reopen");
            const QJsonObject root =
                QJsonDocument::fromJson(jf.readAll()).object();
            check(root.value("analysis_groups").toArray().size() == 5,
                  "aggrp: analysis_groups json array length");
        }
    }

    // 3) 旧 .ofdx (analysis_groups キー無し): ドメインの既定行 (b)
    {
        QTemporaryFile old;
        old.setFileTemplate(QDir::tempPath() + "/ofdx_agrp_old_XXXXXX.ofdx");
        if (old.open()) {
            const QByteArray legacy =
                "{ \"schemaVersion\": \"1.0\", \"domain\": \"acoustic\","
                "  \"optical\": { \"solver\": 0 } }";
            old.write(legacy);
            old.flush();
            Project p;
            check(OfdxIO::load(old.fileName(), p), "aggrp legacy ofdx load");
            check(p.analysisGroups() == defaultAnalysisGroups(p.activeDomain()),
                  "aggrp: legacy file falls back to the domain default groups");
            check(p.analysisGroups().size() == 4 &&
                  p.analysisGroups()[0].name == "RT60 calculator",
                  "aggrp: legacy acoustic file gets the acoustic defaults");
        }
    }
}

// ── メッシュ細分化領域 (.ofdx "geometry.refine_regions") ────────────────────
static void testRefineRegions()
{
    g_file = "refine-regions";

    auto makeRegion = [](const char *name, double lo, double hi, double ratio) {
        RefineRegion r;
        r.name = QString::fromUtf8(name);
        for (int a = 0; a < 3; ++a) { r.min_m[a] = lo; r.max_m[a] = hi; }
        r.ratio = ratio;
        return r;
    };

    // 1) 既定は空 — .ofd も .ofdx も従来の出力のまま (追加キーを書かない)
    {
        Project p;
        check(p.refineRegions().isEmpty(), "refregion: default list is empty");
        const QString base = OfdIO::serialize(p);
        p.refineRegions().push_back(makeRegion("patch", -0.005, 0.005, 3.0));
        check(OfdIO::serialize(p) == base,
              "refregion: regions keep .ofd output byte-identical");

        QTemporaryFile empty;
        empty.setFileTemplate(QDir::tempPath() + "/ofdx_reg_empty_XXXXXX.ofdx");
        if (empty.open()) {
            Project q;   // 既定 (領域なし)
            check(OfdxIO::save(empty.fileName(), q), "refregion empty ofdx save");
            QFile jf(empty.fileName());
            check(jf.open(QIODevice::ReadOnly), "refregion empty ofdx reopen");
            const QJsonObject root =
                QJsonDocument::fromJson(jf.readAll()).object();
            check(!root.contains("geometry"),
                  "refregion: no geometry key when no region is defined");
        }
    }

    // 2) .ofdx ラウンドトリップ (a)
    {
        Project p1;
        RefineRegion a = makeRegion("patch_edge", -0.005, 0.005, 3.0);
        a.max_m[2] = 0.001;
        RefineRegion b = makeRegion("far_region", -0.03, 0.03, 0.5);
        b.enabled = false;
        p1.refineRegions() = { a, b };

        QTemporaryFile ofdx;
        ofdx.setFileTemplate(QDir::tempPath() + "/ofdx_reg_XXXXXX.ofdx");
        if (ofdx.open()) {
            check(OfdxIO::save(ofdx.fileName(), p1), "refregion ofdx save");
            Project p2;
            check(OfdxIO::load(ofdx.fileName(), p2), "refregion ofdx load");
            const QVector<RefineRegion> &r = p2.refineRegions();
            check(r.size() == 2, "refregion: two regions round-trip");
            if (r.size() == 2) {
                check(r[0].enabled && r[0].name == "patch_edge" &&
                      nearlyEq(r[0].min_m[0], -0.005) &&
                      nearlyEq(r[0].max_m[0], 0.005) &&
                      nearlyEq(r[0].max_m[2], 0.001) &&
                      nearlyEq(r[0].ratio, 3.0),
                      "refregion: first region fields round-trip");
                check(!r[1].enabled && r[1].name == "far_region" &&
                      nearlyEq(r[1].min_m[1], -0.03) &&
                      nearlyEq(r[1].ratio, 0.5),
                      "refregion: second region fields round-trip");
            }
            QFile jf(ofdx.fileName());
            check(jf.open(QIODevice::ReadOnly), "refregion ofdx reopen");
            const QJsonObject root =
                QJsonDocument::fromJson(jf.readAll()).object();
            const QJsonObject geo = root.value("geometry").toObject();
            check(geo.contains("refine_regions"),
                  "refregion: geometry.refine_regions key written");
            check(geo.value("refine_regions").toArray().size() == 2,
                  "refregion: json array length");
            // 既存セクションは従来どおり残っている (追加のみ)
            check(root.contains("optical") && root.contains("acoustic") &&
                  root.contains("underwater") && root.contains("tidy3d"),
                  "refregion: existing ofdx sections untouched");
        }
    }

    // 3) 旧 .ofdx (geometry キー無し): 既定値 = 領域なし (b)
    {
        QTemporaryFile old;
        old.setFileTemplate(QDir::tempPath() + "/ofdx_reg_old_XXXXXX.ofdx");
        if (old.open()) {
            const QByteArray legacy =
                "{ \"schemaVersion\": \"1.0\", \"domain\": \"em\","
                "  \"optical\": { \"solver\": 0 } }";
            old.write(legacy);
            old.flush();
            Project p;
            p.refineRegions().push_back(makeRegion("stale", -1, 1, 2));
            check(OfdxIO::load(old.fileName(), p), "refregion legacy ofdx load");
            check(p.refineRegions().size() == 1,
                  "refregion: legacy file leaves the list untouched (no key)");
        }
        // clear() したプロジェクトでは空に戻る
        Project p;
        p.refineRegions().push_back(makeRegion("stale", -1, 1, 2));
        p.clear();
        check(p.refineRegions().isEmpty(), "refregion: clear() empties the list");
    }
}

// ── CIE 表色系の測色計算 (optics/Colorimetry) ───────────────────────────────
// 既知の基準値と突き合わせる:
//   - ȳ(555 nm) ≈ 1 (V(λ) の定義)
//   - 等エネルギー白 E の色度 (1/3, 1/3)
//   - 黒体 2856 K = 標準イルミナント A (x=0.4476, y=0.4074)
//   - 黒体の CCT は自分自身に戻り Duv = 0
//   - 単色 555 nm の発光効率 ≈ 683 lm/W、CCT は定義されない
static void testColorimetry()
{
    g_file = "colorimetry";
    namespace cm = ofd::colorimetry;

    // 1) 等色関数: ȳ のピークは 555 nm 付近で値 ≈ 1
    check(std::fabs(cm::cieYbar(555.0) - 1.0) < 0.02,
          "colorimetry: ybar(555) ~ 1");
    check(cm::cieYbar(555.0) > cm::cieYbar(500.0) &&
          cm::cieYbar(555.0) > cm::cieYbar(650.0),
          "colorimetry: ybar peaks near 555 nm");
    check(cm::cieYbar(300.0) < 1e-3 && cm::cieYbar(900.0) < 1e-3,
          "colorimetry: ybar vanishes outside the visible range");

    // 2) 等エネルギー白 E → x = y = 1/3
    {
        const cm::Chromaticity c =
            cm::chromaticity(cm::integrate([](double) { return 1.0; }));
        check(c.valid, "colorimetry: equal-energy white is valid");
        check(std::fabs(c.x - 1.0 / 3.0) < 0.01 &&
              std::fabs(c.y - 1.0 / 3.0) < 0.01,
              "colorimetry: equal-energy white sits at (1/3, 1/3)");
        // u' = u1960, v' = 1.5 v1960 (CIE 1976 の定義)
        check(std::fabs(c.up - c.u1960) < 1e-12 &&
              std::fabs(c.vp - 1.5 * c.v1960) < 1e-12,
              "colorimetry: u'v' follows the 1960 UCS by definition");
    }

    // 3) 黒体 2856 K = 標準イルミナント A の色度
    {
        const cm::Chromaticity a = cm::chromaticity(
            cm::integrate([](double l) { return cm::planckSpectrum(l, 2856.0); }));
        check(std::fabs(a.x - 0.4476) < 0.005 && std::fabs(a.y - 0.4074) < 0.005,
              "colorimetry: 2856 K blackbody matches illuminant A");
    }

    // 4) 黒体の CCT は温度そのものに戻り、Duv ≈ 0
    for (double T : { 2700.0, 5000.0, 6504.0 }) {
        const cm::Chromaticity c = cm::chromaticity(
            cm::integrate([T](double l) { return cm::planckSpectrum(l, T); }));
        const cm::CctResult k = cm::correlatedColorTemperature(c);
        check(k.valid, "colorimetry: blackbody has a defined CCT");
        check(std::fabs(k.cct_K - T) < 0.01 * T,
              "colorimetry: CCT of a blackbody returns its temperature");
        check(std::fabs(k.duv) < 1e-3, "colorimetry: blackbody Duv ~ 0");
    }

    // 5) 単色 555 nm: K ≈ 683 lm/W、黒体軌跡から遠いので CCT は未定義
    {
        const std::vector<cm::GaussLobe> mono = { { 555.0, 1.0, 1.0 } };
        auto spd = [&mono](double l) { return cm::lobeSpectrum(mono, l); };
        check(std::fabs(cm::luminousEfficacyOfRadiation(spd) - 683.0) < 10.0,
              "colorimetry: 555 nm line has K ~ 683 lm/W");
        check(std::fabs(cm::peakWavelength(spd) - 555.0) < 1.0,
              "colorimetry: peak wavelength of a 555 nm line");
        const cm::CctResult k = cm::correlatedColorTemperature(
            cm::chromaticity(cm::integrate(spd)));
        check(!k.valid, "colorimetry: monochromatic light has no defined CCT");
    }

    // 6) 光源モデル (IlluminationOpts) の既定値は黒体軌跡上の白色
    {
        IlluminationOpts o;                       // 既定 = 白色 LED
        const optics::SourceColor c = optics::evaluateSource(o);
        check(c.valid, "colorimetry: default white LED evaluates");
        check(c.cct.valid && std::fabs(c.cct.cct_K - o.cctTarget_K) <= o.cctTol_K,
              "colorimetry: default white LED meets its CCT target");
        check(std::fabs(c.cct.duv) <= o.duvTol,
              "colorimetry: default white LED sits on the Planckian locus");
        check(c.efficacy_lm_W > 0.0 && c.efficacy_lm_W < 683.0,
              "colorimetry: efficacy of radiation is within (0, 683]");

        IlluminationOpts rgb = o;  rgb.spectrum = 1;
        const optics::SourceColor r = optics::evaluateSource(rgb);
        check(r.cct.valid && std::fabs(r.cct.cct_K - 5000.0) < 150.0,
              "colorimetry: default RGB mix lands on the 5000 K locus");

        IlluminationOpts bb = o;  bb.spectrum = 2;  bb.blackbody_K = 3000.0;
        const optics::SourceColor b = optics::evaluateSource(bb);
        check(b.cct.valid && std::fabs(b.cct.cct_K - 3000.0) < 30.0,
              "colorimetry: blackbody model returns its own temperature");

        IlluminationOpts mono = o;  mono.spectrum = 3;  mono.monoPeak_nm = 470.0;
        const optics::SourceColor m = optics::evaluateSource(mono);
        check(m.valid && std::fabs(m.peak_nm - 470.0) < 1.5,
              "colorimetry: monochromatic model peaks where asked");
    }
}

// ── ディスプレイ / AR-VR 光学の解析式 (optics/DisplayMetrics) ───────────────
static void testDisplayMetrics()
{
    g_file = "display-metrics";
    namespace dm = ofd::displayoptics;

    // 1) 全反射臨界角: n = 1.5 → 41.81°、n <= 1 は 90°
    check(std::fabs(dm::criticalAngle_deg(1.5) - 41.8103) < 1e-3,
          "dispmetrics: critical angle of n = 1.5");
    check(std::fabs(dm::criticalAngle_deg(1.0) - 90.0) < 1e-9,
          "dispmetrics: critical angle is 90 deg for n <= 1");

    // 2) 導波路 FOV: 手計算 sinθ = n·sinθg − λ/Λ と一致すること
    {
        const double n = 1.8, lam = 550.0, per = 385.0, gmax = 80.0;
        const dm::WaveguideFov f = dm::waveguideFov(per, lam, n, gmax);
        check(f.valid, "dispmetrics: SRG waveguide has a guided band");
        const double lo = std::asin(1.0 - lam / per) * 180.0 / M_PI;
        const double hi = std::asin(n * std::sin(gmax * M_PI / 180.0) - lam / per)
                          * 180.0 / M_PI;
        check(std::fabs(f.fovMin_deg - lo) < 1e-6 &&
              std::fabs(f.fovMax_deg - hi) < 1e-6,
              "dispmetrics: FOV limits follow the grating equation");
        check(std::fabs(f.fov_deg - (hi - lo)) < 1e-9,
              "dispmetrics: FOV span is the difference of the limits");
        // サイン空間での帯域幅 sinθmax − sinθmin = n·sinθg,max − 1 は
        // 周期に依らない (格子ベクトル λ/Λ は帯域を平行移動するだけ)
        const double sinSpan = n * std::sin(gmax * M_PI / 180.0) - 1.0;
        for (double period : { 385.0, 500.0, 600.0 }) {
            const dm::WaveguideFov g = dm::waveguideFov(period, lam, n, gmax);
            check(g.valid, "dispmetrics: guided band exists for this period");
            const double span = std::sin(g.fovMax_deg * M_PI / 180.0)
                              - std::sin(g.fovMin_deg * M_PI / 180.0);
            check(std::fabs(span - sinSpan) < 1e-9,
                  "dispmetrics: sine-space band width is period-independent");
        }
        // 周期を広げると帯域そのものが大きい角度側へ動く
        check(dm::waveguideFov(600.0, lam, n, gmax).fovMin_deg > f.fovMin_deg,
              "dispmetrics: a coarser grating shifts the band to larger angles");
        // 帯域が存在しない設定は valid=false (推定値を出さない)
        check(!dm::waveguideFov(200.0, lam, n, gmax).valid,
              "dispmetrics: no band reported when |sin| > 1");
        check(!dm::waveguideFov(0.0, lam, n, gmax).valid,
              "dispmetrics: zero period is rejected");
    }

    // 3) アイボックス: W = L − 2·ER·tan(FOV/2)、負にならない
    {
        const double w = dm::eyeboxWidth_mm(30.0, 18.0, 45.0);
        const double ref = 30.0 - 2.0 * 18.0 * std::tan(22.5 * M_PI / 180.0);
        check(std::fabs(w - ref) < 1e-9, "dispmetrics: eyebox formula");
        check(dm::eyeboxWidth_mm(5.0, 50.0, 60.0) == 0.0,
              "dispmetrics: eyebox is clamped at zero");
    }

    // 4) シースルー透過率: n=1 で 1、n=1.8 で 84.9%
    check(std::fabs(dm::slabTransmittance(1.0) - 1.0) < 1e-12,
          "dispmetrics: index-matched slab transmits everything");
    check(std::fabs(dm::slabTransmittance(1.8) - 0.8491) < 1e-3,
          "dispmetrics: uncoated n = 1.8 slab transmits ~84.9%");

    // 5) 射出円錐 / OLED 取り出し効率の古典近似
    {
        const double n = 1.75;
        const double thc = std::asin(1.0 / n);
        check(std::fabs(dm::escapeConeFraction(n) - 0.5 * (1.0 - std::cos(thc)))
                  < 1e-12,
              "dispmetrics: escape-cone fraction (1-cos)/2");
        check(std::fabs(dm::oledOutcoupling(n) - 1.0 / (2.0 * n * n)) < 1e-12,
              "dispmetrics: OLED outcoupling 1/(2n^2)");
        // 屈折率が上がると取り出し効率は下がる (単調性)
        check(dm::oledOutcoupling(2.0) < dm::oledOutcoupling(1.5),
              "dispmetrics: higher index lowers the outcoupling");
        check(dm::ledExtractionCube(2.45) > dm::ledExtractionTopFace(2.45),
              "dispmetrics: the 6-face bound exceeds the top-face value");
    }

    // 6) 側壁再結合: 4Sτ/L のスケーリング (L→∞ で低下なし)
    {
        // S = 1e4 cm/s, τ = 10 ns → Sτ = 1 μm, L = 5 μm → η = η0/1.8
        const double e = dm::sidewallDeratedIqe(0.8, 5.0, 1.0e4, 10.0);
        check(std::fabs(e - 0.8 / 1.8) < 1e-9,
              "dispmetrics: sidewall derating matches 1/(1+4S tau/L)");
        check(dm::sidewallDeratedIqe(0.8, 500.0, 1.0e4, 10.0) >
              dm::sidewallDeratedIqe(0.8, 5.0, 1.0e4, 10.0),
              "dispmetrics: bigger chips lose less to the sidewalls");
        check(std::fabs(dm::sidewallDeratedIqe(0.8, 5.0, 0.0, 10.0) - 0.8) < 1e-12,
              "dispmetrics: no surface recombination leaves IQE untouched");
    }

    // 7) 環境光コントラスト: 暗室 (E=0) では暗室 CR に一致、明るいほど低下
    {
        const dm::AmbientContrast dark = dm::ambientContrast(500, 5000, 0, 0.045);
        check(dark.valid && std::fabs(dark.contrast - 5000.0) < 1e-6,
              "dispmetrics: zero ambient reproduces the darkroom CR");
        const dm::AmbientContrast lit = dm::ambientContrast(500, 5000, 200, 0.045);
        const double lamb = 0.045 * 200.0 / M_PI;
        check(std::fabs(lit.ambientLuminance_cdm2 - lamb) < 1e-9,
              "dispmetrics: ambient screen luminance R*E/pi");
        check(std::fabs(lit.contrast - (500.0 + lamb) / (0.1 + lamb)) < 1e-6,
              "dispmetrics: ambient contrast definition");
        check(lit.contrast < dark.contrast,
              "dispmetrics: ambient light lowers the contrast");
        check(!dm::ambientContrast(0.0, 5000, 200, 0.045).valid,
              "dispmetrics: invalid luminance is rejected, not guessed");
    }
}

// ── 近軸光線追跡 (optics/ParaxialTrace) ─────────────────────────────────────
static void testParaxialTrace()
{
    g_file = "paraxial";
    namespace px = ofd::paraxial;

    // 1) 薄い両凸レンズ (R = ±50, n = 1.5, 厚み 0):
    //    レンズメーカーの式 1/f = (n-1)(1/R1 - 1/R2) → f = 50 mm
    {
        std::vector<px::Surface> s(2);
        s[0].R = 50.0;  s[0].thickness = 0.0;  s[0].nAfter = 1.5;
        s[1].R = -50.0; s[1].thickness = 50.0; s[1].nAfter = 1.0;
        const px::SystemData d = px::analyze(s, 50.0, 12.5, 10.0);
        check(d.valid, "paraxial: thin biconvex solves");
        check(std::fabs(d.efl - 50.0) < 1e-9, "paraxial: EFL = 50 mm");
        check(std::fabs(d.bfl - 50.0) < 1e-9,
              "paraxial: thin lens BFL equals EFL");
        check(std::fabs(d.backPrincipal) < 1e-9,
              "paraxial: thin lens rear principal plane is at the vertex");
        check(std::fabs(d.fnumber - 4.0) < 1e-9, "paraxial: F/# = f'/EPD");
        check(std::fabs(d.imageHeight - 50.0 * std::tan(10.0 * M_PI / 180.0))
                  < 1e-9,
              "paraxial: paraxial image height f'*tan(theta)");
        check(d.hasImagePlane && std::fabs(d.defocus) < 1e-9,
              "paraxial: image plane at the paraxial focus gives zero defocus");
    }

    // 2) 単一屈折面 (平凸, R = 50, n = 1.5): f' = n'R/(n'-n) = 150 mm、
    //    BFL は像側主点が頂点にあるので f' に等しい
    {
        std::vector<px::Surface> s(1);
        s[0].R = 50.0; s[0].thickness = 0.0; s[0].nAfter = 1.5;
        const px::SystemData d = px::analyze(s, -1.0, 0.0, 0.0);
        check(d.valid, "paraxial: single surface solves");
        // 像空間が n=1.5 のときの後側焦点距離は y/u' で測る (BFL) — 幾何長
        check(std::fabs(d.bfl - 150.0) < 1e-6,
              "paraxial: single-surface back focal distance n'R/(n'-n)");
        check(!d.hasImagePlane, "paraxial: no image plane → no defocus");
    }

    // 3) 平板 (両面平面) はアフォーカル → valid=false (偽の焦点距離を出さない)
    {
        std::vector<px::Surface> s(2);
        s[0].R = 0.0; s[0].thickness = 5.0; s[0].nAfter = 1.5;
        s[1].R = 0.0; s[1].thickness = 0.0; s[1].nAfter = 1.0;
        check(!px::analyze(s, -1.0, 10.0, 0.0).valid,
              "paraxial: a plane-parallel plate is afocal");
        check(!px::analyze({}, -1.0, 10.0, 0.0).valid,
              "paraxial: an empty system is not solvable");
    }

    // 4) 2 枚の薄レンズ (f1 = f2 = 100, 間隔 d = 50):
    //    1/f = 1/f1 + 1/f2 − d/(f1 f2) → f = 66.667 mm
    //    BFL = f(1 − d/f1) = 33.333 mm
    {
        auto thin = [](double f, double t) {
            // 屈折率 1.5 の薄肉レンズ 2 面 (R1 = -R2) で焦点距離 f を作る
            std::vector<px::Surface> two(2);
            const double R = 2.0 * 0.5 * f;      // (n-1)*2/R = 1/f → R = 2(n-1)f
            two[0].R = R;  two[0].thickness = 0.0; two[0].nAfter = 1.5;
            two[1].R = -R; two[1].thickness = t;   two[1].nAfter = 1.0;
            return two;
        };
        std::vector<px::Surface> s = thin(100.0, 50.0);
        const std::vector<px::Surface> b = thin(100.0, 0.0);
        s.insert(s.end(), b.begin(), b.end());
        const px::SystemData d = px::analyze(s, -1.0, 20.0, 0.0);
        check(d.valid, "paraxial: two-lens system solves");
        check(std::fabs(d.efl - 200.0 / 3.0) < 1e-6,
              "paraxial: combined focal length of two thin lenses");
        check(std::fabs(d.bfl - 100.0 / 3.0) < 1e-6,
              "paraxial: back focal length of two thin lenses");
        // 対称系なので前側焦点距離は後側と一致する
        check(std::fabs(d.ffl - d.bfl) < 1e-6,
              "paraxial: symmetric doublet has FFL = BFL");
        check(std::fabs(d.totalTrack - 50.0) < 1e-9,
              "paraxial: total track equals the air gap");
    }
}

// ── .ofdx "display_optics" / "illumination" (追加キー) ──────────────────────
static void testDisplayIlluminationSettings()
{
    g_file = "display-illumination";

    auto setNonDefaults = [](Project &p) {
        DisplayOpticsOpts &d = p.displayOptics();
        d.device = 2;
        d.wgType = 3;
        d.subThick_mm = 1.25;
        d.subIndex = 2.05;
        d.gratPeriod_nm = 420.0;
        d.gratDepth_nm = 180.0;
        d.gratSlant_deg = 45.0;
        d.threeGratings = false;
        d.rcwaOptimize = true;
        d.designLambda_nm = 520.0;
        d.guideMaxAngle_deg = 75.0;
        d.outcouplerLen_mm = 24.0;
        d.eyeRelief_mm = 15.0;
        d.fovTarget_deg = 55.0;
        d.eyeboxTarget_mm = 12.0;
        d.seeThroughTarget_pct = 90.0;
        d.bottomEmission = true;
        d.topEmission = false;
        d.microcavity = false;
        d.sepIqe = false;
        d.sepSpp = false;
        d.sepWaveguide = false;
        d.outcouplingStruct = 3;
        d.oledIndex = 1.62;
        d.oledIqe = 0.75;
        d.chipSize_um = 3.5;
        d.sidewallRecomb = false;
        d.sidewallDbr = false;
        d.directional = true;
        d.mlIndex = 2.6;
        d.mlIqe = 0.65;
        d.mlSurfVel_cm_s = 5.0e3;
        d.mlLifetime_ns = 4.5;
        d.lcdMode = 1;
        d.lcAnisotropy = false;
        d.compFilm = false;
        d.lcdPeakLum_cdm2 = 700.0;
        d.lcdDarkroomCr = 1500.0;
        d.lcdAmbient_lx = 350.0;
        d.lcdReflectance = 0.02;

        IlluminationOpts &i = p.illumination();
        i.app = 2;
        i.srcModel = 0;
        i.rayFile = QStringLiteral("my_source.ray");
        i.spectrum = 3;
        i.flux_lm = 2500.0;
        i.rays = 1.0e7;
        i.reflector = false;
        i.tirLens = true;
        i.diffuser = false;
        i.lightGuide = true;
        i.phosphor = false;
        i.surface = 3;
        i.bluePeak_nm = 455.0;
        i.blueFwhm_nm = 25.0;
        i.phosPeak_nm = 585.0;
        i.phosFwhm_nm = 120.0;
        i.phosRatio = 0.8;
        i.rPeak_nm = 625.0; i.rFwhm_nm = 18.0; i.rRatio = 1.5;
        i.gPeak_nm = 530.0; i.gFwhm_nm = 30.0; i.gRatio = 1.2;
        i.bPeak_nm = 465.0; i.bFwhm_nm = 24.0; i.bRatio = 0.9;
        i.blackbody_K = 2700.0;
        i.monoPeak_nm = 632.8;
        i.monoFwhm_nm = 0.5;
        i.cctTarget_K = 4000.0;
        i.cctTol_K = 200.0;
        i.duvTol = 0.004;
    };

    // 1) これらの設定は .ofd (カーネル入力) を 1 バイトも変えない
    {
        Project p;
        const QString base = OfdIO::serialize(p);
        setNonDefaults(p);
        check(OfdIO::serialize(p) == base,
              "dispillum: settings keep .ofd output byte-identical");
    }

    // 2) 既定値のままなら .ofdx にキー自体を書かない (旧ファイルとバイト一致)
    {
        Project p;
        QTemporaryFile ofdx;
        ofdx.setFileTemplate(QDir::tempPath() + "/ofdx_dispillum_def_XXXXXX.ofdx");
        if (ofdx.open()) {
            check(OfdxIO::save(ofdx.fileName(), p), "dispillum default ofdx save");
            QFile jf(ofdx.fileName());
            check(jf.open(QIODevice::ReadOnly), "dispillum default ofdx reopen");
            const QJsonObject root =
                QJsonDocument::fromJson(jf.readAll()).object();
            check(!root.contains("display_optics"),
                  "dispillum: defaults write no display_optics key");
            check(!root.contains("illumination"),
                  "dispillum: defaults write no illumination key");
        }
    }

    // 3) .ofdx ラウンドトリップ (a: 新キーの往復)
    {
        Project p1;
        setNonDefaults(p1);
        QTemporaryFile ofdx;
        ofdx.setFileTemplate(QDir::tempPath() + "/ofdx_dispillum_XXXXXX.ofdx");
        if (ofdx.open()) {
            check(OfdxIO::save(ofdx.fileName(), p1), "dispillum ofdx save");
            Project p2;
            check(OfdxIO::load(ofdx.fileName(), p2), "dispillum ofdx load");
            const DisplayOpticsOpts &d = p2.displayOptics();
            check(d.device == 2 && d.wgType == 3,
                  "dispillum: device / waveguide type round-trip");
            check(nearlyEq(d.subThick_mm, 1.25) && nearlyEq(d.subIndex, 2.05) &&
                  nearlyEq(d.gratPeriod_nm, 420.0) &&
                  nearlyEq(d.gratDepth_nm, 180.0) &&
                  nearlyEq(d.gratSlant_deg, 45.0),
                  "dispillum: substrate / grating round-trip");
            check(!d.threeGratings && d.rcwaOptimize,
                  "dispillum: waveguide flags round-trip");
            check(nearlyEq(d.designLambda_nm, 520.0) &&
                  nearlyEq(d.guideMaxAngle_deg, 75.0) &&
                  nearlyEq(d.outcouplerLen_mm, 24.0) &&
                  nearlyEq(d.eyeRelief_mm, 15.0),
                  "dispillum: pupil-expansion geometry round-trip");
            check(nearlyEq(d.fovTarget_deg, 55.0) &&
                  nearlyEq(d.eyeboxTarget_mm, 12.0) &&
                  nearlyEq(d.seeThroughTarget_pct, 90.0),
                  "dispillum: design targets round-trip");
            check(d.bottomEmission && !d.topEmission && !d.microcavity &&
                  !d.sepIqe && !d.sepSpp && !d.sepWaveguide &&
                  d.outcouplingStruct == 3 &&
                  nearlyEq(d.oledIndex, 1.62) && nearlyEq(d.oledIqe, 0.75),
                  "dispillum: OLED settings round-trip");
            check(nearlyEq(d.chipSize_um, 3.5) && !d.sidewallRecomb &&
                  !d.sidewallDbr && d.directional &&
                  nearlyEq(d.mlIndex, 2.6) && nearlyEq(d.mlIqe, 0.65) &&
                  nearlyEq(d.mlSurfVel_cm_s, 5.0e3) &&
                  nearlyEq(d.mlLifetime_ns, 4.5),
                  "dispillum: microLED settings round-trip");
            check(d.lcdMode == 1 && !d.lcAnisotropy && !d.compFilm &&
                  nearlyEq(d.lcdPeakLum_cdm2, 700.0) &&
                  nearlyEq(d.lcdDarkroomCr, 1500.0) &&
                  nearlyEq(d.lcdAmbient_lx, 350.0) &&
                  nearlyEq(d.lcdReflectance, 0.02),
                  "dispillum: LCD settings round-trip");

            const IlluminationOpts &i = p2.illumination();
            check(i.app == 2 && i.srcModel == 0 && i.spectrum == 3 &&
                  i.rayFile == QStringLiteral("my_source.ray"),
                  "dispillum: illumination source selectors round-trip");
            check(nearlyEq(i.flux_lm, 2500.0) && nearlyEq(i.rays, 1.0e7),
                  "dispillum: flux / ray count round-trip");
            check(!i.reflector && i.tirLens && !i.diffuser && i.lightGuide &&
                  !i.phosphor && i.surface == 3,
                  "dispillum: illumination optics round-trip");
            check(nearlyEq(i.bluePeak_nm, 455.0) && nearlyEq(i.blueFwhm_nm, 25.0) &&
                  nearlyEq(i.phosPeak_nm, 585.0) &&
                  nearlyEq(i.phosFwhm_nm, 120.0) && nearlyEq(i.phosRatio, 0.8),
                  "dispillum: white-LED lobes round-trip");
            check(nearlyEq(i.rPeak_nm, 625.0) && nearlyEq(i.gFwhm_nm, 30.0) &&
                  nearlyEq(i.bRatio, 0.9),
                  "dispillum: RGB lobes round-trip");
            check(nearlyEq(i.blackbody_K, 2700.0) &&
                  nearlyEq(i.monoPeak_nm, 632.8) &&
                  nearlyEq(i.monoFwhm_nm, 0.5),
                  "dispillum: blackbody / mono parameters round-trip");
            check(nearlyEq(i.cctTarget_K, 4000.0) && nearlyEq(i.cctTol_K, 200.0) &&
                  nearlyEq(i.duvTol, 0.004),
                  "dispillum: colorimetric targets round-trip");

            // JSON 構造 (追加キーの位置)
            QFile jf(ofdx.fileName());
            check(jf.open(QIODevice::ReadOnly), "dispillum ofdx reopen");
            const QJsonObject root =
                QJsonDocument::fromJson(jf.readAll()).object();
            const QJsonObject dj = root.value("display_optics").toObject();
            check(dj.contains("waveguide") && dj.contains("targets") &&
                  dj.contains("oled") && dj.contains("microled") &&
                  dj.contains("lcd"),
                  "dispillum: display_optics json sections present");
            const QJsonObject ij = root.value("illumination").toObject();
            check(ij.contains("optics") && ij.contains("white_led") &&
                  ij.contains("rgb") && ij.contains("mono") &&
                  ij.contains("targets"),
                  "dispillum: illumination json sections present");
            // 既存の "optical" セクションは無傷
            check(root.contains("optical"),
                  "dispillum: existing optical section is untouched");
        }
    }

    // 4) 旧 .ofdx (新キー無し): 既定値のまま (旧ファイル互換, b)
    {
        QTemporaryFile old;
        old.setFileTemplate(QDir::tempPath() + "/ofdx_dispillum_old_XXXXXX.ofdx");
        if (old.open()) {
            const QByteArray legacy =
                "{ \"schemaVersion\": \"1.0\", \"domain\": \"optical\","
                "  \"optical\": { \"solver\": 0 } }";
            old.write(legacy);
            old.flush();
            Project p;
            check(OfdxIO::load(old.fileName(), p), "dispillum legacy ofdx load");
            const DisplayOpticsOpts &d = p.displayOptics();
            check(d.device == 0 && d.wgType == 0 && d.subThick_mm == 0.7 &&
                  d.subIndex == 1.80 && d.gratPeriod_nm == 385.0,
                  "dispillum: legacy file leaves display_optics defaults");
            check(d.designLambda_nm == 550.0 && d.guideMaxAngle_deg == 80.0 &&
                  d.outcouplerLen_mm == 30.0 && d.eyeRelief_mm == 18.0,
                  "dispillum: legacy file leaves pupil defaults");
            check(d.oledIndex == 1.75 && d.oledIqe == 0.90 &&
                  d.mlIndex == 2.45 && d.lcdDarkroomCr == 5000.0,
                  "dispillum: legacy file leaves device-model defaults");
            const IlluminationOpts &i = p.illumination();
            check(i.spectrum == 0 && i.flux_lm == 1200.0 &&
                  i.bluePeak_nm == 450.0 && i.phosPeak_nm == 570.0 &&
                  i.cctTarget_K == 5000.0,
                  "dispillum: legacy file leaves illumination defaults");
        }
    }

    // 5) 壊れたファイルの範囲外値はコンボ index の範囲へクランプされる
    {
        QTemporaryFile bad;
        bad.setFileTemplate(QDir::tempPath() + "/ofdx_dispillum_bad_XXXXXX.ofdx");
        if (bad.open()) {
            const QByteArray broken =
                "{ \"schemaVersion\": \"1.0\", \"domain\": \"optical\","
                "  \"display_optics\": { \"device\": 99,"
                "     \"waveguide\": { \"type\": -4 },"
                "     \"oled\": { \"structure\": 12 },"
                "     \"lcd\": { \"mode\": 9 } },"
                "  \"illumination\": { \"app\": -1, \"source_model\": 7,"
                "     \"spectrum\": 42, \"optics\": { \"surface\": 9 } } }";
            bad.write(broken);
            bad.flush();
            Project p;
            check(OfdxIO::load(bad.fileName(), p), "dispillum broken ofdx load");
            const DisplayOpticsOpts &d = p.displayOptics();
            check(d.device == 3 && d.wgType == 0 && d.outcouplingStruct == 3 &&
                  d.lcdMode == 2, "dispillum: broken display indices clamped");
            const IlluminationOpts &i = p.illumination();
            check(i.app == 0 && i.srcModel == 2 && i.spectrum == 3 &&
                  i.surface == 3, "dispillum: broken illumination indices clamped");
        }
    }

    // 6) clear() で既定値へ戻る
    {
        Project p;
        setNonDefaults(p);
        p.clear();
        check(p.displayOptics().device == 0 && p.displayOptics().subIndex == 1.80,
              "dispillum: clear() resets display_optics");
        check(p.illumination().spectrum == 0 && p.illumination().flux_lm == 1200.0,
              "dispillum: clear() resets illumination");
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // Resolve the sample directory: explicit arg first, then the bundled
    // tests/data (self-contained), then a sibling OpenFDTD checkout's data.
    QString dir;
    if (argc > 1) {
        dir = argv[1];
    } else {
        const QString base = QFileInfo(QString::fromLocal8Bit(argv[0])).path();
        for (const QString &cand : {
                 base + "/../tests/data",      // build/ alongside source
                 base + "/../../tests/data",   // out-of-source build tree
                 QStringLiteral("tests/data"), // run from repo root
                 base + "/../../data/sample" }) {  // sibling OpenFDTD checkout
            if (QDir(cand).exists()) { dir = cand; break; }
        }
    }

    const QStringList files =
        QDir(dir).entryList({ "*.ofd" }, QDir::Files, QDir::Name);
    if (files.isEmpty()) {
        std::fprintf(stderr, "no .ofd samples found under %s\n", qPrintable(dir));
        return 2;
    }

    int loaded = 0;
    for (const QString &name : files) {
        g_file = name;
        const QString path = QDir(dir).filePath(name);

        Project p1;
        QString err;
        if (!OfdIO::load(path, p1, &err)) {
            ++g_failures;
            std::fprintf(stderr, "FAIL %s: load: %s\n",
                         qPrintable(name), qPrintable(err));
            continue;
        }
        ++loaded;

        const QString text = OfdIO::serialize(p1);
        Project p2;
        if (!OfdIO::parse(text, p2, &err)) {
            ++g_failures;
            std::fprintf(stderr, "FAIL %s: reparse: %s\n",
                         qPrintable(name), qPrintable(err));
            continue;
        }
        compareProjects(p1, p2);
    }

    testVoxelizer();
    testGlassCatalog();
    testRoomAcoustics();
    testOperaAcousticSettings();
    testProjectTemplates();
    testAcousticBudgets();
    testKernelResultReader();
    testAudioEditEngine();
    testCalibrationOffsetGate();
    testOnnActivation();
    testRcwaCore();
    testOpticsMaterials();
    testThinFilmStack();
    testOpticalModeSettings();
    testBellhop();
    testH5Reader();
    testOfdIntegration(dir);
    testRunGating();
    testAcousticReport();
    testFdeModeSolver();
    testSoundInsulation();
    testRoomModes();
    testEnvironmentalNoise();
    testFdtdVerification();
    testToleranceStats();
    testFocusedField();
    testPlasmaDispersion();
    testSarMetrics();
    testSolverSelection();
    testRadioPropagation();
    testDispersionFit();
    testBendWaveguide();
    testMeshDiagnostics();
    testRefineRegions();
    testEmcStandards();
    testLumpedRlc();
    testCircuitPorts();
    testPhotonicNetlist();
    testMonitorList();
    testAnalysisGroups();
    testColorimetry();
    testDisplayMetrics();
    testParaxialTrace();
    testDisplayIlluminationSettings();

    std::printf("%d files loaded, %d checks, %d failures\n",
                loaded, g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
