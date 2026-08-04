// ProjectTemplates.cpp — 各テンプレートのシナリオ設定。
// 単位は .ofd と同じくメートル / Hz。material の 0=空気, 1=PEC は組込みで、
// materials() の先頭要素が ID 2 に対応する (Geometry::materialId 参照)。
#include "ProjectTemplates.h"
#include "Project.h"

namespace ofd {
namespace templates {
namespace {

// ── 共通ヘルパー ────────────────────────────────────────────────────────────
void axis(Project &p, int a, double lo, double hi, int cells)
{
    p.mesh(a).nodes = { lo, hi };
    p.mesh(a).divs  = { cells };
}

// 通常媒質を追加して材質番号 (2 始まり) を返す
int addMat(Project &p, const char *name, double epsr, double esgm = 0.0,
           double rho = 1.225, double c = 343.0, double alpha = 0.02)
{
    Material m;
    m.type = 1;
    m.epsr = epsr;
    m.esgm = esgm;
    m.name = QString::fromUtf8(name);
    m.rho = rho;
    m.soundSpeed = c;
    m.absorption = alpha;
    p.materials().push_back(m);
    return 1 + p.materials().size();   // 0=空気, 1=PEC, ユーザーは 2 から
}

// 直方体 (shape 1)
void box(Project &p, int matId, const char *name,
         double x1, double x2, double y1, double y2, double z1, double z2)
{
    Geometry g;
    g.materialId = matId;
    g.shape = 1;
    g.g[0] = x1; g.g[1] = x2; g.g[2] = y1; g.g[3] = y2; g.g[4] = z1; g.g[5] = z2;
    g.name = QString::fromUtf8(name);
    p.geometries().push_back(g);
}

// 楕円体 (shape 2) / 円柱 (shape 11/12/13) — 外接直方体で指定
void shape6(Project &p, int shape, int matId, const char *name,
            double x1, double x2, double y1, double y2, double z1, double z2)
{
    Geometry g;
    g.materialId = matId;
    g.shape = shape;
    g.g[0] = x1; g.g[1] = x2; g.g[2] = y1; g.g[3] = y2; g.g[4] = z1; g.g[5] = z2;
    g.name = QString::fromUtf8(name);
    p.geometries().push_back(g);
}

void feedZ(Project &p, double x, double y, double z, double z0 = 50.0)
{
    Feed f;
    f.dir = 'Z';
    f.x = x; f.y = y; f.z = z;
    f.volt = 1.0;
    f.z0 = z0;
    p.feeds().push_back(f);
}

void probeZ(Project &p, double x, double y, double z)
{
    Probe pr;
    pr.dir = 'Z';
    pr.x = x; pr.y = y; pr.z = z;
    p.probes().push_back(pr);
}

void freq(Project &p, double f1min, double f1max, int f1div, double f2)
{
    GeneralOpts &g = p.general();
    g.f1min = f1min; g.f1max = f1max; g.f1div = f1div;
    g.f2min = f2;    g.f2max = f2;    g.f2div = 0;
    g.hasF1 = true;  g.hasF2 = true;
}

void usePml(Project &p, int layers = 8)
{
    p.general().abc = 1;
    p.general().pmlL = layers;
}

// 音響: 部屋 (直方体シューボックス) — メッシュ + 室情報 + 音源/受音点
void room(Project &p, double L, double W, double H, double cell,
          double srcX, double srcY, double srcZ,
          double micX, double micY, double micZ)
{
    axis(p, 0, 0.0, L, qMax(1, int(L / cell)));
    axis(p, 1, 0.0, W, qMax(1, int(W / cell)));
    axis(p, 2, 0.0, H, qMax(1, int(H / cell)));
    AcousticOpts &a = p.acoustic();
    a.roomL = L; a.roomW = W; a.roomH = H;
    a.volume = L * W * H;
    a.surface = 2.0 * (L * W + L * H + W * H);
    feedZ(p, srcX, srcY, srcZ);
    probeZ(p, micX, micY, micZ);
}

// 吸音バジェット行
AbsorptionRow absRow(int role, const char *name, double area,
                     std::initializer_list<double> a, double airA = 0)
{
    AbsorptionRow r;
    r.role = role;
    r.name = QString::fromUtf8(name);
    r.area = area;
    int i = 0;
    for (double v : a) { if (i < 6) r.alpha[i] = v; ++i; }
    r.airA = airA;
    return r;
}

// 水中: 距離×深度の伝搬領域 (y は薄い 2D 断面)
void ocean(Project &p, double range_m, double depth_m,
           int rangeCells, int depthCells)
{
    axis(p, 0, 0.0, range_m, rangeCells);
    axis(p, 1, -50.0, 50.0, 1);
    axis(p, 2, 0.0, depth_m, depthCells);
}

// ── EM テンプレート ─────────────────────────────────────────────────────────

// 2.45 GHz 半波長ダイポール (アーム長 ~29 mm)
void emDipole(Project &p)
{
    axis(p, 0, -0.06, 0.06, 30);
    axis(p, 1, -0.06, 0.06, 30);
    axis(p, 2, -0.09, 0.09, 45);
    usePml(p);
    box(p, 1, "ダイポール素子(+Z)", -0.002, 0.002, -0.002, 0.002, 0.002, 0.029);
    box(p, 1, "ダイポール素子(-Z)", -0.002, 0.002, -0.002, 0.002, -0.029, -0.002);
    feedZ(p, 0, 0, 0);
    freq(p, 2.0e9, 3.0e9, 20, 2.45e9);
    p.post().zin.enabled = true;
    Far1d f;
    f.dir = 'V'; f.div = 72; f.angle = 0;
    p.post().far1d.push_back(f);
}

void emAntenna(Project &p) { emDipole(p); }

void emEmc(Project &p)
{
    // スロット付きシールド筐体 (300×200×150 mm, 板厚 5 mm) 内の放射源
    axis(p, 0, -0.20, 0.20, 80);
    axis(p, 1, -0.15, 0.15, 60);
    axis(p, 2, -0.12, 0.12, 48);
    usePml(p);
    const double x = 0.15, y = 0.10, z = 0.075, t = 0.005;
    box(p, 1, "底板", -x, x, -y, y, -z, -z + t);
    box(p, 1, "側板-X", -x, -x + t, -y, y, -z, z);
    box(p, 1, "側板+X", x - t, x, -y, y, -z, z);
    box(p, 1, "側板-Y", -x, x, -y, -y + t, -z, z);
    box(p, 1, "側板+Y", -x, x, y - t, y, -z, z);
    // 天板はスロット (100×5 mm) を残して 2 分割
    box(p, 1, "天板(前)", -x, x, -y, -0.0025, z - t, z);
    box(p, 1, "天板(後)", -0.05, x, 0.0025, y, z - t, z);
    box(p, 1, "天板(左)", -x, -0.05, 0.0025, y, z - t, z);
    feedZ(p, 0, 0, 0);   // 筐体内の放射源 (ノイズ源モデル)
    freq(p, 100e6, 1.0e9, 19, 500e6);
    p.post().zin.enabled = true;
}

void emRcs(Project &p)
{
    // PEC 球 (半径 50 mm) の平面波散乱 — Mie 解と比較できる定番構成
    axis(p, 0, -0.1, 0.1, 40);
    axis(p, 1, -0.1, 0.1, 40);
    axis(p, 2, -0.1, 0.1, 40);
    usePml(p);
    shape6(p, 2, 1, "PEC球 r=50mm", -0.05, 0.05, -0.05, 0.05, -0.05, 0.05);
    p.planewave().enabled = true;
    p.planewave().theta = 90;
    p.planewave().phi = 0;
    p.planewave().pol = 1;
    freq(p, 1.0e9, 5.0e9, 8, 3.0e9);
    Far1d f;
    f.dir = 'V'; f.div = 180; f.angle = 0;
    p.post().far1d.push_back(f);
}

void emWaveguide(Project &p)
{
    // WR-90 矩形導波管 (22.86×10.16 mm, X バンド) のプローブ給電
    const double a = 0.02286, b = 0.01016, L = 0.10, t = 0.002;
    axis(p, 0, -0.01, L + 0.01, 55);
    axis(p, 1, -a / 2 - t, a / 2 + t, 27);
    axis(p, 2, -b / 2 - t, b / 2 + t, 14);
    usePml(p);
    box(p, 1, "広壁-Z", 0, L, -a / 2 - t, a / 2 + t, -b / 2 - t, -b / 2);
    box(p, 1, "広壁+Z", 0, L, -a / 2 - t, a / 2 + t, b / 2, b / 2 + t);
    box(p, 1, "狭壁-Y", 0, L, -a / 2 - t, -a / 2, -b / 2, b / 2);
    box(p, 1, "狭壁+Y", 0, L, a / 2, a / 2 + t, -b / 2, b / 2);
    feedZ(p, 0.015, 0, 0);   // λg/4 付近の同軸プローブ相当
    probeZ(p, 0.08, 0, 0);
    freq(p, 8.0e9, 12.4e9, 22, 10.0e9);
    p.post().zin.enabled = true;
    p.post().ref.enabled = true;
}

void emMri(Project &p)
{
    // 3T MRI (128 MHz) ループコイル + 生理食塩水ファントム
    axis(p, 0, -0.15, 0.15, 30);
    axis(p, 1, -0.15, 0.15, 30);
    axis(p, 2, -0.15, 0.15, 30);
    usePml(p);
    const double r = 0.06, w = 0.005;
    box(p, 1, "ループ導体-Y", -r, r, -r - w, -r, -w, w);
    box(p, 1, "ループ導体+Y", -r, r, r, r + w, -w, w);
    box(p, 1, "ループ導体-X", -r - w, -r, -r, r, -w, w);
    // +X 側はギャップ (給電部) を残す
    box(p, 1, "ループ導体+X(上)", r, r + w, 0.01, r, -w, w);
    box(p, 1, "ループ導体+X(下)", r, r + w, -r, -0.01, -w, w);
    const int saline = addMat(p, "生理食塩水ファントム εr=78 σ=0.5", 78.0, 0.5);
    shape6(p, 13, saline, "ファントム円柱", -0.05, 0.05, -0.05, 0.05, 0.02, 0.12);
    feedZ(p, r + w / 2, 0, 0);
    freq(p, 100e6, 150e6, 25, 128e6);
    p.post().zin.enabled = true;
}

void emWpt(Project &p)
{
    // 6.78 MHz 共鳴結合 WPT — 送受 2 ループ (間隔 100 mm)
    axis(p, 0, -0.2, 0.2, 40);
    axis(p, 1, -0.2, 0.2, 40);
    axis(p, 2, -0.15, 0.15, 30);
    usePml(p);
    const double r = 0.1, w = 0.01;
    for (int k = 0; k < 2; ++k) {
        const double z1 = (k == 0) ? -0.055 : 0.045;
        const double z2 = z1 + w;
        const char *tag = (k == 0) ? "送電ループ" : "受電ループ";
        box(p, 1, tag, -r, r, -r - w, -r, z1, z2);
        box(p, 1, tag, -r, r, r, r + w, z1, z2);
        box(p, 1, tag, -r - w, -r, -r, r, z1, z2);
        box(p, 1, tag, r, r + w, 0.02, r, z1, z2);
        box(p, 1, tag, r, r + w, -r, -0.02, z1, z2);
    }
    feedZ(p, 0.105, 0, -0.05);   // 送電側給電
    Load ld;                     // 受電側 50Ω 負荷
    ld.dir = 'Z';
    ld.x = 0.105; ld.y = 0; ld.z = 0.05;
    ld.kind = 'R';
    ld.value = 50.0;
    p.loads().push_back(ld);
    freq(p, 5.0e6, 9.0e6, 40, 6.78e6);
    p.post().zin.enabled = true;
    p.post().coupling.enabled = true;
}

void emSar(Project &p)
{
    // 2.45 GHz ダイポール + 筋肉等価ファントム (SAR 評価の基本構成)
    emDipole(p);
    const int muscle =
        addMat(p, "筋肉等価 εr=52.7 σ=1.74 (2.45GHz)", 52.7, 1.74);
    box(p, muscle, "ファントム 100mm角", 0.015, 0.055, -0.05, 0.05, -0.05, 0.05);
}

void em5g(Project &p)
{
    // 28 GHz マイクロストリップパッチ (Rogers RO4350B εr=3.66, h=0.254mm)
    axis(p, 0, -0.004, 0.004, 40);
    axis(p, 1, -0.004, 0.004, 40);
    axis(p, 2, -0.001, 0.002, 30);
    usePml(p);
    const int sub = addMat(p, "RO4350B εr=3.66", 3.66);
    box(p, 1, "地板", -0.004, 0.004, -0.004, 0.004, -0.000254, -0.000154);
    box(p, sub, "基板 h=0.254mm", -0.004, 0.004, -0.004, 0.004, -0.000154, 0.0001);
    box(p, 1, "パッチ 3.4×2.7mm", -0.0017, 0.0017, -0.00135, 0.00135,
        0.0001, 0.0002);
    feedZ(p, 0.0008, 0, 0);
    freq(p, 26e9, 30e9, 20, 28e9);
    p.post().zin.enabled = true;
    Far1d f;
    f.dir = 'V'; f.div = 72; f.angle = 0;
    p.post().far1d.push_back(f);
}

// ── 光テンプレート ──────────────────────────────────────────────────────────

void optSmallMesh(Project &p)
{
    axis(p, 0, -0.5e-6, 0.5e-6, 10);
    axis(p, 1, -0.5e-6, 0.5e-6, 10);
    axis(p, 2, -0.5e-6, 0.5e-6, 10);
}

void optBpf(Project &p)
{
    // 1550 nm DBR + λ/2 キャビティ (SiO2/Ta2O5) — RCWA 層スタック
    optSmallMesh(p);
    OpticalOpts &o = p.optical();
    o.solver = OpticalSolver::RCWA;
    o.mode = OpticalMode::BPF;
    o.lambdaMin = 1500; o.lambdaMax = 1600; o.lambdaDiv = 201;
    o.bpfBandMin = 1540; o.bpfBandMax = 1560;
    const double nL2 = 2.085, nH2 = 4.41;      // SiO2 n=1.444, Ta2O5 n=2.1
    const double tL = 268.0, tH = 184.5;       // λ/4 厚 [nm]
    auto layer = [](double eps, double t) {
        RcwaLayer l;
        l.eps1 = eps; l.eps2 = eps; l.fill = 1.0; l.thickness_nm = t;
        return l;
    };
    QVector<RcwaLayer> &ls = o.rcwaLayerList;
    ls.push_back(layer(1.0, 0));               // 入射側 半無限 (空気)
    for (int i = 0; i < 4; ++i) {
        ls.push_back(layer(nH2, tH));
        ls.push_back(layer(nL2, tL));
    }
    ls.push_back(layer(nL2, tL));              // λ/2 キャビティ (2×λ/4)
    for (int i = 0; i < 4; ++i) {
        ls.push_back(layer(nH2, tH));
        ls.push_back(layer(nL2, tL));
    }
    ls.push_back(layer(nL2, 0));               // 基板側 半無限 (SiO2)
    p.planewave().enabled = true;              // 垂直入射
    p.planewave().theta = 180;
    p.planewave().phi = 0;
    p.planewave().pol = 1;
    freq(p, 187e12, 200e12, 13, 193.4e12);     // 1500-1600nm / 1550nm
}

void optRing(Project &p)
{
    // Si リング (ディスク近似 r=5μm) + バス導波路 450×220 nm
    axis(p, 0, -7e-6, 7e-6, 140);
    axis(p, 1, -7e-6, 7e-6, 140);
    axis(p, 2, -0.11e-6, 0.11e-6, 2);
    usePml(p);
    const int si = addMat(p, "Si n=3.476", 12.09);
    shape6(p, 13, si, "リング共振器 r=5μm",
           -5e-6, 5e-6, -5e-6, 5e-6, -0.11e-6, 0.11e-6);
    box(p, si, "バス導波路", -7e-6, 7e-6, -5.65e-6, -5.2e-6,
        -0.11e-6, 0.11e-6);
    feedZ(p, -6.5e-6, -5.425e-6, 0);
    probeZ(p, 6.5e-6, -5.425e-6, 0);
    OpticalOpts &o = p.optical();
    o.solver = OpticalSolver::FDTD;
    o.mode = OpticalMode::Ring;
    o.lambdaMin = 1500; o.lambdaMax = 1600; o.lambdaDiv = 201;
    o.ringRadius_um = 5.0;
    o.ringGap_nm = 200.0;
    freq(p, 187e12, 200e12, 13, 193.4e12);
}

void optPhc(Project &p)
{
    // Si スラブのフォトニック結晶 (三角格子近似の空孔列 + 中央欠陥)
    axis(p, 0, -1.05e-6, 1.05e-6, 84);
    axis(p, 1, -1.05e-6, 1.05e-6, 84);
    axis(p, 2, -0.11e-6, 0.11e-6, 2);
    p.general().pbcX = p.general().pbcY = true;
    const int si = addMat(p, "Si n=3.476", 12.09);
    box(p, si, "Si スラブ", -1.05e-6, 1.05e-6, -1.05e-6, 1.05e-6,
        -0.11e-6, 0.11e-6);
    const double a = 0.42e-6, r = 0.12e-6;
    for (int ix = -2; ix <= 2; ++ix) {
        for (int iy = -2; iy <= 2; ++iy) {
            if (ix == 0 && iy == 0) continue;   // 中央は欠陥 (孔なし)
            const double cx = ix * a, cy = iy * a;
            shape6(p, 13, 0, "空孔", cx - r, cx + r, cy - r, cy + r,
                   -0.11e-6, 0.11e-6);
        }
    }
    feedZ(p, 0, 0, 0);   // 欠陥モード励振
    OpticalOpts &o = p.optical();
    o.solver = OpticalSolver::FDTD;
    o.mode = OpticalMode::PhC;
    o.lambdaMin = 1400; o.lambdaMax = 1700; o.lambdaDiv = 301;
    freq(p, 176e12, 214e12, 19, 193.4e12);
}

void optMeta(Project &p)
{
    // TiO2 ピラー メタサーフェス (周期 400 nm) — RCWA
    optSmallMesh(p);
    OpticalOpts &o = p.optical();
    o.solver = OpticalSolver::RCWA;
    o.mode = OpticalMode::Metasurface;
    o.lambdaMin = 400; o.lambdaMax = 700; o.lambdaDiv = 151;
    o.rcwaPeriodX = 400; o.rcwaPeriodY = 400;
    auto layer = [](double e1, double e2, double fill, double t) {
        RcwaLayer l;
        l.eps1 = e1; l.eps2 = e2; l.fill = fill; l.thickness_nm = t;
        return l;
    };
    o.rcwaLayerList = {
        layer(1.0, 1.0, 1.0, 0),         // 空気 (半無限)
        layer(6.25, 1.0, 0.5, 600),      // TiO2 ピラー層 (fill 0.5)
        layer(2.085, 2.085, 1.0, 0),     // SiO2 基板 (半無限)
    };
    p.planewave().enabled = true;        // 垂直入射
    p.planewave().theta = 180;
    p.planewave().phi = 0;
    p.planewave().pol = 1;
    freq(p, 428e12, 749e12, 21, 545e12);
}

void optPlasmon(Project &p)
{
    // 金属ナノロッド (PEC 近似 — 分散性 Drude 金属は本家 .ofd の一次分散に
    // 厳密対応しないため、開始点として PEC で共鳴スケールを見る)
    axis(p, 0, -0.2e-6, 0.2e-6, 40);
    axis(p, 1, -0.2e-6, 0.2e-6, 40);
    axis(p, 2, -0.2e-6, 0.2e-6, 40);
    usePml(p);
    box(p, 1, "ナノロッド 40×40×120nm (PEC近似)",
        -0.02e-6, 0.02e-6, -0.02e-6, 0.02e-6, -0.06e-6, 0.06e-6);
    p.planewave().enabled = true;
    p.planewave().theta = 90;
    p.planewave().phi = 0;
    p.planewave().pol = 1;
    OpticalOpts &o = p.optical();
    o.solver = OpticalSolver::FDTD;
    o.lambdaMin = 600; o.lambdaMax = 900; o.lambdaDiv = 151;
    freq(p, 333e12, 500e12, 21, 375e12);
}

void optNonlinear(Project &p)
{
    // TPA 光活性化 (Opt. Lett. 49, 5811) — OpenBPM の tpa/powersweep
    axis(p, 0, -2e-6, 2e-6, 80);
    axis(p, 1, -2e-6, 2e-6, 80);
    axis(p, 2, 0, 100e-6, 100);
    const int si = addMat(p, "Si n=3.476 (TPA)", 12.09);
    box(p, si, "Si 導波路 450×220nm", -0.225e-6, 0.225e-6,
        -0.11e-6, 0.11e-6, 0, 100e-6);
    feedZ(p, 0, 0, 0);
    OpticalOpts &o = p.optical();
    o.solver = OpticalSolver::BPM;
    o.mode = OpticalMode::Waveguide;
    o.lambdaMin = 1550; o.lambdaMax = 1550; o.lambdaDiv = 1;
    o.tpaEnabled = true;
    o.tpaMaterialId = si;
    o.tpaBeta_cmGW = 424.0;
    o.powerSweepEnabled = true;
    o.psPmin_W = 0.001; o.psPmax_W = 10.0; o.psPoints = 41; o.psLog = true;
    freq(p, 193.4e12, 193.4e12, 1, 193.4e12);
}

void optSolar(Project &p)
{
    // 薄膜 Si 太陽電池 (AR コート付き) — 垂直入射・周期境界
    axis(p, 0, -0.25e-6, 0.25e-6, 20);
    axis(p, 1, -0.25e-6, 0.25e-6, 20);
    axis(p, 2, -0.5e-6, 2.6e-6, 124);
    p.general().pbcX = p.general().pbcY = true;
    const int si = addMat(p, "Si 吸収層", 12.09);
    const int ar = addMat(p, "SiO2 ARコート", 2.085);
    box(p, ar, "ARコート 80nm", -0.25e-6, 0.25e-6, -0.25e-6, 0.25e-6,
        0, 0.08e-6);
    box(p, si, "Si 2μm", -0.25e-6, 0.25e-6, -0.25e-6, 0.25e-6,
        0.08e-6, 2.08e-6);
    box(p, 1, "裏面電極", -0.25e-6, 0.25e-6, -0.25e-6, 0.25e-6,
        2.08e-6, 2.18e-6);
    p.planewave().enabled = true;
    p.planewave().theta = 180;    // 上方から垂直入射
    p.planewave().phi = 0;
    p.planewave().pol = 1;
    OpticalOpts &o = p.optical();
    o.lambdaMin = 400; o.lambdaMax = 1100; o.lambdaDiv = 141;
    freq(p, 273e12, 749e12, 20, 545e12);
}

void optLidar(Project &p)
{
    // 905 nm LiDAR ターゲット後方散乱 (far0d で単一方向 RCS 相当)
    axis(p, 0, -2e-6, 2e-6, 80);
    axis(p, 1, -2e-6, 2e-6, 80);
    axis(p, 2, -2e-6, 2e-6, 80);
    usePml(p);
    box(p, 1, "ターゲット反射板", -1e-6, 1e-6, -1e-6, 1e-6, 0.5e-6, 0.6e-6);
    p.planewave().enabled = true;
    p.planewave().theta = 180;
    p.planewave().phi = 0;
    p.planewave().pol = 1;
    OpticalOpts &o = p.optical();
    o.lambdaMin = 895; o.lambdaMax = 915; o.lambdaDiv = 21;
    freq(p, 327e12, 335e12, 8, 331e12);
    p.post().far0d = true;
    p.post().far0dTheta = 0;    // 後方散乱
    p.post().far0dPhi = 0;
}

void optRaycast(Project &p)
{
    // マイクロレンズ (BK7 n=1.517) の集光 — FDTD で扱える小スケール光学系。
    // 大規模レイトレースは外部ツール連携 (ツール連携タブ) を使う
    axis(p, 0, -3e-6, 3e-6, 120);
    axis(p, 1, -3e-6, 3e-6, 120);
    axis(p, 2, -3e-6, 5e-6, 160);
    usePml(p);
    const int bk7 = addMat(p, "BK7 n=1.517", 2.301);
    shape6(p, 2, bk7, "マイクロレンズ (半球近似)",
           -2.5e-6, 2.5e-6, -2.5e-6, 2.5e-6, -1.2e-6, 1.2e-6);
    p.planewave().enabled = true;
    p.planewave().theta = 180;
    p.planewave().phi = 0;
    p.planewave().pol = 1;
    OpticalOpts &o = p.optical();
    o.lambdaMin = 550; o.lambdaMax = 550; o.lambdaDiv = 1;
    freq(p, 545e12, 545e12, 1, 545e12);
    Near2d n2;
    n2.cmp = "E"; n2.dir = 'Y'; n2.pos = 0;
    p.post().near2d.push_back(n2);
}

void optHybrid(Project &p)
{
    // ナノ格子 (FDTD 部分) — 大スケール伝搬は外部レイツールへ受け渡す構成
    axis(p, 0, -1.2e-6, 1.2e-6, 96);
    axis(p, 1, -0.3e-6, 0.3e-6, 24);
    axis(p, 2, -1e-6, 1e-6, 80);
    p.general().pbcX = p.general().pbcY = true;
    const int si = addMat(p, "Si 格子", 12.09);
    for (int i = -2; i <= 2; ++i) {
        const double cx = i * 0.5e-6;
        box(p, si, "格子ライン", cx - 0.125e-6, cx + 0.125e-6,
            -0.3e-6, 0.3e-6, 0, 0.3e-6);
    }
    p.planewave().enabled = true;
    p.planewave().theta = 180;
    p.planewave().phi = 0;
    p.planewave().pol = 1;
    OpticalOpts &o = p.optical();
    o.lambdaMin = 1500; o.lambdaMax = 1600; o.lambdaDiv = 101;
    freq(p, 187e12, 200e12, 13, 193.4e12);
}

// ── 室内音響テンプレート ────────────────────────────────────────────────────

void acHall(Project &p)
{
    // シューボックスホール 30×20×12 m (吸音バジェットは clear() の既定 =
    // コンサートホール例をそのまま使う)
    room(p, 30, 20, 12, 0.5, 5, 10, 1.5, 20, 10, 1.2);
    AcousticOpts &a = p.acoustic();
    a.rt60 = a.c80 = a.edt = true;
    a.d50 = true;
    a.impulseResponse = true;
    a.occupancy = 2;
    a.rtFormula = 1;   // Eyring
}

void acOffice(Project &p)
{
    room(p, 8, 6, 2.7, 0.1, 1.0, 3.0, 1.2, 6.0, 3.0, 1.2);
    AcousticOpts &a = p.acoustic();
    a.rt60 = a.d50 = a.sti = true;
    a.c80 = false;
    a.occupancy = 1;
    a.rtFormula = 0;   // Sabine (小空間)
    a.absorption = {
        absRow(AbsorptionRow::Ceiling, "岩綿吸音天井", 48,
               { 0.35, 0.55, 0.70, 0.80, 0.85, 0.80 }),
        absRow(AbsorptionRow::Floor, "タイルカーペット", 48,
               { 0.03, 0.06, 0.15, 0.30, 0.40, 0.50 }),
        absRow(AbsorptionRow::SideWall, "石膏ボード壁", 60,
               { 0.10, 0.08, 0.05, 0.04, 0.06, 0.06 }),
        absRow(AbsorptionRow::Other, "在席者・什器", 20,
               { 0.20, 0.30, 0.40, 0.45, 0.50, 0.50 }),
        absRow(AbsorptionRow::Air, "空気吸収", 0, { 0, 0, 0, 0, 0, 0 }, 2),
    };
}

void acStudio(Project &p)
{
    room(p, 5, 4, 3, 0.1, 0.8, 2.0, 1.2, 3.5, 2.0, 1.2);
    AcousticOpts &a = p.acoustic();
    a.rt60 = a.edt = true;
    a.c80 = a.d50 = false;
    a.impulseResponse = true;
    a.rtFormula = 0;
    a.absorption = {
        absRow(AbsorptionRow::Ceiling, "吸音クラウド", 20,
               { 0.40, 0.70, 0.90, 0.95, 0.95, 0.90 }),
        absRow(AbsorptionRow::SideWall, "広帯域吸音パネル", 40,
               { 0.30, 0.60, 0.85, 0.90, 0.90, 0.85 }),
        absRow(AbsorptionRow::RearWall, "ディフューザ", 12,
               { 0.15, 0.20, 0.25, 0.28, 0.30, 0.30 }),
        absRow(AbsorptionRow::Floor, "フローリング", 20,
               { 0.15, 0.11, 0.10, 0.07, 0.06, 0.07 }),
        absRow(AbsorptionRow::Air, "空気吸収", 0, { 0, 0, 0, 0, 0, 0 }, 1),
    };
}

void acOutdoor(Project &p)
{
    // 屋外伝搬 100×20×15 m + 高さ 3 m の遮音壁 (x=30 m)
    axis(p, 0, 0, 100, 100);
    axis(p, 1, 0, 20, 20);
    axis(p, 2, 0, 15, 30);
    const int wall = addMat(p, "コンクリート遮音壁", 1.0, 0.0,
                            2400.0, 3500.0, 0.03);
    box(p, wall, "遮音壁 h=3m", 30.0, 30.3, 0, 20, 0, 3.0);
    feedZ(p, 5, 10, 1.5);
    probeZ(p, 70, 10, 1.5);
    AcousticOpts &a = p.acoustic();
    a.rt60 = a.c80 = a.d50 = a.sti = a.edt = false;
    a.impulseResponse = true;
    a.srcSPL_dB = 100.0;   // 交通騒音源相当
}

void acAural(Project &p)
{
    room(p, 15, 10, 6, 0.25, 2.5, 5, 1.5, 10, 5, 1.2);
    AcousticOpts &a = p.acoustic();
    a.rt60 = a.edt = true;
    a.impulseResponse = true;
    a.auralization = true;
    a.sampleRate = 48000;
}

void acRaytrace(Project &p)
{
    acHall(p);
    // RIR は外部幾何音響 (レイトレース系) ソルバーから取得する構成
    p.operaAcoustic().solverBackend = 4;   // ExternalGeometric
}

void acImageSource(Project &p)
{
    // 剛壁シューボックス — 鏡像法の初期反射検証に向く構成
    room(p, 6, 4, 3, 0.1, 1.0, 2.0, 1.5, 4.5, 2.0, 1.5);
    AcousticOpts &a = p.acoustic();
    a.rt60 = false;
    a.impulseResponse = true;
    a.absorption = {
        absRow(AbsorptionRow::Other, "剛壁 (低吸音)", 108,
               { 0.02, 0.02, 0.03, 0.03, 0.04, 0.05 }),
        absRow(AbsorptionRow::Air, "空気吸収", 0, { 0, 0, 0, 0, 0, 0 }, 1),
    };
    p.operaAcoustic().solverBackend = 4;   // ExternalGeometric
}

void acNoise(Project &p)
{
    room(p, 4, 3, 2.5, 0.1, 0.5, 1.5, 1.2, 3.0, 1.5, 1.2);
    AcousticOpts &a = p.acoustic();
    a.rt60 = true;
    a.sti = false;
    const double lv[7] = { 48, 44, 40, 36, 32, 28, 24 };  // NC-35 相当の現状値
    for (int i = 0; i < 7; ++i) a.noiseLevels[i] = lv[i];
    QVector<NoiseSourceRow> rows = defaultNoiseSources();
    for (NoiseSourceRow &r : rows) r.enabled = true;
    a.noiseSources = rows;
    const int gypsum = addMat(p, "石膏ボード界壁", 1.0, 0.0,
                              700.0, 1600.0, 0.05);
    box(p, gypsum, "界壁", 3.9, 4.0, 0, 3, 0, 2.5);
}

// ── 水中音響テンプレート ────────────────────────────────────────────────────

void uwBase(Project &p, double freq_kHz, double sl_dB, double range_km,
            double depth_m, const char *bottom, double bc, double brho)
{
    ocean(p, range_km * 1000.0, depth_m,
          200, qMax(20, int(depth_m / 25)));
    UnderwaterOpts &u = p.underwater();
    u.sonarFreq_kHz = freq_kHz;
    u.sonarSL_dB = sl_dB;
    u.rangeMax_km = range_km;
    u.bottomType = QString::fromUtf8(bottom);
    u.bottomC_mps = bc;
    u.bottomRho_kgm3 = brho;
    feedZ(p, 0, 0, depth_m * 0.1);          // 音源 (深度 10%)
    probeZ(p, range_km * 1000.0 * 0.8, 0, depth_m * 0.2);
}

void uwSofar(Project &p)
{
    // 深海 SOFAR チャネル伝搬 (Munk 型 SSP は clear() の既定)
    uwBase(p, 3.5, 220, 100, 5000, "clay", 1500, 1500);
    p.underwater().sofar = true;
}

void uwSonar(Project &p)
{
    // 50 kHz アクティブソナー (浅海・負勾配 SSP)
    uwBase(p, 50, 220, 5, 200, "sand", 1650, 1900);
    p.underwater().ssp = { { 0, 1520 }, { 20, 1515 }, { 50, 1505 },
                           { 100, 1500 }, { 200, 1498 } };
}

void uwBathy(Project &p)
{
    // 200 kHz マルチビーム測深
    uwBase(p, 200, 210, 0.5, 300, "rock", 3000, 2500);
    p.underwater().ssp = { { 0, 1510 }, { 50, 1500 }, { 150, 1495 },
                           { 300, 1498 } };
}

void uwBio(Project &p)
{
    // 38 kHz 計量魚探 (魚群・鯨類エコー)
    uwBase(p, 38, 210, 2, 500, "sand", 1650, 1900);
    p.underwater().ssp = { { 0, 1515 }, { 30, 1512 }, { 100, 1500 },
                           { 300, 1492 }, { 500, 1490 } };
}

void uwComm(Project &p)
{
    // 25 kHz 水中音響モデムリンク (混合層のサーフェスダクト)
    uwBase(p, 25, 190, 10, 1000, "silt", 1550, 1600);
    p.underwater().ssp = { { 0, 1500 }, { 50, 1502 }, { 100, 1498 },
                           { 500, 1487 }, { 1000, 1485 } };
}

void uwSeismo(Project &p)
{
    // T-wave (海中を伝わる地震波, ~10 Hz) — SOFAR 軸を長距離伝搬
    uwBase(p, 0.01, 180, 1000, 5000, "rock", 3000, 2500);
    p.underwater().sofar = true;
}

void uwPe(Project &p)
{
    // 放物方程式 (PE) 向きの低周波・長距離レンジ依存伝搬
    uwBase(p, 0.1, 200, 200, 4000, "clay", 1520, 1500);
}

// ── tidy3d テンプレート (光ドメインのクラウドバックエンド) ──────────────────

void t3Large(Project &p)
{
    optMeta(p);
    p.tidy3d().projectName = "metasurface-array";
    p.tidy3d().resolution = "fine";
}

void t3Sweep(Project &p)
{
    optRing(p);
    p.tidy3d().projectName = "param-sweep";
    p.tidy3d().resolution = "medium";
}

void t3Inverse(Project &p)
{
    optHybrid(p);
    p.tidy3d().projectName = "inverse-design";
    p.tidy3d().resolution = "fine";
}

void t3Ml(Project &p)
{
    optBpf(p);
    p.tidy3d().projectName = "dataset-gen";
    p.tidy3d().resolution = "coarse";
}

// ── レジストリ ──────────────────────────────────────────────────────────────
struct Entry {
    const char *domain;
    const char *id;
    const char *title;
    void (*build)(Project &);
};

const Entry kEntries[] = {
    { "em", "em_antenna",   "アンテナ放射パターン (2.45GHz ダイポール)", emAntenna },
    { "em", "em_emc",       "EMC/EMI — スロット付きシールド筐体", emEmc },
    { "em", "em_rcs",       "RCS — PEC 球の平面波散乱", emRcs },
    { "em", "em_waveguide", "マイクロ波回路 — WR-90 導波管", emWaveguide },
    { "em", "em_mri",       "MRI コイル (128MHz) + ファントム", emMri },
    { "em", "em_wpt",       "ワイヤレス給電 (6.78MHz 共鳴結合)", emWpt },
    { "em", "em_sar",       "生体 SAR — ダイポール + 筋肉ファントム", emSar },
    { "em", "em_5g",        "5G ミリ波 — 28GHz パッチアンテナ", em5g },

    { "optical", "opt_bpf",       "BPF — DBR + キャビティ (RCWA)", optBpf },
    { "optical", "opt_ring",      "リング共振器 (Si, r=5μm)", optRing },
    { "optical", "opt_phc",       "フォトニック結晶 欠陥モード", optPhc },
    { "optical", "opt_meta",      "メタサーフェス (TiO2 ピラー)", optMeta },
    { "optical", "opt_plasmon",   "プラズモニクス — ナノロッド (PEC近似)", optPlasmon },
    { "optical", "opt_nonlinear", "非線形 — TPA 光活性化 (BPM)", optNonlinear },
    { "optical", "opt_solar",     "太陽電池 — 薄膜 Si + AR", optSolar },
    { "optical", "opt_lidar",     "LiDAR — 905nm 後方散乱", optLidar },
    { "optical", "opt_raycast",   "マイクロレンズ集光 (FDTD)", optRaycast },
    { "optical", "opt_hybrid",    "ナノ格子 + 外部レイ連携", optHybrid },

    { "acoustic", "ac_hall",        "コンサートホール 30×20×12m", acHall },
    { "acoustic", "ac_office",      "オフィス・教室 (STI)", acOffice },
    { "acoustic", "ac_studio",      "スタジオ・コントロールルーム", acStudio },
    { "acoustic", "ac_outdoor",     "屋外伝搬 + 遮音壁", acOutdoor },
    { "acoustic", "ac_aural",       "オーラリゼーション", acAural },
    { "acoustic", "ac_raytrace",    "幾何音響レイトレース連携", acRaytrace },
    { "acoustic", "ac_imagesource", "鏡像法 — 剛壁シューボックス", acImageSource },
    { "acoustic", "ac_noise",       "騒音解析 / 防音設計", acNoise },

    { "underwater", "uw_sofar",  "SOFAR チャネル伝搬 (Munk SSP)", uwSofar },
    { "underwater", "uw_sonar",  "アクティブソナー 50kHz", uwSonar },
    { "underwater", "uw_bathy",  "マルチビーム測深 200kHz", uwBathy },
    { "underwater", "uw_bio",    "計量魚探 38kHz (魚群エコー)", uwBio },
    { "underwater", "uw_comm",   "水中音響モデム 25kHz", uwComm },
    { "underwater", "uw_seismo", "T-wave (海中地震波 10Hz)", uwSeismo },
    { "underwater", "uw_pe",     "PE 法 — 低周波長距離伝搬", uwPe },

    { "tidy3d", "t3_large",   "大規模 3D — メタサーフェスアレイ", t3Large },
    { "tidy3d", "t3_sweep",   "パラメータスイープ — リング共振器", t3Sweep },
    { "tidy3d", "t3_inverse", "形状最適化 / 逆設計", t3Inverse },
    { "tidy3d", "t3_ml",      "ML/AI — データセット生成", t3Ml },
};

Domain domainFor(const QString &key)
{
    if (key == QLatin1String("optical") || key == QLatin1String("tidy3d"))
        return Domain::Optical;
    if (key == QLatin1String("acoustic"))   return Domain::Acoustic;
    if (key == QLatin1String("underwater")) return Domain::Underwater;
    return Domain::EM;
}

} // namespace

QStringList idsFor(const QString &domainKey)
{
    QStringList out;
    for (const Entry &e : kEntries)
        if (domainKey == QLatin1String(e.domain))
            out << QString::fromLatin1(e.id);
    return out;
}

bool apply(Project &p, const QString &domainKey, const QString &id,
           const QString &displayName)
{
    for (const Entry &e : kEntries) {
        if (domainKey != QLatin1String(e.domain) ||
            id != QLatin1String(e.id))
            continue;
        p.clear();
        p.setActiveDomain(domainFor(domainKey));
        e.build(p);
        p.general().title = displayName.isEmpty()
            ? QString::fromUtf8(e.title) : displayName;
        return true;
    }
    return false;
}

} // namespace templates
} // namespace ofd
