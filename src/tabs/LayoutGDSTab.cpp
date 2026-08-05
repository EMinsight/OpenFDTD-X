// LayoutGDSTab.cpp
#include "LayoutGDSTab.h"
#include "TabHelpers.h"
#include "../core/Project.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QFont>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

using namespace ofd;

// ── タブ専用の翻訳キー (接頭辞 gds_) ────────────────────────────────────────
namespace {
const bool s_i18n = [] {
    using ofd::I18n;
    I18n::reg("gds_layout_section", "GDSII レイアウト / Photonic IC layout",
              "Photonic IC layout (GDSII)");
    I18n::reg("gds_layout_hint",
              "KLayout / SiEPIC PDK / RSoft CAD 互換のフォトニックICレイアウトワークスペース。\n"
              "PCell (パラメトリックセル) を配置し、自動配線でフォトニック回路を構築。",
              "Photonic IC layout workspace compatible with KLayout / SiEPIC PDK / RSoft CAD.\n"
              "Place PCells (parametric cells) and build photonic circuits with automatic routing.");
    I18n::reg("gds_pdk",        "プロセス (PDK)", "Process (PDK)");
    I18n::reg("gds_pdk_custom", "カスタム PDK",   "Custom PDK");
    I18n::reg("gds_chip",       "チップサイズ",   "Chip size");
    I18n::reg("gds_grid",       "グリッド",       "Grid");
    I18n::reg("gds_layers_section", "レイヤー / Layers", "Layers");
    I18n::reg("gds_col_layer",  "レイヤー",   "Layer");
    I18n::reg("gds_col_gdsnum", "GDS番号",    "GDS number");
    I18n::reg("gds_col_use",    "用途",       "Purpose");
    I18n::reg("gds_col_color",  "色",         "Color");
    I18n::reg("gds_use_si",     "Si導波路コア",       "Si waveguide core");
    I18n::reg("gds_use_etch1",  "浅エッチ (220→90nm)", "Shallow etch (220→90nm)");
    I18n::reg("gds_use_etch2",  "深エッチ",           "Deep etch");
    I18n::reg("gds_use_metal",  "金属配線",           "Metal wiring");
    I18n::reg("gds_use_via",    "ビア",               "Via");
    I18n::reg("gds_use_heater", "熱光学ヒーター",     "Thermo-optic heater");
    I18n::reg("gds_use_pad",    "パッド",             "Pad");
    I18n::reg("gds_use_text",   "ラベル",             "Label");
    I18n::reg("gds_cells_section", "配置済みセル / Placed cells (PCells)",
              "Placed cells (PCells)");
    I18n::reg("gds_col_name",     "名前",       "Name");
    I18n::reg("gds_col_celltype", "セルタイプ", "Cell type");
    I18n::reg("gds_col_params",   "パラメータ", "Parameters");
    I18n::reg("gds_col_pos",      "位置",       "Position");
    I18n::reg("gds_cells_note",
              "一覧はプロジェクトの形状ユニットを XY 平面へ投影したものです "
              "(ジオメトリタブで編集)。GDS ファイルの取込・書き出しは未実装なので、"
              "PCell (パラメトリックセル) としての情報は持ちません。",
              "The list is this project's shape units projected onto the XY plane "
              "(edit them in the Geometry tab). GDS import/export is not "
              "implemented, so no parametric-cell (PCell) data is available.");
    I18n::reg("gds_cells_empty",
              "形状がありません — ジオメトリタブで追加してください",
              "No shapes — add them in the Geometry tab");
    I18n::reg("gds_cells_skipped",
              "うち %1 ユニットは外接直方体が一意でない形状 (三角柱/角錐台/円錐台) "
              "のため一覧・DRC から除外しています。",
              "%1 unit(s) are excluded from the list and the DRC because their "
              "bounding box is not unique (prisms / pyramids / cones).");
    I18n::reg("gds_drc_section", "DRC (デザインルールチェック)",
              "DRC (design rule check)");
    I18n::reg("gds_drc_hint",
              "上の一覧 (形状ユニットの XY 投影) に対する幾何チェックです。"
              "線幅・間隔・密度は実際に計算しています。曲率半径とパッド間隔は"
              "対応するデータがモデルに無いため評価しません (—)。",
              "Geometric check over the list above (XY footprints of the shape "
              "units). Line width, spacing and density are actually computed. "
              "Bend radius and pad spacing are not evaluated (—) because the "
              "model holds no such data.");
    I18n::reg("gds_drc_note",
              "しきい値は 220nm SOI シリコンフォトニクスの代表値です "
              "(上の PDK 選択とは未連動)。間隔は矩形間のユークリッド距離、"
              "密度は投影面積の和 (重なりを除いた実面積) / 解析領域の XY 面積。",
              "Thresholds are representative values for 220nm SOI silicon "
              "photonics (not linked to the PDK selector above). Spacing is the "
              "Euclidean distance between footprints; density is their union "
              "area divided by the XY area of the analysis region.");
    I18n::reg("gds_drc_na", "対象外", "Not evaluated");
    I18n::reg("gds_drc_toomany",
              "セル数が多いため未計算", "Too many cells — not computed");
    I18n::reg("gds_drc_worst", "最小 %1", "min %1");
    I18n::reg("gds_drc_density_val", "ρ_Si = %1", "ρ_Si = %1");
    I18n::reg("gds_col_rule",       "ルール", "Rule");
    I18n::reg("gds_col_violations", "違反数", "Violations");
    I18n::reg("gds_rule_width",   "最小線幅 Si ≥ 80nm",   "Min. line width Si ≥ 80nm");
    I18n::reg("gds_rule_space",   "最小間隔 Si ≥ 100nm",  "Min. spacing Si ≥ 100nm");
    I18n::reg("gds_rule_bend",    "最小曲率半径 ≥ 5μm",   "Min. bend radius ≥ 5μm");
    I18n::reg("gds_rule_pad",     "パッド間隔 ≥ 50μm",    "Pad spacing ≥ 50μm");
    I18n::reg("gds_rule_density", "密度 0.3 ≤ ρ_Si ≤ 0.7", "Density 0.3 ≤ ρ_Si ≤ 0.7");
    I18n::reg("gds_run_drc", "▶ DRC 実行",        "▶ Run DRC");
    I18n::reg("gds_export",  "📤 GDS エクスポート", "📤 GDS export");
    I18n::reg("gds_import",  "📥 GDS インポート",   "📥 GDS import");
    I18n::reg("gds_fdtd_section", "FDTD-IC 連携 / FDTD ↔ IC layout",
              "FDTD ↔ IC layout");
    I18n::reg("gds_fdtd_hint",
              "選択した領域だけFDTDで詳細解析し、残りはSパラメータライブラリで"
              "回路シミュレーションする構想 (未実装)。",
              "Concept: only the selected region is analyzed rigorously with FDTD, "
              "the rest as a circuit simulation from the S-parameter library "
              "(not implemented).");
    I18n::reg("gds_fdtd_sel", "選択セルをFDTD解析",
              "FDTD-analyze selected cells");
    I18n::reg("gds_fdtd_lib", "他セルはSパラメータライブラリ参照",
              "Other cells use the S-parameter library");
    I18n::reg("gds_fdtd_rerun", "🔍 選択範囲をFDTDで再解析",
              "🔍 Re-analyze selection with FDTD");
    return true;
}();

// muted text-sm 相当
QLabel *mutedLabel(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setWordWrap(true);
    l->setStyleSheet("color:#7A7A7A; font-size:11px;");
    return l;
}

// チェック列 (mock の <input type="checkbox">) 相当のアイテム
QTableWidgetItem *checkItem(bool checked)
{
    auto *it = new QTableWidgetItem;
    it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
    it->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    return it;
}

// ── レイアウト平面 (XY) への投影 (Footprint は LayoutGDSTab.h) ─────────────
// DRC しきい値 (220nm SOI シリコンフォトニクスの代表値。i18n の
// gds_rule_* の文言と必ず対で直すこと)。単位は m。
const double kMinWidth_m   = 80e-9;
const double kMinSpacing_m = 100e-9;
const double kDensityMin   = 0.30;
const double kDensityMax   = 0.70;
// union 面積 (座標圧縮 = O(n³) 相当) を GUI スレッドで回してよい上限セル数。
// 典型的な FDTD モデルは数十ユニットなので実用上ここに当たらない。
const int    kDensityMaxCells = 64;

// 形状ユニット → XY フットプリント。外接直方体が g[0..5] で決まる形状
// (直方体 / 楕円体 / 円柱) だけを対象にし、それ以外の数を skipped で返す。
// 材質 0 (空気) は構造物ではないので除く。
QVector<Footprint> footprints(const QVector<Geometry> &geos, int *skipped)
{
    QVector<Footprint> out;
    int skip = 0;
    for (int i = 0; i < geos.size(); ++i) {
        const Geometry &g = geos[i];
        if (g.materialId == 0) continue;                 // 空気ユニット
        if (Geometry::paramCount(g.shape) != 6) { ++skip; continue; }
        Footprint f;
        f.name = g.name.isEmpty() ? QStringLiteral("unit%1").arg(i + 1) : g.name;
        f.x0 = std::min(g.g[0], g.g[1]);
        f.x1 = std::max(g.g[0], g.g[1]);
        f.y0 = std::min(g.g[2], g.g[3]);
        f.y1 = std::max(g.g[2], g.g[3]);
        out.push_back(f);
    }
    if (skipped) *skipped = skip;
    return out;
}

// 矩形間の最短距離 [m]。重なっている (または内包) 場合は負を返す。
double rectGap(const Footprint &a, const Footprint &b)
{
    const double dx = std::max(a.x0 - b.x1, b.x0 - a.x1);
    const double dy = std::max(a.y0 - b.y1, b.y0 - a.y1);
    if (dx <= 0 && dy <= 0) return -1.0;                  // 重なり
    return std::hypot(std::max(dx, 0.0), std::max(dy, 0.0));
}

// 矩形群の和集合面積 [m²] (座標圧縮 — 軸平行なので厳密)。
double unionArea(const QVector<Footprint> &f)
{
    QVector<double> xs, ys;
    for (const Footprint &r : f) { xs << r.x0 << r.x1; ys << r.y0 << r.y1; }
    std::sort(xs.begin(), xs.end());
    std::sort(ys.begin(), ys.end());
    xs.erase(std::unique(xs.begin(), xs.end()), xs.end());
    ys.erase(std::unique(ys.begin(), ys.end()), ys.end());
    double sum = 0;
    for (int i = 0; i + 1 < xs.size(); ++i) {
        const double cx = 0.5 * (xs[i] + xs[i + 1]);
        for (int j = 0; j + 1 < ys.size(); ++j) {
            const double cy = 0.5 * (ys[j] + ys[j + 1]);
            for (const Footprint &r : f) {
                if (cx > r.x0 && cx < r.x1 && cy > r.y0 && cy < r.y1) {
                    sum += (xs[i + 1] - xs[i]) * (ys[j + 1] - ys[j]);
                    break;
                }
            }
        }
    }
    return sum;
}

// m → 表示単位。1μm 未満は nm、それ以上は μm で出す。
QString lengthText(double m)
{
    if (m < 1e-6) return QStringLiteral("%1 nm").arg(m * 1e9, 0, 'g', 3);
    return QStringLiteral("%1 μm").arg(m * 1e6, 0, 'g', 4);
}
} // namespace

