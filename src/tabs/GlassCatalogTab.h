// GlassCatalogTab.h — 光学ガラスカタログ (glass-catalog.jsx 相当)。
//   - 検索 + メーカーフィルタ + 一覧表 (nd/vd/価格/特徴)
//   - 選択銘柄の詳細: nd/vd, n@1550nm, n@633nm, Sellmeier B/C, 分散曲線 n(λ)
//   - アッベ図 (nd vs vd 散布図, クリックで選択)
//   - Zemax AGF / CSV カタログ取込 + 取込フォーマット表
//   - 「この銘柄を物性値リストへ」→ Project::materials() に εr = n(λc)² で追加
//   - 「FDTD分散材料化」→ 分散性材質 (type=2) として追加
// 光ドメイン選択時のみ表示される。
#pragma once
#include <QScrollArea>
#include "../core/GlassCatalog.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QTableWidget;

namespace ofd {

class Project;
class MiniPlot;
class SectionBox;

// nd–vd アッベ図 (クリックで銘柄選択)
class AbbeDiagram : public QWidget {
    Q_OBJECT
public:
    explicit AbbeDiagram(QWidget *parent = nullptr);
    void setSelected(int index);   // index into GlassCatalog::all()
signals:
    void glassClicked(int index);
protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
private:
    QPointF toScreen(double vd, double nd) const;
    int m_selected = 0;
};

class GlassCatalogTab : public QScrollArea {
    Q_OBJECT
public:
    explicit GlassCatalogTab(Project *project, QWidget *parent = nullptr);

private slots:
    void refreshList();
    void selectRow(int row);
    void importCatalog(bool agf);
    void addToMaterials();
    void addDispersiveMaterial();   // FDTD分散材料化 (type=2 で追加)
    void copyNameForLensEditor();   // Lens Editor のガラス欄用に銘柄名をコピー

private:
    void showGlass(const Glass &g);
    int  catalogIndexForRow(int row) const;

    Project      *m_p;
    QLineEdit    *m_search;
    QComboBox    *m_maker;
    SectionBox   *m_listSection;   // 表題に銘柄数を出す (mock: ガラス一覧 (N銘柄))
    QTableWidget *m_table;
    QLabel       *m_detTitle;
    QLabel       *m_detNd, *m_detN1550, *m_detN633, *m_detB, *m_detC;
    MiniPlot     *m_dispersion;
    AbbeDiagram  *m_abbe;
    QLabel       *m_status;
    QVector<int>  m_rowToIndex;   // table row → GlassCatalog::all() index
    int           m_selIndex = 0;
};

} // namespace ofd
