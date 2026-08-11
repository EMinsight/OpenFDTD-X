// IlluminationTab.cpp
#include "IlluminationTab.h"
#include "TabHelpers.h"
#include "../core/Project.h"
#include "../optics/IlluminationTrace.h"
#include "../optics/SourceSpectrum.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
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
#include <algorithm>
#include <cmath>
#include <cstring>

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
    I18n::reg("ilm_ray_filter",
              "レイデータ (*.ray *.dat *.txt);;すべてのファイル (*)",
              "Ray data (*.ray *.dat *.txt);;All files (*)");
    I18n::reg("ilm_formats",
              "▸ 対応予定形式: .ray (LightTools), .dat (ASAP), IES TM-25, .txt (Radiant) "
              "(取込は未実装 — ファイル名の記録のみ)",
              "▸ Planned formats: .ray (LightTools), .dat (ASAP), IES TM-25, "
              ".txt (Radiant) (import is not implemented — only the file name is "
              "recorded)");
    I18n::reg("ilm_spectrum", "スペクトル", "Spectrum");
    I18n::reg("ilm_sp_white", "白色LED (青LED + 蛍光体)",
              "White LED (blue LED + phosphor)");
    I18n::reg("ilm_sp_rgb",   "RGB 3チップ", "RGB 3-chip");
    I18n::reg("ilm_sp_full",  "フルスペクトル (黒体放射)",
              "Full spectrum (blackbody)");
    I18n::reg("ilm_sp_mono",  "単色 (波長指定)", "Monochromatic (specified wavelength)");
    I18n::reg("ilm_flux", "光束", "Luminous flux");
    I18n::reg("ilm_flux_unit", "lm · ", "lm · ");
    I18n::reg("ilm_rays", "レイ数", "Rays");
    I18n::reg("ilm_sp_params", "スペクトル パラメータ", "Spectrum parameters");
    I18n::reg("ilm_sp_hint",
              "▸ 分光分布はここで指定したモデル (ガウシアンローブ / プランク黒体) "
              "から作られ、測色量はその分布から計算される。",
              "▸ The spectral power distribution is built from the model specified "
              "here (Gaussian lobes / Planckian radiator); the colorimetric "
              "quantities below are computed from that distribution.");
    I18n::reg("ilm_blue",  "青LED ピーク / 半値全幅", "Blue LED peak / FWHM");
    I18n::reg("ilm_phos",  "蛍光体 ピーク / 半値全幅 / ピーク強度比",
              "Phosphor peak / FWHM / peak-intensity ratio");
    I18n::reg("ilm_red",   "R ピーク / 半値全幅 / 強度比",
              "R peak / FWHM / intensity ratio");
    I18n::reg("ilm_green", "G ピーク / 半値全幅 / 強度比",
              "G peak / FWHM / intensity ratio");
    I18n::reg("ilm_blue3", "B ピーク / 半値全幅 / 強度比",
              "B peak / FWHM / intensity ratio");
    I18n::reg("ilm_bbtemp", "黒体温度", "Blackbody temperature");
    I18n::reg("ilm_mono",  "波長 / 線幅 (半値全幅)", "Wavelength / linewidth (FWHM)");
    I18n::reg("ilm_nm", "nm", "nm");
    I18n::reg("ilm_slash", "/", "/");
    I18n::reg("ilm_kelvin", "K", "K");
    I18n::reg("ilm_target_sec", "設計目標", "Design targets");
    I18n::reg("ilm_tg_cct", "CCT", "CCT");
    I18n::reg("ilm_tg_pm", "K ±", "K ±");
    I18n::reg("ilm_tg_duv", "K · |Duv| ≤", "K · |Duv| ≤");

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
    I18n::reg("ilm_c_basis",  "根拠 / 必要な計算", "Basis / required computation");
    I18n::reg("ilm_pass", "適合", "Pass");
    I18n::reg("ilm_fail", "不適合", "Fail");
    I18n::reg("ilm_ref",  "参考", "Reference");
    I18n::reg("ilm_uncomputed", "未計算", "Not computed");
    I18n::reg("ilm_dash", "—", "—");
    I18n::reg("ilm_m_srcflux", "光源光束 (入力値)", "Source luminous flux (input)");
    I18n::reg("ilm_m_flux",    "系の全光束", "System luminous flux");
    I18n::reg("ilm_m_eff",     "光学効率", "Optical efficiency");
    I18n::reg("ilm_m_beam",    "ビーム角 (FWHM)", "Beam angle (FWHM)");
    I18n::reg("ilm_m_unif",    "照度均斉度", "Illuminance uniformity");
    I18n::reg("ilm_m_cct",     "相関色温度 CCT", "Correlated color temperature CCT");
    I18n::reg("ilm_m_duv",     "Duv (黒体軌跡からのずれ)",
              "Duv (distance from the Planckian locus)");
    I18n::reg("ilm_m_ra",      "演色評価数 Ra", "Color rendering index Ra");
    I18n::reg("ilm_m_tm30",    "TM-30 Rf / Rg", "TM-30 Rf / Rg");
    I18n::reg("ilm_m_xy",      "色度座標 (x, y)", "Chromaticity (x, y)");
    I18n::reg("ilm_m_uv",      "色度座標 (u', v')", "Chromaticity (u', v')");
    I18n::reg("ilm_m_ler",     "放射発光効率 K", "Luminous efficacy of radiation K");
    I18n::reg("ilm_m_peak",    "ピーク波長", "Peak wavelength");
    I18n::reg("ilm_m_uvspread","色ムラ (Δu'v' 配光内)",
              "Color non-uniformity (Δu'v' across the beam)");
    I18n::reg("ilm_m_ugr",     "UGR (グレア)", "UGR (glare)");
    I18n::reg("ilm_b_cie",
              "CIE 1931 等色関数 (Wyman 2013 の解析近似) による三刺激値",
              "Tristimulus values from the CIE 1931 colour-matching functions "
              "(analytic fit, Wyman 2013)");
    I18n::reg("ilm_b_cct",
              "CIE 1960 UCS 上で黒体軌跡までの距離を最小化 (Judd の定義)",
              "Minimum distance to the Planckian locus in the CIE 1960 UCS "
              "(Judd's definition)");
    I18n::reg("ilm_b_ler", "K = 683·∫S·V(λ)dλ / ∫S dλ",
              "K = 683·∫S·V(λ)dλ / ∫S dλ");
    I18n::reg("ilm_b_input", "入力値 (光源仕様)", "Input value (source spec)");
    I18n::reg("ilm_b_needtcs",
              "CIE 13.3 試験色 R1..R8 の分光反射率データが必要 — 未実装",
              "Requires the CIE 13.3 test-colour reflectance data (R1..R8) — "
              "not implemented");
    I18n::reg("ilm_b_needtm30",
              "IES TM-30 の 99 試験色データが必要 — 未実装",
              "Requires the 99 IES TM-30 test colours — not implemented");
    I18n::reg("ilm_b_needugr",
              "器具の輝度分布と観測位置 (CIE 117) が必要 — 未実装",
              "Requires the luminaire luminance distribution and observer geometry "
              "(CIE 117) — not implemented");
    I18n::reg("ilm_cct_undef",
              "黒体軌跡から離れすぎて CCT は定義されない (単色光など)",
              "Too far from the Planckian locus for CCT to be defined "
              "(e.g. monochromatic light)");
    I18n::reg("ilm_photo_note",
              "▸ 測色量は上のスペクトルモデルから計算した結果。"
              "配光量は上のレイトレース幾何を非順次モンテカルロで追跡した結果。"
              "「—」の行は分光反射率データ・波長分解の追跡・輝度分布 (いずれも未実装) "
              "が必要な量で、値を推定して表示することはしない。",
              "▸ The colorimetric quantities are computed from the spectrum model "
              "above; the photometric ones come from a non-sequential Monte Carlo "
              "trace of the ray-trace geometry. Rows showing “—” need spectral "
              "reflectance data, a wavelength-resolved trace or a luminance "
              "distribution (none implemented); no estimated numbers are shown.");
    I18n::reg("ilm_btn_polar", "🗺 配光曲線 (極座標)",
              "🗺 Intensity distribution (polar)");
    I18n::reg("ilm_btn_cie",   "🎨 CIE色度図", "🎨 CIE chromaticity diagram");
    I18n::reg("ilm_btn_illum", "📊 照度分布 (床面)",
              "📊 Illuminance distribution (floor)");
    I18n::reg("ilm_btn_ies",   "💾 IES / LDT 配光ファイル書出",
              "💾 Export IES / LDT photometric file");
    I18n::reg("ilm_ies_hint",
              "▸ IES LM-63 / EULUMDAT (.ldt) 書出は DIALux・AGi32 等の"
              "照明設計ソフト向け (書出は未実装)。",
              "▸ IES LM-63 / EULUMDAT (.ldt) export targets lighting design tools "
              "such as DIALux and AGi32 (export is not implemented).");
    I18n::reg("ilm_uw_optics",
              "TIRレンズ・導光板・蛍光体散乱の 3 要素と、面の反射モデルのうち "
              "「BSDF実測」(いずれも追跡モデルに入っていません)",
              "the TIR lens, light guide and phosphor scattering, and the "
              "“measured BSDF” surface model (none of them are in the traced model)");
    I18n::reg("ilm_uw_optics_ok",
              "面の反射モデル「鏡面 / 拡散 / ABG」と、リフレクタ・拡散板・評価面の"
              "寸法 — 下の非順次レイトレースに入り、配光量 (全光束・光学効率・"
              "ビーム角・均斉度) になります",
              "the “specular / diffuse / ABG” surface models and the reflector, "
              "diffuser and target dimensions — they feed the non-sequential ray "
              "trace below and produce the photometric quantities (flux, "
              "efficiency, beam angle, uniformity)");

    // 非順次レイトレース (optics/IlluminationTrace) の入力と結果
    I18n::reg("ilm_tr_sec", "レイトレース幾何 / Ray-trace geometry",
              "Ray-trace geometry");
    I18n::reg("ilm_tr_refl", "リフレクタ (回転放物面) 焦点距離 / 開口半径 / 反射率",
              "Reflector (paraboloid) focal length / rim radius / reflectance");
    I18n::reg("ilm_tr_diff", "拡散板 位置 z / 半径 / 透過率",
              "Diffuser position z / radius / transmittance");
    I18n::reg("ilm_tr_abg", "ABG 係数 A / B / g", "ABG coefficients A / B / g");
    I18n::reg("ilm_tr_target", "評価面 距離 / 半幅", "Target plane distance / half-width");
    I18n::reg("ilm_tr_chip", "LEDチップ 一辺", "LED chip side");
    I18n::reg("ilm_mm", "mm", "mm");
    I18n::reg("ilm_tr_hint",
              "▸ 光源を焦点に置いた回転放物面。開口半径は R > 2f が要る "
              "(R ≤ 2f では縁が焦点より下に来て光線を捕まえられない)。"
              "評価面は系より遠くに置く。BSDF(Δβ) = A/(B + Δβ^g)。",
              "▸ A paraboloid with the source at its focus. The rim radius must "
              "satisfy R > 2f (at R ≤ 2f the rim sits below the focus and "
              "intercepts nothing). The target plane must be beyond the system. "
              "BSDF(Δβ) = A/(B + Δβ^g).");
    I18n::reg("ilm_m_iaxis", "軸上光度", "Axial intensity");
    I18n::reg("ilm_m_ecenter", "評価面 中心照度", "Target-plane centre illuminance");
    I18n::reg("ilm_b_ray", "非順次モンテカルロ・レイトレース (光線 %1 本)",
              "Non-sequential Monte Carlo ray trace (%1 rays)");
    I18n::reg("ilm_b_ray_stat",
              "レイトレース — セルあたりの光線が足りず統計誤差が大きい (光線数を増やす)",
              "Ray trace — too few rays per cell for a meaningful figure "
              "(increase the ray count)");
    I18n::reg("ilm_b_ray_beam",
              "レイトレース — 中心光度の半分へ落ちる角度が無い (視野内に半値点なし)",
              "Ray trace — the intensity never falls to half of its axial value");
    I18n::reg("ilm_b_needwl",
              "波長分解のレイトレースが必要 (現在の追跡は波長を追わない) — 未実装",
              "Requires a wavelength-resolved ray trace (the current tracer does "
              "not follow wavelength) — not implemented");
    // 追跡できない理由 (絶対規則 5: 値を出さずに理由を出す)
    I18n::reg("ilm_no_raydata",
              "光源モデル「レイデータ (実測)」— ファイル取込が未実装。"
              "「ランバート面」か「LEDチップ」なら追跡します",
              "Source model “ray data (measured)” — the file import is not "
              "implemented. “Lambertian surface” or “LED chip” will trace");
    I18n::reg("ilm_no_bsdf",
              "面の反射モデル「BSDF実測」— 測定データが無い。"
              "「鏡面 / 拡散 / ABGモデル」なら追跡します",
              "Surface model “measured BSDF” — no measured data. "
              "“Specular / diffuse / ABG” will trace");
    I18n::reg("ilm_no_elem",
              "TIRレンズ・導光板・蛍光体散乱は追跡モデルに入っていない "
              "(チェックを外すと追跡します)",
              "The TIR lens, light guide and phosphor scattering are not in the "
              "traced model (clear them to trace)");
    I18n::reg("ilm_no_focal", "リフレクタの焦点距離が 0 以下",
              "The reflector focal length must be positive");
    I18n::reg("ilm_no_radius",
              "リフレクタの開口半径は R > 2f が要る (拡散板の半径は 0 より大きく)",
              "The reflector rim radius must satisfy R > 2f (and the diffuser "
              "radius must be positive)");
    I18n::reg("ilm_no_target", "評価面は系より遠くへ置く (距離・半幅が 0 以下)",
              "The target plane must be beyond the system (distance and "
              "half-width must be positive)");
    I18n::reg("ilm_no_flux", "光束 (または LEDチップ寸法) が 0 以下",
              "The luminous flux (or the LED chip size) must be positive");
    I18n::reg("ilm_no_rays", "光線数が 0 以下", "The ray count must be positive");
    return true;
}();

