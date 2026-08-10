// TransmissionLineTab.cpp
#include "TransmissionLineTab.h"
#include "TabHelpers.h"
#include "../core/Project.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

using namespace ofd;

// ── タブ専用語彙 (file-local 登録, 接頭辞 tln_) ─────────────────────────────
namespace {
const bool s_i18n = [] {
    using ofd::I18n;
    I18n::reg("tln_title", "伝送線路特性 (OpenFDTD §2.14)",
              "Transmission line (OpenFDTD §2.14)");
    I18n::reg("tln_hint", "マイクロストリップ・同軸・導波管などの解析。",
              "Analysis of microstrip, coaxial, waveguide and similar lines.");

    // 特性インピーダンス Z₀
    I18n::reg("tln_z0", "特性インピーダンス Z₀", "Characteristic impedance Z₀");
    I18n::reg("tln_z0_method", "抽出手法", "Extraction method");
    I18n::reg("tln_z0_vi", "V/I 法", "V/I method");
    I18n::reg("tln_z0_power", "電力定義", "Power definition");
    I18n::reg("tln_z0_static", "静電容量法", "Static capacitance method");
    I18n::reg("tln_z0_freqdep", "周波数依存 Z₀(f)", "Frequency-dependent Z₀(f)");
    I18n::reg("tln_z0_reim", "実部・虚部分離", "Separate real / imaginary parts");

    // 伝搬定数 γ
    I18n::reg("tln_gamma", "伝搬定数 γ = α + jβ", "Propagation constant γ = α + jβ");
    I18n::reg("tln_g_beta", "伝搬定数 β(f)", "Phase constant β(f)");
    I18n::reg("tln_g_vp", "位相速度 v_p", "Phase velocity v_p");
    I18n::reg("tln_g_vg", "群速度 v_g", "Group velocity v_g");
    I18n::reg("tln_g_alpha", "減衰定数 α (dB/m)", "Attenuation constant α (dB/m)");
    I18n::reg("tln_g_eeff", "実効誘電率 ε_eff", "Effective permittivity ε_eff");

    // Sパラメータ
    I18n::reg("tln_spara", "Sパラメータ", "S-parameters");
    I18n::reg("tln_ports", "ポート数", "Port count");
    I18n::reg("tln_s_mag", "S11/S21 振幅・位相", "S11/S21 magnitude and phase");
    I18n::reg("tln_s_il", "挿入損失 IL", "Insertion loss IL");
    I18n::reg("tln_s_rl", "リターンロス RL", "Return loss RL");
    I18n::reg("tln_s_delay", "群遅延 τ_g(f)", "Group delay τ_g(f)");
    I18n::reg("tln_s_touchstone", "Touchstone .s2p 出力", "Touchstone .s2p output");

    // 不連続部・整合
    I18n::reg("tln_disc", "不連続部・整合", "Discontinuities and matching");
    I18n::reg("tln_d_bend", "ベンド/分岐の反射", "Bend / branch reflection");
    I18n::reg("tln_d_step", "ステップ不連続", "Step discontinuity");
    I18n::reg("tln_d_xtalk", "近端/遠端クロストーク NEXT/FEXT",
              "Near/far-end crosstalk NEXT/FEXT");
    I18n::reg("tln_d_eye", "アイダイアグラム", "Eye diagram");
    I18n::reg("tln_uw_all", "このタブの設定すべて (タブ全体が設計モックです)",
              "every setting on this tab (the whole tab is a design mock-up)");
    I18n::reg("tln_uw_z0", "特性インピーダンスの算出法と周波数依存 / 複素表示の選択",
              "the characteristic-impedance method and the frequency-dependence / complex-display options");
    I18n::reg("tln_uw_spara", "S パラメータのポート数と出力の選択",
              "the S-parameter port count and output options");
    I18n::reg("tln_uw_chk", "チェック状態",
              "the check boxes");
    return true;
}();

// モックの <Check checked> をそのまま転記
const char *const kGammaKeys[] = { "tln_g_beta", "tln_g_vp", "tln_g_vg",
                                   "tln_g_alpha", "tln_g_eeff" };
const bool kGammaOn[] = { true, false, false, true, true };

const char *const kSKeys[] = { "tln_s_mag", "tln_s_il", "tln_s_rl",
                               "tln_s_delay", "tln_s_touchstone" };
const bool kSOn[] = { true, true, true, false, true };

const char *const kDiscKeys[] = { "tln_d_bend", "tln_d_step", "tln_d_xtalk",
                                  "tln_d_eye" };
const bool kDiscOn[] = { false, false, false, false };
} // namespace

