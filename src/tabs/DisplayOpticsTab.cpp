// DisplayOpticsTab.cpp
#include "DisplayOpticsTab.h"
#include "TabHelpers.h"
#include "../core/Project.h"
#include "../optics/DisplayMetrics.h"
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
#include <QVector>
#include <cmath>

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
    I18n::reg("dpo_sub_unit", "mm 厚 ·", "mm thick ·");
    I18n::reg("dpo_sub_index", "屈折率 n =", "index n =");
    I18n::reg("dpo_grating", "格子", "Grating");
    I18n::reg("dpo_g_period", "周期", "Period");
    I18n::reg("dpo_g_depth",  "深さ", "Depth");
    I18n::reg("dpo_g_slant",  "斜め角", "Slant angle");
    I18n::reg("dpo_nm_dot", "nm ·", "nm ·");
    I18n::reg("dpo_three_grat", "入力/折返し/出力の3格子構成",
              "Three-grating layout (in-coupler / fold / out-coupler)");
    I18n::reg("dpo_rcwa_opt", "RCWAで回折効率を最適化",
              "Optimize diffraction efficiency with RCWA");
    I18n::reg("dpo_pupil", "瞳拡大", "Pupil expansion");
    I18n::reg("dpo_design_lambda", "設計波長", "Design wavelength");
    I18n::reg("dpo_guide_max", "導波角上限", "Max guided angle");
    I18n::reg("dpo_outcoupler", "出射格子長", "Out-coupler length");
    I18n::reg("dpo_eyerelief", "アイレリーフ", "Eye relief");
    I18n::reg("dpo_targets", "設計目標", "Design targets");
    I18n::reg("dpo_tg_fov",  "FOV ≥", "FOV ≥");
    I18n::reg("dpo_tg_eye",  "° · アイボックス ≥", "° · eyebox ≥");
    I18n::reg("dpo_tg_see",  "mm · シースルー ≥", "mm · see-through ≥");
    I18n::reg("dpo_pct", "%", "%");
    I18n::reg("dpo_deg", "°", "°");
    I18n::reg("dpo_mm", "mm", "mm");
    I18n::reg("dpo_nm", "nm", "nm");

    // 評価
    I18n::reg("dpo_metric_sec", "評価 / Metrics", "Metrics");
    I18n::reg("dpo_c_metric", "指標", "Metric");
    I18n::reg("dpo_c_value",  "値", "Value");
    I18n::reg("dpo_c_target", "目標", "Target");
    I18n::reg("dpo_c_judge",  "判定", "Verdict");
    I18n::reg("dpo_c_basis",  "根拠 / 必要な計算", "Basis / required computation");
    I18n::reg("dpo_pass",   "適合", "Pass");
    I18n::reg("dpo_needwork", "要改善", "Needs work");
    I18n::reg("dpo_uncomputed", "未計算", "Not computed");
    I18n::reg("dpo_dash", "—", "—");
    I18n::reg("dpo_m_critangle", "全反射臨界角 θc", "TIR critical angle θc");
    I18n::reg("dpo_m_guiderange", "導波角範囲 θg", "Guided-angle range θg");
    I18n::reg("dpo_m_fov",     "視野角 FOV (格子分散方向)",
              "Field of view (grating dispersion direction)");
    I18n::reg("dpo_m_eyebox",  "アイボックス幅", "Eyebox width");
    I18n::reg("dpo_m_eff",     "光効率 (nit/lm)", "Optical efficiency (nit/lm)");
    I18n::reg("dpo_m_lumunif", "輝度均一性", "Luminance uniformity");
    I18n::reg("dpo_m_colunif", "色均一性 (Δu'v')", "Color uniformity (Δu'v')");
    I18n::reg("dpo_m_stray",   "迷光 (レインボー)", "Stray light (rainbow)");
    I18n::reg("dpo_m_seethru", "シースルー透過率", "See-through transmittance");
    I18n::reg("dpo_b_grating",
              "格子式 n·sinθg = sinθair + λ/Λ と TIR 条件 (Levola 2006)",
              "Grating equation n·sinθg = sinθair + λ/Λ with the TIR condition "
              "(Levola 2006)");
    I18n::reg("dpo_b_tir", "θc = asin(1/n)", "θc = asin(1/n)");
    I18n::reg("dpo_b_eyebox", "W = L − 2·ER·tan(FOV/2) (Kress 2021)",
              "W = L − 2·ER·tan(FOV/2) (Kress 2021)");
    I18n::reg("dpo_b_fresnel",
              "無コート平板のフレネル透過率 T = (1−R)/(1+R) (回折損失は含まない)",
              "Fresnel transmittance of an uncoated slab T = (1−R)/(1+R) "
              "(diffraction losses excluded)");
    I18n::reg("dpo_b_needrcwa",
              "RCWA (回折効率) + レイトレース (瞳内の光線本数) が必要",
              "Requires RCWA (diffraction efficiency) + ray tracing (rays across "
              "the pupil)");
    I18n::reg("dpo_b_needray", "アイボックス走査のレイトレースが必要",
              "Requires a ray trace scanned over the eyebox");
    I18n::reg("dpo_b_needspec",
              "波長別 RCWA + 測色 (Δu'v') が必要", "Requires per-wavelength RCWA "
              "plus colorimetry (Δu'v')");
    I18n::reg("dpo_b_needstray", "外光の非設計次数を含む迷光解析が必要",
              "Requires a stray-light analysis including non-design orders of "
              "ambient light");
    I18n::reg("dpo_metric_note",
              "▸ 「値」は左記の閉形式から入力値で計算した結果。"
              "「—」の行は RCWA / レイトレース (未実装) が必要な量で、"
              "値を推定して表示することはしない。",
              "▸ Values are computed from the closed-form expressions listed, using "
              "the inputs above. Rows showing “—” need RCWA / ray tracing (not "
              "implemented); no estimated numbers are shown for them.");
    I18n::reg("dpo_fov_invalid",
              "この周期・波長・屈折率では 1 次回折の導波帯域が存在しない",
              "No first-order guided band exists for this period, wavelength and "
              "index");
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
    I18n::reg("dpo_o_params", "有機層 n / IQE", "Organic index n / IQE");
    I18n::reg("dpo_o_slash", "/", "/");
    I18n::reg("dpo_extract", "取り出し構造", "Outcoupling structure");
    I18n::reg("dpo_e_none",  "なし", "None");
    I18n::reg("dpo_e_mlens", "マイクロレンズ", "Microlens");
    I18n::reg("dpo_e_scat",  "散乱層", "Scattering layer");
    I18n::reg("dpo_e_phc",   "フォトニック結晶", "Photonic crystal");
    I18n::reg("dpo_eqe_fmt", "外部量子効率 EQE = %1 %", "EQE = %1 %");
    I18n::reg("dpo_m_thetac", "臨界角 θc", "Critical angle θc");
    I18n::reg("dpo_m_escape", "射出円錐割合 (1面)", "Escape-cone fraction (one face)");
    I18n::reg("dpo_m_outc",  "光取り出し効率 η_out", "Outcoupling efficiency η_out");
    I18n::reg("dpo_m_eqe",   "外部量子効率 EQE", "External quantum efficiency EQE");
    I18n::reg("dpo_m_extboost", "取り出し構造による向上分",
              "Gain from the outcoupling structure");
    I18n::reg("dpo_m_colshift", "視野角色シフト Δu'v'", "Off-axis color shift Δu'v'");
    I18n::reg("dpo_m_lossbreak", "損失内訳 (SPP / 導波モード / 基板)",
              "Loss breakdown (SPP / waveguided / substrate)");
    I18n::reg("dpo_b_oled", "古典近似 η = 1/(2n²) (Greenham 1994)",
              "Classical estimate η = 1/(2n²) (Greenham 1994)");
    I18n::reg("dpo_b_escape", "(1−cosθc)/2 (Schubert, LED 9章)",
              "(1−cosθc)/2 (Schubert, LEDs ch. 9)");
    I18n::reg("dpo_b_eqe_planar", "EQE = IQE × η_out (平面構造)",
              "EQE = IQE × η_out (planar structure)");
    I18n::reg("dpo_b_needfdtd",
              "双極子源 FDTD (モード分解) が必要", "Requires dipole-source FDTD "
              "with mode decomposition");
    I18n::reg("dpo_b_needcavity",
              "マイクロキャビティの角度分散計算 + 測色が必要",
              "Requires an angular microcavity calculation plus colorimetry");
    I18n::reg("dpo_oled_note",
              "▸ η_out は平面構造の古典近似。マイクロキャビティ・取り出し構造・"
              "SPP 損失の内訳は双極子 FDTD が必要 (未実装) — 設定は保存のみ。",
              "▸ η_out is the classical planar estimate. Microcavity effects, "
              "outcoupling structures and the SPP loss breakdown need dipole FDTD "
              "(not implemented); those settings are only recorded.");

    // microLED
    I18n::reg("dpo_microled_sec", "microLED 解析", "microLED analysis");
    I18n::reg("dpo_chipsize", "チップサイズ", "Chip size");
    I18n::reg("dpo_chipsize_unit", "μm 角", "μm square");
    I18n::reg("dpo_ml_params", "GaN n / IQE", "GaN index n / IQE");
    I18n::reg("dpo_ml_srv", "表面再結合速度 S", "Surface recombination velocity S");
    I18n::reg("dpo_ml_srv_unit", "cm/s · 寿命 τ", "cm/s · lifetime τ");
    I18n::reg("dpo_ml_ns", "ns", "ns");
    I18n::reg("dpo_ml_recomb", "側壁再結合損失を考慮",
              "Include the sidewall recombination loss");
    I18n::reg("dpo_ml_dbr",    "サイドウォール反射 (DBR)", "Sidewall reflector (DBR)");
    I18n::reg("dpo_ml_dir",    "配光の指向性化", "Directional emission shaping");
    I18n::reg("dpo_ml_badge_fmt", "光取り出し効率 %1 % (上面, 平面近似)",
              "Extraction efficiency %1 % (top face, planar estimate)");
    I18n::reg("dpo_m_ext_top", "光取り出し効率 (上面のみ)",
              "Extraction efficiency (top face only)");
    I18n::reg("dpo_m_ext_cube", "光取り出し効率 (立方体6面, 上限)",
              "Extraction efficiency (cube, 6 faces — upper bound)");
    I18n::reg("dpo_m_iqe_eff", "実効 IQE (側壁再結合込み)",
              "Effective IQE (with sidewall recombination)");
    I18n::reg("dpo_m_halfangle", "半値角", "Half-angle");
    I18n::reg("dpo_b_cube", "3/(2n²) (Schubert, LED 9章)",
              "3/(2n²) (Schubert, LEDs ch. 9)");
    I18n::reg("dpo_b_sidewall", "η = η0/(1+4Sτ/L) (Olivier 2017)",
              "η = η0/(1+4Sτ/L) (Olivier 2017)");
    I18n::reg("dpo_b_lambert", "ランバート配光 I(θ)=I0·cosθ",
              "Lambertian emission I(θ) = I0·cosθ");
    I18n::reg("dpo_b_needraytrace",
              "DBR / 指向性化の効果はチップ形状のレイトレース (または FDTD) が必要",
              "The DBR / beam-shaping effect needs a chip-level ray trace (or FDTD)");
    I18n::reg("dpo_ml_note",
              "▸ 取り出し効率は平面 (上面のみ) と立方体 6 面の古典的な上下限。"
              "側壁 DBR・指向性化の効果は未計算 (—) — 設定は保存のみ。",
              "▸ The extraction efficiencies are the classical lower (top face) and "
              "upper (cube) bounds. The effect of the sidewall DBR and beam shaping "
              "is not computed (—); those settings are only recorded.");

    // LCD/偏光系
    I18n::reg("dpo_lcd_sec", "LCD/偏光系 解析", "LCD / polarization analysis");
    I18n::reg("dpo_lcd_mode", "モード", "Mode");
    I18n::reg("dpo_lc_aniso", "液晶の異方性 (Jones/Berreman 4×4)",
              "Liquid-crystal anisotropy (Jones / Berreman 4×4)");
    I18n::reg("dpo_lc_film",  "視野角補償フィルム", "Viewing-angle compensation film");
    I18n::reg("dpo_lcd_lum", "白輝度", "Peak luminance");
    I18n::reg("dpo_lcd_lum_unit", "cd/m² · 暗室CR", "cd/m² · darkroom CR");
    I18n::reg("dpo_lcd_amb", "環境照度", "Ambient illuminance");
    I18n::reg("dpo_lcd_amb_unit", "lx · 画面反射率", "lx · screen reflectance");
    I18n::reg("dpo_lcd_badge_fmt", "環境光コントラスト比 %1:1",
              "Ambient contrast ratio %1:1");
    I18n::reg("dpo_m_ambcr", "環境光コントラスト比", "Ambient contrast ratio");
    I18n::reg("dpo_m_amblum", "環境光による画面輝度", "Screen luminance from ambient");
    I18n::reg("dpo_m_blacklum", "黒輝度", "Black luminance");
    I18n::reg("dpo_m_viewangle", "視野角 (CR ≥ 10)", "Viewing angle (CR ≥ 10)");
    I18n::reg("dpo_b_ambcr",
              "CR = (Lw + R·E/π)/(Lb + R·E/π), Lb = Lw/CR0 (IEC 62341-6-1)",
              "CR = (Lw + R·E/π)/(Lb + R·E/π), Lb = Lw/CR0 (IEC 62341-6-1)");
    I18n::reg("dpo_b_lambert_screen", "拡散反射 L = R·E/π",
              "Diffuse reflection L = R·E/π");
    I18n::reg("dpo_b_needberreman",
              "液晶配向の Berreman 4×4 角度計算が必要 (未実装)",
              "Requires a Berreman 4×4 angular calculation of the LC director "
              "profile (not implemented)");
    I18n::reg("dpo_lcd_note",
              "▸ 環境光コントラストは定義式による計算。視野角特性は液晶配向の"
              "Berreman 4×4 が必要 (未実装) — モード/補償フィルムの選択は保存のみ。",
              "▸ The ambient contrast is computed from its definition. The viewing "
              "angle needs a Berreman 4×4 calculation of the LC director profile "
              "(not implemented); the mode and film selections are only recorded.");
    return true;
}();