LayoutGDSTab::LayoutGDSTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    const QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);

    // GDSII レイアウト / Photonic IC layout
    auto *sTop = new SectionBox(I18n::tr("gds_layout_section"), body);
    sTop->vbox()->addWidget(mutedLabel(I18n::tr("gds_layout_hint"), sTop));

    m_pdk = new QComboBox(sTop);
    m_pdk->addItem("AMF Silicon Photonics 220nm");
    m_pdk->addItem("IMEC iSiPP200");
    m_pdk->addItem("GlobalFoundries 45SPCLO");
    m_pdk->addItem("LiNbO3 on Insulator (LNOI)");
    m_pdk->addItem("SiN (Ligentec/CSEM)");
    m_pdk->addItem(I18n::tr("gds_pdk_custom"));
    sTop->form()->addRow(I18n::tr("gds_pdk"), m_pdk);

    m_chipW = new QLineEdit("5000", sTop);
    m_chipW->setMaximumWidth(100);
    m_chipH = new QLineEdit("5000", sTop);
    m_chipH->setMaximumWidth(100);
    auto *chipRow = new QHBoxLayout();
    chipRow->addWidget(m_chipW);
    chipRow->addWidget(new QLabel(QString::fromUtf8("×"), sTop));
    chipRow->addWidget(m_chipH);
    chipRow->addWidget(new QLabel(QString::fromUtf8("μm"), sTop));
    chipRow->addStretch(1);
    sTop->form()->addRow(I18n::tr("gds_chip"), chipRow);

    m_grid = new QComboBox(sTop);
    m_grid->addItems({ "Manhattan", "DBU (1nm)", "Hexagonal" });
    m_grid->setCurrentIndex(1);        // mock: value="db"
    sTop->form()->addRow(I18n::tr("gds_grid"), m_grid);
    // このフォームはまだ計算へ配線されていない (apply/refresh 不在)
    sTop->vbox()->addWidget(tabhelp::unwiredNote(sTop));
    v->addWidget(sTop);

    // レイヤー / Layers
    auto *sLay = new SectionBox(I18n::tr("gds_layers_section"), body);
    m_layers = new QTableWidget(8, 5, sLay);
    m_layers->setHorizontalHeaderLabels({
        "", I18n::tr("gds_col_layer"), I18n::tr("gds_col_gdsnum"),
        I18n::tr("gds_col_use"), I18n::tr("gds_col_color") });
    m_layers->verticalHeader()->setVisible(false);
    m_layers->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_layers->setColumnWidth(0, 26);
    m_layers->setColumnWidth(4, 40);
    m_layers->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_layers->setMinimumHeight(250);
    const struct { const char *name, *gds, *useKey, *color; } layers[8] = {
        { "Si",       "1/0",  "gds_use_si",     "#3B82F6" },
        { "Si_etch1", "2/0",  "gds_use_etch1",  "#1E40AF" },
        { "Si_etch2", "3/0",  "gds_use_etch2",  "#1E3A8A" },
        { "Metal1",   "11/0", "gds_use_metal",  "#FBBF24" },
        { "Via1",     "12/0", "gds_use_via",    "#F59E0B" },
        { "Heater",   "21/0", "gds_use_heater", "#EF4444" },
        { "Pad",      "31/0", "gds_use_pad",    "#10B981" },
        { "Text",     "99/0", "gds_use_text",   "#6B7280" },
    };
    for (int i = 0; i < 8; ++i) {
        m_layers->setItem(i, 0, checkItem(true));
        m_layers->setItem(i, 1, new QTableWidgetItem(
            QString::fromUtf8(layers[i].name)));
        auto *gd = new QTableWidgetItem(QString::fromUtf8(layers[i].gds));
        gd->setFont(mono);
        m_layers->setItem(i, 2, gd);
        m_layers->setItem(i, 3, new QTableWidgetItem(
            I18n::tr(layers[i].useKey)));
        auto *col = new QTableWidgetItem;
        col->setBackground(QColor(layers[i].color));
        col->setFlags(Qt::ItemIsEnabled);
        m_layers->setItem(i, 4, col);
    }
    sLay->vbox()->addWidget(m_layers);
    v->addWidget(sLay);

    // 配置済みセル / Placed cells — プロジェクトの形状ユニットの XY 投影
    auto *sCells = new SectionBox(I18n::tr("gds_cells_section"), body);
    m_cells = new QTableWidget(0, 5, sCells);
    m_cells->setHorizontalHeaderLabels({
        "#", I18n::tr("gds_col_name"), I18n::tr("gds_col_celltype"),
        I18n::tr("gds_col_params"), I18n::tr("gds_col_pos") });
    m_cells->verticalHeader()->setVisible(false);
    m_cells->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_cells->setColumnWidth(0, 30);
    m_cells->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_cells->setMinimumHeight(200);
    sCells->vbox()->addWidget(m_cells);
    sCells->vbox()->addWidget(mutedLabel(I18n::tr("gds_cells_note"), sCells));
    m_cellsSkipped = mutedLabel(QString(), sCells);
    sCells->vbox()->addWidget(m_cellsSkipped);
    v->addWidget(sCells);

    // DRC (デザインルールチェック)
    auto *sDrc = new SectionBox(I18n::tr("gds_drc_section"), body);
    sDrc->vbox()->addWidget(mutedLabel(I18n::tr("gds_drc_hint"), sDrc));
    m_drc = new QTableWidget(5, 4, sDrc);
    m_drc->setHorizontalHeaderLabels({
        "", I18n::tr("gds_col_rule"), I18n::tr("gds_col_violations"),
        I18n::tr("gds_col_pos") });
    m_drc->verticalHeader()->setVisible(false);
    m_drc->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_drc->setColumnWidth(0, 40);
    m_drc->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_drc->setMinimumHeight(170);
    sDrc->vbox()->addWidget(m_drc);
    sDrc->vbox()->addWidget(mutedLabel(I18n::tr("gds_drc_note"), sDrc));
    auto *drcRow = new QHBoxLayout();
    // DRC は実計算する。GDS 入出力は未実装 — 無効化して明示する (絶対規則 5)
    auto *btnDrc    = new QPushButton(I18n::tr("gds_run_drc"), sDrc);
    connect(btnDrc, &QPushButton::clicked, this, [this] { refreshLayout(); });
    auto *btnExport = new QPushButton(I18n::tr("gds_export"), sDrc);
    auto *btnImport = new QPushButton(I18n::tr("gds_import"), sDrc);
    drcRow->addWidget(btnDrc);
    for (QPushButton *b : { btnExport, btnImport }) {
        tabhelp::markNotImplemented(b);
        drcRow->addWidget(b);
    }
    drcRow->addStretch(1);
    sDrc->vbox()->addLayout(drcRow);
    v->addWidget(sDrc);

    // FDTD-IC 連携 / FDTD ↔ IC layout
    auto *sFdtd = new SectionBox(I18n::tr("gds_fdtd_section"), body);
    sFdtd->vbox()->addWidget(mutedLabel(I18n::tr("gds_fdtd_hint"), sFdtd));
    auto *chkRow = new QHBoxLayout();
    auto *chkSel = new QCheckBox(I18n::tr("gds_fdtd_sel"), sFdtd);
    chkSel->setChecked(true);
    auto *chkLib = new QCheckBox(I18n::tr("gds_fdtd_lib"), sFdtd);
    chkLib->setChecked(true);
    chkRow->addWidget(chkSel);
    chkRow->addWidget(chkLib);
    chkRow->addStretch(1);
    sFdtd->vbox()->addLayout(chkRow);
    // チェックはどこにも読まれていない (apply/refresh 不在)
    sFdtd->vbox()->addWidget(tabhelp::unwiredNote(sFdtd));
    auto *rerunRow = new QHBoxLayout();
    // FDTD 再解析は未実装 — primary (実行可能な見た目) を外して無効化 (絶対規則 5)
    auto *rerun = new QPushButton(I18n::tr("gds_fdtd_rerun"), sFdtd);
    tabhelp::markNotImplemented(rerun);
    rerunRow->addWidget(rerun);
    rerunRow->addStretch(1);
    sFdtd->vbox()->addLayout(rerunRow);
    v->addWidget(sFdtd);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    // 形状の追加・削除・移動、ファイル読込に追従する
    connect(project, &Project::changed, this, [this] { refreshLayout(); });
    connect(project, &Project::loaded,  this, [this] { refreshLayout(); });
    refreshLayout();
}

