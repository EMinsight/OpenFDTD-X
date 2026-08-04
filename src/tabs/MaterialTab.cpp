// MaterialTab.cpp
#include "MaterialTab.h"
#include "../core/Project.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QStandardItemModel>
#include <QTableWidget>
#include <QVBoxLayout>

#include <cmath>

using namespace ofd;

// mock (tabs.jsx MaterialTab) にしか無かった文言。接頭辞は本タブ専用の ma_。
namespace {
const bool s_i18nMaterial = [] {
    I18n::reg("ma_opt_suffix", " (光学定数)", " (optical constants)");
    I18n::reg("ma_ac_suffix", " (音響パラメータ)", " (acoustic parameters)");
    I18n::reg("ma_rho", "ρ [kg/m³]", "ρ [kg/m³]");
    I18n::reg("ma_c_sound", "c [m/s]", "c [m/s]");
    I18n::reg("ma_absorption", "吸音率α", "Absorption α");
    I18n::reg("ma_impedance", "音響インピーダンス Z", "Acoustic Z");
    I18n::reg("ma_disp_hint",
              "分散モデル: Drude / Lorentz / Sellmeier — 種別を「分散性」にすると "
              "ε∞ / a / b / c の詳細設定になります。",
              "Dispersion models: Drude / Lorentz / Sellmeier — switch the type to "
              "'Dispersive' to edit ε∞ / a / b / c.");
    I18n::reg("ma_lib", "ライブラリ読込", "Load library");
    I18n::reg("ma_lib_std", "標準ライブラリ:", "Standard libraries:");
    I18n::reg("ma_lib_ri", "RefractiveIndex.info (光学定数データベース)",
              "RefractiveIndex.info (optical constants database)");
    I18n::reg("ma_lib_nist", "NIST 標準物性値", "NIST reference material data");
    I18n::reg("ma_lib_astm", "ASTM 音響材料データ", "ASTM acoustic material data");
    I18n::reg("ma_lib_ofd", "OpenFDTD 標準物性値ライブラリ",
              "OpenFDTD standard material library");
    I18n::reg("ma_lib_load", "読込", "Load");
    I18n::reg("ma_lib_todo",
              "%1: オンラインライブラリの直接取込は未対応です。"
              "ガラスカタログ / 物性値エクスプローラの取込機能をご利用ください。",
              "%1: direct online library import is not available yet. Use the glass "
              "catalog / material explorer import instead.");
    // 光ドメインの列見出し (mock: mat_n / mat_k)
    I18n::reg("ma_n", "n", "n");
    I18n::reg("ma_k", "k (消衰係数)", "k (ext. coef.)");
    I18n::reg("ma_nk_tip",
              "n / k は光解析の中心波長 %1 nm で .ofd の εr / σ に換算して "
              "保存します (εr = n² − k², σ = 2nkωε₀)。",
              "n / k are converted to the .ofd εr / σ at the optical centre "
              "wavelength %1 nm (εr = n² − k², σ = 2nkωε₀).");
    return true;
}();

// ── 光ドメインの n / k ↔ .ofd の εr / σ 換算 ────────────────────────────────
// 複素誘電率 εc = (n − jk)² = εr − jσ/(ωε₀) より
//   εr = n² − k²,  σ = 2nk·ω·ε₀        (ω = 2πc/λc)
// λc は光解析の中心波長 (OpticalOpts::lambdaMin/Max [nm])。
// 逆変換は B = σ/(ωε₀) として n = √((εr + √(εr²+B²))/2), k = B/(2n)。
const double kPi   = 3.14159265358979323846;
const double kC0   = 2.99792458e8;      // [m/s]
const double kEps0 = 8.8541878128e-12;  // [F/m]

double centerLambdaNm(const OpticalOpts &o)
{
    const double lam = 0.5 * (o.lambdaMin + o.lambdaMax);
    return (lam > 0) ? lam : 0.0;
}

double omegaOf(const OpticalOpts &o)
{
    const double lam = centerLambdaNm(o);
    return (lam > 0) ? (2.0 * kPi * kC0 / (lam * 1e-9)) : 0.0;
}

void epsSgmToNk(double epsr, double esgm, double omega, double &n, double &k)
{
    const double b = (omega > 0) ? esgm / (omega * kEps0) : 0.0;
    const double r = std::sqrt(epsr * epsr + b * b);
    n = std::sqrt(qMax(0.0, 0.5 * (epsr + r)));
    k = (n > 0) ? b / (2.0 * n) : 0.0;
}

void nkToEpsSgm(double n, double k, double omega, double &epsr, double &esgm)
{
    epsr = n * n - k * k;
    esgm = (omega > 0) ? 2.0 * n * k * omega * kEps0 : 0.0;
}
} // namespace

