// AppGalleryDialog.h — 応用ギャラリー (app.jsx AppGalleryDialog 相当)。
//
// 「FDTD で出来ること」をドメイン別に一覧するテンプレート選択モーダル。
// 5 グループ (電磁/光/室内音響/水中/クラウド) × 全テンプレートカードを
// スクロール可能なグリッドに並べる。カードをクリックすると
//   templatePicked(domainKey, name)
// を発火して閉じる → MainWindow がドメイン切替 + プロジェクト名設定を行う。
#pragma once
#include <QDialog>
#include <QFrame>
#include <QString>

namespace ofd {

// テンプレートカード 1 枚 (mock の <button> カード相当)。
// ホバーでグループのドメイン色の枠線に変わり、クリックで clicked() を発火。
class GalleryCard : public QFrame {
    Q_OBJECT
public:
    GalleryCard(const QString &name, const QString &sub,
                const QString &accent, QWidget *parent = nullptr);

signals:
    void clicked();

protected:
    void enterEvent(QEnterEvent *) override;
    void leaveEvent(QEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;

private:
    void applyStyle(bool hover);

    QString m_accent;
};

class AppGalleryDialog : public QDialog {
    Q_OBJECT
public:
    explicit AppGalleryDialog(QWidget *parent = nullptr);

signals:
    // domainKey は "em" / "optical" / "acoustic" / "underwater" / "tidy3d"
    void templatePicked(const QString &domainKey, const QString &name);
    // フッタのボタン (以前は閉じるだけだったので実動作を通知する)
    void openFileRequested();   // 📂 ファイルから開く
    void blankRequested();      // 空のプロジェクトで開始
};

} // namespace ofd
