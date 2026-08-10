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
#include <QTimer>
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
    ofd::I18n::reg("ag_del", "削除", "Delete");
    ofd::I18n::reg("ag_add_row", "＋ グループを追加…", "＋ Add group…");
    ofd::I18n::reg("ag_add_row_tip",
                   "この行をクリックすると空のグループを 1 行追加します",
                   "Click this row to append an empty group");
    ofd::I18n::reg("ag_list_note",
                   "登録済みグループはプロジェクトの解析グループ定義です "
                   "(.ofdx に保存)。名前・モニター・出力の各セルは編集できます。"
                   "スクリプト実行は未実装のため、この定義は記録であり計算には"
                   "渡されません。",
                   "The registered groups are this project's analysis-group "
                   "definitions (saved in .ofdx). Name / monitors / output cells "
                   "are editable. Script execution is not implemented, so these "
                   "definitions are a record only and are not fed to any solver.");
    ofd::I18n::reg("ag_mon_empty",
                   "モニターが未定義です (モニタータブで追加してください)",
                   "No monitors defined yet (add them in the Monitors tab)");
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
    I18n::reg("ag_uw_lang", "スクリプト言語の選択 (実行が未実装のため)",
              "the script language (running scripts is not implemented)");
    I18n::reg("ag_uw_lang_ok", "作成したグループの一覧そのもの (.ofdx に保存されます)",
              "the group list itself (saved to .ofdx)");
    return true;
}();

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
    sg->vbox()->addWidget(m_groups);
    // 一覧は Project::analysisGroups() のビュー (.ofdx へ保存される実データ)。
    // 計算へ渡らないことだけは明示しておく (絶対規則 5)。
    auto *listNote = new QLabel(I18n::tr("ag_list_note"), sg);
    listNote->setWordWrap(true);
    listNote->setStyleSheet("color:#7A7A7A; font-size:11px;");
    sg->vbox()->addWidget(listNote);
    v->addWidget(sg);

    // 編集 (チェック / 名前 / モニター / 出力) → モデルへ書き戻す
    connect(m_groups, &QTableWidget::itemChanged, this,
            [this] { applyGroups(); });
    // 末尾の「＋ グループを追加…」行のクリックで空の 1 行を追加
    connect(m_groups, &QTableWidget::cellClicked, this, [this](int row, int) {
        if (row == m_p->analysisGroups().size())
            addGroup(QString(), QString());
    });

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
    // モニター候補はプロジェクトのモニター定義 (MonitorsTab / .ofdx) そのもの
    m_monitors = new QListWidget(sc);
    m_monitors->setSelectionMode(QAbstractItemView::MultiSelection);
    m_monitors->setFixedHeight(80);
    sc->form()->addRow(I18n::tr("ag_input_monitors"), m_monitors);
    m_script = new QComboBox(sc);
    sc->form()->addRow(I18n::tr("ag_script"), m_script);
    auto *crow = new QHBoxLayout();
    auto *createBtn = new QPushButton(I18n::tr("ag_create_btn"), sc);
    crow->addWidget(createBtn);
    crow->addStretch(1);
    sc->vbox()->addLayout(crow);
    // 作成したグループは一覧 (.ofdx) に入るが、スクリプト言語の選択は
    // どこにも読まれない (実行が未実装のため)
    sc->vbox()->addWidget(tabhelp::unwiredNote(sc, I18n::tr("ag_uw_lang"), I18n::tr("ag_uw_lang_ok")));
    v->addWidget(sc);

    // 「作成」= 名前 + 選択したモニターから一覧へ 1 行追加する
    connect(createBtn, &QPushButton::clicked, this, [this] {
        QStringList sel;
        for (QListWidgetItem *it : m_monitors->selectedItems())
            sel << it->text();
        addGroup(m_name->text().trimmed(), sel.join(", "));
    });

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    connect(project, &Project::domainChanged, this, &AnalysisGroupsTab::refresh);
    connect(project, &Project::loaded, this, &AnalysisGroupsTab::refresh);
    // 別タブでモニターを増減したら候補リストも追従させる
    connect(project, &Project::changed, this,
            [this] { rebuildMonitorChoices(); });
    refresh();
}

// グループを 1 行追加する (「作成」ボタン / 一覧末尾の追加行から)
void AnalysisGroupsTab::addGroup(const QString &name, const QString &monitors)
{
    QVector<AnalysisGroupRow> &grps = m_p->analysisGroups();
    AnalysisGroupRow r;
    r.name = name.isEmpty() ? QStringLiteral("group_%1").arg(grps.size() + 1)
                            : name;
    r.monitors = monitors;
    r.output = QString::fromUtf8("—");   // 出力は利用者が記入する
    grps.push_back(r);
    m_p->touch();
    rebuildGroups();
    m_groups->setCurrentCell(grps.size() - 1, 1);
}

