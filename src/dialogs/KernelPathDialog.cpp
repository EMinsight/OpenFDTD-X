// KernelPathDialog.cpp
#include "KernelPathDialog.h"
#include "../I18n.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStandardPaths>
#include <QVBoxLayout>

using namespace ofd;

namespace {
const bool s_i18n = [] {
    ofd::I18n::reg("kp_title", "カーネルパスの設定", "Kernel paths");
    ofd::I18n::reg("kp_hint",
        "各ソルバーカーネルのビルド先 (リポジトリルート) を指定します。"
        "直下と bin/ の両方が探索されます。空欄なら環境変数 "
        "(OPENFDTD_HOME 等) と PATH で探索します。\n"
        "この設定はアプリ再起動後も保持され、Finder / Dock からの起動でも"
        "有効です (環境変数は届きません)。",
        "Set the build location (repository root) of each solver kernel. "
        "Both the folder itself and its bin/ are searched. Leave empty to "
        "fall back to the environment variables (OPENFDTD_HOME etc.) and "
        "PATH.\nThe setting persists across restarts and also applies when "
        "the app is launched from Finder / Dock (where environment "
        "variables do not reach).");
    ofd::I18n::reg("kp_em",       "電磁 FDTD (ofd)",        "EM FDTD (ofd)");
    ofd::I18n::reg("kp_rcwa",     "光 RCWA/FMM (orcwa)",    "Optical RCWA/FMM (orcwa)");
    ofd::I18n::reg("kp_bpm",      "光 BPM (obpm)",          "Optical BPM (obpm)");
    ofd::I18n::reg("kp_bellhop",  "水中音響 (bellhopcxx)",  "Underwater (bellhopcxx)");
    ofd::I18n::reg("kp_browse",   "参照…",                  "Browse…");
    ofd::I18n::reg("kp_found",    "✓ %1",                   "✓ %1");
    ofd::I18n::reg("kp_notfound", "未検出 (環境変数・PATH でも見つかりません)",
                   "Not found (also missing from env vars and PATH)");
    return true;
}();

// 行の表示ラベル
const char *rowLabelKey(Kernel k)
{
    switch (k) {
        case Kernel::FDTD:    return "kp_em";
        case Kernel::RCWA:    return "kp_rcwa";
        case Kernel::BPM:     return "kp_bpm";
        case Kernel::Bellhop: return "kp_bellhop";
    }
    return "kp_em";
}
} // namespace

KernelPathDialog::KernelPathDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(I18n::tr("kp_title"));
    setModal(true);
    setMinimumWidth(640);

    m_rows[0].kernel = Kernel::FDTD;
    m_rows[1].kernel = Kernel::RCWA;
    m_rows[2].kernel = Kernel::BPM;
    m_rows[3].kernel = Kernel::Bellhop;

    auto *v = new QVBoxLayout(this);
    auto *hint = new QLabel(I18n::tr("kp_hint"), this);
    hint->setWordWrap(true);
    hint->setStyleSheet("font-size:11px; color:palette(mid);");
    v->addWidget(hint);

    auto *form = new QFormLayout();
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    v->addLayout(form);

    for (int i = 0; i < 4; ++i) {
        Row &row = m_rows[i];
        auto *cell = new QVBoxLayout();
        auto *h = new QHBoxLayout();
        row.dir = new QLineEdit(Runner::kernelDirSetting(row.kernel), this);
        row.dir->setPlaceholderText(
            QLatin1String(Runner::homeVarFor(row.kernel)));
        h->addWidget(row.dir, 1);
        auto *browse = new QPushButton(I18n::tr("kp_browse"), this);
        connect(browse, &QPushButton::clicked, this, [this, i] {
            const QString d = QFileDialog::getExistingDirectory(
                this, I18n::tr("kp_title"), m_rows[i].dir->text());
            if (!d.isEmpty()) m_rows[i].dir->setText(d);
        });
        h->addWidget(browse);
        cell->addLayout(h);
        row.status = new QLabel(this);
        row.status->setStyleSheet("font-size:11px;");
        cell->addWidget(row.status);
        form->addRow(I18n::tr(rowLabelKey(row.kernel)), cell);

        connect(row.dir, &QLineEdit::textChanged, this,
                [this, i] { updateStatus(i); });
        updateStatus(i);
    }

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        saveAll();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    v->addWidget(buttons);
}

// 入力中のディレクトリで実際にソルバーが見つかるかを表示する。
// resolveBinary と同じ規則 (入力欄 → 設定 → 環境変数 → PATH) なので、
// 空欄でも環境変数や PATH で見つかればそのパスが出る。
void KernelPathDialog::updateStatus(int i)
{
    Row &row = m_rows[i];
    RunConfig cfg;
    cfg.kernel = row.kernel;
    cfg.binaryDir = row.dir->text().trimmed();
    QString bin = Runner::solverBinary(cfg);
    bool found = QFileInfo::exists(bin);
    if (!found && !bin.contains(QLatin1Char('/'))) {
        // ディレクトリ探索で見つからず素の名前が返った場合は PATH を確認
        const QString onPath = QStandardPaths::findExecutable(bin);
        if (!onPath.isEmpty()) { bin = onPath; found = true; }
    }
    if (found) {
        row.status->setText(I18n::tr("kp_found").arg(bin));
        row.status->setStyleSheet("font-size:11px; color:#0F7B0F;");
    } else {
        row.status->setText(I18n::tr("kp_notfound"));
        row.status->setStyleSheet("font-size:11px; color:#9D5D00;");
    }
}

void KernelPathDialog::saveAll()
{
    for (const Row &row : m_rows)
        Runner::setKernelDirSetting(row.kernel, row.dir->text().trimmed());
}