// 測光・測色表の 1 行 (kind: "ok"/"warn"/"acc"/"" = 未計算)
struct PhotoOut {
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

double readNum(QLineEdit *e, double fallback)
{
    bool ok = false;
    const double v = e->text().toDouble(&ok);
    return ok ? v : fallback;
}

// 未計算行 (値も目標も出さない)
PhotoOut uncomputed(const QString &item, const QString &basis)
{
    PhotoOut p;
    p.item = item;
    p.value = I18n::tr("ilm_dash");
    p.target = I18n::tr("ilm_dash");
    p.judge = I18n::tr("ilm_uncomputed");
    p.basis = basis;
    p.kind = "";
    return p;
}

// 計算済みだが判定対象でない行
PhotoOut computed(const QString &item, const QString &value,
                  const QString &basis)
{
    PhotoOut p;
    p.item = item;
    p.value = value;
    p.target = I18n::tr("ilm_dash");
    p.judge = I18n::tr("ilm_ref");
    p.basis = basis;
    p.kind = "acc";
    return p;
}

// パラメータ 1 行 (ラベル + QLineEdit の並び) を作るヘルパー
QWidget *paramRow(QWidget *parent, const QVector<QLineEdit *> &edits,
                  const QStringList &units)
{
    auto *w = new QWidget(parent);
    auto *h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    for (int i = 0; i < edits.size(); ++i) {
        h->addWidget(edits[i]);
        if (i < units.size() && !units[i].isEmpty())
            h->addWidget(new QLabel(units[i], w));
    }
    h->addStretch(1);
    return w;
}
} // namespace

