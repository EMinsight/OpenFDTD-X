// ComponentsTab.h — コンポーネントライブラリ (ansys-tabs.jsx ComponentsTab 相当)。
// Ansys Lumerical 流のドラッグ&ドロップパレット:
//   検索 + カテゴリフィルタ + ドメイン別コンポーネントグリッド + 最近使用。
// カテゴリの表示/優先順はアクティブドメイン (em/optical/acoustic/underwater)
// に応じて切り替わる。
// カード (と お気に入り / 最近使用 のチップ) は 3D ビューへのドラッグ元で、
// MIME は widgets/Viewport3D.h の ComponentDrop。実際の配置 (ジオメトリ /
// 給電点 / 観測点の追加) はドロップ先の Viewport3D が行う。
// お気に入り (cl_favorites) はカードの ☆ で開閉するローカル状態のみ (Project 非依存)。
// 最近使用 (cl_recent) は「3D シーンへ実際に配置した」履歴で、お気に入りと
// 同じく QSettings に永続化される (どちらも Project 非依存のアプリ設定)。
#pragma once
#include <QScrollArea>
#include <QStringList>
#include <QVector>

class QGridLayout;
class QHBoxLayout;
class QLabel;
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
    void rebuildRecent();    // 最近使用チップ行を履歴から再構築
    void recordRecent(const QString &name);  // 配置されたものを履歴の先頭へ

    Project     *m_p;
    QLineEdit   *m_search;
    QGridLayout *m_catGrid;
    SectionBox  *m_gridSection;
    QGridLayout *m_grid;
    QLabel      *m_dragHint = nullptr;  // ドラッグ操作のヒント (水中では非表示)
    QLabel      *m_mapHint  = nullptr;  // ドロップ→.ofd の対応ヒント (同上)
    QLabel      *m_uwNote   = nullptr;  // 水中音響: 配置部品が無い理由の説明
    SectionBox  *m_favSection = nullptr;   // お気に入り
    QHBoxLayout *m_favRow     = nullptr;
    SectionBox  *m_recentSection = nullptr; // 最近使用 (配置履歴)
    QHBoxLayout *m_recentRow     = nullptr;
    QStringList  m_favorites;              // ☆ で登録したコンポーネント名
    QStringList  m_recent;                 // 配置履歴 (新しい順, 最大 kRecentMax)
    QString      m_cat = "all";
    QVector<QPushButton *> m_catButtons;
};

} // namespace ofd
