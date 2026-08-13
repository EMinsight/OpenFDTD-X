// Viewport3D.cpp
#include "Viewport3D.h"
#include "../core/ComponentCatalog.h"   // 部品→ドメイン許可表 (ComponentsTab と共有)
#include "../core/Project.h"
#include "../io/BellhopIO.h"
#include "../io/H5Reader.h"    // seriesSliceAxes (面内 2 軸の対応の唯一の出所)
#include "../io/SlicePieces.h" // 直交断面を互いの交線で切り分ける
#include "../core/AimDirection.h"
#include "../I18n.h"
#include "FieldHeatmap.h"     // jet カラーマップ (2D 断面表示と同じ配色)

#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFontMetrics>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QRectF>
#include <QTimer>
#include <QTransform>
#include <QWheelEvent>
#include <algorithm>   // std::clamp (レイ反射の位置クランプ)
#include <cmath>

using namespace ofd;

namespace {
const bool s_i18n = [] {
    // 接頭辞は CenterPane と共通の vp_ (3D ビュー系の語彙)。
    ofd::I18n::reg("vp_fld_none",
        "結果未読込 — 計算を実行するか結果 HDF5 を開いてください",
        "No result loaded — run the solver or open a result HDF5 file");
    ofd::I18n::reg("vp_fld_none_hint",
        "偽の界分布は表示しません",
        "No synthetic field pattern is drawn here");
    ofd::I18n::reg("vp_fld_real",
        "結果断面 (ソルバ出力の実データ)",
        "Result slice (actual solver output)");
    // 縦倍率は掛けたときに必ず出す (縮尺が等方でないことの明示)
    ofd::I18n::reg("vp3_vexag",
        "深さ方向 %1 倍で表示しています (縮尺は等方ではありません)",
        "Depth is exaggerated %1x — the scale is not isotropic");
    ofd::I18n::reg("vp_fld_norm",
        "正規化 |値| (最大 %1)",
        "normalised |value| (max %1)");
    ofd::I18n::reg("vp_fld_decim",
        "表示のみ %1 セル毎に間引き",
        "display decimated: every %1 cells");
    // 再生コントロールの状況 (H5アニメ) — 2D と同じコマを見ていることの明示
    ofd::I18n::reg("vp_fld_playing",
        "再生中  コマ %1 / %2",
        "playing  frame %1 / %2");
    ofd::I18n::reg("vp_fld_paused",
        "停止中  コマ %1 / %2",
        "paused  frame %1 / %2");
    // 弱い値を透かしていることの明示 (消えているのではない)
    ofd::I18n::reg("vp_fld_alpha",
        "弱いところほど透過 (値は 2D 断面・CSV で読む)",
        "weaker values are more transparent (read values in the 2D slice / CSV)");
    ofd::I18n::reg("vp_rays_sample",
        "サンプル表示 — ソルバ結果ではありません (24本 × 4反射)",
        "Sample display — not solver results (24 rays x 4 bounces)");
    // ── ドラッグ&ドロップ配置 ────────────────────────────────────────────
    ofd::I18n::reg("vp_drop_floor",
        "床面 z = %1 m 上", "on the floor plane z = %1 m");
    ofd::I18n::reg("vp_drop_viewplane",
        "視線に垂直な中心面上 (床面と交わらないため)",
        "on the view-perpendicular centre plane (the floor is not hit)");
    ofd::I18n::reg("vp_drop_added",
        "%1 を配置しました — %2   %3",
        "Placed %1 — %2   %3");
    ofd::I18n::reg("vp_drop_geom",
        "形状ユニット #%1 (%2)", "geometry unit #%1 (%2)");
    ofd::I18n::reg("vp_drop_feed",
        "給電点 #%1 (Z 方向)", "feed #%1 (Z direction)");
    ofd::I18n::reg("vp_drop_probe",
        "観測点 #%1 (Z 方向)", "observation point #%1 (Z direction)");
    // 室内音響: スピーカー/マイクは .ofd の feed/point に加えて .ofdx の
    // 音源リスト/受音点リストにも行を追加する (片方だけだと
    // 音源/WAV/指向性タブに反映されず混乱する)
    ofd::I18n::reg("vp_drop_synced_src",
        " + 音源リストへ「%1」を追加", " + added \"%1\" to the source list");
    ofd::I18n::reg("vp_drop_synced_rcv",
        " + 受音点リストへ「%1」を追加",
        " + added \"%1\" to the receiver list");
    // 室内音響: 床面ちょうどに置くと床スラブ・客席ブロック・舞台の内部に
    // 入ってソルバーが「剛体内の点」で失敗する。実務の高さへ持ち上げる
    // (音源 1.5 m / 受音点 1.2 m = ISO 3382-1 の座位耳高さ)
    ofd::I18n::reg("vp_drop_ac_height",
        "※ 室内音響の慣習に合わせて高さ z = %1 m へ配置しました "
        "(音源 1.5 m / 受音点 1.2 m — 床や客席ブロックの内部を避けるため)",
        "* Placed at z = %1 m per room-acoustics practice "
        "(source 1.5 m / receiver 1.2 m — keeps it out of the floor slab "
        "and audience blocks)");
    ofd::I18n::reg("vp_drop_nomesh",
        "メッシュ領域が未定義のため配置できません (メッシュタブで領域を設定"
        "してください)",
        "No mesh region is defined, so nothing can be placed (define it in "
        "the Mesh tab)");
    ofd::I18n::reg("vp_drop_r_verts",
        "%1 は頂点座標の指定が必要なため、ドロップでは配置できません "
        "(形状タブで作成してください)",
        "%1 needs explicit vertex coordinates, so it cannot be placed by "
        "drag & drop (create it in the Geometry tab)");
    ofd::I18n::reg("vp_drop_r_pos",
        "%1 は位置を持たない設定のため、ドロップでは配置できません "
        "(波源タブで設定してください)",
        "%1 has no position, so it cannot be placed by drag & drop "
        "(set it in the Source tab)");
    ofd::I18n::reg("vp_drop_r_file",
        "%1 はファイルの取込が必要なため、ドロップでは配置できません "
        "(形状タブ / レイアウトタブから取り込んでください)",
        "%1 requires importing a file, so it cannot be placed by drag & drop "
        "(import it from the Geometry / Layout tab)");
    // 吸音系コンポーネント (Absorber panel / Diffuser / Audience block) の注記:
    // 形状は剛体としてしか置かれず、吸音率はここでは設定されない
    // (絶対規則 5: 出来ていないことを出来たように見せない)
    ofd::I18n::reg("vp_drop_rigid",
        "※ 形状は剛体として配置されます — 吸音率は室内音響タブの"
        "面別吸音率で設定してください",
        "Note: the shape is placed as a rigid body — set its absorption via "
        "the per-surface absorption coefficients in the Room Acoustics tab");
    ofd::I18n::reg("vp_drop_r_area",
        "%1 は点ではなく面/領域の指定が必要なため、ドロップでは配置できません "
        "(該当タブで設定してください)",
        "%1 needs a surface/region rather than a point, so it cannot be "
        "placed by drag & drop (set it in the corresponding tab)");
    // ── ドメイン許可表による拒否 (core/ComponentCatalog.h) ──────────────
    ofd::I18n::reg("vp_drop_r_uw",
        "水中音響 (BELLHOP) は海洋環境タブの SSP・海底・ソナー設定から入力を"
        "生成するため、配置部品は計算に使われません",
        "Underwater acoustics (BELLHOP) builds its input from the SSP / "
        "seabed / sonar settings in the Ocean Environment tab, so placed "
        "components are not used in the computation");
    ofd::I18n::reg("vp_drop_r_domain",
        "%1 はこのドメインの計算では使われないため配置できません",
        "%1 is not used by this domain's computation, so it cannot be "
        "placed");
    // 形状コード → 表示名 (本家 sol/ingeometry.c の形状)
    ofd::I18n::reg("vp_drop_s1",  "直方体",   "box");
    ofd::I18n::reg("vp_drop_s2",  "楕円体",   "ellipsoid");
    ofd::I18n::reg("vp_drop_s11", "円柱 X",   "X cylinder");
    ofd::I18n::reg("vp_drop_s12", "円柱 Y",   "Y cylinder");
    ofd::I18n::reg("vp_drop_s13", "円柱 Z",   "Z cylinder");
    ofd::I18n::reg("vp_drop_s33", "三角柱 Z", "Z triangular pillar");
    ofd::I18n::reg("vp_drop_s43", "角錐 Z",   "Z pyramid");
    ofd::I18n::reg("vp_drop_s53", "円錐台 Z", "Z truncated cone");
    return true;
}();

// 断面の固定軸 (0=X, 1=Y, 2=Z) → 表示用の軸名
const char *sliceAxisName(int axis)
{
    return (axis == 0) ? "X" : (axis == 1) ? "Y" : "Z";
}

// ── コンポーネントのドロップ配置仕様 ────────────────────────────────────────
// ドロップ 1 回で作れるのは「位置と既定寸法だけで決まる要素」に限る。
// 頂点列やファイル取込が要るもの (多角形 / STL / GDS など) は作らずに
// 理由を出す (CLAUDE.md 絶対規則 5: 出来ないことを出来たように見せない)。
enum class DropKind { Shape, Feed, Probe, Unsupported };

struct DropSpec {
    DropKind    kind  = DropKind::Shape;
    int         shape = 1;      // Geometry::shape (本家 sol/ingeometry.c)
    double      fx = 1, fy = 1, fz = 1;   // 既定寸法 (基準寸法) に対する倍率
    const char *reasonKey = nullptr;      // Unsupported のときの理由キー
};

struct NamedSpec { const char *name; DropSpec spec; };

// コンポーネント名 (ComponentsTab の kComponents と同一) 別の配置仕様。
// 表に無い名前はカテゴリ既定 (catDefaultSpec) を使う。
const NamedSpec kNamedSpecs[] = {
    // ── 基本形状 ──
    { "Rectangle",             { DropKind::Shape, 1,  1.0, 1.0, 1.0 } },
    { "Circle/Disk",           { DropKind::Shape, 13, 1.0, 1.0, 0.2 } },
    { "Sphere",                { DropKind::Shape, 2,  1.0, 1.0, 1.0 } },
    { "Pyramid",               { DropKind::Shape, 43, 1.0, 1.0, 1.0 } },
    { "Triangle",              { DropKind::Shape, 33, 1.0, 1.0, 0.2 } },
    { "Polygon",               { DropKind::Unsupported, 0, 1, 1, 1,
                                 "vp_drop_r_verts" } },
    { "Spline",                { DropKind::Unsupported, 0, 1, 1, 1,
                                 "vp_drop_r_verts" } },
    // ── フォトニクス (導波路系は伝搬方向 x に長い薄板を既定にする) ──
    { "Waveguide (rib)",       { DropKind::Shape, 1,  3.0, 0.6, 0.3 } },
    { "Ring resonator",        { DropKind::Shape, 13, 1.0, 1.0, 0.3 } },
    { "Bragg grating (DBR)",   { DropKind::Shape, 1,  2.0, 0.6, 0.3 } },
    { "Y-branch splitter",     { DropKind::Shape, 1,  2.0, 1.0, 0.3 } },
    { "Directional coupler",   { DropKind::Shape, 1,  2.0, 1.0, 0.3 } },
    { "MMI splitter",          { DropKind::Shape, 1,  2.0, 1.0, 0.3 } },
    { "Photonic crystal",      { DropKind::Shape, 1,  1.0, 1.0, 0.3 } },
    { "Grating coupler",       { DropKind::Shape, 1,  1.0, 1.0, 0.3 } },
    { "MZI",                   { DropKind::Shape, 1,  3.0, 1.0, 0.3 } },
    { "Quantum dot",           { DropKind::Shape, 2,  0.3, 0.3, 0.3 } },
    // ── 金属・プラズモニクス ──
    { "Nanoparticle (Au/Ag)",  { DropKind::Shape, 2,  1.0, 1.0, 1.0 } },
    { "Nanorod",               { DropKind::Shape, 13, 0.3, 0.3, 1.0 } },
    { "Nanowire grid",         { DropKind::Shape, 1,  1.0, 1.0, 0.2 } },
    { "Bow-tie antenna",       { DropKind::Shape, 1,  1.0, 0.6, 0.2 } },
    // ── レンズ (曲面は未対応なので楕円体/円柱で外形を置く) ──
    { "Plano-convex lens",     { DropKind::Shape, 2,  1.0, 1.0, 0.4 } },
    { "Biconvex lens",         { DropKind::Shape, 2,  1.0, 1.0, 0.5 } },
    { "Aspheric lens",         { DropKind::Shape, 2,  1.0, 1.0, 0.4 } },
    { "Metalens",              { DropKind::Shape, 13, 1.0, 1.0, 0.15 } },
    { "GRIN lens",             { DropKind::Shape, 13, 1.0, 1.0, 1.0 } },
    { "Mirror",                { DropKind::Shape, 1,  1.0, 1.0, 0.1 } },
    { "Aperture / Stop",       { DropKind::Shape, 1,  1.0, 1.0, 0.1 } },
    // ── アンテナ ──
    { "Dipole",                { DropKind::Shape, 13, 0.1, 0.1, 1.0 } },
    { "Patch antenna",         { DropKind::Shape, 1,  1.0, 1.0, 0.1 } },
    { "Horn",                  { DropKind::Shape, 53, 1.0, 1.0, 1.0 } },
    { "Helix",                 { DropKind::Shape, 13, 0.5, 0.5, 1.5 } },
    { "Yagi-Uda",              { DropKind::Shape, 1,  1.5, 1.0, 0.1 } },
    { "Array (8×8)",           { DropKind::Shape, 1,  2.0, 2.0, 0.1 } },
    // ── 音響 (スピーカー/マイクは形状ではなく波源・観測点として置く) ──
    { "Loudspeaker",           { DropKind::Feed } },
    { "Microphone",            { DropKind::Probe } },
    { "Absorber panel",        { DropKind::Shape, 1,  1.0, 1.0, 0.15 } },
    { "Diffuser (QRD)",        { DropKind::Shape, 1,  1.0, 1.0, 0.3 } },
    { "Audience block",        { DropKind::Shape, 1,  2.0, 2.0, 0.5 } },
    // ── 波源 (点として置けないものは理由を出す) ──
    { "Plane wave",            { DropKind::Unsupported, 0, 1, 1, 1,
                                 "vp_drop_r_pos" } },
    { "TFSF (全/散乱場)",      { DropKind::Unsupported, 0, 1, 1, 1,
                                 "vp_drop_r_area" } },
    { "Import source",         { DropKind::Unsupported, 0, 1, 1, 1,
                                 "vp_drop_r_file" } },
    // ── モニター (.ofd が保持できるのは点観測 "point =" のみ) ──
    { "Point monitor",         { DropKind::Probe } },
    { "Time monitor",          { DropKind::Probe } },
};

// カテゴリ既定。格子は薄板、波源は給電点、モニターは面/領域指定が要るので
// 既定では作らない (点モニターだけ上の表で Probe にしている)。
DropSpec catDefaultSpec(const QString &cat)
{
    if (cat == QLatin1String("source"))
        return { DropKind::Feed };
    if (cat == QLatin1String("monitor"))
        return { DropKind::Unsupported, 0, 1, 1, 1, "vp_drop_r_area" };
    if (cat == QLatin1String("imported"))
        return { DropKind::Unsupported, 0, 1, 1, 1, "vp_drop_r_file" };
    if (cat == QLatin1String("grating"))
        return { DropKind::Shape, 1, 1.0, 1.0, 0.2 };   // 周期構造は薄板
    return { DropKind::Shape, 1, 1.0, 1.0, 1.0 };
}

DropSpec dropSpecFor(const QString &cat, const QString &name)
{
    // 名前には非 ASCII を含むもの ("TFSF (全/散乱場)" 等) があるので
    // QLatin1String 比較は使えない (バイト列を Latin-1 と解釈して不一致になる)
    for (const NamedSpec &ns : kNamedSpecs)
        if (name == QString::fromUtf8(ns.name)) return ns.spec;
    return catDefaultSpec(cat);
}

QString shapeLabel(int shape)
{
    switch (shape) {
        case 1:  return ofd::I18n::tr("vp_drop_s1");
        case 2:  return ofd::I18n::tr("vp_drop_s2");
        case 11: return ofd::I18n::tr("vp_drop_s11");
        case 12: return ofd::I18n::tr("vp_drop_s12");
        case 13: return ofd::I18n::tr("vp_drop_s13");
        case 33: return ofd::I18n::tr("vp_drop_s33");
        case 43: return ofd::I18n::tr("vp_drop_s43");
        case 53: return ofd::I18n::tr("vp_drop_s53");
    }
    return QString::number(shape);
}

// 配置先の外接直方体 (中心 = ドロップ位置、寸法 = 基準寸法 × 倍率)
void dropBounds(const DropSpec &sp, const double pos[3], double base,
                double lo[3], double hi[3])
{
    const double f[3] = { sp.fx, sp.fy, sp.fz };
    for (int a = 0; a < 3; ++a) {
        const double h = base * f[a] / 2.0;
        lo[a] = pos[a] - h;
        hi[a] = pos[a] + h;
    }
}

QString posText(const double p[3])
{
    return QStringLiteral("x=%1  y=%2  z=%3 [m]")
        .arg(QString::number(p[0], 'g', 4),
             QString::number(p[1], 'g', 4),
             QString::number(p[2], 'g', 4));
}
} // namespace