// ── IlluminationTab ─────────────────────────────────────────────────────────
IlluminationTab::IlluminationTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project),
      m_app(nullptr),
      m_srcModel(nullptr), m_rayFile(nullptr), m_spectrum(nullptr),
      m_flux(nullptr), m_rays(nullptr), m_spectrumStack(nullptr),
      m_bluePeak(nullptr), m_blueFwhm(nullptr), m_phosPeak(nullptr),
      m_phosFwhm(nullptr), m_phosRatio(nullptr),
      m_rPeak(nullptr), m_rFwhm(nullptr), m_rRatio(nullptr),
      m_gPeak(nullptr), m_gFwhm(nullptr), m_gRatio(nullptr),
      m_bPeak(nullptr), m_bFwhm(nullptr), m_bRatio(nullptr),
      m_blackbody(nullptr), m_monoPeak(nullptr), m_monoFwhm(nullptr),
      m_cctTarget(nullptr), m_cctTol(nullptr), m_duvTol(nullptr),
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
                             I18n::tr("ilm_app_solar") }, sTop);
    sTop->form()->addRow(I18n::tr("ilm_app"), appRow);
    v->addWidget(sTop);

    // ── 光源 / Light source ─────────────────────────────────────────────────
    auto *sSrc = new SectionBox(I18n::tr("ilm_src_sec"), body);

    auto *mdlRow = new QHBoxLayout();
    mdlRow->setSpacing(4);
    m_srcModel = segRow(mdlRow, { I18n::tr("ilm_mdl_lambert"),
                                  I18n::tr("ilm_mdl_ray"),
                                  I18n::tr("ilm_mdl_chip") }, sSrc);
    sSrc->form()->addRow(I18n::tr("ilm_model"), mdlRow);

    auto *rayRow = new QHBoxLayout();
    m_rayFile = new QLineEdit(sSrc);
    rayRow->addWidget(m_rayFile, 1);
    // 参照: ファイル選択のみ実配線 (取込パーサは未実装 — ファイル名の記録のみ)
    auto *rayBrowse = new QPushButton(I18n::tr("ilm_browse"), sSrc);
    connect(rayBrowse, &QPushButton::clicked, this, [this] {
        const QString f = QFileDialog::getOpenFileName(
            this, I18n::tr("ilm_raydata"), m_rayFile->text(),
            I18n::tr("ilm_ray_filter"));
        if (!f.isEmpty()) { m_rayFile->setText(f); onEdited(); }
    });
    rayRow->addWidget(rayBrowse);
    sSrc->form()->addRow(I18n::tr("ilm_raydata"), rayRow);

    sSrc->form()->addRow(hintLabel(I18n::tr("ilm_formats"), sSrc));

    m_spectrum = new QComboBox(sSrc);
    m_spectrum->addItem(I18n::tr("ilm_sp_white"));
    m_spectrum->addItem(I18n::tr("ilm_sp_rgb"));
    m_spectrum->addItem(I18n::tr("ilm_sp_full"));
    m_spectrum->addItem(I18n::tr("ilm_sp_mono"));
    sSrc->form()->addRow(I18n::tr("ilm_spectrum"), m_spectrum);

    // スペクトルモデルのパラメータ (選択したモデルの欄だけを見せる)
    m_spectrumStack = new QStackedWidget(sSrc);
    {   // 0: 白色 LED (青 + 蛍光体)
        auto *pg = new QWidget(m_spectrumStack);
        auto *f = new QFormLayout(pg);
        f->setContentsMargins(0, 0, 0, 0);
        m_bluePeak = numEdit(70, pg); m_blueFwhm = numEdit(70, pg);
        m_phosPeak = numEdit(70, pg); m_phosFwhm = numEdit(70, pg);
        m_phosRatio = numEdit(70, pg);
        f->addRow(I18n::tr("ilm_blue"),
                  paramRow(pg, { m_bluePeak, m_blueFwhm },
                           { I18n::tr("ilm_nm"), I18n::tr("ilm_nm") }));
        f->addRow(I18n::tr("ilm_phos"),
                  paramRow(pg, { m_phosPeak, m_phosFwhm, m_phosRatio },
                           { I18n::tr("ilm_nm"), I18n::tr("ilm_nm"), QString() }));
        m_spectrumStack->addWidget(pg);
    }
    {   // 1: RGB 3 チップ
        auto *pg = new QWidget(m_spectrumStack);
        auto *f = new QFormLayout(pg);
        f->setContentsMargins(0, 0, 0, 0);
        m_rPeak = numEdit(70, pg); m_rFwhm = numEdit(70, pg); m_rRatio = numEdit(70, pg);
        m_gPeak = numEdit(70, pg); m_gFwhm = numEdit(70, pg); m_gRatio = numEdit(70, pg);
        m_bPeak = numEdit(70, pg); m_bFwhm = numEdit(70, pg); m_bRatio = numEdit(70, pg);
        const QStringList u = { I18n::tr("ilm_nm"), I18n::tr("ilm_nm"), QString() };
        f->addRow(I18n::tr("ilm_red"),   paramRow(pg, { m_rPeak, m_rFwhm, m_rRatio }, u));
        f->addRow(I18n::tr("ilm_green"), paramRow(pg, { m_gPeak, m_gFwhm, m_gRatio }, u));
        f->addRow(I18n::tr("ilm_blue3"), paramRow(pg, { m_bPeak, m_bFwhm, m_bRatio }, u));
        m_spectrumStack->addWidget(pg);
    }
    {   // 2: フルスペクトル (黒体放射)
        auto *pg = new QWidget(m_spectrumStack);
        auto *f = new QFormLayout(pg);
        f->setContentsMargins(0, 0, 0, 0);
        m_blackbody = numEdit(80, pg);
        f->addRow(I18n::tr("ilm_bbtemp"),
                  paramRow(pg, { m_blackbody }, { I18n::tr("ilm_kelvin") }));
        m_spectrumStack->addWidget(pg);
    }
    {   // 3: 単色
        auto *pg = new QWidget(m_spectrumStack);
        auto *f = new QFormLayout(pg);
        f->setContentsMargins(0, 0, 0, 0);
        m_monoPeak = numEdit(70, pg); m_monoFwhm = numEdit(70, pg);
        f->addRow(I18n::tr("ilm_mono"),
                  paramRow(pg, { m_monoPeak, m_monoFwhm },
                           { I18n::tr("ilm_nm"), I18n::tr("ilm_nm") }));
        m_spectrumStack->addWidget(pg);
    }
    sSrc->form()->addRow(I18n::tr("ilm_sp_params"), m_spectrumStack);
    sSrc->form()->addRow(noteLabel(I18n::tr("ilm_sp_hint"), sSrc));

    auto *fluxRow = new QHBoxLayout();
    m_flux = numEdit(100, sSrc);
    m_rays = numEdit(100, sSrc);
    fluxRow->addWidget(m_flux);
    fluxRow->addWidget(new QLabel(I18n::tr("ilm_flux_unit"), sSrc));
    fluxRow->addWidget(new QLabel(I18n::tr("ilm_rays"), sSrc));
    fluxRow->addWidget(m_rays);
    fluxRow->addStretch(1);
    sSrc->form()->addRow(I18n::tr("ilm_flux"), fluxRow);

    auto *tgRow = new QHBoxLayout();
    m_cctTarget = numEdit(80, sSrc);
    m_cctTol    = numEdit(70, sSrc);
    m_duvTol    = numEdit(80, sSrc);
    tgRow->addWidget(new QLabel(I18n::tr("ilm_tg_cct"), sSrc));
    tgRow->addWidget(m_cctTarget);
    tgRow->addWidget(new QLabel(I18n::tr("ilm_tg_pm"), sSrc));
    tgRow->addWidget(m_cctTol);
    tgRow->addWidget(new QLabel(I18n::tr("ilm_tg_duv"), sSrc));
    tgRow->addWidget(m_duvTol);
    tgRow->addStretch(1);
    sSrc->form()->addRow(I18n::tr("ilm_target_sec"), tgRow);
    v->addWidget(sSrc);

    // ── 光学系 / Optics ─────────────────────────────────────────────────────
    auto *sOpt = new SectionBox(I18n::tr("ilm_opt_sec"), body);

    auto *o1 = new QHBoxLayout();
    m_reflector = makeCheck(I18n::tr("ilm_o_reflector"), sOpt);
    m_tirLens   = makeCheck(I18n::tr("ilm_o_tir"),       sOpt);
    m_diffuser  = makeCheck(I18n::tr("ilm_o_diffuser"),  sOpt);
    o1->addWidget(m_reflector);
    o1->addWidget(m_tirLens);
    o1->addWidget(m_diffuser);
    o1->addStretch(1);
    sOpt->form()->addRow(o1);

    auto *o2 = new QHBoxLayout();
    m_lightGuide = makeCheck(I18n::tr("ilm_o_guide"),    sOpt);
    m_phosphor   = makeCheck(I18n::tr("ilm_o_phosphor"), sOpt);
    o2->addWidget(m_lightGuide);
    o2->addWidget(m_phosphor);
    o2->addStretch(1);
    sOpt->form()->addRow(o2);

    auto *sfRow = new QHBoxLayout();
    sfRow->setSpacing(4);
    m_surface = segRow(sfRow, { I18n::tr("ilm_sf_specular"),
                                I18n::tr("ilm_sf_lambert"),
                                I18n::tr("ilm_sf_bsdf"),
                                I18n::tr("ilm_sf_abg") }, sOpt);
    sOpt->form()->addRow(I18n::tr("ilm_surface"), sfRow);
    // TIRレンズ・導光板・蛍光体・BSDF実測は追跡モデルに無い (絶対規則 5)
    sOpt->vbox()->addWidget(tabhelp::unwiredNote(sOpt, I18n::tr("ilm_uw_optics"), I18n::tr("ilm_uw_optics_ok")));
    v->addWidget(sOpt);

    // ── レイトレース幾何 / Ray-trace geometry ──────────────────────────────
    auto *sTr = new SectionBox(I18n::tr("ilm_tr_sec"), body);
    const QString mm = I18n::tr("ilm_mm");
    m_reflF   = numEdit(70, sTr); m_reflR  = numEdit(70, sTr); m_reflRho = numEdit(70, sTr);
    m_diffZ   = numEdit(70, sTr); m_diffR  = numEdit(70, sTr); m_diffTau = numEdit(70, sTr);
    m_abgA    = numEdit(70, sTr); m_abgB   = numEdit(70, sTr); m_abgG    = numEdit(70, sTr);
    m_tgtD    = numEdit(70, sTr); m_tgtW   = numEdit(70, sTr); m_chip    = numEdit(70, sTr);
    sTr->form()->addRow(I18n::tr("ilm_tr_refl"),
                        paramRow(sTr, { m_reflF, m_reflR, m_reflRho },
                                 { mm, mm, QString() }));
    sTr->form()->addRow(I18n::tr("ilm_tr_diff"),
                        paramRow(sTr, { m_diffZ, m_diffR, m_diffTau },
                                 { mm, mm, QString() }));
    sTr->form()->addRow(I18n::tr("ilm_tr_abg"),
                        paramRow(sTr, { m_abgA, m_abgB, m_abgG },
                                 { QString(), QString(), QString() }));
    sTr->form()->addRow(I18n::tr("ilm_tr_target"),
                        paramRow(sTr, { m_tgtD, m_tgtW }, { mm, mm }));
    sTr->form()->addRow(I18n::tr("ilm_tr_chip"),
                        paramRow(sTr, { m_chip }, { mm }));
    sTr->vbox()->addWidget(hintLabel(I18n::tr("ilm_tr_hint"), sTr));
    v->addWidget(sTr);

    // ── 測光・測色 / Photometry & color ─────────────────────────────────────
    auto *sPh = new SectionBox(I18n::tr("ilm_photo_sec"), body);

    m_photoTable = new QTableWidget(0, 5, sPh);
    m_photoTable->setHorizontalHeaderLabels({ I18n::tr("ilm_c_metric"),
                                              I18n::tr("ilm_c_value"),
                                              I18n::tr("ilm_c_target"),
                                              I18n::tr("ilm_c_judge"),
                                              I18n::tr("ilm_c_basis") });
    m_photoTable->verticalHeader()->setVisible(false);
    m_photoTable->verticalHeader()->setDefaultSectionSize(26);
    m_photoTable->horizontalHeader()
        ->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_photoTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_photoTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_photoTable->setMinimumHeight(330);
    sPh->vbox()->addWidget(m_photoTable);
    sPh->vbox()->addWidget(noteLabel(I18n::tr("ilm_photo_note"), sPh));

    auto *btnRow = new QHBoxLayout();
    // プロット生成・IES/LDT 書出は未実装 — 無効化して明示する (絶対規則 5)
    auto *btnPolar = new QPushButton(I18n::tr("ilm_btn_polar"), sPh);
    auto *btnCie   = new QPushButton(I18n::tr("ilm_btn_cie"), sPh);
    auto *btnIllum = new QPushButton(I18n::tr("ilm_btn_illum"), sPh);
    auto *btnIes   = new QPushButton(I18n::tr("ilm_btn_ies"), sPh);
    for (QPushButton *b : { btnPolar, btnCie, btnIllum, btnIes }) {
        tabhelp::markNotImplemented(b);
        btnRow->addWidget(b);
    }
    btnRow->addStretch(1);
    sPh->vbox()->addLayout(btnRow);

    sPh->vbox()->addWidget(hintLabel(I18n::tr("ilm_ies_hint"), sPh));
    v->addWidget(sPh);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    // ── 配線 ────────────────────────────────────────────────────────────────
    for (QLineEdit *e : { m_reflF, m_reflR, m_reflRho, m_diffZ, m_diffR,
                          m_diffTau, m_abgA, m_abgB, m_abgG,
                          m_tgtD, m_tgtW, m_chip })
        connect(e, &QLineEdit::editingFinished, this, &IlluminationTab::onEdited);
    for (QLineEdit *e : { m_rayFile, m_flux, m_rays,
                          m_bluePeak, m_blueFwhm, m_phosPeak, m_phosFwhm,
                          m_phosRatio, m_rPeak, m_rFwhm, m_rRatio,
                          m_gPeak, m_gFwhm, m_gRatio, m_bPeak, m_bFwhm, m_bRatio,
                          m_blackbody, m_monoPeak, m_monoFwhm,
                          m_cctTarget, m_cctTol, m_duvTol })
        connect(e, &QLineEdit::editingFinished, this, &IlluminationTab::onEdited);
    for (QCheckBox *c : { m_reflector, m_tirLens, m_diffuser, m_lightGuide,
                          m_phosphor })
        connect(c, &QCheckBox::toggled, this, &IlluminationTab::onEdited);
    for (QButtonGroup *g : { m_app, m_srcModel, m_surface })
        connect(g, &QButtonGroup::idClicked, this, &IlluminationTab::onEdited);
    connect(m_spectrum, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { updateSpectrumPage(); onEdited(); });
    connect(project, &Project::loaded, this, &IlluminationTab::refresh);

    refresh();
}