// ── プロジェクト形状 → セル一覧 + DRC ───────────────────────────────────────
void LayoutGDSTab::refreshLayout()
{
    int skipped = 0;
    const QVector<Footprint> foots = footprints(m_p->geometries(), &skipped);
    // Project::changed は全タブの編集で飛んでくるので、形状が変わっていない
    // ときは DRC (対総当たり + 和集合面積) を回さない
    if (m_lastSkipped >= 0 && skipped == m_lastSkipped && foots == m_lastFoots)
        return;
    m_lastFoots = foots;
    m_lastSkipped = skipped;
    rebuildCells(foots, skipped);
    rebuildDrc(foots);
}

void LayoutGDSTab::rebuildCells(const QVector<Footprint> &foots, int skipped)
{
    const QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    const QVector<Geometry> &geos = m_p->geometries();
    const QVector<Material> &mats = m_p->materials();

    m_cells->clearContents();
    m_cells->clearSpans();   // 前回の結合セルを解除
    if (foots.isEmpty()) {
        m_cells->setRowCount(1);
        m_cells->setItem(0, 0,
            new QTableWidgetItem(I18n::tr("gds_cells_empty")));
        m_cells->setSpan(0, 0, 1, 5);
        m_cellsSkipped->setText(skipped > 0
            ? I18n::tr("gds_cells_skipped").arg(skipped) : QString());
        return;
    }
    // フットプリントと形状ユニットの対応 (footprints() と同じ順・同じ条件)
    QVector<int> src;
    for (int i = 0; i < geos.size(); ++i) {
        if (geos[i].materialId == 0) continue;
        if (Geometry::paramCount(geos[i].shape) != 6) continue;
        src.push_back(i);
    }
    m_cells->setRowCount(foots.size());
    for (int r = 0; r < foots.size(); ++r) {
        const Footprint &f = foots[r];
        const Geometry &g = geos[src[r]];
        m_cells->setItem(r, 0, new QTableWidgetItem(QString::number(r + 1)));
        m_cells->setItem(r, 1, new QTableWidgetItem(f.name));
        // セルタイプ = 材質名 + 形状名 (共通キー ge_shape_<code>)
        QString matName;
        if (g.materialId == 1) matName = QStringLiteral("PEC");
        else if (g.materialId - 2 >= 0 && g.materialId - 2 < mats.size())
            matName = mats[g.materialId - 2].name;
        if (matName.isEmpty())
            matName = QStringLiteral("material %1").arg(g.materialId);
        m_cells->setItem(r, 2, new QTableWidgetItem(
            matName + " / " + I18n::tr(QStringLiteral("ge_shape_%1").arg(g.shape))));
        // パラメータ = XY 寸法 + 厚み (Z)
        auto *par = new QTableWidgetItem(QStringLiteral("%1 × %2, t=%3")
            .arg(lengthText(f.width()), lengthText(f.height()),
                 lengthText(std::abs(g.g[5] - g.g[4]))));
        par->setFont(mono);
        m_cells->setItem(r, 3, par);
        auto *pos = new QTableWidgetItem(QStringLiteral("(%1, %2) μm")
            .arg(0.5 * (f.x0 + f.x1) * 1e6, 0, 'g', 4)
            .arg(0.5 * (f.y0 + f.y1) * 1e6, 0, 'g', 4));
        pos->setFont(mono);
        m_cells->setItem(r, 4, pos);
    }
    m_cellsSkipped->setText(skipped > 0
        ? I18n::tr("gds_cells_skipped").arg(skipped) : QString());
}

