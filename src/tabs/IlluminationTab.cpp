// IlluminationTab.cpp
#include "IlluminationTab.h"
#include "../core/Project.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <QVBoxLayout>

using namespace ofd;

// ── タブ専用語彙 (file-local 登録, 接頭辞 ilm_) ──────────────────────────────
namespace {
const bool s_i18n = [] {
    using ofd::I18n;
    // 上段
    I18n::reg("ilm_title", "照明光学・測色 / Illumination & colorimetry",
              "Illumination & colorimetry");
    I18n::reg("ilm_hint",
              "非結像光学系の配光設計と測色評価。LightTools / Photopia 相当。"
              "FDTDは光取り出し (OLED/LEDチップ近傍)、レイトレースは配光を担当。",
              "Intensity-distribution design and colorimetric evaluation of "
              "non-imaging optics, in the class of LightTools / Photopia. FDTD covers "
              "outcoupling (near the OLED / LED chip); ray tracing covers the far-field "
              "distribution.");
    I18n::reg("ilm_app", "用途", "Application");
    I18n::reg("ilm_app_led",       "LED照明", "LED lighting");
    I18n::reg("ilm_app_auto",      "車載ランプ", "Automotive lamp");
    I18n::reg("ilm_app_backlight", "バックライト", "Backlight");
    I18n::reg("ilm_app_solar",     "太陽光集光", "Solar concentrator");

    // 光源
    I18n::reg("ilm_src_sec", "光源 / Light source", "Light source");
    I18n::reg("ilm_model", "モデル", "Model");
    I18n::reg("ilm_mdl_lambert", "ランバート面", "Lambertian surface");
    I18n::reg("ilm_mdl_ray",     "レイデータ (実測)", "Ray data (measured)");
    I18n::reg("ilm_mdl_chip",    "LEDチップ (FDTD連携)", "LED chip (FDTD coupled)");
    I18n::reg("ilm_raydata", "レイデータ", "Ray data");
    I18n::reg("ilm_browse", "📁 参照…", "📁 Browse…");
    I18n::reg("ilm_formats",
              "▸ 対応形式: .ray (LightTools), .dat (ASAP), IES TM-25, .txt (Radiant)",
              "▸ Supported formats: .ray (LightTools), .dat (ASAP), IES TM-25, "
              ".txt (Radiant)");
    I18n::reg("ilm_spectrum", "スペクトル", "Spectrum");
    I18n::reg("ilm_sp_white", "白色LED 5000K (青LED+YAG蛍光体)",
              "White LED 5000 K (blue LED + YAG phosphor)");
    I18n::reg("ilm_sp_rgb",   "RGB 3チップ", "RGB 3-chip");
    I18n::reg("ilm_sp_full",  "フルスペクトル (高CRI)", "Full spectrum (high CRI)");
    I18n::reg("ilm_sp_mono",  "単色 (波長指定)", "Monochromatic (specified wavelength)");
    I18n::reg("ilm_flux", "光束", "Luminous flux");
    I18n::reg("ilm_flux_unit", "lm · ", "lm · ");
    I18n::reg("ilm_rays", "レイ数", "Rays");

    // 光学系
    I18n::reg("ilm_opt_sec", "光学系 / Optics", "Optics");
    I18n::reg("ilm_o_reflector", "リフレクタ (自由曲面)", "Reflector (freeform)");
    I18n::reg("ilm_o_tir",       "TIRレンズ", "TIR lens");
    I18n::reg("ilm_o_diffuser",  "拡散板 (BSDF)", "Diffuser plate (BSDF)");
    I18n::reg("ilm_o_guide",     "導光板 + ドットパターン",
              "Light guide plate + dot pattern");
    I18n::reg("ilm_o_phosphor",  "蛍光体散乱 (Mie/モンテカルロ)",
              "Phosphor scattering (Mie / Monte Carlo)");
    I18n::reg("ilm_surface", "表面特性", "Surface property");
    I18n::reg("ilm_sf_specular", "鏡面", "Specular");
    I18n::reg("ilm_sf_lambert",  "拡散", "Diffuse");
    I18n::reg("ilm_sf_bsdf",     "BSDF実測", "Measured BSDF");
    I18n::reg("ilm_sf_abg",      "ABGモデル", "ABG model");

    // 測光・測色
    I18n::reg("ilm_photo_sec", "測光・測色 / Photometry & color",
              "Photometry & color");
    I18n::reg("ilm_c_metric", "指標", "Metric");
    I18n::reg("ilm_c_value",  "値", "Value");
    I18n::reg("ilm_c_target", "目標/規格", "Target / standard");
    I18n::reg("ilm_c_judge",  "判定", "Verdict");
    I18n::reg("ilm_pass", "適合", "Pass");
    I18n::reg("ilm_ref",  "参考", "Reference");
    I18n::reg("ilm_m_flux",    "全光束", "Total luminous flux");
    I18n::reg("ilm_m_eff",     "光学効率", "Optical efficiency");
    I18n::reg("ilm_m_beam",    "ビーム角 (FWHM)", "Beam angle (FWHM)");
    I18n::reg("ilm_m_unif",    "照度均斉度", "Illuminance uniformity");
    I18n::reg("ilm_m_cct",     "相関色温度 CCT", "Correlated color temperature CCT");
    I18n::reg("ilm_m_ra",      "演色評価数 Ra", "Color rendering index Ra");
    I18n::reg("ilm_m_tm30",    "TM-30 Rf / Rg", "TM-30 Rf / Rg");
    I18n::reg("ilm_m_uv",      "色度座標 (u',v')", "Chromaticity (u', v')");
    I18n::reg("ilm_m_uvspread","色ムラ (Δu'v' 配光内)",
              "Color non-uniformity (Δu'v' across the beam)");
    I18n::reg("ilm_m_ugr",     "UGR (グレア)", "UGR (glare)");
    I18n::reg("ilm_t_ansi", "ANSI C78.377 5000K枠内",
              "Inside the ANSI C78.377 5000 K quadrangle");
    I18n::reg("ilm_t_ugr",  "< 19 (オフィス)", "< 19 (office)");
    I18n::reg("ilm_btn_polar", "🗺 配光曲線 (極座標)",
              "🗺 Intensity distribution (polar)");
    I18n::reg("ilm_btn_cie",   "🎨 CIE色度図", "🎨 CIE chromaticity diagram");
    I18n::reg("ilm_btn_illum", "📊 照度分布 (床面)",
              "📊 Illuminance distribution (floor)");
    I18n::reg("ilm_btn_ies",   "💾 IES / LDT 配光ファイル書出",
              "💾 Export IES / LDT photometric file");
    I18n::reg("ilm_ies_hint",
              "▸ IES LM-63 / EULUMDAT (.ldt) で書出せば DIALux・AGi32 等の"
              "照明設計ソフトで使用可能。",
              "▸ Exporting as IES LM-63 / EULUMDAT (.ldt) makes the data usable in "
              "lighting design tools such as DIALux and AGi32.");
    return true;
}();

// 測光・測色 表 (モックの <tbody> をそのまま)。target が nullptr のときは
// targetKey (規格名など) を左寄せで表示する。
struct PhotoRow {
    const char *itemKey, *value, *target, *targetKey, *judgeKey, *kind;
};
const PhotoRow kPhoto[10] = {
    { "ilm_m_flux",     "1043 lm",         "≥ 1000",     nullptr,      "ilm_pass", "ok" },
    { "ilm_m_eff",      "86.9 %",          "≥ 85",       nullptr,      "ilm_pass", "ok" },
    { "ilm_m_beam",     "24.2 °",          "25 ± 3",     nullptr,      "ilm_pass", "ok" },
    { "ilm_m_unif",     "0.78",            "≥ 0.7",      nullptr,      "ilm_pass", "ok" },
    { "ilm_m_cct",      "4980 K",          "5000 ± 300", nullptr,      "ilm_pass", "ok" },
    { "ilm_m_ra",       "83.4",            "≥ 80",       nullptr,      "ilm_pass", "ok" },
    { "ilm_m_tm30",     "86 / 98",         "—",          nullptr,      "ilm_ref",  ""   },
    { "ilm_m_uv",       "0.2131, 0.4956",  nullptr,      "ilm_t_ansi", "ilm_pass", "ok" },
    { "ilm_m_uvspread", "0.0042",          "< 0.006",    nullptr,      "ilm_pass", "ok" },
    { "ilm_m_ugr",      "17.8",            nullptr,      "ilm_t_ugr",  "ilm_pass", "ok" },
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

// ── IlluminationTab ─────────────────────────────────────────────────────────
IlluminationTab::IlluminationTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project),
      m_app(nullptr),
      m_srcModel(nullptr), m_rayFile(nullptr), m_spectrum(nullptr),
      m_flux(nullptr), m_rays(nullptr),
      m_reflector(nullptr), m_tirLens(nullptr), m_diffuser(nullptr),
      m_lightGuide(nullptr), m_phosphor(nullptr), m_surface(nullptr),
      m_photoTable(nullptr)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // ── 照明光学・測色 (用途) ───────────────────────────────────────────────
    auto *sTop = new SectionBox(I18n::tr("ilm_title"), body);
    sTop->vbox()->addWidget(hintLabel(I18n::tr("ilm_hint"), sTop));