// 評価表 1 行 ("—" は未計算を表す。kind: "ok"/"warn"/"" = 未計算)
struct MetricOut {
    QString item, value, target, judge, basis;
    const char *kind = "";
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

QLabel *noteLabel(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setTextFormat(Qt::PlainText);
    l->setWordWrap(true);
    l->setStyleSheet("font-size:11px; color:palette(mid);");
    return l;
}

QLineEdit *numEdit(int width, QWidget *parent)
{
    auto *e = new QLineEdit(parent);
    if (width > 0) e->setMaximumWidth(width);
    return e;
}

QCheckBox *makeCheck(const QString &text, QWidget *parent)
{
    return new QCheckBox(text, parent);
}

// <Seg> 相当: 排他 checkable QPushButton の一列
QButtonGroup *segRow(QHBoxLayout *row, const QStringList &labels, QWidget *parent)
{
    auto *g = new QButtonGroup(parent);
    g->setExclusive(true);
    for (int i = 0; i < labels.size(); ++i) {
        auto *b = new QPushButton(labels.at(i), parent);
        b->setCheckable(true);
        b->setStyleSheet("font-size:11px; padding:2px 8px;");
        g->addButton(b, i);
        row->addWidget(b);
    }
    row->addStretch(1);
    return g;
}

void setSeg(QButtonGroup *g, int index)
{
    if (auto *b = g->button(index)) b->setChecked(true);
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

QString fmt(double v, int dec) { return QString::number(v, 'f', dec); }

// 数値入力の読み出し (不正値は既定値のまま)
double readNum(QLineEdit *e, double fallback)
{
    bool ok = false;
    const double v = e->text().toDouble(&ok);
    return ok ? v : fallback;
}

// 「大きいほど良い」判定
const char *judgeGE(double value, double target)
{
    return (value >= target) ? "ok" : "warn";
}

// 未計算行
MetricOut uncomputed(const QString &item, const QString &basis)
{
    MetricOut m;
    m.item = item;
    m.value = I18n::tr("dpo_dash");
    m.target = I18n::tr("dpo_dash");
    m.judge = I18n::tr("dpo_uncomputed");
    m.basis = basis;
    m.kind = "";
    return m;
}

// QVector<MetricOut> → QTableWidget (5 列: 指標/値/目標/判定/根拠)
void fillMetricTable(QTableWidget *t, const QVector<MetricOut> &rows)
{
    t->clearContents();
    t->setRowCount(rows.size());
    for (int r = 0; r < rows.size(); ++r) {
        const MetricOut &m = rows[r];
        t->setItem(r, 0, textItem(m.item));
        t->setItem(r, 1, numItem(m.value));
        t->setItem(r, 2, numItem(m.target));
        t->setCellWidget(r, 3, badgeCell(m.judge, m.kind));
        t->setItem(r, 4, textItem(m.basis));
        // 「根拠」列は左パネルが狭いと隠れるので、行全体のツールチップにも出す
        for (int c = 0; c < 5; ++c)
            if (auto *it = t->item(r, c)) it->setToolTip(m.basis);
    }
}

QTableWidget *makeMetricTable(QWidget *parent, int minHeight)
{
    auto *t = new QTableWidget(0, 5, parent);
    t->setHorizontalHeaderLabels({ I18n::tr("dpo_c_metric"),
                                   I18n::tr("dpo_c_value"),
                                   I18n::tr("dpo_c_target"),
                                   I18n::tr("dpo_c_judge"),
                                   I18n::tr("dpo_c_basis") });
    t->verticalHeader()->setVisible(false);
    t->verticalHeader()->setDefaultSectionSize(26);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    t->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setMinimumHeight(minHeight);
    return t;
}
} // namespace

// ── DisplayOpticsTab ────────────────────────────────────────────────────────
DisplayOpticsTab::DisplayOpticsTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project),
      m_device(nullptr),
      m_arwgPage(nullptr), m_oledPage(nullptr), m_sharedSec(nullptr),
      m_sharedStack(nullptr),
      m_wgType(nullptr), m_subThick(nullptr), m_subIndex(nullptr),
      m_gratPeriod(nullptr), m_gratDepth(nullptr), m_gratSlant(nullptr),
      m_designLambda(nullptr), m_guideMax(nullptr), m_outcouplerLen(nullptr),
      m_eyeRelief(nullptr), m_fovTarget(nullptr), m_eyeboxTarget(nullptr),
      m_seeThroughTarget(nullptr),
      m_threeGratings(nullptr), m_rcwaOptimize(nullptr), m_metricTable(nullptr),
      m_bottomEmission(nullptr), m_topEmission(nullptr), m_microcavity(nullptr),
      m_iqe(nullptr), m_sppLoss(nullptr), m_waveguideLoss(nullptr),
      m_outcoupling(nullptr), m_oledIndex(nullptr), m_oledIqe(nullptr),
      m_eqeBadge(nullptr), m_oledTable(nullptr),
      m_chipSize(nullptr), m_mlIndex(nullptr), m_mlIqe(nullptr),
      m_mlSurfVel(nullptr), m_mlLifetime(nullptr),
      m_sidewallRecomb(nullptr), m_sidewallDbr(nullptr),
      m_directional(nullptr), m_microLedBadge(nullptr), m_mlTable(nullptr),
      m_lcdMode(nullptr), m_lcAnisotropy(nullptr), m_compFilm(nullptr),
      m_lcdPeakLum(nullptr), m_lcdCr(nullptr), m_lcdAmbient(nullptr),
      m_lcdRefl(nullptr), m_lcdBadge(nullptr), m_lcdTable(nullptr)
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
                                I18n::tr("dpo_dev_lcd") }, sTop);
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

    connect(m_device, &QButtonGroup::idClicked, this, [this](int i) {
        deviceChanged(i);
        onEdited();
    });
    connect(project, &Project::loaded, this, &DisplayOpticsTab::refresh);

    refresh();
}

