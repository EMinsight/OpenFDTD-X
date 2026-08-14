// DxfOutline.cpp
#include "DxfOutline.h"

#include <QFile>
#include <QStringConverter>

#include <cmath>

namespace ofd {

namespace {

// 線分 (p1,p2) と (p3,p4) が端点を共有せずに交わるか。多角形の自己交差判定用
double cross2(double ax, double ay, double bx, double by)
{
    return ax * by - ay * bx;
}

bool segmentsCross(double x1, double y1, double x2, double y2,
                   double x3, double y3, double x4, double y4)
{
    const double d1 = cross2(x4 - x3, y4 - y3, x1 - x3, y1 - y3);
    const double d2 = cross2(x4 - x3, y4 - y3, x2 - x3, y2 - y3);
    const double d3 = cross2(x2 - x1, y2 - y1, x3 - x1, y3 - y1);
    const double d4 = cross2(x2 - x1, y2 - y1, x4 - x1, y4 - y1);
    // 真に跨いでいる場合だけ true (接触・共線は数えない — 隣り合う辺は
    // 必ず端点を共有するため、そこを交差と数えると全部が自己交差になる)
    return ((d1 > 0 && d2 < 0) || (d1 < 0 && d2 > 0))
        && ((d3 > 0 && d4 < 0) || (d3 < 0 && d4 > 0));
}

// 多角形が自己交差しているか (隣接しない辺同士の交差を総当たりで見る)。
// 頂点数が多いときは O(n²) を避けて判定しない (false = 交差なしとみなす)
bool polygonSelfIntersects(const QVector<double> &x, const QVector<double> &y)
{
    const int n = x.size();
    if (n < 4 || n > 512) return false;
    for (int i = 0; i < n; ++i) {
        const int i2 = (i + 1) % n;
        for (int j = i + 1; j < n; ++j) {
            const int j2 = (j + 1) % n;
            if (i == j || i2 == j || i == j2) continue;   // 端点を共有する辺
            if (segmentsCross(x[i], y[i], x[i2], y[i2],
                              x[j], y[j], x[j2], y[j2]))
                return true;
        }
    }
    return false;
}

void bump(DxfOutline *o, double x, double y)
{
    if (!o->hasBBox) {
        o->bbox[0] = o->bbox[2] = x;
        o->bbox[1] = o->bbox[3] = y;
        o->hasBBox = true;
        return;
    }
    o->bbox[0] = std::min(o->bbox[0], x);
    o->bbox[1] = std::min(o->bbox[1], y);
    o->bbox[2] = std::max(o->bbox[2], x);
    o->bbox[3] = std::max(o->bbox[3], y);
}

DxfUnit unitFromInsunits(int v)
{
    switch (v) {
    case 1:  return DxfUnit::Inch;
    case 2:  return DxfUnit::Foot;
    case 4:  return DxfUnit::Millimeter;
    case 5:  return DxfUnit::Centimeter;
    case 6:  return DxfUnit::Meter;
    default: return DxfUnit::Unknown;    // 0 = unitless もここ
    }
}

} // namespace

double dxfUnitToMeter(DxfUnit u)
{
    switch (u) {
    case DxfUnit::Millimeter: return 1e-3;
    case DxfUnit::Centimeter: return 1e-2;
    case DxfUnit::Meter:      return 1.0;
    case DxfUnit::Inch:       return 0.0254;
    case DxfUnit::Foot:       return 0.3048;
    default:                  return 0.0;
    }
}

const char *dxfUnitName(DxfUnit u)
{
    switch (u) {
    case DxfUnit::Millimeter: return "mm";
    case DxfUnit::Centimeter: return "cm";
    case DxfUnit::Meter:      return "m";
    case DxfUnit::Inch:       return "inch";
    case DxfUnit::Foot:       return "ft";
    default:                  return "";
    }
}

bool parseDxfOutline(const QString &text, DxfOutline *out, QString *err)
{
    if (!out) return false;
    *out = DxfOutline();
    if (text.startsWith(QLatin1String("AutoCAD Binary DXF"))) {
        if (err) *err = QStringLiteral("binary DXF");
        return false;
    }

    // ASCII DXF は「グループコードの行」と「値の行」の繰り返し
    const QStringList raw = text.split(QLatin1Char('\n'));
    QVector<int> codes;
    QStringList values;
    codes.reserve(raw.size() / 2 + 1);
    for (int i = 0; i + 1 < raw.size(); i += 2) {
        bool okc = false;
        const int c = raw[i].trimmed().toInt(&okc);
        if (!okc) {
            // 途中で対がずれたら読めない (グループコードは必ず整数)
            if (err) *err = QStringLiteral("group code at line %1").arg(i + 1);
            return false;
        }
        codes.push_back(c);
        values.push_back(raw[i + 1].trimmed());
    }
    if (codes.isEmpty()) {
        if (err) *err = QStringLiteral("empty");
        return false;
    }

    QString section;
    bool inEntities = false;

    // LWPOLYLINE 組み立て中の状態
    bool inLw = false;
    QVector<double> px, py;
    int lwFlags = 0, arcVerts = 0;
    bool pendingX = false;
    double curX = 0.0;

    auto finishLw = [&]() {
        if (!inLw) return;
        // 閉じているか (グループコード 70 のビット 1)
        if ((lwFlags & 1) && px.size() >= 3) {
            DxfLoop lp;
            lp.x = px; lp.y = py;
            lp.arcVertices = arcVerts;
            double a = 0.0, per = 0.0;
            const int n = px.size();
            for (int i = 0; i < n; ++i) {
                const int j = (i + 1) % n;
                a += px[i] * py[j] - px[j] * py[i];
                per += std::hypot(px[j] - px[i], py[j] - py[i]);
            }
            lp.area = std::fabs(a) * 0.5;
            lp.perimeter = per;
            lp.selfIntersecting = polygonSelfIntersects(px, py);
            if (lp.selfIntersecting) lp.area = 0.0;   // 意味のある面積が無い
            out->loops.push_back(lp);
        } else if (px.size() >= 2) {
            out->openPolylines++;
        }
        inLw = false;
        px.clear(); py.clear();
        lwFlags = arcVerts = 0;
        pendingX = false;
    };

    for (int i = 0; i < codes.size(); ++i) {
        const int c = codes[i];
        const QString &v = values[i];

        if (c == 0) {
            finishLw();                    // 実体の切れ目で確定させる
            if (v == QLatin1String("SECTION")) { section.clear(); continue; }
            if (v == QLatin1String("ENDSEC")) { inEntities = false; continue; }
            if (v == QLatin1String("EOF")) break;
            if (!inEntities) continue;
            if (v == QLatin1String("LWPOLYLINE")) {
                inLw = true;
            } else if (v == QLatin1String("LINE")) {
                out->lineSegments++;
            } else if (v != QLatin1String("SEQEND")
                       && v != QLatin1String("VERTEX")) {
                out->skippedEntities++;    // CIRCLE / SPLINE / INSERT など
            }
            continue;
        }

        // セクション名 (SECTION の直後の 2)
        if (c == 2 && section.isEmpty()) {
            section = v;
            inEntities = (v == QLatin1String("ENTITIES"));
            continue;
        }

        // HEADER の $INSUNITS
        if (c == 9 && v == QLatin1String("$INSUNITS")) {
            // 次に来る 70 が値
            for (int k = i + 1; k < codes.size() && k < i + 4; ++k) {
                if (codes[k] == 70) {
                    out->unit = unitFromInsunits(values[k].toInt());
                    break;
                }
            }
            continue;
        }

        if (!inEntities) continue;

        if (inLw) {
            if (c == 70) { lwFlags = v.toInt(); continue; }
            if (c == 42) { if (v.toDouble() != 0.0) arcVerts++; continue; }
            if (c == 10) { curX = v.toDouble(); pendingX = true; continue; }
            if (c == 20 && pendingX) {
                const double yy = v.toDouble();
                px.push_back(curX);
                py.push_back(yy);
                bump(out, curX, yy);
                pendingX = false;
                continue;
            }
        } else {
            // LINE の端点も外形の範囲には効かせる
            if (c == 10 || c == 11) { curX = v.toDouble(); pendingX = true; continue; }
            if ((c == 20 || c == 21) && pendingX) {
                bump(out, curX, v.toDouble());
                pendingX = false;
                continue;
            }
        }
    }
    finishLw();

    out->ok = true;
    return true;
}

bool loadDxfOutline(const QString &path, DxfOutline *out, QString *err)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (err) *err = f.errorString();
        return false;
    }
    const QByteArray bytes = f.readAll();
    // DXF は既定が ANSI のことが多い。UTF-8 として妥当なら UTF-8、
    // そうでなければ latin1 として読む (座標は ASCII なのでどちらでも読める)
    QStringDecoder dec(QStringConverter::Utf8,
                       QStringConverter::Flag::ConvertInvalidToNull);
    QString text = dec(bytes);
    if (dec.hasError()) text = QString::fromLatin1(bytes);
    return parseDxfOutline(text, out, err);
}

} // namespace ofd
