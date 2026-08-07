// EvReader.h — カーネルの作図出力 .ev2 を読み、QPainter で描き直す。
//
// 形式は OpenFDTD 同梱の "ev" 作図ライブラリ (post/ev2d.c) が書くもので、
// **プレーンテキストの表示リスト**。ev2d_end_data() の fprintf がそのまま仕様:
//
//   -1 <width> <height>            新規ページ (キャンバス寸法)
//   -2 <r> <g> <b>                 以降の色 (0..255)
//    2 x1 y1 x2 y2                 線
//    3 x1 y1 x2 y2 x3 y3           三角形 (塗り)
//    4 x1 y1 x2 y2 x3 y3 x4 y4     四角形 (塗り)
//   21 x1 y1 x2 y2                 楕円 (外形)   ※2 点は外接矩形の対角
//   22 x1 y1 x2 y2                 楕円 (塗り)
//   -3 x y h                       文字列 — 次の 1 行が本文
//      <text>
//
// 座標系の原点は **左下** (HTML 出力側が y を Height - y と反転して描く)。
// drawTriangle / drawQuadrangle / drawRectangle / drawPolyline は書き出し側で
// 線 (idx=2) に分解されるので、読む側が知るべき図形はこれで全部。
//
// 3D (.ev3) は別形式で未対応。
#pragma once
#include <QColor>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector>

class QPainter;

namespace ofd {

// 描画コマンド 1 個
struct EvCommand {
    enum Kind { Line, FillTriangle, FillQuad, Ellipse, FillEllipse, Text };
    Kind             kind = Line;
    QColor           color = Qt::black;
    QVector<QPointF> pts;     // ev 座標 (原点は左下)
    QString          text;    // Text のみ
    double           height = 0.0;   // Text の文字高 (ev 単位)
};

// 1 ページ = 1 枚の図
struct EvPage {
    double             width = 0.0, height = 0.0;   // キャンバス寸法 (ev 単位)
    QVector<EvCommand> commands;
};

struct EvDocument {
    QVector<EvPage> pages;
    bool isEmpty() const { return pages.isEmpty(); }
};

namespace EvReader {

// .ev2 を読む。読めた図形が 1 つも無ければ false + err。
bool load(const QString &path, EvDocument &doc, QString *err = nullptr);
// テキストから直接 (selftest 用 — ファイル I/O を挟まない)
bool parse(const QString &text, EvDocument &doc, QString *err = nullptr);

// ページを rect へ収めて描く。ev の左下原点 → 画面の左上原点へ反転し、
// 縦横比を保ったまま最大化する (図が歪まない)。
void render(QPainter &p, const QRectF &rect, const EvPage &page);

} // namespace EvReader
} // namespace ofd