void IlluminationTab::apply()
{
    if (m_updating) return;
    IlluminationOpts &o = m_p->illumination();

    o.app      = qBound(0, m_app->checkedId(), 3);
    o.srcModel = qBound(0, m_srcModel->checkedId(), 2);
    o.rayFile  = m_rayFile->text();
    o.spectrum = qBound(0, m_spectrum->currentIndex(), 3);
    o.flux_lm  = readNum(m_flux, o.flux_lm);
    o.rays     = readNum(m_rays, o.rays);

    o.reflector  = m_reflector->isChecked();
    o.tirLens    = m_tirLens->isChecked();
    o.diffuser   = m_diffuser->isChecked();
    o.lightGuide = m_lightGuide->isChecked();
    o.phosphor   = m_phosphor->isChecked();
    o.surface    = qBound(0, m_surface->checkedId(), 3);

    o.bluePeak_nm = readNum(m_bluePeak, o.bluePeak_nm);
    o.blueFwhm_nm = readNum(m_blueFwhm, o.blueFwhm_nm);
    o.phosPeak_nm = readNum(m_phosPeak, o.phosPeak_nm);
    o.phosFwhm_nm = readNum(m_phosFwhm, o.phosFwhm_nm);
    o.phosRatio   = readNum(m_phosRatio, o.phosRatio);
    o.rPeak_nm = readNum(m_rPeak, o.rPeak_nm);
    o.rFwhm_nm = readNum(m_rFwhm, o.rFwhm_nm);
    o.rRatio   = readNum(m_rRatio, o.rRatio);
    o.gPeak_nm = readNum(m_gPeak, o.gPeak_nm);
    o.gFwhm_nm = readNum(m_gFwhm, o.gFwhm_nm);
    o.gRatio   = readNum(m_gRatio, o.gRatio);
    o.bPeak_nm = readNum(m_bPeak, o.bPeak_nm);
    o.bFwhm_nm = readNum(m_bFwhm, o.bFwhm_nm);
    o.bRatio   = readNum(m_bRatio, o.bRatio);
    o.blackbody_K = readNum(m_blackbody, o.blackbody_K);
    o.monoPeak_nm = readNum(m_monoPeak, o.monoPeak_nm);
    o.monoFwhm_nm = readNum(m_monoFwhm, o.monoFwhm_nm);

    o.cctTarget_K = readNum(m_cctTarget, o.cctTarget_K);
    o.cctTol_K    = readNum(m_cctTol, o.cctTol_K);
    o.duvTol      = readNum(m_duvTol, o.duvTol);

    o.reflFocal_mm  = readNum(m_reflF, o.reflFocal_mm);
    o.reflRadius_mm = readNum(m_reflR, o.reflRadius_mm);
    o.reflReflect   = readNum(m_reflRho, o.reflReflect);
    o.diffZ_mm      = readNum(m_diffZ, o.diffZ_mm);
    o.diffRadius_mm = readNum(m_diffR, o.diffRadius_mm);
    o.diffTrans     = readNum(m_diffTau, o.diffTrans);
    o.abgA          = readNum(m_abgA, o.abgA);
    o.abgB          = readNum(m_abgB, o.abgB);
    o.abgG          = readNum(m_abgG, o.abgG);
    o.targetDist_mm = readNum(m_tgtD, o.targetDist_mm);
    o.targetHalf_mm = readNum(m_tgtW, o.targetHalf_mm);
    o.chipSize_mm   = readNum(m_chip, o.chipSize_mm);

    m_p->touch();
}

