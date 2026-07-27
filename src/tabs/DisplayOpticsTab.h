// DisplayOpticsTab.h — ディスプレイ・AR/VR光学タブ
//                       (optical-applications.jsx DisplayOpticsTab 相当)。
// FDTD/RCWA で格子・微細構造、レイトレースで導光と瞳を解析するマルチスケール設計。
// デバイス Seg でセクションが丸ごと切り替わる:
//   - AR導波路コンバイナ : 導波路コンバイナ (方式/基板/格子) + 評価表
//   - OLED光取り出し     : 構造・損失分離・取り出し構造 + EQE バッジ
//   - microLED / LCD偏光系: 1 つのセクションを共有し、タイトルと中身の両方が切替
// 表示専用 (.ofd に対応フィールドが無いため状態はすべてローカル)。
#pragma once
#include <QScrollArea>

class QButtonGroup;
class QCheckBox;
class QLabel;
class QLineEdit;
class QStackedWidget;
class QTableWidget;

namespace ofd {

class Project;
class SectionBox;

class DisplayOpticsTab : public QScrollArea {
    Q_OBJECT
public:
    explicit DisplayOpticsTab(Project *project, QWidget *parent = nullptr);

private slots:
    void deviceChanged(int index);      // デバイス切替 → セクションの出し入れ

private:
    QWidget *buildArwgPage();           // AR導波路コンバイナ (2 セクション)
    QWidget *buildOledPage();           // OLED光取り出し
    QWidget *buildMicroLedPage();       // microLED 解析 (共有セクションの中身)
    QWidget *buildLcdPage();            // LCD/偏光系 解析 (共有セクションの中身)

    Project        *m_p;

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
    QLineEdit      *m_gratPeriod;
    QLineEdit      *m_gratDepth;
    QLineEdit      *m_gratSlant;
    QCheckBox      *m_threeGratings;
    QCheckBox      *m_rcwaOptimize;
    QTableWidget   *m_metricTable;

    // OLED
    QCheckBox      *m_bottomEmission;
    QCheckBox      *m_topEmission;
    QCheckBox      *m_microcavity;
    QCheckBox      *m_iqe;
    QCheckBox      *m_sppLoss;
    QCheckBox      *m_waveguideLoss;
    QButtonGroup   *m_outcoupling;      // なし / マイクロレンズ / 散乱層 / PhC
    QLabel         *m_eqeBadge;
    QLabel         *m_oledDetail;

    // microLED
    QLineEdit      *m_chipSize;
    QCheckBox      *m_sidewallRecomb;
    QCheckBox      *m_sidewallDbr;
    QCheckBox      *m_directional;
    QLabel         *m_microLedBadge;

    // LCD/偏光系
    QButtonGroup   *m_lcdMode;          // TN / IPS / VA
    QCheckBox      *m_lcAnisotropy;
    QCheckBox      *m_compFilm;
    QLabel         *m_lcdBadge;
};

} // namespace ofd
