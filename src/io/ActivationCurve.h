// ActivationCurve.h — OpenBPM の ONN パワースイープ出力
// (activation_curve.csv, ヘッダ "P_in_W,P_out_W,transmission") のパーサ。
//
// obpm は powersweep キー指定時に作業ディレクトリへこの CSV を書き、
// ログに "ONN: A_eff = ... [m^2]" を出す。GUI (OpticalTab) は実行完了後に
// この CSV を読み込み、活性化カーブ P_out(P_in) と透過率 T(P_in) を表示する。
// 出典: Honda, Shoji, Amemiya, Opt. Lett. 49, 5811 (2024).
#pragma once
#include <QString>
#include <QVector>

namespace ofd {

struct ActivationPoint {
    double pin  = 0;   // P_in  [W]
    double pout = 0;   // P_out [W]
    double T    = 0;   // 透過率 P_out / P_in
};

class ActivationCurve {
public:
    // CSV テキストをパースする。ヘッダ行・空行・コメント (#) は読み飛ばし、
    // 数値3列 (P_in,P_out,T) の行だけを採用する。1点も無ければ失敗。
    static bool parse(const QString &text, QVector<ActivationPoint> &pts,
                      QString *err = nullptr);

    // activation_curve.csv ファイルを読む (存在しなければ失敗)。
    static bool load(const QString &path, QVector<ActivationPoint> &pts,
                     QString *err = nullptr);

    // カーネルログ行から実効断面積 A_eff [m^2] を抽出する。
    // 形式: "ONN: A_eff = <値> [m^2]"。該当しなければ 0 を返す。
    static double aeffFromLogLine(const QString &line);
};

} // namespace ofd
