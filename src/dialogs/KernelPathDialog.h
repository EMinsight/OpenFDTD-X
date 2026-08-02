// KernelPathDialog.h — ソルバーカーネルの場所を GUI で設定するダイアログ。
//
// 環境変数 (OPENFDTD_HOME 等) は macOS の Finder / Dock 起動では GUI に
// 届かないため、QSettings ("OpenFDTD/Kernels"、openuwa と共有) に永続化する
// 設定手段を用意する。各カーネルの行にディレクトリ入力 + 参照ボタン +
// 現在の解決結果 (実際に見つかったバイナリのパス / 未検出) を表示する。
#pragma once
#include <QDialog>

#include "../kernel/Runner.h"

class QLabel;
class QLineEdit;

namespace ofd {

class KernelPathDialog : public QDialog {
    Q_OBJECT
public:
    explicit KernelPathDialog(QWidget *parent = nullptr);

private:
    void updateStatus(int row);   // 行の解決結果表示を更新
    void saveAll();               // QSettings へ保存 (accept 時)

    struct Row {
        Kernel     kernel;
        QLineEdit *dir = nullptr;
        QLabel    *status = nullptr;
    };
    Row m_rows[4];
};

} // namespace ofd