// widgets → model
void DisplayOpticsTab::apply()
{
    if (m_updating) return;
    DisplayOpticsOpts &d = m_p->displayOptics();

    d.device = qBound(0, m_device->checkedId(), 3);

    d.wgType = qBound(0, m_wgType->checkedId(), 3);
    d.subThick_mm      = readNum(m_subThick, d.subThick_mm);
    d.subIndex         = readNum(m_subIndex, d.subIndex);
    d.gratPeriod_nm    = readNum(m_gratPeriod, d.gratPeriod_nm);
    d.gratDepth_nm     = readNum(m_gratDepth, d.gratDepth_nm);
    d.gratSlant_deg    = readNum(m_gratSlant, d.gratSlant_deg);
    d.designLambda_nm  = readNum(m_designLambda, d.designLambda_nm);
    d.guideMaxAngle_deg= readNum(m_guideMax, d.guideMaxAngle_deg);
    d.outcouplerLen_mm = readNum(m_outcouplerLen, d.outcouplerLen_mm);
    d.eyeRelief_mm     = readNum(m_eyeRelief, d.eyeRelief_mm);
    d.fovTarget_deg    = readNum(m_fovTarget, d.fovTarget_deg);
    d.eyeboxTarget_mm  = readNum(m_eyeboxTarget, d.eyeboxTarget_mm);
    d.seeThroughTarget_pct = readNum(m_seeThroughTarget, d.seeThroughTarget_pct);
    d.threeGratings = m_threeGratings->isChecked();
    d.rcwaOptimize  = m_rcwaOptimize->isChecked();

    d.bottomEmission = m_bottomEmission->isChecked();
    d.topEmission    = m_topEmission->isChecked();
    d.microcavity    = m_microcavity->isChecked();
    d.sepIqe         = m_iqe->isChecked();
    d.sepSpp         = m_sppLoss->isChecked();
    d.sepWaveguide   = m_waveguideLoss->isChecked();
    d.outcouplingStruct = qBound(0, m_outcoupling->checkedId(), 3);
    d.oledIndex = readNum(m_oledIndex, d.oledIndex);
    d.oledIqe   = readNum(m_oledIqe, d.oledIqe);

    d.chipSize_um     = readNum(m_chipSize, d.chipSize_um);
    d.mlIndex         = readNum(m_mlIndex, d.mlIndex);
    d.mlIqe           = readNum(m_mlIqe, d.mlIqe);
    d.mlSurfVel_cm_s  = readNum(m_mlSurfVel, d.mlSurfVel_cm_s);
    d.mlLifetime_ns   = readNum(m_mlLifetime, d.mlLifetime_ns);
    d.sidewallRecomb = m_sidewallRecomb->isChecked();
    d.sidewallDbr    = m_sidewallDbr->isChecked();
    d.directional    = m_directional->isChecked();

    d.lcdMode = qBound(0, m_lcdMode->checkedId(), 2);
    d.lcAnisotropy = m_lcAnisotropy->isChecked();
    d.compFilm     = m_compFilm->isChecked();
    d.lcdPeakLum_cdm2 = readNum(m_lcdPeakLum, d.lcdPeakLum_cdm2);
    d.lcdDarkroomCr   = readNum(m_lcdCr, d.lcdDarkroomCr);
    d.lcdAmbient_lx   = readNum(m_lcdAmbient, d.lcdAmbient_lx);
    d.lcdReflectance  = readNum(m_lcdRefl, d.lcdReflectance);

    m_p->touch();
}

