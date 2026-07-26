// VerificationTab.h — 検証タブ (ansys-workflow.jsx VerificationTab 相当)。
//   ① メッシュ収束 ② PML吸収品質 ③ 時間精度 (エネルギー減衰 MiniPlot)
//   ④ クロスバリデーション + 自動診断表。
//   Lumerical FDTD course の「Result Verification」相当。
#pragma once
#include <QScrollArea>

class QComboBox;
class QTableWidget;

namespace ofd {

class Project;
class MiniPlot;

class VerificationTab : public QScrollArea {
    Q_OBJECT
public:
    explicit VerificationTab(Project *project, QWidget *parent = nullptr);

private slots:
    void refreshDomain();     // ドメイン依存表示 (チェック量・診断備考) を更新

private:
    Project      *m_p;
    QComboBox    *m_qtyBox;       // ① チェックする量 (ドメイン別)
    QTableWidget *m_diag;         // 自動診断表 (行0 の備考がドメイン別)
    MiniPlot     *m_energyPlot;   // ③ エネルギー減衰
};

} // namespace ofd
