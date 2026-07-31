// UnitNav.cpp
#include "UnitNav.h"
#include "../I18n.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QToolButton>

using namespace ofd;

namespace {
const bool s_i18n = [] {
    ofd::I18n::reg("un_first", "最初のユニットへ", "First unit");
    ofd::I18n::reg("un_last",  "最後のユニットへ", "Last unit");
    ofd::I18n::reg("un_goto",  "ユニット番号を入力して移動",
                   "Type a unit number to jump");
    return true;
}();
} // namespace

UnitNav::UnitNav(QWidget *parent)
    : QWidget(parent)
{
    auto *h = new QHBoxLayout(this);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(4);

    m_first = new QToolButton(this);
    m_first->setText(QStringLiteral("|◀"));
    m_first->setToolTip(I18n::tr("un_first"));
    m_prev = new QToolButton(this);
    m_prev->setArrowType(Qt::LeftArrow);
    m_next = new QToolButton(this);
    m_next->setArrowType(Qt::RightArrow);
    m_last = new QToolButton(this);
    m_last->setText(QStringLiteral("▶|"));
    m_last->setToolTip(I18n::tr("un_last"));
    m_label = new QLabel("- / -", this);
    m_label->setMinimumWidth(56);
    m_label->setAlignment(Qt::AlignCenter);
    m_num = new QSpinBox(this);
    m_num->setRange(1, 1);
    m_num->setToolTip(I18n::tr("un_goto"));
    m_num->setKeyboardTracking(false);   // 入力確定時のみ移動
    m_num->setMaximumWidth(64);

    h->addWidget(m_first);
    h->addWidget(m_prev);
    h->addWidget(m_label);
    h->addWidget(m_next);
    h->addWidget(m_last);
    h->addWidget(m_num);

    auto jump = [this](int index) {
        if (m_count <= 0) return;
        const int to = qBound(0, index, m_count - 1);
        if (to == m_index) return;
        setCurrent(to);
        emit currentChanged(m_index);
    };
    connect(m_first, &QToolButton::clicked, this, [jump] { jump(0); });
    connect(m_prev, &QToolButton::clicked, this,
            [this, jump] { jump(m_index - 1); });
    connect(m_next, &QToolButton::clicked, this,
            [this, jump] { jump(m_index + 1); });
    connect(m_last, &QToolButton::clicked, this,
            [this, jump] { jump(m_count - 1); });
    connect(m_num, &QSpinBox::valueChanged, this,
            [jump](int v) { jump(v - 1); });
    refresh();
}

void UnitNav::setRange(int count)
{
    m_count = count;
    if (m_index >= count) m_index = count - 1;
    if (m_index < 0 && count > 0) m_index = 0;
    refresh();
}

void UnitNav::setCurrent(int index)
{
    m_index = qBound(-1, index, m_count - 1);
    refresh();
}

void UnitNav::refresh()
{
    m_label->setText(m_count > 0
        ? QStringLiteral("%1 / %2").arg(m_index + 1).arg(m_count)
        : QStringLiteral("- / -"));
    m_first->setEnabled(m_index > 0);
    m_prev->setEnabled(m_index > 0);
    m_next->setEnabled(m_index + 1 < m_count);
    m_last->setEnabled(m_index + 1 < m_count);
    {
        QSignalBlocker b(m_num);
        m_num->setRange(1, qMax(1, m_count));
        m_num->setValue(qMax(1, m_index + 1));
    }
    m_num->setEnabled(m_count > 0);
}
