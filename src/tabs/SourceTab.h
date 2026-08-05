// SourceTab.h — feeds, plane wave, observation points (波源・観測点タブ).
// Maps 1:1 to the "feed =", "planewave =", "point =" lines.
//
// mock (tabs.jsx SourceTab) の構成:
//   波源の種類 / Source type — 給電点 か 平面波入射 の排他選択 (表示フィルタ)
//   給電点 (feed)           — feed = 行の表
//   平面波入射 (planewave)  — θ / φ / 偏波
//   波形 (src_waveform)     — ガウシアンパルス/正弦波/リッカー/周波数掃引 +
//                             プレビュー波形 (.ofd に無いのでローカル状態)
//   観測点 (point)          — point = 行の表
#pragma once
#include <QScrollArea>

class QTableWidget;
class QCheckBox;
class QLineEdit;
class QComboBox;
class QLabel;
class QButtonGroup;
class QRadioButton;
class QFormLayout;
class QPushButton;

namespace ofd {

class Project;
class MiniPlot;

class SourceTab : public QScrollArea {
    Q_OBJECT
public:
    explicit SourceTab(Project *project, QWidget *parent = nullptr);

    // 「🎤 音源/WAV/指向性 タブへ」の飛び先 (左ナビのキー)。ボタンが黙って
    // 効かなくならないよう、キーが NavCatalog に存在することを selftest が
    // 検証できるようにここへ出してある (AcousticTab::workflowNavKey と同じ流儀)。
    static const char *acSourceNavKey() { return "acsource"; }

signals:
    // 「🎤 音源/WAV/指向性 タブへ」が押された → 左ナビをこのキーへ切り替えて
    // ほしい (タブ間の直接依存を作らないよう MainWindow が中継する)。
    void navigateRequested(const QString &navKey);

private slots:
    void refresh();

private:
    void applyFeeds();
    void applyPoints();
    void updateExclusiveWarning();
    void updateSourceType();     // 波源の種類 → 給電点/平面波セクションの表示切替
    void updateWaveform();       // 波形の種類 → パラメータ行 + プレビュー更新
    void updateDomainVisibility(); // ドメイン別の出し分け (Z0 列 / 偏波 / 平面波)

    Project      *m_p;
    bool          m_updating = false;
    QTableWidget *m_feeds;
    QCheckBox    *m_pwEnable;
    QLineEdit    *m_pwTheta, *m_pwPhi;
    QComboBox    *m_pwPol;
    // mock の平面波「振幅」欄。.ofd の planewave = θ φ pol に振幅キーは無い
    // (振幅は給電点の電圧が担う) ので、ここはローカル状態のみ。
    QLineEdit    *m_pwAmp = nullptr;
    // 平面波セクションのフォーム (偏波行のドメイン別出し分けに使う)
    QFormLayout  *m_pwForm = nullptr;
    QTableWidget *m_points;
    QLabel       *m_warning;
    // 音響ドメインのみ表示: 励振波形と音源リスト WAV の役割分担の注記
    QLabel       *m_acWaveNote = nullptr;
    // 音響/水中で表示: 音源リストタブ (acsource) へ移動するボタン
    QPushButton  *m_acGotoSrc = nullptr;

    // ── 波源の種類 (mock: 波源の種類 / Source type) ──────────────────────────
    // 表示フィルタのみのローカル状態。モデルの平面波 ON/OFF は従来どおり
    // so_pw_enable チェックボックスが唯一の源 (既存の保存挙動を変えない)。
    QRadioButton *m_srcFeed  = nullptr;
    QRadioButton *m_srcPlane = nullptr;
    QWidget      *m_feedSection = nullptr;
    QWidget      *m_pwSection   = nullptr;

    // ── 波形 (mock: src_waveform) ────────────────────────────────────────────
    // .ofd の pulsewidth / frequency は「全般」タブが所有するので、ここは
    // モックどおりのローカル状態 + プレビュー描画のみ (永続化しない)。
    QButtonGroup *m_waveGroup = nullptr;
    int           m_wave = 0;          // 0=pulse 1=cw 2=ricker 3=sweep
    QWidget      *m_rowPulseWidth = nullptr;
    QWidget      *m_rowF0 = nullptr;
    QWidget      *m_rowPeak = nullptr;
    QWidget      *m_rowFmin = nullptr;
    QWidget      *m_rowFmax = nullptr;
    QWidget      *m_rowSweep = nullptr;
    MiniPlot     *m_wavePlot = nullptr;
    // 周波数行のラベル (ドメイン別に単位表記を切り替える — 値はローカル
    // プレビュー専用でモデルへ保存されないため、換算は発生しない)
    QLabel       *m_f0Label   = nullptr;
    QLabel       *m_fminLabel = nullptr;
    QLabel       *m_fmaxLabel = nullptr;
};

} // namespace ofd
