// PhotometricIO.h — 配光ファイル (IES LM-63) の読み書き。
//
// 照明設計ソフト (DIALux / AGi32 / Relux) が読む標準形式。OpenFDTD-X 側では
// `optics/IlluminationTrace` の追跡結果 (θ ビンごとの光度) をこの形式で書き出す。
// 読み側は自分が書いたものの検算 (ラウンドトリップ) と、外部の配光ファイルを
// 取り込むために持っている。
//
// ── 対応形式 ───────────────────────────────────────────────────────────────
//
// **IESNA LM-63-2002 (Type C 配光, TILT=NONE) のみ**。ファイルは
//
//     IESNA:LM-63-2002
//     [キーワード] 値                       ← 任意個
//     TILT=NONE
//     <ランプ数> <ランプ光束> <倍率> <鉛直角数> <水平角数> <測光型> <単位> <幅> <長さ> <高さ>
//     <バラスト係数> <将来用> <入力電力>
//     <鉛直角 …>
//     <水平角 …>
//     <光度 (最初の水平角の全鉛直角) …>
//     <光度 (次の水平角) …>
//
// の並び。測光型 1 = Type C、単位 2 = メートル。光度は「倍率を掛ける前」の値
// なので、読み側は倍率を掛けて `candela` に入れる (書き側は常に倍率 1)。
//
// **EULUMDAT (.ldt) は未対応**。呼び出し側はボタンを無効化して理由を出すこと。
//
// ── 角度の取り方 (追跡結果を書き出すとき) ──────────────────────────────────
//
// 追跡は θ を等幅のビンに集めるので、各値は**そのビンの立体角平均**である。
// `fromTrace()` は鉛直角に**ビン中心** (0.5°, 1.5°, … 179.5°) を並べ、
// その旨を [MORE] 行としてファイルに書く。こうすると隣り合う角度の中点が
// ちょうどビンの境界に一致するので、`integratedFlux()` は追跡が出した
// 系の全光束を**厳密に**復元する (selftest で検証)。
//
// 端の半ビン (0〜0.5° と 179.5〜180°) は外部ツールの台形積分から落ちるが、
// その立体角は 2π(1−cos0.5°) ≈ 2.4×10⁻⁵ sr で、光束にすると無視できる。
#pragma once
#include <QString>
#include <QVector>

namespace ofd {

namespace illum { struct Result; }

// 配光データ (IES Type C)。軸対称なら horizAngles_deg は 1 要素。
struct PhotometricData {
    // ヘッダのキーワード (空なら書き出さない)
    QString test, testLab, issueDate, manufacturer, lumCat, luminaire, lamp;
    QVector<QString> more;        // [MORE] 行 (計算条件の注記)

    int    lamps = 1;
    double lumensPerLamp = 0.0;   // ランプ光束 [lm] (負値 = 絶対測光)
    double inputWatts = 0.0;
    double ballastFactor = 1.0;
    // 器具寸法 [m] (点光源は 0)
    double width = 0.0, length = 0.0, height = 0.0;

    QVector<double> vertAngles_deg;   // 昇順 (Type C の γ)
    QVector<double> horizAngles_deg;  // 昇順 (C 平面)
    // candela[h][v] — 倍率を適用済みの実光度 [cd]
    QVector<QVector<double>> candela;

    bool isEmpty() const
    {
        return vertAngles_deg.isEmpty() || horizAngles_deg.isEmpty()
            || candela.isEmpty();
    }
};

class PhotometricIO {
public:
    // IES LM-63-2002 を書く / 読む。失敗時は false で err に理由。
    static bool writeIes(const QString &path, const PhotometricData &d,
                         QString *err = nullptr);
    static bool readIes(const QString &path, PhotometricData *d,
                        QString *err = nullptr);

    // 配光を立体角で積分した光束 [lm]。鉛直方向の境界は隣り合う角度の中点
    // (端は 0° / 180°)、水平方向は C 平面が 1 枚なら全周とする。
    static double integratedFlux(const PhotometricData &d);

    // 追跡結果 → 配光データ (鉛直角はビン中心。lampLumens は光源光束)
    static PhotometricData fromTrace(const illum::Result &r, double lampLumens);
};

} // namespace ofd
