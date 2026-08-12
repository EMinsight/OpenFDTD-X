// TransmissionLineTab.cpp
#include "TransmissionLineTab.h"
#include "TabHelpers.h"
#include "../core/Project.h"
#include "../core/EyeDiagram.h"
#include "../core/TransmissionLine.h"
#include "../io/Touchstone.h"
#include "../widgets/MiniPlot.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include <cmath>

using namespace ofd;

// ── タブ専用語彙 (file-local 登録, 接頭辞 tln_) ─────────────────────────────
namespace {
const bool s_i18n = [] {
    using ofd::I18n;
    I18n::reg("tln_title", "伝送線路特性 (OpenFDTD §2.14)",
              "Transmission line (OpenFDTD §2.14)");
    I18n::reg("tln_hint",
              "マイクロストリップ・ストリップライン・同軸・平行 2 線・"
              "コプレーナの準 TEM 解析。断面と材料から Z₀ / ε_eff / γ / S を"
              "閉形式で計算します。",
              "Quasi-TEM analysis of microstrip, stripline, coaxial, two-wire "
              "and coplanar lines. Z0 / eps_eff / gamma / S are computed in "
              "closed form from the cross-section and the materials.");

    // 断面
    I18n::reg("tln_geom", "断面と材料", "Cross-section and materials");
    I18n::reg("tln_kind", "線路種別", "Line type");
    I18n::reg("tln_k_ms",   "マイクロストリップ", "Microstrip");
    I18n::reg("tln_k_sl",   "ストリップライン", "Stripline");
    I18n::reg("tln_k_coax", "同軸", "Coaxial");
    I18n::reg("tln_k_2w",   "平行 2 線", "Two-wire");
    I18n::reg("tln_k_cpw",  "コプレーナ (CPW)", "Coplanar (CPW)");
    I18n::reg("tln_w", "線路幅 W [mm]", "Strip width W [mm]");
    I18n::reg("tln_h", "基板厚 h [mm]", "Substrate height h [mm]");
    I18n::reg("tln_b_sl", "地板間隔 b [mm]", "Ground spacing b [mm]");
    I18n::reg("tln_a", "内導体半径 a [mm]", "Inner radius a [mm]");
    I18n::reg("tln_b", "外導体内半径 b [mm]", "Outer radius b [mm]");
    I18n::reg("tln_d", "中心間隔 D [mm]", "Centre spacing D [mm]");
    I18n::reg("tln_dia", "線径 d [mm]", "Wire diameter d [mm]");
    I18n::reg("tln_s_cpw", "中心導体幅 S [mm]", "Centre conductor width S [mm]");
    I18n::reg("tln_slot", "スロット幅 W [mm]", "Slot width W [mm]");
    I18n::reg("tln_epsr", "比誘電率 εr", "Relative permittivity epsr");
    I18n::reg("tln_tand", "誘電正接 tanδ", "Loss tangent tan(delta)");
    I18n::reg("tln_sigma", "導体導電率 σ [MS/m]", "Conductivity sigma [MS/m]");
    I18n::reg("tln_length", "線路長 [mm]", "Line length [mm]");
    I18n::reg("tln_freq", "周波数 [GHz]", "Frequency [GHz]");
    I18n::reg("tln_z0ref", "基準インピーダンス [Ω]", "Reference impedance [ohm]");

    // 特性インピーダンス Z₀
    I18n::reg("tln_z0", "特性インピーダンス Z₀", "Characteristic impedance Z₀");
    I18n::reg("tln_z0_method", "抽出手法", "Extraction method");
    I18n::reg("tln_z0_vi", "V/I 法", "V/I method");
    I18n::reg("tln_z0_power", "電力定義", "Power definition");
    I18n::reg("tln_z0_static", "静電容量法", "Static capacitance method");
    I18n::reg("tln_z0_freqdep", "周波数依存 Z₀(f)", "Frequency-dependent Z₀(f)");
    I18n::reg("tln_z0_reim", "実部・虚部分離", "Separate real / imaginary parts");
    I18n::reg("tln_uw_method",
              "抽出手法の選択 — 準 TEM ではこの 3 定義は一致するので値は"
              "変わりません (差が出るのは非 TEM モード)",
              "the extraction method — the three definitions coincide in the "
              "quasi-TEM range, so the number does not change (they differ "
              "only for non-TEM modes)");

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
    I18n::reg("tln_s2p_btn", "💾 .s2p 書出 (周波数掃引)",
              "💾 Export .s2p (frequency sweep)");
    I18n::reg("tln_s2p_title", "Touchstone .s2p の書出",
              "Export a Touchstone .s2p file");
    I18n::reg("tln_s2p_filter", "Touchstone 2ポート (*.s2p);;すべてのファイル (*)",
              "Touchstone 2-port (*.s2p);;All files (*)");
    I18n::reg("tln_s2p_ok", "書き出しました: %1\n%2 点 (%3 〜 %4 GHz)",
              "Written: %1\n%2 points (%3 to %4 GHz)");
    I18n::reg("tln_s2p_ng", "書き出せませんでした: %1", "Could not write: %1");
    I18n::reg("tln_uw_ports",
              "ポート数 — 一様な線路 1 本は本質的に 2 ポートなので、"
              "3 ポート以上は回路網の定義が要ります (未実装)",
              "the port count — a single uniform line is inherently a 2-port, "
              "so more ports need a network definition (not implemented)");

    // 不連続部・整合
    I18n::reg("tln_disc", "不連続部・整合", "Discontinuities and matching");
    I18n::reg("tln_d_bend", "ベンド/分岐の反射", "Bend / branch reflection");
    I18n::reg("tln_d_step", "ステップ不連続", "Step discontinuity");
    I18n::reg("tln_d_xtalk", "近端/遠端クロストーク NEXT/FEXT",
              "Near/far-end crosstalk NEXT/FEXT");
    I18n::reg("tln_d_eye", "アイダイアグラム", "Eye diagram");
    I18n::reg("tln_uw_disc",
              "ベンド・ステップ・クロストークの 3 項目 (等価回路モデルが"
              "未実装 — ベンド/ステップは等価容量/インダクタンス、"
              "クロストークは結合線路が要ります)",
              "the bend, step and crosstalk items (no equivalent-circuit model "
              "yet: bends and steps need equivalent C/L, crosstalk needs "
              "coupled lines)");
    // アイダイアグラム (実装済み)
    I18n::reg("tln_eye_show", "アイダイアグラムを描く", "Draw the eye diagram");
    I18n::reg("tln_eye_rate", "ビットレート [Gbps]", "Bit rate [Gbps]");
    I18n::reg("tln_eye_prbs", "PRBS 次数 (周期 2ⁿ−1)", "PRBS order (2^n-1)");
    I18n::reg("tln_eye_rise", "遷移時間 [ps]", "Transition time [ps]");
    I18n::reg("tln_eye_x", "時間 [ps]", "Time [ps]");
    I18n::reg("tln_eye_y", "受信電圧 [V]", "Received voltage [V]");
    I18n::reg("tln_eye_note",
              "開口 %1 V (送信振幅 2.000 V に対して) / 開口幅 %2 ps "
              "(1 UI = %3 ps) / データ依存ジッタ %4 ps。線路の S21(f) を掛けた"
              "受信波形を PRBS 1 周期の巡回畳み込みで求め、1 UI ごとに"
              "折り返しています。",
              "Opening %1 V (against a 2.000 V transmitted swing) / opening "
              "width %2 ps (1 UI = %3 ps) / data-dependent jitter %4 ps. The "
              "received waveform is the cyclic convolution of one PRBS period "
              "with the line's S21(f), folded every UI.");
    I18n::reg("tln_eye_shut",
              "アイは閉じています (開口 %1 V ≤ 0) — この線路長・ビットレート"
              "では判定できません。",
              "The eye is shut (opening %1 V <= 0) -- this length and bit rate "
              "cannot be sliced.");
    I18n::reg("tln_eye_alias",
              " ⚠ 1 UI あたりの標本数がチャネルを解像できていません "
              "(|S21| がナイキストで %1)。折り返しで開口が正しく出ないので、"
              "ビットレートを下げるか線路を長くしてください。",
              " [!] The sampling does not resolve the channel (|S21| at Nyquist "
              "is %1). Aliasing makes the opening unreliable -- lower the bit "
              "rate or lengthen the line.");

    // 結果表
    I18n::reg("tln_res", "計算結果", "Results");
    I18n::reg("tln_c_item", "項目", "Quantity");
    I18n::reg("tln_c_value", "値", "Value");
    I18n::reg("tln_c_basis", "根拠", "Basis");
    I18n::reg("tln_r_z0", "特性インピーダンス Z₀", "Characteristic impedance Z₀");
    I18n::reg("tln_r_z0c", "複素 Z₀ (損失込み)", "Complex Z₀ (with loss)");
    I18n::reg("tln_r_z0f", "Z₀ (%1 GHz)", "Z₀ (%1 GHz)");
    I18n::reg("tln_r_s11", "S11 (振幅 / 位相)", "S11 (magnitude / phase)");
    I18n::reg("tln_r_s21", "S21 (振幅 / 位相)", "S21 (magnitude / phase)");
    I18n::reg("tln_r_il", "挿入損失 IL", "Insertion loss IL");
    I18n::reg("tln_r_rl", "リターンロス RL", "Return loss RL");
    I18n::reg("tln_r_delay", "群遅延 (線路長ぶん)", "Group delay (over the length)");
    I18n::reg("tln_r_elec", "電気長", "Electrical length");
    I18n::reg("tln_b_exact", "断面の厳密解 (等角写像 / 閉形式)",
              "Exact for this cross-section (conformal mapping / closed form)");
    I18n::reg("tln_b_hj", "Hammerstad & Jensen (1980) の近似式 (数値解に 1% 級)",
              "Hammerstad & Jensen (1980) fit (about 1% against numerical "
              "solutions)");
    I18n::reg("tln_b_line", "分布定数線路の式 (γ と Z₀ から)",
              "Distributed-line formulas (from gamma and Z0)");
    I18n::reg("tln_b_lossexact", "表皮抵抗からの厳密式 (同軸)",
              "Exact from the surface resistance (coaxial)");
    I18n::reg("tln_b_lossapx",
              "広線路近似 — 細い線路では過小評価します",
              "Wide-strip approximation — it underestimates narrow lines");
    I18n::reg("tln_invalid",
              "▸ 断面の寸法が不正です (内外半径・線間隔・幅が正で、"
              "外側が内側より大きいこと)。",
              "▸ The cross-section is not valid (radii, spacings and widths "
              "must be positive, and the outer dimension larger than the "
              "inner one).");
    I18n::reg("tln_note",
              "▸ 値はすべて準 TEM の閉形式による計算結果です。導体厚 t の補正・"
              "高次モード・分散 (マイクロストリップの ε_eff(f)) は扱いません。"
              "任意断面は OpenFEM / OpenPEEC の領分です。",
              "▸ Every number is a quasi-TEM closed-form result. Conductor "
              "thickness, higher-order modes and dispersion (the microstrip "
              "eps_eff(f)) are not covered. Arbitrary cross-sections belong to "
              "OpenFEM / OpenPEEC.");
    return true;
}();

QDoubleSpinBox *spin(QWidget *p, double lo, double hi, double val, int dec,
                     double step = 0.1)
{
    auto *s = new QDoubleSpinBox(p);
    s->setRange(lo, hi);
    s->setDecimals(dec);
    s->setSingleStep(step);
    s->setValue(val);
    s->setMaximumWidth(120);
    return s;
}

const char *const kGammaKeys[] = { "tln_g_beta", "tln_g_vp", "tln_g_vg",
                                   "tln_g_alpha", "tln_g_eeff" };
const char *const kSKeys[] = { "tln_s_mag", "tln_s_il", "tln_s_rl",
                               "tln_s_delay", "tln_s_touchstone" };
const char kAccEm[] = "#0078D4";     // Theme.cpp の既定 accent (--acc-em)

// アイダイアグラムは実装済みなので、ここ (未実装の一覧) には残さない
const char *const kDiscKeys[] = { "tln_d_bend", "tln_d_step", "tln_d_xtalk" };

QTableWidgetItem *cell(const QString &t)
{
    auto *it = new QTableWidgetItem(t);
    it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    return it;
}
} // namespace