// 一覧 (ウィジェット) → モデル
void AnalysisGroupsTab::applyGroups()
{
    if (m_updating) return;
    QVector<AnalysisGroupRow> &grps = m_p->analysisGroups();
    for (int i = 0; i < grps.size() && i < m_groups->rowCount(); ++i) {
        auto cell = [this, i](int c) {
            QTableWidgetItem *it = m_groups->item(i, c);
            return it ? it->text() : QString();
        };
        if (QTableWidgetItem *ck = m_groups->item(i, 0))
            grps[i].enabled = (ck->checkState() == Qt::Checked);
        grps[i].name = cell(1);
        grps[i].monitors = cell(2);
        grps[i].output = cell(3);
    }
    m_p->touch();
}

// モデル → 一覧 (ウィジェット)
void AnalysisGroupsTab::rebuildGroups()
{
    const QVector<AnalysisGroupRow> &grps = m_p->analysisGroups();
    const int n = grps.size();

    m_updating = true;
    m_groups->clearSpans();
    m_groups->setRowCount(0);        // 旧行のセルウィジェット (削除ボタン) も破棄
    m_groups->setRowCount(n + 1);
    for (int i = 0; i < n; ++i) {
        auto *ck = new QTableWidgetItem;
        ck->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        ck->setCheckState(grps[i].enabled ? Qt::Checked : Qt::Unchecked);
        m_groups->setItem(i, 0, ck);
        auto *nm = new QTableWidgetItem(grps[i].name);
        nm->setForeground(QColor("#0078D4"));    // badge acc 相当
        m_groups->setItem(i, 1, nm);
        m_groups->setItem(i, 2, new QTableWidgetItem(grps[i].monitors));
        m_groups->setItem(i, 3, new QTableWidgetItem(grps[i].output));
        auto *delBtn = new QPushButton(I18n::tr("ag_del"), m_groups);
        // 削除するとこのボタン自身 (セルウィジェット) が消えるので、クリック
        // ハンドラから抜けた後に実行する (自分のイベント中に破棄しない)
        connect(delBtn, &QPushButton::clicked, this, [this, i] {
            QTimer::singleShot(0, this, [this, i] {
                QVector<AnalysisGroupRow> &g = m_p->analysisGroups();
                if (i < 0 || i >= g.size()) return;
                g.remove(i);
                m_p->touch();
                rebuildGroups();
            });
        });
        m_groups->setCellWidget(i, 4, delBtn);
    }
    // ＋ グループを追加… 行 (クリックで 1 行追加)
    auto *ck = new QTableWidgetItem;
    ck->setFlags(Qt::ItemIsEnabled);
    m_groups->setItem(n, 0, ck);
    m_groups->setSpan(n, 1, 1, 4);
    auto *add = new QTableWidgetItem(I18n::tr("ag_add_row"));
    add->setFlags(add->flags() & ~Qt::ItemIsEditable);
    add->setToolTip(I18n::tr("ag_add_row_tip"));
    QFont f = add->font();
    f.setItalic(true);
    add->setFont(f);
    add->setForeground(QColor("#888888"));
    m_groups->setItem(n, 1, add);

    m_groups->setMinimumHeight(30 * (n + 1) + 38);
    m_updating = false;
}

// 新規作成フォームのモニター候補 = プロジェクトのモニター定義 (実データ)
void AnalysisGroupsTab::rebuildMonitorChoices()
{
    const QVector<MonitorRow> &mons = m_p->monitors();
    QStringList names;
    for (const MonitorRow &m : mons)
        names << QStringLiteral("%1 (%2)").arg(m.name, m.type);
    if (names == m_monNames) return;      // 変化なし (changed() の度の再構築を避ける)
    m_monNames = names;
    m_monitors->clear();
    if (names.isEmpty()) {
        auto *empty = new QListWidgetItem(I18n::tr("ag_mon_empty"));
        empty->setFlags(Qt::NoItemFlags);
        m_monitors->addItem(empty);
    } else {
        m_monitors->addItems(names);
    }
}

// ドメイン切替 / ファイル読込での再構築
void AnalysisGroupsTab::refresh()
{
    // まだ編集されていない (どれかのドメインの既定そのままの) 一覧なら、
    // 新しいドメインの既定へ差し替える。編集済みの一覧はそのまま残す。
    const Domain d = m_p->activeDomain();
    QVector<AnalysisGroupRow> &grps = m_p->analysisGroups();
    if (isDefaultAnalysisGroupSet(grps) && grps != defaultAnalysisGroups(d))
        grps = defaultAnalysisGroups(d);
    rebuildGroups();
    rebuildMonitorChoices();

    m_updating = true;
    // スクリプト言語: LSF (Lumerical スクリプト) は光ドメインのみ意味を持つ
    m_script->clear();
    if (d == Domain::Optical)
        m_script->addItem(QStringLiteral("LSF"));
    m_script->addItem(QStringLiteral("Python"));
    m_updating = false;
}
