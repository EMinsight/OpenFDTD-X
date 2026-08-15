// OfdPreviewDialog.h — 保存前に .ofd / .ofdx の中身を見る読み取り専用ビュー
//
// 表示するのは `io/OfdPreview` が組み立てた**保存経路そのものの出力**なので、
// ここに見えるものと保存されるものは一致する。編集はできない (見るだけ)。
#ifndef OFD_WIDGETS_OFDPREVIEWDIALOG_H
#define OFD_WIDGETS_OFDPREVIEWDIALOG_H

#include <QDialog>

class QCheckBox;
class QLabel;
class QPlainTextEdit;

namespace ofd {

class Project;

class OfdPreviewDialog : public QDialog {
    Q_OBJECT
public:
    explicit OfdPreviewDialog(const Project *project, QWidget *parent = nullptr);

private:
    void rebuild();
    void applyExtraHighlight();

    const Project  *m_project = nullptr;
    QPlainTextEdit *m_ofd     = nullptr;
    QPlainTextEdit *m_ofdx    = nullptr;
    QCheckBox      *m_markExtra = nullptr;
    QLabel         *m_summary = nullptr;
    QLabel         *m_extraNote = nullptr;
    QVector<int>    m_extraRows;
};

} // namespace ofd

#endif // OFD_WIDGETS_OFDPREVIEWDIALOG_H
