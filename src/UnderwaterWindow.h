// UnderwaterWindow.h — OpenUWA 分離アプリのメインシェル
// (元 underwater-app.jsx / OpenUWA-Underwater.html)。
//
// 本体 OpenFDTD-X との違い:
//   - ドメインタブを持たない (水中音響に固定)
//   - 左は水中関連 5 タブのフラットなタブバー (カテゴリ分けなし)
//   - 既定は ダーク + Comfortable 密度 (モックの useTweaks 既定)
//
//   ┌ titlebar  〜 OpenUWA — 水中音響解析 (OpenFDTD-X 派生)      共有エンジン… ┐
//   ├──────────────────────────────┬─────────────────────────────────────────┤
//   │ [🌏海洋環境][〜伝搬解析][🎤音源] │                                         │
//   │ [🎬H5アニメ][🔗ツール連携]      │            Viewport3D                   │
//   │ タブ本体 (幅 46%, 最小 420)     │                                         │
//   ├──────────────────────────────┴─────────────────────────────────────────┤
//   │ ドメイン: 水中音響 (固定)      .ofdx (本体と共通) / .env .bty .ssp        │
//   └────────────────────────────────────────────────────────────────────────┘
//
// タブ実装は本体と同一クラスをそのまま再利用する (ofdx_gui 共有ライブラリ)。
#pragma once
#include <QMainWindow>

class QTabWidget;

namespace ofd {

class Project;
class Viewport3D;

class UnderwaterWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit UnderwaterWindow(QWidget *parent = nullptr);

    Project *project() const { return m_project; }

public slots:
    void openProject(const QString &path);
    // CLI --left-tab 用: タブ名に部分一致するものを選ぶ
    void selectTab(const QString &titlePart);

private:
    void buildUi();

    Project     *m_project = nullptr;
    QTabWidget  *m_tabs = nullptr;
    Viewport3D  *m_viewport = nullptr;
};

} // namespace ofd
