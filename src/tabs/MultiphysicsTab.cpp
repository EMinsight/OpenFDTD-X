// MultiphysicsTab.cpp
#include "MultiphysicsTab.h"
#include "../core/Project.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"

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
              "FDTDと他物理を連成。Ansys CHARGE/HEAT, COMSOL Multiphysics 相当。\n"
              "※ 表示モジュールは現在のドメイン (%1) で意味のあるもののみ。",
              "Couples FDTD with other physics. Equivalent to Ansys CHARGE/HEAT and "
              "COMSOL Multiphysics.\n"
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
    }
    v->addWidget(m_secThermo);

    // ── 光: プラズマ効果 (Drude) ───────────────────────────────────────────
    m_secPlasma = new SectionBox(I18n::tr("mph_plasma"), body);
    {
        m_secPlasma->form()->addRow(QString::fromUtf8("Δn ~ -8.8e-22 × ΔN"),
                                    new QLabel(I18n::tr("mph_soref"), m_secPlasma));
        auto *row = new QHBoxLayout();
        row->addWidget(check(I18n::tr("mph_electron"), true, m_secPlasma));
        row->addWidget(check(I18n::tr("mph_hole"), true, m_secPlasma));
        row->addStretch(1);
        m_secPlasma->form()->addRow(row);
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
    }
    v->addWidget(m_secOcean);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    connect(project, &Project::domainChanged, this,
            &MultiphysicsTab::rebuildDomain);
    rebuildDomain();
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
