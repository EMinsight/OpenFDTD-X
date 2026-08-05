// SolverSelectorTab.h — ソルバ詳細タブ (optics-tabs.jsx SolverSelectorTab 相当)。
//   CST Studio 風のソルバ選択カード一覧 (現在ドメインでフィルタ・推奨バッジ)
//   + ドメイン別の自動選定ヒント。
// 選定ヒントの数値 (L/λ・λ/Δx・分解できる Q の上限・Schroeder 周波数・
// Thorp 吸収) は core/SolverSelection の式にプロジェクト設定を入れて算出する
// (固定値ではない)。カード自体は表示専用 (選択状態は持たない)。
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
    void refreshHints();          // 設定変更 → 目安の数値だけ再算出

private:
    void rebuildCards();          // ドメインで絞ったソルバカード一覧

    Project     *m_p;
    SectionBox  *m_cardSection;
    QGridLayout *m_cardGrid;
    SectionBox  *m_hintSection;
    QVBoxLayout *m_hintBox;
};

} // namespace ofd
