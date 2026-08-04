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
//
// 【吸音率 α の出所について】
// 以下の α は建築音響で広く引用される「材料区分ごとの代表値」を 2 桁に
// 丸めたもの (L. Beranek, "Concert Halls and Opera Houses" の聴衆・客席、
// および一般的な建築材料吸音率表 — 岩綿吸音板 / グラスウール / 石膏ボード /
// カーペット / 板張り床 など)。特定製品の実測値ではないので、実設計では
// メーカーの残響室法吸音率 (JIS A 1409) に差し替えること。
//
// 【空気吸収 A_air】
// A_air = 4·m·V (20 °C / 相対湿度 50 %、m(1 kHz) ≈ 0.003 m⁻¹) より
// A_air(1 kHz) ≈ 0.012·V [Sabin]。AbsorptionRow::Air には 1 kHz 値を入れ、
// 帯域換算は RoomAcoustics.cpp の kAirRatio が行う。
//
// 【目標残響時間】
// 各テンプレートのコメントに用途別の目標 RT60 (中音域 = 500 Hz と 1 kHz の
// 平均) を明記する。吸音バジェットは「目標に合うように」面積と α を組んで
// あり、tests/selftest.cpp の testProjectTemplates が roomac::rt60() で
// 期待レンジに入ることを検証する (期待値はテスト側に独立に書いてある)。

void acHall(Project &p)
{
    // シューボックス型コンサートホール 30×20×12 m (V = 7200 m³, S = 2400 m²、
    // 約 800 席相当)。
    // 目標 RT60 (満席・中音域): 1.7〜2.0 s。交響楽用ホールの推奨値は容積に
    // 応じて 1.6〜2.1 s とされ、V ≈ 7000 m³ 級では 1.8 s 前後が標準的
    // (Beranek の推奨レンジ — 典型値)。低音比 BR = T125/Tmid は 1.1〜1.25 が
    // 目安で、本バジェットは約 1.2。
    room(p, 30, 20, 12, 0.5, 5, 10, 1.5, 20, 10, 1.2);
    AcousticOpts &a = p.acoustic();
    a.rt60 = a.c80 = a.edt = true;
    a.d50 = true;
    a.impulseResponse = true;
    a.occupancy = 2;
    a.rtFormula = 1;   // Eyring (吸音が面ごとに偏る大空間)
    a.absorption = {
        // 客席 420 m² = 舞台前の平土間+段床客席。満席の聴衆 (中程度の
        // 布張り椅子) の典型値。
        absRow(AbsorptionRow::Audience, "客席(満席・布張り椅子)", 420,
               { 0.62, 0.72, 0.80, 0.83, 0.84, 0.85 }),
        absRow(AbsorptionRow::Floor, "舞台床・通路(板張り)", 180,
               { 0.15, 0.11, 0.10, 0.07, 0.06, 0.07 }),
        absRow(AbsorptionRow::Ceiling, "天井(GRC・反射性)", 600,
               { 0.08, 0.07, 0.06, 0.05, 0.04, 0.03 }),
        absRow(AbsorptionRow::SideWall, "側壁(厚板・剛壁下地)", 720,
               { 0.10, 0.09, 0.09, 0.08, 0.08, 0.07 }),
        absRow(AbsorptionRow::RearWall, "後壁(拡散体)", 240,
               { 0.14, 0.14, 0.14, 0.13, 0.13, 0.12 }),
        absRow(AbsorptionRow::Other, "天井反射板・舞台まわり", 240,
               { 0.08, 0.07, 0.07, 0.06, 0.06, 0.06 }),
        absRow(AbsorptionRow::Air, "空気吸収 4mV", 0,
               { 0, 0, 0, 0, 0, 0 }, 86),   // 0.012 × 7200
    };
    // ── 障害物ジオメトリ (舞台・段床客席・バルコニー・反射板) ──
    const int stageW = addMat(p, "舞台床(木)", 1.0, 0.0, 600.0, 3500.0, 0.10);
    const int seat   = addMat(p, "客席ブロック(布張り)", 1.0, 0.0,
                              90.0, 343.0, 0.80);
    const int rc     = addMat(p, "バルコニー床版(RC)", 1.0, 0.0,
                              2400.0, 3500.0, 0.02);
    const int refl   = addMat(p, "天井反射板(GRC)", 1.0, 0.0,
                              1900.0, 3200.0, 0.05);
    box(p, stageW, "舞台 (h=1.0m)", 0.5, 8.0, 2.0, 18.0, 0.0, 1.0);
    box(p, seat, "前方客席ブロック", 10.0, 19.5, 2.0, 18.0, 0.0, 0.9);
    box(p, seat, "後方客席ブロック (段床)", 20.5, 29.0, 2.0, 18.0, 0.9, 2.4);
    box(p, rc, "バルコニー床版", 22.0, 29.5, 1.0, 19.0, 6.0, 6.5);
    box(p, refl, "天井反射板 (浮雲)", 5.0, 13.0, 5.0, 15.0, 9.4, 9.7);
    // 音源 (5,10,1.5) は舞台上面 +0.5 m、受音点 (20,10,1.2) は前後客席
    // ブロックの間の横通路 (x = 19.5〜20.5) に置いてある。
}