MaterialTab::MaterialTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // materials
    auto *sm = new SectionBox(I18n::tr("ma_section"), body);
    m_matSection = sm;
    sm->vbox()->addWidget(new QLabel(I18n::tr("ma_builtin"), sm));

    m_mats = new QTableWidget(0, 7, sm);
    m_mats->setHorizontalHeaderLabels({
        I18n::tr("ma_type"),
        "εr / ε∞", "σ / a", "μr / b", "σm / c",
        I18n::tr("ma_name"), I18n::tr("ma_id") });
    m_mats->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_mats->horizontalHeader()->setStretchLastSection(true);
    m_mats->verticalHeader()->setDefaultSectionSize(24);
    m_mats->setMinimumHeight(160);
    sm->vbox()->addWidget(m_mats);

    auto *mrow = new QHBoxLayout();
    auto *madd = new QPushButton(I18n::tr("ma_add"), sm);
    auto *mdel = new QPushButton(I18n::tr("ma_del"), sm);
    mrow->addWidget(madd);
    mrow->addWidget(mdel);
    mrow->addStretch(1);
    sm->vbox()->addLayout(mrow);

    // 分散モデルの案内 (mock: 光ドメインのみ表示)
    m_dispHint = new QLabel(I18n::tr("ma_disp_hint"), sm);
    m_dispHint->setWordWrap(true);
    sm->vbox()->addWidget(m_dispHint);
    v->addWidget(sm);

    // lumped elements (.ofd の load キー — EM FDTD 専用。updateColumns で出し分け)
    auto *sl = new SectionBox(I18n::tr("ma_lumped"), body);
    m_lumpedSection = sl;
    m_loads = new QTableWidget(0, 6, sl);
    m_loads->setHorizontalHeaderLabels({
        I18n::tr("ma_dir"), "X [m]", "Y [m]", "Z [m]",
        I18n::tr("ma_kind"), I18n::tr("ma_value") });
    m_loads->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_loads->verticalHeader()->setDefaultSectionSize(24);
    m_loads->setMinimumHeight(110);
    sl->vbox()->addWidget(m_loads);

    auto *lrow = new QHBoxLayout();
    auto *ladd = new QPushButton(I18n::tr("ma_add_load"), sl);
    auto *ldel = new QPushButton(I18n::tr("ma_del_load"), sl);
    lrow->addWidget(ladd);
    lrow->addWidget(ldel);
    lrow->addStretch(1);
    sl->vbox()->addLayout(lrow);
    v->addWidget(sl);

    // ライブラリ読込 (mock の library サブタブ)
    auto *slib = new SectionBox(I18n::tr("ma_lib"), body);
    slib->vbox()->addWidget(new QLabel(I18n::tr("ma_lib_std"), slib));
    static const char *libKeys[4] = { "ma_lib_ri", "ma_lib_nist",
                                      "ma_lib_astm", "ma_lib_ofd" };
    // 各行を QWidget に包み、ドメイン限定の行を丸ごと隠せるようにする
    QWidget *libRows[4];
    for (int i = 0; i < 4; ++i) {
        const QString name = I18n::tr(libKeys[i]);
        auto *row = new QWidget(slib);
        auto *r = new QHBoxLayout(row);
        r->setContentsMargins(0, 0, 0, 0);
        r->addWidget(new QLabel(QString::fromUtf8("▸"), row));
        r->addWidget(new QLabel(name, row), 1);
        auto *load = new QPushButton(I18n::tr("ma_lib_load"), row);
        r->addWidget(load);
        slib->vbox()->addWidget(row);
        libRows[i] = row;
        connect(load, &QPushButton::clicked, this, [this, name] {
            m_libStatus->setText(I18n::tr("ma_lib_todo").arg(name));
        });
    }
    // RefractiveIndex.info は光学定数 DB、ASTM は音響材料 DB (updateColumns で切替)
    m_libRowRi   = libRows[0];
    m_libRowAstm = libRows[2];
    m_libStatus = new QLabel(slib);
    m_libStatus->setWordWrap(true);
    slib->vbox()->addWidget(m_libStatus);
    v->addWidget(slib);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    connect(madd, &QPushButton::clicked, this, [this] {
        Material m;
        m.epsr = 2.0;
        m_p->materials().push_back(m);
        refresh();
        m_p->touch();
    });
    connect(mdel, &QPushButton::clicked, this, [this] {
        const int row = m_mats->currentRow();
        auto &mats = m_p->materials();
        if (row >= 0 && row < mats.size()) {
            mats.removeAt(row);
            refresh();
            m_p->touch();
        }
    });
    connect(m_mats, &QTableWidget::cellChanged, this, [this] {
        if (m_updating) return;
        applyMaterials();
        if (isAcousticDomain()) refresh();   // Z = ρc を再計算
        m_p->touch();
    });

    connect(ladd, &QPushButton::clicked, this, [this] {
        m_p->loads().push_back(Load{});
        refresh();
        m_p->touch();
    });
    connect(ldel, &QPushButton::clicked, this, [this] {
        const int row = m_loads->currentRow();
        auto &loads = m_p->loads();
        if (row >= 0 && row < loads.size()) {
            loads.removeAt(row);
            refresh();
            m_p->touch();
        }
    });
    connect(m_loads, &QTableWidget::cellChanged, this, [this] {
        if (m_updating) return;
        applyLoads();
        m_p->touch();
    });

    connect(project, &Project::loaded, this, &MaterialTab::refresh);
    connect(project, &Project::materialsEdited, this, &MaterialTab::refresh);
    // ドメイン切替で列の意味 (電磁 εr… / 音響 ρ,c,α,Z) と見出しが変わる
    connect(project, &Project::domainChanged, this, [this] { refresh(); });
    refresh();
}

