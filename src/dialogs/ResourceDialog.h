// ResourceDialog.h — 並列実行資源設定 (ansys-workflow.jsx ResourceDialog 相当)。
//
// Lumerical の "Resource Configuration" を再現:
//   プロセス数 (MPI) × スレッド数 (OpenMP) = 利用コア数
// マシンコア数との比較でバッジを 超過!/最適/未使用コア に切り替える。
// コア数・GPU・実装メモリは実機から検出する (モックの固定値 16 コア /
// RTX 4090 ×2 / 64GB は表示しない — 実機と異なる値を見せない)。
// モックは静的プロトタイプなので設定はローカル状態のみ (Project に対応欄なし)。
#pragma once
#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QSlider;

namespace ofd {

class ResourceDialog : public QDialog {
    Q_OBJECT
public:
    explicit ResourceDialog(QWidget *parent = nullptr);

private:
    void updateCores();      // プロセス×スレッド → 合計コア表示とバッジ

    int        m_machineCores = 1;   // 実機の論理コア数 (起動時に検出)

    QSlider   *m_processes;
    QSlider   *m_threads;
    QLabel    *m_procVal, *m_threadVal;
    QLabel    *m_total, *m_badge;

    QCheckBox *m_parallelSweep, *m_licenseShare, *m_cuda;
    QComboBox *m_gpu;
    QLineEdit *m_memLimit;
};

} // namespace ofd
