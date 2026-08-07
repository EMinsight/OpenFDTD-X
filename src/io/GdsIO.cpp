// GdsIO.cpp
#include "GdsIO.h"

#include <QDataStream>
#include <QFile>

#include <cmath>

namespace ofd {
namespace GdsIO {

namespace {

// レコード型
enum Rec : quint8 {
    HEADER = 0x00, BGNLIB = 0x01, LIBNAME = 0x02, UNITS = 0x03, ENDLIB = 0x04,
    BGNSTR = 0x05, STRNAME = 0x06, ENDSTR = 0x07, BOUNDARY = 0x08,
    LAYER = 0x0D, DATATYPE = 0x0E, XY = 0x10, ENDEL = 0x11,
};
// データ型
enum Dt : quint8 {
    NODATA = 0x00, INT2 = 0x02, INT4 = 0x03, REAL8 = 0x05, ASCII = 0x06,
};

void putU16(QByteArray &b, quint16 v)
{
    b.append(char((v >> 8) & 0xFF));
    b.append(char(v & 0xFF));
}
void putI32(QByteArray &b, qint32 v)
{
    const quint32 u = quint32(v);
    b.append(char((u >> 24) & 0xFF));
    b.append(char((u >> 16) & 0xFF));
    b.append(char((u >> 8) & 0xFF));
    b.append(char(u & 0xFF));
}
void putU64(QByteArray &b, quint64 v)
{
    for (int i = 7; i >= 0; --i) b.append(char((v >> (i * 8)) & 0xFF));
}

// レコード 1 個を書く (長さはヘッダ 4B を含む)
void putRecord(QByteArray &out, quint8 rec, quint8 dt, const QByteArray &data)
{
    // GDSII のレコードは偶数長。奇数なら 0 で詰める (ASCII で起きる)
    QByteArray d = data;
    if (d.size() % 2) d.append('\0');
    putU16(out, quint16(d.size() + 4));
    out.append(char(rec));
    out.append(char(dt));
    out.append(d);
}

void putAscii(QByteArray &out, quint8 rec, const QString &s)
{
    putRecord(out, rec, ASCII, s.toLatin1());
}

quint16 getU16(const QByteArray &b, int off)
{
    return quint16((quint8(b[off]) << 8) | quint8(b[off + 1]));
}
qint32 getI32(const QByteArray &b, int off)
{
    const quint32 u = (quint32(quint8(b[off])) << 24)
                    | (quint32(quint8(b[off + 1])) << 16)
                    | (quint32(quint8(b[off + 2])) << 8)
                    |  quint32(quint8(b[off + 3]));
    return qint32(u);
}
quint64 getU64(const QByteArray &b, int off)
{
    quint64 v = 0;
    for (int i = 0; i < 8; ++i) v = (v << 8) | quint8(b[off + i]);
    return v;
}

} // namespace

// ── REAL8 (excess-64 / 基数 16) ─────────────────────────────────────────────
// 値 = (-1)^s · (仮数 / 2^56) · 16^(指数 − 64)
// 仮数は正規化されて 1/16 <= 仮数/2^56 < 1 になる。
quint64 toReal8(double v)
{
    if (v == 0.0) return 0;
    const bool neg = (v < 0.0);
    double a = std::fabs(v);

    // 16^(e-64) で割って仮数を [1/16, 1) に収める
    int e = 64;
    while (a >= 1.0)      { a /= 16.0; ++e; }
    while (a < 1.0 / 16.0) { a *= 16.0; --e; }
    if (e < 0) e = 0;
    if (e > 127) e = 127;

    // 仮数を 56 bit へ。丸めで 1.0 に達したら桁上げする
    quint64 mant = quint64(std::llround(a * 72057594037927936.0));   // 2^56
    if (mant >> 56) { mant >>= 4; ++e; }

    quint64 bits = quint64(neg ? 0x80 : 0x00) | quint64(e & 0x7F);
    bits <<= 56;
    bits |= (mant & 0x00FFFFFFFFFFFFFFULL);
    return bits;
}

double fromReal8(quint64 bits)
{
    const bool neg = (bits >> 63) & 1;
    const int  e   = int((bits >> 56) & 0x7F);
    const quint64 mant = bits & 0x00FFFFFFFFFFFFFFULL;
    if (mant == 0) return 0.0;
    const double m = double(mant) / 72057594037927936.0;   // / 2^56
    const double v = m * std::pow(16.0, double(e - 64));
    return neg ? -v : v;
}

// ── 書き出し ───────────────────────────────────────────────────────────────
QByteArray serialize(const GdsLibrary &lib)
{
    QByteArray out;
    // HEADER: バージョン 600 (= GDSII 6.0)。広く受け付けられる値。
    QByteArray ver;
    putU16(ver, 600);
    putRecord(out, HEADER, INT2, ver);

    // BGNLIB / BGNSTR は 12 個の INT2 (最終更新・最終アクセス日時)。
    // 再現性のため 0 で埋める (同じ入力 → 同じバイト列)。
    QByteArray zeros12(24, '\0');
    putRecord(out, BGNLIB, INT2, zeros12);
    putAscii(out, LIBNAME, lib.name);

    QByteArray units;
    putU64(units, toReal8(lib.userUnit));
    putU64(units, toReal8(lib.dbUnit_m));
    putRecord(out, UNITS, REAL8, units);

    const double db = (lib.dbUnit_m > 0.0) ? lib.dbUnit_m : 1e-9;
    for (const GdsStructure &st : lib.structures) {
        putRecord(out, BGNSTR, INT2, zeros12);
        putAscii(out, STRNAME, st.name);
        for (const GdsPolygon &p : st.polygons) {
            if (p.x_m.size() < 3 || p.x_m.size() != p.y_m.size()) continue;
            putRecord(out, BOUNDARY, NODATA, QByteArray());
            QByteArray lay;  putU16(lay, quint16(p.layer));
            putRecord(out, LAYER, INT2, lay);
            QByteArray dtp;  putU16(dtp, quint16(p.datatype));
            putRecord(out, DATATYPE, INT2, dtp);

            QByteArray xy;
            const int n = p.x_m.size();
            for (int i = 0; i < n; ++i) {
                putI32(xy, qint32(std::llround(p.x_m[i] / db)));
                putI32(xy, qint32(std::llround(p.y_m[i] / db)));
            }
            // BOUNDARY は閉じている必要がある — 末尾が始点でなければ足す
            if (p.x_m.first() != p.x_m.last()
                || p.y_m.first() != p.y_m.last()) {
                putI32(xy, qint32(std::llround(p.x_m.first() / db)));
                putI32(xy, qint32(std::llround(p.y_m.first() / db)));
            }
            putRecord(out, XY, INT4, xy);
            putRecord(out, ENDEL, NODATA, QByteArray());
        }
        putRecord(out, ENDSTR, NODATA, QByteArray());
    }
    putRecord(out, ENDLIB, NODATA, QByteArray());
    return out;
}

bool save(const QString &path, const GdsLibrary &lib, QString *err)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        if (err) *err = QStringLiteral("cannot write %1: %2")
                            .arg(path, f.errorString());
        return false;
    }
    const QByteArray b = serialize(lib);
    if (f.write(b) != b.size()) {
        if (err) *err = QStringLiteral("short write to %1").arg(path);
        return false;
    }
    return true;
}