TransmissionLineTab::TransmissionLineTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // ── 説明 ───────────────────────────────────────────────────────────────
    auto *sTop = new SectionBox(I18n::tr("tln_title"), body);
    auto *hint = new QLabel(I18n::tr("tln_hint"), sTop);
    hint->setWordWrap(true);
    sTop->vbox()->addWidget(hint);
    v->addWidget(sTop);

    // ── 断面と材料 ─────────────────────────────────────────────────────────
    auto *sG = new SectionBox(I18n::tr("tln_geom"), body);
    m_kind = new QComboBox(sG);
    m_kind->addItem(I18n::tr("tln_k_ms"));
    m_kind->addItem(I18n::tr("tln_k_sl"));
    m_kind->addItem(I18n::tr("tln_k_coax"));
    m_kind->addItem(I18n::tr("tln_k_2w"));
    m_kind->addItem(I18n::tr("tln_k_cpw"));
    sG->form()->addRow(I18n::tr("tln_kind"), m_kind);

    // 種別ごとに独立した入力欄を持つ (共有ウィジェットの付け替えはしない)。
    // ストリップラインとマイクロストリップは同じモデル欄 (w_mm / h_mm) を
    // 使うので、refresh() で両方へ同じ値を書き、apply() は現在のページから読む。
    m_geom = new QStackedWidget(sG);
    m_w    = spin(m_geom, 0.001, 1000.0, 3.0, 3);    // マイクロストリップ W
    m_h    = spin(m_geom, 0.001, 1000.0, 1.6, 3);    // 同 h
    m_slW  = spin(m_geom, 0.001, 1000.0, 3.0, 3);    // ストリップライン W
    m_slB  = spin(m_geom, 0.001, 1000.0, 1.6, 3);    // 同 b (地板間隔)
    m_a    = spin(m_geom, 0.001, 1000.0, 0.5, 3);
    m_b    = spin(m_geom, 0.001, 1000.0, 1.68, 3);
    m_d    = spin(m_geom, 0.001, 1000.0, 3.0, 3);
    m_dia  = spin(m_geom, 0.001, 1000.0, 1.0, 3);
    m_cpwS = spin(m_geom, 0.001, 1000.0, 3.0, 3);    // CPW 中心導体幅 S
    m_slot = spin(m_geom, 0.001, 1000.0, 0.3, 3);
    struct Page { const char *l1; QDoubleSpinBox *w1; const char *l2;
                  QDoubleSpinBox *w2; };
    const Page kPages[5] = {
        { "tln_w",     m_w,    "tln_h",    m_h   },
        { "tln_w",     m_slW,  "tln_b_sl", m_slB },
        { "tln_a",     m_a,    "tln_b",    m_b   },
        { "tln_d",     m_d,    "tln_dia",  m_dia },
        { "tln_s_cpw", m_cpwS, "tln_slot", m_slot },
    };
    for (const Page &pg : kPages) {
        auto *w = new QWidget(m_geom);
        auto *f = new QFormLayout(w);
        f->setContentsMargins(0, 0, 0, 0);
        f->addRow(I18n::tr(pg.l1), pg.w1);
        f->addRow(I18n::tr(pg.l2), pg.w2);
        m_geom->addWidget(w);
    }
    sG->form()->addRow(m_geom);

    m_epsr   = spin(sG, 1.0, 100.0, 4.4, 3, 0.1);
    m_tanD   = spin(sG, 0.0, 1.0, 0.02, 5, 0.001);
    m_sigma  = spin(sG, 0.0, 100.0, 58.0, 2, 1.0);   // MS/m
    m_length = spin(sG, 0.0, 100000.0, 50.0, 2, 1.0);
    m_freq   = spin(sG, 1.0e-6, 1000.0, 1.0, 4, 0.1);
    m_z0Ref  = spin(sG, 0.1, 10000.0, 50.0, 2, 1.0);
    sG->form()->addRow(I18n::tr("tln_epsr"), m_epsr);
    sG->form()->addRow(I18n::tr("tln_tand"), m_tanD);
    sG->form()->addRow(I18n::tr("tln_sigma"), m_sigma);
    sG->form()->addRow(I18n::tr("tln_length"), m_length);
    sG->form()->addRow(I18n::tr("tln_freq"), m_freq);
    sG->form()->addRow(I18n::tr("tln_z0ref"), m_z0Ref);
    v->addWidget(sG);

    // ── 特性インピーダンス Z₀ ──────────────────────────────────────────────
    auto *sZ = new SectionBox(I18n::tr("tln_z0"), body);
    m_z0Method = new QComboBox(sZ);
    m_z0Method->addItem(I18n::tr("tln_z0_vi"));
    m_z0Method->addItem(I18n::tr("tln_z0_power"));
    m_z0Method->addItem(I18n::tr("tln_z0_static"));
    // 準 TEM ではこの 3 定義は一致する — 選べるように見せない (絶対規則 5)
    m_z0Method->setEnabled(false);
    sZ->form()->addRow(I18n::tr("tln_z0_method"), m_z0Method);
    m_z0FreqDep = new QCheckBox(I18n::tr("tln_z0_freqdep"), sZ);
    sZ->form()->addRow(m_z0FreqDep);
    m_z0ReIm = new QCheckBox(I18n::tr("tln_z0_reim"), sZ);
    sZ->form()->addRow(m_z0ReIm);
    sZ->vbox()->addWidget(tabhelp::unwiredNote(sZ, I18n::tr("tln_uw_method")));
    v->addWidget(sZ);

    // ── 伝搬定数 γ ─────────────────────────────────────────────────────────
    auto *sGa = new SectionBox(I18n::tr("tln_gamma"), body);
    for (const char *k : kGammaKeys) {
        auto *ck = new QCheckBox(I18n::tr(k), sGa);
        sGa->vbox()->addWidget(ck);
        m_gamma.push_back(ck);
    }
    v->addWidget(sGa);

    // ── S パラメータ ───────────────────────────────────────────────────────
    auto *sS = new SectionBox(I18n::tr("tln_spara"), body);
    m_ports = new QSpinBox(sS);
    m_ports->setRange(1, 32);
    m_ports->setMaximumWidth(80);
    sS->form()->addRow(I18n::tr("tln_ports"), m_ports);
    for (const char *k : kSKeys) {
        auto *ck = new QCheckBox(I18n::tr(k), sS);
        sS->form()->addRow(ck);
        m_spara.push_back(ck);
    }
    m_s2pBtn = new QPushButton(I18n::tr("tln_s2p_btn"), sS);
    connect(m_s2pBtn, &QPushButton::clicked,
            this, &TransmissionLineTab::exportTouchstone);
    auto *bRow = new QHBoxLayout();
    bRow->addWidget(m_s2pBtn);
    bRow->addStretch(1);
    sS->vbox()->addLayout(bRow);
    sS->vbox()->addWidget(tabhelp::unwiredNote(sS, I18n::tr("tln_uw_ports")));
    v->addWidget(sS);

    // ── 不連続部・整合 (未実装) ────────────────────────────────────────────
    auto *sD = new SectionBox(I18n::tr("tln_disc"), body);
    for (const char *k : kDiscKeys) {
        auto *ck = new QCheckBox(I18n::tr(k), sD);
        ck->setEnabled(false);
        sD->vbox()->addWidget(ck);
        m_disc.push_back(ck);
    }
    sD->vbox()->addWidget(tabhelp::unwiredNote(sD, I18n::tr("tln_uw_disc")));
    v->addWidget(sD);

    // ── アイダイアグラム (実装済み — 上の 3 項目とは別扱い) ───────────────
    auto *sE = new SectionBox(I18n::tr("tln_d_eye"), body);
    m_eyeShow = new QCheckBox(I18n::tr("tln_eye_show"), sE);
    sE->vbox()->addWidget(m_eyeShow);
    auto *eForm = new QFormLayout();
    m_eyeRate = new QDoubleSpinBox(sE);
    m_eyeRate->setRange(0.001, 400.0);
    m_eyeRate->setDecimals(3);
    m_eyeRate->setSuffix(" Gbps");
    m_eyePrbs = new QSpinBox(sE);
    m_eyePrbs->setRange(3, 11);
    m_eyeRise = new QDoubleSpinBox(sE);
    m_eyeRise->setRange(0.0, 1000.0);
    m_eyeRise->setDecimals(1);
    m_eyeRise->setSuffix(" ps");
    eForm->addRow(I18n::tr("tln_eye_rate"), m_eyeRate);
    eForm->addRow(I18n::tr("tln_eye_prbs"), m_eyePrbs);
    eForm->addRow(I18n::tr("tln_eye_rise"), m_eyeRise);
    sE->vbox()->addLayout(eForm);
    m_eyePlot = new MiniPlot(sE);
    m_eyePlot->setMinimumHeight(190);
    m_eyePlot->setLabels(I18n::tr("tln_eye_x"), I18n::tr("tln_eye_y"));
    sE->vbox()->addWidget(m_eyePlot);
    m_eyeNote = new QLabel(sE);
    m_eyeNote->setWordWrap(true);
    sE->vbox()->addWidget(m_eyeNote);
    v->addWidget(sE);
    connect(m_eyeShow, &QCheckBox::toggled, this, &TransmissionLineTab::apply);
    connect(m_eyeRate, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &TransmissionLineTab::apply);
    connect(m_eyePrbs, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &TransmissionLineTab::apply);
    connect(m_eyeRise, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &TransmissionLineTab::apply);

    // ── 結果 ───────────────────────────────────────────────────────────────
    auto *sR = new SectionBox(I18n::tr("tln_res"), body);
    m_table = new QTableWidget(0, 3, sR);
    m_table->setHorizontalHeaderLabels({ I18n::tr("tln_c_item"),
                                         I18n::tr("tln_c_value"),
                                         I18n::tr("tln_c_basis") });
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(24);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setMinimumHeight(300);
    sR->vbox()->addWidget(m_table);
    m_note = new QLabel(sR);
    m_note->setWordWrap(true);
    sR->vbox()->addWidget(m_note);
    auto *foot = new QLabel(I18n::tr("tln_note"), sR);
    foot->setWordWrap(true);
    foot->setStyleSheet(QStringLiteral("color:#888888;"));
    sR->vbox()->addWidget(foot);
    v->addWidget(sR);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    // ── 配線 ───────────────────────────────────────────────────────────────
    for (QDoubleSpinBox *s : { m_w, m_h, m_slW, m_slB, m_a, m_b, m_d, m_dia,
                               m_cpwS, m_slot, m_epsr, m_tanD, m_sigma,
                               m_length, m_freq, m_z0Ref })
        connect(s, &QDoubleSpinBox::valueChanged, this,
                [this](double) { onEdited(); });
    connect(m_kind, &QComboBox::currentIndexChanged, this,
            [this](int) { onEdited(); });
    connect(m_ports, &QSpinBox::valueChanged, this, [this](int) { onEdited(); });
    for (QCheckBox *c : m_gamma)
        connect(c, &QCheckBox::toggled, this, [this](bool) { onEdited(); });
    for (QCheckBox *c : m_spara)
        connect(c, &QCheckBox::toggled, this, [this](bool) { onEdited(); });
    for (QCheckBox *c : { m_z0FreqDep, m_z0ReIm })
        connect(c, &QCheckBox::toggled, this, [this](bool) { onEdited(); });
    connect(project, &Project::loaded, this, &TransmissionLineTab::refresh);

    refresh();
}