// ── ComponentDrop (ComponentsTab と共有するドラッグ&ドロップ契約) ───────────
const char *ofd::ComponentDrop::mimeType()
{
    return "application/x-openfdtd-component";
}

QByteArray ofd::ComponentDrop::encode(const QString &cat, const QString &name)
{
    return (cat + QLatin1Char('\t') + name).toUtf8();
}

bool ofd::ComponentDrop::decode(const QByteArray &data, QString *cat,
                                QString *name)
{
    const QString s = QString::fromUtf8(data);
    const int tab = s.indexOf(QLatin1Char('\t'));
    if (tab <= 0 || tab + 1 >= s.size()) return false;
    if (cat)  *cat  = s.left(tab);
    if (name) *name = s.mid(tab + 1);
    return true;
}

bool ofd::ComponentDrop::canPlace(const QString &cat, const QString &name,
                                  QString *why)
{
    const DropSpec sp = dropSpecFor(cat, name);
    if (sp.kind != DropKind::Unsupported) return true;
    if (why)
        *why = I18n::tr(QLatin1String(sp.reasonKey)).arg(name);
    return false;
}

// ドメイン付き判定 — 許可表 (core/ComponentCatalog.h) を先に確認する。
// ComponentsTab のドラッグ開始側と Viewport3D のドロップ側の両方がこれを
// 使う (お気に入りチップ経由のドラッグも同じ判定を通る)。
bool ofd::ComponentDrop::canPlace(const QString &cat, const QString &name,
                                  const QString &domain, QString *why)
{
    // 水中音響: BELLHOP は配置部品を一切使わないので全部品を理由付きで拒否
    if (domain == QLatin1String("underwater")) {
        if (why) *why = I18n::tr("vp_drop_r_uw");
        return false;
    }
    if (!domain.isEmpty() && !ComponentCatalog::allowedInDomain(name, domain)) {
        if (why) *why = I18n::tr("vp_drop_r_domain").arg(name);
        return false;
    }
    return canPlace(cat, name, why);
}

Viewport3D::Viewport3D(Project *project, QWidget *parent)
    : QWidget(parent), m_project(project)
{
    setObjectName("Viewport3D");
    setMinimumSize(320, 240);
    setMouseTracking(false);
    setAutoFillBackground(false);
    // コンポーネントライブラリのカードを受け取る (ComponentDrop の MIME)
    setAcceptDrops(true);
    connect(project, &Project::changed, this, qOverload<>(&QWidget::update));
    connect(project, &Project::loaded, this, qOverload<>(&QWidget::update));
    // Field は実データの静止断面を描くだけなのでアニメーション用タイマーは
    // 持たない (ヘッドレス/リモートで CPU を回さない)。
}

void Viewport3D::setViewStyle(ViewStyle s)
{
    if (m_viewStyle == s) return;
    m_viewStyle = s;
    m_solid = (s != ViewStyle::Wireframe);
    update();
}

void Viewport3D::setResultSlice(const QVector<double> &cells, int rows,
                                int cols, int axis, double pos_m,
                                double u0, double u1, double v0, double v1,
                                const QString &label, double scaleMax)
{
    SliceSpec s;
    s.cells = cells;
    s.rows = rows; s.cols = cols;
    s.axis = axis; s.pos_m = pos_m;
    s.u0 = u0; s.u1 = u1; s.v0 = v0; s.v1 = v1;
    s.label = label;
    s.scaleMax = scaleMax;
    QVector<SliceSpec> one;
    one.push_back(s);
    setResultSlices(one);
}

