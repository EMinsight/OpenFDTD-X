// ThinFilmTab.h — 薄膜多層膜設計タブ (optical-applications.jsx ThinFilmTab 相当)。
// Essential Macleod / OptiLayer / TFCalc 相当の設計環境:
//   - プリセット (ARコート / DBR / バンドパス / ダイクロイック / Low-E / 偏光子)
//     を選ぶと層数バッジと目標仕様が切り替わる
//   - 層構成   : 基板 + 層スタック表 (材料/n/膜厚/QWOT/役割) + 周期記法
//   - 分光特性 : 入射角・波長範囲 + R/T スペクトル MiniPlot + 指標判定表
//   - 最適化設計: 手法 (単純降下/ニードル/トンネル/GA) + 変数 + ターゲット表
//   - 製造・誤差: 成膜法・膜厚誤差・モニタリング + モンテカルロ歩留まり
// 表示専用 (.ofd に対応フィールドが無いため状態はすべてローカル)。
#pragma once
#include <QScrollArea>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QTableWidget;
class QTabWidget;

namespace ofd {

class MiniPlot;
class Project;

class ThinFilmTab : public QScrollArea {
    Q_OBJECT
public:
    explicit ThinFilmTab(Project *project, QWidget *parent = nullptr);

private slots:
    void presetChanged(int index);      // プリセット切替 → 層数バッジ・目標・スペクトル

private:
    QWidget *buildStackPage();          // 層構成
    QWidget *buildSpecPage();           // 分光特性
    QWidget *buildDesignPage();         // 最適化設計
    QWidget *buildMfgPage();            // 製造・誤差
    void     updateSpecPlot();          // モックの数式で R/T スペクトルを生成

    Project      *m_p;

    // 上段 (プリセット)
    QComboBox    *m_preset;
    QLabel       *m_layerBadge;         // "N 層" (badge acc)
    QLabel       *m_targetLabel;        // "目標: …"
    QTabWidget   *m_tabs;

    // 層構成
    QComboBox    *m_substrate;
    QTableWidget *m_layerTable;
    QLineEdit    *m_periodic;
    QCheckBox    *m_useDispersion;
    QCheckBox    *m_useAbsorption;

    // 分光特性
    QLineEdit    *m_aoi;
    QCheckBox    *m_angleSweep;
    QCheckBox    *m_splitSP;
    QLineEdit    *m_lamMin;
    QLineEdit    *m_lamMax;
    MiniPlot     *m_specPlot;
    QTableWidget *m_specTable;

    // 最適化設計
    QButtonGroup *m_method;
    QCheckBox    *m_varThickness;
    QCheckBox    *m_varCount;
    QCheckBox    *m_varMaterial;
    QTableWidget *m_targetTable;

    // 製造・誤差
    QButtonGroup *m_deposition;
    QLineEdit    *m_thickErr;
    QCheckBox    *m_systematic;
    QCheckBox    *m_correlated;
    QButtonGroup *m_monitoring;
    QLabel       *m_yieldBadge;
    QLabel       *m_sensitiveLabel;
};

} // namespace ofd
