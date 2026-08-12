// ChannelTab.h — 電波伝搬・チャネル解析タブ (em-applications.jsx ChannelTab 相当)。
// 屋内・市街地・車内・トンネルの電波カバレッジとチャネル特性を扱う。
// FDTD は近傍・小規模 (回折)、レイトレースは広域を担当し、ハイブリッドが既定。
// 環境モデルのファイル名は環境モデルの選択に追従する
//   (モックの defaultValue={env==="indoor" ? "office_floor3.ifc" : "city_shibuya.osm"})。
// モックは静的プロトタイプのため、設定は全てローカル state (.ofd 非対応)。
//
// 受信点を「格子」にするとカバレッジ図 (2 波モデルの受信電力 [dBm]) を描く。
// 経路・個別点は配置の入力が要るので未実装のまま (図は出さずに理由を出す)。
//
// チャネル特性表は「リンク条件」欄の入力 (周波数・距離・アンテナ高・EIRP・
// 帯域幅・雑音指数・大地反射係数) を見通し内の伝搬モデル
// (src/em/RadioPropagation: Friis / 2 波 / 熱雑音 / Shannon) に入れて
// **実計算**する。多重波の統計が要る指標 (RMS 遅延スプレッド・角度スプレッド)
// はレイトレース / FDTD の実行が必要なため「未計算」と表示する。
//
// 環境の選択 (屋内 / 市街地 / 車内・車車間 / トンネル) は経路損失の**経験式**
// を選ぶ: 屋内 = ITU-R P.1238、市街地 = 奥村-秦 / COST-231 Hata、
// 車内・車車間 = 2 波モデル。適用範囲の外では値を出さず理由を書く
// (経験式の外挿はしない)。受信電力・SNR・容量はこの損失を使う。
#pragma once
#include <QScrollArea>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QTableWidget;

namespace ofd {

class Project;
class FieldHeatmap;

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
    // 経路損失モデル (環境の選択から決まる) とそのパラメータ
    QLabel         *m_modelName = nullptr;
    QDoubleSpinBox *m_indoorN = nullptr;    // ITU-R P.1238 の距離損失係数 N
    QCheckBox      *m_largeCity = nullptr;  // 奥村-秦 の大都市補正
    QWidget        *m_indoorNRow = nullptr;
    QWidget        *m_largeCityRow = nullptr;

    // 送受信 / TX-RX
    QLineEdit    *m_apCount = nullptr;
    // 複数 AP のカバレッジ (配置半径・図に出す量・カバー判定の閾値)
    QLineEdit    *m_apRadius = nullptr;
    QComboBox    *m_covQuantity = nullptr;
    QLineEdit    *m_covThreshold = nullptr;
    QCheckBox    *m_mimo = nullptr;
    QCheckBox    *m_beamforming = nullptr;
    void updateCoverage(double dist, double ht, double hr, double f,
                        double eirp, double grx, double refl,
                        double lam, double noiseBw_hz, double noiseFigureDb);

    QButtonGroup *m_rxKind = nullptr;
    FieldHeatmap *m_coverage = nullptr;   // カバレッジ格子 (受信点=格子のとき)
    QLabel       *m_coverageNote = nullptr;

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
