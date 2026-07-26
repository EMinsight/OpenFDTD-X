// RunDialog.cpp
#include "RunDialog.h"
#include "../I18n.h"
#include "../kernel/Runner.h"
#include "../widgets/LogConsole.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

using namespace ofd;

namespace {
const bool s_i18n = [] {
    ofd::I18n::reg("rund_pause",  "⏸ 一時停止", "⏸ Pause");
    ofd::I18n::reg("rund_stop",   "■ 停止",     "■ Stop");
    ofd::I18n::reg("rund_close",  "閉じる",     "Close");
    return true;
}();
} // namespace

RunDialog::RunDialog(Runner *runner, QWidget *parent)
    : QDialog(parent), m_runner(runner)
{
    setWindowTitle(I18n::tr("run_console") + " — OpenFDTD");
    setModal(false);                       // 計算を見ながら他操作も可
    resize(640, 420);

    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    m_log = new LogConsole(this);
    v->addWidget(m_log, 1);

    auto *foot = new QWidget(this);
    auto *h = new QHBoxLayout(foot);
    h->setContentsMargins(10, 8, 10, 8);
    m_pause = new QPushButton(I18n::tr("rund_pause"), foot);
    m_pause->setEnabled(false);            // カーネルに一時停止 API が無いため
    m_stop = new QPushButton(I18n::tr("rund_stop"), foot);
    auto *close = new QPushButton(I18n::tr("rund_close"), foot);
    close->setDefault(true);
    h->addWidget(m_pause);
    h->addWidget(m_stop);
    h->addStretch(1);
    h->addWidget(close);
    v->addWidget(foot);

    connect(m_stop, &QPushButton::clicked, this, [this] { m_runner->stop(); });
    connect(close, &QPushButton::clicked, this, &QDialog::hide);
    connect(m_runner, &Runner::finished, this, [this](bool) {
        m_stop->setEnabled(false);
    });
    connect(m_runner, &Runner::started, this, [this] {
        m_stop->setEnabled(true);
    });
}

void RunDialog::appendLine(const QString &line) { m_log->appendLine(line); }

void RunDialog::clearLog() { m_log->clear(); }