void acOffice(Project &p)
{
    // オフィス / 教室 8×6×2.7 m (V = 129.6 m³, S = 171.6 m²)。
    // 目標 RT60 (中音域): 0.4〜0.6 s。事務室・普通教室で会話明瞭度 (STI) を
    // 確保する際の慣行値で、天井の吸音施工率で調整する。本バジェットは
    // 岩綿吸音板を天井面の約 6 割に施工した想定 (mid ≈ 0.50 s)。
    room(p, 8, 6, 2.7, 0.1, 1.0, 3.0, 1.2, 6.0, 3.0, 1.2);
    AcousticOpts &a = p.acoustic();
    a.rt60 = a.d50 = a.sti = true;
    a.c80 = false;
    a.occupancy = 1;
    a.rtFormula = 0;   // Sabine (小空間)
    a.absorption = {
        absRow(AbsorptionRow::Ceiling, "岩綿吸音天井(施工率60%)", 30,
               { 0.35, 0.55, 0.70, 0.80, 0.85, 0.80 }),
        // 石膏ボード + 背後空気層は膜共鳴で低音側の α が高い (典型値)。
        absRow(AbsorptionRow::Ceiling, "石膏ボード天井(残り)", 18,
               { 0.30, 0.12, 0.06, 0.05, 0.06, 0.07 }),
        absRow(AbsorptionRow::Floor, "タイルカーペット(直貼り)", 48,
               { 0.02, 0.04, 0.08, 0.15, 0.25, 0.35 }),
        absRow(AbsorptionRow::SideWall, "石膏ボード壁(下地空気層)", 60,
               { 0.30, 0.12, 0.06, 0.05, 0.06, 0.07 }),
        absRow(AbsorptionRow::Other, "在席者・什器", 20,
               { 0.20, 0.30, 0.40, 0.45, 0.50, 0.50 }),
        absRow(AbsorptionRow::Air, "空気吸収 4mV", 0,
               { 0, 0, 0, 0, 0, 0 }, 2),     // 0.012 × 129.6
    };
    // ── 障害物ジオメトリ (防音間仕切り・什器) ──
    const int part = addMat(p, "吸音間仕切りパーティション", 1.0, 0.0,
                            120.0, 343.0, 0.60);
    const int furn = addMat(p, "什器 (書棚・デスク)", 1.0, 0.0,
                            550.0, 3300.0, 0.15);
    box(p, part, "防音間仕切り h=1.6m", 3.95, 4.05, 0.0, 4.0, 0.0, 1.6);
    box(p, furn, "書棚", 0.2, 0.6, 0.5, 3.5, 0.0, 1.8);
    box(p, furn, "デスク島 (音源側)", 1.0, 2.6, 1.8, 4.2, 0.6, 0.75);
    box(p, furn, "デスク島 (受音側)", 5.0, 6.6, 1.8, 4.2, 0.6, 0.75);
    // 音源 (1,3,1.2) と受音点 (6,3,1.2) の直線上に間仕切り (x = 4) が入る。
}

