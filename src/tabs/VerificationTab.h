// VerificationTab.h — 検証タブ (ansys-workflow.jsx VerificationTab 相当)。
//   ① メッシュ収束 ② PML吸収品質 ③ 時間精度 (エネルギー減衰 MiniPlot)
//   ④ クロスバリデーション + 自動診断表。
//   Lumerical FDTD course の「Result Verification」相当。
//   ドメイン依存の項目 (PML/時間精度セクション・比較ソルバリスト・
//   診断行) は refreshDomain() で出し分ける。
#pragma once
#include <QScrollArea>

class QComboBox;
class QTableWidget;

namespace ofd {

class Project;
class MiniPlot;
class SectionBox;

class VerificationTab : public QScrollArea {
    Q_OBJECT
public:
    explicit VerificationTab(Project *project, QWidget *parent = nullptr);

private slots:
    void refreshDomain();     // ドメイン依存表示 (チェック量・診断備考) を更新

private:
    Project      *m_p;
    QComboBox    *m_qtyBox;       // ① チェックする量 (ドメイン別)
    SectionBox   *m_pmlSection;   // ② PML吸収品質 (水中では非表示)
    SectionBox   *m_timeSection;  // ③ 時間精度 (水中では非表示)
    QComboBox    *m_crossBox;     // ④ 比較ソルバ (ドメイン別リスト)
    QTableWidget *m_diag;         // 自動診断表 (行0 備考と一部行がドメイン別)
    MiniPlot     *m_energyPlot;   // ③ エネルギー減衰
};

} // namespace ofd
