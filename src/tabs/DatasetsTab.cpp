// DatasetsTab.cpp
#include "DatasetsTab.h"
#include "../core/Project.h"
#include "../kernel/Runner.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"

#include <QCheckBox>
#include <QColor>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

using namespace ofd;

namespace {
// タブ専用語彙 (接頭辞 ds_) — file-local 登録
const bool s_i18n = [] {
    ofd::I18n::reg("ds_title", "データセット", "Datasets");
    ofd::I18n::reg("ds_hint",
        "COMSOL風の結果データ管理。作業ディレクトリの実在する結果ファイルを"
        "一覧します (計算実行後に更新)。",
        "COMSOL-style result data management. Lists the result files that "
        "actually exist in the working directory (updated after a run).");
    ofd::I18n::reg("ds_files", "結果ファイル", "Result files");
    ofd::I18n::reg("ds_wd", "作業ディレクトリ: %1", "Working directory: %1");
    ofd::I18n::reg("ds_wd_none", "作業ディレクトリ未確定 (プロジェクト未保存)",
                   "Working directory undecided (project not saved)");
    ofd::I18n::reg("ds_empty",
        "（結果なし — このプロジェクトの計算実行後に表示されます）",
        "(no results — populated after running this project)");
    ofd::I18n::reg("ds_reload", "↻ 再読込", "↻ Reload");
    ofd::I18n::reg("ds_notimpl", "未実装", "Not implemented");
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

// 作業ディレクトリに現れうる結果ファイル (カーネル出力の既知パターン)。
// ワイルドカードは QDir のネームフィルタ書式。
const char *kResultPatterns[] = {
    "*.log",                 // ofd.log / orcwa.log / obpm.log / solver.log
    "*.out",                 // ofd.out / obpm.out (ポスト処理入力)
    "*.csv",                 // zin.csv / rcwa_efficiency.csv / activation_curve.csv ...
    "*.h5",                  // time_series_data.h5
    "*.s[0-9]p",             // Touchstone
    "ev2d*", "ev3d*", "*.htm",   // ofd_post の作図出力
    "*.shd", "*.prt",        // bellhop (伝搬損失場 / テキスト結果)
    "*.wav",                 // 可聴化出力
};

QString sizeText(qint64 bytes)
{
    if (bytes >= 1024 * 1024)
        return QStringLiteral("%1 MB").arg(double(bytes) / (1024.0 * 1024.0),
                                           0, 'f', 1);
    if (bytes >= 1024)
        return QStringLiteral("%1 KB").arg(double(bytes) / 1024.0, 0, 'f', 1);
    return QStringLiteral("%1 B").arg(bytes);
}
} // namespace

DatasetsTab::DatasetsTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // データセット / Datasets — 実在する結果ファイルのブラウザ。
    // モックの固定ツリー (Study 1 / Cut Plane / Q-factor …) は実体が無いので
    // 表示しない (絶対規則 5 — 実行していない結果を見せない)。
    auto *sd = new SectionBox(I18n::tr("ds_title"), body);
    auto *hint = new QLabel(I18n::tr("ds_hint"), sd);
    hint->setWordWrap(true);
    sd->vbox()->addWidget(hint);

    m_wdLabel = new QLabel(sd);
    m_wdLabel->setWordWrap(true);
    m_wdLabel->setStyleSheet("font-size:11px; color:palette(mid);");
    sd->vbox()->addWidget(m_wdLabel);

    m_tree = new QTreeWidget(sd);
    m_tree->setColumnCount(2);
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setIndentation(16);
    m_tree->setMinimumHeight(220);
    sd->vbox()->addWidget(m_tree);

    auto *rrow = new QHBoxLayout();
    auto *reload = new QPushButton(I18n::tr("ds_reload"), sd);
    connect(reload, &QPushButton::clicked, this, &DatasetsTab::rebuildTree);
    rrow->addWidget(reload);
    rrow->addStretch(1);
    sd->vbox()->addLayout(rrow);
    v->addWidget(sd);

    // 派生量定義 / Derived value — フォームは設計どおり置くが、評価器が
    // 無いので「追加」は未実装表示 (押せる見た目にしない)
    auto *sv = new SectionBox(I18n::tr("ds_derived"), body);
    m_name = new QLineEdit("peak_T", sv);
    sv->form()->addRow(I18n::tr("ds_name"), m_name);
    // 式の既定例はドメイン別のプレースホルダで示す (updateDomainVisibility)
    m_expr = new QLineEdit(sv);
    sv->form()->addRow(I18n::tr("ds_expr"), m_expr);
    m_unit = new QLineEdit(QString::fromUtf8("—"), sv);
    m_unit->setMaximumWidth(100);
    sv->form()->addRow(I18n::tr("ds_unit"), m_unit);
    m_autoRecalc = new QCheckBox(I18n::tr("ds_auto_recalc"), sv);
    m_autoRecalc->setChecked(true);
    sv->vbox()->addWidget(m_autoRecalc);
    auto *arow = new QHBoxLayout();
    auto *add = new QPushButton(I18n::tr("ds_add"), sv);
    add->setEnabled(false);
    add->setToolTip(I18n::tr("ds_notimpl"));
    arow->addWidget(add);
    arow->addStretch(1);
    sv->vbox()->addLayout(arow);
    v->addWidget(sv);