// model → widgets
void DisplayOpticsTab::refresh()
{
    m_updating = true;
    const DisplayOpticsOpts &d = m_p->displayOptics();

    setSeg(m_device, d.device);
    setSeg(m_wgType, d.wgType);
    m_subThick->setText(fmt(d.subThick_mm, 2));
    m_subIndex->setText(fmt(d.subIndex, 3));
    m_gratPeriod->setText(fmt(d.gratPeriod_nm, 1));
    m_gratDepth->setText(fmt(d.gratDepth_nm, 1));
    m_gratSlant->setText(fmt(d.gratSlant_deg, 1));
    m_designLambda->setText(fmt(d.designLambda_nm, 1));
    m_guideMax->setText(fmt(d.guideMaxAngle_deg, 1));
    m_outcouplerLen->setText(fmt(d.outcouplerLen_mm, 1));
    m_eyeRelief->setText(fmt(d.eyeRelief_mm, 1));
    m_fovTarget->setText(fmt(d.fovTarget_deg, 1));
    m_eyeboxTarget->setText(fmt(d.eyeboxTarget_mm, 1));
    m_seeThroughTarget->setText(fmt(d.seeThroughTarget_pct, 1));
    m_threeGratings->setChecked(d.threeGratings);
    m_rcwaOptimize->setChecked(d.rcwaOptimize);

    m_bottomEmission->setChecked(d.bottomEmission);
    m_topEmission->setChecked(d.topEmission);
    m_microcavity->setChecked(d.microcavity);
    m_iqe->setChecked(d.sepIqe);
    m_sppLoss->setChecked(d.sepSpp);
    m_waveguideLoss->setChecked(d.sepWaveguide);
    setSeg(m_outcoupling, d.outcouplingStruct);
    m_oledIndex->setText(fmt(d.oledIndex, 3));
    m_oledIqe->setText(fmt(d.oledIqe, 3));

    m_chipSize->setText(fmt(d.chipSize_um, 2));
    m_mlIndex->setText(fmt(d.mlIndex, 3));
    m_mlIqe->setText(fmt(d.mlIqe, 3));
    m_mlSurfVel->setText(QString::number(d.mlSurfVel_cm_s, 'g', 6));
    m_mlLifetime->setText(fmt(d.mlLifetime_ns, 2));
    m_sidewallRecomb->setChecked(d.sidewallRecomb);
    m_sidewallDbr->setChecked(d.sidewallDbr);
    m_directional->setChecked(d.directional);

    setSeg(m_lcdMode, d.lcdMode);
    m_lcAnisotropy->setChecked(d.lcAnisotropy);
    m_compFilm->setChecked(d.compFilm);
    m_lcdPeakLum->setText(fmt(d.lcdPeakLum_cdm2, 1));
    m_lcdCr->setText(fmt(d.lcdDarkroomCr, 0));
    m_lcdAmbient->setText(fmt(d.lcdAmbient_lx, 1));
    m_lcdRefl->setText(fmt(d.lcdReflectance, 4));

    m_updating = false;
    deviceChanged(d.device);
    recomputeAll();
}

void DisplayOpticsTab::onEdited()
{
    if (m_updating) return;
    apply();
    recomputeAll();
}

