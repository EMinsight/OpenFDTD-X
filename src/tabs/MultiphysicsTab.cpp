// MultiphysicsTab.cpp
#include "MultiphysicsTab.h"
#include "../core/Project.h"
#include "../io/KernelResultReader.h"
#include "../optics/PlasmaDispersion.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"
#include "TabHelpers.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QTableWidget>
#include <QVBoxLayout>

using namespace ofd;

// ── タブ専用語彙 (file-local 登録, 接頭辞 mph_) ─────────────────────────────
namespace {
const bool s_i18n = [] {
    using ofd::I18n;
    I18n::reg("mph_title", "連成解析", "Coupled simulations");
    I18n::reg("mph_hint",
              "FDTDと他物理の連成設定画面。(連成解析は未実装 — 画面は設計モック)\n"
              "※ 表示モジュールは現在のドメイン (%1) で意味のあるもののみ。",
              "Settings page for coupling FDTD with other physics. "
              "(Coupled simulation is not implemented — this page is a design mock)\n"
              "Note: only modules meaningful in the current domain (%1) are listed.");
    I18n::reg("mph_c_module", "モジュール", "Module");
    I18n::reg("mph_c_dir", "方向", "Direction");
    I18n::reg("mph_c_note", "備考", "Note");
    I18n::reg("mph_none", "このドメインに対応する連成モジュールはありません",
              "No coupling module is available for this domain");
    // 連成モジュールの正式名称 (i18n.js の mp_*) — 一覧行のツールチップに使う。
    // 行の表示テキスト自体は mock (ansys-tabs.jsx) の badge + name をそのまま使う
    // ため、ここでは重複表示させない。en は i18n.js に無いので同流儀で補う。
    I18n::reg("mp_charge",  "CHARGE (半導体)",     "CHARGE (semiconductor)");
    I18n::reg("mp_heat",    "HEAT (熱)",           "HEAT (thermal)");
    I18n::reg("mp_stress",  "STRESS (応力)",       "STRESS (mechanical)");
    I18n::reg("mp_circuit", "回路 (INTERCONNECT)", "Circuit (INTERCONNECT)");

    I18n::reg("mph_scheme", "連成方式", "Coupling scheme");
    I18n::reg("mph_weak", "弱連成 (順次)", "Weak (sequential)");
    I18n::reg("mph_strong", "強連成 (反復)", "Strong (iterative)");
    I18n::reg("mph_twoway", "双方向", "Two-way");
    I18n::reg("mph_tol", "反復許容誤差", "Iteration tolerance");
    I18n::reg("mph_maxiter", "最大反復", "Max iterations");

    // 光: 熱光学 / プラズマ
    I18n::reg("mph_thermo", "熱光学連成設定", "Thermo-optic");
    I18n::reg("mph_th_hint",
              "▸ カーネル (ofd) の熱解析レイヤは **入力キーを持たず常に動作**し、"
              "周波数ごとの発熱密度の総和を ofd.log へ書きます。上の設定群は "
              ".ofd に対応キーが無いためカーネルへは渡りません。"
              "値は近傍界 DFT が入射スペクトルで正規化されていないため "
              "**絶対的な W ではなく相対量**です。"
              "CPU 版 (ofd) のみ — ofd_mpi / ofd_cuda には熱解析レイヤがありません。",
              "The kernel's thermal layer takes no input keys and always runs, "
              "writing the integrated dissipation per frequency into ofd.log. "
              "The settings above have no matching .ofd key and are not passed "
              "to the kernel. The values are relative, not absolute watts "
              "(the near-field DFT is not normalised by the incident "
              "spectrum). CPU build only — ofd_mpi / ofd_cuda have no thermal "
              "layer.");
    I18n::reg("mph_th_c_idx", "#", "#");
    I18n::reg("mph_th_c_freq", "周波数 [Hz]", "Frequency [Hz]");
    I18n::reg("mph_th_c_val", "発熱密度の総和 (相対値)",
              "Integrated dissipation (relative)");
    I18n::reg("mph_th_idle",
              "未実行 — 計算を実行すると ofd.log から読み込みます",
              "Not run yet — read from ofd.log after a run");
    I18n::reg("mph_th_none",
              "この実行のログに熱解析の行がありません "
              "(CPU 版 ofd で frequency2 を設定して実行してください)",
              "No thermal lines in this run's log (run the CPU ofd build with "
              "frequency2 set)");
    I18n::reg("mph_th_zero",
              "全周波数で 0 — 損失材料 (σ > 0) が解析領域に無いためです",
              "Zero at every frequency — there is no lossy material "
              "(sigma > 0) in the domain");
    I18n::reg("mph_th_ok", "%1 周波数を読み込みました",
              "Loaded %1 frequencies");
    I18n::reg("mph_heatsrc", "熱源", "Heat sources");
    I18n::reg("mph_absorb", "光吸収", "Optical absorption");
    I18n::reg("mph_joule", "ジュール熱", "Joule heating");
    I18n::reg("mph_tpa", "非線形吸収 (TPA)", "Nonlinear absorption (TPA)");
    I18n::reg("mph_heat_bc", "境界条件 (HEAT)", "Boundary conditions (HEAT)");
    I18n::reg("mph_substrate", "基板放熱", "Substrate heat sink");
    I18n::reg("mph_convection", "自然対流", "Natural convection");
    I18n::reg("mph_ambient", "周囲温度固定", "Fixed ambient temperature");
    I18n::reg("mph_plasma", "プラズマ効果 (Drude)", "Plasma dispersion (Drude)");
    I18n::reg("mph_soref", "Soref-Bennettモデル", "Soref-Bennett model");
    I18n::reg("mph_electron", "電子濃度依存", "Electron density dependence");
    I18n::reg("mph_hole", "正孔濃度依存", "Hole density dependence");
    // プラズマ効果 — 実計算 (src/optics/PlasmaDispersion) 用の語彙
    I18n::reg("mph_pl_model", "モデル", "Model");
    I18n::reg("mph_pl_sb", "Soref-Bennett 実測フィット (c-Si)",
              "Soref-Bennett empirical fit (c-Si)");
    I18n::reg("mph_pl_drude", "Drude (一般材料)", "Drude (general material)");
    I18n::reg("mph_pl_dn", "電子密度 ΔN", "Electron density ΔN");
    I18n::reg("mph_pl_dp", "正孔密度 ΔP", "Hole density ΔP");
    I18n::reg("mph_pl_lambda", "波長 λ", "Wavelength λ");
    I18n::reg("mph_pl_index", "背景屈折率 n", "Background index n");
    I18n::reg("mph_pl_carrier", "考慮するキャリア", "Carriers included");
    I18n::reg("mph_pl_result", "算出値", "Computed");
    I18n::reg("mph_pl_formula", "使用式", "Formula");
    I18n::reg("mph_pl_f_sb",
              "Δn = −[a·ΔN + b·ΔP^0.8],  Δα = c·ΔN + d·ΔP  "
              "(Soref & Bennett, IEEE JQE-23, 123 (1987))",
              "Δn = −[a·ΔN + b·ΔP^0.8],  Δα = c·ΔN + d·ΔP  "
              "(Soref & Bennett, IEEE JQE-23, 123 (1987))");
    I18n::reg("mph_pl_f_drude",
              "Δn = −ω_p²/(2nω²),  ω_p² = ΔN·e²/(ε₀m*)  (Drude)",
              "Δn = −ω_p²/(2nω²),  ω_p² = ΔN·e²/(ε₀m*)  (Drude)");
    I18n::reg("mph_pl_note",
              "▸ 上記は入力値に対する材料モデルの評価値 (実計算) です。"
              "FDTD ↔ CHARGE の連成計算の結果ではありません。",
              "▸ The values above are this material model evaluated for the "
              "inputs (a real calculation) — not the result of an FDTD ↔ CHARGE "
              "co-simulation.");
    I18n::reg("mph_pl_scope",
              "▸ 算出した Δn・Δα はカーネル入力へは渡していません (連成は未実装)。",
              "▸ The computed Δn / Δα are not passed to the solver kernel "
              "(coupling is not implemented).");
    I18n::reg("mph_pl_extrap",
              "⚠ λ が実測フィットの帯 (1.31 / 1.55 μm ±5 %) の外です — 外挿値",
              "⚠ λ is outside the fitted bands (1.31 / 1.55 μm ±5 %) — "
              "extrapolated");
    I18n::reg("mph_pl_drude_note",
              "※ Drude の Δα は直流移動度を使うため実測より小さく出ます "
              "(Si・1.3〜1.55 μm で約 1/20)。Si では実測フィットを使ってください。",
              "Note: the Drude Δα uses the DC mobility and therefore "
              "under-predicts the measured value (about 1/20 for Si at "
              "1.3-1.55 μm). Prefer the empirical fit for silicon.");
    I18n::reg("mph_pl_bad", "⚠ 入力が不正です (λ > 0, n > 0, ΔN・ΔP ≥ 0)",
              "⚠ Invalid input (λ > 0, n > 0, ΔN, ΔP ≥ 0)");

    // EM: SAR / Bioheat
    I18n::reg("mph_sar", "SAR/Bioheat 連成", "SAR → Temperature");
    I18n::reg("mph_tissue", "組織モデル", "Tissue model");
    I18n::reg("mph_vhp_m", "VHP (Visible Human Project) 男性",
              "VHP (Visible Human Project) male");
    I18n::reg("mph_vhp_f", "VHP 女性", "VHP female");
    I18n::reg("mph_nifti", "カスタム NIfTI…", "Custom NIfTI…");
    I18n::reg("mph_perfusion", "血液灌流", "Blood perfusion");
    I18n::reg("mph_perfusion_model", "温度依存灌流モデル",
              "Temperature-dependent perfusion model");
    I18n::reg("mph_metric", "評価指標", "Metrics");
    I18n::reg("mph_sar10g", "局所SAR (10g)", "Local SAR (10 g)");
    I18n::reg("mph_sar_body", "全身SAR", "Whole-body SAR");
    I18n::reg("mph_dtemp", "温度上昇", "Temperature rise");

    // 音響: 振動音響
    I18n::reg("mph_vibro", "振動音響連成", "Vibro-acoustic");
    I18n::reg("mph_excite", "励振源", "Excitation");
    I18n::reg("mph_harm", "調和", "Harmonic");
    I18n::reg("mph_impulse", "インパルス", "Impulse");
    I18n::reg("mph_random", "ランダム", "Random");
    I18n::reg("mph_modal", "モード重畳法", "Modal superposition");
    I18n::reg("mph_frf", "周波数応答 H(f)", "Frequency response H(f)");

    // 水中: 海洋環境
    I18n::reg("mph_ocean", "海洋環境連成", "Ocean coupling");
    I18n::reg("mph_temp_data", "水温データ", "Water temperature data");
    I18n::reg("mph_woa13", "WOA13 (World Ocean Atlas)", "WOA13 (World Ocean Atlas)");
    I18n::reg("mph_hycom", "HYCOM リアルタイム", "HYCOM real-time");
    I18n::reg("mph_netcdf", "カスタム NetCDF…", "Custom NetCDF…");
    I18n::reg("mph_salinity", "塩分プロファイル", "Salinity profile");
    I18n::reg("mph_current", "流れ場 (ドップラー)", "Current field (Doppler)");
    I18n::reg("mph_wave", "海面波", "Sea surface");
    I18n::reg("mph_flat", "鏡面", "Flat (specular)");
    I18n::reg("mph_uw_all", "このタブの設定すべて (.ofd に対応キーが無く、カーネルへ渡せません)",
              "every setting on this tab (there is no corresponding .ofd key, so nothing can be handed to the kernel)");
    I18n::reg("mph_uw_scheme", "連成スキームの選択と収束条件 (許容誤差・最大反復)",
              "the coupling scheme and the convergence settings (tolerance, maximum iterations)");
    I18n::reg("mph_uw_thermal", "熱解析の設定",
              "the thermal-analysis settings");
    I18n::reg("mph_uw_sar", "生体組織モデルと灌流の設定",
              "the tissue model and perfusion settings");
    I18n::reg("mph_uw_vibro", "振動音響の励振・モーダル解析の設定",
              "the vibro-acoustic excitation and modal-analysis settings");
    I18n::reg("mph_uw_ocean", "海洋環境データの選択 (水温・塩分・流れ)",
              "the ocean-environment data selection (temperature, salinity, currents)");
    return true;
}();

// ドメインビット
enum : unsigned { EM = 1, OPT = 2, AC = 4, UW = 8 };

unsigned domainBit(ofd::Domain d)
{
    switch (d) {
        case ofd::Domain::Optical:    return OPT;
        case ofd::Domain::Acoustic:   return AC;
        case ofd::Domain::Underwater: return UW;
        default:                      return EM;
    }
}

// mock の modules[] をそのまま転記 (labelKey = i18n.js の正式名称キー / 無しは nullptr)
struct ModuleDef {
    bool ck;
    const char *badge, *name, *coup, *note;
    unsigned domains;
    const char *labelKey;
};
const ModuleDef kModules[9] = {
    { true,  "CHARGE",  "半導体電子輸送", "FDTD ↔ CHARGE",
      "電流→屈折率変調 (プラズマ効果)", OPT,      "mp_charge"  },
    { true,  "HEAT",    "熱伝導",         "FDTD → HEAT",
      "光吸収→温度上昇→熱光学",         OPT | EM, "mp_heat"    },
    { false, "STRESS",  "応力解析",       "STRESS → FDTD",
      "応力光学効果",                   OPT,      "mp_stress"  },
    { false, "RF",      "回路",           "FDTD ↔ Circuit",
      "SPICE/INTERCONNECT 連携",        EM | OPT, "mp_circuit" },
    { false, "CFD",     "流体",           "CFD → 音響",
      "流体騒音 (Aeroacoustics)",       AC,       nullptr      },
    { false, "BIOHEAT", "生体熱輸送",     "FDTD → BioHeat",
      "SAR→温度上昇 (Pennes方程式)",    EM,       nullptr      },
    { false, "VIBRO",   "構造振動",       "VIBRO ↔ Acoustic",
      "振動音響 (車体/楽器)",           AC | UW,  nullptr      },
    { false, "FSI",     "流体構造連成",   "FSI ↔ Acoustic",
      "波浪・水中構造振動",             UW,       nullptr      },
    { false, "OCEAN",   "海洋環境",       "OCEAN → Acoustic",
      "水温/塩分/流れ→音速分布",        UW,       nullptr      },
};

QLabel *hintLabel(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setWordWrap(true);
    return l;
}

QLineEdit *numEdit(const char *value, int width, QWidget *parent)
{
    auto *e = new QLineEdit(QString::fromUtf8(value), parent);
    if (width > 0) e->setMaximumWidth(width);
    return e;
}

QCheckBox *check(const QString &text, bool on, QWidget *parent)
{
    auto *c = new QCheckBox(text, parent);
    c->setChecked(on);
    return c;
}

// 表セル内のモジュールバッジ + 名前
QWidget *moduleCell(const QString &badge, const QString &name, bool acc)
{
    auto *w = new QWidget;
    auto *h = new QHBoxLayout(w);
    h->setContentsMargins(4, 2, 4, 2);
    h->setSpacing(6);
    auto *b = new QLabel(badge, w);
    QString css = "border-radius:3px; padding:1px 6px; font-size:11px;";
    css += acc ? "background:#DEECF9; color:#0078D4;" : "background:palette(midlight);";
    b->setStyleSheet(css);
    h->addWidget(b);
    h->addWidget(new QLabel(name, w));
    h->addStretch(1);
    return w;
}
} // namespace

