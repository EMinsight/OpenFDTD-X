// AntennaCharTab.cpp
#include "AntennaCharTab.h"
#include "../core/Project.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

using namespace ofd;

// ── タブ専用語彙 (file-local 登録, 接頭辞 ant_) ─────────────────────────────
namespace {
const bool s_i18n = [] {
    using ofd::I18n;
    I18n::reg("ant_title", "アンテナ特性 (OpenFDTD §2.13)",
              "Antenna characteristics (OpenFDTD §2.13)");
    I18n::reg("ant_hint", "アンテナ解析専用の評価指標。給電点 #1 を基準に自動計算。",
              "Antenna-specific figures of merit, computed automatically about feed point #1.");

    // 入力特性 / Input characteristics
    I18n::reg("ant_input", "入力特性", "Input characteristics");
    I18n::reg("ant_in_z", "入力インピーダンス Z(f)", "Input impedance Z(f)");
    I18n::reg("ant_in_vswr", "VSWR(f)", "VSWR(f)");
    I18n::reg("ant_in_gamma", "反射係数 Γ(f), S11", "Reflection coefficient Γ(f), S11");
    I18n::reg("ant_in_res", "共振周波数の自動抽出", "Auto-extract resonant frequencies");
    I18n::reg("ant_in_bw", "-10dB 帯域幅", "-10 dB bandwidth");
    I18n::reg("ant_in_smith", "スミスチャート出力", "Smith chart output");

    // 放射特性 / Radiation characteristics
    I18n::reg("ant_rad", "放射特性", "Radiation characteristics");
    I18n::reg("ant_rad_pattern", "放射パターン (θ, φ) 全方向",
              "Radiation pattern (θ, φ), all directions");
    I18n::reg("ant_rad_beam", "主ビーム方向・3dB幅", "Main-beam direction / 3 dB width");
    I18n::reg("ant_rad_sll", "サイドローブレベル (SLL)", "Side-lobe level (SLL)");
    I18n::reg("ant_rad_fb", "フロントバック比 (F/B)", "Front-to-back ratio (F/B)");
    I18n::reg("ant_rad_gain", "ゲイン (dBi, 絶対)", "Gain (dBi, absolute)");
    I18n::reg("ant_rad_dir", "指向性 (Directivity)", "Directivity");
    I18n::reg("ant_rad_eff", "放射効率 η_rad", "Radiation efficiency η_rad");
    I18n::reg("ant_rad_tot", "アンテナ効率 η_tot (整合損含む)",
              "Antenna efficiency η_tot (incl. mismatch loss)");

    // 偏波特性 / Polarization
    I18n::reg("ant_pol", "偏波特性", "Polarization");
    I18n::reg("ant_pol_comp", "θ成分 / φ成分", "θ component / φ component");
    I18n::reg("ant_pol_ar", "軸比 AR (円偏波)", "Axial ratio AR (circular polarization)");
    I18n::reg("ant_pol_cp", "LHCP / RHCP 分離出力", "LHCP / RHCP separated output");
    I18n::reg("ant_pol_xpd", "交差偏波識別度 XPD", "Cross-polar discrimination XPD");

    // アレイ特性 / Array
    I18n::reg("ant_array", "アレイ特性 (多素子)", "Array (multi-element)");
    I18n::reg("ant_arr_zact", "アクティブインピーダンス Z_act(n)",
              "Active impedance Z_act(n)");
    I18n::reg("ant_arr_coupling", "結合度行列 [S]", "Coupling matrix [S]");
    I18n::reg("ant_arr_steer", "ビームステアリング解析", "Beam-steering analysis");
    I18n::reg("ant_arr_grating", "グレーティングローブ検出", "Grating-lobe detection");

    I18n::reg("ant_output", "出力先", "Outputs");
    return true;
}();

// モックの <Check checked> をそのまま転記 (キー + 既定チェック状態)
const char *const kInputKeys[] = { "ant_in_z", "ant_in_vswr", "ant_in_gamma",
                                   "ant_in_res", "ant_in_bw", "ant_in_smith" };
const bool kInputOn[] = { true, true, true, true, true, false };

const char *const kRadKeys[] = { "ant_rad_pattern", "ant_rad_beam", "ant_rad_sll",
                                 "ant_rad_fb", "ant_rad_gain", "ant_rad_dir",
                                 "ant_rad_eff", "ant_rad_tot" };
const bool kRadOn[] = { true, true, true, false, true, true, true, false };

const char *const kPolKeys[] = { "ant_pol_comp", "ant_pol_ar", "ant_pol_cp",
                                 "ant_pol_xpd" };
const bool kPolOn[] = { true, false, false, false };

const char *const kArrKeys[] = { "ant_arr_zact", "ant_arr_coupling",
                                 "ant_arr_steer", "ant_arr_grating" };
const bool kArrOn[] = { false, false, false, false };
} // namespace

AntennaCharTab::AntennaCharTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // ── アンテナ特性 (説明) ────────────────────────────────────────────────
    auto *sTop = new SectionBox(I18n::tr("ant_title"), body);
    auto *hint = new QLabel(I18n::tr("ant_hint"), sTop);
    hint->setWordWrap(true);
    sTop->vbox()->addWidget(hint);
    v->addWidget(sTop);

    v->addWidget(checkSection(body, "ant_input", kInputKeys, kInputOn,
                              int(sizeof(kInputKeys) / sizeof(kInputKeys[0])), &m_input));
    v->addWidget(checkSection(body, "ant_rad", kRadKeys, kRadOn,
                              int(sizeof(kRadKeys) / sizeof(kRadKeys[0])), &m_radiation));
    v->addWidget(checkSection(body, "ant_pol", kPolKeys, kPolOn,
                              int(sizeof(kPolKeys) / sizeof(kPolKeys[0])), &m_polar));
    v->addWidget(checkSection(body, "ant_array", kArrKeys, kArrOn,
                              int(sizeof(kArrKeys) / sizeof(kArrKeys[0])), &m_array));

    // ── 出力先 ────────────────────────────────────────────────────────────
    auto *sOut = new SectionBox(I18n::tr("ant_output"), body);
    auto *row = new QHBoxLayout();
    row->addWidget(new QPushButton("📄 antenna_report.csv", sOut));
    row->addWidget(new QPushButton("📊 antenna_pattern.h5", sOut));
    row->addWidget(new QPushButton("📐 .nec / .ffe", sOut));
    row->addStretch(1);
    sOut->vbox()->addLayout(row);
    v->addWidget(sOut);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
}

SectionBox *AntennaCharTab::checkSection(QWidget *parent, const char *titleKey,
                                         const char *const *keys, const bool *checked,
                                         int n, QVector<QCheckBox *> *out)
{
    auto *s = new SectionBox(I18n::tr(titleKey), parent);
    for (int i = 0; i < n; ++i) {
        auto *ck = new QCheckBox(I18n::tr(keys[i]), s);
        ck->setChecked(checked[i]);
        s->vbox()->addWidget(ck);
        out->push_back(ck);
    }
    return s;
}
