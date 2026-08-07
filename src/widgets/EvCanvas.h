// EvCanvas.h — カーネルの作図出力 (.ev2) をアプリ内で描く画面。
//
// 従来は外部依存だった: ブラウザで ev2d.htm を開く (Html) か、別途入手する
// ev2d / ev3d の実行ファイルを起動する (Process)。どちらも手元に無いと
// 図が見られないため、**アプリ内で描く** 経路を用意する。
// 形式の詳細は io/EvReader.h (post/ev2d.c の出力仕様そのまま)。
#pragma once
#include <QWidget>

#include "../io/EvReader.h"

namespace ofd {

class EvCanvas : public QWidget {
    Q_OBJECT
public:
    explicit EvCanvas(QWidget *parent = nullptr);

    // .ev2 を読み込む。失敗したら理由を保持して false。
    bool load(const QString &path, QString *err = nullptr);
    void clear();

    int  pageCount() const { return m_doc.pages.size(); }
    int  page() const { return m_page; }
    void setPage(int i);

    // 読み込み済みのファイル (見出し表示用)
    QString sourcePath() const { return m_path; }

signals:
    void pageChanged(int page, int count);

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    EvDocument m_doc;
    QString    m_path;
    QString    m_placeholder;
    int        m_page = 0;
};

} // namespace ofd