void DisplayOpticsTab::recomputeAll()
{
    recomputeArwg();
    recomputeOled();
    recomputeMicroLed();
    recomputeLcd();
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
                      sWg);
    sWg->form()->addRow(I18n::tr("dpo_wg_type"), tRow);

    auto *subRow = new QHBoxLayout();
    m_subThick = numEdit(70, sWg);
    m_subIndex = numEdit(70, sWg);
    subRow->addWidget(m_subThick);
    subRow->addWidget(new QLabel(I18n::tr("dpo_sub_unit"), sWg));
    subRow->addWidget(new QLabel(I18n::tr("dpo_sub_index"), sWg));
    subRow->addWidget(m_subIndex);
    subRow->addStretch(1);
    sWg->form()->addRow(I18n::tr("dpo_substrate"), subRow);

    auto *gRow = new QHBoxLayout();
    m_gratPeriod = numEdit(70, sWg);
    m_gratDepth  = numEdit(70, sWg);
    m_gratSlant  = numEdit(70, sWg);
    gRow->addWidget(new QLabel(I18n::tr("dpo_g_period"), sWg));
    gRow->addWidget(m_gratPeriod);
    gRow->addWidget(new QLabel(I18n::tr("dpo_nm_dot"), sWg));
    gRow->addWidget(new QLabel(I18n::tr("dpo_g_depth"), sWg));
    gRow->addWidget(m_gratDepth);
    gRow->addWidget(new QLabel(I18n::tr("dpo_nm_dot"), sWg));
    gRow->addWidget(new QLabel(I18n::tr("dpo_g_slant"), sWg));
    gRow->addWidget(m_gratSlant);
    gRow->addWidget(new QLabel(I18n::tr("dpo_deg"), sWg));
    gRow->addStretch(1);
    sWg->form()->addRow(I18n::tr("dpo_grating"), gRow);

    // 瞳拡大 (アイボックス計算に使う幾何) — 設計波長・導波角上限も同じ行に
    auto *pRow = new QHBoxLayout();
    m_designLambda  = numEdit(70, sWg);
    m_guideMax      = numEdit(60, sWg);
    m_outcouplerLen = numEdit(60, sWg);
    m_eyeRelief     = numEdit(60, sWg);
    pRow->addWidget(new QLabel(I18n::tr("dpo_design_lambda"), sWg));
    pRow->addWidget(m_designLambda);
    pRow->addWidget(new QLabel(I18n::tr("dpo_nm_dot"), sWg));
    pRow->addWidget(new QLabel(I18n::tr("dpo_guide_max"), sWg));
    pRow->addWidget(m_guideMax);
    pRow->addWidget(new QLabel(I18n::tr("dpo_deg"), sWg));
    pRow->addWidget(new QLabel(I18n::tr("dpo_outcoupler"), sWg));
    pRow->addWidget(m_outcouplerLen);
    pRow->addWidget(new QLabel(I18n::tr("dpo_mm"), sWg));
    pRow->addWidget(new QLabel(I18n::tr("dpo_eyerelief"), sWg));
    pRow->addWidget(m_eyeRelief);
    pRow->addWidget(new QLabel(I18n::tr("dpo_mm"), sWg));
    pRow->addStretch(1);
    sWg->form()->addRow(I18n::tr("dpo_pupil"), pRow);

    // 設計目標 (評価表の判定バッジのしきい値)
    auto *tgRow = new QHBoxLayout();
    m_fovTarget        = numEdit(60, sWg);
    m_eyeboxTarget     = numEdit(60, sWg);
    m_seeThroughTarget = numEdit(60, sWg);
    tgRow->addWidget(new QLabel(I18n::tr("dpo_tg_fov"), sWg));
    tgRow->addWidget(m_fovTarget);
    tgRow->addWidget(new QLabel(I18n::tr("dpo_tg_eye"), sWg));
    tgRow->addWidget(m_eyeboxTarget);
    tgRow->addWidget(new QLabel(I18n::tr("dpo_tg_see"), sWg));
    tgRow->addWidget(m_seeThroughTarget);
    tgRow->addWidget(new QLabel(I18n::tr("dpo_pct"), sWg));
    tgRow->addStretch(1);
    sWg->form()->addRow(I18n::tr("dpo_targets"), tgRow);

    auto *ckRow = new QHBoxLayout();
    m_threeGratings = makeCheck(I18n::tr("dpo_three_grat"), sWg);
    m_rcwaOptimize  = makeCheck(I18n::tr("dpo_rcwa_opt"),   sWg);
    // RCWA (orcwa) との最適化連携は未実装 — 押せる見た目にしない (絶対規則 5)
    tabhelp::markNotImplemented(m_rcwaOptimize);
    ckRow->addWidget(m_threeGratings);
    ckRow->addWidget(m_rcwaOptimize);
    ckRow->addStretch(1);
    sWg->form()->addRow(ckRow);
    v->addWidget(sWg);

    for (QLineEdit *e : { m_subThick, m_subIndex, m_gratPeriod, m_gratDepth,
                          m_gratSlant, m_designLambda, m_guideMax,
                          m_outcouplerLen, m_eyeRelief, m_fovTarget,
                          m_eyeboxTarget, m_seeThroughTarget })
        connect(e, &QLineEdit::editingFinished, this, &DisplayOpticsTab::onEdited);
    connect(m_threeGratings, &QCheckBox::toggled, this, &DisplayOpticsTab::onEdited);
    connect(m_wgType, &QButtonGroup::idClicked, this, &DisplayOpticsTab::onEdited);

    // 評価 / Metrics
    auto *sMe = new SectionBox(I18n::tr("dpo_metric_sec"), page);
    m_metricTable = makeMetricTable(sMe, 250);
    sMe->vbox()->addWidget(m_metricTable);
    sMe->vbox()->addWidget(noteLabel(I18n::tr("dpo_metric_note"), sMe));

    auto *btnRow = new QHBoxLayout();
    // プロット生成は未実装 — 無効化して明示する (絶対規則 5)
    auto *btnEyebox   = new QPushButton(I18n::tr("dpo_btn_eyebox"), sMe);
    auto *btnTradeoff = new QPushButton(I18n::tr("dpo_btn_tradeoff"), sMe);
    tabhelp::markNotImplemented(btnEyebox);
    tabhelp::markNotImplemented(btnTradeoff);
    btnRow->addWidget(btnEyebox);
    btnRow->addWidget(btnTradeoff);
    btnRow->addStretch(1);
    sMe->vbox()->addLayout(btnRow);
    v->addWidget(sMe);

    return page;
}

