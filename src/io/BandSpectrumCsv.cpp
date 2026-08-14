// BandSpectrumCsv.cpp — 帯域スペクトルの CSV 書出 (詳細は .h)
#include "BandSpectrumCsv.h"

namespace ofd {
namespace io {

namespace {

// `#` 行に入れる文字。改行が混ざると行番号がずれ、`,` は列を壊す
QString oneLine(const QString &s)
{
    QString t = s;
    t.replace('\r', ' ');
    t.replace('\n', ' ');
    return t.trimmed();
}

} // namespace

QString buildBandSpectrumCsv(const BandSpectrum &s)
{
    if (!s.isValid()) return QString();

    const QString quantity = s.quantity.isEmpty() ? QStringLiteral("value")
                                                  : oneLine(s.quantity);
    const QString unit = oneLine(s.unit);

    QString out;
    out += QStringLiteral("# OpenFDTD-X band spectrum\n");
    if (!s.scenario.isEmpty())
        out += QStringLiteral("# scenario: %1\n").arg(oneLine(s.scenario));
    out += unit.isEmpty()
               ? QStringLiteral("# quantity: %1\n").arg(quantity)
               : QStringLiteral("# quantity: %1 [%2]\n").arg(quantity, unit);
    for (const QString &n : s.notes)
        if (!n.trimmed().isEmpty())
            out += QStringLiteral("# %1\n").arg(oneLine(n));

    // 見出し行 (数値に読めないので参照系列として読むときは落ちる)
    out += unit.isEmpty()
               ? QStringLiteral("freq_Hz,%1\n").arg(quantity)
               : QStringLiteral("freq_Hz,%1_%2\n").arg(quantity, unit);

    for (int i = 0; i < s.freqHz.size(); ++i) {
        // C ロケール固定 (小数点が `,` になると列が壊れる)
        out += QStringLiteral("%1,%2\n")
                   .arg(QString::number(s.freqHz[i], 'g', 10),
                        QString::number(s.value[i], 'g', 10));
    }
    return out;
}

} // namespace io
} // namespace ofd