void Viewport3D::setResultSlices(const QVector<SliceSpec> &specs)
{
    QVector<Slice> keep;
    QStringList labels;
    // 正規化は「与えられた実データの最大値」で行う (勝手な下駄を履かせない)。
    // scaleMax > 0 なら呼び側の指定を使う — 連続するフレームを共通の尺度で
    // 見せたいときに要る (フレームごとの最大値で割ると時間変化が消える)。
    // **複数断面では全断面で 1 つの値に揃える** (カラーバーが 1 本しかない
    // ので、面ごとに違う尺度で塗ると同じ色が違う強さを意味してしまう)。
    double smax = 0.0;
    bool given = false;
    for (const SliceSpec &sp : specs) {
        const qint64 need = qint64(sp.rows) * qint64(sp.cols);
        if (sp.rows <= 0 || sp.cols <= 0 || qint64(sp.cells.size()) < need)
            continue;                                    // 壊れた指定は捨てる
        if (sp.scaleMax > 0.0 && std::isfinite(sp.scaleMax)) {
            given = true;
            smax = std::max(smax, sp.scaleMax);
        }
        Slice s;
        s.cells = sp.cells;
        s.rows = sp.rows; s.cols = sp.cols;
        s.axis = qBound(0, sp.axis, 2);
        s.pos = sp.pos_m;
        s.u0 = sp.u0; s.u1 = sp.u1; s.v0 = sp.v0; s.v1 = sp.v1;
        s.label = sp.label;
        keep.push_back(s);
        if (!sp.label.isEmpty()) labels << sp.label;
    }
    if (keep.isEmpty()) { clearResultSlice(); return; }

    if (!given) {
        for (const Slice &s : keep) {
            const qint64 need = qint64(s.rows) * qint64(s.cols);
            for (qint64 i = 0; i < need; ++i) {
                const double v = std::fabs(s.cells[int(i)]);
                if (std::isfinite(v) && v > smax) smax = v;
            }
        }
    }
    m_slices = keep;
    m_sliceMax = smax;
    m_sliceLabel = labels.join(QStringLiteral(" / "));
    rebuildSliceImages();
    update();
}

void Viewport3D::setSlicePlayback(int frame, int frameCount, bool playing)
{
    if (m_sliceFrame == frame && m_sliceFrameCount == frameCount
        && m_slicePlaying == playing) return;
    m_sliceFrame = frame;
    m_sliceFrameCount = frameCount;
    m_slicePlaying = playing;
    update();
}

void Viewport3D::clearResultSlice()
{
    if (!hasResultSlice() && m_sliceLabel.isEmpty()) return;
    m_slices.clear();
    m_sliceMax = 0.0;
    m_sliceLabel.clear();
    m_sliceDecim = 1;
    m_sliceFrame = m_sliceFrameCount = 0;
    m_slicePlaying = false;
    update();
}

void Viewport3D::fitView()
{
    m_zoom = 1.0;
    m_panPx = QPointF();
    update();
}

void Viewport3D::setAzimuth(double deg)
{
    if (qFuzzyCompare(m_azimuthDeg, deg)) return;
    m_azimuthDeg = deg;
    update();
}

void Viewport3D::setElevation(double deg)
{
    const double v = qBound(-89.0, deg, 89.0);
    if (qFuzzyCompare(m_elevationDeg, v)) return;
    m_elevationDeg = v;
    update();
}

// モックの [XY][YZ][ZX] 軸タグ相当: 正射影で各主平面を正面に向ける
void Viewport3D::setDomain(Domain d)
{
    const bool entering = (d == Domain::Underwater && m_domain != d);
    m_domain = d;
    // 画面 y に z が乗る向き (projectPoint: el = 90° で y2 = −dz) にする。
    // az = 0 / el = 0 は x-y が画面に乗る向きなので、y ≡ 0 の海は**線に
    // しか見えない**。ツールバーの視点ボタン 0 と同じ角度を使う。
    if (entering) setViewPlane(0);
    update();
}

void Viewport3D::setVerticalExaggeration(double k)
{
    const double v = std::max(1.0, std::min(k, 200.0));
    if (v == m_vScale) return;
    m_vScale = v;
    update();
}

void Viewport3D::setViewPlane(int plane)
{
    switch (plane) {
    case 0: m_azimuthDeg =   0; m_elevationDeg =  89; break;  // XY (上から)
    case 1: m_azimuthDeg =  90; m_elevationDeg =   0; break;  // YZ (X軸方向)
    case 2: m_azimuthDeg =   0; m_elevationDeg =   0; break;  // ZX (Y軸方向)
    default: return;
    }
    update();
    emit viewChanged(m_azimuthDeg, m_elevationDeg);
}

// メッシュ領域の範囲。1 軸も広がりが無ければ既定の箱を入れて false を返す
// (paintEvent の従来の挙動そのまま — 空プロジェクトでも軸が描けるように)。
// 水中音響のシーン範囲 — .ofd のメッシュではなく**海**で決まる。
// x = 距離 [m] (受波器距離の範囲)、z = 深度を下向き負、y は 0 のまま
// (BELLHOP の 2D 解に横の広がりは無いので、厚みのある箱にしない)。
bool Viewport3D::oceanBounds(double lo[3], double hi[3]) const
{
    const UnderwaterOpts &u = m_project->underwater();
    const double x0 = u.tlRangeMin_km * 1000.0;
    const double x1 = u.rangeMax_km * 1000.0;
    // **深さは .env を書くのと同じ関数から取る** — 別に数え直すと画面と
    // カーネル入力が食い違う。
    const double depth = BellhopIO::bottomDepth(u);
    if (!(x1 > x0) || !(depth > 0.0)) return false;
    lo[0] = x0;    hi[0] = x1;
    lo[1] = 0.0;   hi[1] = 0.0;       // 面 (厚みを持たせない)
    // 縦は表示倍率を掛けた後の値で囲う (掛けた分だけ画面に収める)
    lo[2] = -depth * m_vScale; hi[2] = 0.0;   // 海面 z = 0、海底 z = −水深
    return true;
}

bool Viewport3D::sceneBounds(double lo[3], double hi[3]) const
{
    if (m_domain == Domain::Underwater && oceanBounds(lo, hi)) return true;
    bool any = false;
    for (int a = 0; a < 3; ++a) {
        const MeshAxis &ax = m_project->mesh(a);
        lo[a] = ax.min(); hi[a] = ax.max();
        if (hi[a] > lo[a]) any = true;
    }
    if (!any) { lo[0]=lo[1]=lo[2]=-0.5; hi[0]=hi[1]=hi[2]=0.5; }
    return any;
}

// projectPoint が使う中心と縮尺を現在の状態から求める。paintEvent と
// ドロップの逆変換で同じ値を使う (描画前でも逆変換が正しく効くように)。
void Viewport3D::updateSceneTransform() const
{
    double lo[3], hi[3];
    sceneBounds(lo, hi);
    m_cx = (lo[0] + hi[0]) / 2;
    m_cy = (lo[1] + hi[1]) / 2;
    m_cz = (lo[2] + hi[2]) / 2;
    const double ext = std::max({ hi[0]-lo[0], hi[1]-lo[1], hi[2]-lo[2], 1e-12 });
    m_scale = 0.55 * std::min(width(), height()) / ext * m_zoom;
}

// 3 軸すべてに広がりがあるか (ドロップ配置の前提)
bool Viewport3D::meshDefined() const
{
    for (int a = 0; a < 3; ++a)
        if (!(m_project->mesh(a).max() > m_project->mesh(a).min()))
            return false;
    return true;
}

// ドロップで作る要素の基準寸法 = メッシュ最小スパンの 1/10 [m]
double Viewport3D::defaultSize() const
{
    double s = 0.0;
    for (int a = 0; a < 3; ++a) {
        const double d = m_project->mesh(a).max() - m_project->mesh(a).min();
        if (d > 0.0) s = (s == 0.0) ? d : std::min(s, d);
    }
    return s * 0.1;
}

QPointF Viewport3D::projectPoint(double x, double y, double z) const
{
    // center + rotate (azimuth around Z, then elevation around screen-X)
    const double az = m_azimuthDeg  * M_PI / 180.0;
    const double el = m_elevationDeg * M_PI / 180.0;
    const double dx = (x - m_cx) * m_scale;
    const double dy = (y - m_cy) * m_scale;
    const double dz = (z - m_cz) * m_scale;

    const double x1 =  dx * std::cos(az) + dy * std::sin(az);
    const double y1 = -dx * std::sin(az) + dy * std::cos(az);
    const double y2 =  y1 * std::cos(el) - dz * std::sin(el);
    // screen: x right, y down
    return QPointF(width()  / 2.0 + m_panPx.x() + x1,
                   height() / 2.0 + m_panPx.y() + y2);
}

// projectPoint と同じ基底の第 3 軸 e3 = (−sinA·sinE, cosA·sinE, cosE)。
// 正射影なので画面座標からは決まらない奥行きをここで出す。**大きいほど
// 手前** — el = 0 (真上から) では e3 = (0,0,1) で、z が大きいほど視点に
// 近いことから向きが決まる。
double Viewport3D::sceneDepth(double x, double y, double z) const
{
    const double az = m_azimuthDeg   * M_PI / 180.0;
    const double el = m_elevationDeg * M_PI / 180.0;
    const double dx = (x - m_cx) * m_scale;
    const double dy = (y - m_cy) * m_scale;
    const double dz = (z - m_cz) * m_scale;
    return -dx * std::sin(az) * std::sin(el)
         +  dy * std::cos(az) * std::sin(el)
         +  dz * std::cos(el);
}