// 導波路コンバイナの評価量 — すべて optics/DisplayMetrics の閉形式で計算する
void DisplayOpticsTab::recomputeArwg()
{
    const DisplayOpticsOpts &d = m_p->displayOptics();
    namespace dm = ofd::displayoptics;

    const dm::WaveguideFov fov = dm::waveguideFov(
        d.gratPeriod_nm, d.designLambda_nm, d.subIndex, d.guideMaxAngle_deg);
    const double thetaC = dm::criticalAngle_deg(d.subIndex);
    const double see = dm::slabTransmittance(d.subIndex) * 100.0;

    QVector<MetricOut> rows;

    MetricOut mc;
    mc.item = I18n::tr("dpo_m_critangle");
    mc.value = fmt(thetaC, 2) + QString::fromUtf8(" °");
    mc.target = I18n::tr("dpo_dash");
    mc.judge = I18n::tr("dpo_dash");
    mc.basis = I18n::tr("dpo_b_tir");
    mc.kind = "acc";
    rows.push_back(mc);

    MetricOut gr;
    gr.item = I18n::tr("dpo_m_guiderange");
    gr.value = fmt(thetaC, 1) + QString::fromUtf8(" – ")
             + fmt(qBound(thetaC, d.guideMaxAngle_deg, 90.0), 1)
             + QString::fromUtf8(" °");
    gr.target = I18n::tr("dpo_dash");
    gr.judge = I18n::tr("dpo_dash");
    gr.basis = I18n::tr("dpo_b_tir");
    gr.kind = "acc";
    rows.push_back(gr);

    if (fov.valid) {
        MetricOut mf;
        mf.item = I18n::tr("dpo_m_fov");
        mf.value = fmt(fov.fov_deg, 1) + QString::fromUtf8(" ° (")
                 + fmt(fov.fovMin_deg, 1) + QString::fromUtf8(" … ")
                 + fmt(fov.fovMax_deg, 1) + QString::fromUtf8(" °)");
        mf.target = QString::fromUtf8("≥ ") + fmt(d.fovTarget_deg, 1);
        mf.kind = judgeGE(fov.fov_deg, d.fovTarget_deg);
        mf.judge = I18n::tr(qstrcmp(mf.kind, "ok") == 0 ? "dpo_pass"
                                                        : "dpo_needwork");
        mf.basis = I18n::tr("dpo_b_grating");
        rows.push_back(mf);

        const double eyebox = dm::eyeboxWidth_mm(d.outcouplerLen_mm,
                                                 d.eyeRelief_mm, fov.fov_deg);
        MetricOut me;
        me.item = I18n::tr("dpo_m_eyebox");
        me.value = fmt(eyebox, 2) + QString::fromUtf8(" mm");
        me.target = QString::fromUtf8("≥ ") + fmt(d.eyeboxTarget_mm, 1);
        me.kind = judgeGE(eyebox, d.eyeboxTarget_mm);
        me.judge = I18n::tr(qstrcmp(me.kind, "ok") == 0 ? "dpo_pass"
                                                        : "dpo_needwork");
        me.basis = I18n::tr("dpo_b_eyebox");
        rows.push_back(me);
    } else {
        rows.push_back(uncomputed(I18n::tr("dpo_m_fov"),
                                  I18n::tr("dpo_fov_invalid")));
        rows.push_back(uncomputed(I18n::tr("dpo_m_eyebox"),
                                  I18n::tr("dpo_fov_invalid")));
    }

    MetricOut ms;
    ms.item = I18n::tr("dpo_m_seethru");
    ms.value = fmt(see, 2) + QString::fromUtf8(" %");
    ms.target = QString::fromUtf8("≥ ") + fmt(d.seeThroughTarget_pct, 1);
    ms.kind = judgeGE(see, d.seeThroughTarget_pct);
    ms.judge = I18n::tr(qstrcmp(ms.kind, "ok") == 0 ? "dpo_pass"
                                                    : "dpo_needwork");
    ms.basis = I18n::tr("dpo_b_fresnel");
    rows.push_back(ms);

    // 以下は RCWA / レイトレースが要る量 — 値は出さない (絶対規則 5)
    rows.push_back(uncomputed(I18n::tr("dpo_m_eff"), I18n::tr("dpo_b_needrcwa")));
    rows.push_back(uncomputed(I18n::tr("dpo_m_lumunif"),
                              I18n::tr("dpo_b_needray")));
    rows.push_back(uncomputed(I18n::tr("dpo_m_colunif"),
                              I18n::tr("dpo_b_needspec")));
    rows.push_back(uncomputed(I18n::tr("dpo_m_stray"),
                              I18n::tr("dpo_b_needstray")));

    fillMetricTable(m_metricTable, rows);
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
    m_bottomEmission = makeCheck(I18n::tr("dpo_o_bottom"), s);
    m_topEmission    = makeCheck(I18n::tr("dpo_o_top"),    s);
    m_microcavity    = makeCheck(I18n::tr("dpo_o_cavity"), s);
    stRow->addWidget(m_bottomEmission);
    stRow->addWidget(m_topEmission);
    stRow->addWidget(m_microcavity);
    stRow->addStretch(1);
    s->form()->addRow(I18n::tr("dpo_structure"), stRow);

    s->form()->addRow(I18n::tr("dpo_stack"),
                      hintLabel(I18n::tr("dpo_stack_hint"), s));

    auto *pRow = new QHBoxLayout();
    m_oledIndex = numEdit(70, s);
    m_oledIqe   = numEdit(70, s);
    pRow->addWidget(m_oledIndex);
    pRow->addWidget(new QLabel(I18n::tr("dpo_o_slash"), s));
    pRow->addWidget(m_oledIqe);
    pRow->addStretch(1);
    s->form()->addRow(I18n::tr("dpo_o_params"), pRow);

    auto *lossRow = new QHBoxLayout();
    m_iqe           = makeCheck(I18n::tr("dpo_o_iqe"),    s);
    m_sppLoss       = makeCheck(I18n::tr("dpo_o_spp"),    s);
    m_waveguideLoss = makeCheck(I18n::tr("dpo_o_wgloss"), s);
    lossRow->addWidget(m_iqe);
    lossRow->addWidget(m_sppLoss);
    lossRow->addWidget(m_waveguideLoss);
    lossRow->addStretch(1);
    s->form()->addRow(lossRow);

    auto *exRow = new QHBoxLayout();
    exRow->setSpacing(4);
    m_outcoupling = segRow(exRow, { I18n::tr("dpo_e_none"), I18n::tr("dpo_e_mlens"),
                                    I18n::tr("dpo_e_scat"), I18n::tr("dpo_e_phc") },
                           s);
    s->form()->addRow(I18n::tr("dpo_extract"), exRow);

    auto *eqeRow = new QHBoxLayout();
    m_eqeBadge = makeBadge(QString(), "acc", s);
    eqeRow->addWidget(m_eqeBadge);
    eqeRow->addStretch(1);
    s->form()->addRow(eqeRow);

    m_oledTable = makeMetricTable(s, 190);
    s->vbox()->addWidget(m_oledTable);
    s->vbox()->addWidget(noteLabel(I18n::tr("dpo_oled_note"), s));
    v->addWidget(s);

    for (QLineEdit *e : { m_oledIndex, m_oledIqe })
        connect(e, &QLineEdit::editingFinished, this, &DisplayOpticsTab::onEdited);
    for (QCheckBox *c : { m_bottomEmission, m_topEmission, m_microcavity,
                          m_iqe, m_sppLoss, m_waveguideLoss })
        connect(c, &QCheckBox::toggled, this, &DisplayOpticsTab::onEdited);
    connect(m_outcoupling, &QButtonGroup::idClicked,
            this, &DisplayOpticsTab::onEdited);

    return page;
}

