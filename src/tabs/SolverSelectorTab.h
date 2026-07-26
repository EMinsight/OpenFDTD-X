// SolverSelectorTab.h — ソルバ詳細タブ (optics-tabs.jsx SolverSelectorTab 相当)。
//   CST Studio 風のソルバ選択カード一覧 (現在ドメインでフィルタ・推奨バッジ)
//   + ドメイン別の自動選定ヒント。表示専用 (状態は持たない)。
#pragma once
#include <QScrollArea>

class QGridLayout;
class QVBoxLayout;

namespace ofd {

class Project;
class SectionBox;

class SolverSelectorTab : public QScrollArea {
    Q_OBJECT
public:
    explicit SolverSelectorTab(Project *project, QWidget *parent = nullptr);

private slots:
    void rebuild();               // ドメイン変更 → カード・ヒントを再構築

private:
    Project     *m_p;
    SectionBox  *m_cardSection;
    QGridLayout *m_cardGrid;
    SectionBox  *m_hintSection;
    QVBoxLayout *m_hintBox;
};

} // namespace ofd
