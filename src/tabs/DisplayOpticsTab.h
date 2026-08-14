// DisplayOpticsTab.h — ディスプレイ・AR/VR光学タブ
//                       (optical-applications.jsx DisplayOpticsTab 相当)。
// FDTD/RCWA で格子・微細構造、レイトレースで導光と瞳を解析するマルチスケール設計。
// デバイス Seg でセクションが丸ごと切り替わる:
//   - AR導波路コンバイナ : 導波路コンバイナ (方式/基板/格子) + 評価表
//   - OLED光取り出し     : 構造・損失分離・取り出し構造 + EQE
//   - microLED / LCD偏光系: 1 つのセクションを共有し、タイトルと中身の両方が切替
//
// フォームは Project::displayOptics() (.ofdx "display_optics") の View。
// 評価量は `optics/DisplayMetrics` の解析式 (導波路の格子/TIR 帯域、フレネル
// 透過率、射出円錐、側壁再結合、環境光コントラスト) で **実計算** する。
// 実光線追跡 / RCWA / Berreman 4x4 が要る量 (輝度均一性・迷光・視野角特性) は
// 値を出さず「—」と表示する (絶対規則 5: 未実装を動作済みに見せない)。
#pragma once
#include <QScrollArea>

class QButtonGroup;
class QCheckBox;
class QLabel;
class QLineEdit;
class QStackedWidget;
class QTableWidget;

#include "../optics/DisplayMetrics.h"

namespace ofd {

class Project;
class SectionBox;
class MiniPlot;

class DisplayOpticsTab : public QScrollArea {
    Q_OBJECT
public:
    explicit DisplayOpticsTab(Project *project, QWidget *parent = nullptr);

    void apply();      // widgets → model
    void refresh();    // model → widgets (m_updating ガード付き)

private slots:
    void deviceChanged(int index);      // デバイス切替 → セクションの出し入れ
    void onEdited();                    // apply() + 評価量の再計算

private:
    QWidget *buildArwgPage();           // AR導波路コンバイナ (2 セクション)
    QWidget *buildOledPage();           // OLED光取り出し
    QWidget *buildMicroLedPage();       // microLED 解析 (共有セクションの中身)
    QWidget *buildLcdPage();            // LCD/偏光系 解析 (共有セクションの中身)

    void recomputeArwg();               // 導波路コンバイナの評価表
    // 評価表と同じ帯域・同じ閉形式で 2 つの図を作る (別計算にしない)
    void refreshArwgPlots(const ofd::displayoptics::WaveguideFov &fov);
    void recomputeOled();
    void recomputeMicroLed();
    void recomputeLcd();
    void recomputeAll();

    Project        *m_p;
    bool            m_updating = false;

    // 上段
    QButtonGroup   *m_device;           // arwg / oled / microled / lcd

    // デバイス別ページ
    QWidget        *m_arwgPage;
    QWidget        *m_oledPage;
    SectionBox     *m_sharedSec;        // microLED / LCD で共有 (タイトルも切替)
    QStackedWidget *m_sharedStack;

    // 導波路コンバイナ
    QButtonGroup   *m_wgType;           // SRG / VHG / PVG / 幾何
    QLineEdit      *m_subThick;
    QLineEdit      *m_subIndex;
    QLineEdit      *m_gratPeriod;
    QLineEdit      *m_gratDepth;
    QLineEdit      *m_gratSlant;
    QLineEdit      *m_designLambda;
    QLineEdit      *m_guideMax;
    QLineEdit      *m_outcouplerLen;
    QLineEdit      *m_eyeRelief;
    QLineEdit      *m_fovTarget;
    QLineEdit      *m_eyeboxTarget;
    QLineEdit      *m_seeThroughTarget;
    QCheckBox      *m_threeGratings;
    QCheckBox      *m_rcwaOptimize;
    QTableWidget   *m_metricTable;
    // 押されるまでは隠す 2 つの図 (アイボックス幅 / FOV トレードオフ)
    MiniPlot       *m_eyeboxPlot = nullptr;
    QLabel         *m_eyeboxNote = nullptr;
    MiniPlot       *m_tradeoffPlot = nullptr;
    QLabel         *m_tradeoffNote = nullptr;
    bool            m_showEyebox = false;
    bool            m_showTradeoff = false;

    // OLED
    QCheckBox      *m_bottomEmission;
    QCheckBox      *m_topEmission;
    QCheckBox      *m_microcavity;
    QCheckBox      *m_iqe;
    QCheckBox      *m_sppLoss;
    QCheckBox      *m_waveguideLoss;
    QButtonGroup   *m_outcoupling;      // なし / マイクロレンズ / 散乱層 / PhC
    QLineEdit      *m_oledIndex;
    QLineEdit      *m_oledIqe;
    QLabel         *m_eqeBadge;
    QTableWidget   *m_oledTable;

    // microLED
    QLineEdit      *m_chipSize;
    QLineEdit      *m_mlIndex;
    QLineEdit      *m_mlIqe;
    QLineEdit      *m_mlSurfVel;
    QLineEdit      *m_mlLifetime;
    QCheckBox      *m_sidewallRecomb;
    QCheckBox      *m_sidewallDbr;
    QCheckBox      *m_directional;
    QLabel         *m_microLedBadge;
    QTableWidget   *m_mlTable;

    // LCD/偏光系
    QButtonGroup   *m_lcdMode;          // TN / IPS / VA
    QCheckBox      *m_lcAnisotropy;
    QCheckBox      *m_compFilm;
    QLineEdit      *m_lcdPeakLum;
    QLineEdit      *m_lcdCr;
    QLineEdit      *m_lcdAmbient;
    QLineEdit      *m_lcdRefl;
    QLabel         *m_lcdBadge;
    QTableWidget   *m_lcdTable;
};

} // namespace ofd
