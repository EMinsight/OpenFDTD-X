// DisplayOpticsTab.cpp
#include "DisplayOpticsTab.h"
#include "../core/Project.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QStringList>
#include <QTableWidget>
#include <QVBoxLayout>

using namespace ofd;

// ── タブ専用語彙 (file-local 登録, 接頭辞 dpo_) ──────────────────────────────
namespace {
const bool s_i18n = [] {
    using ofd::I18n;
    // 上段
    I18n::reg("dpo_title", "ディスプレイ・AR/VR光学 / Display & near-eye optics",
              "Display & near-eye optics");
    I18n::reg("dpo_hint",
              "FDTD/RCWAで格子・微細構造を、レイトレースで導光と瞳を解析する"
              "マルチスケール設計。",
              "Multi-scale design: FDTD/RCWA for gratings and fine structures, ray "
              "tracing for light guiding and the pupil.");
    I18n::reg("dpo_device", "デバイス", "Device");
    I18n::reg("dpo_dev_arwg",     "AR導波路コンバイナ", "AR waveguide combiner");
    I18n::reg("dpo_dev_oled",     "OLED光取り出し", "OLED outcoupling");
    I18n::reg("dpo_dev_microled", "microLED", "microLED");
    I18n::reg("dpo_dev_lcd",      "LCD/偏光系", "LCD / polarization optics");

    // 導波路コンバイナ
    I18n::reg("dpo_wg_sec", "導波路コンバイナ / Waveguide combiner",
              "Waveguide combiner");
    I18n::reg("dpo_wg_type", "方式", "Type");
    I18n::reg("dpo_wg_sr",  "表面レリーフ格子 (SRG)", "Surface-relief grating (SRG)");
    I18n::reg("dpo_wg_vhg", "体積ホログラム (VHG)", "Volume hologram (VHG)");
    I18n::reg("dpo_wg_pvg", "偏光体積格子 (PVG)", "Polarization volume grating (PVG)");
    I18n::reg("dpo_wg_geo", "幾何 (ハーフミラーアレイ)",
              "Geometric (half-mirror array)");
    I18n::reg("dpo_substrate", "基板", "Substrate");
    I18n::reg("dpo_sub_unit", "mm 厚 · n=1.80 (高屈折率ガラス)",
              "mm thick · n=1.80 (high-index glass)");
    I18n::reg("dpo_grating", "格子", "Grating");
    I18n::reg("dpo_g_period", "周期", "Period");
    I18n::reg("dpo_g_depth",  "深さ", "Depth");
    I18n::reg("dpo_g_slant",  "斜め角", "Slant angle");
    I18n::reg("dpo_nm_dot", "nm ·", "nm ·");
    I18n::reg("dpo_three_grat", "入力/折返し/出力の3格子構成",
              "Three-grating layout (in-coupler / fold / out-coupler)");
    I18n::reg("dpo_rcwa_opt", "RCWAで回折効率を最適化",
              "Optimize diffraction efficiency with RCWA");

    // 評価
    I18n::reg("dpo_metric_sec", "評価 / Metrics", "Metrics");
    I18n::reg("dpo_c_metric", "指標", "Metric");
    I18n::reg("dpo_c_value",  "値", "Value");
    I18n::reg("dpo_c_target", "目標", "Target");
    I18n::reg("dpo_c_judge",  "判定", "Verdict");
    I18n::reg("dpo_pass",   "適合", "Pass");
    I18n::reg("dpo_needwork", "要改善", "Needs work");
    I18n::reg("dpo_m_fov",     "視野角 FOV (対角)", "Field of view (diagonal)");
    I18n::reg("dpo_m_eyebox",  "アイボックス", "Eyebox");
    I18n::reg("dpo_m_eff",     "光効率", "Optical efficiency");
    I18n::reg("dpo_m_lumunif", "輝度均一性", "Luminance uniformity");
    I18n::reg("dpo_m_colunif", "色均一性 (Δu'v')", "Color uniformity (Δu'v')");
    I18n::reg("dpo_m_stray",   "迷光 (レインボー)", "Stray light (rainbow)");
    I18n::reg("dpo_m_seethru", "シースルー透過率", "See-through transmittance");
    I18n::reg("dpo_btn_eyebox", "🗺 アイボックス輝度マップ",
              "🗺 Eyebox luminance map");
    I18n::reg("dpo_btn_tradeoff", "📊 FOV-効率トレードオフ",
              "📊 FOV / efficiency trade-off");

    // OLED
    I18n::reg("dpo_oled_sec", "OLED 光取り出し / Outcoupling", "OLED outcoupling");
    I18n::reg("dpo_structure", "構造", "Structure");
    I18n::reg("dpo_o_bottom", "ボトムエミッション", "Bottom emission");
    I18n::reg("dpo_o_top",    "トップエミッション", "Top emission");
    I18n::reg("dpo_o_cavity", "マイクロキャビティ", "Microcavity");
    I18n::reg("dpo_stack", "層構成", "Layer stack");
    I18n::reg("dpo_stack_hint", "ITO/HTL/EML/ETL/Cathode — 材料Explorer から n,k 参照",
              "ITO/HTL/EML/ETL/Cathode — n, k taken from the material explorer");
    I18n::reg("dpo_o_iqe",  "内部量子効率 IQE", "Internal quantum efficiency IQE");
    I18n::reg("dpo_o_spp",  "表面プラズモン損失を分離",
              "Separate the surface-plasmon loss");
    I18n::reg("dpo_o_wgloss", "導波モード損失", "Waveguided-mode loss");
    I18n::reg("dpo_extract", "取り出し構造", "Outcoupling structure");
    I18n::reg("dpo_e_none",  "なし", "None");
    I18n::reg("dpo_e_mlens", "マイクロレンズ", "Microlens");
    I18n::reg("dpo_e_scat",  "散乱層", "Scattering layer");
    I18n::reg("dpo_e_phc",   "フォトニック結晶", "Photonic crystal");
    I18n::reg("dpo_eqe", "外部量子効率 EQE = 24.8%",
              "External quantum efficiency EQE = 24.8%");
    I18n::reg("dpo_oled_detail",
              "取り出し効率 32% · 視野角色シフト Δu'v'=0.018 @60°",
              "Outcoupling efficiency 32% · off-axis color shift Δu'v' = 0.018 @60°");

    // microLED
    I18n::reg("dpo_microled_sec", "microLED 解析", "microLED analysis");
    I18n::reg("dpo_chipsize", "チップサイズ", "Chip size");
    I18n::reg("dpo_chipsize_unit", "μm 角", "μm square");
    I18n::reg("dpo_ml_recomb", "側壁再結合損失", "Sidewall recombination loss");
    I18n::reg("dpo_ml_dbr",    "サイドウォール反射 (DBR)", "Sidewall reflector (DBR)");
    I18n::reg("dpo_ml_dir",    "配光の指向性化", "Directional emission shaping");
    I18n::reg("dpo_ml_badge", "光取り出し効率 68% · 半値角 ±38°",
              "Extraction efficiency 68% · half-angle ±38°");

    // LCD/偏光系
    I18n::reg("dpo_lcd_sec", "LCD/偏光系 解析", "LCD / polarization analysis");
    I18n::reg("dpo_lcd_mode", "モード", "Mode");
    I18n::reg("dpo_lc_aniso", "液晶の異方性 (Jones/Berreman 4×4)",
              "Liquid-crystal anisotropy (Jones / Berreman 4×4)");
    I18n::reg("dpo_lc_film",  "視野角補償フィルム", "Viewing-angle compensation film");
    I18n::reg("dpo_lcd_badge", "コントラスト比 4200:1 · 視野角 178°",
              "Contrast ratio 4200:1 · viewing angle 178°");
    return true;
}();

// 評価表 (モックの <tbody> をそのまま)
struct MetricRow { const char *itemKey, *value, *target, *judgeKey, *kind; };
const MetricRow kMetrics[7] = {
    { "dpo_m_fov",     "52 °",        "≥ 50",     "dpo_pass",     "ok"   },
    { "dpo_m_eyebox",  "12 × 9 mm",   "≥ 10×8",   "dpo_pass",     "ok"   },
    { "dpo_m_eff",     "142 nit/lm",  "≥ 120",    "dpo_pass",     "ok"   },
    { "dpo_m_lumunif", "0.68",        "≥ 0.7",    "dpo_needwork", "warn" },
    { "dpo_m_colunif", "0.011",       "< 0.010",  "dpo_needwork", "warn" },
    { "dpo_m_stray",   "-32 dB",      "< -30",    "dpo_pass",     "ok"   },
    { "dpo_m_seethru", "84 %",        "≥ 80",     "dpo_pass",     "ok"   },
};

// バッジ (mock の .badge ok / warn / err / acc)
QString badgeCss(const char *kind)
{
    QString css = "border-radius:3px; padding:1px 6px; font-size:11px;";
    if (qstrcmp(kind, "ok") == 0)        css += "background:#DFF6DD; color:#0F7B0F;";
    else if (qstrcmp(kind, "warn") == 0) css += "background:#FFF4CE; color:#9D5D00;";
    else if (qstrcmp(kind, "err") == 0)  css += "background:#FDE7E9; color:#B91C1C;";
    else if (qstrcmp(kind, "acc") == 0)  css += "background:#DEECF9; color:#0078D4;";
    else                                 css += "background:palette(midlight);";
    return css;
}

QLabel *makeBadge(const QString &text, const char *kind, QWidget *parent)
{
    auto *b = new QLabel(text, parent);
    b->setTextFormat(Qt::PlainText);
    b->setStyleSheet(badgeCss(kind));
    return b;
}

// 表セル内バッジ (左寄せ)
QWidget *badgeCell(const QString &text, const char *kind)
{
    auto *w = new QWidget;
    auto *h = new QHBoxLayout(w);
    h->setContentsMargins(4, 2, 4, 2);
    h->addWidget(makeBadge(text, kind, w));
    h->addStretch(1);
    return w;
}

QLabel *hintLabel(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setTextFormat(Qt::PlainText);
    l->setWordWrap(true);
    return l;
}

QLineEdit *numEdit(const char *value, int width, QWidget *parent)
{
    auto *e = new QLineEdit(QString::fromUtf8(value), parent);
    if (width > 0) e->setMaximumWidth(width);
    return e;
}

QCheckBox *makeCheck(const QString &text, bool on, QWidget *parent)
{
    auto *c = new QCheckBox(text, parent);
    c->setChecked(on);
    return c;
}

// <Seg> 相当: 排他 checkable QPushButton の一列
QButtonGroup *segRow(QHBoxLayout *row, const QStringList &labels, int current,
                     QWidget *parent)
{
    auto *g = new QButtonGroup(parent);
    g->setExclusive(true);
    for (int i = 0; i < labels.size(); ++i) {
        auto *b = new QPushButton(labels.at(i), parent);
        b->setCheckable(true);
        b->setStyleSheet("font-size:11px; padding:2px 8px;");
        if (i == current) b->setChecked(true);
        g->addButton(b, i);
        row->addWidget(b);
    }
    row->addStretch(1);
    return g;
}

QTableWidgetItem *textItem(const QString &s)
{
    auto *it = new QTableWidgetItem(s);
    it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    return it;
}

QTableWidgetItem *numItem(const QString &s)
{
    auto *it = textItem(s);
    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return it;
}
} // namespace

