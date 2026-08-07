// EvViewer.h — カーネルの作図出力 (ev2/ev3) を見せる 3 経路 (docs/ev-format.md):
//   (A) Native  : .ev2 を自前で読んでアプリ内に描く (既定 — 外部依存なし)
//   (B) Html    : ポストを -html で走らせ ev2d.htm / ev3d.htm をブラウザで開く
//   (C) Process : 別途入手した ev2d / ev3d の実行ファイルを起動する
//
// 重要 (過去の不具合): (B) は ofd_post に -html を渡して初めて .htm が
// 生成される。この選択が RunConfig::evHtml に届いていないと、.htm は永遠に
// 作られず「出力ファイルが見つかりません」だけが出る。needsHtmlOutput() が
// その配線口で、MainWindow::currentRunConfig() が必ず参照する。
#pragma once
#include <QWidget>
#include "../core/Domain.h"

class QComboBox;
class QLabel;
class QPushButton;

namespace ofd {

class EvCanvas;

// 並びはコンボの表示順と一致させる (currentIndex をそのまま使う)
enum class EvBackend { Native, Html, Process };

class EvViewer : public QWidget {
    Q_OBJECT
public:
    explicit EvViewer(QWidget *parent = nullptr);

    EvBackend backend() const;
    void setWorkdir(const QString &dir) { m_workdir = dir; }
    // ポスト処理に -html が要るか (Html バックエンドを選んでいるときだけ)。
    // これを RunConfig::evHtml に渡さないと .htm は生成されない。
    bool needsHtmlOutput() const { return backend() == EvBackend::Html; }

signals:
    // アプリ内描画を選んで「ev2d を開く」を押した — MainWindow が中央の
    // 「カーネル作図」タブへ切り替えてこのファイルを読む。
    void showNativeRequested(const QString &ev2Path);

public slots:
    void open2D();
    void open3D();

private:
    void open(bool threeD);

    QComboBox *m_backendBox;
    QLabel    *m_status;
    QString    m_workdir;
};

} // namespace ofd
