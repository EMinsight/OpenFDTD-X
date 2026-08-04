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
#include <cmath>
#include <cstdio>

#include "audio/AudioEditEngine.h"
#include "core/Project.h"
#include "core/ProjectTemplates.h"
#include "io/ActivationCurve.h"
#include "io/BellhopIO.h"
#include "io/H5Reader.h"
#include "io/OfdIO.h"
#include "kernel/Runner.h"
#include "io/StlImporter.h"
#include "io/Voxelizer.h"
#include "core/GlassCatalog.h"
#include "core/RoomAcoustics.h"
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
        }
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
    check(total == 37, "37 templates registered");

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
    testAudioEditEngine();
    testCalibrationOffsetGate();
    testOnnActivation();
    testRcwaCore();
    testBellhop();
    testH5Reader();
    testOfdIntegration(dir);
    testRunGating();
    testAcousticReport();

    std::printf("%d files loaded, %d checks, %d failures\n",
                loaded, g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