// ── 読み込み ───────────────────────────────────────────────────────────────
bool parse(const QByteArray &bytes, GdsLibrary &lib, QString *err)
{
    lib = GdsLibrary();
    int off = 0;
    bool sawHeader = false, sawEndlib = false;
    GdsStructure cur;
    bool inStruct = false;
    GdsPolygon poly;
    bool inBoundary = false;

    while (off + 4 <= bytes.size()) {
        const int len = int(getU16(bytes, off));
        if (len < 4 || off + len > bytes.size()) {
            if (err) *err = QStringLiteral("truncated record at byte %1")
                                .arg(off);
            return false;
        }
        const quint8 rec = quint8(bytes[off + 2]);
        const int dataOff = off + 4, dataLen = len - 4;

        switch (rec) {
        case HEADER: sawHeader = true; break;
        case LIBNAME:
            lib.name = QString::fromLatin1(bytes.mid(dataOff, dataLen))
                           .remove(QLatin1Char('\0'));
            break;
        case UNITS:
            if (dataLen >= 16) {
                lib.userUnit = fromReal8(getU64(bytes, dataOff));
                lib.dbUnit_m = fromReal8(getU64(bytes, dataOff + 8));
            }
            break;
        case BGNSTR:
            cur = GdsStructure();
            inStruct = true;
            break;
        case STRNAME:
            cur.name = QString::fromLatin1(bytes.mid(dataOff, dataLen))
                           .remove(QLatin1Char('\0'));
            break;
        case ENDSTR:
            if (inStruct) lib.structures.push_back(cur);
            inStruct = false;
            break;
        case BOUNDARY:
            poly = GdsPolygon();
            inBoundary = true;
            break;
        case LAYER:
            if (inBoundary && dataLen >= 2)
                poly.layer = int(qint16(getU16(bytes, dataOff)));
            break;
        case DATATYPE:
            if (inBoundary && dataLen >= 2)
                poly.datatype = int(qint16(getU16(bytes, dataOff)));
            break;
        case XY:
            if (inBoundary) {
                const double db = (lib.dbUnit_m > 0.0) ? lib.dbUnit_m : 1e-9;
                for (int i = 0; i + 8 <= dataLen; i += 8) {
                    poly.x_m.push_back(getI32(bytes, dataOff + i) * db);
                    poly.y_m.push_back(getI32(bytes, dataOff + i + 4) * db);
                }
            }
            break;
        case ENDEL:
            if (inBoundary && poly.x_m.size() >= 3) cur.polygons.push_back(poly);
            inBoundary = false;
            break;
        case ENDLIB:
            sawEndlib = true;
            break;
        default:
            break;   // 未対応レコード (SREF/PATH/TEXT 等) は読み飛ばす
        }
        off += len;
        if (sawEndlib) break;
    }

    if (!sawHeader) {
        if (err) *err = QStringLiteral("not a GDSII stream (no HEADER record)");
        lib = GdsLibrary();
        return false;
    }
    return true;
}

bool load(const QString &path, GdsLibrary &lib, QString *err)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (err) *err = QStringLiteral("cannot open %1: %2")
                            .arg(path, f.errorString());
        return false;
    }
    return parse(f.readAll(), lib, err);
}

} // namespace GdsIO
} // namespace ofd