// ── DisplayOpticsTab ────────────────────────────────────────────────────────
DisplayOpticsTab::DisplayOpticsTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project),
      m_device(nullptr),
      m_arwgPage(nullptr), m_oledPage(nullptr), m_sharedSec(nullptr),
      m_sharedStack(nullptr),
      m_wgType(nullptr), m_subThick(nullptr), m_gratPeriod(nullptr),
      m_gratDepth(nullptr), m_gratSlant(nullptr), m_threeGratings(nullptr),
      m_rcwaOptimize(nullptr), m_metricTable(nullptr),
      m_bottomEmission(nullptr), m_topEmission(nullptr), m_microcavity(nullptr),
      m_iqe(nullptr), m_sppLoss(nullptr), m_waveguideLoss(nullptr),
      m_outcoupling(nullptr), m_eqeBadge(nullptr), m_oledDetail(nullptr),
      m_chipSize(nullptr), m_sidewallRecomb(nullptr), m_sidewallDbr(nullptr),
      m_directional(nullptr), m_microLedBadge(nullptr),
      m_lcdMode(nullptr), m_lcAnisotropy(nullptr), m_compFilm(nullptr),
      m_lcdBadge(nullptr)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // ── ディスプレイ・AR/VR光学 (デバイス) ──────────────────────────────────
    auto *sTop = new SectionBox(I18n::tr("dpo_title"), body);
    sTop->vbox()->addWidget(hintLabel(I18n::tr("dpo_hint"), sTop));

    auto *devRow = new QHBoxLayout();
    devRow->setSpacing(4);
    m_device = segRow(devRow, { I18n::tr("dpo_dev_arwg"), I18n::tr("dpo_dev_oled"),
                                I18n::tr("dpo_dev_microled"),
                                I18n::tr("dpo_dev_lcd") },
                      0, sTop);                     // 既定 "arwg"
    sTop->form()->addRow(I18n::tr("dpo_device"), devRow);
    v->addWidget(sTop);

    // ── デバイス別セクション ────────────────────────────────────────────────
    m_arwgPage = buildArwgPage();
    v->addWidget(m_arwgPage);              // addWidget が body へ reparent する

    m_oledPage = buildOledPage();
    v->addWidget(m_oledPage);

    // microLED と LCD/偏光系 は 1 セクションを共有 (タイトルも中身も切替)
    m_sharedSec = new SectionBox(I18n::tr("dpo_microled_sec"), body);
    m_sharedStack = new QStackedWidget(m_sharedSec);
    m_sharedStack->addWidget(buildMicroLedPage());   // 0 microled
    m_sharedStack->addWidget(buildLcdPage());        // 1 lcd
    m_sharedSec->vbox()->addWidget(m_sharedStack);
    v->addWidget(m_sharedSec);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    connect(m_device, &QButtonGroup::idClicked,
            this, &DisplayOpticsTab::deviceChanged);
    deviceChanged(0);                                // 既定 "arwg"
}