void TransmissionLineTab::apply()
{
    if (m_updating) return;
    TransmissionLineOpts &t = m_p->tline();
    t.kind = qBound(0, m_kind->currentIndex(), 4);
    // 幅と厚みはページごとに別の入力欄なので、現在の種別のものを読む
    switch (t.kind) {
    case 1: t.w_mm = m_slW->value();  t.h_mm = m_slB->value(); break;
    case 4: t.w_mm = m_cpwS->value(); break;
    default: t.w_mm = m_w->value();   t.h_mm = m_h->value();   break;
    }
    t.a_mm = m_a->value();
    t.b_mm = m_b->value();
    t.d_mm = m_d->value();
    t.dia_mm = m_dia->value();
    t.slot_mm = m_slot->value();
    t.epsr = m_epsr->value();
    t.tanD = m_tanD->value();
    t.sigma_Sm = m_sigma->value() * 1.0e6;      // MS/m → S/m
    t.length_mm = m_length->value();
    t.freq_GHz = m_freq->value();
    t.z0Ref_ohm = m_z0Ref->value();
    t.ports = m_ports->value();
    t.showBeta   = m_gamma[0]->isChecked();
    t.showVp     = m_gamma[1]->isChecked();
    t.showVg     = m_gamma[2]->isChecked();
    t.showAlpha  = m_gamma[3]->isChecked();
    t.showEpsEff = m_gamma[4]->isChecked();
    t.showSmag       = m_spara[0]->isChecked();
    t.showIL         = m_spara[1]->isChecked();
    t.showRL         = m_spara[2]->isChecked();
    t.showDelay      = m_spara[3]->isChecked();
    t.showTouchstone = m_spara[4]->isChecked();
    t.z0FreqDep = m_z0FreqDep->isChecked();
    t.z0ReIm = m_z0ReIm->isChecked();
    t.eyeShow = m_eyeShow->isChecked();
    t.eyeBitRate_Gbps = m_eyeRate->value();
    t.eyePrbsOrder = m_eyePrbs->value();
    t.eyeRise_ps = m_eyeRise->value();
    m_p->touch();
}

