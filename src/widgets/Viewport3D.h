// Viewport3D.h — central 3D view of the project (mesh region, geometry
// units, feeds, probes). QPainter-based orthographic wireframe so that no
// OpenGL context is required (works in headless / remote sessions too).
//
// Mouse: left-drag = orbit, middle-drag = pan, wheel = zoom, double = fit.
// Drop  : コンポーネントライブラリ (ComponentsTab) のカードを落とすと、
//         その位置にジオメトリ / 給電点 / 観測点を追加する。
#pragma once
#include <QByteArray>
#include <QImage>
#include <QString>
#include <QVector>
#include <QWidget>
#include <QPointF>
#include "../core/Domain.h"

class QDragEnterEvent;
class QDragLeaveEvent;
class QDragMoveEvent;
class QDropEvent;

namespace ofd {

class Project;

// ── コンポーネントのドラッグ&ドロップ契約 (ComponentsTab → Viewport3D) ─────
// アプリ内専用の独自 MIME タイプでカテゴリと名前を運ぶ。ドロップ側の
// 配置仕様 (どの形状コードを作るか) は Viewport3D.cpp が持つ。
namespace ComponentDrop {

// MIME タイプ ("application/x-openfdtd-component")
const char *mimeType();

// カテゴリ + 名前 → MIME データ (UTF-8 / TAB 区切り)
QByteArray encode(const QString &cat, const QString &name);

// MIME データ → カテゴリ + 名前。壊れたデータでは false。
bool decode(const QByteArray &data, QString *cat, QString *name);

// ドロップ配置に対応しているコンポーネントか。
// false のとき why に理由 (I18n 済み。取込モデルなど位置だけでは作れないもの)。
bool canPlace(const QString &cat, const QString &name, QString *why = nullptr);

// ドメインを考慮した判定 (domain は core/Domain.h の domainKey() の文字列)。
// 上の canPlace に加えて core/ComponentCatalog.h のドメイン許可表を確認する:
//   - 水中音響 (underwater) は BELLHOP が配置部品を一切使わないため全部品拒否
//   - 許可表でそのドメインに無い部品 (例: EM でのプラズモニクス) も拒否
// domain が空のときはドメイン判定を行わない (従来動作)。
bool canPlace(const QString &cat, const QString &name, const QString &domain,
              QString *why = nullptr);

} // namespace ComponentDrop

// モックの TweaksPanel「3D ビュー / Viewport」に対応する描画スタイル。
//   Wireframe — 形状を薄い線画で
//   Solid     — 面塗り (既定)
//   Field     — Solid + 結果断面 (実データ) のオーバーレイ。
//               断面が未設定のときは合成パターンを描かず未読込を明示する
//   Rays      — Solid + サンプルのレイ線 (24本 × 4反射。ソルバ結果ではない)
enum class ViewStyle { Wireframe, Solid, Field, Rays };

class Viewport3D : public QWidget {
    Q_OBJECT
public:
    explicit Viewport3D(Project *project, QWidget *parent = nullptr);

    // ドメイン切替。水中音響へ入ったときだけ視点を y 軸方向 (鉛直断面を
    // 正面に見る向き) へ倒す — 解が y = 0 の 1 枚の面なので、斜めから見ると
    // 線にしか見えないため。既に水中音響なら視点は触らない。
    void setDomain(Domain d);
    void setSolidMode(bool solid) { m_solid = solid; update(); }
    bool solidMode() const { return m_solid; }

    void setViewStyle(ViewStyle s);
    ViewStyle viewStyle() const { return m_viewStyle; }
    // 暗いパレット時にビューポートの地色/線色を合わせる
    void setDarkPalette(bool dark) { m_dark = dark; update(); }

    double azimuth() const   { return m_azimuthDeg; }
    double elevation() const { return m_elevationDeg; }
    // グリッド/境界(PML)の表示切替 — モックの Snap/境界チェックボックス相当
    void setGridVisible(bool on)     { m_showGrid = on; update(); }
    void setBoundaryVisible(bool on) { m_showBoundary = on; update(); }

    // ── 深度方向の表示倍率 (水中音響のみ) ──────────────────────────────────
    // 海は 50 km x 3 km のように極端に平たく、等方の縮尺では帯にしか見えない。
    // 1.0 = 等方 (既定)。1 以外のときは倍率を画面に明記する
    // (断りなく縦に伸ばした図は縮尺の嘘になるため)。
    void setVerticalExaggeration(double k);
    double verticalExaggeration() const { return m_vScale; }

