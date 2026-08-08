// MovieExport.h — 動画書き出しの純関数群 (H5ViewerTab の再生・書き出し用)。
//
// GUI から切り離してあるのは、ffmpeg を起動せずに引数の組み立てを検証
// できるようにするため (selftest から直接呼ぶ)。副作用は一切持たない。
#pragma once
#include <QString>
#include <QStringList>
#include <QVector>

namespace ofd {
namespace movie {

// ── 時間範囲 → フレーム番号 ────────────────────────────────────────────────
// times は各フレームの時刻 [s] (単調増加を想定)。[loSec, hiSec] に入る
// フレーム番号の範囲を [f0, f1] (両端含む) で返す。
//
// 範囲に 1 フレームも入らない場合は false を返し f0/f1 を変更しない
// (「何も再生しない」状態を黙って作らない — 呼び出し側が理由を出す)。
// lo > hi は入れ替えて扱う。times が空なら false。
//
// 端の比較は相対イプシロン 1e-9 を許す。利用者は「フレームの時刻ちょうど」を
// 境界に入れるのが自然だが、時刻列が t[i] = i·Δt のように積まれていると
// 丸めで境界の内外が入れ替わってしまうため (実測で 9·1e-9 > 9e-9)。
bool frameRangeForTimes(const QVector<double> &times, double loSec,
                        double hiSec, int &f0, int &f1);

// ── ffmpeg の引数組み立て ──────────────────────────────────────────────────
enum class Codec { H264, H265, VP9 };

struct MovieOptions {
    int    fps = 30;            // 1..240 へクランプ
    int    width = 0;           // 0 = 元の大きさのまま (スケールしない)
    int    height = 0;
    Codec  codec = Codec::H264;
    bool   gif = false;         // true なら GIF (コーデック指定は無視)

    MovieOptions() {}
};

// PNG 連番 (framePattern 例: "/tmp/x/frame%05d.png") から outPath を作る
// ffmpeg 引数列。実行ファイル名は含めない。
//
// 規約:
//   - -y (上書き) / -framerate <fps> / -i <pattern> の順で始める
//   - MP4 系は yuv420p + 偶数化 (crop) を必ず付ける。width/height 指定が
//     あるときは scale を先に掛けてから偶数化する
//   - GIF は palettegen/paletteuse を使わず fps フィルタのみ
//     (2 パス化は呼び出し側の判断。ここでは 1 パスで完結させる)
QStringList buildFfmpegArgs(const QString &framePattern,
                            const QString &outPath, const MovieOptions &opt);

} // namespace movie
} // namespace ofd