TransmissionLineTab::TransmissionLineTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // ── 伝送線路特性 (説明) ────────────────────────────────────────────────
    auto *sTop = new SectionBox(I18n::tr("tln_title"), body);
    auto *hint = new QLabel(I18n::tr("tln_hint"), sTop);
    hint->setWordWrap(true);
    sTop->vbox()->addWidget(hint);
    // このタブは全体が設計モック — どの設定も計算へ配線されていない
    sTop->vbox()->addWidget(tabhelp::unwiredNote(sTop, I18n::tr("tln_uw_all")));
    v->addWidget(sTop);

    // ── 特性インピーダンス Z₀ ──────────────────────────────────────────────
    auto *sZ = new SectionBox(I18n::tr("tln_z0"), body);
    m_z0Method = new QComboBox(sZ);
    m_z0Method->addItem(I18n::tr("tln_z0_vi"));
    m_z0Method->addItem(I18n::tr("tln_z0_power"));
    m_z0Method->addItem(I18n::tr("tln_z0_static"));
    m_z0Method->setCurrentIndex(0);              // 既定 "vi"
    sZ->form()->addRow(I18n::tr("tln_z0_method"), m_z0Method);
    m_z0FreqDep = new QCheckBox(I18n::tr("tln_z0_freqdep"), sZ);
    m_z0FreqDep->setChecked(true);
    sZ->form()->addRow(m_z0FreqDep);
    m_z0ReIm = new QCheckBox(I18n::tr("tln_z0_reim"), sZ);
    sZ->form()->addRow(m_z0ReIm);
    sZ->form()->addRow(tabhelp::unwiredNote(sZ, I18n::tr("tln_uw_z0")));   // 全設定が未読
    v->addWidget(sZ);

    // ── 伝搬定数 γ = α + jβ ────────────────────────────────────────────────
    v->addWidget(checkSection(body, "tln_gamma", kGammaKeys, kGammaOn,
                              int(sizeof(kGammaKeys) / sizeof(kGammaKeys[0])), &m_gamma));

    // ── Sパラメータ ────────────────────────────────────────────────────────
    auto *sS = new SectionBox(I18n::tr("tln_spara"), body);
    m_ports = new QLineEdit("2", sS);
    m_ports->setMaximumWidth(70);
    sS->form()->addRow(I18n::tr("tln_ports"), m_ports);
    for (int i = 0; i < int(sizeof(kSKeys) / sizeof(kSKeys[0])); ++i) {
        auto *ck = new QCheckBox(I18n::tr(kSKeys[i]), sS);
        ck->setChecked(kSOn[i]);
        sS->form()->addRow(ck);
        m_spara.push_back(ck);
    }
    sS->form()->addRow(tabhelp::unwiredNote(sS, I18n::tr("tln_uw_spara")));   // 全設定が未読
    v->addWidget(sS);

    // ── 不連続部・整合 ────────────────────────────────────────────────────
    v->addWidget(checkSection(body, "tln_disc", kDiscKeys, kDiscOn,
                              int(sizeof(kDiscKeys) / sizeof(kDiscKeys[0])), &m_disc));

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
}

SectionBox *TransmissionLineTab::checkSection(QWidget *parent, const char *titleKey,
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
    // チェック状態はどこにも読まれない (タブ全体が設計モック)
    s->vbox()->addWidget(tabhelp::unwiredNote(s, I18n::tr("tln_uw_chk")));
    return s;
}
