// KernelPathDialog.cpp
#include "KernelPathDialog.h"
#include "../I18n.h"
#include "../core/Project.h"
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
    ofd::I18n::reg("kp_em",       "OpenFDTD (ofd)",         "OpenFDTD (ofd)");
    ofd::I18n::reg("kp_rcwa",     "光 RCWA/FMM (orcwa)",    "Optical RCWA/FMM (orcwa)");
    ofd::I18n::reg("kp_bpm",      "光 BPM (obpm)",          "Optical BPM (obpm)");
    ofd::I18n::reg("kp_bellhop",  "水中音響 (bellhopcxx)",  "Underwater (bellhopcxx)");
    ofd::I18n::reg("kp_peec",     "PEEC 抽出 (peec)",       "PEEC extraction (peec)");
    ofd::I18n::reg("kp_fem",      "準静的 FEM 抽出 (ofe)",  "Quasi-static FEM (ofe)");
    ofd::I18n::reg("kp_grp_circuit", "回路パラメータ抽出", "Circuit parameter extraction");
    ofd::I18n::reg("kp_cir_note",
                   "R/L/C 抽出は姉妹リポジトリ OpenPEEC / OpenFEM のバイナリを"
                   "起動します (回路ソルバタブの「抽出実行」)。",
                   "R/L/C extraction launches the OpenPEEC / OpenFEM binaries "
                   "from the sibling repositories (Circuit solvers tab).");
    ofd::I18n::reg("kp_browse",   "参照…",                  "Browse…");
    ofd::I18n::reg("kp_found",    "✓ %1",                   "✓ %1");
    ofd::I18n::reg("kp_notfound", "未検出 (環境変数・PATH でも見つかりません)",
                   "Not found (also missing from env vars and PATH)");
    // 見出し: 基幹カーネルとドメイン専用ソルバを分ける
    // (ofd は電磁波専用ではない — 下の kp_em_note の 3 用途を参照)
    ofd::I18n::reg("kp_grp_core",     "基幹カーネル (全ドメイン共通)",
                                      "Core kernel (all domains)");
    ofd::I18n::reg("kp_grp_optical",  "光 — 専用ソルバ", "Optical — dedicated solvers");
    ofd::I18n::reg("kp_grp_acoustic", "室内音響 — 専用ソルバ",
                                      "Room acoustics — dedicated solver");
    ofd::I18n::reg("kp_grp_uw",       "水中音響", "Underwater acoustics");
    ofd::I18n::reg("kp_em_note",
        "▸ OpenFDTD は電磁波専用ではありません。<b>電磁波は常に</b>、"
        "<b>光はソルバに FDTD を選んだとき</b> (既定)、"
        "<b>室内音響の「計算」も</b>これを起動します "
        "(室内音響の専用ソルバは下の行)。無いと 3 ドメインで計算できません。",
        "OpenFDTD is not EM-only. It runs for <b>every EM project</b>, for "
        "<b>optical projects whose solver is FDTD</b> (the default), and for "
        "the <b>Run button in room acoustics</b> (the dedicated acoustic "
        "solver is listed below). Without it those three domains cannot "
        "compute.");
    ofd::I18n::reg("kp_opt_note",
        "▸ 光は「光解析」タブのソルバ選択で起動先が変わります "
        "(RCWA / FMM → orcwa、BPM → obpm、FDTD → 上の OpenFDTD)。",
        "The optical domain picks its kernel from the solver setting "
        "(RCWA / FMM → orcwa, BPM → obpm, FDTD → OpenFDTD above).");
    ofd::I18n::reg("kp_active", "▶ このプロジェクトはこれを起動します",
                   "▶ This project launches this one");
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
        case Kernel::PEEC:    return "kp_peec";
        case Kernel::FEM:     return "kp_fem";
    }
    return "kp_em";
}
} // namespace

KernelPathDialog::KernelPathDialog(QWidget *parent, const Project *project)
    : QDialog(parent)
{
    setWindowTitle(I18n::tr("kp_title"));
    setModal(true);
    setMinimumWidth(660);

    // 並びは「基幹カーネル」→ 光専用 → 室内音響専用 → 水中音響。
    // ofd をドメイン別に置くと「電磁波専用」と誤解されるため独立させ、
    // 実際の 3 用途を kp_em_note で明示する。
    const struct { const char *group, *note; bool acoustic; Kernel kernel; }
    kDefs[] = {
        { "kp_grp_core",     "kp_em_note",  false, Kernel::FDTD    },
        { "kp_grp_optical",  nullptr,       false, Kernel::RCWA    },
        { nullptr,           "kp_opt_note", false, Kernel::BPM     },
        { "kp_grp_acoustic", nullptr,       true,  Kernel::FDTD    }, // kernel 未使用
        { "kp_grp_uw",       nullptr,       false, Kernel::Bellhop },
        // 回路パラメータ抽出 (姉妹リポジトリ OpenPEEC / OpenFEM)
        { "kp_grp_circuit",  nullptr,       false, Kernel::PEEC    },
        { nullptr,           "kp_cir_note", false, Kernel::FEM     },
    };
    for (const auto &d : kDefs) {
        Row r;
        r.groupKey = d.group;
        r.noteKey = d.note;
        r.acoustic = d.acoustic;
        r.kernel = d.kernel;
        m_rows.push_back(r);
    }

    // このプロジェクトが実際に起動するカーネル (印を付ける行の判定)
    const bool haveProject = (project != nullptr);
    const Kernel activeKernel =
        haveProject ? Runner::kernelForProject(*project) : Kernel::FDTD;
    const bool activeIsAcoustic = false;   // 「計算」ボタンは常に Runner 側

    auto *v = new QVBoxLayout(this);
    auto *hint = new QLabel(I18n::tr("kp_hint"), this);
    hint->setWordWrap(true);
    hint->setStyleSheet("font-size:11px; color:palette(mid);");
    v->addWidget(hint);

    auto *form = new QFormLayout();
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    v->addLayout(form);

    for (int i = 0; i < m_rows.size(); ++i) {
        Row &row = m_rows[i];

        if (row.groupKey) {
            auto *head = new QLabel(I18n::tr(row.groupKey), this);
            QFont f = head->font();
            f.setBold(true);
            head->setFont(f);
            head->setStyleSheet("margin-top:6px;");
            form->addRow(head);
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

        // 現在のプロジェクトが起動する行に印を付ける (どれが要るのかを示す)
        if (haveProject && !row.acoustic && row.kernel == activeKernel
            && !activeIsAcoustic) {
            auto *act = new QLabel(I18n::tr("kp_active"), this);
            act->setStyleSheet(QStringLiteral(
                "font-size:11px; font-weight:600; color:%1;")
                    .arg(accentColor(project->activeDomain())));
            cell->addWidget(act);
        }

        form->addRow(row.acoustic ? I18n::tr("kp_acoustic")
                                  : I18n::tr(rowLabelKey(row.kernel)), cell);

        connect(row.dir, &QLineEdit::textChanged, this,
                [this, i] { updateStatus(i); });
        updateStatus(i);

        // 行の補足 (ofd の 3 用途 / 光のソルバ選択で起動先が変わること /
        // 室内音響の指定粒度)
        if (row.noteKey || row.acoustic) {
            auto *note = new QLabel(
                I18n::tr(row.acoustic ? "kp_acoustic_note" : row.noteKey), this);
            note->setWordWrap(true);
            note->setTextFormat(Qt::RichText);   // <b> を効かせる
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