bool MaterialTab::isAcousticDomain() const
{
    const Domain d = m_p->activeDomain();
    return d == Domain::Acoustic || d == Domain::Underwater;
}

bool MaterialTab::isOpticalDomain() const
{
    return m_p->activeDomain() == Domain::Optical;
}

// ドメイン別の見出し (mock の isOpt / isAc 分岐)
// 光ドメインは mock どおり n / k (消衰係数) 列にする (通常媒質の行のみ換算)。
void MaterialTab::updateColumns()
{
    const bool ac = isAcousticDomain();
    const bool opt = isOpticalDomain();
    m_mats->setHorizontalHeaderLabels(ac
        ? QStringList{ I18n::tr("ma_type"), I18n::tr("ma_rho"),
                       I18n::tr("ma_c_sound"), I18n::tr("ma_absorption"),
                       I18n::tr("ma_impedance"), I18n::tr("ma_name"),
                       I18n::tr("ma_id") }
        : opt
        ? QStringList{ I18n::tr("ma_type"), I18n::tr("ma_n"), I18n::tr("ma_k"),
                       QString::fromUtf8("μr / b"), QString::fromUtf8("σm / c"),
                       I18n::tr("ma_name"), I18n::tr("ma_id") }
        : QStringList{ I18n::tr("ma_type"), QString::fromUtf8("εr / ε∞"),
                       QString::fromUtf8("σ / a"), QString::fromUtf8("μr / b"),
                       QString::fromUtf8("σm / c"), I18n::tr("ma_name"),
                       I18n::tr("ma_id") });
    if (opt) {
        // n / k と .ofd の εr / σ の関係を見出しの tooltip で明示する
        const QString tip = I18n::tr("ma_nk_tip")
            .arg(QString::number(centerLambdaNm(m_p->optical()), 'f', 1));
        for (int c : { 1, 2 })
            if (auto *h = m_mats->horizontalHeaderItem(c)) h->setToolTip(tip);
    }
    m_matSection->setTitle(I18n::tr("ma_section")
        + (opt ? I18n::tr("ma_opt_suffix")
               : ac ? I18n::tr("ma_ac_suffix") : QString()));
    m_dispHint->setVisible(opt);

    // ── ドメイン別のセクション/行の出し分け ────────────────────────────────
    // 集中定数素子 (.ofd の load キー) は EM FDTD 専用。隠すだけでモデルと
    // シリアライズは従来どおり保持する (可視性で書き込みを分岐しない)。
    m_lumpedSection->setVisible(m_p->activeDomain() == Domain::EM);
    // 標準ライブラリ: RefractiveIndex.info は光のみ、ASTM 音響材料は音響/水中のみ
    m_libRowRi->setVisible(opt);
    m_libRowAstm->setVisible(ac);
}

