// Touchstone.h — read/write S-parameters in Touchstone format (.s1p / .s2p / .sNp).
//
// 周波数特性プロット (plotspara) の共通出力形式。光・電磁ドメイン共通で
// 他ツール (ADS / scikit-rf / Lumerical INTERCONNECT) に持ち込める。
//
// 読み側はカーネル (OpenFDTD sol/outputSpara.c) が書く "# Hz S MA R 50" を
// はじめ、Touchstone 1.1 の周波数単位 (HZ/KHZ/MHZ/GHZ) と数値形式
// (MA/DB/RI) を受ける。2 ポートだけ列順が S11 S21 S12 S22 と転置になる
// 仕様なので、読み書きの両方でここを吸収し、`TouchstoneData::s` は常に
// 行優先の行列として持つ。
//
// 注意 — カーネルの test.snp は仕様準拠ではない: ofd/orcwa は port1 だけを
// 励振して S_n1 (行列の **第 1 列**) しか計算しないので、1 行あたりの数値は
// 1 + 2·N 個 (準拠なら 1 + 2·N²)。読み側はこれを検出して `column1Only` を
// 立て、**第 1 列以外を未知として扱う** (0 で埋めるが使ってはならない)。
// 欠けた要素を勝手に補完しない (相反性を仮定して S12 = S21 と置くこともしない
// — S22 は依然として不明で、埋めた瞬間に「計算していない値」を出力してしまう)。
#pragma once
#include <QString>
#include <QVector>
#include <complex>

namespace ofd {

// N ポート S パラメータの周波数掃引 (読み書き共通の器)。
struct TouchstoneData {
    int    ports = 0;              // ポート数 n
    double z0 = 50.0;              // 基準インピーダンス [Ω]
    QVector<double> freqHz;        // 周波数 [Hz] — 読み込み時に Hz へ正規化
    // s[i] = i 番目の周波数の n×n 行列 (行優先。s[i][r*n + c] = S(r+1, c+1))
    QVector<QVector<std::complex<double>>> s;
    // 第 1 列 (S_n1) しか入っていない = カーネル出力。true のとき第 2 列以降は
    // **未知** (0 が入っているだけ)。書き出しや表示に使ってはならない。
    bool column1Only = false;

    bool isEmpty() const { return ports <= 0 || freqHz.isEmpty(); }
    // S(row, col) — 1 始まりのポート番号で引く。範囲外は 0。
    std::complex<double> at(int freqIndex, int row, int col) const;
    // その要素が実際に計算された値かどうか (column1Only のとき col==1 のみ)
    bool isKnown(int row, int col) const;
    // 掃引全体にわたる S(row,col) の並び (無効な指定では空)
    QVector<std::complex<double>> series(int row, int col) const;
};

class Touchstone {
public:
    // 1-port: S11 only.
    static bool writeS1p(const QString &path,
                         const QVector<double> &freqHz,
                         const QVector<std::complex<double>> &s11,
                         QString *err = nullptr);

    // 2-port: S11 S21 S12 S22.
    static bool writeS2p(const QString &path,
                         const QVector<double> &freqHz,
                         const QVector<std::complex<double>> &s11,
                         const QVector<std::complex<double>> &s21,
                         const QVector<std::complex<double>> &s12,
                         const QVector<std::complex<double>> &s22,
                         QString *err = nullptr);

    // N ポート出力 (Hz / RI / R z0)。ports==2 は仕様どおり列を転置する。
    static bool writeSnp(const QString &path, const TouchstoneData &d,
                         QString *err = nullptr);

    // Touchstone 1.x の読み込み。失敗時は false + err (Touchstone 2.0 の
    // [Version] 記法は未対応 — 黙って誤読せずエラーにする)。
    // portsHint > 0 を渡すと、1 行の数値の個数が「N ポート全行列」とも
    // 「M ポートの第 1 列だけ」とも読める場合 (例 9 個 = 2 ポート全行列 /
    //  4 ポート第 1 列) の曖昧さをそのポート数で解く。
    static bool read(const QString &path, TouchstoneData *out,
                     QString *err = nullptr, int portsHint = 0);

    // 位相の連続化 [rad] — 隣接点の差が ±π を超えたら 2π 単位で戻す。
    static QVector<double> unwrapPhaseRad(
        const QVector<std::complex<double>> &s);

    // 群遅延 τ_g = −dφ/dω [s]。位相を連続化してから中心差分 (端は片側差分)。
    // 点数が 2 未満、または周波数が重複する点では 0 を返す。
    static QVector<double> groupDelaySec(
        const QVector<double> &freqHz,
        const QVector<std::complex<double>> &s);

    // 部分行列の取り出し — ports 個のポート番号 (1 始まり) を選び、その順に
    // 並べ替えた小行列を返す。番号が範囲外のとき、および未知の要素
    // (column1Only の第 2 列以降) を含むときは空を返す。
    static TouchstoneData subset(const TouchstoneData &d,
                                 const QVector<int> &ports1based);

    // Convert input impedance to S11 against reference Z0.
    static std::complex<double> zToS(std::complex<double> z, double z0 = 50.0);

    // S パラメータ → 参照系列 CSV (`io/parseSeriesCsv` が読む 2 列以上の形)。
    // 実測の .sNp を検証タブで比較するための入口で、列は
    //     freq_Hz, S11_dB, S11_deg, S21_dB, S21_deg, …
    // となる (既定の 1・2 列目 = 周波数と S11 の dB)。
    //
    // **計算されていない要素は列にしない** — `column1Only` のファイルは
    // 第 1 列 (S_n1) だけを書く。0 が入っているだけの要素を列にすると
    // 「測ったが 0 dB」と読めてしまう。
    //
    // 厳密に 0 の要素があるときは **変換せず空文字列を返す** (err に理由)。
    // 20·log10(0) = −∞ で、床値を置けば嘘になり、`-inf` と書けば読み手が
    // その行ごと落とす (どちらも黙って値を変える)。
    static QString toCsv(const TouchstoneData &d,
                         const QString &sourceName = QString(),
                         QString *err = nullptr);
};

} // namespace ofd