// 画面座標 → シーン座標 (projectPoint の逆変換)。
//
// projectPoint の回転は正規直交基底で書ける。縮尺後の相対座標
// (u, v, w) = ((x-cx), (y-cy), (z-cz)) * scale に対して
//   x1 (画面右)   = e1・(u,v,w),  e1 = ( cosA,          sinA,         0     )
//   y2 (画面下)   = e2・(u,v,w),  e2 = (-sinA·cosE,  cosA·cosE,  -sinE)
//   d  (奥行き)   = e3・(u,v,w),  e3 = (-sinA·sinE,  cosA·sinE,   cosE)
// 逆に (u,v,w) = x1·e1 + y2·e2 + d·e3。正射影なので奥行き d は画面からは
// 決まらない → 床面 (メッシュ領域の z 最小面) との交点で d を決める。
// 視線が床面と平行に近い (|cosE| が小さい) か、交点がメッシュ領域の外に
// なるときは d = 0、すなわちシーン中心を通る視線垂直面へ落とす。
bool Viewport3D::unprojectToScene(const QPointF &screen, double out[3],
                                  bool *onFloor) const
{
    if (onFloor) *onFloor = false;
    if (!meshDefined()) return false;      // 置く場所が定義されていない
    updateSceneTransform();
    if (!(m_scale > 0.0)) return false;

    double lo[3], hi[3];
    sceneBounds(lo, hi);

    const double az = m_azimuthDeg   * M_PI / 180.0;
    const double el = m_elevationDeg * M_PI / 180.0;
    const double ca = std::cos(az), sa = std::sin(az);
    const double ce = std::cos(el), se = std::sin(el);
    const double e1[3] = {  ca,       sa,      0.0 };
    const double e2[3] = { -sa * ce,  ca * ce, -se };
    const double e3[3] = { -sa * se,  ca * se,  ce };

    const double x1 = screen.x() - width()  / 2.0 - m_panPx.x();
    const double y2 = screen.y() - height() / 2.0 - m_panPx.y();

    const double c[3] = { m_cx, m_cy, m_cz };
    const auto scenePoint = [&](double d, double p[3]) {
        for (int a = 0; a < 3; ++a)
            p[a] = c[a] + (x1 * e1[a] + y2 * e2[a] + d * e3[a]) / m_scale;
    };

    // ① 床面 (z = lo[2]) との交点。w = (lo[2]-cz)*scale を満たす d を解く。
    //    e3[2] = cosE が 0 に近いと視線が床面と平行で交点が定まらない。
    if (std::fabs(e3[2]) > 0.10) {
        const double w = (lo[2] - m_cz) * m_scale;
        const double d = (w - x1 * e1[2] - y2 * e2[2]) / e3[2];
        double p[3];
        scenePoint(d, p);
        p[2] = lo[2];                       // 丸め誤差を残さず床面に載せる
        // メッシュ領域の外は「床の外」なので採用しない
        const double tol = 1e-9 * std::max(hi[0]-lo[0], hi[1]-lo[1]);
        if (p[0] >= lo[0] - tol && p[0] <= hi[0] + tol &&
            p[1] >= lo[1] - tol && p[1] <= hi[1] + tol) {
            out[0] = p[0]; out[1] = p[1]; out[2] = p[2];
            if (onFloor) *onFloor = true;
            return true;
        }
    }

    // ② 代替: シーン中心を通る視線垂直面 (d = 0) へ落とし、領域内へ丸める
    double p[3];
    scenePoint(0.0, p);
    for (int a = 0; a < 3; ++a) out[a] = qBound(lo[a], p[a], hi[a]);
    return true;
}

void Viewport3D::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor("#1d2430"));

    // scene extents from the mesh
    double lo[3], hi[3];
    sceneBounds(lo, hi);
    updateSceneTransform();
    const double ext = std::max({ hi[0]-lo[0], hi[1]-lo[1], hi[2]-lo[2], 1e-12 });

    const QColor accent(accentColor(m_domain));

    // axis triad (bottom-left)
    {
        const double L = ext * 0.18;
        const QPointF o  = projectPoint(lo[0], lo[1], lo[2]);
        const QPointF px = projectPoint(lo[0] + L, lo[1], lo[2]);
        const QPointF py = projectPoint(lo[0], lo[1] + L, lo[2]);
        const QPointF pz = projectPoint(lo[0], lo[1], lo[2] + L);
        p.setPen(QPen(QColor("#e05555"), 2)); p.drawLine(o, px); p.drawText(px, "X");
        p.setPen(QPen(QColor("#4fb24f"), 2)); p.drawLine(o, py); p.drawText(py, "Y");
        p.setPen(QPen(QColor("#5b8fd9"), 2)); p.drawLine(o, pz); p.drawText(pz, "Z");
    }

    // mesh region box
    auto drawBox = [&](const double a[3], const double b[3], const QPen &pen) {
        drawWireBox(p, a, b, pen);
    };
    drawBox(lo, hi, QPen(QColor(255,255,255,70), 1, Qt::DashLine));

    // PML 境界の可視化 (境界チェックボックス) — 解析領域を内側へ縮めた箱
    if (m_showBoundary && m_project->general().abc == 1) {
        const int L = qMax(1, m_project->general().pmlL);
        double plo[3], phi[3];
        for (int a = 0; a < 3; ++a) {
            const MeshAxis &ax = m_project->mesh(a);
            const double d = (ax.minSpacing() < 1e307) ? ax.minSpacing() * L : 0.0;
            plo[a] = lo[a] + d;
            phi[a] = hi[a] - d;
        }
        if (plo[0] < phi[0] && plo[1] < phi[1] && plo[2] < phi[2])
            drawBox(plo, phi, QPen(QColor(245, 158, 11, 150), 1, Qt::DotLine));
    }

    // mesh grid ticks on the bottom face (z = lo[2])
    if (m_showGrid) {
        p.setPen(QPen(QColor(255,255,255,28), 1));
        const MeshAxis &mx = m_project->mesh(0);
        const MeshAxis &my = m_project->mesh(1);
        for (int i = 0; i < mx.divs.size(); ++i) {
            const double x0 = mx.nodes[i], x1 = mx.nodes[i+1];
            for (int k = 0; k <= mx.divs[i]; ++k) {
                const double x = x0 + (x1 - x0) * k / mx.divs[i];
                p.drawLine(projectPoint(x, lo[1], lo[2]),
                           projectPoint(x, hi[1], lo[2]));
            }
        }
        for (int i = 0; i < my.divs.size(); ++i) {
            const double y0 = my.nodes[i], y1 = my.nodes[i+1];
            for (int k = 0; k <= my.divs[i]; ++k) {
                const double y = y0 + (y1 - y0) * k / my.divs[i];
                p.drawLine(projectPoint(lo[0], y, lo[2]),
                           projectPoint(hi[0], y, lo[2]));
            }
        }
    }

    // geometry units
    int unit = 0;
    for (const Geometry &g : m_project->geometries()) {
        ++unit;
        QColor col = accent;
        col.setAlpha(m_solid ? 110 : 230);
        const QPen pen(col.lighter(120), 1.4);

        // all 6-parameter shapes are drawn from their bounding box; the
        // ellipsoid/cylinder shapes additionally show an inscribed outline
        double a[3] = { g.g[0], g.g[2], g.g[4] };
        double b[3] = { g.g[1], g.g[3], g.g[5] };
        if (Geometry::paramCount(g.shape) == 8) {
            // 8-param shapes: use min/max of the coordinate list as a hull
            a[0] = std::min({g.g[0], g.g[1]}); b[0] = std::max({g.g[0], g.g[1]});
            a[1] = std::min({g.g[2], g.g[3]}); b[1] = std::max({g.g[2], g.g[3]});
            a[2] = std::min({g.g[4], g.g[5], g.g[6], g.g[7]});
            b[2] = std::max({g.g[4], g.g[5], g.g[6], g.g[7]});
        }

        if (m_solid) {
            // shade the top face
            QPainterPath path;
            path.moveTo(projectPoint(a[0], a[1], b[2]));
            path.lineTo(projectPoint(b[0], a[1], b[2]));
            path.lineTo(projectPoint(b[0], b[1], b[2]));
            path.lineTo(projectPoint(a[0], b[1], b[2]));
            path.closeSubpath();
            p.fillPath(path, col);
        }
        drawBox(a, b, pen);

        if (g.shape == 2 || (g.shape >= 11 && g.shape <= 13)) {
            // inscribed ellipse outline on the mid plane
            p.setPen(pen);
            const int N = 36;
            QPolygonF poly;
            for (int k = 0; k <= N; ++k) {
                const double t = 2 * M_PI * k / N;
                double x = (a[0]+b[0])/2, y = (a[1]+b[1])/2, z = (a[2]+b[2])/2;
                const double rx = (b[0]-a[0])/2, ry = (b[1]-a[1])/2,
                             rz = (b[2]-a[2])/2;
                switch (g.shape) {
                    case 11: y += ry*std::cos(t); z += rz*std::sin(t); break;
                    case 12: x += rx*std::cos(t); z += rz*std::sin(t); break;
                    default: x += rx*std::cos(t); y += ry*std::sin(t); break;
                }
                poly << projectPoint(x, y, z);
            }
            p.drawPolyline(poly);
        }

        p.setPen(QColor(255,255,255,140));
        p.drawText(projectPoint(b[0], b[1], b[2]) + QPointF(3,-3),
                   g.name.isEmpty() ? QStringLiteral("#%1").arg(unit) : g.name);
    }

    // feeds (red diamonds) and probes (green circles)
    p.setPen(Qt::NoPen);
    for (const Feed &f : m_project->feeds()) {
        const QPointF c = projectPoint(f.x, f.y, f.z);
        QPolygonF d; d << c+QPointF(0,-5) << c+QPointF(5,0)
                       << c+QPointF(0,5)  << c+QPointF(-5,0);
        p.setBrush(QColor("#ff5252"));
        p.drawPolygon(d);
    }
    for (const Probe &pr : m_project->probes()) {
        const QPointF c = projectPoint(pr.x, pr.y, pr.z);
        p.setBrush(QColor("#69d069"));
        p.drawEllipse(c, 4, 4);
    }

    // 水中音響: 海面・海底地形・音源。TL 断面はこの面の上に重なる。
    if (m_domain == Domain::Underwater) drawOcean(p);

    // 室内音響: スピーカーの位置と **向き** を法線矢印で描く。
    // aim ("+X" / "-Z 30°" / "0,0,-1") が解けたものだけ矢印を出す —
    // 解けない文字列に適当な向きを描くと、間違った情報を見せることになる
    // (core/AimDirection)。矢印長は解析領域の 12 % (どの規模でも見える)。
    if (m_domain == Domain::Acoustic) {
        const double arrow = ext * 0.12;
        for (const AcousticSourceRow &sr : m_project->acoustic().sources) {
            if (!sr.enabled) continue;
            const QPointF c = projectPoint(sr.x_m, sr.y_m, sr.z_m);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor("#ffb02e"));
            p.drawEllipse(c, 4, 4);

            double d[3];
            if (!parseAim(sr.aim, d)) continue;   // 向き不明 → 矢印は描かない
            const QPointF tip = projectPoint(sr.x_m + d[0] * arrow,
                                             sr.y_m + d[1] * arrow,
                                             sr.z_m + d[2] * arrow);
            p.setPen(QPen(QColor("#ffb02e"), 2));
            p.drawLine(c, tip);
            // 矢じり (画面上で作る — 投影後の向きに合わせる)
            const QPointF v = tip - c;
            const double len = std::hypot(v.x(), v.y());
            if (len > 1.0) {
                const QPointF u = v / len, n(-u.y(), u.x());
                QPolygonF head;
                head << tip << (tip - u * 8.0 + n * 4.0)
                     << (tip - u * 8.0 - n * 4.0);
                p.setPen(Qt::NoPen);
                p.setBrush(QColor("#ffb02e"));
                p.drawPolygon(head);
            }
        }
    }
    if (m_project->planewave().enabled) {
        // incident direction arrow from outside the box
        const double th = m_project->planewave().theta * M_PI / 180.0;
        const double ph = m_project->planewave().phi   * M_PI / 180.0;
        const double R = ext * 0.75;
        const QPointF from = projectPoint(m_cx + R*std::sin(th)*std::cos(ph),
                                          m_cy + R*std::sin(th)*std::sin(ph),
                                          m_cz + R*std::cos(th));
        const QPointF to = projectPoint(m_cx, m_cy, m_cz);
        p.setPen(QPen(QColor("#ffd24d"), 2));
        p.drawLine(from, to);
        p.setBrush(QColor("#ffd24d"));
        p.drawEllipse(to, 3, 3);
    }

    // ビュースタイル別オーバーレイ
    if (m_viewStyle == ViewStyle::Field) drawResultSlice(p);
    if (m_viewStyle == ViewStyle::Rays)  drawRayOverlay(p);

    // ドラッグ中の配置プレビューとドロップ結果の一時表示
    if (m_dragHover) drawDropPreview(p);
    drawDropMessage(p);

    // overlay text
    p.setPen(QColor(255,255,255,150));
    p.drawText(8, height() - 10,
               QStringLiteral("%1   cells: %L2   az %3°  el %4°")
               .arg(domainKey(m_domain))
               .arg(m_project->totalCells())
               .arg(int(m_azimuthDeg)).arg(int(m_elevationDeg)));
}