void acStudio(Project &p)
{
    // スタジオ / コントロールルーム 5×4×3 m (V = 60 m³, S = 94 m²)。
    // 目標 RT60 (中音域): 0.16〜0.26 s。EBU Tech 3276 / ITU-R BS.1116 の
    // 基準室残響時間 Tm = 0.25·(V/100)^(1/3) s は V = 60 m³ で 0.21 s、
    // 許容差 ±0.05 s。本バジェットは mid ≈ 0.22 s、125 Hz ≈ 0.36 s
    // (低域側許容 +0.3 s 以内)。
    room(p, 5, 4, 3, 0.1, 0.8, 2.0, 1.2, 3.5, 2.0, 1.2);
    AcousticOpts &a = p.acoustic();
    a.rt60 = a.edt = true;
    a.c80 = a.d50 = false;
    a.impulseResponse = true;
    a.rtFormula = 0;
    a.absorption = {
        absRow(AbsorptionRow::Ceiling, "吸音クラウド", 12,
               { 0.40, 0.70, 0.90, 0.95, 0.95, 0.90 }),
        absRow(AbsorptionRow::Ceiling, "石膏ボード天井(残り)", 8,
               { 0.30, 0.12, 0.06, 0.05, 0.06, 0.07 }),
        absRow(AbsorptionRow::SideWall, "広帯域吸音パネル", 24,
               { 0.30, 0.60, 0.85, 0.90, 0.90, 0.85 }),
        absRow(AbsorptionRow::SideWall, "木リブ拡散面", 12,
               { 0.15, 0.15, 0.14, 0.12, 0.12, 0.12 }),
        absRow(AbsorptionRow::RearWall, "ディフューザ", 12,
               { 0.15, 0.20, 0.25, 0.28, 0.30, 0.30 }),
        absRow(AbsorptionRow::Floor, "フローリング", 20,
               { 0.15, 0.11, 0.10, 0.07, 0.06, 0.07 }),
        // 隅のバストラップ (下のジオメトリと対応)。低域に効く典型値。
        absRow(AbsorptionRow::Other, "隅バストラップ", 10,
               { 0.60, 0.55, 0.45, 0.35, 0.30, 0.25 }),
        absRow(AbsorptionRow::Air, "空気吸収 4mV", 0,
               { 0, 0, 0, 0, 0, 0 }, 1),     // 0.012 × 60
    };
    // ── 障害物ジオメトリ (バストラップ・調整卓・観測窓) ──
    const int trap  = addMat(p, "バストラップ (GW充填)", 1.0, 0.0,
                             48.0, 343.0, 0.55);
    const int desk  = addMat(p, "調整卓", 1.0, 0.0, 700.0, 3300.0, 0.20);
    const int glass = addMat(p, "観測窓 (合わせガラス)", 1.0, 0.0,
                             2500.0, 5500.0, 0.04);
    box(p, trap, "隅バストラップ (0,0)", 0.0, 0.4, 0.0, 0.4, 0.0, 3.0);
    box(p, trap, "隅バストラップ (L,0)", 4.6, 5.0, 0.0, 0.4, 0.0, 3.0);
    box(p, trap, "隅バストラップ (0,W)", 0.0, 0.4, 3.6, 4.0, 0.0, 3.0);
    box(p, trap, "隅バストラップ (L,W)", 4.6, 5.0, 3.6, 4.0, 0.0, 3.0);
    box(p, desk, "調整卓", 2.0, 3.0, 1.3, 2.7, 0.7, 1.15);
    box(p, glass, "観測窓", 4.94, 5.0, 1.0, 3.0, 1.0, 2.1);
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
    // 可聴化用の小ホール 15×10×6 m (V = 900 m³, S = 600 m²、約 150 席)。
    // 目標 RT60 (中音域): 0.9〜1.2 s。多目的の小ホール・講堂は音楽と会話の
    // 両立のため 1.0 s 前後に設定するのが慣行 (容積 1000 m³ 級の目安)。
    // 本バジェットは mid ≈ 1.06 s。
    room(p, 15, 10, 6, 0.25, 2.5, 5, 1.5, 10, 5, 1.2);
    AcousticOpts &a = p.acoustic();
    a.rt60 = a.edt = true;
    a.c80 = a.d50 = true;
    a.impulseResponse = true;
    a.auralization = true;
    a.sampleRate = 48000;
    a.occupancy = 2;
    a.rtFormula = 1;   // Eyring
    a.absorption = {
        absRow(AbsorptionRow::Audience, "客席(満席・布張り椅子)", 70,
               { 0.55, 0.66, 0.75, 0.79, 0.80, 0.80 }),
        absRow(AbsorptionRow::Floor, "木床(舞台・通路)", 80,
               { 0.15, 0.11, 0.10, 0.07, 0.06, 0.07 }),
        absRow(AbsorptionRow::Ceiling, "天井(石膏+部分吸音)", 150,
               { 0.12, 0.10, 0.08, 0.07, 0.06, 0.06 }),
        absRow(AbsorptionRow::SideWall, "側壁(木パネル)", 180,
               { 0.15, 0.12, 0.10, 0.09, 0.08, 0.07 }),
        absRow(AbsorptionRow::RearWall, "後壁(吸音+拡散)", 60,
               { 0.20, 0.30, 0.35, 0.38, 0.38, 0.35 }),
        absRow(AbsorptionRow::Other, "舞台まわり", 60,
               { 0.12, 0.10, 0.08, 0.07, 0.06, 0.06 }),
        absRow(AbsorptionRow::Air, "空気吸収 4mV", 0,
               { 0, 0, 0, 0, 0, 0 }, 11),    // 0.012 × 900
    };
    // ── 障害物ジオメトリ (ステージ・客席ブロック) ──
    // バイノーラル可聴化では客席ブロックによる座席列越しの回折
    // (seat-dip effect) が効くので、段床を持たせておく。
    const int stageW = addMat(p, "ステージ床(木)", 1.0, 0.0,
                              600.0, 3500.0, 0.10);
    const int seat   = addMat(p, "客席ブロック(布張り)", 1.0, 0.0,
                              90.0, 343.0, 0.78);
    box(p, stageW, "ステージ (h=0.6m)", 0.5, 4.0, 1.0, 9.0, 0.0, 0.6);
    box(p, seat, "前方客席ブロック", 6.0, 10.0, 1.0, 9.0, 0.0, 0.45);
    box(p, seat, "後方客席ブロック (段床)", 10.5, 14.0, 1.0, 9.0, 0.45, 0.9);
}

void acRaytrace(Project &p)
{
    acHall(p);
    // RIR は外部幾何音響 (レイトレース系) ソルバーから取得する構成
    p.operaAcoustic().solverBackend = 4;   // ExternalGeometric
}

