// Post2Tab.h — ポスト処理制御(2): 遠方界・近傍界プロット類.
// Maps 1:1 to plotfar0d/plotfar1d/plotfar2d/plotnear1d/plotnear2d
// and their style keys (far1dstyle, far1dcomponent, near2ddim, ...).
//
// mock (tabs.jsx Post1Tab/Post2Tab) の構成のうち、このタブが受け持つもの:
//   遠方界面上(2D) / 全方向(3D) — 表 + 成分 (far2dcomponent の 7 成分:
//                                E/θ/φ/主軸/副軸/左旋/右旋) + スケール指定
//     面の向き = X面/Y面/Z面/φ一定面/θ一定面 (plotfar1d の X/Y/Z/V/H)
//     形式     = 円プロット/XYプロット (far1dstyle 0/1)
//     最大値で正規化 (far1dnorm) / 角度分割数 θ・φ (plotfar2d)
//   近傍界線上(2D)              — plotnear1d の表
//   近傍界面上(2D+3D)           — plotnear2d の表 + 物体を描く/一部拡大/動画 +
//                                描画方法 (カラー塗りつぶし/等高線/ベクトル)
//   エクスポート / Export       — CSV / HDF5 出力 (実体は Runner 側)
#pragma once
#include <QScrollArea>

class QCheckBox;
class QSpinBox;
class QLineEdit;
class QComboBox;
class QTableWidget;

namespace ofd {

class Project;

class Post2Tab : public QScrollArea {
    Q_OBJECT
public:
    explicit Post2Tab(Project *project, QWidget *parent = nullptr);

private slots:
    void refresh();

private:
    void apply();
    void applyFar1dTable();
    void applyNear1dTable();
    void applyNear2dTable();
    // 等高線チェック (near2dcontour) → 描画方法コンボの表示を合わせる
    void syncDrawMethod();

    Project   *m_p;
    bool       m_updating = false;

    // far0d
    QCheckBox *m_far0d;
    QLineEdit *m_far0dTheta, *m_far0dPhi;

    // far1d
    QTableWidget *m_far1d;
    QComboBox *m_far1dStyle;
    QCheckBox *m_far1dDb, *m_far1dNorm;
    QCheckBox *m_far1dCompE, *m_far1dCompTheta, *m_far1dCompPhi;

    // far2d
    QCheckBox *m_far2d;
    QSpinBox  *m_far2dTheta, *m_far2dPhi;
    QCheckBox *m_far2dDb;
    QLineEdit *m_far2dObj;
    // 成分 (far2dcomponent = E Eθ Eφ 主軸 副軸 左旋 右旋) と
    // スケール指定 (far2dscale = min max) — mock の pp_far_2d 成分行 / pp_scale
    QCheckBox *m_far2dComp[7] = { nullptr, nullptr, nullptr, nullptr,
                                  nullptr, nullptr, nullptr };
    QCheckBox *m_far2dUserScale = nullptr;
    QLineEdit *m_far2dMin = nullptr, *m_far2dMax = nullptr;

    // near1d / near2d
    QTableWidget *m_near1d;
    QCheckBox *m_near1dDb, *m_near1dNoinc;
    QTableWidget *m_near2d;
    QSpinBox  *m_near2dDim0, *m_near2dDim1;
    QCheckBox *m_near2dDb, *m_near2dContour, *m_near2dNoinc;
    // mock: 物体を描く (near2dobj) / 一部拡大 (near2dzoom)
    QCheckBox *m_near2dDrawObj = nullptr, *m_near2dZoom = nullptr;
    // near2dobj は 0/1/2 を取りうるので、チェックを外して戻したときに
    // 元の値 (2 など) を失わないよう保持する。
    int        m_near2dObjValue = 1;
    // 動画フレーム数 — .ofd に対応キーが無いのでローカル状態 (mock 既定 100)
    QLineEdit *m_near2dFrames = nullptr;
    // mock の表「動画」列の ON/OFF。フレーム数と違い near2dframe が .ofd に
    // あるのでこちらは永続化する。
    QCheckBox *m_near2dAnim = nullptr;
    // mock: 描画方法 (pp_draw_method) — カラー塗りつぶし / 等高線 / ベクトル。
    // .ofd にあるのは等高線 (near2dcontour) だけなので、コンボは等高線チェックと
    // 双方向に同期させ、「ベクトル」だけをローカル状態として覚えておく。
    QComboBox *m_near2dDrawMethod = nullptr;
    int        m_drawMethod = 0;   // 0=塗りつぶし 1=等高線 2=ベクトル (mock 既定 0)
};

} // namespace ofd