void TransmissionLineTab::refresh()
{
    m_updating = true;
    const TransmissionLineOpts &t = m_p->tline();
    m_kind->setCurrentIndex(qBound(0, t.kind, 4));
    m_w->setValue(t.w_mm);
    m_h->setValue(t.h_mm);
    m_slW->setValue(t.w_mm);
    m_slB->setValue(t.h_mm);
    m_cpwS->setValue(t.w_mm);
    m_a->setValue(t.a_mm);
    m_b->setValue(t.b_mm);
    m_d->setValue(t.d_mm);
    m_dia->setValue(t.dia_mm);
    m_slot->setValue(t.slot_mm);
    m_epsr->setValue(t.epsr);
    m_tanD->setValue(t.tanD);
    m_sigma->setValue(t.sigma_Sm / 1.0e6);
    m_length->setValue(t.length_mm);
    m_freq->setValue(t.freq_GHz);
    m_z0Ref->setValue(t.z0Ref_ohm);
    m_ports->setValue(t.ports);
    m_eyeShow->setChecked(t.eyeShow);
    m_eyeRate->setValue(t.eyeBitRate_Gbps);
    m_eyePrbs->setValue(qBound(3, t.eyePrbsOrder, 11));
    m_eyeRise->setValue(t.eyeRise_ps);
    m_gamma[0]->setChecked(t.showBeta);
    m_gamma[1]->setChecked(t.showVp);
    m_gamma[2]->setChecked(t.showVg);
    m_gamma[3]->setChecked(t.showAlpha);
    m_gamma[4]->setChecked(t.showEpsEff);
    m_spara[0]->setChecked(t.showSmag);
    m_spara[1]->setChecked(t.showIL);
    m_spara[2]->setChecked(t.showRL);
    m_spara[3]->setChecked(t.showDelay);
    m_spara[4]->setChecked(t.showTouchstone);
    m_z0FreqDep->setChecked(t.z0FreqDep);
    m_z0ReIm->setChecked(t.z0ReIm);
    m_updating = false;
    recompute();
}