// 面内座標 (u, v) [m] → 画面座標。固定軸は m_sliceAxis / m_slicePos。
// 深度方向の表示倍率。**水中音響ドメインのときだけ**効く。海は 50 km x 3 km の
// ように極端に平たいので、等方の縮尺では帯にしか見えない。倍率を掛けた分は
// 画面に必ず明記する (断りなく縦に伸ばすと縮尺の嘘になる)。
double Viewport3D::zView(double z) const
{
    return (m_domain == Domain::Underwater) ? z * m_vScale : z;
}

// 面内座標 (u, v) → シーン座標。u/v がどの軸かは断面の固定軸で決まる
// (io/H5Reader::seriesSliceAxes と同じ規約 — u は axis=0 のとき y、他は x。
//  v は axis=2 のとき y、他は z)。
void Viewport3D::sliceScenePoint(const Slice &s, double u, double v,
                                 double out[3]) const
{
    switch (s.axis) {
    case 0:  out[0] = s.pos; out[1] = u;     out[2] = zView(v); break; // YZ
    case 1:  out[0] = u;     out[1] = s.pos; out[2] = zView(v); break; // XZ
    default: out[0] = u;     out[1] = v;     out[2] = s.pos;    break; // XY
    }
}

QPointF Viewport3D::projectSlicePoint(const Slice &s, double u, double v) const
{
    double q[3];
    sliceScenePoint(s, u, v, q);
    return projectPoint(q[0], q[1], q[2]);
}

// 結果断面オーバーレイ — setResultSlice() で渡された実データを 3D 空間の
// 該当平面に描く。面の 4 隅を投影し、色画像をアフィン変換で貼るだけ
// (1 枚の平面なので自己遮蔽は無く深度ソート不要)。
// 断面が未設定のときは合成パターンを描かず「未読込」を明示する
// (存在しない結果を界分布らしく見せない — CLAUDE.md 絶対規則 5)。
void Viewport3D::drawResultSlice(QPainter &p)
{
    if (!hasResultSlice()) {
        // ── 未読込の明示 ────────────────────────────────────────────────
        const QString msg  = I18n::tr("vp_fld_none");
        const QString hint = I18n::tr("vp_fld_none_hint");
        const QFontMetrics fm(p.font());
        const int w = std::max(fm.horizontalAdvance(msg),
                               fm.horizontalAdvance(hint)) + 28;
        const int h = fm.height() * 2 + 24;
        const QRectF box((width() - w) / 2.0, (height() - h) / 2.0, w, h);
        p.setPen(QPen(QColor(245, 158, 11, 200), 1));
        p.setBrush(QColor(20, 26, 36, 215));
        p.drawRoundedRect(box, 6, 6);
        p.setPen(QColor(245, 158, 11));
        p.drawText(QRectF(box.x(), box.y() + 8, box.width(), fm.height()),
                   Qt::AlignHCenter | Qt::AlignVCenter, msg);
        p.setPen(QColor(255, 255, 255, 165));
        p.drawText(QRectF(box.x(), box.y() + 10 + fm.height(), box.width(),
                          fm.height()),
                   Qt::AlignHCenter | Qt::AlignVCenter, hint);
        p.setBrush(Qt::NoBrush);
        return;
    }

    // ── 断面を互いの交線で切り分ける ──────────────────────────────────────
    // 断面が 2 枚以上あると互いに貫通するので、**四角形のままでは画家の
    // アルゴリズムが原理的に成立しない** (どう並べても正しい前後にならない)。
    // 各面を「他の面の位置」で切って小片にすると、切ったあとは互いに
    // 貫通しないので重心の奥行きで厳密に前後が決まる。直交 3 面なら
    // 各面が 4 分割されて計 12 片。
    QVector<SlicePlane> planes;
    planes.reserve(m_slices.size());
    for (const Slice &s : m_slices) {
        SlicePlane pl;
        pl.axis = s.axis; pl.pos = s.pos;
        pl.u0 = s.u0; pl.u1 = s.u1; pl.v0 = s.v0; pl.v1 = s.v1;
        planes.push_back(pl);
    }
    // 切り分けは io/SlicePieces (Qt 非依存・selftest 済み)。ここは奥行きを
    // 付けて並べ替えるだけ
    QVector<SlicePiece> cut = cutSlices(planes);
    QVector<QPair<double, SlicePiece>> pieces;
    pieces.reserve(cut.size());
    for (const SlicePiece &pc : cut) {
        const Slice &s = m_slices[pc.plane];
        double q[3];
        sliceScenePoint(s, (pc.ua + pc.ub) / 2.0, (pc.va + pc.vb) / 2.0, q);
        pieces.push_back({ sceneDepth(q[0], q[1], q[2]), pc });
    }
    // 奥から手前へ (io/MeshProjection と同じ向きの規約)
    std::sort(pieces.begin(), pieces.end(),
              [](const QPair<double, SlicePiece> &a,
                 const QPair<double, SlicePiece> &b) {
                  return a.first < b.first;
              });

    for (const QPair<double, SlicePiece> &entry : pieces) {
        const SlicePiece &pc = entry.second;
        const Slice &s = m_slices[pc.plane];
        if (s.img.isNull()) continue;
        // 面全体の 4 隅 → 画像全体、というアフィン変換を 1 つ作り、小片は
        // クリップで切り出す。正射影なので像は必ずアフィン変換で表せる。
        // セル毎に四辺形を描くと隣接セルの縁が重なって透明度が飽和する
        // (下の形状が透けない) ため、色画像を 1 回だけ貼る。
        const QPointF c00 = projectSlicePoint(s, s.u0, s.v1);
        const QPointF c10 = projectSlicePoint(s, s.u1, s.v1);
        const QPointF c11 = projectSlicePoint(s, s.u1, s.v0);
        const QPointF c01 = projectSlicePoint(s, s.u0, s.v0);
        const double iw = s.img.width(), ih = s.img.height();
        const QPolygonF src{ QPointF(0, 0), QPointF(iw, 0),
                             QPointF(iw, ih), QPointF(0, ih) };
        const QPolygonF dst{ c00, c10, c11, c01 };
        QTransform t;
        if (!QTransform::quadToQuad(src, dst, t)) continue;

        // 小片のクリップ領域 (画面座標)。同じ面の隣り合う小片のあいだに
        // 隙間が出ないよう、重心まわりに僅かに広げる (重なるぶんには
        // 同一面の同じ絵なので害が無い)
        QPolygonF clip{ projectSlicePoint(s, pc.ua, pc.vb),
                        projectSlicePoint(s, pc.ub, pc.vb),
                        projectSlicePoint(s, pc.ub, pc.va),
                        projectSlicePoint(s, pc.ua, pc.va) };
        QPointF ctr;
        for (const QPointF &q : clip) ctr += q;
        ctr /= double(clip.size());
        for (QPointF &q : clip) q = ctr + (q - ctr) * 1.01 + QPointF(0, 0);

        p.save();
        p.setClipRegion(QRegion(clip.toPolygon()));
        // データを補間しない (実際のセル解像度をそのまま見せる)
        p.setRenderHint(QPainter::SmoothPixmapTransform, false);
        p.setRenderHint(QPainter::Antialiasing, false);
        p.setOpacity(0.72);         // 形状ワイヤが透ける程度の透明度
        p.setTransform(t, true);
        p.drawImage(QPointF(0, 0), s.img);
        p.restore();
    }

    // 断面の外枠 (面の位置を分かりやすく)。小片ではなく面ごとに 1 本
    p.setPen(QPen(QColor(255, 255, 255, 110), 1));
    p.setBrush(Qt::NoBrush);
    for (const Slice &s : m_slices) {
        const QPolygonF outline{ projectSlicePoint(s, s.u0, s.v1),
                                 projectSlicePoint(s, s.u1, s.v1),
                                 projectSlicePoint(s, s.u1, s.v0),
                                 projectSlicePoint(s, s.u0, s.v0) };
        p.drawPolygon(outline);
    }

    drawSliceLegend(p, m_sliceDecim);
}

