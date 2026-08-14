// IlluminationTab.h — 照明光学・測色タブ (optical-applications.jsx IlluminationTab 相当)。
// 非結像光学系の配光設計と測色評価 (LightTools / Photopia 相当):
//   - 用途 (LED照明 / 車載ランプ / バックライト / 太陽光集光)
//   - 光源   : モデル (ランバート/レイデータ/LEDチップ) + レイデータ + スペクトル + 光束
//   - 光学系 : リフレクタ・TIRレンズ・拡散板・導光板・蛍光体散乱 + 表面特性
//   - 測光・測色: 指標判定表と配光ファイル書出
//
// フォームは Project::illumination() (.ofdx "illumination") の View。
// スペクトルは解析モデル (ガウシアンローブ / 黒体) で定義され、測色量
// (色度 x,y・u',v'・CCT・Duv・発光効率) は `optics/Colorimetry` で **実計算**
// する。配光に依存する量 (系の全光束・光学効率・ビーム角・均斉度・軸上光度・
// 中心照度) は `optics/IlluminationTrace` の非順次モンテカルロ・レイトレースで
// **実計算**する。分光反射率の数表が要る量 (Ra・TM-30)、波長分解の追跡が要る
// 色ムラ、輝度分布が要る UGR は値を出さず「—」と表示する
// (絶対規則 5: 未実装を動作済みに見せない)。
//
// 追跡モデルに入っていない要素 (TIR レンズ・導光板・蛍光体散乱・BSDF 実測・
// 実測レイデータ) が選ばれているときは、配光量を計算せずその理由を出す。
#pragma once
#include <QScrollArea>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QStackedWidget;
class QTableWidget;

namespace ofd {

class Project;
namespace illum { struct Result; }

class IlluminationTab : public QScrollArea {
    Q_OBJECT
public:
    explicit IlluminationTab(Project *project, QWidget *parent = nullptr);

    void apply();     // widgets → model
    void refresh();   // model → widgets (m_updating ガード付き)

private slots:
    void onEdited();  // apply() + 測色量の再計算

private:
    void recompute();          // スペクトルモデル → 測色表、追跡 → 配光量
    void updateSpectrumPage(); // スペクトル選択 → パラメータ欄の切替

    // 現在の設定で追跡する。追跡モデルに入っていない選択や不正な幾何では
    // false を返す (理由は測光・測色表の「未計算」行に出ている)
    bool traceNow(illum::Result *out, long long *rays) const;
    void showPolarPlot();       // 配光曲線 (極座標)
    void showIlluminanceMap();  // 評価面の照度分布
    void exportIes();           // IES LM-63 書出
    void exportLdt();           // EULUMDAT (.ldt) 書出

    Project      *m_p;
    bool          m_updating = false;

    // 上段
    QButtonGroup *m_app;            // 用途

    // 光源
    QButtonGroup *m_srcModel;       // ランバート面 / レイデータ / LEDチップ
    QLineEdit    *m_rayFile;
    QComboBox    *m_spectrum;
    QLineEdit    *m_flux;
    QLineEdit    *m_rays;

    // スペクトルモデルのパラメータ (選択に応じて切替)
    QStackedWidget *m_spectrumStack;
    QLineEdit    *m_bluePeak, *m_blueFwhm, *m_phosPeak, *m_phosFwhm, *m_phosRatio;
    QLineEdit    *m_rPeak, *m_rFwhm, *m_rRatio;
    QLineEdit    *m_gPeak, *m_gFwhm, *m_gRatio;
    QLineEdit    *m_bPeak, *m_bFwhm, *m_bRatio;
    QLineEdit    *m_blackbody;
    QLineEdit    *m_monoPeak, *m_monoFwhm;

    // 設計目標
    QLineEdit    *m_cctTarget, *m_cctTol, *m_duvTol;

    // 光学系
    QCheckBox    *m_reflector;
    QCheckBox    *m_tirLens;
    QCheckBox    *m_diffuser;
    QCheckBox    *m_lightGuide;
    QCheckBox    *m_phosphor;
    QButtonGroup *m_surface;        // 鏡面 / 拡散 / BSDF実測 / ABGモデル

    // 非順次レイトレースの幾何 (optics/IlluminationTrace への入力)
    QLineEdit    *m_reflF, *m_reflR, *m_reflRho;
    QLineEdit    *m_diffZ, *m_diffR, *m_diffTau;
    QLineEdit    *m_abgA, *m_abgB, *m_abgG;
    QLineEdit    *m_tgtD, *m_tgtW, *m_chip;

    // 測光・測色
    QTableWidget *m_photoTable;
};

} // namespace ofd
