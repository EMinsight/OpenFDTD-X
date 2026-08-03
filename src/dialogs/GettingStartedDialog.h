// GettingStartedDialog.h — はじめてのシミュレーション
// (元 mock: ansys-workflow.jsx GettingStartedDialog)。
//
// OpenFDTD-X の標準ワークフローを 9 ステップで案内するモーダル。
// 各ステップのボタンは jumpTo(target) を発火して閉じる。target は
//   "gallery" (ギャラリーを開く) / "run" (計算実行) / タブキー
//   ("geometry" "material" "solverregion" "source" "monitors"
//    "datasets" "verification") のいずれか。verification は
// エキスパート表示のみのため、受け側 (MainWindow) が表示レベルを
// 切替えてから選択する。
#pragma once
#include <QDialog>

namespace ofd {

class GettingStartedDialog : public QDialog {
    Q_OBJECT
public:
    explicit GettingStartedDialog(QWidget *parent = nullptr);

signals:
    void jumpTo(const QString &target);
};

} // namespace ofd