// デバイス切替: mock の {dev === "…" && <>…</>} を可視性で再現
void DisplayOpticsTab::deviceChanged(int index)
{
    index = qBound(0, index, 3);
    m_arwgPage->setVisible(index == 0);
    m_oledPage->setVisible(index == 1);
    const bool shared = (index == 2 || index == 3);
    m_sharedSec->setVisible(shared);
    if (shared) {
        m_sharedStack->setCurrentIndex(index == 2 ? 0 : 1);
        m_sharedSec->setTitle(I18n::tr(index == 2 ? "dpo_microled_sec"
                                                  : "dpo_lcd_sec"));
    }
}

// ── AR導波路コンバイナ (導波路コンバイナ + 評価 の 2 セクション) ─────────────
QWidget *DisplayOpticsTab::buildArwgPage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(8);

    // 導波路コンバイナ / Waveguide combiner
    auto *sWg = new SectionBox(I18n::tr("dpo_wg_sec"), page);

    auto *tRow = new QHBoxLayout();
    tRow->setSpacing(4);
    m_wgType = segRow(tRow, { I18n::tr("dpo_wg_sr"), I18n::tr("dpo_wg_vhg"),
                              I18n::tr("dpo_wg_pvg"), I18n::tr("dpo_wg_geo") },
                      0, sWg);                       // 既定 "sr"
    sWg->form()->addRow(I18n::tr("dpo_wg_type"), tRow);

    auto *subRow = new QHBoxLayout();
    m_subThick = numEdit("0.7", 70, sWg);
    subRow->addWidget(m_subThick);
    subRow->addWidget(new QLabel(I18n::tr("dpo_sub_unit"), sWg));
    subRow->addStretch(1);
    sWg->form()->addRow(I18n::tr("dpo_substrate"), subRow);

    auto *gRow = new QHBoxLayout();
    m_gratPeriod = numEdit("385", 70, sWg);
    m_gratDepth  = numEdit("220", 70, sWg);
    m_gratSlant  = numEdit("30",  70, sWg);
    gRow->addWidget(new QLabel(I18n::tr("dpo_g_period"), sWg));
    gRow->addWidget(m_gratPeriod);
    gRow->addWidget(new QLabel(I18n::tr("dpo_nm_dot"), sWg));
    gRow->addWidget(new QLabel(I18n::tr("dpo_g_depth"), sWg));
    gRow->addWidget(m_gratDepth);
    gRow->addWidget(new QLabel(I18n::tr("dpo_nm_dot"), sWg));
    gRow->addWidget(new QLabel(I18n::tr("dpo_g_slant"), sWg));
    gRow->addWidget(m_gratSlant);
    gRow->addWidget(new QLabel("°", sWg));
    gRow->addStretch(1);
    sWg->form()->addRow(I18n::tr("dpo_grating"), gRow);

    auto *ckRow = new QHBoxLayout();
    m_threeGratings = makeCheck(I18n::tr("dpo_three_grat"), true, sWg);
    m_rcwaOptimize  = makeCheck(I18n::tr("dpo_rcwa_opt"),   true, sWg);
    ckRow->addWidget(m_threeGratings);
    ckRow->addWidget(m_rcwaOptimize);
    ckRow->addStretch(1);
    sWg->form()->addRow(ckRow);
    v->addWidget(sWg);

    // 評価 / Metrics
    auto *sMe = new SectionBox(I18n::tr("dpo_metric_sec"), page);
    m_metricTable = new QTableWidget(7, 4, sMe);
    m_metricTable->setHorizontalHeaderLabels({ I18n::tr("dpo_c_metric"),
                                               I18n::tr("dpo_c_value"),
                                               I18n::tr("dpo_c_target"),
                                               I18n::tr("dpo_c_judge") });
    m_metricTable->verticalHeader()->setVisible(false);
    m_metricTable->verticalHeader()->setDefaultSectionSize(26);
    m_metricTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_metricTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_metricTable->setMinimumHeight(220);
    for (int r = 0; r < 7; ++r) {
        const MetricRow &m = kMetrics[r];
        m_metricTable->setItem(r, 0, textItem(I18n::tr(m.itemKey)));
        m_metricTable->setItem(r, 1, numItem(QString::fromUtf8(m.value)));
        m_metricTable->setItem(r, 2, numItem(QString::fromUtf8(m.target)));
        m_metricTable->setCellWidget(r, 3,
                                     badgeCell(I18n::tr(m.judgeKey), m.kind));
    }
    sMe->vbox()->addWidget(m_metricTable);

    auto *btnRow = new QHBoxLayout();
    btnRow->addWidget(new QPushButton(I18n::tr("dpo_btn_eyebox"), sMe));
    btnRow->addWidget(new QPushButton(I18n::tr("dpo_btn_tradeoff"), sMe));
    btnRow->addStretch(1);
    sMe->vbox()->addLayout(btnRow);
    v->addWidget(sMe);

    return page;
}

