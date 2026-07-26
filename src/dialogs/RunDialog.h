// RunDialog.h — 計算コンソールモーダル (app.jsx RunDialog 相当)。
// Runner のログをリアルタイム表示し、一時停止/停止/閉じるを提供する。
// 閉じても計算は続く (ログは RightDock にも流れている)。
#pragma once
#include <QDialog>

class QPushButton;

namespace ofd {

class LogConsole;
class Runner;

class RunDialog : public QDialog {
    Q_OBJECT
public:
    explicit RunDialog(Runner *runner, QWidget *parent = nullptr);

    void appendLine(const QString &line);
    void clearLog();

private:
    Runner      *m_runner;
    LogConsole  *m_log;
    QPushButton *m_pause, *m_stop;
};

} // namespace ofd
