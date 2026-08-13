// LensEditorTab.h — レンズデータエディタ (optics-tabs.jsx LensEditorTab 相当)。
//   - Lens Data Editor: Zemax OpticStudio 風の面テーブル (初期値 Cooke triplet)。
//     ガラス欄は GlassCatalog から自動補完、行の挿入/削除、STO 行ハイライト。
//   - システム諸元 (入射瞳径 / 視野 / 波長サンプル / 座標系)
//   - 近軸諸元 : 面テーブルから y-nu 近軸追跡で焦点距離・主点・F 値等を実計算
//   - Merit Function (FoM) オペランド表 + 最適化ボタン
//     (近軸オペランド EFFL/PIMH/ISFN と収差オペランド SPHA/COMA/ASTI/DIST を
//      実計算。後者は 3 次収差 (ザイデル和) で、近軸追跡だけで決まる)
//   - 3 次収差 (ザイデル) : 面ごとの寄与と総和 (optics/SeidelAberration)
//   - 解析プロット : スポットダイアグラム・光線収差図・色収差の焦点移動を
//     実光線追跡 (optics/RayTrace) で計算する。MTF 等の残り 5 種は未実装
//   - 面テーブルから子午面 2D 光線追跡するレイアウトプレビュー
// 光ドメイン選択時のみ表示される。面データは Project (.ofdx) に保存し、
// 光解析タブの「光学系定義」節と共有する (既定のままなら保存しない)。
#pragma once
#include <QColor>
#include <QPointF>
#include <QScrollArea>
#include <QString>
#include <QStringList>
#include <QVector>

#include <vector>

#include "../core/Project.h"
#include "../optics/ParaxialTrace.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

namespace ofd {

class MiniPlot;
class Project;

// 1面ぶんの行データ。**実体は core/Project の LensSurfaceRow** —
// 面テーブルはプロジェクト (.ofdx) に保存し、光解析タブとも共有する。
using LensSurface = LensSurfaceRow;

// 面テーブルから描くレンズ断面 + 子午光線追跡プレビュー (QPainter)
class LensLayoutView : public QWidget {
    Q_OBJECT
public:
    explicit LensLayoutView(QWidget *parent = nullptr);
    void setSystem(const QVector<LensSurface> &rows, double epd, double fieldDeg);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    struct Surf {
        double z = 0, R = 0, semiD = 7, conic = 0;
        double n2 = 1.0;         // 面の後ろ側の屈折率
        bool   plane = false, stop = false, image = false;
    };
    QVector<Surf> m_surfs;
    double m_epd = 12.0, m_field = 20.0;
};

// スポットダイアグラム表示 (縦横同スケール + エアリー円)。
// 散布図で縦横比が崩れると形の判断ができないので、MiniPlot ではなく
// 専用に描く (レイファンは折れ線なので MiniPlot を使う)。
class SpotDiagramView : public QWidget {
    Q_OBJECT
public:
    explicit SpotDiagramView(QWidget *parent = nullptr);

    struct Cloud {
        QVector<QPointF> pts;    // 像面上の位置 [mm]
        QPointF centroid;
        QColor  color;
        QString label;
    };
    // airyRadius <= 0 ならエアリー円を描かない
    void setClouds(const QVector<Cloud> &clouds, double airyRadius);
    void clear();

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QVector<Cloud> m_clouds;
    double m_airy = 0.0;
};

// Merit Function の 1 オペランド。目標と重みは利用者が編集できる定義値、
// 「値」は近軸追跡と 3 次収差で計算できるものだけを埋める。
struct MeritOperand {
    QString code;      // EFFL / PIMH / ISFN / SPHA / COMA / ASTI / DIST
    QString label;     // 表示名
    QString target;    // 目標値 (編集可能)
    QString weight;    // 重み   (編集可能)
};

class LensEditorTab : public QScrollArea {
    Q_OBJECT
public:
    explicit LensEditorTab(Project *project, QWidget *parent = nullptr);

private slots:
    void retrace();                 // 面テーブル + 諸元 → プレビュー再追跡
    void runSpotDiagram();          // ⊙ スポットダイアグラム (実光線追跡)
    void runRayFan();               // 📐 レイファン (光線収差図)
    void runChromatic();            // 🌈 色収差 (波長ごとの焦点移動)
    void runMtf();                  // 📊 MTF (回折限界 + 幾何)
    void runEncircled();            // ⬡ 包絡エネルギー (幾何)
    void runDistortion();           // ▦ 歪曲格子 (主光線 vs 近軸)
    void runFieldCurvature();       // ⌖ 像面湾曲 (実光線の交点)
    void runOptimize();             // ▶ 最適化 (減衰最小二乗、書き戻しはしない)
    void applyOptimize();           // 提案値を面テーブルへ適用
    void addWavelength();           // + 波長サンプルの追加

private:
    void rebuildTable();            // m_rows → QTableWidget (行挿入/削除後)
    void syncRowFromTable(int row); // QTableWidget → m_rows (セル編集後)
    void applyStopHighlight();      // STO 行の背景ハイライト
    void rebuildMeritTable();       // m_fom → Merit 表 (目標/重みは編集可能)
    void recomputeParaxial();       // 面テーブル → 近軸諸元 + 収差 + Merit
    // 面テーブル → 近軸面の並び (像面までの距離と屈折率仮定の銘柄も返す)。
    // 近軸諸元・3 次収差・実光線追跡が同じ表から系を作るための共通処理。
    // lambda_um は屈折率を評価する波長 [µm]。**0 以下 = カタログの実測 nd
    // (d 線)** で、これが既定。正の値なら Sellmeier 分散式で引き直す。
    std::vector<paraxial::Surface> collectSurfaces(double *imageDistance,
                                                  QStringList *assumedGlass,
                                                  double lambda_um = 0.0) const;
    void rebuildWaveBadges();       // m_waves → 波長バッジの行
    void loadRowsFromProject();     // .ofdx の面テーブル (空なら既定値)
    void pushRowsToProject();       // 編集結果を .ofdx へ (既定のままなら書かない)
    double epdValue() const;
    double fieldValue() const;

    Project      *m_p;
    bool          m_updating = false;
    QVector<LensSurface> m_rows;
    QLineEdit  *m_optTarget = nullptr;   // 目標 f'
    QCheckBox  *m_optSpot = nullptr;     // スポット RMS も目標にする
    QLineEdit  *m_optWeight = nullptr;   // その重み
    QPushButton *m_optApply = nullptr;   // 提案値の適用
    QLabel     *m_optInfo = nullptr;
    QVector<QPair<int, double>> m_optSolution;   // (行, 提案する R)
    QVector<MeritOperand> m_fom;

    QTableWidget *m_table;
    QTableWidget *m_merit = nullptr;
    QTableWidget *m_paraxial = nullptr;
    QTableWidget *m_seidel = nullptr;
    QLineEdit    *m_epd, *m_field;
    QComboBox    *m_coord;
    LensLayoutView *m_layout;
    // 波長サンプル [nm]。昇順・重複なし。プロジェクトには保存しない
    // (解析の見方の設定であって設計データではないため)。
    QVector<double> m_waves;
    QWidget        *m_waveBadges = nullptr;
    // 解析プロット (実光線追跡)
    SpotDiagramView *m_spotView = nullptr;
    MiniPlot        *m_fanPlot = nullptr;
    QLabel          *m_anInfo = nullptr;   // 数値 (RMS / GEO / エアリー半径)
    QLabel          *m_anNote = nullptr;   // 前提と限界
};

} // namespace ofd