void IlluminationTab::refresh()
{
    m_updating = true;
    const IlluminationOpts &o = m_p->illumination();

    setSeg(m_app, o.app);
    setSeg(m_srcModel, o.srcModel);
    m_rayFile->setText(o.rayFile);
    m_spectrum->setCurrentIndex(qBound(0, o.spectrum, 3));
    m_flux->setText(fmt(o.flux_lm, 1));
    m_rays->setText(QString::number(o.rays, 'g', 9));

    m_reflector->setChecked(o.reflector);
    m_tirLens->setChecked(o.tirLens);
    m_diffuser->setChecked(o.diffuser);
    m_lightGuide->setChecked(o.lightGuide);
    m_phosphor->setChecked(o.phosphor);
    setSeg(m_surface, o.surface);

    m_bluePeak->setText(fmt(o.bluePeak_nm, 1));
    m_blueFwhm->setText(fmt(o.blueFwhm_nm, 1));
    m_phosPeak->setText(fmt(o.phosPeak_nm, 1));
    m_phosFwhm->setText(fmt(o.phosFwhm_nm, 1));
    m_phosRatio->setText(fmt(o.phosRatio, 3));
    m_rPeak->setText(fmt(o.rPeak_nm, 1));
    m_rFwhm->setText(fmt(o.rFwhm_nm, 1));
    m_rRatio->setText(fmt(o.rRatio, 3));
    m_gPeak->setText(fmt(o.gPeak_nm, 1));
    m_gFwhm->setText(fmt(o.gFwhm_nm, 1));
    m_gRatio->setText(fmt(o.gRatio, 3));
    m_bPeak->setText(fmt(o.bPeak_nm, 1));
    m_bFwhm->setText(fmt(o.bFwhm_nm, 1));
    m_bRatio->setText(fmt(o.bRatio, 3));
    m_blackbody->setText(fmt(o.blackbody_K, 1));
    m_monoPeak->setText(fmt(o.monoPeak_nm, 1));
    m_monoFwhm->setText(fmt(o.monoFwhm_nm, 2));

    m_cctTarget->setText(fmt(o.cctTarget_K, 1));
    m_cctTol->setText(fmt(o.cctTol_K, 1));
    m_duvTol->setText(fmt(o.duvTol, 4));

    m_reflF->setText(fmt(o.reflFocal_mm, 2));
    m_reflR->setText(fmt(o.reflRadius_mm, 2));
    m_reflRho->setText(fmt(o.reflReflect, 3));
    m_diffZ->setText(fmt(o.diffZ_mm, 2));
    m_diffR->setText(fmt(o.diffRadius_mm, 2));
    m_diffTau->setText(fmt(o.diffTrans, 3));
    m_abgA->setText(QString::number(o.abgA, 'g', 4));
    m_abgB->setText(QString::number(o.abgB, 'g', 4));
    m_abgG->setText(fmt(o.abgG, 2));
    m_tgtD->setText(fmt(o.targetDist_mm, 1));
    m_tgtW->setText(fmt(o.targetHalf_mm, 1));
    m_chip->setText(fmt(o.chipSize_mm, 3));

    m_updating = false;
    updateSpectrumPage();
    recompute();
}

