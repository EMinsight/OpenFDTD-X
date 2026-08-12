// Tidy3dTab.h — tidy3d クラウド連携タブ (光ドメイン専用).
// 設計判断: tidy3d は物理ドメインではなく光FDTDのクラウドバックエンド —
// このタブは光ドメイン選択時のみ表示される (MainWindow::onDomainChanged)。
//
// mock (tabs.jsx / Tidy3DTab) のセクション構成:
//   ☁ tidy3d クラウド計算 / 接続 / 自動変換マッピング / エクスポート設定 /
//   ジョブ送信 / 書き出したジョブスクリプト / ローカル ↔ クラウド比較
// Project へ永続化するのは Tidy3dOpts (projectName / resolution / autoPml) と
// QSettings 上の APIキー・書き出し履歴のみ。他は mock の既定値を持つローカル状態。
// クラウド API は叩いていないので、ジョブ状態・残高・コストは表示しない
// (取得していない旨を出す — CLAUDE.md 絶対規則 5)。
#pragma once
#include <QScrollArea>

#include "../core/SeriesCompare.h"

class QLineEdit;
class QComboBox;
class QCheckBox;
class QPushButton;
class QLabel;
class QTableWidget;

namespace ofd {

class Project;

class Tidy3dTab : public QScrollArea {
    Q_OBJECT
public:
    explicit Tidy3dTab(Project *project, QWidget *parent = nullptr);

private slots:
    // 結果差分の自動チェック (クラウドの結果 CSV ↔ ローカルの給電点掃引)
    void loadCloudResult();
    void updateCloudCompare();
    void refresh();
    void exportScript();
    void previewScript();       // プレビュー (.json) — 生成スクリプトを表示
    void verifyKey();           // 接続セクションの「検証」
    void submitJob();           // ジョブ送信 (実送信は生成スクリプト経由)

private:
    void apply();
    void updateConnBadge();     // APIキーの有無で接続バッジを更新
    void rebuildJobs();         // 書き出し履歴 (QSettings) → スクリプト表

    Project   *m_p;
    bool       m_updating = false;

    QLineEdit *m_apiKey;
    QLineEdit *m_project;
    QComboBox *m_resolution;
    QCheckBox *m_autoPml;
    QLabel    *m_status;

    // ── mock のローカル状態 (Project には持たない) ──
    QLabel      *m_connBadge   = nullptr;
    QCheckBox   *m_subpixel    = nullptr;   // サブピクセル平均化
    QCheckBox   *m_dft         = nullptr;   // モニターで時間DFT記録
    QComboBox   *m_priority    = nullptr;   // 優先度 (通常 / 高)
    QLabel      *m_jobStatus   = nullptr;
    QTableWidget *m_jobs       = nullptr;
    QCheckBox   *m_cmpParallel = nullptr;
    QCheckBox   *m_cmpDiff     = nullptr;
    QCheckBox   *m_cmpNotify   = nullptr;
    // 結果差分の自動チェック (クラウドの結果 CSV ↔ ローカルの給電点掃引)
    QComboBox   *m_cmpScale = nullptr;
    QPushButton *m_cmpLoad = nullptr;
    QLabel      *m_cmpResult = nullptr;
    ofd::cmp::Series m_cloud;
    QString     m_cloudName;
};

} // namespace ofd