void TransmissionLineTab::onEdited()
{
    if (m_updating) return;
    apply();
    recompute();
}

void TransmissionLineTab::recompute()
{
    const TransmissionLineOpts &t = m_p->tline();
    m_geom->setCurrentIndex(qBound(0, t.kind, 4));

    tline::Line L;
    L.kind = static_cast<tline::Kind>(qBound(0, t.kind, 4));
    L.w_mm = t.w_mm; L.h_mm = t.h_mm; L.a_mm = t.a_mm; L.b_mm = t.b_mm;
    L.d_mm = t.d_mm; L.dia_mm = t.dia_mm; L.slot_mm = t.slot_mm;
    L.epsr = t.epsr; L.tanD = t.tanD; L.sigma_Sm = t.sigma_Sm;
    L.length_mm = t.length_mm;
    const double f = t.freq_GHz * 1.0e9;
    const tline::Result r = tline::analyze(L, f);

    m_s2pBtn->setEnabled(r.valid && t.showTouchstone);
    m_table->clearContents();
    if (!r.valid) {
        m_table->setRowCount(0);
        m_note->setText(I18n::tr("tln_invalid"));
        return;
    }
    m_note->clear();

    const bool exact = (t.kind != 0);
    const QString basisZ = I18n::tr(exact ? "tln_b_exact" : "tln_b_hj");

    struct Row { QString item, value, basis; };
    QVector<Row> rows;
    auto num = [](double v, int d) { return QString::number(v, 'f', d); };

    rows.push_back({ I18n::tr("tln_r_z0"),
                     num(r.z0_ohm, 2) + QStringLiteral(" Ω"), basisZ });
    if (t.showEpsEff)
        rows.push_back({ I18n::tr("tln_g_eeff"), num(r.epsEff, 4), basisZ });
    if (t.z0ReIm)
        rows.push_back({ I18n::tr("tln_r_z0c"),
                         num(r.z0Complex.real(), 3) + QStringLiteral(" ")
                             + (r.z0Complex.imag() < 0 ? QStringLiteral("−")
                                                       : QStringLiteral("+"))
                             + QStringLiteral(" j")
                             + num(std::fabs(r.z0Complex.imag()), 3)
                             + QStringLiteral(" Ω"),
                         I18n::tr("tln_b_line") });
    if (t.z0FreqDep) {
        for (double mul : { 0.1, 10.0 }) {
            const tline::Result q = tline::analyze(L, f * mul);
            if (!q.valid) continue;
            rows.push_back({ I18n::tr("tln_r_z0f").arg(num(t.freq_GHz * mul, 4)),
                             num(q.z0Complex.real(), 3) + QStringLiteral(" Ω"),
                             I18n::tr("tln_b_line") });
        }
    }
    if (t.showBeta)
        rows.push_back({ I18n::tr("tln_g_beta"),
                         num(r.beta_radm, 3) + QStringLiteral(" rad/m"), basisZ });
    if (t.showVp)
        rows.push_back({ I18n::tr("tln_g_vp"),
                         num(r.vp_mps / 1.0e6, 3) + QStringLiteral(" ×10⁶ m/s"),
                         basisZ });
    if (t.showVg)
        // 分散を扱っていないので群速度は位相速度に等しい (その旨を根拠に書く)
        rows.push_back({ I18n::tr("tln_g_vg"),
                         num(r.vp_mps / 1.0e6, 3) + QStringLiteral(" ×10⁶ m/s"),
                         I18n::tr("tln_b_line") });
    if (t.showAlpha)
        rows.push_back({ I18n::tr("tln_g_alpha"),
                         num(r.alpha_dBm, 4) + QStringLiteral(" dB/m"),
                         I18n::tr(r.alphaCApprox ? "tln_b_lossapx"
                                                 : "tln_b_lossexact") });

    const tline::SParam s = tline::sParameters(r, t.length_mm, t.z0Ref_ohm);
    if (s.valid) {
        const double m11 = std::abs(s.s11), m21 = std::abs(s.s21);
        if (t.showSmag) {
            rows.push_back({ I18n::tr("tln_r_s11"),
                             num(m11, 5) + QStringLiteral(" / ")
                                 + num(std::arg(s.s11) * 180.0 / 3.14159265358979,
                                       2) + QStringLiteral("°"),
                             I18n::tr("tln_b_line") });
            rows.push_back({ I18n::tr("tln_r_s21"),
                             num(m21, 5) + QStringLiteral(" / ")
                                 + num(std::arg(s.s21) * 180.0 / 3.14159265358979,
                                       2) + QStringLiteral("°"),
                             I18n::tr("tln_b_line") });
        }
        if (t.showIL)
            rows.push_back({ I18n::tr("tln_r_il"),
                             (m21 > 0.0 ? num(-20.0 * std::log10(m21), 4)
                                        : QStringLiteral("∞"))
                                 + QStringLiteral(" dB"),
                             I18n::tr("tln_b_line") });
        if (t.showRL)
            rows.push_back({ I18n::tr("tln_r_rl"),
                             (m11 > 0.0 ? num(-20.0 * std::log10(m11), 2)
                                        : QStringLiteral("∞"))
                                 + QStringLiteral(" dB"),
                             I18n::tr("tln_b_line") });
    }
    if (t.showDelay) {
        rows.push_back({ I18n::tr("tln_r_delay"),
                         num(r.delay_s * 1.0e12, 2) + QStringLiteral(" ps"),
                         I18n::tr("tln_b_line") });
        rows.push_back({ I18n::tr("tln_r_elec"),
                         num(r.beta_radm * t.length_mm * 1.0e-3 * 180.0
                                 / 3.14159265358979, 2) + QStringLiteral("°"),
                         I18n::tr("tln_b_line") });
    }

    m_table->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        m_table->setItem(i, 0, cell(rows[i].item));
        m_table->setItem(i, 1, cell(rows[i].value));
        m_table->setItem(i, 2, cell(rows[i].basis));
    }

    updateEye();
}