// ── MultiphysicsTab ─────────────────────────────────────────────────────────
MultiphysicsTab::MultiphysicsTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // ── 連成解析 / Coupled simulations ─────────────────────────────────────
    auto *sTop = new SectionBox(I18n::tr("mph_title"), body);
    m_hint = hintLabel(QString(), sTop);
    sTop->vbox()->addWidget(m_hint);
    // タブ全体が設計モック — どの設定も計算へ反映されない
    sTop->vbox()->addWidget(tabhelp::unwiredNote(sTop, I18n::tr("mph_uw_all")));

    m_modules = new QTableWidget(0, 4, sTop);
    m_modules->setHorizontalHeaderLabels({ QString(), I18n::tr("mph_c_module"),
                                           I18n::tr("mph_c_dir"),
                                           I18n::tr("mph_c_note") });
    m_modules->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_modules->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_modules->horizontalHeader()->resizeSection(0, 24);
    m_modules->verticalHeader()->setVisible(false);
    m_modules->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_modules->setMinimumHeight(180);
    sTop->vbox()->addWidget(m_modules);
    v->addWidget(sTop);

    // ── 連成方式 / Coupling scheme ─────────────────────────────────────────
    auto *sScheme = new SectionBox(I18n::tr("mph_scheme"), body);
    m_scheme = new QComboBox(sScheme);
    m_scheme->addItem(I18n::tr("mph_weak"));
    m_scheme->addItem(I18n::tr("mph_strong"));
    m_scheme->addItem(I18n::tr("mph_twoway"));
    sScheme->vbox()->addWidget(m_scheme);
    m_tol = numEdit("1.0e-4", 100, sScheme);
    sScheme->form()->addRow(I18n::tr("mph_tol"), m_tol);
    m_maxIter = numEdit("20", 70, sScheme);
    sScheme->form()->addRow(I18n::tr("mph_maxiter"), m_maxIter);
    sScheme->form()->addRow(tabhelp::unwiredNote(sScheme, I18n::tr("mph_uw_scheme")));
    v->addWidget(sScheme);

    // ── 光: 熱光学連成設定 ─────────────────────────────────────────────────
    m_secThermo = new SectionBox(I18n::tr("mph_thermo"), body);
    {
        auto *dnRow = new QHBoxLayout();
        dnRow->addWidget(numEdit("1.86e-4", 90, m_secThermo));
        dnRow->addWidget(new QLabel("/K (Si)", m_secThermo));
        dnRow->addStretch(1);
        m_secThermo->form()->addRow("dn/dT", dnRow);

        auto *srcRow = new QHBoxLayout();
        srcRow->addWidget(check(I18n::tr("mph_absorb"), true, m_secThermo));
        srcRow->addWidget(check(I18n::tr("mph_joule"), false, m_secThermo));
        srcRow->addWidget(check(I18n::tr("mph_tpa"), false, m_secThermo));
        srcRow->addStretch(1);
        m_secThermo->form()->addRow(I18n::tr("mph_heatsrc"), srcRow);

        auto *bcRow = new QHBoxLayout();
        bcRow->addWidget(check(I18n::tr("mph_substrate"), true, m_secThermo));
        bcRow->addWidget(check(I18n::tr("mph_convection"), false, m_secThermo));
        bcRow->addWidget(check(I18n::tr("mph_ambient"), true, m_secThermo));
        bcRow->addStretch(1);
        m_secThermo->form()->addRow(I18n::tr("mph_heat_bc"), bcRow);
        // 上の設定群は .ofd に対応キーが無く、カーネルへ渡せない
        m_secThermo->form()->addRow(tabhelp::unwiredNote(m_secThermo, I18n::tr("mph_uw_thermal")));

        // ── カーネルの熱解析レイヤ (実測値) ──────────────────────────────
        // ofd は入力キー無しで常に発熱密度を積算し、周波数ごとに
        //   Thermal: dissipated[i] = <値> (f=<周波数> Hz)
        // を ofd.log へ書く。GUI はこれまでこれを読んでいなかった。
        auto *thHint = new QLabel(I18n::tr("mph_th_hint"), m_secThermo);
        thHint->setWordWrap(true);
        thHint->setStyleSheet("background:#DEECF9; color:#204E7A; "
                              "border-radius:3px; padding:4px 8px; "
                              "font-size:11px;");
        m_secThermo->vbox()->addWidget(thHint);

        m_thermalTbl = new QTableWidget(0, 3, m_secThermo);
        m_thermalTbl->setHorizontalHeaderLabels({ I18n::tr("mph_th_c_idx"),
            I18n::tr("mph_th_c_freq"), I18n::tr("mph_th_c_val") });
        m_thermalTbl->horizontalHeader()->setSectionResizeMode(
            QHeaderView::Stretch);
        m_thermalTbl->verticalHeader()->setVisible(false);
        m_thermalTbl->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_thermalTbl->setMinimumHeight(110);
        m_secThermo->vbox()->addWidget(m_thermalTbl);

        m_thermalStatus = new QLabel(I18n::tr("mph_th_idle"), m_secThermo);
        m_thermalStatus->setWordWrap(true);
        m_thermalStatus->setStyleSheet("font-size:11px; color:palette(mid);");
        m_secThermo->vbox()->addWidget(m_thermalStatus);
    }
    v->addWidget(m_secThermo);

    // ── 光: プラズマ効果 (Drude / Soref-Bennett) ───────────────────────────
    // 固定表示だった Δn の式を、入力値を代入した実計算に置き換える
    // (計算実体は src/optics/PlasmaDispersion — selftest で解析解と照合)。
    m_secPlasma = new SectionBox(I18n::tr("mph_plasma"), body);
    {
        m_plModel = new QComboBox(m_secPlasma);
        m_plModel->addItem(I18n::tr("mph_pl_sb"));       // 0 = Soref-Bennett
        m_plModel->addItem(I18n::tr("mph_pl_drude"));    // 1 = Drude
        m_secPlasma->form()->addRow(I18n::tr("mph_pl_model"), m_plModel);

        m_plDeltaN = numEdit("1e18", 110, m_secPlasma);
        auto *nRow = new QHBoxLayout();
        nRow->addWidget(m_plDeltaN);
        nRow->addWidget(new QLabel(QString::fromUtf8("cm⁻³"), m_secPlasma));
        nRow->addStretch(1);
        m_secPlasma->form()->addRow(I18n::tr("mph_pl_dn"), nRow);

        m_plDeltaP = numEdit("1e18", 110, m_secPlasma);
        auto *pRow = new QHBoxLayout();
        pRow->addWidget(m_plDeltaP);
        pRow->addWidget(new QLabel(QString::fromUtf8("cm⁻³"), m_secPlasma));
        pRow->addStretch(1);
        m_secPlasma->form()->addRow(I18n::tr("mph_pl_dp"), pRow);

        // λ の既定はプロジェクトの光波長帯の中心 (refreshPlasma で読み直す)
        m_plLambda = numEdit("1550", 90, m_secPlasma);
        auto *lRow = new QHBoxLayout();
        lRow->addWidget(m_plLambda);
        lRow->addWidget(new QLabel("nm", m_secPlasma));
        lRow->addStretch(1);
        m_secPlasma->form()->addRow(I18n::tr("mph_pl_lambda"), lRow);

        m_plIndex = numEdit("3.48", 90, m_secPlasma);
        m_secPlasma->form()->addRow(I18n::tr("mph_pl_index"), m_plIndex);

        m_plElectron = check(I18n::tr("mph_electron"), true, m_secPlasma);
        m_plHole     = check(I18n::tr("mph_hole"), true, m_secPlasma);
        auto *row = new QHBoxLayout();
        row->addWidget(m_plElectron);
        row->addWidget(m_plHole);
        row->addStretch(1);
        m_secPlasma->form()->addRow(I18n::tr("mph_pl_carrier"), row);

        m_plResult = new QLabel(m_secPlasma);
        m_plResult->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_secPlasma->form()->addRow(I18n::tr("mph_pl_result"), m_plResult);

        m_plFormula = hintLabel(QString(), m_secPlasma);
        m_secPlasma->form()->addRow(I18n::tr("mph_pl_formula"), m_plFormula);

        m_plWarn = hintLabel(QString(), m_secPlasma);
        m_plWarn->setStyleSheet("color:#B45309;");
        m_secPlasma->form()->addRow(m_plWarn);

        auto *note = hintLabel(I18n::tr("mph_pl_note"), m_secPlasma);
        note->setStyleSheet("font-size:11px; color:palette(mid);");
        m_secPlasma->form()->addRow(note);
        auto *scope = hintLabel(I18n::tr("mph_pl_scope"), m_secPlasma);
        scope->setStyleSheet("font-size:11px; color:palette(mid);");
        m_secPlasma->form()->addRow(scope);

        for (QLineEdit *e : { m_plDeltaN, m_plDeltaP, m_plLambda, m_plIndex })
            connect(e, &QLineEdit::textChanged, this,
                    &MultiphysicsTab::updatePlasma);
        connect(m_plModel, &QComboBox::currentIndexChanged, this,
                [this](int) { updatePlasma(); });
        for (QCheckBox *c : { m_plElectron, m_plHole })
            connect(c, &QCheckBox::toggled, this,
                    [this](bool) { updatePlasma(); });
    }
    v->addWidget(m_secPlasma);

    // ── EM: SAR/Bioheat 連成 ───────────────────────────────────────────────
    m_secSar = new SectionBox(I18n::tr("mph_sar"), body);
    {
        auto *tissue = new QComboBox(m_secSar);
        tissue->addItem(I18n::tr("mph_vhp_m"));
        tissue->addItem(I18n::tr("mph_vhp_f"));
        tissue->addItem("Duke (IT'IS)");
        tissue->addItem("Ella (IT'IS)");
        tissue->addItem(I18n::tr("mph_nifti"));
        m_secSar->form()->addRow(I18n::tr("mph_tissue"), tissue);
        m_secSar->form()->addRow(I18n::tr("mph_perfusion"),
            check(I18n::tr("mph_perfusion_model"), true, m_secSar));
        auto *metRow = new QHBoxLayout();
        metRow->addWidget(check(I18n::tr("mph_sar10g"), true, m_secSar));
        metRow->addWidget(check(I18n::tr("mph_sar_body"), false, m_secSar));
        metRow->addWidget(check(I18n::tr("mph_dtemp"), true, m_secSar));
        metRow->addStretch(1);
        m_secSar->form()->addRow(I18n::tr("mph_metric"), metRow);
        m_secSar->form()->addRow(tabhelp::unwiredNote(m_secSar, I18n::tr("mph_uw_sar")));
    }
    v->addWidget(m_secSar);

    // ── 音響: 振動音響連成 ─────────────────────────────────────────────────
    m_secVibro = new SectionBox(I18n::tr("mph_vibro"), body);
    {
        auto *ex = new QComboBox(m_secVibro);
        ex->addItem(I18n::tr("mph_harm"));
        ex->addItem(I18n::tr("mph_impulse"));
        ex->addItem(I18n::tr("mph_random"));
        m_secVibro->form()->addRow(I18n::tr("mph_excite"), ex);
        auto *row = new QHBoxLayout();
        row->addWidget(check(I18n::tr("mph_modal"), true, m_secVibro));
        row->addWidget(check(I18n::tr("mph_frf"), true, m_secVibro));
        row->addStretch(1);
        m_secVibro->form()->addRow(row);
        m_secVibro->form()->addRow(tabhelp::unwiredNote(m_secVibro, I18n::tr("mph_uw_vibro")));
    }
    v->addWidget(m_secVibro);

    // ── 水中: 海洋環境連成 ─────────────────────────────────────────────────
    m_secOcean = new SectionBox(I18n::tr("mph_ocean"), body);
    {
        auto *td = new QComboBox(m_secOcean);
        td->addItem(I18n::tr("mph_woa13"));
        td->addItem(I18n::tr("mph_hycom"));
        td->addItem(I18n::tr("mph_netcdf"));
        m_secOcean->form()->addRow(I18n::tr("mph_temp_data"), td);
        auto *row = new QHBoxLayout();
        row->addWidget(check(I18n::tr("mph_salinity"), true, m_secOcean));
        row->addWidget(check(I18n::tr("mph_current"), false, m_secOcean));
        row->addStretch(1);
        m_secOcean->form()->addRow(row);
        auto *wave = new QComboBox(m_secOcean);
        wave->addItem(I18n::tr("mph_flat"));
        wave->addItem("Pierson-Moskowitz");
        wave->addItem("JONSWAP");
        wave->setCurrentIndex(1);            // 既定 "pier"
        m_secOcean->form()->addRow(I18n::tr("mph_wave"), wave);
        m_secOcean->form()->addRow(tabhelp::unwiredNote(m_secOcean, I18n::tr("mph_uw_ocean")));
    }
    v->addWidget(m_secOcean);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    connect(project, &Project::domainChanged, this,
            &MultiphysicsTab::rebuildDomain);
    connect(project, &Project::loaded, this, &MultiphysicsTab::refreshPlasma);
    rebuildDomain();
    refreshPlasma();
}

