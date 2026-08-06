// ArrReader.cpp
#include "ArrReader.h"

#include <QFile>
#include <QFileInfo>
#include <cmath>

using namespace ofd;

namespace {

void setErr(QString *err, const QString &m) { if (err) *err = m; }

// 行ではなくトークン単位で読む。ヘッダの配列は 1 行に収まるとは限らず
// (全球格子のように長い場合は折り返す)、到達行は必ず 8 個 — どちらも
// 「次の N トークン」で扱うのが安全。
class TokenStream {
public:
    explicit TokenStream(QFile *f) : m_f(f) {}

    bool next(QString *tok)
    {
        while (m_pos >= m_line.size()) {
            if (m_f->atEnd()) return false;
            m_line = QString::fromLatin1(m_f->readLine()).trimmed();
            m_pos = 0;
            if (m_line.isEmpty()) { m_pos = 1; continue; }   // 空行を飛ばす
        }
        // 空白区切りで 1 つ切り出す
        while (m_pos < m_line.size() && m_line[m_pos].isSpace()) ++m_pos;
        if (m_pos >= m_line.size()) { m_pos = m_line.size() + 1; return next(tok); }
        const int start = m_pos;
        while (m_pos < m_line.size() && !m_line[m_pos].isSpace()) ++m_pos;
        *tok = m_line.mid(start, m_pos - start);
        return true;
    }

    bool nextDouble(double *v)
    {
        QString t;
        if (!next(&t)) return false;
        bool ok = false;
        *v = t.toDouble(&ok);
        return ok;
    }

    bool nextInt(int *v)
    {
        double d = 0;
        if (!nextDouble(&d)) return false;
        *v = int(d);
        return true;
    }

    // N 個の実数を読む (行をまたいでよい)
    bool nextArray(int n, QVector<double> *out)
    {
        out->clear();
        out->reserve(n);
        for (int i = 0; i < n; ++i) {
            double v = 0;
            if (!nextDouble(&v)) return false;
            out->push_back(v);
        }
        return true;
    }

private:
    QFile  *m_f;
    QString m_line;
    int     m_pos = 0;
};

// ヘッダを読み進める (ストリームは音源ブロックの先頭で止まる)
bool readHeaderInto(TokenStream &ts, ArrHeader &out, QString *err)
{
    QString dim;
    if (!ts.next(&dim)) { setErr(err, QStringLiteral("empty .arr")); return false; }
    // "'2D'" / "2D" のどちらもありうる (LDOFile は引用符を付ける)
    dim.remove(QLatin1Char('\''));
    if (dim.compare(QLatin1String("2D"), Qt::CaseInsensitive) != 0) {
        setErr(err, QStringLiteral("only 2-D arrival files are supported "
                                   "(this one is %1)").arg(dim));
        return false;
    }
    int n = 0;
    if (!ts.nextDouble(&out.freqHz)) { setErr(err, QStringLiteral("no frequency")); return false; }
    if (!ts.nextInt(&n) || n <= 0 || !ts.nextArray(n, &out.sz)) {
        setErr(err, QStringLiteral("bad source-depth block")); return false;
    }
    if (!ts.nextInt(&n) || n <= 0 || !ts.nextArray(n, &out.rz)) {
        setErr(err, QStringLiteral("bad receiver-depth block")); return false;
    }
    if (!ts.nextInt(&n) || n <= 0 || !ts.nextArray(n, &out.rr)) {
        setErr(err, QStringLiteral("bad receiver-range block")); return false;
    }
    return true;
}

} // namespace

bool ArrReader::readHeader(const QString &path, ArrHeader &out, QString *err)
{
    out = ArrHeader();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setErr(err, QStringLiteral("cannot open %1").arg(path));
        return false;
    }
    TokenStream ts(&f);
    return readHeaderInto(ts, out, err);
}