// アイダイアグラム — 線路の S21(f) をそのまま伝達関数に使う。
// **表示している Z₀ / γ と同じ `tline::` の式**から作るので、結果表の数値と
// 図が食い違うことはない。
void TransmissionLineTab::updateEye()
{
    if (!m_eyePlot || !m_eyeNote) return;
    const TransmissionLineOpts &t = m_p->tline();
    const bool on = t.eyeShow;
    m_eyePlot->setVisible(on);
    m_eyeNote->setVisible(on);
    m_eyeRate->setEnabled(on);
    m_eyePrbs->setEnabled(on);
    m_eyeRise->setEnabled(on);
    if (!on) { m_eyePlot->setSeries({}); m_eyeNote->clear(); return; }

    tline::Line L;
    L.kind = static_cast<tline::Kind>(qBound(0, t.kind, 4));
    L.w_mm = t.w_mm; L.h_mm = t.h_mm; L.a_mm = t.a_mm; L.b_mm = t.b_mm;
    L.d_mm = t.d_mm; L.dia_mm = t.dia_mm; L.slot_mm = t.slot_mm;
    L.epsr = t.epsr; L.tanD = t.tanD; L.sigma_Sm = t.sigma_Sm;
    L.length_mm = t.length_mm;
    const double z0Ref = t.z0Ref_ohm;

    eye::Config c;
    c.bitRate_bps = t.eyeBitRate_Gbps * 1.0e9;
    c.prbsOrder = qBound(3, t.eyePrbsOrder, 11);
    c.samplesPerBit = 32;
    c.amplitude_V = 1.0;
    c.riseTime_s = t.eyeRise_ps * 1.0e-12;

    // H(f) = S21(f)。直流は損失が消えるので S21 → 1 (連続な極限)。
    const eye::Transfer H = [&](double f) -> std::complex<double> {
        if (f <= 0.0) return std::complex<double>(1.0, 0.0);
        const tline::Result rr = tline::analyze(L, f);
        if (!rr.valid) return std::complex<double>(0.0, 0.0);
        return tline::sParameters(rr, L.length_mm, z0Ref).s21;
    };
    const eye::Result e = eye::build(c, H);
    if (!e.ok()) { m_eyePlot->setSeries({}); m_eyeNote->clear(); return; }

    QVector<MiniSeries> series;
    series.reserve(int(e.traces.size()));
    double vmax = 0.0;
    for (const std::vector<double> &tr : e.traces) {
        MiniSeries s;
        s.color = QColor(kAccEm);
        s.color.setAlpha(60);          // 重ね描きなので薄く
        for (std::size_t k = 0; k < tr.size(); ++k) {
            s.pts.push_back(QPointF(double(k) * e.dt_s * 1.0e12, tr[k]));
            vmax = std::max(vmax, std::fabs(tr[k]));
        }
        series.push_back(s);
    }
    m_eyePlot->setYRange(-1.05 * vmax, 1.05 * vmax);
    m_eyePlot->setSeries(series);

    const double ui_ps = 1.0e12 / c.bitRate_bps;
    QString note;
    if (e.height_V <= 0.0) {
        note = I18n::tr("tln_eye_shut")
                   .arg(QString::number(e.height_V, 'f', 3));
    } else {
        note = I18n::tr("tln_eye_note")
                   .arg(QString::number(e.height_V, 'f', 3),
                        QString::number(e.width_s * 1.0e12, 'f', 1),
                        QString::number(ui_ps, 'f', 1),
                        QString::number(e.jitter_s * 1.0e12, 'f', 1));
    }
    // 標本化がチャネルを解像できていないときは**必ず断る** (折り返しで
    // 開口が正しく出ないため — 黙って数字だけ出さない)
    if (e.nyquistMag > 0.1)
        note += I18n::tr("tln_eye_alias")
                    .arg(QString::number(e.nyquistMag, 'f', 3));
    m_eyeNote->setText(note);
}


