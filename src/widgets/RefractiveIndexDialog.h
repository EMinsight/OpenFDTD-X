// RefractiveIndexDialog.h — refractiveindex.info から n,k を取り込むダイアログ
//
// 通信は**利用者が押したときだけ**行う (開いただけでは何も取りに行かない)。
// 通信先は画面に出す。Qt6::Network が無い構成では取得ボタンを出さず、
// 配布ページを開くだけにして、手元の CSV を「📁 n,k 取込」で使うよう案内する。
#ifndef OFD_WIDGETS_REFRACTIVEINDEXDIALOG_H
#define OFD_WIDGETS_REFRACTIVEINDEXDIALOG_H

#include <QDialog>
#include <QVector>

#include "../io/NkCsv.h"
#include "../io/RefractiveIndexDb.h"

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

namespace ofd {

class RefractiveIndexDialog : public QDialog {
    Q_OBJECT
public:
    explicit RefractiveIndexDialog(QWidget *parent = nullptr);
    ~RefractiveIndexDialog() override;

    // 取り込む材料 (accept 後に有効)
    const NkTable &table() const { return m_table; }
    QString        name()  const { return m_name; }

private:
    void fetchCatalog();
    void fetchSelected();
    void applyFilter();
    void setBusy(bool on, const QString &what);

    class Impl;
    Impl *m_impl = nullptr;

    QVector<RiEntry> m_entries;
    QVector<int>     m_shown;      // m_entries への索引 (絞り込み結果)
    NkTable          m_table;
    QString          m_name;

    QLineEdit   *m_filter   = nullptr;
    QListWidget *m_list     = nullptr;
    QLabel      *m_status   = nullptr;
    QLabel      *m_detail   = nullptr;
    QPushButton *m_fetchBtn = nullptr;
    QPushButton *m_takeBtn  = nullptr;
};

} // namespace ofd

#endif // OFD_WIDGETS_REFRACTIVEINDEXDIALOG_H
