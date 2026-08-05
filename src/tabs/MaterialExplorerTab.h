// MaterialExplorerTab.h — 材料Explorer (material-explorer.jsx 相当)。
// Ansys Lumerical "Material Explorer" 相当:
//   - 内蔵 n,k データベースツリー (金属/半導体/誘電体/2D材料/光学ガラス)
//     + 検索フィルタ
//   - フィットモデル切替 (Multi-coefficient / Drude / Lorentz / Sampled)
//   - n,k フィット結果プレビュープロット (光学ガラスは Sellmeier 実曲線)
//   - モデル診断表 + 「この材料を物性値リストに追加」→ Project::materials()
//
// フィットと診断は実計算: 公刊 Sellmeier 係数から作った参照データに
// src/optics/DispersionFit の極モデルを最小二乗で当て、RMS 残差・因果律
// (透明域の必要条件)・受動性・n_min を求める。実分散データを持たない材料は
// フィットできないので、診断は「評価対象外」と表示する (偽の合格を出さない)。
#pragma once
#include <QScrollArea>
#include <QVector>

#include "../optics/DispersionFit.h"
#include "../widgets/MiniPlot.h"

class QComboBox;
class QLabel;
class QPushButton;
class QLineEdit;
class QStackedWidget;
class QTableWidget;
class QTreeWidget;
class QTreeWidgetItem;

namespace ofd {

class Project;
class SectionBox;

class MaterialExplorerTab : public QScrollArea {
    Q_OBJECT
public:
    explicit MaterialExplorerTab(Project *project, QWidget *parent = nullptr);

private slots:
    void filterTree(const QString &query);
    void addToMaterials();
    void runFit();                  // 参照データへ分散モデルを当てる (実計算)

private:
    // DBの1材料。glassIndex >= 0 なら GlassCatalog::all() の光学ガラス。
    struct Entry {
        QString id, name, model, range;
        int     glassIndex = -1;
    };
    void buildDatabase();
    void showEntry(int index);
    double previewN(const Entry &e, double lambda_nm) const;
    // 参照データ (公刊 Sellmeier) の有効範囲 [nm]。無い材料は false
    bool referenceRange(const Entry &e, double &lo_nm, double &hi_nm) const;
    void clearFit();                // フィット結果を捨てて「未計算」表示へ
    void showFit();                 // バッジ・診断表・重ね描きの更新
    static void setBadge(QLabel *badge, const QString &text, const char *color);

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

    // ── フィット (実計算) ──
    QPushButton    *m_fitBtn = nullptr;
    QLabel         *m_badgeRms = nullptr, *m_badgeCausal = nullptr;
    QLabel         *m_fitStatus = nullptr;     // 参照データ・結果・エラーの説明
    QTableWidget   *m_diag = nullptr;          // モデル診断表
    MiniSeries      m_refSeries;               // 参照データ曲線 (重ね描き用)
    optics::FitReport m_fit;
    bool            m_hasFit = false;
    double          m_fitLo_nm = 0.0, m_fitHi_nm = 0.0;   // 実際に使った範囲
};

} // namespace ofd