// ── プラズマ効果: プロジェクトの光波長を既定値として読み直す ────────────────
void MultiphysicsTab::refreshPlasma()
{
    m_updating = true;
    const OpticalOpts &o = m_p->optical();
    const double lambda = 0.5 * (o.lambdaMin + o.lambdaMax);   // nm
    if (lambda > 0.0)
        m_plLambda->setText(QString::number(lambda, 'g', 6));
    m_updating = false;
    updatePlasma();
}

// ── プラズマ効果: 入力値 → Δn / Δα (実計算) ────────────────────────────────
void MultiphysicsTab::updatePlasma()
{
    if (m_updating) return;

    bool okN = false, okP = false, okL = false, okI = false;
    const double dN = m_plDeltaN->text().trimmed().toDouble(&okN);
    const double dP = m_plDeltaP->text().trimmed().toDouble(&okP);
    const double lambda = m_plLambda->text().trimmed().toDouble(&okL);
    const double nbg = m_plIndex->text().trimmed().toDouble(&okI);

    // チェックの入っていないキャリアは寄与ゼロとして扱う (設定が結果に効く)
    const double useN = m_plElectron->isChecked() ? dN : 0.0;
    const double useP = m_plHole->isChecked() ? dP : 0.0;

    const bool drude = (m_plModel->currentIndex() == 1);
    m_plFormula->setText(I18n::tr(drude ? "mph_pl_f_drude" : "mph_pl_f_sb"));

    if (!okN || !okP || !okL || !okI || lambda <= 0.0 || nbg <= 0.0
        || useN < 0.0 || useP < 0.0) {
        m_plResult->setText(QString::fromUtf8("—"));
        m_plWarn->setText(I18n::tr("mph_pl_bad"));
        return;
    }

    optics::PlasmaResult r;
    QString warn;
    if (drude) {
        optics::CarrierState cs;
        cs.deltaN_cm3 = useN;
        cs.deltaP_cm3 = useP;
        r = optics::drudeFreeCarrier(lambda, nbg, cs);
        warn = I18n::tr("mph_pl_drude_note");
    } else {
        r = optics::sorefBennettSilicon(lambda, useN, useP);
        if (!optics::sorefBennettApplicable(lambda))
            warn = I18n::tr("mph_pl_extrap");
    }

    if (!r.valid) {
        m_plResult->setText(QString::fromUtf8("—"));
        m_plWarn->setText(I18n::tr("mph_pl_bad"));
        return;
    }
    m_plResult->setText(
        QString::fromUtf8("Δn = %1    Δα = %2 cm⁻¹ (%3 dB/cm)")
            .arg(QString::number(r.deltaN_index, 'e', 3))
            .arg(QString::number(r.deltaAlpha_per_cm, 'e', 3))
            .arg(QString::number(r.deltaAlpha_dB_per_cm, 'f', 2)));
    m_plWarn->setText(warn);
}

