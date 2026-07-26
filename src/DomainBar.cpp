// DomainBar.cpp
#include "DomainBar.h"
#include "I18n.h"

#include <QHBoxLayout>
#include <QToolButton>
#include <QButtonGroup>

using namespace ofd;

DomainBar::DomainBar(QWidget *parent)
    : QWidget(parent)
    , m_group(new QButtonGroup(this))
{
    setObjectName("DomainBar");
    setFixedHeight(30);
    // 素の QWidget 派生では QSS の background が無視されるため明示的に有効化する
    // (これが無いと palette の明色で塗られ、QSS の明色文字と同系色になって読めない)
    setAttribute(Qt::WA_StyledBackground, true);

    auto *h = new QHBoxLayout(this);
    h->setContentsMargins(8, 0, 8, 0);
    h->setSpacing(0);

    const struct { Domain d; const char *labelKey; const char *glyph; } items[] = {
        { Domain::EM,         "d_em",         "⚡" },
        { Domain::Optical,    "d_optical",    "✦" },
        { Domain::Acoustic,   "d_acoustic",   "♪" },
        { Domain::Underwater, "d_underwater", "≋" },
    };

    int id = 0;
    for (const auto &it : items) {
        auto *btn = new QToolButton(this);
        btn->setText(QStringLiteral("%1 %2")
                     .arg(QString::fromUtf8(it.glyph), I18n::tr(it.labelKey)));
        btn->setCheckable(true);
        btn->setAutoRaise(true);
        btn->setCursor(Qt::PointingHandCursor);
        // 選択時の上罫線だけドメイン色を指定する。地色/hover はアプリ全体の
        // QSS (Theme) に任せる — ここで palette(...) を焼くとダーク/Scientific
        // でウィジェット単位の指定がアプリ QSS に勝ってしまい明色のまま残る。
        btn->setObjectName("domainTab");
        btn->setProperty("accent", accentColor(it.d));
        btn->setStyleSheet(QStringLiteral(
            "QToolButton { padding: 5px 14px; border: none;"
            "  border-top: 2px solid transparent; }"
            "QToolButton:checked { border-top-color: %1; font-weight: 600; }")
            .arg(accentColor(it.d)));
        h->addWidget(btn);
        m_group->addButton(btn, id++);
        m_buttons.push_back({ it.d, btn });
        connect(btn, &QToolButton::clicked, this, [this, d = it.d] {
            emit domainSelected(d);
        });
    }
    h->addStretch(1);

    if (!m_buttons.isEmpty()) m_buttons.first().btn->setChecked(true);
}

void DomainBar::setActiveDomain(Domain d) {
    for (auto &e : m_buttons) e.btn->setChecked(e.d == d);
}
