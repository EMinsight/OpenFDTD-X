// DxfOutline.h — ASCII DXF から 2D の輪郭 (閉ポリライン) と線分を読む。
//
// 遮音タブの「仕切壁面積 S」を図面から入れるための読み取り。**図面の全体を
// 理解するものではない** — ENTITIES セクションの LINE と LWPOLYLINE だけを見る。
//
// ── なぜ LINE と LWPOLYLINE だけなのか ────────────────────────────────────
// DXF は Autodesk が仕様を公開しているが全体は非常に大きく、ブロック参照
// (INSERT) の入れ子・スプライン・楕円まで扱い出すと際限が無い。面積を読む
// という目的に対して、**閉じた LWPOLYLINE の囲む面積**だけが必要十分。
// 読めなかった実体は種類ごとに数えて呼び出し側へ返し、画面に出す
// (黙って無視すると「図面全部を読んだ上での面積」に見えてしまう)。
//
// ── 面積の求め方と、その限界 ──────────────────────────────────────────────
// 閉ポリラインの囲む面積は靴紐公式 Σ(x_i·y_{i+1} − x_{i+1}·y_i)/2 の絶対値。
// **自己交差する多角形では意味のある面積にならない**ので、そのときは
// 面積を返さない (符号付き面積が打ち消し合った値を面積として出さない)。
//
// LWPOLYLINE は頂点にふくらみ (bulge, グループコード 42) を持てて、
// これは円弧を表す。**円弧を弦で近似すると面積が小さく出る**ので、bulge が
// 0 でない頂点の数を数えて返す。呼び出し側は「円弧を直線で近似した面積」
// である旨を必ず表示すること。
//
// ── 単位 ──────────────────────────────────────────────────────────────────
// DXF の座標に単位は無い。HEADER の $INSUNITS があればそれを採る
// (4=mm / 5=cm / 6=m / 1=inch / 2=feet)。無い・0 (unitless)・未対応の値なら
// Unknown を返し、**呼び出し側が利用者に選ばせる**。推測で m とみなさない。
#pragma once
#include <QString>
#include <QVector>

namespace ofd {

// $INSUNITS の解釈結果
enum class DxfUnit { Unknown, Millimeter, Centimeter, Meter, Inch, Foot };

// 単位 → メートル換算係数。Unknown は 0 (使ってはいけない)
double dxfUnitToMeter(DxfUnit u);
// 表示用の短い名前 ("mm" 等)。Unknown は空
const char *dxfUnitName(DxfUnit u);

// 閉じた輪郭 1 つ
struct DxfLoop {
    QVector<double> x, y;      // 頂点 (図面の単位のまま)
    double area = 0.0;         // 靴紐公式の絶対値 (図面の単位の 2 乗)
    double perimeter = 0.0;
    int    arcVertices = 0;    // bulge != 0 の頂点数 (0 なら直線だけ)
    bool   selfIntersecting = false;   // true なら area は信用できない
};

struct DxfOutline {
    bool ok = false;
    DxfUnit unit = DxfUnit::Unknown;
    QVector<DxfLoop> loops;    // 閉じた LWPOLYLINE
    int openPolylines = 0;     // 閉じていない LWPOLYLINE の本数
    int lineSegments = 0;      // LINE の本数 (輪郭には組み立てない)
    int skippedEntities = 0;   // 読まなかった実体 (CIRCLE / SPLINE など)
    double bbox[4] = { 0, 0, 0, 0 };   // xmin, ymin, xmax, ymax
    bool hasBBox = false;
};

// ASCII DXF のテキストを読む。バイナリ DXF は扱わない (先頭が
// "AutoCAD Binary DXF" なら false)。
bool parseDxfOutline(const QString &text, DxfOutline *out, QString *err);

// ファイルから読む (UTF-8 として解釈。失敗したら latin1 として読み直す)
bool loadDxfOutline(const QString &path, DxfOutline *out, QString *err);

} // namespace ofd