// ── OLED 光取り出し / Outcoupling ───────────────────────────────────────────
QWidget *DisplayOpticsTab::buildOledPage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(8);

    auto *s = new SectionBox(I18n::tr("dpo_oled_sec"), page);

    auto *stRow = new QHBoxLayout();
    m_bottomEmission = makeCheck(I18n::tr("dpo_o_bottom"), false, s);
    m_topEmission    = makeCheck(I18n::tr("dpo_o_top"),    true,  s);
    m_microcavity    = makeCheck(I18n::tr("dpo_o_cavity"), true,  s);
    stRow->addWidget(m_bottomEmission);
    stRow->addWidget(m_topEmission);
    stRow->addWidget(m_microcavity);
    stRow->addStretch(1);
    s->form()->addRow(I18n::tr("dpo_structure"), stRow);

    s->form()->addRow(I18n::tr("dpo_stack"),
                      hintLabel(I18n::tr("dpo_stack_hint"), s));

    auto *lossRow = new QHBoxLayout();
    m_iqe           = makeCheck(I18n::tr("dpo_o_iqe"),    true, s);
    m_sppLoss       = makeCheck(I18n::tr("dpo_o_spp"),    true, s);
    m_waveguideLoss = makeCheck(I18n::tr("dpo_o_wgloss"), true, s);
    lossRow->addWidget(m_iqe);
    lossRow->addWidget(m_sppLoss);
    lossRow->addWidget(m_waveguideLoss);
    lossRow->addStretch(1);
    s->form()->addRow(lossRow);

    auto *exRow = new QHBoxLayout();
    exRow->setSpacing(4);
    m_outcoupling = segRow(exRow, { I18n::tr("dpo_e_none"), I18n::tr("dpo_e_mlens"),
                                    I18n::tr("dpo_e_scat"), I18n::tr("dpo_e_phc") },
                           0, s);                    // 既定 "none"
    s->form()->addRow(I18n::tr("dpo_extract"), exRow);

    auto *eqeRow = new QHBoxLayout();
    m_eqeBadge = makeBadge(I18n::tr("dpo_eqe"), "acc", s);
    eqeRow->addWidget(m_eqeBadge);
    m_oledDetail = hintLabel(I18n::tr("dpo_oled_detail"), s);
    eqeRow->addWidget(m_oledDetail);
    eqeRow->addStretch(1);
    s->form()->addRow(eqeRow);
    v->addWidget(s);

    return page;
}