// 幾何 DRC。線幅・間隔・密度は実計算し、モデルに対応データが無いルール
// (曲率半径 / パッド間隔) は「対象外」を出す (0 件と偽らない)。
void LayoutGDSTab::rebuildDrc(const QVector<Footprint> &foots)
{
    const QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);

    struct Result { int state; int count; QString detail; };  // state: 1=OK 0=NG -1=対象外
    Result res[5];
    const QString dash = QString::fromUtf8("—");

    // 1) 最小線幅 — 各フットプリントの短辺
    if (foots.isEmpty()) {
        res[0] = { -1, 0, I18n::tr("gds_cells_empty") };
    } else {
        int bad = 0;
        double worst = 1e300;
        QString who;
        for (const Footprint &f : foots) {
            if (f.minDim() < worst) { worst = f.minDim(); who = f.name; }
            if (f.minDim() < kMinWidth_m) ++bad;
        }
        res[0] = { bad ? 0 : 1, bad,
                   I18n::tr("gds_drc_worst").arg(lengthText(worst))
                       + " (" + who + ")" };
    }

    // 2) 最小間隔 — 重なっていない矩形対のユークリッド距離
    if (foots.size() < 2) {
        res[1] = { -1, 0, dash };
    } else {
        int bad = 0;
        double worst = 1e300;
        QString who;
        for (int i = 0; i < foots.size(); ++i)
            for (int j = i + 1; j < foots.size(); ++j) {
                const double d = rectGap(foots[i], foots[j]);
                if (d <= 0) continue;              // 重なり/接触は間隔違反にしない
                if (d < worst) {
                    worst = d;
                    who = foots[i].name + " ↔ " + foots[j].name;
                }
                if (d < kMinSpacing_m) ++bad;
            }
        res[1] = who.isEmpty()
            ? Result{ -1, 0, dash }
            : Result{ bad ? 0 : 1, bad,
                      I18n::tr("gds_drc_worst").arg(lengthText(worst))
                          + " (" + who + ")" };
    }

    // 3) 最小曲率半径 — 曲線導波路はモデルに無い (直方体/円柱ユニットのみ)
    res[2] = { -1, 0, I18n::tr("gds_drc_na") };
    // 4) パッド間隔 — パッド層はモデルに無い
    res[3] = { -1, 0, I18n::tr("gds_drc_na") };

    // 5) 密度 ρ_Si = フットプリントの和集合面積 / 解析領域の XY 面積
    const double regionArea = (m_p->mesh(0).max() - m_p->mesh(0).min())
                            * (m_p->mesh(1).max() - m_p->mesh(1).min());
    if (foots.isEmpty() || regionArea <= 0) {
        res[4] = { -1, 0, dash };
    } else if (foots.size() > kDensityMaxCells) {
        res[4] = { -1, 0, I18n::tr("gds_drc_toomany") };
    } else {
        const double rho = unionArea(foots) / regionArea;
        const bool ok = (rho >= kDensityMin && rho <= kDensityMax);
        // 解析領域が構造より遥かに広いと ρ は極端に小さくなるので有効数字で出す
        res[4] = { ok ? 1 : 0, ok ? 0 : 1,
                   I18n::tr("gds_drc_density_val")
                       .arg(QString::number(rho, 'g', 3)) };
    }

    static const char *kRuleKeys[5] = { "gds_rule_width", "gds_rule_space",
                                        "gds_rule_bend", "gds_rule_pad",
                                        "gds_rule_density" };
    m_drc->setRowCount(5);
    for (int i = 0; i < 5; ++i) {
        const int st = res[i].state;
        auto *stIt = new QTableWidgetItem(st == 1 ? QStringLiteral("OK")
                                        : st == 0 ? QStringLiteral("!")
                                                  : dash);
        stIt->setTextAlignment(Qt::AlignCenter);
        stIt->setForeground(st == 1 ? QColor("#2E7D32")
                          : st == 0 ? QColor("#B06B0F")
                                    : QColor("#7A7A7A"));
        QFont bold = stIt->font();
        bold.setBold(true);
        stIt->setFont(bold);
        m_drc->setItem(i, 0, stIt);
        m_drc->setItem(i, 1, new QTableWidgetItem(I18n::tr(kRuleKeys[i])));
        // 評価していないルールは違反数を 0 と偽らず「—」にする
        auto *cnt = new QTableWidgetItem(st < 0 ? dash
                                                : QString::number(res[i].count));
        cnt->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_drc->setItem(i, 2, cnt);
        auto *det = new QTableWidgetItem(res[i].detail);
        det->setFont(mono);
        m_drc->setItem(i, 3, det);
    }
}
