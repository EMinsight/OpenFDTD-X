// ChannelTab.h — 電波伝搬・チャネル解析タブ (em-applications.jsx ChannelTab 相当)。
// 屋内・市街地・車内・トンネルの電波カバレッジとチャネル特性を扱う。
// FDTD は近傍・小規模 (回折)、レイトレースは広域を担当し、ハイブリッドが既定。
// 環境モデルのファイル名は環境モデルの選択に追従する
//   (モックの defaultValue={env==="indoor" ? "office_floor3.ifc" : "city_shibuya.osm"})。
// モックは静的プロトタイプのため、設定は全てローカル state (.ofd 非対応)。
#pragma once
#include <QScrollArea>

class QButtonGroup;
class QCheckBox;
class QLineEdit;
class QTableWidget;

namespace ofd {

class Project;

class ChannelTab : public QScrollArea {
    Q_OBJECT
public:
    explicit ChannelTab(Project *project, QWidget *parent = nullptr);

private slots:
    void refresh();
    void onEnvChanged();           // 環境モデル → 間取り/地形ファイルの既定値

private:
    void fillMetricsTable();

    Project *m_p;
    bool     m_updating = false;
    int      m_envIdx = 0;         // mock: useState("indoor") 0=indoor

    // 伝搬・チャネル解析 / propagation
    QButtonGroup *m_env = nullptr;
    QButtonGroup *m_band = nullptr;
    QButtonGroup *m_method = nullptr;

    // 環境モデル / environment
    QLineEdit *m_envFile = nullptr;
    QCheckBox *m_matDb = nullptr;
    QCheckBox *m_matScatter = nullptr;

    // 送受信 / TX-RX
    QLineEdit    *m_apCount = nullptr;
    QCheckBox    *m_mimo = nullptr;
    QCheckBox    *m_beamforming = nullptr;
    QButtonGroup *m_rxKind = nullptr;

    // チャネル特性 / channel metrics
    QTableWidget *m_metrics = nullptr;
};

} // namespace ofd