void acImageSource(Project &p)
{
    // 剛壁シューボックス 6×4×3 m — 鏡像法の初期反射検証に向く構成。
    //
    // 【障害物を入れてはいけない】
    // 鏡像法 (image-source method) の解析解は「凸な空room = 単純な直方体」で
    // しか成立しない。室内に障害物を置くと鏡像が遮蔽され (可視性判定が必要に
    // なり)、RoomAcoustics::echogram() の 1 次鏡像列や外部幾何音響ソルバーの
    // 出力と解析解を突き合わせる検証ケースとして機能しなくなる。
    // したがって本テンプレートだけは意図的にジオメトリを持たない。
    //
    // 吸音は低吸音の剛壁のみ (mid ≈ 2.9 s)。残響時間を目標に合わせる用途では
    // なく、初期反射の到達時刻と振幅を厳密解と比べるための構成である。
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
    // 住宅の居室 4×3×2.5 m (V = 30 m³, S = 59 m²) — 隣室からの透過音と
    // 設備騒音の検討用。
    // 目標 RT60 (中音域): 0.35〜0.55 s。住宅居室の残響時間は 0.4〜0.6 s が
    // 目安 (家具・カーテンを含む一般的な内装での慣行値)。本バジェットは
    // mid ≈ 0.45 s。
    room(p, 4, 3, 2.5, 0.1, 0.5, 1.5, 1.2, 3.0, 1.5, 1.2);
    AcousticOpts &a = p.acoustic();
    a.rt60 = true;
    a.sti = false;
    a.rtFormula = 1;   // Eyring
    a.absorption = {
        // 石膏ボード + 下地空気層は膜共鳴で低音側の α が高い (典型値)。
        absRow(AbsorptionRow::Ceiling, "石膏ボード天井", 12,
               { 0.30, 0.12, 0.06, 0.05, 0.06, 0.07 }),
        absRow(AbsorptionRow::Floor, "カーペット敷き", 12,
               { 0.02, 0.06, 0.14, 0.37, 0.60, 0.65 }),
        absRow(AbsorptionRow::SideWall, "石膏ボード壁", 20,
               { 0.30, 0.12, 0.06, 0.05, 0.06, 0.07 }),
        absRow(AbsorptionRow::RearWall, "界壁・端壁", 15,
               { 0.30, 0.12, 0.06, 0.05, 0.06, 0.07 }),
        absRow(AbsorptionRow::Other, "家具・カーテン", 8,
               { 0.15, 0.35, 0.50, 0.55, 0.55, 0.50 }),
        absRow(AbsorptionRow::Air, "空気吸収 4mV", 0,
               { 0, 0, 0, 0, 0, 0 }, 0.4),   // 0.012 × 30
    };
    const double lv[7] = { 48, 44, 40, 36, 32, 28, 24 };  // NC-35 相当の現状値
    for (int i = 0; i < 7; ++i) a.noiseLevels[i] = lv[i];
    QVector<NoiseSourceRow> rows = defaultNoiseSources();
    for (NoiseSourceRow &r : rows) r.enabled = true;
    a.noiseSources = rows;
    const int gypsum = addMat(p, "石膏ボード界壁", 1.0, 0.0,
                              700.0, 1600.0, 0.05);
    box(p, gypsum, "界壁", 3.9, 4.0, 0, 3, 0, 2.5);
    // ── 障害物ジオメトリ (家具・カーテン — 上の Other 行と対応) ──
    const int furn = addMat(p, "家具 (書棚)", 1.0, 0.0, 550.0, 3300.0, 0.15);
    const int curt = addMat(p, "厚手カーテン", 1.0, 0.0, 40.0, 343.0, 0.50);
    box(p, furn, "書棚", 0.1, 0.45, 0.2, 1.8, 0.0, 1.8);
    box(p, curt, "厚手カーテン", 0.0, 0.06, 0.3, 2.7, 0.4, 2.2);
}

// ── 追加の会場テンプレート ──────────────────────────────────────────────────
// 既存の 8 種は「ホール / 事務室 / スタジオ / 屋外 / 可聴化 / レイトレース /
// 鏡像法 / 騒音」で、用途は揃っているが会場タイプの音響的な幅が狭い。
// 以下は音響設計上の要求がそれぞれ明確に異なる 5 会場を追加する:
//   映画館       — 規格に基づく短く平坦な残響 + 段床客席 (必須)
//   ライブハウス — 拡声音楽向けの中残響・低域制御 + PA/立見客 (必須)
//   体育館       — 残響過多で吸音改修が要る典型 (「対策前」を再現する)
//   教会         — 長残響側の極 (石造・オルガン/聖歌隊)
//   レストラン   — 暗騒音と会話明瞭度が支配的な小残響空間

