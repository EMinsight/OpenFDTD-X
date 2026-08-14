// BandSpectrumCsv.h — 1/3 オクターブ帯域スペクトルの CSV 書出。
//
// 遮音タブの R(f) / 挿入損失 IL(f) のように「帯域中心周波数と 1 つの量」から
// なる結果を、外部ツール (表計算・実測との突き合わせ) へ渡すための形式。
//
// ── 書式 ──────────────────────────────────────────────────────────────────
//
//     # OpenFDTD-X band spectrum
//     # scenario: 界壁 (二重壁)
//     # quantity: R [dB]
//     # <注記があれば 1 行ずつ>
//     freq_Hz,R_dB
//     50,12.3
//     ...
//
// `#` 行と見出し行は `io/parseSeriesCsv` が読み飛ばす (数値に読めない行を
// 捨てる規則) ので、**書いたファイルをそのまま参照系列として読み戻せる**。
// 検証タブが実測値と比較する入口と同じ読み手を使う、ということでもある。
//
// 数値は `QString::number` (C ロケール) で書く。ロケールによって小数点が
// `,` になると列が壊れるため、ロケール依存の変換を使ってはならない。
#pragma once
#include <QString>
#include <QStringList>
#include <QVector>

namespace ofd {
namespace io {

struct BandSpectrum {
    QString scenario;              // どの画面の値か (人が読む用)
    QString quantity = QStringLiteral("R");   // 量の記号 (列名にも使う)
    QString unit = QStringLiteral("dB");
    QStringList notes;             // モデル名・前提など (`#` 行として出る)
    QVector<double> freqHz;        // 帯域中心周波数 [Hz]
    QVector<double> value;         // 同じ長さ

    bool isValid() const
    {
        return !freqHz.isEmpty() && freqHz.size() == value.size();
    }
};

// スペクトル → CSV 本文。`isValid()` でなければ空文字列を返す
// (空のファイルを「書き出せた」と言わないため、呼び出し側で弾くこと)。
QString buildBandSpectrumCsv(const BandSpectrum &s);

} // namespace io
} // namespace ofd
