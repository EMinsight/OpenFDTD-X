// BeamPatternCsv.cpp
#include "BeamPatternCsv.h"

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>

using namespace ofd;
using namespace ofd::beamcsv;

namespace {

// 計測ファイルとして現実的な上限。これを超えるものは指向パターンではなく
// 別のデータを間違えて開いている可能性が高いので、読む前に弾く。
const qint64 kMaxBytes = 4 * 1024 * 1024;
const int    kMaxPoints = 100000;

// 区切りは「カンマ / タブ / セミコロン / 連続空白」のいずれでもよい。
// 計測器の書出しが揃っていないため、1 つに決め打ちしない。
QStringList splitColumns(const QString &line)
{
    static const QRegularExpression re(QStringLiteral("[,;\\t ]+"));
    return line.trimmed().split(re, Qt::SkipEmptyParts);
}

bool isComment(const QString &s)
{
    if (s.isEmpty()) return true;
    const QChar c = s.at(0);
    return c == '#' || c == '!' || c == ';';
}

} // namespace

Result beamcsv::parse(const QString &text, QString *err)
{
    Result r;
    const auto fail = [&](const QString &why) {
        if (err) *err = why;
        r.points.clear();
        return r;
    };

    // 改行は CRLF / LF / CR のいずれでも受ける (計測器と OS の組合せが読めない)
    static const QRegularExpression nl(QStringLiteral("\r\n|\r|\n"));
    const QStringList lines = text.split(nl);

    bool headerAllowed = true;      // 見出し行を許すのは最初の 1 行だけ
    int lineNo = 0;
    for (const QString &raw : lines) {
        ++lineNo;
        const QString line = raw.trimmed();
        // 空行は数えない (末尾改行で 1 増えてしまい、数として意味が無くなる)
        if (isComment(line)) { if (!line.isEmpty()) ++r.skipped; continue; }

        const QStringList col = splitColumns(line);
        if (col.size() < 2)
            return fail(QStringLiteral("line %1: expected 2 columns (angle, "
                                       "level), got %2")
                            .arg(lineNo).arg(col.size()));

        bool okA = false, okB = false;
        const double a = col[0].toDouble(&okA);
        const double b = col[1].toDouble(&okB);
        if (!okA || !okB) {
            // 先頭の 1 行だけは列名とみなして読み飛ばす
            if (headerAllowed) { headerAllowed = false; ++r.skipped; continue; }
            return fail(QStringLiteral("line %1: not a number (%2)")
                            .arg(lineNo).arg(line.left(40)));
        }
        headerAllowed = false;

        if (!std::isfinite(a) || !std::isfinite(b))
            return fail(QStringLiteral("line %1: value is not finite").arg(lineNo));
        if (a < -180.0 || a > 180.0)
            return fail(QStringLiteral("line %1: angle %2 deg is outside "
                                       "[-180, 180]").arg(lineNo).arg(a));
        if (r.points.size() >= kMaxPoints)
            return fail(QStringLiteral("too many points (limit %1)")
                            .arg(kMaxPoints));
        r.points.push_back({ a, b });
    }

    if (r.points.size() < 2)
        return fail(QStringLiteral("need at least 2 points, got %1")
                        .arg(r.points.size()));

    // 角度昇順へ。表の順序は計測器によって降順のこともある。
    std::sort(r.points.begin(), r.points.end(),
              [](const BeamPatternPoint &x, const BeamPatternPoint &y) {
                  return x.angle_deg < y.angle_deg;
              });
    for (int i = 1; i < r.points.size(); ++i) {
        if (r.points[i].angle_deg == r.points[i - 1].angle_deg)
            return fail(QStringLiteral("duplicate angle %1 deg")
                            .arg(r.points[i].angle_deg));
    }

    // ピークを 0 dB へ平行移動する (理由はヘッダ参照)。引いた量は返す。
    double peak = r.points[0].level_dB;
    for (const BeamPatternPoint &p : r.points) peak = std::max(peak, p.level_dB);
    r.shift_dB = peak;
    for (BeamPatternPoint &p : r.points) p.level_dB -= peak;
    return r;
}

Result beamcsv::load(const QString &path, QString *err)
{
    Result r;
    const QFileInfo fi(path);
    if (!fi.exists() || !fi.isFile()) {
        if (err) *err = QStringLiteral("file not found");
        return r;
    }
    if (fi.size() > kMaxBytes) {
        if (err) *err = QStringLiteral("file is too large (%1 bytes, limit %2)")
                            .arg(fi.size()).arg(kMaxBytes);
        return r;
    }
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (err) *err = f.errorString();
        return r;
    }
    return parse(QString::fromUtf8(f.readAll()), err);
}

QVector<BeamPatternPoint> beamcsv::clampToFloor(
    const QVector<BeamPatternPoint> &pts, double floorDb)
{
    QVector<BeamPatternPoint> out = pts;
    for (BeamPatternPoint &p : out)
        p.level_dB = std::max(p.level_dB, floorDb);
    return out;
}
