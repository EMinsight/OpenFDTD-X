// KernelPathDialog.h — ソルバーカーネルの場所を GUI で設定するダイアログ。
//
// 環境変数 (OPENFDTD_HOME 等) は macOS の Finder / Dock 起動では GUI に
// 届かないため、QSettings ("OpenFDTD/Kernels"、openuwa と共有) に永続化する
// 設定手段を用意する。各カーネルの行に入力欄 + 参照ボタン +
// 現在の解決結果 (実際に見つかったバイナリのパス / 未検出) を表示する。
//
// 行は「基幹カーネル (全ドメイン共通)」→ ドメイン専用ソルバ の順に並べる。
// OpenFDTD (ofd) は電磁波だけのものではなく、Runner::kernelForProject の
// 分岐どおり **電磁波は常に / 光はソルバに FDTD を選んだとき / 室内音響の
// 「計算」も** これを起動する — ドメイン別に並べると誤解を招くため独立させた。
// タブ分割にしていないのは、この画面の主目的が「どのカーネルが入っていて
// どれが足りないか」の一覧確認であり、5 行しかないため隠す損の方が大きいから。
// 現在のプロジェクトが実際に起動する行には印を付ける (activeKernel)。
//
// 室内音響だけはディレクトリではなく**実行ファイル**のパスを持つ
// (探索名がバックエンドで変わるため)。値は AcousticRunner の
// solverPathSetting() 側に保存し、プロジェクト個別指定
// (.ofdx solver.executable) が空のときの既定として使われる。
#pragma once
#include <QDialog>
#include <QVector>

#include "../kernel/Runner.h"

class QLabel;
class QLineEdit;

namespace ofd {

class Project;

class KernelPathDialog : public QDialog {
    Q_OBJECT
public:
    // project: 「このプロジェクトが実際に起動するカーネル」に印を付けるために
    // 参照する (nullptr = 印なし)。設定内容そのものはプロジェクトに依存しない。
    explicit KernelPathDialog(QWidget *parent = nullptr,
                              const Project *project = nullptr);

private:
    void updateStatus(int row);   // 行の解決結果表示を更新
    void saveAll();               // QSettings へ保存 (accept 時)

    struct Row {
        const char *groupKey = nullptr;   // 見出し (nullptr = 直前の行と同じ)
        const char *noteKey = nullptr;    // 行の下に出す補足 (nullptr = なし)
        bool       acoustic = false;  // true = 外部音響ソルバー (実行ファイル指定)
        Kernel     kernel = Kernel::FDTD;   // acoustic=false のとき有効
        QLineEdit *dir = nullptr;
        QLabel    *status = nullptr;
    };
    QVector<Row> m_rows;
};

} // namespace ofd