    // ── 結果断面 (ソルバが出した実データ) の 3D 表示 ────────────────────────
    // 3D 空間内の 1 平面として重ねて描く (ViewStyle::Field のとき)。
    //   cells : 振幅 (rows*cols, row-major)。0..1 に正規化済みでなくてよい
    //           (ウィジェット側で最大値正規化する)。行 0 = 第 2 軸の +側。
    //   axis  : 0=X 一定 (YZ 面) / 1=Y 一定 (XZ 面) / 2=Z 一定 (XY 面)
    //   pos_m : 固定軸の座標 [m]
    //   u0,u1 : 面内 第1軸の範囲 [m] (axis=0 は y, axis=1 は x, axis=2 は x)
    //   v0,v1 : 面内 第2軸の範囲 [m] (axis=0 は z, axis=1 は z, axis=2 は y)
    //   label : 凡例に出す説明 (データセット名・時刻など)
    //   scaleMax : 正規化に使う最大値。**0 以下なら与えたデータの最大値**
    //           (従来動作)。アニメーションのようにフレームを次々と差し替える
    //           場合、フレームごとの最大値で正規化すると弱いフレームも強い
    //           フレームも同じ明るさになり、時間変化が読めなくなる。呼び側が
    //           共通の最大値を持っているならそれを渡す。
    void setResultSlice(const QVector<double> &cells, int rows, int cols,
                        int axis, double pos_m,
                        double u0, double u1, double v0, double v1,
                        const QString &label, double scaleMax = 0.0);

    // ── 複数断面 (直交する断面を同時に載せる) ─────────────────────────────
    // H5アニメの「3 面ビュー」を 3D にもそのまま出すための入口。1 枚ぶんの
    // 指定を並べて渡す。**正規化は全断面で共通の最大値**に揃える (面ごとに
    // 割ると、同じ色が面によって違う強さを意味することになり、カラーバーが
    // 嘘になる)。共通値は指定 scaleMax の最大、無指定なら全データの最大。
    struct SliceSpec {
        QVector<double> cells;
        int    rows = 0, cols = 0;
        int    axis = 2;              // 0=X 一定 / 1=Y 一定 / 2=Z 一定
        double pos_m = 0.0;
        double u0 = 0.0, u1 = 0.0, v0 = 0.0, v1 = 0.0;
        QString label;
        double scaleMax = 0.0;
    };
    void setResultSlices(const QVector<SliceSpec> &specs);

    // 再生コントロールの状況を凡例へ出す (H5アニメ再生中のコマ位置)。
    // frameCount <= 0 なら表示しない (静止画の断面では出さない)。
    void setSlicePlayback(int frame, int frameCount, bool playing);

