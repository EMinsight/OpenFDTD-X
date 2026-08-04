// AnalysisGroupsTab.cpp
#include "AnalysisGroupsTab.h"
#include "../core/Project.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"
#include "TabHelpers.h"

#include <QColor>
#include <QComboBox>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

using namespace ofd;

namespace {
// タブ専用語彙 (接頭辞 ag_) — file-local 登録
const bool s_i18n = [] {
    ofd::I18n::reg("ag_title", "解析グループ", "Analysis Groups");
    ofd::I18n::reg("ag_hint",
        "モニター+スクリプトをまとめた再利用可能なポスト処理単位。Lumerical の Analysis Group 相当。\n"
        "同じ解析を異なるシミュレーションで使い回せます。",
        "Reusable post-processing units bundling monitors + scripts — the equivalent of "
        "Lumerical's Analysis Groups.\nThe same analysis can be reused across different simulations.");
    ofd::I18n::reg("ag_registered", "登録済みグループ", "Registered groups");
    ofd::I18n::reg("ag_col_name", "名前", "Name");
    ofd::I18n::reg("ag_col_monitors", "含まれるモニター", "Included monitors");
    ofd::I18n::reg("ag_col_output", "出力", "Output");
    ofd::I18n::reg("ag_edit", "編集", "Edit");
    ofd::I18n::reg("ag_add_row", "＋ グループを追加…", "＋ Add group…");
    ofd::I18n::reg("ag_library", "ライブラリから読込", "Library");
    ofd::I18n::reg("ag_library_hint",
        "公式・コミュニティ製の解析グループを読み込み (読込は未実装)",
        "Load official or community-made analysis groups "
        "(loading not implemented)");
    ofd::I18n::reg("ag_lib_std", "📚 標準ライブラリ", "📚 Standard library");
    ofd::I18n::reg("ag_lib_community", "🌐 コミュニティ (GitHub)", "🌐 Community (GitHub)");
    ofd::I18n::reg("ag_lib_file", "📁 ファイルから (.lsf/.py)", "📁 From file (.lsf/.py)");
    ofd::I18n::reg("ag_create", "新規作成", "Create new");
    ofd::I18n::reg("ag_name", "名前", "Name");
    ofd::I18n::reg("ag_input_monitors", "入力モニター", "Input monitors");
    ofd::I18n::reg("ag_script", "解析スクリプト", "Analysis script");
    ofd::I18n::reg("ag_create_btn", "作成", "Create");
    return true;
}();

// 登録済みグループ (ドメイン毎) — mock の配列をそのまま転記
struct GroupRow { const char *name, *monitors, *output; };

const GroupRow kOptical[] = {
    { "Q-factor analyzer",     "3 time monitors",      "Q値, 共振λ, FWHM" },
    { "Transmission spectrum", "2 frequency monitors", "T(λ), R(λ)" },
    { "Mode source coupler",   "1 mode monitor",       "結合効率, η" },
    { "NTFF analyzer",         "6 box monitors",       "遠方界 E(θ,φ)" },
    { "S-matrix extractor",    "N port monitors",      ".s2p Touchstone" },
    { "Polarization analyzer", "1 plane monitor",      "Stokes parameters" },
};
const GroupRow kEm[] = {
    { "Antenna patterns", "6 box monitors",  "遠方界・ゲイン・効率" },
    { "S-parameter",      "N port monitors", "S11/S21 .sNp" },
    { "VSWR",             "feed monitor",    "VSWR(f)" },
    { "SAR analyzer",     "volume monitor",  "局所/全身SAR" },
};
const GroupRow kAcoustic[] = {
    { "RT60 calculator",  "3 point monitors", "RT60(オクターブ)" },
    { "Clarity (C80/D50)","3 point monitors", "C80, D50, STI" },
    { "Auralization",     "binaural monitor", ".wav (HRTF畳み込み)" },
    { "Spatial impulse",  "line monitors",    "反射面別エネルギー" },
};
const GroupRow kUnderwater[] = {
    { "TL analyzer",    "range monitors", "TL(range, depth)" },
    { "Eigenray finder","point monitors", "τ, θ, E" },
    { "Beam pattern",   "sphere monitor", "ソナー指向性 B(θ,φ)" },
};

// 新規作成フォームの入力モニター候補 (ドメイン別の固定サンプル — unwiredNote 済み)
const char *const kMonEm[] = {
    "port1 (feed monitor)", "farfield_box (6 box monitors)",
    "E_field_3D (volume monitor)" };
const char *const kMonOptical[] = {
    "T_drop (plane monitor)", "thru_mode (mode monitor)",
    "E_field_3D (volume monitor)" };
const char *const kMonAcoustic[] = {
    "mic_1 (point monitor)", "mic_2 (point monitor)",
    "binaural_LR (binaural monitor)" };
const char *const kMonUnderwater[] = {
    "rx_line (range monitors)", "hydrophone_1 (point monitor)",
    "beam_sphere (sphere monitor)" };
} // namespace

