// ShdReader.cpp
#include "ShdReader.h"

#include <QDataStream>
#include <QFile>
#include <QFileInfo>
#include <cmath>
#include <limits>

using namespace ofd;

namespace {
void setErr(QString *err, const QString &m) { if (err) *err = m; }
}

bool ShdReader::read(const QString &path, ShdField &out, QString *err)
{
    out = ShdField();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        setErr(err, QStringLiteral("cannot open %1").arg(path));
        return false;
    }
    QDataStream in(&f);
    in.setByteOrder(QDataStream::LittleEndian);
    in.setFloatingPointPrecision(QDataStream::SinglePrecision);

    qint32 lrecl4 = 0;   // 4 byte 語数
    in >> lrecl4;
    if (lrecl4 <= 0 || lrecl4 > (1 << 22)) {
        setErr(err, QStringLiteral("%1: bad record length (%2)")
                        .arg(QFileInfo(path).fileName()).arg(lrecl4));
        return false;
    }
    const qint64 recl = qint64(lrecl4) * 4;

    QByteArray title(80, '\0');
    if (in.readRawData(title.data(), 80) != 80) {
        setErr(err, QStringLiteral("%1: truncated header")
                        .arg(QFileInfo(path).fileName()));
        return false;
    }
    out.title = QString::fromLatin1(title).trimmed();

    // rec 1: PlotType
    if (!f.seek(recl)) { setErr(err, QStringLiteral("%1: truncated").arg(path)); return false; }
    QByteArray plot(10, '\0');
    in.readRawData(plot.data(), 10);
    out.plotType = QString::fromLatin1(plot).trimmed();

    // rec 2: 個数
    if (!f.seek(2 * recl)) { setErr(err, QStringLiteral("%1: truncated").arg(path)); return false; }
    qint32 v[7] = {};
    for (int i = 0; i < 7; ++i) in >> v[i];
    out.nfreq = v[0]; out.ntheta = v[1];
    out.nsx = v[2]; out.nsy = v[3]; out.nsz = v[4];
    out.nrz = v[5]; out.nrr = v[6];
    if (out.nrz <= 0 || out.nrr <= 0 || out.nsz <= 0
        || qint64(out.nrr) * 8 > recl) {
        setErr(err, QStringLiteral("%1: inconsistent header "
                                   "(NRz=%2 NRr=%3 recl=%4)")
                        .arg(QFileInfo(path).fileName())
                        .arg(out.nrz).arg(out.nrr).arg(recl));
        return false;
    }

    // 音源 #0 (isx=isy=isz=0)・方位 #0 の深度断面 = rec 10 + irz
    out.tl_dB.fill(ShdField::kNoField, qsizetype(out.nrz) * out.nrr);
    double lo = std::numeric_limits<double>::max();
    double hi = -std::numeric_limits<double>::max();
    QVector<float> row(qsizetype(out.nrr) * 2);
    for (int irz = 0; irz < out.nrz; ++irz) {
        const qint64 off = (10LL + irz) * recl;
        if (!f.seek(off)) {
            setErr(err, QStringLiteral("%1: truncated at depth row %2")
                            .arg(QFileInfo(path).fileName()).arg(irz));
            return false;
        }
        for (int i = 0; i < row.size(); ++i) in >> row[i];
        if (in.status() != QDataStream::Ok) {
            setErr(err, QStringLiteral("%1: truncated at depth row %2")
                            .arg(QFileInfo(path).fileName()).arg(irz));
            return false;
        }
        for (int ir = 0; ir < out.nrr; ++ir) {
            const double re = row[2 * ir], im = row[2 * ir + 1];
            const double a = std::hypot(re, im);
            if (!(a > 0.0) || !std::isfinite(a)) continue;   // 到達なし
            const double tl = -20.0 * std::log10(a);
            out.tl_dB[qsizetype(irz) * out.nrr + ir] = float(tl);
            lo = std::min(lo, tl);
            hi = std::max(hi, tl);
        }
    }
    if (lo > hi) {
        setErr(err, QStringLiteral("%1: the field is empty "
                                   "(no ray reached any receiver)")
                        .arg(QFileInfo(path).fileName()));
        return false;
    }
    out.minTL = lo;
    out.maxTL = hi;
    return true;
}
