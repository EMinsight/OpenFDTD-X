// GdsIO.h — GDSII (Calma Stream Format) の最小限の読み書き。
//
// 外部ライブラリを足さない方針 (CLAUDE.md 規則 5) のため自前で実装する。
// GDSII は公開されたレコード形式なので、必要な部分集合だけなら小さい。
//
// レコード構造:
//   [2B 長さ (ビッグエンディアン、ヘッダ 4B を含む)]
//   [1B レコード型] [1B データ型] [データ …]
//
// 扱うレコード (これで多角形レイアウトは表現できる):
//   HEADER 0x00 / BGNLIB 0x01 / LIBNAME 0x02 / UNITS 0x03 / ENDLIB 0x04
//   BGNSTR 0x05 / STRNAME 0x06 / ENDSTR 0x07
//   BOUNDARY 0x08 / LAYER 0x0D / DATATYPE 0x0E / XY 0x10 / ENDEL 0x11
//
// 座標は **データベース単位の整数**。UNITS レコードが
//   [ユーザー単位/データベース単位, データベース単位の meter 値]
// を REAL8 で持つ。既定はレイアウト業界の慣用値 (1 µm ユーザー単位 /
// 1 nm データベース単位 → 1e-3, 1e-9)。
//
// REAL8 は IEEE754 ではなく **excess-64 の 16 進指数形式**:
//   値 = (-1)^s · (仮数 / 2^56) · 16^(指数 − 64)
// ここが GDSII 実装で最も間違えやすいので、selftest で既知のビット列と
// 突き合わせている。
//
// 未対応: SREF/AREF (セル参照)、PATH、TEXT、回転・拡大。読み飛ばす。
#pragma once
#include <QString>
#include <QVector>

namespace ofd {

// 多角形 1 個 (閉じた輪郭)。座標は meter。
struct GdsPolygon {
    int             layer = 0;
    int             datatype = 0;
    QVector<double> x_m, y_m;      // 同じ長さ。最終点 = 始点 (閉じている)
};

// 構造 (セル) 1 個
struct GdsStructure {
    QString             name;
    QVector<GdsPolygon> polygons;
};

struct GdsLibrary {
    QString                 name = QStringLiteral("LIB");
    double                  userUnit = 1e-3;   // ユーザー単位 / データベース単位
    double                  dbUnit_m = 1e-9;   // データベース単位の meter 値
    QVector<GdsStructure>   structures;

    int polygonCount() const
    {
        int n = 0;
        for (const GdsStructure &s : structures) n += s.polygons.size();
        return n;
    }
};

namespace GdsIO {

// 読み書き。失敗理由は err へ。
bool load(const QString &path, GdsLibrary &lib, QString *err = nullptr);
bool save(const QString &path, const GdsLibrary &lib, QString *err = nullptr);

// バイト列で直接扱う版 (selftest がファイル I/O を挟まずに往復させる)
bool parse(const QByteArray &bytes, GdsLibrary &lib, QString *err = nullptr);
QByteArray serialize(const GdsLibrary &lib);

// GDSII の REAL8 (excess-64 / 基数 16) と double の相互変換。
// 実装で最も間違えやすい箇所なので公開して直接検証する。
quint64 toReal8(double v);
double  fromReal8(quint64 bits);

} // namespace GdsIO
} // namespace ofd
