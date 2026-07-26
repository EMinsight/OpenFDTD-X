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
#include <QTableWidget>
#include <QVBoxLayout>

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
    return true;
}();
}

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

    // lumped elements
    auto *sl = new SectionBox(I18n::tr("ma_lumped"), body);
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
    for (const char *key : libKeys) {
        const QString name = I18n::tr(key);
        auto *r = new QHBoxLayout();
        r->addWidget(new QLabel(QString::fromUtf8("▸"), slib));
        r->addWidget(new QLabel(name, slib), 1);
        auto *load = new QPushButton(I18n::tr("ma_lib_load"), slib);
        r->addWidget(load);
        slib->vbox()->addLayout(r);
        connect(load, &QPushButton::clicked, this, [this, name] {
            m_libStatus->setText(I18n::tr("ma_lib_todo").arg(name));
        });
    }
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
void MaterialTab::updateColumns()
{
    const bool ac = isAcousticDomain();
    m_mats->setHorizontalHeaderLabels(ac
        ? QStringList{ I18n::tr("ma_type"), I18n::tr("ma_rho"),
                       I18n::tr("ma_c_sound"), I18n::tr("ma_absorption"),
                       I18n::tr("ma_impedance"), I18n::tr("ma_name"),
                       I18n::tr("ma_id") }
        : QStringList{ I18n::tr("ma_type"), QString::fromUtf8("εr / ε∞"),
                       QString::fromUtf8("σ / a"), QString::fromUtf8("μr / b"),
                       QString::fromUtf8("σm / c"), I18n::tr("ma_name"),
                       I18n::tr("ma_id") });
    m_matSection->setTitle(I18n::tr("ma_section")
        + (isOpticalDomain() ? I18n::tr("ma_opt_suffix")
                             : ac ? I18n::tr("ma_ac_suffix") : QString()));
    m_dispHint->setVisible(isOpticalDomain());
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

    const auto &mats = m_p->materials();
    m_mats->setRowCount(mats.size());
    for (int r = 0; r < mats.size(); ++r) {
        const Material &m = mats[r];
        auto *type = new QComboBox(m_mats);
        type->addItem(I18n::tr("ma_normal"));
        type->addItem(I18n::tr("ma_dispersive"));
        type->setCurrentIndex(m.type == 2 ? 1 : 0);
        connect(type, &QComboBox::currentIndexChanged, this, [this] {
            if (m_updating) return;
            applyMaterials();
            refresh();          // re-label value columns
            m_p->touch();
        });
        m_mats->setCellWidget(r, 0, type);

        const double vals[4] = {
            ac ? m.rho        : m.type == 2 ? m.einf : m.epsr,
            ac ? m.soundSpeed : m.type == 2 ? m.ae   : m.esgm,
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