void acCinema(Project &p)
{
    // 映画館 (シネコン 1 スクリーン) 25×16×8 m
    // (V = 3200 m³, S = 1456 m², 約 250 席)。
    // 目標 RT60 (中音域): 0.45〜0.60 s。SMPTE ST 202 / ISO 2969 系 (X カーブ)
    // の劇場では容積に応じて残響時間を抑え、3000 m³ 級で 0.5 s 前後・
    // 帯域間でできるだけ平坦にするのが慣行値。本バジェットは mid ≈ 0.50 s、
    // 125 Hz ≈ 0.76 s (低域の持ち上がりを抑えるため側壁・天井に空気層付き
    // 吸音を配して膜共鳴で低音を取る構成)。
    room(p, 25, 16, 8, 0.5, 0.8, 8.0, 3.2, 17.0, 8.0, 2.65);
    AcousticOpts &a = p.acoustic();
    a.rt60 = a.d50 = a.sti = a.edt = true;
    a.c80 = false;                 // 音楽ホール指標は使わない (音声再生室)
    a.impulseResponse = true;
    a.occupancy = 2;
    a.rtFormula = 1;               // Eyring (ᾱ が大きく Sabine は過大評価)
    a.srcDirectivity = "speaker";  // スクリーン裏センターチャンネル
    a.absorption = {
        absRow(AbsorptionRow::Audience, "客席(満席・厚手布張り)", 260,
               { 0.62, 0.72, 0.80, 0.83, 0.84, 0.85 }),
        absRow(AbsorptionRow::Floor, "通路カーペット", 140,
               { 0.02, 0.06, 0.14, 0.37, 0.60, 0.65 }),
        absRow(AbsorptionRow::Ceiling, "天井(石膏+部分GW)", 400,
               { 0.28, 0.22, 0.14, 0.11, 0.10, 0.10 }),
        absRow(AbsorptionRow::SideWall, "側壁 布張り吸音(GW50+空気層)", 400,
               { 0.35, 0.50, 0.55, 0.55, 0.58, 0.58 }),
        absRow(AbsorptionRow::RearWall, "後壁 厚手吸音(GW100)", 128,
               { 0.55, 0.80, 0.90, 0.92, 0.92, 0.88 }),
        absRow(AbsorptionRow::Other, "スクリーン壁(穴あき+背後吸音)", 128,
               { 0.40, 0.60, 0.70, 0.72, 0.72, 0.68 }),
        absRow(AbsorptionRow::Air, "空気吸収 4mV", 0,
               { 0, 0, 0, 0, 0, 0 }, 38),    // 0.012 × 3200
    };
    // ── 障害物ジオメトリ (スクリーン・段床客席・吸音パネル) ──
    const int screen = addMat(p, "穴あきスクリーン", 1.0, 0.0,
                              25.0, 343.0, 0.55);
    const int absorb = addMat(p, "GW 吸音層", 1.0, 0.0, 32.0, 343.0, 0.85);
    const int seat   = addMat(p, "客席段床ブロック", 1.0, 0.0,
                              110.0, 343.0, 0.80);
    box(p, absorb, "スクリーン裏 吸音層", 0.2, 0.5, 1.0, 15.0, 0.0, 8.0);
    box(p, screen, "スクリーン (12×5.4m)", 1.0, 1.1, 2.0, 14.0, 1.6, 7.0);
    box(p, seat, "段床客席 1列目ブロック", 6.0, 11.0, 1.5, 14.5, 0.0, 0.35);
    box(p, seat, "段床客席 2段目", 11.0, 15.0, 1.5, 14.5, 0.35, 0.90);
    box(p, seat, "段床客席 3段目", 15.0, 19.0, 1.5, 14.5, 0.90, 1.45);
    box(p, seat, "段床客席 4段目", 19.0, 23.5, 1.5, 14.5, 1.45, 2.00);
    box(p, absorb, "側壁吸音パネル (-Y)", 4.0, 24.0, 0.0, 0.2, 1.0, 6.0);
    box(p, absorb, "側壁吸音パネル (+Y)", 4.0, 24.0, 15.8, 16.0, 1.0, 6.0);
    box(p, absorb, "後壁吸音層", 24.7, 25.0, 0.0, 16.0, 0.0, 8.0);
    // 音源 = スクリーン裏のセンターチャンネル (0.8, 8, 3.2)、
    // 受音点 = 3 段目の基準受聴位置 (17, 8, 段床 1.45 + 耳高 1.2 = 2.65)。
}

