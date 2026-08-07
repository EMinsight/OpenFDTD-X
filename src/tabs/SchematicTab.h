// SchematicTab.h — フォトニック回路シミュレーション (optics-tabs.jsx SchematicTab 相当)。
//   - Ansys INTERCONNECT / Synopsys PhotonicCAD 風の回路レベルエディタ。
//     S パラメータ・コンパクトモデルを連結してチップ全体を秒単位で解析する。
//   - シミュレーションモード (周波数領域 / 時間領域 / 混合)
//   - 要素ライブラリ: 導波路・リング・DBR・MZI・MMI・GC・PD・レーザ… 12 種のカード
//     (回路図キャンバスが無いのでドラッグ配置はできない)
//   - ネットリスト表 (From / To / 波長依存) — `Project::photonicNetlist()` の
//     ビュー。編集はモデルへ書き戻され .ofdx ("schematic.netlist") に保存される。
//     回路図からの自動生成と回路シミュレーションは未実装。
//   - ノイズ・温度効果 (ショット/熱/RIN/位相雑音、熱光学シフト自動適用)
// 光ドメイン選択時のみ表示される。ネットリスト以外の設定はローカル状態。
#pragma once
#include <QFont>
#include <QScrollArea>

class QCheckBox;
class QComboBox;
class QLabel;
class QDoubleSpinBox;
class QSpinBox;
class QLineEdit;
class QTableWidget;
class QTableWidgetItem;

namespace ofd {

class MiniPlot;

class Project;

class SchematicTab : public QScrollArea {
    Q_OBJECT
public:
    explicit SchematicTab(Project *project, QWidget *parent = nullptr);

private slots:
    void runCircuitSim();        // 素子 S 行列 → 波長掃引 → 指標
    void refreshNetlist();                        // model → widgets
    void refreshNetPath();                        // 経路表示を更新
    void onNetItemChanged(QTableWidgetItem *it);  // widgets → model

private:
    Project      *m_p;
    bool          m_updating = false;   // refresh 中の itemChanged 再入ガード

    // シミュレーション設定 / Simulation
    QComboBox    *m_mode;

    // ネットリスト / Connections
    QTableWidget *m_net;
    QFont         m_netFont;            // 波長列の等幅フォント

    // 回路シミュレーション (optics/PhotonicCircuit)
    QComboBox      *m_device = nullptr;    // リング (全域通過/アドドロップ) / MZI
    QDoubleSpinBox *m_neff = nullptr, *m_ng = nullptr, *m_loss = nullptr;
    QDoubleSpinBox *m_radius = nullptr, *m_k1 = nullptr, *m_k2 = nullptr;
    QDoubleSpinBox *m_dL = nullptr, *m_shift = nullptr;
    QDoubleSpinBox *m_lam1 = nullptr, *m_lam2 = nullptr;
    QSpinBox       *m_points = nullptr;
    MiniPlot       *m_spectrum = nullptr;
    QLabel         *m_simResult = nullptr;

    // ノイズ・温度効果 / Noise & temperature
    QCheckBox    *m_shot, *m_thermal, *m_rin, *m_phase, *m_toShift;
    QLabel       *m_netPath = nullptr;   // ネットリストから辿った経路
    QLineEdit    *m_temp;
};

} // namespace ofd
