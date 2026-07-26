// GettingStartedDialog.h — はじめてのシミュレーション
// (ansys-workflow.jsx GettingStartedDialog 相当)。
//
// Ansys Lumerical FDTD の標準ワークフローを 9 ステップでガイドするモーダル。
// 各ステップのボタンは jumpTo(target) を発火して閉じる。target は
//   "gallery" (ギャラリーを開く) / "run" (計算実行) / タブキー
//   ("geometry" "material" "solverregion" "source" "monitors"
//    "datasets" "verification") のいずれか。
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