void acLiveHouse(Project &p)
{
    // ライブハウス (スタンディング 300 人規模) 20×12×6 m
    // (V = 1440 m³, S = 864 m²)。
    // 目標 RT60 (中音域): 0.65〜0.95 s。拡声を伴う音楽 (amplified music) の
    // 会場は明瞭度確保のため 0.6〜1.0 s に抑えるのが慣行値で、低域が
    // 持ち上がると音が濁るため BR ≈ 1.2 以下を狙う。本バジェットは
    // mid ≈ 0.82 s、125 Hz ≈ 0.96 s (BR ≈ 1.17)。
    // 低域は合板張り (膜共鳴吸音) と天井 GW で取っている。
    room(p, 20, 12, 6, 0.25, 3.0, 6.0, 1.8, 11.0, 6.0, 1.6);
    AcousticOpts &a = p.acoustic();
    a.rt60 = a.d50 = a.sti = a.edt = true;
    a.c80 = true;                  // 拡声音楽でも明瞭度指標として見る
    a.impulseResponse = true;
    a.occupancy = 2;               // 満員 (立見)
    a.rtFormula = 1;               // Eyring
    a.srcDirectivity = "speaker";  // PA スタック
    a.srcSPL_dB = 105.0;           // ライブ時の客席平均 SPL の目安
    a.absorption = {
        // 立位の聴衆は占有床面積あたりの等価吸音率で扱う (典型値)。
        absRow(AbsorptionRow::Audience, "立見客(満員)", 130,
               { 0.30, 0.45, 0.55, 0.60, 0.62, 0.62 }),
        absRow(AbsorptionRow::Floor, "コンクリート床", 110,
               { 0.02, 0.03, 0.03, 0.04, 0.05, 0.05 }),
        absRow(AbsorptionRow::Ceiling, "天井 GW 吸音(黒塗り)", 240,
               { 0.25, 0.28, 0.28, 0.30, 0.30, 0.28 }),
        absRow(AbsorptionRow::SideWall, "合板張り(膜共鳴)", 240,
               { 0.30, 0.20, 0.14, 0.12, 0.12, 0.10 }),
        absRow(AbsorptionRow::RearWall, "後壁 吸音", 72,
               { 0.35, 0.50, 0.55, 0.58, 0.58, 0.52 }),
        absRow(AbsorptionRow::Other, "ステージまわり", 72,
               { 0.15, 0.15, 0.15, 0.15, 0.15, 0.15 }),
        absRow(AbsorptionRow::Air, "空気吸収 4mV", 0,
               { 0, 0, 0, 0, 0, 0 }, 17),    // 0.012 × 1440
    };
    // ── 障害物ジオメトリ (ステージ・PA スタック・柱・バー・バッフル) ──
    const int stageW = addMat(p, "ステージ床(木)", 1.0, 0.0,
                              600.0, 3500.0, 0.10);
    const int pa     = addMat(p, "PA スタック(木箱)", 1.0, 0.0,
                              700.0, 3300.0, 0.25);
    const int pillar = addMat(p, "鉄骨柱", 1.0, 0.0, 7800.0, 5000.0, 0.03);
    const int bar    = addMat(p, "バーカウンター", 1.0, 0.0,
                              600.0, 3300.0, 0.12);
    const int baffle = addMat(p, "天井吸音バッフル", 1.0, 0.0,
                              32.0, 343.0, 0.80);
    box(p, stageW, "ステージ (h=0.9m)", 0.5, 6.0, 1.0, 11.0, 0.0, 0.9);
    box(p, pa, "PA スタック L", 5.0, 6.0, 0.8, 2.0, 0.9, 3.2);
    box(p, pa, "PA スタック R", 5.0, 6.0, 10.0, 11.2, 0.9, 3.2);
    shape6(p, 13, pillar, "柱 (-Y側)", 11.5, 12.1, 2.0, 2.6, 0.0, 6.0);
    shape6(p, 13, pillar, "柱 (+Y側)", 11.5, 12.1, 9.4, 10.0, 0.0, 6.0);
    box(p, bar, "バーカウンター", 16.5, 18.5, 1.0, 11.0, 0.0, 1.1);
    box(p, baffle, "天井吸音バッフル", 7.0, 16.0, 2.0, 10.0, 5.5, 5.8);
    // 音源 (3,6,1.8) はステージ上 +0.9 m、受音点 (11,6,1.6) はフロア中央の
    // 立位耳高 (柱 x=11.5 の手前)。
}

void acGymnasium(Project &p)
{
    // 体育館 / 屋内運動場 36×22×9 m (V = 7128 m³, S = 2628 m²)。
    // 【対策前の現状を再現するテンプレート】
    // 目標 RT60 (中音域): 集会・拡声利用では 1.5〜2.0 s 程度が推奨される
    // (この容積級の多目的空間の慣行値)。一方、木床 + コンクリート壁 +
    // 折板屋根という一般的な仕様では吸音力が足りず mid ≈ 3.6 s になる。
    // 本テンプレートはその「未対策の現状」を再現し、
    //   追加必要吸音力 ΔA ≈ 0.161·V/T_目標 − A_現状
    // を見積もる (= 吸音改修の検討) ためのケースである。
    // したがって期待 RT60 は 3.0〜4.2 s であり、これは「良い設計値」ではない。
    room(p, 36, 22, 9, 0.5, 6.0, 11.0, 1.5, 28.0, 11.0, 1.5);
    AcousticOpts &a = p.acoustic();
    a.rt60 = a.sti = a.d50 = a.edt = true;
    a.c80 = false;
    a.impulseResponse = true;
    a.occupancy = 1;               // 集会時に半分程度
    a.rtFormula = 1;               // Eyring
    a.srcDirectivity = "speaker";  // 拡声設備
    a.absorption = {
        absRow(AbsorptionRow::Audience, "集会時の在館者(床座)", 200,
               { 0.15, 0.25, 0.40, 0.45, 0.50, 0.50 }),
        absRow(AbsorptionRow::Floor, "体育館用木床(根太組)", 592,
               { 0.15, 0.11, 0.10, 0.07, 0.06, 0.07 }),
        absRow(AbsorptionRow::Ceiling, "折板屋根(一部有孔)", 792,
               { 0.10, 0.10, 0.08, 0.08, 0.09, 0.10 }),
        absRow(AbsorptionRow::SideWall, "コンクリート壁+木製ラス腰壁", 648,
               { 0.08, 0.06, 0.05, 0.05, 0.06, 0.07 }),
        absRow(AbsorptionRow::RearWall, "妻壁(ブロック塗装)", 396,
               { 0.02, 0.02, 0.03, 0.03, 0.04, 0.05 }),
        absRow(AbsorptionRow::Air, "空気吸収 4mV", 0,
               { 0, 0, 0, 0, 0, 0 }, 86),    // 0.012 × 7128
    };
    // ── 障害物ジオメトリ (壇上・ギャラリー・器具庫) ──
    const int stageW = addMat(p, "壇上(木)", 1.0, 0.0, 600.0, 3500.0, 0.10);
    const int rc     = addMat(p, "ギャラリー床版(RC)", 1.0, 0.0,
                              2400.0, 3500.0, 0.02);
    const int rail   = addMat(p, "ギャラリー手すりパネル", 1.0, 0.0,
                              600.0, 3300.0, 0.10);
    const int store  = addMat(p, "器具庫 間仕切り", 1.0, 0.0,
                              700.0, 1600.0, 0.05);
    box(p, stageW, "壇上 (h=0.9m)", 0.5, 4.5, 6.0, 16.0, 0.0, 0.9);
    box(p, rc, "ギャラリー床版 (-Y)", 1.0, 35.0, 0.0, 1.8, 5.0, 5.4);
    box(p, rc, "ギャラリー床版 (+Y)", 1.0, 35.0, 20.2, 22.0, 5.0, 5.4);
    box(p, rail, "ギャラリー手すり (-Y)", 1.0, 35.0, 1.5, 1.8, 5.4, 6.5);
    box(p, rail, "ギャラリー手すり (+Y)", 1.0, 35.0, 20.2, 20.5, 5.4, 6.5);
    box(p, store, "器具庫", 31.5, 36.0, 0.0, 4.5, 0.0, 3.0);
    // 音源 (6,11,1.5) は壇上前の拡声スピーカ、受音点 (28,11,1.5) は
    // 反対側 (拡声の明瞭度が最も厳しい位置)。
}