void DisplayOpticsTab::recomputeOled()
{
    const DisplayOpticsOpts &d = m_p->displayOptics();
    namespace dm = ofd::displayoptics;

    const double thetaC = dm::criticalAngle_deg(d.oledIndex);
    const double escape = dm::escapeConeFraction(d.oledIndex);
    const double etaOut = dm::oledOutcoupling(d.oledIndex);
    const double eqe = qBound(0.0, d.oledIqe, 1.0) * etaOut;

    m_eqeBadge->setText(I18n::tr("dpo_eqe_fmt").arg(fmt(eqe * 100.0, 2)));

    QVector<MetricOut> rows;
    auto computed = [](const QString &item, const QString &value,
                       const QString &basis) {
        MetricOut m;
        m.item = item; m.value = value;
        m.target = I18n::tr("dpo_dash");
        m.judge = I18n::tr("dpo_dash");
        m.basis = basis; m.kind = "acc";
        return m;
    };
    rows.push_back(computed(I18n::tr("dpo_m_thetac"),
                            fmt(thetaC, 2) + QString::fromUtf8(" °"),
                            I18n::tr("dpo_b_tir")));
    rows.push_back(computed(I18n::tr("dpo_m_escape"),
                            fmt(escape * 100.0, 2) + QString::fromUtf8(" %"),
                            I18n::tr("dpo_b_escape")));
    rows.push_back(computed(I18n::tr("dpo_m_outc"),
                            fmt(etaOut * 100.0, 2) + QString::fromUtf8(" %"),
                            I18n::tr("dpo_b_oled")));
    rows.push_back(computed(I18n::tr("dpo_m_eqe"),
                            fmt(eqe * 100.0, 2) + QString::fromUtf8(" %"),
                            I18n::tr("dpo_b_eqe_planar")));
    rows.push_back(uncomputed(I18n::tr("dpo_m_extboost"),
                              I18n::tr("dpo_b_needfdtd")));
    rows.push_back(uncomputed(I18n::tr("dpo_m_lossbreak"),
                              I18n::tr("dpo_b_needfdtd")));
    rows.push_back(uncomputed(I18n::tr("dpo_m_colshift"),
                              I18n::tr("dpo_b_needcavity")));
    fillMetricTable(m_oledTable, rows);
}

// ── microLED 解析 (共有セクションの中身) ────────────────────────────────────
QWidget *DisplayOpticsTab::buildMicroLedPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->setContentsMargins(0, 0, 0, 0);

    auto *csRow = new QHBoxLayout();
    m_chipSize = numEdit(70, page);
    csRow->addWidget(m_chipSize);
    csRow->addWidget(new QLabel(I18n::tr("dpo_chipsize_unit"), page));
    csRow->addStretch(1);
    form->addRow(I18n::tr("dpo_chipsize"), csRow);

    auto *pRow = new QHBoxLayout();
    m_mlIndex = numEdit(70, page);
    m_mlIqe   = numEdit(70, page);
    pRow->addWidget(m_mlIndex);
    pRow->addWidget(new QLabel(I18n::tr("dpo_o_slash"), page));
    pRow->addWidget(m_mlIqe);
    pRow->addStretch(1);
    form->addRow(I18n::tr("dpo_ml_params"), pRow);

    auto *srRow = new QHBoxLayout();
    m_mlSurfVel  = numEdit(90, page);
    m_mlLifetime = numEdit(70, page);
    srRow->addWidget(m_mlSurfVel);
    srRow->addWidget(new QLabel(I18n::tr("dpo_ml_srv_unit"), page));
    srRow->addWidget(m_mlLifetime);
    srRow->addWidget(new QLabel(I18n::tr("dpo_ml_ns"), page));
    srRow->addStretch(1);
    form->addRow(I18n::tr("dpo_ml_srv"), srRow);

    auto *ckRow = new QHBoxLayout();
    m_sidewallRecomb = makeCheck(I18n::tr("dpo_ml_recomb"), page);
    m_sidewallDbr    = makeCheck(I18n::tr("dpo_ml_dbr"),    page);
    m_directional    = makeCheck(I18n::tr("dpo_ml_dir"),    page);
    ckRow->addWidget(m_sidewallRecomb);
    ckRow->addWidget(m_sidewallDbr);
    ckRow->addWidget(m_directional);
    ckRow->addStretch(1);
    form->addRow(ckRow);

    auto *bRow = new QHBoxLayout();
    m_microLedBadge = makeBadge(QString(), "acc", page);
    bRow->addWidget(m_microLedBadge);
    bRow->addStretch(1);
    form->addRow(bRow);

    m_mlTable = makeMetricTable(page, 180);
    form->addRow(m_mlTable);
    form->addRow(noteLabel(I18n::tr("dpo_ml_note"), page));

    for (QLineEdit *e : { m_chipSize, m_mlIndex, m_mlIqe, m_mlSurfVel,
                          m_mlLifetime })
        connect(e, &QLineEdit::editingFinished, this, &DisplayOpticsTab::onEdited);
    for (QCheckBox *c : { m_sidewallRecomb, m_sidewallDbr, m_directional })
        connect(c, &QCheckBox::toggled, this, &DisplayOpticsTab::onEdited);

    return page;
}