    void clearResultSlice();
    bool hasResultSlice() const { return !m_slices.isEmpty(); }

public slots:
    void fitView();
    void setAzimuth(double deg);
    void setElevation(double deg);
    // 0 = XY (上から), 1 = YZ (X軸方向から), 2 = ZX (Y軸方向から)
    void setViewPlane(int plane);

signals:
    // マウス操作で視点が変わったときに発火 (ツールバーのスライダー同期用)
    void viewChanged(double azimuthDeg, double elevationDeg);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void wheelEvent(QWheelEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    // コンポーネントのドラッグ&ドロップ配置
    void dragEnterEvent(QDragEnterEvent *) override;
    void dragMoveEvent(QDragMoveEvent *) override;
    void dragLeaveEvent(QDragLeaveEvent *) override;
    void dropEvent(QDropEvent *) override;

private:
    QPointF projectPoint(double x, double y, double z) const;
    // 正射影の奥行き (**大きいほど手前**)。projectPoint と同じ基底の第 3 軸
    // e3 = (−sinA·sinE, cosA·sinE, cosE) との内積。画家のアルゴリズムで
    // 断面片を並べ替えるのに使う (io/MeshProjection と同じ向きの規約)。
    double sceneDepth(double x, double y, double z) const;

    // ── シーンの範囲と投影変換 (paintEvent とドロップ処理で共用) ───────────
    // メッシュ領域 [lo, hi]。1 軸も広がりが無ければ既定の箱を入れて false。
    // 水中音響ドメインでは代わりに海 (oceanBounds) を使う。
    bool sceneBounds(double lo[3], double hi[3]) const;
    // 海のシーン範囲 (x = 距離, z = 深度を下向き負, y は 0)。距離か水深が
    // 決まっていなければ false。
    bool oceanBounds(double lo[3], double hi[3]) const;
    // 海面・海底地形・音源位置を描く (水中音響ドメインのみ)
    void drawOcean(QPainter &p);
    // 深度方向の表示倍率を掛けた z (水中音響ドメイン以外は素通し)
    double zView(double z) const;
    // m_cx/m_cy/m_cz/m_scale を現在のメッシュ・ウィジェット寸法から更新する
    void updateSceneTransform() const;
    // 3 軸すべてに広がりがあるか (ドロップ配置の前提条件)
    bool meshDefined() const;
    // ドロップで作る要素の既定寸法 = メッシュ最小スパンの 1/10 [m]
    double defaultSize() const;
    // 画面座標 → シーン座標 (正射影 projectPoint の逆変換)。
    // 床面 (メッシュ領域の z 最小面) との交点を基本とし、交点が求まらない
    // /領域外になるときはシーン中心を通る視線垂直面へ落とす (onFloor=false)。
    bool unprojectToScene(const QPointF &screen, double out[3],
                          bool *onFloor) const;

    void drawWireBox(QPainter &p, const double a[3], const double b[3],
                     const QPen &pen) const;
    void drawDropPreview(QPainter &p);   // ドラッグ中の配置プレビュー
    void drawDropMessage(QPainter &p);   // ドロップ結果 / 拒否理由の一時表示
    // ドラッグ位置 → プレビュー状態を更新。配置可能なら true。
    bool updateDragTarget(const QPointF &pos);
    // ドロップされたコンポーネントをモデルへ反映する。
    // 追加できたら true (msg に追加内容)、できなければ false (msg に理由)。
    bool placeComponent(const QString &cat, const QString &name,
                        const double pos[3], bool onFloor, QString *msg);
    void showDropMessage(const QString &msg, bool ok);

    void drawResultSlice(QPainter &p);   // 実データ断面 (無ければ未読込の明示)
    void drawSliceLegend(QPainter &p, int decim);
    void drawRayOverlay(QPainter &p);

    // 3D シーンに載っている断面 1 枚。色画像は共通スケール m_sliceMax で作る
    struct Slice {
        QVector<double> cells;
        int    rows = 0, cols = 0;
        int    axis = 2;
        double pos = 0.0;
        double u0 = 0.0, u1 = 0.0, v0 = 0.0, v1 = 0.0;
        QString label;
        QImage img;
        int    decim = 1;
    };
    // 面内座標 (u, v) [m] → シーン座標 / 画面座標
    void    sliceScenePoint(const Slice &s, double u, double v,
                            double out[3]) const;
    QPointF projectSlicePoint(const Slice &s, double u, double v) const;
    void rebuildSliceImages();           // 断面データ → 色画像 (jet)

    Project *m_project;
    Domain   m_domain = Domain::EM;
    bool     m_solid = false;
    bool     m_showGrid = true;
    bool     m_showBoundary = false;
    bool     m_dark = false;
    ViewStyle m_viewStyle = ViewStyle::Solid;

    // 結果断面 (実データ)。空 = 未読込。複数あるときは直交する断面同士
    QVector<Slice> m_slices;
    double   m_sliceMax = 0.0;       // 全断面で共通の正規化最大値 (絶対値)
    QString  m_sliceLabel;           // 凡例の説明 (複数なら " / " で連結)
    int      m_sliceDecim = 1;       // 画像化で束ねたセル数 (1 = 等倍)
    // 再生コントロールの状況 (凡例のコマ表示。frameCount <= 0 で非表示)
    int      m_sliceFrame = 0;
    int      m_sliceFrameCount = 0;
    bool     m_slicePlaying = false;
    double   m_vScale = 1.0;         // 深度方向の表示倍率 (水中音響のみ)

    // ── ドラッグ&ドロップ配置の状態 ──
    bool     m_dragHover = false;      // コンポーネントをドラッグ中か
    QString  m_dragCat, m_dragName;    // ドラッグ中のコンポーネント
    QPointF  m_dragPos;                // ドラッグ位置 (ウィジェット座標)
    double   m_dragScene[3] = { 0, 0, 0 };  // 配置先 [m]
    bool     m_dragOnFloor = false;    // 床面との交点を使ったか
    bool     m_dragOk = false;         // 配置可能か
    QString  m_dragWhy;                // 配置できない理由
    QString  m_dropMsg;                // ドロップ結果の一時メッセージ
    bool     m_dropMsgOk = false;
    int      m_dropMsgSeq = 0;         // 古いタイマーが新しい表示を消さない用

    double   m_azimuthDeg = -60;
    double   m_elevationDeg = 25;
    double   m_zoom = 1.0;
    QPointF  m_panPx;
    QPointF  m_lastPos;
    Qt::MouseButton m_dragButton = Qt::NoButton;

    // cached scene transform (set in paintEvent)
    mutable double m_cx = 0, m_cy = 0, m_cz = 0, m_scale = 1.0;
};

} // namespace ofd
