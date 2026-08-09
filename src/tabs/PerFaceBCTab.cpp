// PerFaceBCTab.cpp
#include "PerFaceBCTab.h"
#include "TabHelpers.h"
#include "../core/Project.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

using namespace ofd;

// ── タブ固有語彙 (pfb_) + i18n.js 由来の共有キー (bc_) — file-local 登録 ───
namespace {
const bool s_i18n = [] {
    ofd::I18n::reg("pfb_title", "境界面別設定", "Per-face boundary conditions");
    ofd::I18n::reg("pfb_hint",
        "Ansys Lumerical FDTD と同様、6面それぞれに独立して境界条件を設定。\n"
        "対称面を活用すると計算量を1/2〜1/8に削減できます。",
        "Like Ansys Lumerical FDTD, each of the six faces has its own boundary "
        "condition.\nExploiting symmetry planes reduces computation to 1/2-1/8.");
    ofd::I18n::reg("pfb_col_face", "面", "Face");
    ofd::I18n::reg("pfb_col_bc", "境界条件", "Boundary condition");
    ofd::I18n::reg("pfb_col_param", "パラメータ", "Parameters");
    ofd::I18n::reg("pfb_col_note", "備考", "Notes");
    ofd::I18n::reg("pfb_ground", "接地面 (基板)", "Ground plane (substrate)");
    ofd::I18n::reg("pfb_use_sym", "対称利用で計算量1/2",
                   "Use symmetry (1/2 computation)");
    ofd::I18n::reg("pfb_use_periodic", "周期構造を活用",
                   "Exploit periodic structure");
    ofd::I18n::reg("pfb_pml", "PML設定", "PML parameters");
    ofd::I18n::reg("pfb_profile", "プロファイル", "Profile");
    ofd::I18n::reg("pfb_prof_std", "標準", "Standard");
    ofd::I18n::reg("pfb_prof_stretched", "Stretched (推奨)",
                   "Stretched (recommended)");
    ofd::I18n::reg("pfb_layers", "層数", "Layers");
    ofd::I18n::reg("pfb_poly", "多項式次数", "Polynomial order");
    ofd::I18n::reg("pfb_dissipative", "散逸層 (low-Q構造)",
                   "Dissipative layer (low-Q structures)");
    ofd::I18n::reg("pfb_evanescent", "エバネッセント波吸収",
                   "Evanescent wave absorption");
    ofd::I18n::reg("pfb_applied_note",
                   "※ 層数・多項式次数のみ .ofd の PML 設定 (pmlL / pmlM) へ"
                   "反映されます。",
                   "* Only the layer count and polynomial order are applied to "
                   "the .ofd PML settings (pmlL / pmlM).");

    // 面名・境界条件名 (i18n.js の bc_* — 既存キーがあればそちらが優先される)
    ofd::I18n::reg("bc_xmin", "X最小面", "X min face");
    ofd::I18n::reg("bc_xmax", "X最大面", "X max face");
    ofd::I18n::reg("bc_ymin", "Y最小面", "Y min face");
    ofd::I18n::reg("bc_ymax", "Y最大面", "Y max face");
    ofd::I18n::reg("bc_zmin", "Z最小面", "Z min face");
    ofd::I18n::reg("bc_zmax", "Z最大面", "Z max face");
    ofd::I18n::reg("bc_pml", "PML", "PML");
    ofd::I18n::reg("bc_pec", "PEC", "PEC");
    ofd::I18n::reg("bc_pmc", "PMC", "PMC");
    ofd::I18n::reg("bc_periodic", "周期", "Periodic");
    ofd::I18n::reg("bc_bloch", "ブロッホ", "Bloch");
    ofd::I18n::reg("bc_symmetric", "対称", "Symmetric");
    ofd::I18n::reg("bc_antisym", "反対称", "Anti-symmetric");
    I18n::reg("pfb_uw_face", "面別の境界条件コンボと対称 / 周期のチェック",
              "the per-face boundary-condition combo boxes and the symmetry / periodic check boxes");
    I18n::reg("pfb_uw_pml", "PML の詳細チェック",
              "the detailed PML check boxes");
    return true;
}();

const char *kFaceKeys[6] = { "bc_xmin", "bc_xmax", "bc_ymin",
                             "bc_ymax", "bc_zmin", "bc_zmax" };
const char *kBcKeys[7]   = { "bc_pml", "bc_pec", "bc_pmc", "bc_periodic",
                             "bc_bloch", "bc_symmetric", "bc_antisym" };
} // namespace

