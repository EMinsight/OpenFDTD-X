// SchematicTab.h — フォトニック回路シミュレーション (optics-tabs.jsx SchematicTab 相当)。
//   - Ansys INTERCONNECT / Synopsys PhotonicCAD 風の回路レベルエディタ。
//     S パラメータ・コンパクトモデルを連結してチップ全体を秒単位で解析する。
//   - シミュレーションモード (周波数領域 / 時間領域 / 混合)
//   - 要素ライブラリ: 導波路・リング・DBR・MZI・MMI・GC・PD・レーザ… 12 種のカード
//     (ドラッグして回路図に配置する想定なので OpenHandCursor)
//   - ネットリスト表 (From / To / 波長依存)
//   - ノイズ・温度効果 (ショット/熱/RIN/位相雑音、熱光学シフト自動適用)
// 光ドメイン選択時のみ表示される。状態はローカル (モック忠実、Project 対応欄なし)。
#pragma once
#include <QScrollArea>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QTableWidget;

namespace ofd {

class Project;

class SchematicTab : public QScrollArea {
    Q_OBJECT
public:
    explicit SchematicTab(Project *project, QWidget *parent = nullptr);

private:
    Project      *m_p;

    // シミュレーション設定 / Simulation
    QComboBox    *m_mode;

    // ネットリスト / Connections
    QTableWidget *m_net;

    // ノイズ・温度効果 / Noise & temperature
    QCheckBox    *m_shot, *m_thermal, *m_rin, *m_phase, *m_toShift;
    QLineEdit    *m_temp;
};

} // namespace ofd
