// ComponentsTab.h — コンポーネントライブラリ (ansys-tabs.jsx ComponentsTab 相当)。
// Ansys Lumerical 流のドラッグ&ドロップパレット:
//   検索 + カテゴリフィルタ + ドメイン別コンポーネントグリッド + 最近使用。
// カテゴリの表示/優先順はアクティブドメイン (em/optical/acoustic/underwater)
// に応じて切り替わる。配置そのもの (drag & drop) はオーケストレータ側で配線。
// お気に入り (cl_favorites) はカードの ☆ で開閉するローカル状態のみ (Project 非依存)。
#pragma once
#include <QScrollArea>
#include <QStringList>
#include <QVector>

class QGridLayout;
class QHBoxLayout;
class QLineEdit;
class QPushButton;

namespace ofd {

class Project;
class SectionBox;

class ComponentsTab : public QScrollArea {
    Q_OBJECT
public:
    explicit ComponentsTab(Project *project, QWidget *parent = nullptr);

private slots:
    void rebuildCats();      // ドメイン変更 → カテゴリボタン行を再構築
    void rebuildGrid();      // 検索/カテゴリ変更 → コンポーネントグリッド再構築

private:
    void rebuildFavorites(); // お気に入りチップ行を再構築

    Project     *m_p;
    QLineEdit   *m_search;
    QGridLayout *m_catGrid;
    SectionBox  *m_gridSection;
    QGridLayout *m_grid;
    SectionBox  *m_favSection = nullptr;   // お気に入り
    QHBoxLayout *m_favRow     = nullptr;
    QStringList  m_favorites;              // ☆ で登録したコンポーネント名
    QString      m_cat = "all";
    QVector<QPushButton *> m_catButtons;
};

} // namespace ofd