void acChurch(Project &p)
{
    // 教会 / チャペル (石造・ヴォールト天井の身廊) 32×14×13 m
    // (V = 5824 m³, S = 2092 m²)。
    // 目標 RT60 (満席・中音域): 2.2〜3.0 s。パイプオルガンと聖歌隊のための
    // 礼拝堂は 2.0〜3.0 s が推奨レンジ (説教の明瞭度を優先するなら 1.8 s 級)。
    // 本バジェットは mid ≈ 2.7 s で、125 Hz は 3.9 s と低域が伸びる
    // (石壁・石床の低吸音による典型的な特性)。
    room(p, 32, 14, 13, 0.5, 3.0, 7.0, 1.6, 18.0, 7.0, 1.2);
    AcousticOpts &a = p.acoustic();
    a.rt60 = a.c80 = a.edt = true;
    a.d50 = a.sti = true;          // 説教の明瞭度も同時に見る
    a.impulseResponse = true;
    a.occupancy = 2;
    a.rtFormula = 1;               // Eyring
    a.absorption = {
        // 木製ベンチに着席した会衆 (満席) の典型値。
        absRow(AbsorptionRow::Audience, "会衆席(満席・木製ベンチ)", 250,
               { 0.57, 0.61, 0.75, 0.86, 0.91, 0.86 }),
        absRow(AbsorptionRow::Floor, "石床", 198,
               { 0.01, 0.01, 0.02, 0.02, 0.02, 0.02 }),
        absRow(AbsorptionRow::Ceiling, "ヴォールト天井(漆喰)", 448,
               { 0.02, 0.02, 0.03, 0.04, 0.05, 0.05 }),
        absRow(AbsorptionRow::SideWall, "石壁", 700,
               { 0.02, 0.02, 0.03, 0.04, 0.05, 0.05 }),
        // 板ガラスは低音側で膜共鳴により吸音する (典型値)。
        absRow(AbsorptionRow::Other, "ステンドグラス窓", 132,
               { 0.35, 0.25, 0.18, 0.12, 0.07, 0.04 }),
        absRow(AbsorptionRow::RearWall, "妻壁(石)", 364,
               { 0.02, 0.02, 0.03, 0.04, 0.05, 0.05 }),
        absRow(AbsorptionRow::Air, "空気吸収 4mV", 0,
               { 0, 0, 0, 0, 0, 0 }, 70),    // 0.012 × 5824
    };
    // ── 障害物ジオメトリ (祭壇・会衆席・列柱・オルガンギャラリー) ──
    const int altar = addMat(p, "祭壇壇上(石)", 1.0, 0.0,
                             2600.0, 4000.0, 0.02);
    const int pew   = addMat(p, "会衆席(木製ベンチ)", 1.0, 0.0,
                             600.0, 3300.0, 0.30);
    const int col   = addMat(p, "石柱", 1.0, 0.0, 2600.0, 4000.0, 0.02);
    const int gall  = addMat(p, "オルガンギャラリー床版", 1.0, 0.0,
                             600.0, 3300.0, 0.10);
    box(p, altar, "祭壇壇上 (h=0.6m)", 0.5, 5.0, 4.0, 10.0, 0.0, 0.6);
    box(p, pew, "会衆席ブロック (-Y)", 8.0, 27.0, 1.5, 6.2, 0.0, 1.0);
    box(p, pew, "会衆席ブロック (+Y)", 8.0, 27.0, 7.8, 12.5, 0.0, 1.0);
    for (int i = 0; i < 3; ++i) {
        const double cx = 12.0 + 6.0 * i;   // x = 12 / 18 / 24
        shape6(p, 13, col, "列柱 (-Y)", cx - 0.45, cx + 0.45,
               0.55, 1.45, 0.0, 10.0);
        shape6(p, 13, col, "列柱 (+Y)", cx - 0.45, cx + 0.45,
               12.55, 13.45, 0.0, 10.0);
    }
    box(p, gall, "オルガンギャラリー", 27.5, 32.0, 1.0, 13.0, 7.5, 7.9);
    // 音源 (3,7,1.6) は祭壇上の説教者、受音点 (18,7,1.2) は中央通路
    // (y = 6.2〜7.8) の着席耳高。
}