    auto *appRow = new QHBoxLayout();
    appRow->setSpacing(4);
    m_app = segRow(appRow, { I18n::tr("ilm_app_led"), I18n::tr("ilm_app_auto"),
                             I18n::tr("ilm_app_backlight"),
                             I18n::tr("ilm_app_solar") },
                   0, sTop);                        // 既定 "led"
    sTop->form()->addRow(I18n::tr("ilm_app"), appRow);
    v->addWidget(sTop);

    // ── 光源 / Light source ─────────────────────────────────────────────────
    auto *sSrc = new SectionBox(I18n::tr("ilm_src_sec"), body);

    auto *mdlRow = new QHBoxLayout();
    mdlRow->setSpacing(4);
    m_srcModel = segRow(mdlRow, { I18n::tr("ilm_mdl_lambert"),
                                  I18n::tr("ilm_mdl_ray"),
                                  I18n::tr("ilm_mdl_chip") },
                        1, sSrc);                   // 既定 "ray"
    sSrc->form()->addRow(I18n::tr("ilm_model"), mdlRow);

    auto *rayRow = new QHBoxLayout();
    m_rayFile = numEdit("CREE_XPG3_5000K.ray", 0, sSrc);
    rayRow->addWidget(m_rayFile, 1);
    rayRow->addWidget(new QPushButton(I18n::tr("ilm_browse"), sSrc));
    sSrc->form()->addRow(I18n::tr("ilm_raydata"), rayRow);