void MultiphysicsTab::rebuildDomain()
{
    const Domain d = m_p->activeDomain();
    const unsigned bit = domainBit(d);
    m_hint->setText(I18n::tr("mph_hint").arg(domainKey(d).toUpper()));

    // ── モジュール一覧 (ドメインフィルタ) ──────────────────────────────────
    m_modules->clearSpans();
    m_modules->setRowCount(0);           // 旧行のセルウィジェットも破棄される
    int n = 0;
    for (const ModuleDef &m : kModules)
        if (m.domains & bit) ++n;
    m_modules->setRowCount(n > 0 ? n : 1);

    if (n == 0) {
        // mock: 「このドメインに対応する連成モジュールはありません」
        auto *none = new QTableWidgetItem(I18n::tr("mph_none"));
        QFont f = none->font();
        f.setItalic(true);
        none->setFont(f);
        m_modules->setItem(0, 0, none);
        m_modules->setSpan(0, 0, 1, 4);
    } else {
        int r = 0;
        for (const ModuleDef &m : kModules) {
            if (!(m.domains & bit)) continue;
            auto *ck = new QTableWidgetItem;
            ck->setCheckState(m.ck ? Qt::Checked : Qt::Unchecked);
            ck->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
            m_modules->setItem(r, 0, ck);
            QWidget *cell = moduleCell(QString::fromUtf8(m.badge),
                                       QString::fromUtf8(m.name), m.ck);
            // 正式名称 (mp_charge / mp_heat / mp_stress / mp_circuit) を行のヒントに
            if (m.labelKey) {
                const QString label = I18n::tr(m.labelKey);
                cell->setToolTip(label);
                ck->setToolTip(label);
            }
            m_modules->setCellWidget(r, 1, cell);
            m_modules->setItem(r, 2, new QTableWidgetItem(QString::fromUtf8(m.coup)));
            m_modules->setItem(r, 3, new QTableWidgetItem(QString::fromUtf8(m.note)));
            ++r;
        }
    }

    // ── ドメイン別詳細セクションの表示切替 ─────────────────────────────────
    m_secThermo->setVisible(d == Domain::Optical);
    m_secPlasma->setVisible(d == Domain::Optical);
    m_secSar->setVisible(d == Domain::EM);
    m_secVibro->setVisible(d == Domain::Acoustic);
    m_secOcean->setVisible(d == Domain::Underwater);
}

