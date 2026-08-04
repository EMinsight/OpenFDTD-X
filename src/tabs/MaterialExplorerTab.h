// MaterialExplorerTab.h — 材料Explorer (material-explorer.jsx 相当)。
// Ansys Lumerical "Material Explorer" 相当:
//   - 内蔵 n,k データベースツリー (金属/半導体/誘電体/2D材料/光学ガラス)
//     + 検索フィルタ
//   - フィットモデル切替 (Multi-coefficient / Drude / Lorentz / Sampled)
//   - n,k フィット結果プレビュープロット (光学ガラスは Sellmeier 実曲線)
//   - モデル診断表 + 「この材料を物性値リストに追加」→ Project::materials()
#pragma once
#include <QScrollArea>
#include <QVector>

class QComboBox;
class QLabel;
class QPushButton;
class QLineEdit;
class QStackedWidget;
class QTreeWidget;
class QTreeWidgetItem;

namespace ofd {

class Project;
class MiniPlot;
class SectionBox;

class MaterialExplorerTab : public QScrollArea {
    Q_OBJECT
public:
    explicit MaterialExplorerTab(Project *project, QWidget *parent = nullptr);

private slots:
    void filterTree(const QString &query);
    void addToMaterials();

private:
    // DBの1材料。glassIndex >= 0 なら GlassCatalog::all() の光学ガラス。
    struct Entry {
        QString id, name, model, range;
        int     glassIndex = -1;
    };
    void buildDatabase();
    void showEntry(int index);
    double previewN(const Entry &e, double lambda_nm) const;

    Project        *m_p;
    QVector<Entry>  m_entries;
    QTreeWidget    *m_tree;
    QLineEdit      *m_search;
    SectionBox     *m_selSection;
    QLabel         *m_modelBadge, *m_rangeLabel;
    QComboBox      *m_fitModel;
    QStackedWidget *m_modelStack;
    QLineEdit      *m_fitMin, *m_fitMax;
    QLineEdit      *m_nCoef, *m_rmsTol, *m_iters;            // Multi-coefficient
    QLineEdit      *m_epsInfD, *m_wpD, *m_gammaD;            // Drude
    QLineEdit      *m_epsInfL, *m_wpL, *m_gammaL, *m_w0L;    // Lorentz
    MiniPlot       *m_plotN, *m_plotK;
    QPushButton    *m_addBtn = nullptr;
    QLabel         *m_previewNote = nullptr;   // 実分散 / 例示曲線の別を明示
    int             m_sel = 0;
};

} // namespace ofd
