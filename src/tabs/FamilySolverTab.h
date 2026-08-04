// FamilySolverTab.h — 姉妹ソルバタブ (openfdtd-family.jsx FamilySolverTab 相当)。
//   OpenFDTD / OpenRTM / OpenTHFD / OpenMOM / OpenSTF / マイクロ波トモグラフィー
//   のカード選択 + ソルバ別詳細設定 + ソルバ間連携チェックリスト。
//   カードは現在のドメイン (EM / 光) でフィルタ表示される。
//   音響 / 水中ドメインでは姉妹ソルバが 1 枚も無いため、一覧・詳細・
//   ソルバ間連携のセクションごと非表示にする (空グリッドの混乱防止)。
#pragma once
#include <QScrollArea>
#include <QFrame>
#include <QVector>

class QGridLayout;
class QLabel;
class QMouseEvent;
class QStackedWidget;

namespace ofd {

class Project;
class SectionBox;

// 1ソルバ分のクリック可能カード (mock のカード <div> 相当)
class FamilyCard : public QFrame {
    Q_OBJECT
public:
    FamilyCard(const QString &name, const QString &ver, const QString &color,
               const QString &method, const QString &use,
               const QString &strengths, const QString &example,
               QWidget *parent = nullptr);
    void setSelected(bool on);

signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent *) override;

private:
    QString m_color;
    QLabel *m_badge;
};

class FamilySolverTab : public QScrollArea {
    Q_OBJECT
public:
    explicit FamilySolverTab(Project *project, QWidget *parent = nullptr);

private slots:
    void rebuildCards();          // ドメイン変更 → カード一覧を再構築

private:
    void select(int familyIndex);
    QWidget *buildFdtdPage();
    QWidget *buildRtmPage();
    QWidget *buildThfdPage();
    QWidget *buildMomPage();
    QWidget *buildStfPage();
    QWidget *buildTomoPage();

    Project        *m_p;
    int             m_pick = 0;           // kFamily[] index of the selection
    QGridLayout    *m_cardGrid;
    QVector<FamilyCard*> m_cards;         // visible cards (rebuild on domain)
    QVector<int>    m_cardIndex;          // card slot → kFamily[] index
    SectionBox     *m_listSection;        // 姉妹ソルバ一覧 (音響/水中では非表示)
    SectionBox     *m_detailSection;
    QStackedWidget *m_detailStack;
    SectionBox     *m_crossSection;       // ソルバ間連携 (音響/水中では非表示)
};

} // namespace ofd
