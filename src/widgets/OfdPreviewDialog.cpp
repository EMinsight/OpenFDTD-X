// OfdPreviewDialog.cpp — 保存内容のプレビュー (OfdPreviewDialog.h 参照)
#include "OfdPreviewDialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextEdit>
#include <QVBoxLayout>

#include "../I18n.h"
#include "../core/Project.h"
#include "../io/OfdPreview.h"

using namespace ofd;

namespace {

void registerStrings()
{
    static bool done = false;
    if (done) return;
    done = true;
    I18n::reg("ofdprev_title", "保存内容のプレビュー (.ofd / .ofdx)",
              "Preview of what will be saved (.ofd / .ofdx)");
    I18n::reg("ofdprev_intro",
              "保存したときにファイルへ書かれる中身です。保存経路と同じ関数で"
              "作っているので、ここに見えるものがそのまま書かれます。"
              "この画面ではファイルを変更しません。",
              "This is what will be written when you save. It is produced by "
              "the same code path as saving, so what you see here is what gets "
              "written. This dialog does not modify any file.");
    I18n::reg("ofdprev_tab_ofd",  ".ofd (カーネル入力)", ".ofd (kernel input)");
    I18n::reg("ofdprev_tab_ofdx", ".ofdx (JSON サイドカー)", ".ofdx (JSON sidecar)");
    I18n::reg("ofdprev_mark_extra", "GUI が知らないキーを強調",
              "Highlight keys the GUI does not model");
    I18n::reg("ofdprev_extra_n",
              "強調中の %1 行は、読み込んだファイルにあって GUI が扱わないキーです。"
              "保存時にそのまま書き戻されます。",
              "The %1 highlighted lines are keys that were in the loaded file "
              "and that the GUI does not model. They are written back as is.");
    I18n::reg("ofdprev_extra_none",
              "GUI が知らないキーはありません (この .ofd は全て GUI が扱う範囲です)。",
              "There are no keys outside what the GUI models in this .ofd.");
    I18n::reg("ofdprev_summary", "%1 行 / %2 バイト", "%1 lines / %2 bytes");
    I18n::reg("ofdprev_eol",
              "改行は保存時に OS の流儀へ変換されます (Windows は CRLF) ので、"
              "実際のファイルサイズは上の値と異なることがあります。",
              "Line endings are converted to the platform convention when "
              "saving (CRLF on Windows), so the file size can differ from the "
              "value above.");
    I18n::reg("ofdprev_copy", "表示中のタブをコピー", "Copy the visible tab");
    I18n::reg("ofdprev_copied", "コピーしました", "Copied");
}

QPlainTextEdit *makeView(QWidget *parent)
{
    auto *e = new QPlainTextEdit(parent);
    e->setReadOnly(true);
    e->setLineWrapMode(QPlainTextEdit::NoWrap);
    e->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    return e;
}

} // namespace

namespace ofd {

OfdPreviewDialog::OfdPreviewDialog(const Project *project, QWidget *parent)
    : QDialog(parent), m_project(project)
{
    registerStrings();
    setWindowTitle(I18n::tr("ofdprev_title"));
    resize(760, 620);

    auto *v = new QVBoxLayout(this);

    auto *intro = new QLabel(I18n::tr("ofdprev_intro"), this);
    intro->setWordWrap(true);
    intro->setStyleSheet("color:#555; font-size:11px;");
    v->addWidget(intro);

    auto *tabs = new QTabWidget(this);
    m_ofd  = makeView(tabs);
    m_ofdx = makeView(tabs);
    tabs->addTab(m_ofd,  I18n::tr("ofdprev_tab_ofd"));
    tabs->addTab(m_ofdx, I18n::tr("ofdprev_tab_ofdx"));
    v->addWidget(tabs, 1);

    auto *row = new QHBoxLayout();
    m_markExtra = new QCheckBox(I18n::tr("ofdprev_mark_extra"), this);
    m_markExtra->setChecked(true);
    connect(m_markExtra, &QCheckBox::toggled,
            this, &OfdPreviewDialog::applyExtraHighlight);
    row->addWidget(m_markExtra);
    row->addStretch(1);
    m_summary = new QLabel(this);
    m_summary->setStyleSheet("color:#555; font-size:11px;");
    row->addWidget(m_summary);
    v->addLayout(row);

    m_extraNote = new QLabel(this);
    m_extraNote->setWordWrap(true);
    m_extraNote->setStyleSheet("color:#555; font-size:11px;");
    v->addWidget(m_extraNote);

    auto *eol = new QLabel(I18n::tr("ofdprev_eol"), this);
    eol->setWordWrap(true);
    eol->setStyleSheet("color:#888; font-size:10px;");
    v->addWidget(eol);

    auto *bb = new QDialogButtonBox(QDialogButtonBox::Close, this);
    auto *copy = bb->addButton(I18n::tr("ofdprev_copy"),
                               QDialogButtonBox::ActionRole);
    connect(copy, &QPushButton::clicked, this, [this, tabs, copy] {
        const QString text = (tabs->currentIndex() == 0)
            ? m_ofd->toPlainText() : m_ofdx->toPlainText();
        QApplication::clipboard()->setText(text);
        copy->setText(I18n::tr("ofdprev_copied"));
    });
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    v->addWidget(bb);

    rebuild();
}

void OfdPreviewDialog::rebuild()
{
    if (!m_project) return;
    const OfdPreviewText t = buildOfdPreview(*m_project);

    m_ofd->setPlainText(t.ofd);
    m_ofdx->setPlainText(QString::fromUtf8(t.ofdx));
    m_extraRows = t.extraRows;

    m_summary->setText(I18n::tr("ofdprev_summary")
                           .arg(t.ofdRows).arg(t.ofdBytes));
    m_extraNote->setText(m_extraRows.isEmpty()
        ? I18n::tr("ofdprev_extra_none")
        : I18n::tr("ofdprev_extra_n").arg(m_extraRows.size()));
    applyExtraHighlight();
}

// 強調は ExtraSelection (文書は書き換えない — 読み取り専用のまま)
void OfdPreviewDialog::applyExtraHighlight()
{
    QList<QTextEdit::ExtraSelection> sel;
    if (m_markExtra && m_markExtra->isChecked()) {
        for (int row : m_extraRows) {
            QTextCursor c(m_ofd->document()->findBlockByNumber(row));
            if (c.isNull()) continue;
            QTextEdit::ExtraSelection s;
            s.cursor = c;
            s.format.setBackground(QColor("#FFF3C4"));
            s.format.setProperty(QTextFormat::FullWidthSelection, true);
            sel.push_back(s);
        }
    }
    m_ofd->setExtraSelections(sel);
}

} // namespace ofd
