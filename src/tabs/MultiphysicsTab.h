// MultiphysicsTab.h — 連成解析タブ (ansys-tabs.jsx MultiphysicsTab 相当)。
// Ansys CHARGE/HEAT・COMSOL Multiphysics 相当の連成設定:
//   - 連成モジュール一覧 (現在ドメインで意味のあるものだけ表示)
//   - 連成方式 (弱/強/双方向) と反復条件
//   - ドメイン別詳細: 熱光学+プラズマ効果 (光) / SAR→Bioheat (EM) /
//     振動音響 (音響) / 海洋環境 (水中)
// 表示専用 (Project に対応フィールドが無いためローカル状態のみ)。
#pragma once
#include <QScrollArea>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QTableWidget;

namespace ofd {

class Project;
class SectionBox;

class MultiphysicsTab : public QScrollArea {
    Q_OBJECT
public:
    explicit MultiphysicsTab(Project *project, QWidget *parent = nullptr);

    // 実行完了時に MainWindow が呼ぶ。作業ディレクトリの <kernel>.log から
    // 熱解析レイヤの診断行を読んで表へ入れる (この実行が出したものだけ)。
    void loadThermalFrom(const QString &logPath);
    void clearThermal();

private slots:
    void rebuildDomain();     // ドメイン変更 → モジュール表と詳細セクション
    void refreshPlasma();     // プロジェクトの波長を読み直して Δn を再計算
    void updatePlasma();      // 入力 → Δn / Δα (src/optics/PlasmaDispersion)

private:
    Project      *m_p;
    bool          m_updating = false;

    QLabel       *m_hint;         // ドメイン名を含む説明文
    QTableWidget *m_modules;      // 連成モジュール一覧

    QTableWidget *m_thermalTbl = nullptr;   // 熱解析レイヤの実測値
    QLabel       *m_thermalStatus = nullptr;

    QComboBox    *m_scheme;       // 弱連成 / 強連成 / 双方向
    QLineEdit    *m_tol, *m_maxIter;

    // ドメイン別詳細セクション (表示切替)
    SectionBox   *m_secThermo;    // 光: 熱光学
    SectionBox   *m_secPlasma;    // 光: プラズマ効果 (Drude)
    // プラズマ効果の入力と算出結果 (実計算 — PlasmaDispersion)
    QComboBox    *m_plModel   = nullptr;   // Soref-Bennett / Drude
    QLineEdit    *m_plDeltaN  = nullptr;   // ΔN [cm^-3]
    QLineEdit    *m_plDeltaP  = nullptr;   // ΔP [cm^-3]
    QLineEdit    *m_plLambda  = nullptr;   // λ [nm] (既定はプロジェクトの光波長)
    QLineEdit    *m_plIndex   = nullptr;   // 背景屈折率 n
    QCheckBox    *m_plElectron = nullptr;  // 電子濃度依存を含める
    QCheckBox    *m_plHole     = nullptr;  // 正孔濃度依存を含める
    QLabel       *m_plFormula = nullptr;   // 使用した式
    QLabel       *m_plResult  = nullptr;   // Δn / Δα の算出値
    QLabel       *m_plWarn    = nullptr;   // 適用範囲外・入力不正の警告
    SectionBox   *m_secSar;       // EM: SAR → 温度
    SectionBox   *m_secVibro;     // 音響: 振動音響
    SectionBox   *m_secOcean;     // 水中: 海洋環境
};

} // namespace ofd