void IlluminationTab::onEdited()
{
    if (m_updating) return;
    apply();
    recompute();
}

void IlluminationTab::updateSpectrumPage()
{
    m_spectrumStack->setCurrentIndex(qBound(0, m_spectrum->currentIndex(), 3));
}

// IlluminationOpts → 非順次レイトレースの系。追跡が成り立たないときは
// 「なぜ計算しないのか」を利用者へそのまま出せる I18n キーを返す (絶対規則 5)。
static const char *buildTraceScene(const IlluminationOpts &o,
                                   ofd::illum::Scene *sc, long long *nRays)
{
    using namespace ofd::illum;

    // 追跡モデルに入っていない選択 — 値を出さずに理由を返す
    if (o.srcModel == 1) return "ilm_no_raydata";      // レイデータ (実測)
    if (o.surface == 2)  return "ilm_no_bsdf";         // BSDF 実測
    if (o.tirLens || o.lightGuide || o.phosphor) return "ilm_no_elem";

    Scatter model = Scatter::Specular;
    if (o.surface == 1)      model = Scatter::Lambertian;
    else if (o.surface == 3) model = Scatter::ABG;

    Scene &s = *sc;
    s.source.kind = (o.srcModel == 2) ? Source::Chip : Source::Point;
    s.source.size_mm = o.chipSize_mm;
    s.source.flux_lm = o.flux_lm;

    s.reflector.enabled = o.reflector;
    s.reflector.focal_mm = o.reflFocal_mm;
    s.reflector.radius_mm = o.reflRadius_mm;
    s.reflector.reflectance = o.reflReflect;
    s.reflector.model = model;
    s.reflector.abg = { o.abgA, o.abgB, o.abgG };

    s.diffuser.enabled = o.diffuser;
    s.diffuser.z_mm = o.diffZ_mm;
    s.diffuser.radius_mm = o.diffRadius_mm;
    s.diffuser.transmittance = o.diffTrans;
    // 拡散板の「鏡面」は素通し (散乱しない透明板) の意味になる
    s.diffuser.model = model;
    s.diffuser.abg = { o.abgA, o.abgB, o.abgG };

    s.target.distance_mm = o.targetDist_mm;
    s.target.half_mm = o.targetHalf_mm;

    // 編集のたびに追跡するので本数は打ち切る (根拠欄に実際の本数を出す)
    const double want = (o.rays > 0.0) ? o.rays : 0.0;
    *nRays = static_cast<long long>(std::min(want, 200000.0));

    const char *b = traceBlocker(s, *nRays);
    if (b == nullptr) return nullptr;
    if (std::strcmp(b, "rays") == 0)   return "ilm_no_rays";
    if (std::strcmp(b, "flux") == 0)   return "ilm_no_flux";
    if (std::strcmp(b, "focal") == 0)  return "ilm_no_focal";
    if (std::strcmp(b, "radius") == 0) return "ilm_no_radius";
    return "ilm_no_target";
}

