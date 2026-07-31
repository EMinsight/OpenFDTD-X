// UnitNav.h — "ユニット番号" navigator (|◀ ◀ n / total ▶ ▶| + 番号入力),
// as in the 本家 GUI + mock tabs.jsx の UnitNav。
#pragma once
#include <QWidget>

class QToolButton;
class QLabel;
class QSpinBox;

namespace ofd {

class UnitNav : public QWidget {
    Q_OBJECT
public:
    explicit UnitNav(QWidget *parent = nullptr);

    void setRange(int count);          // 1..count, empty when count == 0
    void setCurrent(int index);        // 0-based
    int  current() const { return m_index; }

signals:
    void currentChanged(int index);    // 0-based

private:
    void refresh();

    QToolButton *m_first;              // |◀ 先頭へ
    QToolButton *m_prev;
    QToolButton *m_next;
    QToolButton *m_last;               // ▶| 末尾へ
    QLabel      *m_label;
    QSpinBox    *m_num;                // 番号入力 (1-based) + 移動
    int          m_index = -1;
    int          m_count = 0;
};

} // namespace ofd
