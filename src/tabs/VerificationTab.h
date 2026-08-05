// VerificationTab.h — 検証タブ (ansys-workflow.jsx VerificationTab 相当)。
//   ① メッシュ解像度 (計画値) ② 境界吸収の設計反射率
//   ③ 収束履歴 (ソルバー実行ログ) ④ クロスバリデーション + 自動診断表。
//   Lumerical FDTD course の「Result Verification」相当。
//
// 表示の区分 (捏造値を置かないための原則):
//   - 設定から決まる量 (セル数 / λ/Δx / 推定メモリ / 設計反射率 / 安定条件・
//     分解能・配置の診断) は core/FdtdVerification の実計算。
//   - 実行が要る量 (収束履歴・シャットオフ到達) はソルバーの実行ログを
//     読んで表示し、未実行なら「未実行」と明示する (空の値を出す)。
//   - 実装されていない量 (各解像度での結果比較・反射率の実測) は列ごと
//     「—」にし、何をすれば埋まるかを注記する。
//
//   ドメイン依存の項目 (PML/収束履歴セクション・比較ソルバリスト・
//   診断行) は refreshDomain() で出し分ける。
#pragma once
#include <QScrollArea>
#include <QString>
#include <vector>

#include "../core/FdtdVerification.h"

class QComboBox;
class QLabel;
class QPushButton;
class QShowEvent;
class QTableWidget;

namespace ofd {

class Project;
class MiniPlot;
class SectionBox;

class VerificationTab : public QScrollArea {
    Q_OBJECT
public:
    explicit VerificationTab(Project *project, QWidget *parent = nullptr);

protected:
    // タブが見えたときにだけ実行ログを読み直す (常時監視はしない)
    void showEvent(QShowEvent *e) override;

private slots:
    void refreshDomain();     // ドメイン依存表示 (チェック量・比較ソルバ) を更新
    void increasePmlLayers(); // ② PML層数を増加 (Project::general を実際に変更)
    void addBoundaryMargin(); // ② 境界余裕 +λ/4 (メッシュ各軸の両端を拡張)
    void refreshPmlButtons(); // ② 対策ボタンの文言・有効状態を現状に合わせる
    void refreshComputed();   // 設定から決まる表示 (①②診断) を再計算する
    void reloadRunLog();      // ソルバー実行ログを読み直し ③ と診断へ反映する

private:
    // 自動診断表の行 (順序は m_diag の行番号と一致させる)
    enum DiagRow {
        DiagResolution = 0,   // λ/Δx
        DiagCourant,          // CFL 安定条件
        DiagBoundary,         // 吸収境界の設定
        DiagConverged,        // 収束 (シャットオフ) 到達 — 実行ログが必要
        DiagMonitorInside,    // 観測点が解析領域内
        DiagSeparation,       // 波源と観測点の距離
        DiagMargin,           // 形状と境界の余裕
        DiagRowCount
    };

    void updateMeshTable();      // ① メッシュ解像度の計画値
    void updateBoundaryTable();  // ② 設計反射率
    void updateEnergyPlot();     // ③ 収束履歴のプロットとバッジ
    void updateDiagnostics();    // 自動診断 7 行

    Project      *m_p;
    QComboBox    *m_qtyBox;       // ① チェックする量 (ドメイン別)
    QTableWidget *m_meshTbl;      // ① メッシュ解像度表
    QLabel       *m_meshStatus;   // ① 現在メッシュの λ/Δx バッジ
    QLabel       *m_meshNote;     // ① 何が実計算で何が未実行かの注記

    SectionBox   *m_pmlSection;   // ② 境界吸収 (水中では非表示)
    QTableWidget *m_pmlTbl;       // ② 入射角ごとの設計反射率
    QLabel       *m_pmlNote;      // ② 設計値の根拠と適用範囲
    QPushButton  *m_pmlBtn1;      // ② PML層数増加ボタン (配線済み)
    QPushButton  *m_pmlBtn2;      // ② 境界余裕 +λ/4 ボタン (配線済み)

    SectionBox   *m_timeSection;  // ③ 収束履歴 (水中では非表示)
    MiniPlot     *m_energyPlot;   // ③ 収束履歴プロット
    QLabel       *m_timeBadge;    // ③ 収束状態 (実データ or 未実行)
    QLabel       *m_timeSource;   // ③ 読込元ログのパスと点数

    QComboBox    *m_crossBox;     // ④ 比較ソルバ (ドメイン別リスト)
    QTableWidget *m_diag;         // 自動診断表
    QLabel       *m_diagBadge[DiagRowCount] = {};   // 各行の判定バッジ
    QLabel       *m_diagNote;     // 判定閾値の根拠と仮定

    // ③ 実行ログから読んだ収束履歴 (空 = 未実行)
    std::vector<verify::ConvergencePoint> m_history;
    QString m_logPath;            // 実際に読めたログのパス (空 = 未検出)
    QString m_logName;            // 期待するログ名 (ofd.log 等)
};

} // namespace ofd