AnalysisGroupsTab::AnalysisGroupsTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // 概要 / Analysis Groups
    auto *si = new SectionBox(I18n::tr("ag_title"), body);
    auto *hint = new QLabel(I18n::tr("ag_hint"), si);
    hint->setWordWrap(true);
    si->vbox()->addWidget(hint);
    v->addWidget(si);

    // 登録済みグループ / Registered groups
    auto *sg = new SectionBox(I18n::tr("ag_registered"), body);
    m_groups = new QTableWidget(0, 5, sg);
    m_groups->setHorizontalHeaderLabels({ "", I18n::tr("ag_col_name"),
        I18n::tr("ag_col_monitors"), I18n::tr("ag_col_output"), "" });
    m_groups->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_groups->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_groups->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_groups->verticalHeader()->setVisible(false);
    m_groups->setEditTriggers(QAbstractItemView::NoEditTriggers);
    sg->vbox()->addWidget(m_groups);
    // 登録済みグループはドメイン別の固定サンプル (絶対規則 5)
    sg->vbox()->addWidget(tabhelp::sampleNote(sg));
    v->addWidget(sg);

    // ライブラリから読込 / Library
    auto *sl = new SectionBox(I18n::tr("ag_library"), body);
    auto *lh = new QLabel(I18n::tr("ag_library_hint"), sl);
    lh->setWordWrap(true);
    sl->vbox()->addWidget(lh);
    auto *lrow = new QHBoxLayout();
    auto *libStd  = new QPushButton(I18n::tr("ag_lib_std"), sl);
    auto *libComm = new QPushButton(I18n::tr("ag_lib_community"), sl);
    auto *libFile = new QPushButton(I18n::tr("ag_lib_file"), sl);
    tabhelp::markNotImplemented(libStd);
    tabhelp::markNotImplemented(libComm);
    tabhelp::markNotImplemented(libFile);
    lrow->addWidget(libStd);
    lrow->addWidget(libComm);
    lrow->addWidget(libFile);
    lrow->addStretch(1);
    sl->vbox()->addLayout(lrow);
    v->addWidget(sl);

    // 新規作成 / Create new
    auto *sc = new SectionBox(I18n::tr("ag_create"), body);
    m_name = new QLineEdit("my_analysis", sc);
    sc->form()->addRow(I18n::tr("ag_name"), m_name);
    // モニター候補とスクリプト言語はドメイン別サンプル (refresh で構築)
    m_monitors = new QListWidget(sc);
    m_monitors->setSelectionMode(QAbstractItemView::MultiSelection);
    m_monitors->setFixedHeight(80);
    sc->form()->addRow(I18n::tr("ag_input_monitors"), m_monitors);
    m_script = new QComboBox(sc);
    sc->form()->addRow(I18n::tr("ag_script"), m_script);
    auto *crow = new QHBoxLayout();
    auto *createBtn = new QPushButton(I18n::tr("ag_create_btn"), sc);
    tabhelp::markNotImplemented(createBtn);
    crow->addWidget(createBtn);
    crow->addStretch(1);
    sc->vbox()->addLayout(crow);
    // フォーム (モニター候補は固定サンプル) はどこにも読まれない
    sc->vbox()->addWidget(tabhelp::unwiredNote(sc));
    v->addWidget(sc);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    connect(project, &Project::domainChanged, this, &AnalysisGroupsTab::refresh);
    connect(project, &Project::loaded, this, &AnalysisGroupsTab::refresh);
    refresh();
}

// ドメインに応じた登録済みグループ表の再構築 (mock: domain 分岐の配列)
void AnalysisGroupsTab::refresh()
{
    m_updating = true;
    const GroupRow *rows = kEm;
    int n = 4;
    const char *const *mons = kMonEm;
    bool lsf = false;   // LSF (Lumerical スクリプト) は光ドメインのみ意味を持つ
    switch (m_p->activeDomain()) {
        case Domain::Optical:
            rows = kOptical;    n = 6; mons = kMonOptical;    lsf = true; break;
        case Domain::Acoustic:
            rows = kAcoustic;   n = 4; mons = kMonAcoustic;   break;
        case Domain::Underwater:
            rows = kUnderwater; n = 3; mons = kMonUnderwater; break;
        default:
            rows = kEm;         n = 4; mons = kMonEm;         break;
    }

    m_groups->clearSpans();
    m_groups->setRowCount(0);        // 旧行のセルウィジェット (編集ボタン) も破棄
    m_groups->setRowCount(n + 1);
    for (int i = 0; i < n; ++i) {
        auto *ck = new QTableWidgetItem;
        ck->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        ck->setCheckState(i < 2 ? Qt::Checked : Qt::Unchecked);   // 上位2つは既定ON
        m_groups->setItem(i, 0, ck);
        auto *nm = new QTableWidgetItem(QString::fromUtf8(rows[i].name));
        nm->setForeground(QColor("#0078D4"));    // badge acc 相当
        m_groups->setItem(i, 1, nm);
        m_groups->setItem(i, 2,
            new QTableWidgetItem(QString::fromUtf8(rows[i].monitors)));
        m_groups->setItem(i, 3,
            new QTableWidgetItem(QString::fromUtf8(rows[i].output)));
        auto *editBtn = new QPushButton(I18n::tr("ag_edit"), m_groups);
        tabhelp::markNotImplemented(editBtn);   // グループ編集は未実装
        m_groups->setCellWidget(i, 4, editBtn);
    }
    // ＋ グループを追加… 行
    auto *ck = new QTableWidgetItem;
    ck->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
    ck->setCheckState(Qt::Unchecked);
    m_groups->setItem(n, 0, ck);
    m_groups->setSpan(n, 1, 1, 4);
    auto *add = new QTableWidgetItem(I18n::tr("ag_add_row"));
    QFont f = add->font();
    f.setItalic(true);
    add->setFont(f);
    add->setForeground(QColor("#888888"));
    m_groups->setItem(n, 1, add);

    m_groups->setMinimumHeight(30 * (n + 1) + 38);

    // 新規作成フォームのモニター候補 / スクリプト言語もドメイン別サンプルへ
    // 差し替える (どこにも読まれない固定サンプル — unwiredNote 済み)
    m_monitors->clear();
    for (int i = 0; i < 3; ++i)
        m_monitors->addItem(QString::fromUtf8(mons[i]));
    m_script->clear();
    if (lsf)
        m_script->addItem(QStringLiteral("LSF"));
    m_script->addItem(QStringLiteral("Python"));

    m_updating = false;
}
