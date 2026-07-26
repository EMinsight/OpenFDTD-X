// DatasetsTab.cpp
#include "DatasetsTab.h"
#include "../core/Project.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"

#include <QCheckBox>
#include <QColor>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

using namespace ofd;

namespace {
// タブ専用語彙 (接頭辞 ds_) — file-local 登録
const bool s_i18n = [] {
    ofd::I18n::reg("ds_title", "データセット", "Datasets");
    ofd::I18n::reg("ds_hint",
        "COMSOL風の結果データ管理。データセット→派生量→プロットの順で結果を整理。",
        "COMSOL-style result data management. Results are organized as "
        "datasets → derived values → plots.");
    ofd::I18n::reg("ds_derived", "派生量定義", "Derived value");
    ofd::I18n::reg("ds_name", "名前", "Name");
    ofd::I18n::reg("ds_expr", "式", "Expression");
    ofd::I18n::reg("ds_unit", "単位", "Unit");
    ofd::I18n::reg("ds_auto_recalc", "自動再計算", "Auto recompute");
    ofd::I18n::reg("ds_add", "追加", "Add");
    ofd::I18n::reg("ds_export", "エクスポート", "Export");
    ofd::I18n::reg("ds_exp_h5", "💾 HDF5 (一括)", "💾 HDF5 (bulk)");
    return true;
}();
} // namespace

DatasetsTab::DatasetsTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // データセット / Datasets — 結果ブラウザツリー
    auto *sd = new SectionBox(I18n::tr("ds_title"), body);
    auto *hint = new QLabel(I18n::tr("ds_hint"), sd);
    hint->setWordWrap(true);
    sd->vbox()->addWidget(hint);

    m_tree = new QTreeWidget(sd);
    m_tree->setColumnCount(2);
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setIndentation(16);
    auto top = [this](const char *text) {
        return new QTreeWidgetItem(m_tree, { QString::fromUtf8(text) });
    };
    auto child = [](QTreeWidgetItem *parent, const char *text,
                    const char *tag = nullptr) {
        auto *it = new QTreeWidgetItem(parent, { QString::fromUtf8(text),
            tag ? QString::fromUtf8(tag) : QString() });
        it->setForeground(1, QColor("#888888"));   // tag は muted 表示
        return it;
    };
    auto *nd = top("📁 Datasets");
    child(nd, "📊 Study 1/Solution 1 (FDTD time)");
    child(nd, "📊 Study 1/Solution 2 (Frequency)");
    child(nd, "🔍 Cut Plane Z=2.5μm");
    child(nd, "🔍 Cut Line along X");
    child(nd, "📊 Parametric Sweep (R=4..6μm, 11pts)");
    auto *nv = top("📐 Derived Values");
    child(nv, "📈 Peak transmission (max T_drop)");
    child(nv, "📈 Resonance λ (argmax)");
    child(nv, "📈 Q-factor extraction");
    child(nv, "📈 Volume integral |E|²");
    auto *np = top("📊 Plot Groups");
    child(np, "🌈 1D — T(λ)", "3 traces");
    child(np, "🌈 2D — E-field surface (Z plane)");
    child(np, "🌈 3D — |E| volume isosurface");
    child(np, "🌈 Polar — far-field θ scan");
    child(np, "🌈 Animation — propagation movie");
    auto *nr = top("📄 Reports");
    child(nr, "📝 Auto-report 1 (HTML)");
    child(nr, "📝 Auto-report 2 (PDF)");
    m_tree->expandAll();
    m_tree->resizeColumnToContents(0);
    m_tree->setMinimumHeight(340);
    sd->vbox()->addWidget(m_tree);
    v->addWidget(sd);

    // 派生量定義 / Derived value
    auto *sv = new SectionBox(I18n::tr("ds_derived"), body);
    m_name = new QLineEdit("peak_T", sv);
    sv->form()->addRow(I18n::tr("ds_name"), m_name);
    m_expr = new QLineEdit("max(T_drop.transmission, dim=lambda)", sv);
    sv->form()->addRow(I18n::tr("ds_expr"), m_expr);
    m_unit = new QLineEdit(QString::fromUtf8("—"), sv);
    m_unit->setMaximumWidth(100);
    sv->form()->addRow(I18n::tr("ds_unit"), m_unit);
    m_autoRecalc = new QCheckBox(I18n::tr("ds_auto_recalc"), sv);
    m_autoRecalc->setChecked(true);
    sv->vbox()->addWidget(m_autoRecalc);
    auto *arow = new QHBoxLayout();
    arow->addWidget(new QPushButton(I18n::tr("ds_add"), sv));
    arow->addStretch(1);
    sv->vbox()->addLayout(arow);
    v->addWidget(sv);

    // エクスポート / Export
    auto *se = new SectionBox(I18n::tr("ds_export"), body);
    auto *erow = new QHBoxLayout();
    erow->addWidget(new QPushButton("📊 PNG/SVG", se));
    erow->addWidget(new QPushButton("📄 CSV", se));
    erow->addWidget(new QPushButton(I18n::tr("ds_exp_h5"), se));
    erow->addWidget(new QPushButton("📑 Auto-report (HTML)", se));
    erow->addWidget(new QPushButton("📑 PDF", se));
    erow->addWidget(new QPushButton("📁 Touchstone .s2p", se));
    erow->addStretch(1);
    se->vbox()->addLayout(erow);
    v->addWidget(se);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
}
