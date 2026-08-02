// CloudDialog.cpp
#include "CloudDialog.h"
#include "../core/Project.h"
#include "../I18n.h"
#include "../Theme.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

using namespace ofd;

// ── ダイアログ固有語彙 (cld_) — file-local 登録 ─────────────────────────────
namespace {
const bool s_i18n = [] {
    ofd::I18n::reg("cld_title", "☁ tidy3d クラウド計算 / Cloud submission",
                   "☁ tidy3d cloud submission");
    ofd::I18n::reg("cld_job", "ジョブ名", "Job name");
    ofd::I18n::reg("cld_cost", "見積コスト", "Estimated cost");
    ofd::I18n::reg("cld_credits", "(残: 1,245.8)", "(left: 1,245.8)");
    ofd::I18n::reg("cld_runtime", "推定実行時間", "Est. runtime");
    ofd::I18n::reg("cld_runtime_val", "~ 2分 30秒", "~ 2 min 30 s");
    ofd::I18n::reg("cld_resolution", "解像度", "Resolution");
    ofd::I18n::reg("cld_res_coarse", "粗 (auto-refine)", "Coarse (auto-refine)");
    ofd::I18n::reg("cld_res_medium", "中 (auto-refine)", "Medium (auto-refine)");
    ofd::I18n::reg("cld_res_fine", "細 (auto-refine)", "Fine (auto-refine)");
    ofd::I18n::reg("cld_pml_auto", "自動 (%1 layers)", "Auto (%1 layers)");
    ofd::I18n::reg("cld_pml_manual", "手動 (%1 layers)", "Manual (%1 layers)");
    ofd::I18n::reg("cld_compare", "ローカル計算結果と並列比較",
                   "Compare side by side with the local result");
    ofd::I18n::reg("cld_download", "結果を自動ダウンロード",
                   "Download results automatically");
    ofd::I18n::reg("cld_notify", "完了時に通知", "Notify on completion");
    ofd::I18n::reg("cld_cancel", "キャンセル", "Cancel");
    ofd::I18n::reg("cld_submit", "🚀 送信 / Submit", "🚀 Submit");
    return true;
}();

// モックの静的値
const char kCostText[]    = "~ 8.4 FlexCredits";
const char kDefaultJob[]  = "patch_antenna_2.5GHz_run01";
const int  kCloudPmlAuto  = 12;      // "自動 (12 layers)"

QLabel *monoLabel(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setStyleSheet(Theme::monoQss());
    return l;
}
} // namespace

CloudDialog::CloudDialog(Project *project, QWidget *parent)
    : QDialog(parent), m_p(project)
{
    setWindowTitle(I18n::tr("cld_title"));
    setModal(true);
    setMinimumWidth(480);

    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    auto *body = new QWidget(this);
    auto *bv = new QVBoxLayout(body);
    bv->setContentsMargins(16, 14, 16, 12);
    bv->setSpacing(6);

    auto *form = new QFormLayout();
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    form->setLabelAlignment(Qt::AlignLeft);
    form->setHorizontalSpacing(8);
    form->setVerticalSpacing(6);
    bv->addLayout(form);

    // ── ジョブ名 ────────────────────────────────────────────────────────────
    m_jobName = new QLineEdit(body);
    form->addRow(I18n::tr("cld_job"), m_jobName);

    // ── 見積コスト / 残クレジット ───────────────────────────────────────────
    auto *costRow = new QHBoxLayout();
    m_cost = monoLabel(QString::fromUtf8(kCostText), body);
    costRow->addWidget(m_cost);
    m_credits = new QLabel(I18n::tr("cld_credits"), body);
    m_credits->setStyleSheet("color:palette(mid);");
    costRow->addWidget(m_credits);
    costRow->addStretch(1);
    form->addRow(I18n::tr("cld_cost"), costRow);

    // ── 推定実行時間 ────────────────────────────────────────────────────────
    m_runtime = monoLabel(I18n::tr("cld_runtime_val"), body);
    form->addRow(I18n::tr("cld_runtime"), m_runtime);

    // ── 解像度 / PML (Tidy3dOpts 由来) ──────────────────────────────────────
    m_resolution = monoLabel(QString(), body);
    form->addRow(I18n::tr("cld_resolution"), m_resolution);
    m_pml = monoLabel(QString(), body);
    form->addRow("PML", m_pml);

    auto *sep = new QFrame(body);
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Sunken);
    bv->addWidget(sep);

    // ── オプション ──────────────────────────────────────────────────────────
    m_compareLocal = new QCheckBox(I18n::tr("cld_compare"), body);
    m_autoDownload = new QCheckBox(I18n::tr("cld_download"), body);
    m_autoDownload->setChecked(true);
    m_notify = new QCheckBox(I18n::tr("cld_notify"), body);
    m_notify->setChecked(true);
    bv->addWidget(m_compareLocal);
    bv->addWidget(m_autoDownload);
    bv->addWidget(m_notify);

    bv->addStretch(1);
    v->addWidget(body, 1);

    // ── フッタ ──────────────────────────────────────────────────────────────
    auto *foot = new QWidget(this);
    auto *h = new QHBoxLayout(foot);
    h->setContentsMargins(12, 8, 12, 8);
    h->addStretch(1);
    auto *cancel = new QPushButton(I18n::tr("cld_cancel"), foot);
    auto *submit = new QPushButton(I18n::tr("cld_submit"), foot);
    submit->setDefault(true);
    h->addWidget(cancel);
    h->addWidget(submit);
    v->addWidget(foot);

    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(submit, &QPushButton::clicked, this, [this] {
        // ジョブ名を tidy3d プロジェクト名として永続化してから送信
        const QString job = m_jobName->text().trimmed();
        if (!job.isEmpty() && job != m_p->tidy3d().projectName) {
            m_p->tidy3d().projectName = job;
            m_p->touch();
        }
        emit submitted();
        accept();
    });
    connect(project, &Project::loaded, this, [this] { refresh(); });

    refresh();
}

void CloudDialog::showEvent(QShowEvent *e)
{
    refresh();                 // 開くたびに現在の Project 設定を反映
    QDialog::showEvent(e);
}

void CloudDialog::refresh()
{
    const Tidy3dOpts &t3 = m_p->tidy3d();
    const GeneralOpts &g = m_p->general();

    // ジョブ名: tidy3d プロジェクト名 → 無ければタイトル → モック既定値
    QString job = t3.projectName.trimmed();
    if (job.isEmpty() || job == QStringLiteral("openfdtd-x")) {
        job = g.title.trimmed();
        if (job.isEmpty()) job = QString::fromUtf8(kDefaultJob);
        else job = job.simplified().replace(' ', '_') + QStringLiteral("_run01");
    }
    m_jobName->setText(job);

    m_resolution->setText(t3.resolution == QStringLiteral("coarse")
                              ? I18n::tr("cld_res_coarse")
                          : t3.resolution == QStringLiteral("fine")
                              ? I18n::tr("cld_res_fine")
                              : I18n::tr("cld_res_medium"));

    m_pml->setText(t3.autoPml
        ? I18n::tr("cld_pml_auto").arg(kCloudPmlAuto)
        : I18n::tr("cld_pml_manual").arg(g.pmlL));
}
