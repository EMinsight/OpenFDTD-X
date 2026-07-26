// LayoutGDSTab.h — GDSII レイアウト取込・レイヤーマップ (optics-tabs.jsx LayoutGDSTab 相当)。
//   - PDK 選択 / チップサイズ / グリッド
//   - レイヤー表 (GDS 番号・用途・色見本)
//   - 配置済み PCell 一覧
//   - DRC (デザインルールチェック) 結果 + GDS 入出力ボタン
//   - FDTD-IC 連携 (選択領域のみ FDTD、残りは S パラメータライブラリ)
// KLayout / SiEPIC PDK / RSoft CAD 風。光ドメイン選択時のみ表示される。
#pragma once
#include <QScrollArea>

class QComboBox;
class QLineEdit;
class QTableWidget;

namespace ofd {

class Project;

class LayoutGDSTab : public QScrollArea {
    Q_OBJECT
public:
    explicit LayoutGDSTab(Project *project, QWidget *parent = nullptr);

private:
    Project      *m_p;
    QComboBox    *m_pdk;
    QLineEdit    *m_chipW, *m_chipH;
    QComboBox    *m_grid;
    QTableWidget *m_layers, *m_cells, *m_drc;
};

} // namespace ofd
