// MultiphysicsTab.h — 連成解析タブ (ansys-tabs.jsx MultiphysicsTab 相当)。
// Ansys CHARGE/HEAT・COMSOL Multiphysics 相当の連成設定:
//   - 連成モジュール一覧 (現在ドメインで意味のあるものだけ表示)
//   - 連成方式 (弱/強/双方向) と反復条件
//   - ドメイン別詳細: 熱光学+プラズマ効果 (光) / SAR→Bioheat (EM) /
//     振動音響 (音響) / 海洋環境 (水中)
// 表示専用 (Project に対応フィールドが無いためローカル状態のみ)。
#pragma once
#include <QScrollArea>

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

private slots:
    void rebuildDomain();     // ドメイン変更 → モジュール表と詳細セクション

private:
    Project      *m_p;

    QLabel       *m_hint;         // ドメイン名を含む説明文
    QTableWidget *m_modules;      // 連成モジュール一覧

    QComboBox    *m_scheme;       // 弱連成 / 強連成 / 双方向
    QLineEdit    *m_tol, *m_maxIter;

    // ドメイン別詳細セクション (表示切替)
    SectionBox   *m_secThermo;    // 光: 熱光学
    SectionBox   *m_secPlasma;    // 光: プラズマ効果 (Drude)
    SectionBox   *m_secSar;       // EM: SAR → 温度
    SectionBox   *m_secVibro;     // 音響: 振動音響
    SectionBox   *m_secOcean;     // 水中: 海洋環境
};

} // namespace ofd