void MaterialTab::applyMaterials()
{
    auto &mats = m_p->materials();
    for (int r = 0; r < m_mats->rowCount() && r < mats.size(); ++r) {
        Material &m = mats[r];
        if (auto *cb = qobject_cast<QComboBox *>(m_mats->cellWidget(r, 0)))
            m.type = cb->currentIndex() + 1;
        auto cell = [this, r](int c) {
            auto *it = m_mats->item(r, c);
            return it ? it->text() : QString();
        };
        if (isAcousticDomain()) {
            // 音響/水中ドメイン: ρ, c, α を編集 (Z = ρc は計算値なので読取専用)
            m.rho        = cell(1).toDouble();
            m.soundSpeed = cell(2).toDouble();
            m.absorption = cell(3).toDouble();
        } else if (m.type == 2) {
            m.einf = cell(1).toDouble();
            m.ae   = cell(2).toDouble();
            m.be   = cell(3).toDouble();
            m.ce   = cell(4).toDouble();
        } else if (isOpticalDomain()) {
            // 光ドメイン: n / k 入力 → .ofd の εr / σ へ換算 (書式は不変)
            nkToEpsSgm(cell(1).toDouble(), cell(2).toDouble(),
                       omegaOf(m_p->optical()), m.epsr, m.esgm);
            m.amur = cell(3).toDouble();
            m.msgm = cell(4).toDouble();
        } else {
            m.epsr = cell(1).toDouble();
            m.esgm = cell(2).toDouble();
            m.amur = cell(3).toDouble();
            m.msgm = cell(4).toDouble();
        }
        m.name = cell(5);
    }
}

void MaterialTab::applyLoads()
{
    auto &loads = m_p->loads();
    for (int r = 0; r < m_loads->rowCount() && r < loads.size(); ++r) {
        Load &l = loads[r];
        auto cell = [this, r](int c) {
            auto *it = m_loads->item(r, c);
            return it ? it->text() : QString();
        };
        if (auto *cb = qobject_cast<QComboBox *>(m_loads->cellWidget(r, 0)))
            l.dir = "XYZ"[cb->currentIndex()];
        l.x = cell(1).toDouble();
        l.y = cell(2).toDouble();
        l.z = cell(3).toDouble();
        if (auto *cb = qobject_cast<QComboBox *>(m_loads->cellWidget(r, 4)))
            l.kind = "RLC"[cb->currentIndex()];
        l.value = cell(5).toDouble();
    }
}

