// SliceLineIntegral.h — 断面上の直線に沿った線積分 ∫f dl (計算側)。
//
// 場マップの上に線を引いて「その線に沿った分布」と「積分値」を見るための計算。
// 描画側はこの結果を描くだけにする (io/SlicePieces・io/SliceOverlay と同じ
// 分け方 — 描画に数値計算を書かない)。
//
// ── 格子が一様とは限らない ────────────────────────────────────────────────
// ofd の格子は軸ごとに間隔が変わる (領域の中心を細かく、外側を粗く切る)。
// **等間隔だと思って添字を線形に計算すると、粗いところで位置がずれる**。
// ここでは節点座標の配列を二分探索して実位置から小数添字を出す。
//
// ── 行の向き ──────────────────────────────────────────────────────────────
// 場マップの行 0 は面内 第2軸の **+ 側** (io/H5Reader の規約。io/SliceOverlay
// が同じ向きで描いている)。つまり行 r は vCoord[rows-1-r] に対応する。
// ここを取り違えると上下が反転した線を積分することになる。
//
// ── 単位 ──────────────────────────────────────────────────────────────────
// 弧長は渡した座標の単位そのまま (節点座標を渡せば [m])。したがって
// 積分値の単位は「値 × 座標の単位」になる。呼び出し側が単位を明記すること。
#pragma once
#include <QVector>

namespace ofd {

struct LineSample {
    double s = 0.0;       // 始点からの弧長
    double value = 0.0;   // その位置の値 (双一次補間)
};

struct LineIntegralResult {
    bool   ok = false;
    double integral = 0.0;    // ∫ f dl (台形則)
    double length = 0.0;      // 線分の長さ
    double mean = 0.0;        // integral / length (length > 0 のとき)
    double maxAbs = 0.0;      // |f| の最大 (サンプル点上)
    QVector<LineSample> samples;
};

// 断面上の線分 (u0,v0)-(u1,v1) に沿って積分する。
//   cells  : rows*cols (row-major)。**行 0 = vCoord の + 側**
//   uCoord : 列に対応する座標 (昇順、cols 個)
//   vCoord : 行に対応する座標 (昇順、rows 個)
//   nSamples : サンプル点数 (2 以上)。台形則の分点になる
// 座標配列の個数が rows/cols と合わない、線分の長さが 0、線分が断面の
// 範囲からはみ出す場合は false (推測で外挿しない)。
bool sliceLineIntegral(const QVector<double> &cells, int rows, int cols,
                       const QVector<double> &uCoord,
                       const QVector<double> &vCoord,
                       double u0, double v0, double u1, double v1,
                       int nSamples, LineIntegralResult *out);

} // namespace ofd
