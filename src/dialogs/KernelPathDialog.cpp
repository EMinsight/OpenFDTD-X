// KernelPathDialog.cpp
#include "KernelPathDialog.h"
#include "../I18n.h"
#include "../kernel/AcousticRunner.h"

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
    // ドメイン見出し (行がどのドメインで使われるかを示す)
    ofd::I18n::reg("kp_grp_em",       "電磁波", "Electromagnetic");
    ofd::I18n::reg("kp_grp_optical",  "光",     "Optical");
    ofd::I18n::reg("kp_grp_acoustic", "室内音響", "Room acoustics");
    ofd::I18n::reg("kp_grp_uw",       "水中音響", "Underwater acoustics");
    ofd::I18n::reg("kp_grp_active",   " (現在のドメイン)", " (current domain)");
    // 室内音響: 外部ソルバーは実行ファイル指定 + プロジェクト個別指定が優先
    ofd::I18n::reg("kp_acoustic", "外部音響ソルバー", "External acoustic solver");
    ofd::I18n::reg("kp_acoustic_ph", "$OFDX_ACOUSTIC_SOLVER",
                   "$OFDX_ACOUSTIC_SOLVER");
    ofd::I18n::reg("kp_acoustic_note",
        "▸ 室内音響だけはディレクトリではなく実行ファイルを指定します "
        "(探索名がバックエンドで変わるため)。ここは全プロジェクト共通の既定で、"
        "プロジェクト個別の指定 (音響ソルバ連携タブ) があればそちらが優先されます。"
        "外部ソルバー本体は別リポジトリで開発中です。",
        "Room acoustics takes an executable, not a directory (the binary name "
        "depends on the backend). This is the default shared by every project; "
        "a per-project path (Acoustic solver tab) takes precedence. The "
        "external solver itself is developed in a separate repository.");
    ofd::I18n::reg("kp_browse_file", "実行ファイルを選択",
                   "Choose solver executable");
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

const char *groupLabelKey(Domain d)
{
    switch (d) {
        case Domain::EM:         return "kp_grp_em";
        case Domain::Optical:    return "kp_grp_optical";
        case Domain::Acoustic:   return "kp_grp_acoustic";
        case Domain::Underwater: return "kp_grp_uw";
    }
    return "kp_grp_em";
}
} // namespace

KernelPathDialog::KernelPathDialog(QWidget *parent, Domain activeDomain,
                                   bool markActiveDomain)
    : QDialog(parent)
{
    setWindowTitle(I18n::tr("kp_title"));
    setModal(true);
    setMinimumWidth(660);

    // ドメイン順に並べる (電磁 → 光 2 本 → 室内音響 → 水中音響)
    const struct { Domain domain; bool acoustic; Kernel kernel; } kDefs[] = {
        { Domain::EM,         false, Kernel::FDTD    },
        { Domain::Optical,    false, Kernel::RCWA    },
        { Domain::Optical,    false, Kernel::BPM     },
        { Domain::Acoustic,   true,  Kernel::FDTD    },   // kernel は未使用
        { Domain::Underwater, false, Kernel::Bellhop },
    };
    for (const auto &d : kDefs) {
        Row r;
        r.domain = d.domain;
        r.acoustic = d.acoustic;
        r.kernel = d.kernel;
        m_rows.push_back(r);
    }

    auto *v = new QVBoxLayout(this);
    auto *hint = new QLabel(I18n::tr("kp_hint"), this);
    hint->setWordWrap(true);
    hint->setStyleSheet("font-size:11px; color:palette(mid);");
    v->addWidget(hint);

    auto *form = new QFormLayout();
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    v->addLayout(form);

    Domain lastGroup = Domain::EM;
    bool haveGroup = false;
    for (int i = 0; i < m_rows.size(); ++i) {
        Row &row = m_rows[i];

        // ドメインが変わったら見出しを挟む (タブで隠さず 1 画面に並べる)
        if (!haveGroup || row.domain != lastGroup) {
            QString text = I18n::tr(groupLabelKey(row.domain));
            if (markActiveDomain && row.domain == activeDomain)
                text += I18n::tr("kp_grp_active");
            auto *head = new QLabel(text, this);
            QFont f = head->font();
            f.setBold(true);
            head->setFont(f);
            const bool active = markActiveDomain && row.domain == activeDomain;
            head->setStyleSheet(active
                ? QStringLiteral("color:%1; margin-top:6px;").arg(accentColor(row.domain))
                : QStringLiteral("margin-top:6px;"));
            form->addRow(head);
            lastGroup = row.domain;
            haveGroup = true;
        }

        auto *cell = new QVBoxLayout();
        auto *h = new QHBoxLayout();
        row.dir = new QLineEdit(
            row.acoustic ? AcousticRunner::solverPathSetting()
                         : Runner::kernelDirSetting(row.kernel), this);
        row.dir->setPlaceholderText(
            row.acoustic ? I18n::tr("kp_acoustic_ph")
                         : QLatin1String(Runner::homeVarFor(row.kernel)));
        h->addWidget(row.dir, 1);
        auto *browse = new QPushButton(I18n::tr("kp_browse"), this);
        connect(browse, &QPushButton::clicked, this, [this, i] {
            Row &r = m_rows[i];
            // 室内音響は実行ファイル、他はディレクトリを選ばせる
            const QString picked = r.acoustic
                ? QFileDialog::getOpenFileName(this,
                      I18n::tr("kp_browse_file"), r.dir->text())
                : QFileDialog::getExistingDirectory(this,
                      I18n::tr("kp_title"), r.dir->text());
            if (!picked.isEmpty()) r.dir->setText(picked);
        });
        h->addWidget(browse);
        cell->addLayout(h);
        row.status = new QLabel(this);
        row.status->setStyleSheet("font-size:11px;");
        cell->addWidget(row.status);
        form->addRow(row.acoustic ? I18n::tr("kp_acoustic")
                                  : I18n::tr(rowLabelKey(row.kernel)), cell);

        connect(row.dir, &QLineEdit::textChanged, this,
                [this, i] { updateStatus(i); });
        updateStatus(i);

        // 室内音響の但し書き (指定の粒度と優先順位が他と違うため)
        if (row.acoustic) {
            auto *note = new QLabel(I18n::tr("kp_acoustic_note"), this);
            note->setWordWrap(true);
            note->setStyleSheet("font-size:11px; color:palette(mid);");
            form->addRow(note);
        }
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
    QString bin;
    if (row.acoustic) {
        // 入力欄の値を「明示指定」として解決する (保存前でも結果が見える)。
        // 空欄なら現在の設定・環境変数・kernel/・PATH の探索結果を出す。
        AcousticRunConfig cfg;
        cfg.executable = row.dir->text().trimmed();
        bin = AcousticRunner::resolveSolver(cfg);
    } else {
        RunConfig cfg;
        cfg.kernel = row.kernel;
        cfg.binaryDir = row.dir->text().trimmed();
        bin = Runner::resolvedSolverPath(cfg);
    }
    if (!bin.isEmpty()) {
        row.status->setText(I18n::tr("kp_found").arg(bin));
        row.status->setStyleSheet("font-size:11px; color:#0F7B0F;");
    } else {
        row.status->setText(I18n::tr("kp_notfound"));
        row.status->setStyleSheet("font-size:11px; color:#9D5D00;");
    }
}

void KernelPathDialog::saveAll()
{
    for (const Row &row : m_rows) {
        const QString value = row.dir->text().trimmed();
        if (row.acoustic) AcousticRunner::setSolverPathSetting(value);
        else              Runner::setKernelDirSetting(row.kernel, value);
    }
}