void TransmissionLineTab::exportTouchstone()
{
    const TransmissionLineOpts &t = m_p->tline();
    tline::Line L;
    L.kind = static_cast<tline::Kind>(qBound(0, t.kind, 4));
    L.w_mm = t.w_mm; L.h_mm = t.h_mm; L.a_mm = t.a_mm; L.b_mm = t.b_mm;
    L.d_mm = t.d_mm; L.dia_mm = t.dia_mm; L.slot_mm = t.slot_mm;
    L.epsr = t.epsr; L.tanD = t.tanD; L.sigma_Sm = t.sigma_Sm;
    L.length_mm = t.length_mm;

    const QString path = QFileDialog::getSaveFileName(
        this, I18n::tr("tln_s2p_title"), QStringLiteral("line.s2p"),
        I18n::tr("tln_s2p_filter"));
    if (path.isEmpty()) return;

    // 指定周波数を中心に 1 桁下から 1 桁上まで 201 点 (対数等間隔)
    const int n = 201;
    const double f0 = t.freq_GHz * 1.0e9;
    QVector<double> freq;
    QVector<std::complex<double>> s11, s21;
    for (int i = 0; i < n; ++i) {
        const double fx = f0 * std::pow(10.0, -1.0 + 2.0 * i / (n - 1));
        const tline::Result r = tline::analyze(L, fx);
        const tline::SParam s = tline::sParameters(r, t.length_mm, t.z0Ref_ohm);
        if (!s.valid) continue;
        freq.push_back(fx);
        s11.push_back(s.s11);
        s21.push_back(s.s21);
    }
    if (freq.isEmpty()) {
        QMessageBox::warning(this, I18n::tr("tln_s2p_title"),
                             I18n::tr("tln_invalid"));
        return;
    }
    QString err;
    // 相反・対称な 2 ポートなので S12 = S21, S22 = S11
    if (!Touchstone::writeS2p(path, freq, s11, s21, s21, s11, &err)) {
        QMessageBox::warning(this, I18n::tr("tln_s2p_title"),
                             I18n::tr("tln_s2p_ng").arg(err));
        return;
    }
    QMessageBox::information(
        this, I18n::tr("tln_s2p_title"),
        I18n::tr("tln_s2p_ok").arg(path, QString::number(freq.size()),
                                   QString::number(freq.first() / 1e9, 'g', 4),
                                   QString::number(freq.last() / 1e9, 'g', 4)));
}