// ── 水中音響の「舞台」 ─────────────────────────────────────────────────────
// 海面 (z = 0) と海底地形と音源を、TL 断面と**同じ鉛直面 (y = 0)** の上に描く。
// メッシュ領域とは無関係で、距離と水深 (SSP / 地形断面) だけで決まる。
// 地形断面が無いときは平坦海底 — 「地形がある」ように描かない。
void Viewport3D::drawOcean(QPainter &p)
{
    double lo[3], hi[3];
    if (!oceanBounds(lo, hi)) return;
    const UnderwaterOpts &u = m_project->underwater();
    const double x0 = lo[0], x1 = hi[0], depth = -lo[2];

    // 海面 (z = 0) — 倍率を掛けても 0 のまま
    p.setPen(QPen(QColor(120, 190, 255, 190), 2));
    p.drawLine(projectPoint(x0, 0.0, 0.0), projectPoint(x1, 0.0, 0.0));

    // 海底 — 地形断面があればその折れ線、無ければ平坦
    QPolygonF bottom;
    if (u.bathymetry.size() >= 2) {
        for (const BathyPoint &b : u.bathymetry) {
            const double x = b.range_km * 1000.0;
            if (x < x0 || x > x1) continue;
            bottom << projectPoint(x, 0.0, zView(-b.depth_m));
        }
    }
    if (bottom.size() < 2) {
        bottom.clear();
        bottom << projectPoint(x0, 0.0, zView(-depth))
               << projectPoint(x1, 0.0, zView(-depth));
    }
    p.setPen(QPen(QColor(190, 150, 100, 200), 2));
    p.drawPolyline(bottom);

    // 側面の枠 (断面がどこに載るのかを示す)
    p.setPen(QPen(QColor(255, 255, 255, 45), 1, Qt::DashLine));
    p.drawLine(projectPoint(x0, 0.0, 0.0), projectPoint(x0, 0.0, zView(-depth)));
    p.drawLine(projectPoint(x1, 0.0, 0.0), projectPoint(x1, 0.0, zView(-depth)));

    // 縦倍率を掛けているなら**必ず画面に書く** — 断りなく縦に伸ばした図は
    // 縮尺の嘘になる (絶対規則 5 と同じ趣旨)。
    if (m_vScale != 1.0) {
        p.setPen(QColor(245, 158, 11));
        p.drawText(10, height() - 26,
                   I18n::tr("vp3_vexag").arg(QString::number(m_vScale, 'g', 3)));
    }

    // 音源 — 深度は .env と同じ規則 (BellhopIO::sourceDepth)
    const QPointF sc = projectPoint(x0, 0.0, zView(-BellhopIO::sourceDepth(u)));
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#ffb02e"));
    p.drawEllipse(sc, 4, 4);
}

// 断面データ → 色画像 (行 0 = 第 2 軸の +側)。setResultSlice のたびに 1 回
// だけ作り、再描画では貼るだけにする。巨大格子は平均で束ねて画像サイズを
// 抑える (束ねたら m_sliceDecim に残して凡例に出す)。
void Viewport3D::rebuildSliceImages()
{
    m_sliceDecim = 1;
    // 正規化は全断面共通 (m_sliceMax)。面ごとの最大値では割らない
    const double inv = (m_sliceMax > 0.0) ? 1.0 / m_sliceMax : 0.0;
    for (Slice &s : m_slices) {
        s.img = QImage();
        s.decim = 1;
        const int rows = s.rows, cols = s.cols;
        if (rows <= 0 || cols <= 0) continue;
        const int maxDim = 1024;
        int step = 1;
        while ((cols + step - 1) / step > maxDim
               || (rows + step - 1) / step > maxDim)
            ++step;
        s.decim = step;
        m_sliceDecim = std::max(m_sliceDecim, step);

        const int w = (cols + step - 1) / step;
        const int h = (rows + step - 1) / step;
        QImage img(w, h, QImage::Format_ARGB32);
        if (img.isNull()) continue;
        for (int r = 0; r < h; ++r) {
            QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(r));
            const int r1 = std::min((r + 1) * step, rows);
            for (int c = 0; c < w; ++c) {
                const int c1 = std::min((c + 1) * step, cols);
                double sum = 0.0;
                int n = 0;
                for (int rr = r * step; rr < r1; ++rr) {
                    const int base = rr * cols;
                    for (int cc = c * step; cc < c1; ++cc) {
                        const double v = s.cells[base + cc];
                        if (std::isfinite(v)) { sum += std::fabs(v); ++n; }
                    }
                }
                if (n == 0) { line[c] = qRgba(0, 0, 0, 0); continue; } // 値なし
                const double t = qBound(0.0, sum / n * inv, 1.0);
                const QColor col = FieldHeatmap::jet(t);
                // ── 弱いところほど透かす ────────────────────────────────
                // 場がほぼ 0 の一帯 (jet の濃い青) を不透明のまま塗ると、
                // 奥の断面も物体形状も完全に隠れてしまう。3D に重ねる意味が
                // 「何がどこにあるか」を一緒に見ることなので、強度で不透明度
                // を付ける。t < kClear は完全に透ける。
                // **これは表示の重み付けであってデータの間引きではない** —
                // 値そのものは 2D 断面・CSV 側で読む。
                const double kClear = 0.05;
                double a = (t - kClear) / (1.0 - kClear);
                a = (a <= 0.0) ? 0.0 : std::pow(a, 0.7);
                line[c] = qRgba(col.red(), col.green(), col.blue(),
                                int(qBound(0.0, a, 1.0) * 255.0 + 0.5));
            }
        }
        s.img = img;
    }
}

// 結果断面の凡例 — カラーバー (0..1) + 実データである旨 + label
void Viewport3D::drawSliceLegend(QPainter &p, int decim)
{
    const QFontMetrics fm(p.font());
    // ── 左上: 実データ表記 + データセット名/時刻 + 断面位置 ─────────────
    if (m_slices.isEmpty()) return;
    QStringList lines;
    lines << I18n::tr("vp_fld_real");
    // 断面ごとに「どの軸のどこか」を並べる (複数面のときにどれがどれか
    // 分からなくならないように)
    QStringList where;
    for (const Slice &s : m_slices)
        where << QStringLiteral("%1 = %2 m")
                     .arg(QLatin1String(sliceAxisName(s.axis)))
                     .arg(QString::number(s.pos, 'g', 4));
    QString sub = where.join(QStringLiteral("  /  "));
    if (!m_sliceLabel.isEmpty())
        sub = m_sliceLabel + QStringLiteral("   ") + sub;
    lines << sub;
    // 解像度も面ごと。正規化最大値は全面共通の 1 つ (カラーバーが 1 本)
    QStringList dims;
    for (const Slice &s : m_slices)
        dims << QStringLiteral("%1 x %2").arg(s.cols).arg(s.rows);
    lines << dims.join(QStringLiteral("  /  "))
             + QStringLiteral("   ")
             + I18n::tr("vp_fld_norm")
                   .arg(QString::number(m_sliceMax, 'g', 4));
    if (decim > 1)
        lines << I18n::tr("vp_fld_decim").arg(decim);
    // 再生コントロールの状況 — 2D 側と同じコマを見ていることが 3D だけでも
    // 分かるようにする (コマ番号は 0 起点。2D の表示と同じ数え方)
    const bool hasPlay = (m_sliceFrameCount > 0);
    if (hasPlay)
        lines << (m_slicePlaying ? I18n::tr("vp_fld_playing")
                                 : I18n::tr("vp_fld_paused"))
                     .arg(m_sliceFrame).arg(m_sliceFrameCount - 1);
    // 弱いところを透かしていることを明記する (消えているのではなく薄い)
    lines << I18n::tr("vp_fld_alpha");

    int w = 0;
    for (const QString &s : lines) w = std::max(w, fm.horizontalAdvance(s));
    const QRectF box(6, 6, w + 16, fm.height() * lines.size() + 12);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(15, 20, 28, 175));
    p.drawRoundedRect(box, 4, 4);
    p.setBrush(Qt::NoBrush);
    for (int i = 0; i < lines.size(); ++i) {
        p.setPen(i == 0 ? QColor(accentColor(m_domain))
                        : QColor(255, 255, 255, 175));
        p.drawText(QRectF(box.x() + 8, box.y() + 6 + fm.height() * i,
                          box.width() - 16, fm.height()),
                   Qt::AlignLeft | Qt::AlignVCenter, lines[i]);
    }

    // 再生位置のバー (凡例の下辺。コマ位置が一目で分かるように)
    if (hasPlay && m_sliceFrameCount > 1) {
        const double t = qBound(0.0, double(m_sliceFrame)
                                         / double(m_sliceFrameCount - 1), 1.0);
        const QRectF bar(box.x() + 8, box.bottom() - 4,
                         box.width() - 16, 2.0);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 60));
        p.drawRect(bar);
        p.setBrush(QColor(accentColor(m_domain)));
        p.drawRect(QRectF(bar.x(), bar.y(), bar.width() * t, bar.height()));
        p.setBrush(Qt::NoBrush);
    }

    // ── 右辺: カラーバー (0.0 〜 1.0) ───────────────────────────────────
    const int bh = qBound(60, height() - 120, 160);
    const int bw = 12;
    const int bx = width() - bw - 46;
    const int by = 24;
    if (bx <= 0 || bh <= 0) return;
    for (int i = 0; i < bh; ++i) {
        const double t = 1.0 - double(i) / double(bh - 1);
        p.setPen(FieldHeatmap::jet(t));
        p.drawLine(bx, by + i, bx + bw, by + i);
    }
    p.setPen(QColor(255, 255, 255, 150));
    p.drawRect(bx, by, bw, bh);
    p.drawText(QRectF(bx + bw + 3, by - fm.height() / 2.0, 40, fm.height()),
               Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("1.0"));
    p.drawText(QRectF(bx + bw + 3, by + bh - fm.height() / 2.0, 40,
                      fm.height()),
               Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("0.0"));
}