// スペクトルモデル → 測色量、レイトレース → 配光量。
// 分光反射率データ・波長分解の追跡・輝度分布が要る量は「—」のまま (絶対規則 5)。
void IlluminationTab::recompute()
{
    const IlluminationOpts &o = m_p->illumination();
    const optics::SourceColor c = optics::evaluateSource(o);

    QVector<PhotoOut> rows;

    // 入力値であることを明示した光源光束 (系の全光束ではない)
    rows.push_back(computed(I18n::tr("ilm_m_srcflux"),
                            fmt(o.flux_lm, 1) + QString::fromUtf8(" lm"),
                            I18n::tr("ilm_b_input")));

    if (c.valid) {
        // CCT: 目標との突き合わせ (判定バッジは計算値に対してのみ出す)
        if (c.cct.valid) {
            PhotoOut cct;
            cct.item = I18n::tr("ilm_m_cct");
            cct.value = fmt(c.cct.cct_K, 0) + QString::fromUtf8(" K");
            cct.target = fmt(o.cctTarget_K, 0) + QString::fromUtf8(" ± ")
                       + fmt(o.cctTol_K, 0) + QString::fromUtf8(" K");
            const bool ok = std::fabs(c.cct.cct_K - o.cctTarget_K) <= o.cctTol_K;
            cct.kind = ok ? "ok" : "warn";
            cct.judge = I18n::tr(ok ? "ilm_pass" : "ilm_fail");
            cct.basis = I18n::tr("ilm_b_cct");
            rows.push_back(cct);

            PhotoOut duv;
            duv.item = I18n::tr("ilm_m_duv");
            duv.value = QString::number(c.cct.duv, 'f', 5);
            duv.target = QString::fromUtf8("|Duv| ≤ ") + fmt(o.duvTol, 4);
            const bool dok = std::fabs(c.cct.duv) <= o.duvTol;
            duv.kind = dok ? "ok" : "warn";
            duv.judge = I18n::tr(dok ? "ilm_pass" : "ilm_fail");
            duv.basis = I18n::tr("ilm_b_cct");
            rows.push_back(duv);
        } else {
            rows.push_back(uncomputed(I18n::tr("ilm_m_cct"),
                                      I18n::tr("ilm_cct_undef")));
            rows.push_back(uncomputed(I18n::tr("ilm_m_duv"),
                                      I18n::tr("ilm_cct_undef")));
        }

        rows.push_back(computed(I18n::tr("ilm_m_xy"),
                                fmt(c.chrom.x, 4) + QString::fromUtf8(", ")
                                    + fmt(c.chrom.y, 4),
                                I18n::tr("ilm_b_cie")));
        rows.push_back(computed(I18n::tr("ilm_m_uv"),
                                fmt(c.chrom.up, 4) + QString::fromUtf8(", ")
                                    + fmt(c.chrom.vp, 4),
                                I18n::tr("ilm_b_cie")));
        rows.push_back(computed(I18n::tr("ilm_m_ler"),
                                fmt(c.efficacy_lm_W, 1)
                                    + QString::fromUtf8(" lm/W"),
                                I18n::tr("ilm_b_ler")));
        rows.push_back(computed(I18n::tr("ilm_m_peak"),
                                fmt(c.peak_nm, 1) + QString::fromUtf8(" nm"),
                                I18n::tr("ilm_b_cie")));
    } else {
        for (const char *key : { "ilm_m_cct", "ilm_m_duv", "ilm_m_xy",
                                 "ilm_m_uv", "ilm_m_ler", "ilm_m_peak" })
            rows.push_back(uncomputed(I18n::tr(key), I18n::tr("ilm_b_cie")));
    }

    // 分光反射率の数表が要る演色性指標 (未実装)
    rows.push_back(uncomputed(I18n::tr("ilm_m_ra"), I18n::tr("ilm_b_needtcs")));
    rows.push_back(uncomputed(I18n::tr("ilm_m_tm30"), I18n::tr("ilm_b_needtm30")));

    // ── 配光量 = 非順次レイトレース (optics/IlluminationTrace) の実計算 ────
    illum::Scene scene;
    long long nRays = 0;
    if (const char *why = buildTraceScene(o, &scene, &nRays)) {
        for (const char *key : { "ilm_m_flux", "ilm_m_eff", "ilm_m_beam",
                                 "ilm_m_unif", "ilm_m_iaxis", "ilm_m_ecenter" })
            rows.push_back(uncomputed(I18n::tr(key), I18n::tr(why)));
    } else {
        const illum::Result t = illum::trace(scene, nRays);
        const QString basis = I18n::tr("ilm_b_ray").arg(nRays);
        rows.push_back(computed(I18n::tr("ilm_m_flux"),
                                fmt(t.fluxOut_lm, 1) + QString::fromUtf8(" lm"),
                                basis));
        rows.push_back(computed(I18n::tr("ilm_m_eff"),
                                fmt(100.0 * t.efficiency, 1)
                                    + QString::fromUtf8(" %"),
                                basis));
        if (t.beamValid)
            rows.push_back(computed(I18n::tr("ilm_m_beam"),
                                    fmt(t.beamAngleFwhm_deg, 1)
                                        + QString::fromUtf8(" °"),
                                    basis));
        else
            rows.push_back(uncomputed(I18n::tr("ilm_m_beam"),
                                      I18n::tr("ilm_b_ray_beam")));
        if (t.uniformityValid)
            rows.push_back(computed(I18n::tr("ilm_m_unif"),
                                    fmt(t.uniformityMinAvg, 3), basis));
        else
            rows.push_back(uncomputed(I18n::tr("ilm_m_unif"),
                                      I18n::tr("ilm_b_ray_stat")));
        rows.push_back(computed(I18n::tr("ilm_m_iaxis"),
                                fmt(t.axialIntensity_cd, 1)
                                    + QString::fromUtf8(" cd"),
                                basis));
        rows.push_back(computed(I18n::tr("ilm_m_ecenter"),
                                fmt(t.illumCenter_lx, 1)
                                    + QString::fromUtf8(" lx"),
                                basis));
    }
    // 色ムラは波長分解の追跡が、UGR は輝度分布が要る (どちらも未実装)
    rows.push_back(uncomputed(I18n::tr("ilm_m_uvspread"), I18n::tr("ilm_b_needwl")));
    rows.push_back(uncomputed(I18n::tr("ilm_m_ugr"), I18n::tr("ilm_b_needugr")));

    m_photoTable->clearContents();
    m_photoTable->setRowCount(rows.size());
    for (int r = 0; r < rows.size(); ++r) {
        const PhotoOut &p = rows[r];
        m_photoTable->setItem(r, 0, textItem(p.item));
        m_photoTable->setItem(r, 1, numItem(p.value));
        m_photoTable->setItem(r, 2, numItem(p.target));
        m_photoTable->setCellWidget(r, 3, badgeCell(p.judge, p.kind));
        m_photoTable->setItem(r, 4, textItem(p.basis));
        // 「根拠」列は左パネルが狭いと隠れるので、行全体のツールチップにも出す
        for (int c = 0; c < 5; ++c)
            if (auto *it = m_photoTable->item(r, c)) it->setToolTip(p.basis);
    }
}