// ── microLED 解析 (共有セクションの中身) ────────────────────────────────────
QWidget *DisplayOpticsTab::buildMicroLedPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->setContentsMargins(0, 0, 0, 0);

    auto *csRow = new QHBoxLayout();
    m_chipSize = numEdit("5", 70, page);
    csRow->addWidget(m_chipSize);
    csRow->addWidget(new QLabel(I18n::tr("dpo_chipsize_unit"), page));
    csRow->addStretch(1);
    form->addRow(I18n::tr("dpo_chipsize"), csRow);

    auto *ckRow = new QHBoxLayout();
    m_sidewallRecomb = makeCheck(I18n::tr("dpo_ml_recomb"), true, page);
    m_sidewallDbr    = makeCheck(I18n::tr("dpo_ml_dbr"),    true, page);
    m_directional    = makeCheck(I18n::tr("dpo_ml_dir"),    true, page);
    ckRow->addWidget(m_sidewallRecomb);
    ckRow->addWidget(m_sidewallDbr);
    ckRow->addWidget(m_directional);
    ckRow->addStretch(1);
    form->addRow(ckRow);

    auto *bRow = new QHBoxLayout();
    m_microLedBadge = makeBadge(I18n::tr("dpo_ml_badge"), "acc", page);
    bRow->addWidget(m_microLedBadge);
    bRow->addStretch(1);
    form->addRow(bRow);
    return page;
}

// ── LCD/偏光系 解析 (共有セクションの中身) ──────────────────────────────────
QWidget *DisplayOpticsTab::buildLcdPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->setContentsMargins(0, 0, 0, 0);

    auto *mRow = new QHBoxLayout();
    mRow->setSpacing(4);
    m_lcdMode = segRow(mRow, { "TN", "IPS", "VA" }, 2, page);   // 既定 "va"
    form->addRow(I18n::tr("dpo_lcd_mode"), mRow);

    auto *ckRow = new QHBoxLayout();
    m_lcAnisotropy = makeCheck(I18n::tr("dpo_lc_aniso"), true, page);
    m_compFilm     = makeCheck(I18n::tr("dpo_lc_film"),  true, page);
    ckRow->addWidget(m_lcAnisotropy);
    ckRow->addWidget(m_compFilm);
    ckRow->addStretch(1);
    form->addRow(ckRow);

    auto *bRow = new QHBoxLayout();
    m_lcdBadge = makeBadge(I18n::tr("dpo_lcd_badge"), "acc", page);
    bRow->addWidget(m_lcdBadge);
    bRow->addStretch(1);
    form->addRow(bRow);
    return page;
}
