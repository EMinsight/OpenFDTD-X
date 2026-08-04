// KernelPathDialog.h — ソルバーカーネルの場所を GUI で設定するダイアログ。
//
// 環境変数 (OPENFDTD_HOME 等) は macOS の Finder / Dock 起動では GUI に
// 届かないため、QSettings ("OpenFDTD/Kernels"、openuwa と共有) に永続化する
// 設定手段を用意する。各カーネルの行に入力欄 + 参照ボタン +
// 現在の解決結果 (実際に見つかったバイナリのパス / 未検出) を表示する。
//
// 行はドメイン (電磁 / 光 / 室内音響 / 水中音響) ごとに見出しを付けて並べる。
// タブ分割にしていないのは、この画面の主目的が「どのカーネルが入っていて
// どれが足りないか」の一覧確認であり、5 行しかないため隠す損の方が大きいから。
// 現在のドメインの見出しには印を付け、どこを見ればよいかが分かるようにする。
//
// 室内音響だけはディレクトリではなく**実行ファイル**のパスを持つ
// (探索名がバックエンドで変わるため)。値は AcousticRunner の
// solverPathSetting() 側に保存し、プロジェクト個別指定
// (.ofdx solver.executable) が空のときの既定として使われる。
#pragma once
#include <QDialog>
#include <QVector>

#include "../core/Domain.h"
#include "../kernel/Runner.h"

class QLabel;
class QLineEdit;

namespace ofd {

class KernelPathDialog : public QDialog {
    Q_OBJECT
public:
    // activeDomain: 見出しに「現在のドメイン」印を付けるためだけに使う
    // (既定 = 印なし)。設定内容そのものはドメインに依存しない。
    explicit KernelPathDialog(QWidget *parent = nullptr,
                              Domain activeDomain = Domain::EM,
                              bool markActiveDomain = false);

private:
    void updateStatus(int row);   // 行の解決結果表示を更新
    void saveAll();               // QSettings へ保存 (accept 時)

    struct Row {
        Domain     domain = Domain::EM;
        bool       acoustic = false;  // true = 外部音響ソルバー (実行ファイル指定)
        Kernel     kernel = Kernel::FDTD;   // acoustic=false のとき有効
        QLineEdit *dir = nullptr;
        QLabel    *status = nullptr;
    };
    QVector<Row> m_rows;
};

} // namespace ofd