    // エクスポート / Export — 実装済みの出力は各タブ (PlotPanel の CSV/PNG、
    // H5 タブ等) にあり、ここからの一括出力は未実装。無効表示にする。
    auto *se = new SectionBox(I18n::tr("ds_export"), body);
    auto *erow = new QHBoxLayout();
    const char *kExpLabels[] = { "📊 PNG/SVG", "📄 CSV", nullptr,
                                 "📑 Auto-report (HTML)", "📑 PDF" };
    for (const char *label : kExpLabels) {
        auto *b = new QPushButton(
            label ? QString::fromUtf8(label) : I18n::tr("ds_exp_h5"), se);
        b->setEnabled(false);
        b->setToolTip(I18n::tr("ds_notimpl"));
        erow->addWidget(b);
    }
    // Touchstone .s2p は S 行列 (EM/光) のみ意味を持つ → ドメイン別に非表示
    m_expTouchstone = new QPushButton(
        QString::fromUtf8("📁 Touchstone .s2p"), se);
    m_expTouchstone->setEnabled(false);
    m_expTouchstone->setToolTip(I18n::tr("ds_notimpl"));
    erow->addWidget(m_expTouchstone);
    erow->addStretch(1);
    se->vbox()->addLayout(erow);
    v->addWidget(se);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    // プロジェクトの読み込み/保存でパスが変わったら一覧を取り直す
    connect(m_p, &Project::loaded, this, &DatasetsTab::rebuildTree);
    // ドメイン別の出し分け (式の既定例プレースホルダ / Touchstone ボタン)
    connect(m_p, &Project::domainChanged, this,
            &DatasetsTab::updateDomainVisibility);
    connect(m_p, &Project::loaded, this,
            &DatasetsTab::updateDomainVisibility);
    rebuildTree();
    updateDomainVisibility();
}

// ドメインに応じた出し分け:
//   - 派生量の式の既定例 (プレースホルダ) をドメインで意味を持つ量に替える
//   - Touchstone .s2p は S 行列出力 — 音響/水中では意味を持たないため隠す
// (フォームは未実装のためモデル・保存内容には一切影響しない)
void DatasetsTab::updateDomainVisibility()
{
    const Domain d = m_p->activeDomain();
    const char *example = "min(S11.dB, dim=freq)";                     // EM
    switch (d) {
    case Domain::Optical:
        example = "max(T_drop.transmission, dim=lambda)"; break;
    case Domain::Acoustic:
        example = "mean(RT60.octave, dim=band)"; break;
    case Domain::Underwater:
        example = "min(TL.field, dim=range)"; break;
    default: break;
    }
    m_expr->setPlaceholderText(QString::fromLatin1(example));
    m_expTouchstone->setVisible(d != Domain::Acoustic
                                && d != Domain::Underwater);
}

// 作業ディレクトリを走査して実在する結果ファイルだけを列挙する
void DatasetsTab::rebuildTree()
{
    m_tree->clear();

    const QString wd = Runner::resolveWorkingDir(m_p, {});
    m_wdLabel->setText(wd.isEmpty()
        ? I18n::tr("ds_wd_none")
        : I18n::tr("ds_wd").arg(QDir::toNativeSeparators(wd)));

    auto *root = new QTreeWidgetItem(
        m_tree, { QStringLiteral("📁 ") + I18n::tr("ds_files") });

    QFileInfoList files;
    if (!wd.isEmpty() && QDir(wd).exists()) {
        QStringList patterns;
        for (const char *p : kResultPatterns)
            patterns << QString::fromLatin1(p);
        files = QDir(wd).entryInfoList(patterns, QDir::Files, QDir::Time);
    }

    if (files.isEmpty()) {
        auto *it = new QTreeWidgetItem(root, { I18n::tr("ds_empty") });
        it->setForeground(0, QColor("#888888"));
    } else {
        const QLocale loc;
        for (const QFileInfo &fi : files) {
            auto *it = new QTreeWidgetItem(root,
                { QStringLiteral("📄 ") + fi.fileName(),
                  sizeText(fi.size()) + QStringLiteral("  ")
                      + loc.toString(fi.lastModified(),
                                     QLocale::ShortFormat) });
            it->setForeground(1, QColor("#888888"));
        }
    }
    m_tree->expandAll();
    m_tree->resizeColumnToContents(0);
}