void DisplayOpticsTab::recomputeMicroLed()
{
    const DisplayOpticsOpts &d = m_p->displayOptics();
    namespace dm = ofd::displayoptics;

    const double thetaC = dm::criticalAngle_deg(d.mlIndex);
    const double top = dm::ledExtractionTopFace(d.mlIndex);
    const double cube = dm::ledExtractionCube(d.mlIndex);
    const double iqe0 = qBound(0.0, d.mlIqe, 1.0);
    const double iqeEff = d.sidewallRecomb
        ? dm::sidewallDeratedIqe(iqe0, d.chipSize_um, d.mlSurfVel_cm_s,
                                 d.mlLifetime_ns)
        : iqe0;

    m_microLedBadge->setText(
        I18n::tr("dpo_ml_badge_fmt").arg(fmt(top * 100.0, 2)));

    QVector<MetricOut> rows;
    auto computed = [](const QString &item, const QString &value,
                       const QString &basis) {
        MetricOut m;
        m.item = item; m.value = value;
        m.target = I18n::tr("dpo_dash");
        m.judge = I18n::tr("dpo_dash");
        m.basis = basis; m.kind = "acc";
        return m;
    };
    rows.push_back(computed(I18n::tr("dpo_m_thetac"),
                            fmt(thetaC, 2) + QString::fromUtf8(" °"),
                            I18n::tr("dpo_b_tir")));
    rows.push_back(computed(I18n::tr("dpo_m_ext_top"),
                            fmt(top * 100.0, 2) + QString::fromUtf8(" %"),
                            I18n::tr("dpo_b_escape")));
    rows.push_back(computed(I18n::tr("dpo_m_ext_cube"),
                            fmt(cube * 100.0, 2) + QString::fromUtf8(" %"),
                            I18n::tr("dpo_b_cube")));
    rows.push_back(computed(I18n::tr("dpo_m_iqe_eff"),
                            fmt(iqeEff * 100.0, 2) + QString::fromUtf8(" %"),
                            I18n::tr("dpo_b_sidewall")));
    rows.push_back(computed(I18n::tr("dpo_m_eqe"),
                            fmt(iqeEff * top * 100.0, 3)
                                + QString::fromUtf8(" … ")
                                + fmt(iqeEff * cube * 100.0, 3)
                                + QString::fromUtf8(" %"),
                            I18n::tr("dpo_b_eqe_planar")));
    if (d.directional) {
        rows.push_back(uncomputed(I18n::tr("dpo_m_halfangle"),
                                  I18n::tr("dpo_b_needraytrace")));
    } else {
        rows.push_back(computed(I18n::tr("dpo_m_halfangle"),
                                QString::fromUtf8("± ")
                                    + fmt(dm::lambertianHalfAngle_deg(), 1)
                                    + QString::fromUtf8(" °"),
                                I18n::tr("dpo_b_lambert")));
    }
    if (d.sidewallDbr)
        rows.push_back(uncomputed(I18n::tr("dpo_ml_dbr"),
                                  I18n::tr("dpo_b_needraytrace")));
    fillMetricTable(m_mlTable, rows);
}

// ── LCD/偏光系 解析 (共有セクションの中身) ──────────────────────────────────
QWidget *DisplayOpticsTab::buildLcdPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->setContentsMargins(0, 0, 0, 0);

    auto *mRow = new QHBoxLayout();
    mRow->setSpacing(4);
    m_lcdMode = segRow(mRow, { "TN", "IPS", "VA" }, page);
    form->addRow(I18n::tr("dpo_lcd_mode"), mRow);

    auto *ckRow = new QHBoxLayout();
    m_lcAnisotropy = makeCheck(I18n::tr("dpo_lc_aniso"), page);
    m_compFilm     = makeCheck(I18n::tr("dpo_lc_film"),  page);
    ckRow->addWidget(m_lcAnisotropy);
    ckRow->addWidget(m_compFilm);
    ckRow->addStretch(1);
    form->addRow(ckRow);

    auto *lumRow = new QHBoxLayout();
    m_lcdPeakLum = numEdit(80, page);
    m_lcdCr      = numEdit(80, page);
    lumRow->addWidget(m_lcdPeakLum);
    lumRow->addWidget(new QLabel(I18n::tr("dpo_lcd_lum_unit"), page));
    lumRow->addWidget(m_lcdCr);
    lumRow->addStretch(1);
    form->addRow(I18n::tr("dpo_lcd_lum"), lumRow);

    auto *ambRow = new QHBoxLayout();
    m_lcdAmbient = numEdit(80, page);
    m_lcdRefl    = numEdit(80, page);
    ambRow->addWidget(m_lcdAmbient);
    ambRow->addWidget(new QLabel(I18n::tr("dpo_lcd_amb_unit"), page));
    ambRow->addWidget(m_lcdRefl);
    ambRow->addStretch(1);
    form->addRow(I18n::tr("dpo_lcd_amb"), ambRow);

    auto *bRow = new QHBoxLayout();
    m_lcdBadge = makeBadge(QString(), "acc", page);
    bRow->addWidget(m_lcdBadge);
    bRow->addStretch(1);
    form->addRow(bRow);

    m_lcdTable = makeMetricTable(page, 150);
    form->addRow(m_lcdTable);
    form->addRow(noteLabel(I18n::tr("dpo_lcd_note"), page));

    for (QLineEdit *e : { m_lcdPeakLum, m_lcdCr, m_lcdAmbient, m_lcdRefl })
        connect(e, &QLineEdit::editingFinished, this, &DisplayOpticsTab::onEdited);
    for (QCheckBox *c : { m_lcAnisotropy, m_compFilm })
        connect(c, &QCheckBox::toggled, this, &DisplayOpticsTab::onEdited);
    connect(m_lcdMode, &QButtonGroup::idClicked, this, &DisplayOpticsTab::onEdited);

    return page;
}

void DisplayOpticsTab::recomputeLcd()
{
    const DisplayOpticsOpts &d = m_p->displayOptics();
    namespace dm = ofd::displayoptics;

    const dm::AmbientContrast ac = dm::ambientContrast(
        d.lcdPeakLum_cdm2, d.lcdDarkroomCr, d.lcdAmbient_lx, d.lcdReflectance);

    QVector<MetricOut> rows;
    auto computed = [](const QString &item, const QString &value,
                       const QString &basis) {
        MetricOut m;
        m.item = item; m.value = value;
        m.target = I18n::tr("dpo_dash");
        m.judge = I18n::tr("dpo_dash");
        m.basis = basis; m.kind = "acc";
        return m;
    };
    if (ac.valid) {
        m_lcdBadge->setText(
            I18n::tr("dpo_lcd_badge_fmt").arg(fmt(ac.contrast, 1)));
        rows.push_back(computed(I18n::tr("dpo_m_amblum"),
                                fmt(ac.ambientLuminance_cdm2, 3)
                                    + QString::fromUtf8(" cd/m²"),
                                I18n::tr("dpo_b_lambert_screen")));
        rows.push_back(computed(I18n::tr("dpo_m_blacklum"),
                                fmt(ac.blackLuminance_cdm2, 4)
                                    + QString::fromUtf8(" cd/m²"),
                                I18n::tr("dpo_b_ambcr")));
        rows.push_back(computed(I18n::tr("dpo_m_ambcr"),
                                fmt(ac.contrast, 1) + QString::fromUtf8(" : 1"),
                                I18n::tr("dpo_b_ambcr")));
    } else {
        m_lcdBadge->setText(I18n::tr("dpo_dash"));
        rows.push_back(uncomputed(I18n::tr("dpo_m_ambcr"),
                                  I18n::tr("dpo_b_ambcr")));
    }
    rows.push_back(uncomputed(I18n::tr("dpo_m_viewangle"),
                              I18n::tr("dpo_b_needberreman")));
    fillMetricTable(m_lcdTable, rows);
}
