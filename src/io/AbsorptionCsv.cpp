// AbsorptionCsv.cpp
#include "AbsorptionCsv.h"

#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

#include <cmath>

namespace ofd {

double nrcFromAlpha(const double alpha[6])
{
    // ASTM C423: 250 / 500 / 1k / 2k の 4 バンド (= 添字 1..4)。
    // 125 Hz と 4 kHz は入らない。
    const double avg = (alpha[1] + alpha[2] + alpha[3] + alpha[4]) / 4.0;
    // 0.05 刻みへ丸める。**割らずに 20 を掛ける**こと — 0.05 は 2 進で
    // 表せないので `avg / 0.05` は 0.075 で 1.4999999999999998 になり、
    // ちょうど半分の値が下側へ落ちる。`avg * 20` なら 1.5 のままなので、
    // std::round が 0 から遠い側へ送って ASTM の「nearest」と一致する。
    return std::round(avg * 20.0) / 20.0;
}

AbsorptionTable parseAbsorptionCsv(const QString &text)
{
    AbsorptionTable t;

    // 名称に空白が入りうるので空白は区切りにしない
    static const QRegularExpression sep(QStringLiteral("[,;\\t]"));
    bool sawHeader = false;

    const QStringList lines = text.split(QRegularExpression("\r\n|\n|\r"));
    for (const QString &raw : lines) {
        const QString line = raw.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) continue;

        const QStringList f = line.split(sep);
        if (f.size() < 7) { ++t.skipped; continue; }

        AbsorptionMaterial m;
        m.name = f[0].trimmed();
        bool good = true;
        bool over = false;
        for (int b = 0; b < 6; ++b) {
            bool okv = false;
            const double v = f[b + 1].trimmed().toDouble(&okv);
            if (!okv || !std::isfinite(v) || v < 0.0) { good = false; break; }
            if (v > 1.0) over = true;      // 残響室法では正常 (捨てない)
            m.alpha[b] = v;
        }
        if (!good) {
            // 数値でない行は、最初の 1 本だけヘッダとして扱う
            if (!sawHeader && t.materials.empty()) sawHeader = true;
            else ++t.skipped;
            continue;
        }
        if (m.name.isEmpty()) { ++t.skipped; continue; }
        if (over) ++t.overUnity;
        t.materials.push_back(m);
    }
    t.rows = static_cast<int>(t.materials.size());

    if (t.materials.empty()) {
        t.error = QStringLiteral("no rows with a name and six alpha values");
        return t;
    }
    t.ok = true;
    return t;
}

AbsorptionTable readAbsorptionCsv(const QString &path)
{
    AbsorptionTable t;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        t.error = QStringLiteral("cannot open the file");
        return t;
    }
    QTextStream in(&f);
    const QString text = in.readAll();
    f.close();
    return parseAbsorptionCsv(text);
}

} // namespace ofd
