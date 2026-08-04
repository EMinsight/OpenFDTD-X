// ChannelTab.h — 電波伝搬・チャネル解析タブ (em-applications.jsx ChannelTab 相当)。
// 屋内・市街地・車内・トンネルの電波カバレッジとチャネル特性を扱う。
// FDTD は近傍・小規模 (回折)、レイトレースは広域を担当し、ハイブリッドが既定。
// 環境モデルのファイル名は環境モデルの選択に追従する
//   (モックの defaultValue={env==="indoor" ? "office_floor3.ifc" : "city_shibuya.osm"})。
// モックは静的プロトタイプのため、設定は全てローカル state (.ofd 非対応)。
//
// チャネル特性表は「リンク条件」欄の入力 (周波数・距離・アンテナ高・EIRP・
// 帯域幅・雑音指数・大地反射係数) を見通し内の伝搬モデル
// (src/em/RadioPropagation: Friis / 2 波 / 熱雑音 / Shannon) に入れて
// **実計算**する。多重波の統計が要る指標 (RMS 遅延スプレッド・角度スプレッド)
// はレイトレース / FDTD の実行が必要なため「未計算」と表示する。
#pragma once
#include <QScrollArea>

class QButtonGroup;
class QCheckBox;
class QLabel;
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
    void recompute();              // リンク条件 → チャネル特性 (実計算)

private:
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

    // リンク条件 / link budget inputs (チャネル特性の計算入力)
    QLineEdit *m_freqGHz = nullptr;   // 中心周波数 [GHz]
    QLineEdit *m_dist = nullptr;      // 送受信距離 [m]
    QLineEdit *m_hTx = nullptr;       // 送信アンテナ高 [m]
    QLineEdit *m_hRx = nullptr;       // 受信アンテナ高 [m]
    QLineEdit *m_eirp = nullptr;      // 送信 EIRP [dBm]
    QLineEdit *m_gRx = nullptr;       // 受信アンテナ利得 [dBi]
    QLineEdit *m_bw = nullptr;        // 帯域幅 [MHz]
    QLineEdit *m_nf = nullptr;        // 受信機雑音指数 [dB]
    QLineEdit *m_refl = nullptr;      // 大地反射係数 |Γ| (0..1)
    QLabel    *m_inputError = nullptr;

    // チャネル特性 / channel metrics
    QTableWidget *m_metrics = nullptr;
};

} // namespace ofd
