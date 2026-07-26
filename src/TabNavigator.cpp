// TabNavigator.cpp
#include "TabNavigator.h"
#include "I18n.h"

#include <QFont>

using namespace ofd;

TabNavigator::TabNavigator(QWidget *parent)
    : QListWidget(parent)
{
    setObjectName("tabNavigator");
    setSelectionMode(QAbstractItemView::SingleSelection);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setMinimumWidth(150);
    setMaximumWidth(210);
    setUniformItemSizes(false);
    connect(this, &QListWidget::currentItemChanged, this, [this] { emitCurrent(); });
}

void TabNavigator::addEntry(const Entry &e)
{
    m_entries.push_back(e);
}

void TabNavigator::rebuild(Domain d, bool expert)
{
    const QString keep = currentKey();
    blockSignals(true);
    clear();

    QString lastCat;
    for (const Entry &e : m_entries) {
        if (!e.core && !expert) continue;
        if (!e.domains.isEmpty() && !e.domains.contains(d)) continue;

        if (e.categoryKey != lastCat) {
            lastCat = e.categoryKey;
            auto *cat = new QListWidgetItem(I18n::tr(e.categoryKey), this);
            cat->setFlags(Qt::NoItemFlags);          // 見出し: 選択不可
            QFont f = cat->font();
            f.setPointSizeF(f.pointSizeF() - 1.5);
            f.setBold(true);
            f.setCapitalization(QFont::AllUppercase);
            cat->setFont(f);
            cat->setForeground(palette().brush(QPalette::Disabled, QPalette::Text));
        }
        auto *it = new QListWidgetItem("  " + I18n::tr(e.labelKey), this);
        it->setData(Qt::UserRole, e.key);
    }
    blockSignals(false);

    if (!keep.isEmpty() && selectKey(keep)) return;
    // 先頭の選択可能項目へ
    for (int i = 0; i < count(); ++i)
        if (item(i)->flags() & Qt::ItemIsSelectable) {
            setCurrentRow(i);
            return;
        }
}

QString TabNavigator::currentKey() const
{
    auto *it = currentItem();
    return it ? it->data(Qt::UserRole).toString() : QString();
}

bool TabNavigator::selectKey(const QString &key)
{
    for (int i = 0; i < count(); ++i)
        if (item(i)->data(Qt::UserRole).toString() == key) {
            setCurrentRow(i);
            return true;
        }
    return false;
}

bool TabNavigator::selectByLabel(const QString &part)
{
    for (int i = 0; i < count(); ++i)
        if ((item(i)->flags() & Qt::ItemIsSelectable) &&
            item(i)->text().contains(part, Qt::CaseInsensitive)) {
            setCurrentRow(i);
            return true;
        }
    return false;
}

void TabNavigator::emitCurrent()
{
    const QString key = currentKey();
    if (key.isEmpty()) return;
    for (const Entry &e : m_entries)
        if (e.key == key) {
            emit pageSelected(e.page);
            return;
        }
}