PerFaceBCTab::PerFaceBCTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // ── 境界面別設定 / Per-face boundary conditions ─────────────────────────
    auto *s = new SectionBox(I18n::tr("pfb_title"), body);
    auto *hint = new QLabel(I18n::tr("pfb_hint"), s);
    hint->setWordWrap(true);
    s->vbox()->addWidget(hint);

    m_faces = new QTableWidget(6, 4, s);
    m_faces->setHorizontalHeaderLabels({ I18n::tr("pfb_col_face"),
                                         I18n::tr("pfb_col_bc"),
                                         I18n::tr("pfb_col_param"),
                                         I18n::tr("pfb_col_note") });
    m_faces->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_faces->verticalHeader()->setVisible(false);
    m_faces->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_faces->setMinimumHeight(220);
    for (int r = 0; r < 6; ++r) {
        m_faces->setItem(r, 0, new QTableWidgetItem(I18n::tr(kFaceKeys[r])));
        m_faceBC[r] = new QComboBox(m_faces);
        for (const char *key : kBcKeys)
            m_faceBC[r]->addItem(I18n::tr(key));
        m_faceBC[r]->setCurrentIndex(r == 4 ? 1 : 0);   // Z最小面のみ PEC
        m_faces->setCellWidget(r, 1, m_faceBC[r]);
        m_faces->setItem(r, 2, new QTableWidgetItem("8 layers, order=4"));
        m_faces->setItem(r, 3, new QTableWidgetItem(
            r == 4 ? I18n::tr("pfb_ground") : QString()));
    }
    s->vbox()->addWidget(m_faces);

    auto *checks = new QHBoxLayout();
    m_useSym = new QCheckBox(I18n::tr("pfb_use_sym"), s);
    m_usePeriodic = new QCheckBox(I18n::tr("pfb_use_periodic"), s);
    checks->addWidget(m_useSym);
    checks->addWidget(m_usePeriodic);
    checks->addStretch(1);
    s->vbox()->addLayout(checks);
    // 面別 BC コンボ・対称/周期チェックはどこにも反映されない
    // (未実装の明示 — 絶対規則 5)
    s->vbox()->addWidget(ofd::tabhelp::unwiredNote(s, I18n::tr("pfb_uw_face")));
    v->addWidget(s);

    // ── PML設定 / PML parameters ────────────────────────────────────────────
    auto *sp = new SectionBox(I18n::tr("pfb_pml"), body);
    m_profile = new QComboBox(sp);
    m_profile->addItem(I18n::tr("pfb_prof_std"));
    m_profile->addItem(I18n::tr("pfb_prof_stretched"));
    m_profile->addItem("CPML");
    m_profile->addItem("UPML");
    m_profile->setCurrentIndex(1);       // 既定 "stretched"
    sp->form()->addRow(I18n::tr("pfb_profile"), m_profile);

    m_layers = new QSpinBox(sp);
    m_layers->setRange(1, 64);
    m_layers->setValue(8);               // モック既定 (refresh で Project 値に)
    sp->form()->addRow(I18n::tr("pfb_layers"), m_layers);

    auto num = [&sp](const char *def) {
        auto *e = new QLineEdit(def, sp);
        e->setMaximumWidth(70);
        return e;
    };
    m_alphaMax = num("0.0");
    m_kappaMax = num("2.0");
    m_sigmaMax = num("1.0");
    m_polyOrder = num("3");
    sp->form()->addRow("α_max", m_alphaMax);
    sp->form()->addRow("κ_max", m_kappaMax);
    sp->form()->addRow("σ_max", m_sigmaMax);
    sp->form()->addRow(I18n::tr("pfb_poly"), m_polyOrder);

    auto *pmlChecks = new QHBoxLayout();
    m_dissipative = new QCheckBox(I18n::tr("pfb_dissipative"), sp);
    m_evanescent = new QCheckBox(I18n::tr("pfb_evanescent"), sp);
    m_evanescent->setChecked(true);
    pmlChecks->addWidget(m_dissipative);
    pmlChecks->addWidget(m_evanescent);
    pmlChecks->addStretch(1);
    sp->form()->addRow(pmlChecks);
    // apply() が反映するのは pmlL / pmlM のみ。プロファイル・α/κ/σ・
    // 散逸/エバネッセントは未接続 (未実装の明示 — 絶対規則 5)
    auto *appliedNote = new QLabel(I18n::tr("pfb_applied_note"), sp);
    appliedNote->setWordWrap(true);
    appliedNote->setStyleSheet("font-size:11px; color:palette(mid);");
    sp->vbox()->addWidget(appliedNote);
    sp->vbox()->addWidget(ofd::tabhelp::unwiredNote(sp, I18n::tr("pfb_uw_pml")));
    v->addWidget(sp);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    connect(m_layers, &QSpinBox::valueChanged, this, [this] { apply(); });
    connect(m_polyOrder, &QLineEdit::editingFinished, this, [this] { apply(); });
    connect(project, &Project::loaded, this, &PerFaceBCTab::refresh);
    refresh();
}

// 層数 / 多項式次数のみ Project (GeneralOpts) に対応するので永続化
void PerFaceBCTab::apply()
{
    if (m_updating) return;
    GeneralOpts &g = m_p->general();
    g.pmlL = m_layers->value();
    bool ok = false;
    const double m = m_polyOrder->text().toDouble(&ok);
    if (ok && m > 0) g.pmlM = m;
    m_p->touch();
}

void PerFaceBCTab::refresh()
{
    m_updating = true;
    const GeneralOpts &g = m_p->general();
    m_layers->setValue(g.pmlL);
    m_polyOrder->setText(QString::number(g.pmlM, 'g', 6));
    m_updating = false;
}
