// AcousticSourceTab.h — 音源モデリングタブ (acoustic-source.jsx 相当)。
// AFMG EASE / Odeon / CATT-Acoustic 風の音源モジュール:
//   音源リスト          — スピーカー/ソナー配置表 + 共通設定
//   入力信号 (WAV)      — 無響録音ファイル入力 + ライブラリ + 波形プレビュー
//   指向性              — CLF/GLL 指向性 + 帯域別表 + ポーラプロット + 周波数特性
//   アレイ・ライン音源  — ラインアレイ / ビームステアリング / サブアレイ
//   可聴化 Auralization — IRF 畳み込み (HRTF/Ambisonics) + A/B 試聴
// 音響/水中ドメインで表示され、水中選択時はソナー音源リストに切替わる。
#pragma once
#include <QScrollArea>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QTabWidget;

namespace ofd {

class Project;
class MiniPlot;

// 水平面ポーラパターン描画 (一次指向性 r = a + b·cosθ — mock の SVG を
// 係数パラメトリック化したもの)
class PolarPatternView : public QWidget {
    Q_OBJECT
public:
    explicit PolarPatternView(QWidget *parent = nullptr);
    // 一次指向性の係数を設定 (a+b = 1 に正規化された値を想定)。
    // omni: (1,0) / cardioid: (0.5,0.5) / super: (0.37,0.63) /
    // hyper: (0.25,0.75) / fig-8: (0,1)
    void setPattern(double a, double b);
protected:
    void paintEvent(QPaintEvent *) override;
private:
    double m_a = 0.5, m_b = 0.5;   // 既定はカーディオイド
};

class AcousticSourceTab : public QScrollArea {
    Q_OBJECT
public:
    explicit AcousticSourceTab(Project *project, QWidget *parent = nullptr);

private slots:
    void refresh();              // model → widgets
    void onDomainChanged();      // 音響 ⇔ 水中 で音源リスト等を切替

private:
    void apply();
    QWidget *buildSourcesPage();
    QWidget *buildSignalPage();
    QWidget *buildDirectivityPage();
    QWidget *buildArrayPage();
    QWidget *buildAuralPage();
    void fillSourceTable(bool underwater);
    bool isUnderwater() const;
    // 選択 WAV を実読込して包絡線とレベル指標を表示 (QThread で非同期)
    void loadWavPreview(const QString &path);
    // 選択された解析指向性モデルをポーラ図・特性値へ反映
    void updateDirectivity();

    Project    *m_p;
    bool        m_updating = false;
    QTabWidget *m_tabs;

    // sources
    QLabel       *m_srcHint;
    QTableWidget *m_srcTable;
    QPushButton  *m_presetBtn;
    QLineEdit    *m_baseSpl;
    QLabel       *m_baseSplUnit;

    // signal
    QComboBox    *m_sigKind;
    QLineEdit    *m_wavFile;
    QTableWidget *m_libTable;
    MiniPlot     *m_wavePlot;
    QLabel       *m_wavStats = nullptr;    // RMS/Peak/Crest (実計算後に更新)
    QLabel       *m_previewNote = nullptr; // 見本表示の注記 (実読込後は隠す)
    QLabel       *m_srateValue = nullptr;  // サンプリングレート表示
    QString       m_previewPath;           // 実読込済みファイル (再読込防止)
    bool          m_previewBusy = false;   // 非同期読込中

    // directivity
    QComboBox    *m_dirModel, *m_dirSource;
    QLineEdit    *m_gllFile;
    MiniPlot     *m_freqResp;
    PolarPatternView *m_polar = nullptr;
    QLabel       *m_polarInfo = nullptr;    // ビーム幅 / F/B / Q (閉形式)
    QLabel       *m_polarClfNote = nullptr; // CLF/GLL 選択時のみ表示する注記

    // aural
    QComboBox    *m_renderRate;
};

} // namespace ofd
