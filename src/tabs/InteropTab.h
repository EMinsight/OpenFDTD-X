// InteropTab.h — 外部ツール連携タブ (interop.jsx 相当)。
// サードパーティ製ツールとの入出力ブリッジをドメイン毎にまとめたタブ:
//   - インストール済みツール検出表 (未検出時の内蔵ソルバ代替を明示)
//   - インポート / エクスポート形式表 (⬅ / ➡ をトグルで切替)
//   - 一括変換 (監視フォルダ + CLI ヒント)
//   - スクリプトAPI連携バッジ / OSS代替スタック表
// 表の内容はすべて activeDomain() に依存するため refresh() で再構築する。
#pragma once
#include <QScrollArea>

class QLineEdit;
class QPushButton;
class QTableWidget;

namespace ofd {

class Project;

class InteropTab : public QScrollArea {
    Q_OBJECT
public:
    explicit InteropTab(Project *project, QWidget *parent = nullptr);

private slots:
    void refresh();             // ドメイン変更 → 3つの表を作り直す

private:
    void rebuildDetected();     // 🧰 インストール済みツール検出
    void rebuildBridges();      // 🔗 インポート / エクスポート形式
    void rebuildOss();          // OSS代替スタック

    Project      *m_p;
    bool          m_updating = false;

    QTableWidget *m_detected;
    QTableWidget *m_bridges;
    QTableWidget *m_oss;

    QPushButton  *m_dirImport, *m_dirExport;
    int           m_dir = 0;    // 0=import 1=export (mock の dir state)

    QLineEdit    *m_watchDir;
};

} // namespace ofd