    sSrc->form()->addRow(hintLabel(I18n::tr("ilm_formats"), sSrc));

    m_spectrum = new QComboBox(sSrc);
    m_spectrum->addItem(I18n::tr("ilm_sp_white"));
    m_spectrum->addItem(I18n::tr("ilm_sp_rgb"));
    m_spectrum->addItem(I18n::tr("ilm_sp_full"));
    m_spectrum->addItem(I18n::tr("ilm_sp_mono"));
    sSrc->form()->addRow(I18n::tr("ilm_spectrum"), m_spectrum);

    auto *fluxRow = new QHBoxLayout();
    m_flux = numEdit("1200", 100, sSrc);
    m_rays = numEdit("5000000", 100, sSrc);
    fluxRow->addWidget(m_flux);
    fluxRow->addWidget(new QLabel(I18n::tr("ilm_flux_unit"), sSrc));
    fluxRow->addWidget(new QLabel(I18n::tr("ilm_rays"), sSrc));
    fluxRow->addWidget(m_rays);
    fluxRow->addStretch(1);
    sSrc->form()->addRow(I18n::tr("ilm_flux"), fluxRow);
    v->addWidget(sSrc);

    // ── 光学系 / Optics ─────────────────────────────────────────────────────
    auto *sOpt = new SectionBox(I18n::tr("ilm_opt_sec"), body);