void acRestaurant(Project &p)
{
    // レストラン / カフェ 15×10×3.2 m (V = 480 m³, S = 460 m², 約 60 席)。
    // 目標 RT60 (中音域): 0.35〜0.55 s。飲食店では残響時間を 0.6 s 以下に
    // 抑えないと、話者が周囲の騒音に負けまいと声を張る (ロンバード効果) 悪循環で
    // 暗騒音が上がり会話明瞭度が落ちる、というのが設計上の慣行。
    // 本バジェットは mid ≈ 0.42 s。
    // 暗騒音は空調 + 厨房 + 客のざわめきを想定した NC-40 級の現状値を入れる。
    room(p, 15, 10, 3.2, 0.2, 5.5, 3.7, 1.2, 7.0, 3.7, 1.2);
    AcousticOpts &a = p.acoustic();
    a.rt60 = a.d50 = a.sti = true;
    a.c80 = false;
    a.edt = true;
    a.impulseResponse = true;
    a.occupancy = 2;
    a.rtFormula = 0;               // Sabine (小空間)
    a.srcSPL_dB = 70.0;            // 通常会話 (1 m) の目安
    a.absorption = {
        absRow(AbsorptionRow::Ceiling, "岩綿吸音板+背後空気層", 150,
               { 0.35, 0.55, 0.70, 0.75, 0.80, 0.75 }),
        absRow(AbsorptionRow::Floor, "タイル床", 60,
               { 0.02, 0.03, 0.04, 0.05, 0.05, 0.05 }),
        absRow(AbsorptionRow::Other, "在席客・ブース席", 90,
               { 0.25, 0.35, 0.45, 0.50, 0.52, 0.50 }),
        absRow(AbsorptionRow::SideWall, "塗装壁+ガラス面", 96,
               { 0.20, 0.12, 0.08, 0.06, 0.05, 0.05 }),
        absRow(AbsorptionRow::RearWall, "端壁+カーテン", 64,
               { 0.20, 0.25, 0.30, 0.32, 0.32, 0.30 }),
        absRow(AbsorptionRow::Air, "空気吸収 4mV", 0,
               { 0, 0, 0, 0, 0, 0 }, 6),     // 0.012 × 480
    };
    // NC-40 相当の現状暗騒音 (63Hz〜4kHz)。飲食店の暗騒音は NC-35〜45 が
    // 一般的な実態値で、NC-40 は「やや騒がしい」水準。
    const double lv[7] = { 56, 50, 45, 41, 38, 36, 34 };
    for (int i = 0; i < 7; ++i) a.noiseLevels[i] = lv[i];
    // ── 障害物ジオメトリ (カウンター・ブース間仕切り・テーブル・厨房壁) ──
    const int counter = addMat(p, "バーカウンター(木)", 1.0, 0.0,
                               600.0, 3300.0, 0.12);
    const int booth   = addMat(p, "ブース席 背もたれ(布張り)", 1.0, 0.0,
                               90.0, 343.0, 0.55);
    const int table   = addMat(p, "テーブル天板", 1.0, 0.0,
                               650.0, 3300.0, 0.08);
    const int kitchen = addMat(p, "厨房間仕切り壁", 1.0, 0.0,
                               700.0, 1600.0, 0.05);
    box(p, counter, "バーカウンター", 0.4, 2.0, 1.0, 9.0, 0.0, 1.1);
    for (int i = 0; i < 3; ++i) {
        const double y = 2.4 + 2.5 * i;     // y = 2.4 / 4.9 / 7.4
        box(p, booth, "ブース間仕切り (h=1.4m)", 4.0, 9.0, y, y + 0.1,
            0.0, 1.4);
    }
    box(p, table, "テーブル (音源席)", 5.0, 7.0, 3.0, 4.4, 0.7, 0.78);
    box(p, table, "テーブル (隣席)", 5.0, 7.0, 5.5, 6.9, 0.7, 0.78);
    box(p, kitchen, "厨房間仕切り壁", 12.5, 12.6, 0.0, 7.0, 0.0, 3.2);
    // 音源 (5.5,3.7,1.2) と受音点 (7.0,3.7,1.2) はテーブル越し 1.5 m の
    // 対面会話 (STI 評価の基本配置)。
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
    { "acoustic", "ac_cinema",      "映画館 25×16×8m (RT 0.5s)", acCinema },
    { "acoustic", "ac_livehouse",   "ライブハウス 20×12×6m (RT 0.8s)", acLiveHouse },
    { "acoustic", "ac_gym",         "体育館 36×22×9m (未対策・残響過多)", acGymnasium },
    { "acoustic", "ac_church",      "教会・チャペル 32×14×13m (RT 2.7s)", acChurch },
    { "acoustic", "ac_restaurant",  "レストラン・カフェ 15×10×3.2m", acRestaurant },

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