// ── カーネルの熱解析レイヤの読み込み ────────────────────────────────────────
// 値は絶対的な W ではなく相対量 (近傍界 DFT が入射スペクトルで正規化されて
// いないため — カーネル README)。単位を付けずに「相対値」と明示して出す
// (校正なしの絶対値を出さない — 絶対規則 6 と同じ考え方)。
void MultiphysicsTab::loadThermalFrom(const QString &logPath)
{
    if (!m_thermalTbl) return;
    const QVector<ThermalPoint> pts =
        KernelResultReader::readThermal(logPath);
    m_thermalTbl->setRowCount(pts.size());
    for (int r = 0; r < pts.size(); ++r) {
        m_thermalTbl->setItem(r, 0, tabhelp::roItem(
            QString::number(pts[r].index)));
        m_thermalTbl->setItem(r, 1, tabhelp::roItem(
            QStringLiteral("%1").arg(pts[r].freqHz, 0, 'g', 6)));
        m_thermalTbl->setItem(r, 2, tabhelp::roItem(
            QStringLiteral("%1").arg(pts[r].dissipated, 0, 'e', 6)));
    }
    if (pts.isEmpty()) {
        m_thermalStatus->setText(I18n::tr("mph_th_none"));
        return;
    }
    // 全て 0 なら「損失材料が無い」— 数字だけ見せて放置しない
    bool allZero = true;
    for (const ThermalPoint &p : pts) if (p.dissipated != 0.0) allZero = false;
    m_thermalStatus->setText(allZero ? I18n::tr("mph_th_zero")
                                     : I18n::tr("mph_th_ok").arg(pts.size()));
}

void MultiphysicsTab::clearThermal()
{
    if (!m_thermalTbl) return;
    m_thermalTbl->setRowCount(0);
    m_thermalStatus->setText(I18n::tr("mph_th_idle"));
}