// レイトレースオーバーレイ — モックと同じ 24本 × 最大4反射。
// 領域境界で支配軸を反転させ、反射ごとにエネルギーを 0.7 倍する。
// **ソルバの計算結果ではなく見た目のサンプル** なので、その旨を画面に明示する
// (未実装のものを動作済みに見せない — CLAUDE.md 絶対規則 5)。
void Viewport3D::drawRayOverlay(QPainter &p)
{
    // 先に注記を描く (以降で return しても必ず出る)
    {
        const QString msg = I18n::tr("vp_rays_sample");
        const QFontMetrics fm(p.font());
        const QRectF box(6, 6, fm.horizontalAdvance(msg) + 16,
                         fm.height() + 10);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(15, 20, 28, 175));
        p.drawRoundedRect(box, 4, 4);
        p.setBrush(Qt::NoBrush);
        p.setPen(QColor(245, 158, 11));
        p.drawText(box.adjusted(8, 0, -8, 0),
                   Qt::AlignLeft | Qt::AlignVCenter, msg);
    }

    double lo[3], hi[3];
    for (int a = 0; a < 3; ++a) {
        lo[a] = m_project->mesh(a).min();
        hi[a] = m_project->mesh(a).max();
        if (!(hi[a] > lo[a])) return;
    }
    // 波源: feed があればその位置、無ければ領域中心
    double src[3] = { m_cx, m_cy, m_cz };
    if (!m_project->feeds().isEmpty()) {
        const Feed &f = m_project->feeds().first();
        src[0] = f.x; src[1] = f.y; src[2] = f.z;
    }

    const int N = 24, bounces = 4;
    const double diag = std::sqrt((hi[0]-lo[0])*(hi[0]-lo[0])
                                + (hi[1]-lo[1])*(hi[1]-lo[1])
                                + (hi[2]-lo[2])*(hi[2]-lo[2]));
    const double stepLen = diag * 0.02;

    QColor col(accentColor(m_domain));
    col.setAlphaF(0.55);
    p.setPen(QPen(col, 0.9));
    p.setBrush(Qt::NoBrush);

    for (int i = 0; i < N; ++i) {
        const double theta = double(i) / N * 2.0 * M_PI;
        const double phi = M_PI / 2.0 + std::sin(i * 0.7) * 0.4;
        double dir[3] = { std::cos(theta) * std::sin(phi),
                          std::cos(phi),
                          std::sin(theta) * std::sin(phi) };
        double pos[3] = { src[0], src[1], src[2] };

        QPainterPath path;
        path.moveTo(projectPoint(pos[0], pos[1], pos[2]));
        double energy = 1.0;
        for (int b = 0; b < bounces; ++b) {
            for (int s = 0; s < 80; ++s) {
                for (int k = 0; k < 3; ++k) pos[k] += dir[k] * stepLen;
                if (pos[0] < lo[0] || pos[0] > hi[0] ||
                    pos[1] < lo[1] || pos[1] > hi[1] ||
                    pos[2] < lo[2] || pos[2] > hi[2]) break;
            }
            path.lineTo(projectPoint(pos[0], pos[1], pos[2]));
            // 最も外へ出ている軸で反射させ、位置を領域内へ戻す
            int axis = 0;
            double worst = 0;
            for (int k = 0; k < 3; ++k) {
                const double over = std::max(lo[k] - pos[k], pos[k] - hi[k]);
                if (over > worst) { worst = over; axis = k; }
            }
            dir[axis] *= -1.0;
            pos[axis] = std::clamp(pos[axis], lo[axis], hi[axis]);
            energy *= 0.7;
            if (energy < 0.1) break;
        }
        p.drawPath(path);
    }
}

void Viewport3D::mousePressEvent(QMouseEvent *e)
{
    m_lastPos = e->position();
    m_dragButton = e->button();
}

void Viewport3D::mouseMoveEvent(QMouseEvent *e)
{
    const QPointF d = e->position() - m_lastPos;
    m_lastPos = e->position();
    if (m_dragButton == Qt::LeftButton) {
        m_azimuthDeg  += d.x() * 0.5;
        m_elevationDeg = qBound(-89.0, m_elevationDeg + d.y() * 0.5, 89.0);
        update();
        emit viewChanged(m_azimuthDeg, m_elevationDeg);   // ツールバー同期
    } else if (m_dragButton == Qt::MiddleButton) {
        m_panPx += d;
        update();
    }
}

void Viewport3D::wheelEvent(QWheelEvent *e)
{
    const double f = std::pow(1.0015, e->angleDelta().y());
    m_zoom = qBound(0.05, m_zoom * f, 50.0);
    update();
}

void Viewport3D::mouseDoubleClickEvent(QMouseEvent *)
{
    fitView();
}

// ── ドラッグ&ドロップ配置 ────────────────────────────────────────────────────

// 直方体のワイヤフレーム (メッシュ領域・形状ユニット・配置プレビュー共用)
void Viewport3D::drawWireBox(QPainter &p, const double a[3], const double b[3],
                             const QPen &pen) const
{
    const QPointF v[8] = {
        projectPoint(a[0],a[1],a[2]), projectPoint(b[0],a[1],a[2]),
        projectPoint(b[0],b[1],a[2]), projectPoint(a[0],b[1],a[2]),
        projectPoint(a[0],a[1],b[2]), projectPoint(b[0],a[1],b[2]),
        projectPoint(b[0],b[1],b[2]), projectPoint(a[0],b[1],b[2]),
    };
    static const int e[12][2] = { {0,1},{1,2},{2,3},{3,0},
                                  {4,5},{5,6},{6,7},{7,4},
                                  {0,4},{1,5},{2,6},{3,7} };
    p.setPen(pen);
    for (auto &ed : e) p.drawLine(v[ed[0]], v[ed[1]]);
}

// ドラッグ位置 → 配置先の再計算。配置できるときだけ true。
bool Viewport3D::updateDragTarget(const QPointF &pos)
{
    m_dragPos = pos;
    m_dragWhy.clear();
    m_dragOk = false;

    QString why;
    // ドメイン許可表込みの判定 (水中音響は全部品不可、ドメイン外部品も不可)
    if (!ComponentDrop::canPlace(m_dragCat, m_dragName, domainKey(m_domain),
                                 &why)) {
        m_dragWhy = why;
        return false;
    }
    if (!meshDefined()) {
        m_dragWhy = I18n::tr("vp_drop_nomesh");
        return false;
    }
    if (!unprojectToScene(pos, m_dragScene, &m_dragOnFloor)) {
        m_dragWhy = I18n::tr("vp_drop_nomesh");
        return false;
    }
    m_dragOk = true;
    return true;
}

void Viewport3D::dragEnterEvent(QDragEnterEvent *e)
{
    if (!e->mimeData()->hasFormat(ComponentDrop::mimeType())) {
        e->ignore();
        return;
    }
    if (!ComponentDrop::decode(e->mimeData()->data(ComponentDrop::mimeType()),
                               &m_dragCat, &m_dragName)) {
        e->ignore();
        return;
    }
    m_dragHover = true;
    m_dropMsg.clear();          // 前回の結果表示はプレビューに譲る
    // 配置できない場合もドラッグは受け付けて理由を画面に出す
    // (カーソルが「禁止」になるだけで理由が分からない状態にしない)。
    e->acceptProposedAction();
    updateDragTarget(e->position());
    update();
}

void Viewport3D::dragMoveEvent(QDragMoveEvent *e)
{
    if (!m_dragHover) { e->ignore(); return; }
    const bool ok = updateDragTarget(e->position());
    if (ok) e->acceptProposedAction();
    else    e->ignore();        // 落とせないことはカーソルで、理由は画面で示す
    update();
}

void Viewport3D::dragLeaveEvent(QDragLeaveEvent *)
{
    m_dragHover = false;
    m_dragOk = false;
    m_dragWhy.clear();
    update();
}

void Viewport3D::dropEvent(QDropEvent *e)
{
    QString cat, name;
    if (!e->mimeData()->hasFormat(ComponentDrop::mimeType())
        || !ComponentDrop::decode(
               e->mimeData()->data(ComponentDrop::mimeType()), &cat, &name)) {
        e->ignore();
        return;
    }
    m_dragHover = false;
    m_dragCat = cat;
    m_dragName = name;

    QString msg;
    if (!updateDragTarget(e->position())) {
        showDropMessage(m_dragWhy, false);
        e->ignore();
        update();
        return;
    }
    const bool ok = placeComponent(cat, name, m_dragScene, m_dragOnFloor, &msg);
    showDropMessage(msg, ok);
    if (ok) e->acceptProposedAction();
    else    e->ignore();
    update();
}