    auto *o1 = new QHBoxLayout();
    m_reflector = makeCheck(I18n::tr("ilm_o_reflector"), true,  sOpt);
    m_tirLens   = makeCheck(I18n::tr("ilm_o_tir"),       false, sOpt);
    m_diffuser  = makeCheck(I18n::tr("ilm_o_diffuser"),  true,  sOpt);
    o1->addWidget(m_reflector);
    o1->addWidget(m_tirLens);
    o1->addWidget(m_diffuser);
    o1->addStretch(1);
    sOpt->form()->addRow(o1);

    auto *o2 = new QHBoxLayout();
    m_lightGuide = makeCheck(I18n::tr("ilm_o_guide"),    false, sOpt);
    m_phosphor   = makeCheck(I18n::tr("ilm_o_phosphor"), true,  sOpt);
    o2->addWidget(m_lightGuide);
    o2->addWidget(m_phosphor);
    o2->addStretch(1);
    sOpt->form()->addRow(o2);

    auto *sfRow = new QHBoxLayout();
    sfRow->setSpacing(4);
    m_surface = segRow(sfRow, { I18n::tr("ilm_sf_specular"),
                                I18n::tr("ilm_sf_lambert"),
                                I18n::tr("ilm_sf_bsdf"),
                                I18n::tr("ilm_sf_abg") },
                       2, sOpt);                    // 既定 "bsdf"
    sOpt->form()->addRow(I18n::tr("ilm_surface"), sfRow);
    v->addWidget(sOpt);

    // ── 測光・測色 / Photometry & color ─────────────────────────────────────
    auto *sPh = new SectionBox(I18n::tr("ilm_photo_sec"), body);

    m_photoTable = new QTableWidget(10, 4, sPh);
    m_photoTable->setHorizontalHeaderLabels({ I18n::tr("ilm_c_metric"),
                                              I18n::tr("ilm_c_value"),
                                              I18n::tr("ilm_c_target"),
                                              I18n::tr("ilm_c_judge") });
    m_photoTable->verticalHeader()->setVisible(false);
    m_photoTable->verticalHeader()->setDefaultSectionSize(26);
    m_photoTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_photoTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_photoTable->setMinimumHeight(300);
    for (int r = 0; r < 10; ++r) {
        const PhotoRow &p = kPhoto[r];
        m_photoTable->setItem(r, 0, textItem(I18n::tr(p.itemKey)));
        m_photoTable->setItem(r, 1, numItem(QString::fromUtf8(p.value)));
        // モックで className="num" が付かないセル (規格名) は左寄せのまま
        m_photoTable->setItem(r, 2, p.target ? numItem(QString::fromUtf8(p.target))
                                             : textItem(I18n::tr(p.targetKey)));
        m_photoTable->setCellWidget(r, 3,
                                    badgeCell(I18n::tr(p.judgeKey), p.kind));
    }
    sPh->vbox()->addWidget(m_photoTable);

    auto *btnRow = new QHBoxLayout();
    btnRow->addWidget(new QPushButton(I18n::tr("ilm_btn_polar"), sPh));
    btnRow->addWidget(new QPushButton(I18n::tr("ilm_btn_cie"), sPh));
    btnRow->addWidget(new QPushButton(I18n::tr("ilm_btn_illum"), sPh));
    btnRow->addWidget(new QPushButton(I18n::tr("ilm_btn_ies"), sPh));
    btnRow->addStretch(1);
    sPh->vbox()->addLayout(btnRow);

    sPh->vbox()->addWidget(hintLabel(I18n::tr("ilm_ies_hint"), sPh));
    v->addWidget(sPh);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
}
