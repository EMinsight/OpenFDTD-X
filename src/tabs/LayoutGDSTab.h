// LayoutGDSTab.h — GDSII レイアウト取込・レイヤーマップ (optics-tabs.jsx LayoutGDSTab 相当)。
//   - PDK 選択 / チップサイズ / グリッド
//   - レイヤー表 (GDS 番号・用途・色見本)
//   - 配置済みセル一覧 = プロジェクトの形状ユニットの XY 投影 (実データ)
//   - DRC: 線幅・間隔・密度を投影フットプリントから実計算する
//     (曲率半径・パッド間隔は対応データがモデルに無いので「対象外」)
//   - FDTD-IC 連携 (選択領域のみ FDTD、残りは S パラメータライブラリ)
//   - GDS の取込 → 選択レイヤーを直方体ユニットへ変換 (io/GdsGeometry)
// KLayout / SiEPIC PDK / RSoft CAD 風。光ドメイン選択時のみ表示される。
#pragma once
#include <QScrollArea>

#include "../io/GdsGeometry.h"
#include <QString>
#include <QVector>

class QComboBox;
class QLabel;
class QPushButton;
class QLineEdit;
class QTableWidget;

namespace ofd {

class Project;

// 形状ユニットの外接直方体を XY 平面 (レイアウト平面) へ落としたもの。
// 単位はモデルと同じ m。セル一覧と DRC が共有する。
struct Footprint {
    QString name;
    double  x0 = 0, x1 = 0, y0 = 0, y1 = 0;
    double  width()  const { return x1 - x0; }
    double  height() const { return y1 - y0; }
    double  minDim() const { return width() < height() ? width() : height(); }
    double  area()   const { return width() * height(); }
};

inline bool operator==(const Footprint &a, const Footprint &b)
{
    return a.name == b.name && a.x0 == b.x0 && a.x1 == b.x1
        && a.y0 == b.y0 && a.y1 == b.y1;
}

class LayoutGDSTab : public QScrollArea {
    Q_OBJECT
public:
    explicit LayoutGDSTab(Project *project, QWidget *parent = nullptr);

private slots:
    void exportGds();         // Footprint → GDSII (BOUNDARY)
    void importGds();         // GDSII → 内容の要約 + ライブラリの保持
    void convertToGeometry(); // 取り込んだ GDS の選択レイヤ → 形状ユニット

private:
    void refreshLayout();     // 形状 → セル一覧 + DRC を作り直す
    void rebuildCells(const QVector<Footprint> &foots, int skipped);
    void rebuildDrc(const QVector<Footprint> &foots);

    Project      *m_p;
    QComboBox    *m_pdk;
    QLineEdit    *m_chipW, *m_chipH;
    QComboBox    *m_grid;
    QTableWidget *m_layers, *m_cells, *m_drc;
    QLabel       *m_cellsSkipped = nullptr;  // 除外ユニット数の注記
    QLabel       *m_ioStatus = nullptr;      // GDS 入出力の結果表示
    // 取り込んだ GDS (「形状へ変換」で使う)。空なら未取込
    GdsLibrary    m_lib;
    QString       m_libPath;
    QPushButton  *m_convertBtn = nullptr;
    QLabel       *m_convertStatus = nullptr;
    // 直前の投影 (Project::changed の度に DRC を回さないためのキャッシュ)
    QVector<Footprint> m_lastFoots;
    int          m_lastSkipped = -1;
};

} // namespace ofd