// ドロップされたコンポーネントをモデルへ反映する。
// 形状系 → geometry、波源系 → feed、モニター系 (点) → point。
// 位置だけでは作れないもの (取込モデル・平面波・面/領域モニター) は
// 何も追加せず理由を返す。
bool Viewport3D::placeComponent(const QString &cat, const QString &name,
                                const double pos[3], bool onFloor,
                                QString *msg)
{
    const DropSpec sp = dropSpecFor(cat, name);
    // ドメイン許可表 (水中音響は全部品不可) + ドロップ対応の両方を確認する。
    // 通常は updateDragTarget が先に弾くが、モデルへ書く直前にも判定する
    QString why;
    if (!ComponentDrop::canPlace(cat, name, domainKey(m_domain), &why)) {
        if (msg) *msg = why;
        return false;
    }
    if (!meshDefined()) {
        if (msg) *msg = I18n::tr("vp_drop_nomesh");
        return false;
    }

    const QString where = onFloor
        ? I18n::tr("vp_drop_floor").arg(QString::number(pos[2], 'g', 4))
        : I18n::tr("vp_drop_viewplane");
    QString what;

    // 室内音響では床面ぴったりに置くと、床スラブ・客席ブロック・舞台などの
    // 剛体形状の中に入ってしまい、ソルバーが「剛体内の点」として失敗する。
    // 実務どおりの高さへ持ち上げる: 音源 1.5 m / 受音点 1.2 m
    // (ISO 3382-1 の座位耳高さ)。天井を突き抜けないようメッシュ内に収める。
    double zSrc = pos[2], zRcv = pos[2];
    QString lifted;
    if (m_domain == Domain::Acoustic && onFloor) {
        const double zTop = m_project->mesh(2).max();
        auto lift = [&](double h) {
            const double z = pos[2] + h;
            return (z < zTop) ? z : pos[2] + (zTop - pos[2]) * 0.5;
        };
        zSrc = lift(1.5);
        zRcv = lift(1.2);
        if (sp.kind == DropKind::Feed || sp.kind == DropKind::Probe)
            lifted = I18n::tr("vp_drop_ac_height")
                         .arg(QString::number(
                             sp.kind == DropKind::Feed ? zSrc : zRcv, 'g', 3));
    }

    switch (sp.kind) {
    case DropKind::Feed: {
        Feed f;
        f.dir = 'Z';
        f.x = pos[0]; f.y = pos[1]; f.z = zSrc;
        m_project->feeds().push_back(f);
        what = I18n::tr("vp_drop_feed").arg(m_project->feeds().size());
        // スピーカーは音源リスト (音源/WAV/指向性タブ) にも同じ位置の行を
        // 追加する。位置が feed と厳密一致するので「ソルバ波源 #n」の
        // マーカーも自動で立つ
        if (m_domain == Domain::Acoustic
            && name == QLatin1String("Loudspeaker")) {
            const QString nm = ComponentCatalog::addLoudspeakerSourceRow(
                *m_project, pos[0], pos[1], zSrc);
            what += I18n::tr("vp_drop_synced_src").arg(nm);
        }
        break;
    }
    case DropKind::Probe: {
        Probe pr;
        pr.dir = 'Z';
        pr.x = pos[0]; pr.y = pos[1]; pr.z = zRcv;
        // 1 点目は伝搬方向を持つ (SourceTab の追加と同じ既定)
        if (m_project->probes().isEmpty()) pr.propagation = "+X";
        m_project->probes().push_back(pr);
        what = I18n::tr("vp_drop_probe").arg(m_project->probes().size());
        // マイクロホンは受音点リスト (可聴化の一括レンダリング対象) にも
        // 同じ位置の行を追加する
        if (m_domain == Domain::Acoustic
            && name == QLatin1String("Microphone")) {
            const QString nm = ComponentCatalog::addMicrophoneReceiverRow(
                *m_project, pos[0], pos[1], zRcv);
            what += I18n::tr("vp_drop_synced_rcv").arg(nm);
        }
        break;
    }
    default: {
        const double base = defaultSize();
        double lo[3], hi[3];
        dropBounds(sp, pos, base, lo, hi);

        Geometry g;
        g.shape = sp.shape;
        g.name  = name;
        // 材料は先頭のユーザー定義材料 (id=2)。まだ 1 つも無いときは
        // 必ず存在する PEC (id=1) にする (存在しない材料番号を書かない)。
        g.materialId = m_project->materials().isEmpty() ? 1 : 2;

        switch (sp.shape) {
        case 33: {
            // 三角柱 Z: g[0..1]=z範囲, g[2..4]=頂点の x, g[5..7]=頂点の y
            // (本家 sol/ingeometry.c shape 33 / inout3 の並び)。
            // 外接円半径 r の正三角形を xy 面に置く。
            g.g[0] = lo[2]; g.g[1] = hi[2];
            const double xc = (lo[0]+hi[0])/2, yc = (lo[1]+hi[1])/2;
            const double rx = (hi[0]-lo[0])/2, ry = (hi[1]-lo[1])/2;
            for (int k = 0; k < 3; ++k) {
                const double t = M_PI / 2.0 + 2.0 * M_PI * k / 3.0;
                g.g[2 + k] = xc + rx * std::cos(t);
                g.g[5 + k] = yc + ry * std::sin(t);
            }
            break;
        }
        case 43:
            // 角錐 Z: g = z1 z2 x0 y0 (z1の x幅) (z1の y幅) (z2の x幅) (z2の y幅)
            // (本家 shape 43。幅は全幅で、カーネル側が /2 する)
            g.g[0] = lo[2]; g.g[1] = hi[2];
            g.g[2] = (lo[0]+hi[0])/2; g.g[3] = (lo[1]+hi[1])/2;
            g.g[4] = hi[0]-lo[0]; g.g[5] = hi[1]-lo[1];
            g.g[6] = 0.0;         g.g[7] = 0.0;          // 上端は頂点
            break;
        case 53:
            // 円錐台 Z: g = z1 z2 x0 y0 (z1の径x) (z1の径y) (z2の径x) (z2の径y)
            // 上端の径は 0 にしない — 本家 shape 53 は径で除算するため
            // (ingeometry.c)、頂点を持つ円錐は下端の 1/10 の径で近似する。
            g.g[0] = lo[2]; g.g[1] = hi[2];
            g.g[2] = (lo[0]+hi[0])/2; g.g[3] = (lo[1]+hi[1])/2;
            g.g[4] = hi[0]-lo[0];         g.g[5] = hi[1]-lo[1];
            g.g[6] = (hi[0]-lo[0]) * 0.1; g.g[7] = (hi[1]-lo[1]) * 0.1;
            break;
        default:
            // 6 パラメータ形状 (1 直方体 / 2 楕円体 / 11-13 円柱) は
            // 外接直方体そのもの
            for (int a = 0; a < 3; ++a) {
                g.g[2*a]     = lo[a];
                g.g[2*a + 1] = hi[a];
            }
            break;
        }
        m_project->geometries().push_back(g);
        what = I18n::tr("vp_drop_geom")
                   .arg(m_project->geometries().size())
                   .arg(shapeLabel(sp.shape));
        break;
    }
    }

    // モデルが変わった → ビューポート / ツリー / ステータスバーが自動更新
    m_project->touch();
    if (msg) {
        *msg = I18n::tr("vp_drop_added").arg(name, what,
                                             posText(pos) + "   " + where);
        // 音響ドメインの吸音系コンポーネントは剛体形状としてしか置かれない
        // — 吸音率が設定されたと誤解しないよう注記する
        const bool absorberLike =
            (name == QLatin1String("Absorber panel")
             || name == QLatin1String("Diffuser (QRD)")
             || name == QLatin1String("Audience block"));
        if (m_domain == Domain::Acoustic && sp.kind == DropKind::Shape
            && absorberLike)
            *msg += QStringLiteral("\n") + I18n::tr("vp_drop_rigid");
        // 音源/受音点を床から持ち上げた場合はその高さを明記する
        if (!lifted.isEmpty()) *msg += QStringLiteral("\n") + lifted;
    }
    return true;
}

// ドロップ結果 (成功/理由) の一時表示。数秒で自動的に消す。
void Viewport3D::showDropMessage(const QString &msg, bool ok)
{
    m_dropMsg = msg;
    m_dropMsgOk = ok;
    const int seq = ++m_dropMsgSeq;
    if (msg.isEmpty()) return;
    QTimer::singleShot(6000, this, [this, seq] {
        if (seq != m_dropMsgSeq) return;   // 新しい表示に置き換わっている
        m_dropMsg.clear();
        update();
    });
}

// ドラッグ中の配置プレビュー — 置かれる位置に輪郭を描く。
// 配置できない場合は理由を出す (黙って何も起きない状態にしない)。
void Viewport3D::drawDropPreview(QPainter &p)
{
    const QFontMetrics fm(p.font());
    QStringList lines;
    lines << m_dragName;

    if (m_dragOk) {
        const DropSpec sp = dropSpecFor(m_dragCat, m_dragName);
        QColor col(accentColor(m_domain));
        col = col.lighter(140);
        const QPointF c = projectPoint(m_dragScene[0], m_dragScene[1],
                                       m_dragScene[2]);
        if (sp.kind == DropKind::Shape) {
            double lo[3], hi[3];
            dropBounds(sp, m_dragScene, defaultSize(), lo, hi);
            drawWireBox(p, lo, hi, QPen(col, 1.6, Qt::DashLine));
        } else if (sp.kind == DropKind::Feed) {
            QPolygonF d; d << c+QPointF(0,-7) << c+QPointF(7,0)
                           << c+QPointF(0,7)  << c+QPointF(-7,0);
            p.setPen(QPen(QColor("#ff5252"), 1.6, Qt::DashLine));
            p.setBrush(Qt::NoBrush);
            p.drawPolygon(d);
        } else {
            p.setPen(QPen(QColor("#69d069"), 1.6, Qt::DashLine));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(c, 6, 6);
        }
        // 配置点の十字 (どの点に落ちるかを明示)
        p.setPen(QPen(col, 1));
        p.drawLine(c + QPointF(-6, 0), c + QPointF(6, 0));
        p.drawLine(c + QPointF(0, -6), c + QPointF(0, 6));

        lines << posText(m_dragScene);
        lines << (m_dragOnFloor
                  ? I18n::tr("vp_drop_floor")
                        .arg(QString::number(m_dragScene[2], 'g', 4))
                  : I18n::tr("vp_drop_viewplane"));
    } else {
        lines << m_dragWhy;
    }

    // ラベル (カーソルの右下。画面外へはみ出さないように寄せる)
    int w = 0;
    for (const QString &s : lines) w = std::max(w, fm.horizontalAdvance(s));
    const double bw = w + 16, bh = fm.height() * lines.size() + 10;
    double bx = m_dragPos.x() + 14, by = m_dragPos.y() + 14;
    bx = std::min(bx, width() - bw - 4.0);
    by = std::min(by, height() - bh - 4.0);
    const QRectF box(std::max(4.0, bx), std::max(4.0, by), bw, bh);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(15, 20, 28, 205));
    p.drawRoundedRect(box, 4, 4);
    p.setBrush(Qt::NoBrush);
    for (int i = 0; i < lines.size(); ++i) {
        p.setPen(i == 0 ? QColor(accentColor(m_domain)).lighter(150)
                        : (m_dragOk ? QColor(255,255,255,185)
                                    : QColor(245, 158, 11)));
        p.drawText(QRectF(box.x() + 8, box.y() + 5 + fm.height() * i,
                          box.width() - 16, fm.height()),
                   Qt::AlignLeft | Qt::AlignVCenter, lines[i]);
    }
}

// ドロップ結果 / 拒否理由の一時表示 (画面下部)
void Viewport3D::drawDropMessage(QPainter &p)
{
    if (m_dropMsg.isEmpty()) return;
    const QFontMetrics fm(p.font());
    // 長い理由文は折り返す
    const int maxW = std::max(120, width() - 40);
    const QRectF need = fm.boundingRect(QRect(0, 0, maxW, 1000),
                                        Qt::TextWordWrap, m_dropMsg);
    const QRectF box((width() - need.width()) / 2.0 - 10,
                     height() - need.height() - 40,
                     need.width() + 20, need.height() + 12);
    p.setPen(QPen(m_dropMsgOk ? QColor(105, 208, 105, 200)
                              : QColor(245, 158, 11, 200), 1));
    p.setBrush(QColor(15, 20, 28, 215));
    p.drawRoundedRect(box, 5, 5);
    p.setPen(m_dropMsgOk ? QColor(210, 245, 210) : QColor(245, 158, 11));
    p.drawText(box.adjusted(10, 6, -10, -6),
               Qt::AlignLeft | Qt::TextWordWrap, m_dropMsg);
    p.setBrush(Qt::NoBrush);
}