void MaterialTab::refresh()
{
    m_updating = true;
    updateColumns();
    const bool ac = isAcousticDomain();
    const bool opt = isOpticalDomain();
    const double omega = opt ? omegaOf(m_p->optical()) : 0.0;

    const auto &mats = m_p->materials();
    m_mats->setRowCount(mats.size());
    for (int r = 0; r < mats.size(); ++r) {
        const Material &m = mats[r];
        auto *type = new QComboBox(m_mats);
        type->addItem(I18n::tr("ma_normal"));
        type->addItem(I18n::tr("ma_dispersive"));
        type->setCurrentIndex(m.type == 2 ? 1 : 0);
        // 音響/水中: 分散モデル (Drude/Lorentz/Sellmeier) は光学の概念なので
        // 「分散性」を選択不可にする。既に分散性の材料は表示のみ維持
        // (無効項目でも currentIndex には設定できる)。
        if (ac)
            if (auto *im = qobject_cast<QStandardItemModel *>(type->model()))
                if (auto *item = im->item(1))
                    item->setEnabled(false);
        connect(type, &QComboBox::currentIndexChanged, this, [this] {
            if (m_updating) return;
            applyMaterials();
            refresh();          // re-label value columns
            m_p->touch();
        });
        m_mats->setCellWidget(r, 0, type);

        // 光ドメインの通常媒質は εr / σ を n / k に戻して見せる (mock の列)
        const bool nk = (opt && m.type != 2);
        double nOpt = 0.0, kOpt = 0.0;
        if (nk) epsSgmToNk(m.epsr, m.esgm, omega, nOpt, kOpt);
        const double vals[4] = {
            ac ? m.rho        : nk ? nOpt : m.type == 2 ? m.einf : m.epsr,
            ac ? m.soundSpeed : nk ? kOpt : m.type == 2 ? m.ae   : m.esgm,
            ac ? m.absorption : m.type == 2 ? m.be   : m.amur,
            ac ? m.rho * m.soundSpeed : m.type == 2 ? m.ce : m.msgm };
        for (int c = 0; c < 4; ++c) {
            auto *it = new QTableWidgetItem(QString::number(vals[c], 'g', 8));
            if (ac && c == 3)   // 音響インピーダンス Z = ρc は計算値
                it->setFlags(it->flags() & ~Qt::ItemIsEditable);
            m_mats->setItem(r, 1 + c, it);
        }
        m_mats->setItem(r, 5, new QTableWidgetItem(m.name));
        auto *id = new QTableWidgetItem(QString::number(r + 2));
        id->setFlags(id->flags() & ~Qt::ItemIsEditable);
        m_mats->setItem(r, 6, id);
    }

    const auto &loads = m_p->loads();
    m_loads->setRowCount(loads.size());
    for (int r = 0; r < loads.size(); ++r) {
        const Load &l = loads[r];
        auto *dir = new QComboBox(m_loads);
        dir->addItems({ "X", "Y", "Z" });
        dir->setCurrentIndex(l.dir == 'X' ? 0 : l.dir == 'Y' ? 1 : 2);
        connect(dir, &QComboBox::currentIndexChanged, this, [this] {
            if (m_updating) return;
            applyLoads();
            m_p->touch();
        });
        m_loads->setCellWidget(r, 0, dir);
        m_loads->setItem(r, 1, new QTableWidgetItem(QString::number(l.x, 'g', 8)));
        m_loads->setItem(r, 2, new QTableWidgetItem(QString::number(l.y, 'g', 8)));
        m_loads->setItem(r, 3, new QTableWidgetItem(QString::number(l.z, 'g', 8)));
        auto *kind = new QComboBox(m_loads);
        kind->addItems({ "R", "L", "C" });
        kind->setCurrentIndex(l.kind == 'R' ? 0 : l.kind == 'L' ? 1 : 2);
        connect(kind, &QComboBox::currentIndexChanged, this, [this] {
            if (m_updating) return;
            applyLoads();
            m_p->touch();
        });
        m_loads->setCellWidget(r, 4, kind);
        m_loads->setItem(r, 5, new QTableWidgetItem(QString::number(l.value, 'g', 8)));
    }

    m_updating = false;
}
