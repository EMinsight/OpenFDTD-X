// CloudDialog.h — tidy3d クラウド送信モーダル (app.jsx CloudDialog 相当)。
//
// 光ドメイン専用。ジョブ名・見積コスト・推定実行時間・解像度・PML を確認し、
// 「🚀 送信」で submitted() を発火 → MainWindow が Python スクリプトを生成する。
// 表示値は Project の Tidy3dOpts / GeneralOpts から作る (モックの静的値が既定)。
#pragma once
#include <QDialog>

class QCheckBox;
class QLabel;
class QLineEdit;

namespace ofd {

class Project;

class CloudDialog : public QDialog {
    Q_OBJECT
public:
    explicit CloudDialog(Project *project, QWidget *parent = nullptr);

signals:
    void submitted();

protected:
    void showEvent(QShowEvent *) override;

private:
    void refresh();          // Project → ジョブ名・解像度・PML 表示

    Project   *m_p;

    QLineEdit *m_jobName;
    QLabel    *m_cost, *m_credits, *m_runtime, *m_resolution, *m_pml;
    QCheckBox *m_compareLocal, *m_autoDownload, *m_notify;
};

} // namespace ofd
