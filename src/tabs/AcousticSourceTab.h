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

// 水平面ポーラパターン描画 (カーディオイド @1kHz — mock の SVG 相当)
class PolarPatternView : public QWidget {
    Q_OBJECT
public:
    explicit PolarPatternView(QWidget *parent = nullptr);
protected:
    void paintEvent(QPaintEvent *) override;
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

    // directivity
    QComboBox    *m_dirModel, *m_dirSource;
    QLineEdit    *m_gllFile;
    MiniPlot     *m_freqResp;

    // aural
    QComboBox    *m_renderRate;
};

} // namespace ofd