bool ArrReader::readArrivals(const QString &path, int iz, int ir,
                             ArrHeader &header, QVector<ArrArrival> &out,
                             QString *err)
{
    out.clear();
    header = ArrHeader();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setErr(err, QStringLiteral("cannot open %1").arg(path));
        return false;
    }
    TokenStream ts(&f);
    if (!readHeaderInto(ts, header, err)) return false;

    const int nrz = int(header.rz.size()), nrr = int(header.rr.size());
    if (iz < 0 || iz >= nrz || ir < 0 || ir >= nrr) {
        setErr(err, QStringLiteral("receiver (%1, %2) is outside the grid "
                                   "(%3 depths x %4 ranges)")
                        .arg(iz).arg(ir).arg(nrz).arg(nrr));
        return false;
    }

    // 音源 #0 のブロックだけを見る (GUI は単一音源しか書き出さない)
    int maxn = 0;
    if (!ts.nextInt(&maxn)) {
        setErr(err, QStringLiteral("%1: no source block")
                        .arg(QFileInfo(path).fileName()));
        return false;
    }
    // 受波器は 深度 iz → 距離 ir の順に並ぶ。目的の 1 点まで読み飛ばす。
    const qint64 target = qint64(iz) * nrr + ir;
    for (qint64 k = 0; k <= target; ++k) {
        int narr = 0;
        if (!ts.nextInt(&narr)) {
            setErr(err, QStringLiteral("%1: truncated at receiver %2")
                            .arg(QFileInfo(path).fileName()).arg(k));
            return false;
        }
        for (int a = 0; a < narr; ++a) {
            ArrArrival arr;
            double v[8] = {};
            for (int c = 0; c < 8; ++c) {
                if (!ts.nextDouble(&v[c])) {
                    setErr(err, QStringLiteral("%1: truncated arrival %2 "
                                               "at receiver %3")
                                    .arg(QFileInfo(path).fileName())
                                    .arg(a).arg(k));
                    return false;
                }
            }
            if (k != target) continue;   // 目的の受波器だけ保持する
            arr.amp = v[0];
            arr.phaseDeg = v[1];
            arr.delayS = v[2];
            arr.delayImagS = v[3];
            arr.srcAngleDeg = v[4];
            arr.rcvAngleDeg = v[5];
            arr.nTop = int(v[6]);
            arr.nBot = int(v[7]);
            out.push_back(arr);
        }
    }
    return true;
}

QVector<double> ofd::synthesizeIr(const QVector<ArrArrival> &arrivals,
                                  double fsHz, double tailS, IrSynthInfo *info)
{
    IrSynthInfo nfo;
    QVector<double> ir;
    if (arrivals.isEmpty() || !(fsHz > 0.0)) {
        if (info) *info = nfo;
        return ir;
    }
    double tMin = arrivals.first().delayS, tMax = tMin;
    for (const ArrArrival &a : arrivals) {
        if (!std::isfinite(a.delayS) || !std::isfinite(a.amp)) continue;
        tMin = std::min(tMin, a.delayS);
        tMax = std::max(tMax, a.delayS);
    }
    nfo.firstDelayS = tMin;
    nfo.lastDelayS = tMax;

    // t = 0 を最初の到達 (直接波) に置く — ISO 3382-1 の RIR と同じ規約で、
    // 可聴化したときに元の音より前に音が出る事故を防ぐ。
    const double span = tMax - tMin + std::max(0.0, tailS);
    const int kHalf = 8;   // 分数遅延 sinc の片側タップ数
    const int n = int(std::ceil(span * fsHz)) + 2 * kHalf + 1;
    if (n <= 0 || n > 200 * 1000 * 1000) {   // 200 M サンプル = 常識的な上限
        if (info) *info = nfo;
        return ir;
    }
    ir.resize(n);
    ir.fill(0.0);

    for (const ArrArrival &a : arrivals) {
        if (!std::isfinite(a.delayS) || !std::isfinite(a.amp)) { ++nfo.dropped; continue; }
        // 複素振幅 A = a·e^{iφ} の実部を置く
        const double w = a.amp * std::cos(a.phaseDeg * M_PI / 180.0);
        if (!std::isfinite(w) || w == 0.0) { ++nfo.dropped; continue; }
        const double x = (a.delayS - tMin) * fsHz + kHalf;   // サンプル位置
        const int i0 = int(std::floor(x));
        const double frac = x - i0;
        // Hann 窓付き sinc の分数遅延 (整数へ丸めると櫛形の位置がずれる)
        for (int k = -kHalf + 1; k <= kHalf; ++k) {
            const int idx = i0 + k;
            if (idx < 0 || idx >= n) continue;
            const double d = frac - k;
            double s;
            if (std::fabs(d) < 1e-9) {
                s = 1.0;
            } else {
                const double pd = M_PI * d;
                s = std::sin(pd) / pd;
                // Hann 窓 (幅 2·kHalf)
                s *= 0.5 * (1.0 + std::cos(M_PI * d / kHalf));
            }
            ir[idx] += w * s;
        }
        ++nfo.arrivals;
    }
    for (const double v : ir) nfo.peak = std::max(nfo.peak, std::fabs(v));
    nfo.length = int(ir.size());
    if (info) *info = nfo;
    return ir;
}
