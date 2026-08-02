// DomainBar.cpp
#include "DomainBar.h"
#include "I18n.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QLabel>
#include <QSettings>
#include <QToolButton>

using namespace ofd;

namespace {
const bool s_i18n = [] {
    // 判定は QSettings の API キー有無のみ。「接続中」は疎通確認を連想させる
    // ためラベル自体を実態に合わせる (tooltip 任せにしない)。
    ofd::I18n::reg("db_t3d_on",  "tidy3d: APIキー設定済み",
                                 "tidy3d: API key set");
    ofd::I18n::reg("db_t3d_off", "tidy3d: APIキー未設定",
                                 "tidy3d: API key not set");
    ofd::I18n::reg("db_t3d_tip",
        "QSettings の API キー設定有無のみを表示します — "
        "実際の API 疎通確認は行いません",
        "Shows only whether an API key is configured in QSettings — "
        "no real API connectivity check is performed");
    return true;
}();
} // namespace

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

    // tidy3d 接続ピル (右端) — 光ドメイン専用のクラウドバックエンド表示。
    // QSettings "tidy3d/apiKey" の有無だけを表示する (実 API 疎通なし —
    // tooltip で明示。CLAUDE.md 絶対規則 5)。
    m_tidy3dPill = new QLabel(this);
    m_tidy3dPill->setToolTip(I18n::tr("db_t3d_tip"));
    m_tidy3dPill->setVisible(false);
    h->addWidget(m_tidy3dPill);

    if (!m_buttons.isEmpty()) m_buttons.first().btn->setChecked(true);
    refreshTidy3dPill(m_buttons.isEmpty() ? Domain::EM : m_buttons.first().d);
}

void DomainBar::setActiveDomain(Domain d) {
    for (auto &e : m_buttons) e.btn->setChecked(e.d == d);
    refreshTidy3dPill(d);
}

// 光ドメインのときだけ表示し、APIキー設定の有無で 接続中/未接続 を切替える
void DomainBar::refreshTidy3dPill(Domain d)
{
    const bool optical = (d == Domain::Optical);
    m_tidy3dPill->setVisible(optical);
    if (!optical) return;
    const bool hasKey =
        !QSettings().value("tidy3d/apiKey").toString().trimmed().isEmpty();
    m_tidy3dPill->setText(I18n::tr(hasKey ? "db_t3d_on" : "db_t3d_off"));
    m_tidy3dPill->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; border: 1px solid %1; border-radius: 9px;"
        "  padding: 1px 10px; font-weight: 600; }")
        .arg(hasKey ? QStringLiteral("#2E8B57") : QStringLiteral("#888888")));
}
