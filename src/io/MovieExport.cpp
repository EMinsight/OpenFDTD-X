// MovieExport.cpp
#include "MovieExport.h"

#include <algorithm>
#include <cmath>

namespace ofd {
namespace movie {

bool frameRangeForTimes(const QVector<double> &times, double loSec,
                        double hiSec, int &f0, int &f1)
{
    if (times.isEmpty()) return false;
    if (loSec > hiSec) std::swap(loSec, hiSec);

    // 端の比較には相対イプシロンを使う。利用者は「フレームの時刻ちょうど」を
    // 境界に入れるのが自然だが、時刻列が t[i] = i·Δt のように積まれていると
    // 丸めで境界の内外が入れ替わる。1e-9 相対はフレーム間隔よりはるかに
    // 小さく、丸め誤差 (1e-16 相対) よりはるかに大きい。
    const double mag = std::max(std::fabs(loSec), std::fabs(hiSec));
    const double eps = 1e-9 * mag;

    int a = -1, b = -1;
    for (int i = 0; i < times.size(); ++i) {
        const double t = times[i];
        if (t < loSec - eps || t > hiSec + eps) continue;
        if (a < 0) a = i;
        b = i;
    }
    if (a < 0) return false;      // 1 フレームも入らない
    f0 = a;
    f1 = b;
    return true;
}

QStringList buildFfmpegArgs(const QString &framePattern,
                            const QString &outPath, const MovieOptions &opt)
{
    const int fps = std::min(240, std::max(1, opt.fps));

    QStringList args;
    args << QStringLiteral("-y")
         << QStringLiteral("-framerate") << QString::number(fps)
         << QStringLiteral("-i") << framePattern;

    // スケール (指定があるときだけ)。アスペクト比を保つため片方 0 なら -1。
    QStringList filters;
    if (opt.width > 0 || opt.height > 0) {
        filters << QStringLiteral("scale=%1:%2")
                       .arg(opt.width > 0 ? QString::number(opt.width)
                                          : QStringLiteral("-2"),
                            opt.height > 0 ? QString::number(opt.height)
                                           : QStringLiteral("-2"));
    }

    if (opt.gif) {
        filters << QStringLiteral("fps=%1").arg(fps);
        args << QStringLiteral("-vf") << filters.join(QLatin1Char(','));
        args << outPath;
        return args;
    }

    // yuv420p は偶数サイズを要求する — スケール後に必ず偶数化する
    filters << QStringLiteral("crop=trunc(iw/2)*2:trunc(ih/2)*2");

    const char *codec = "libx264";
    switch (opt.codec) {
        case Codec::H265: codec = "libx265";    break;
        case Codec::VP9:  codec = "libvpx-vp9"; break;
        case Codec::H264:
        default:          codec = "libx264";    break;
    }
    args << QStringLiteral("-c:v") << QLatin1String(codec)
         << QStringLiteral("-pix_fmt") << QStringLiteral("yuv420p")
         << QStringLiteral("-vf") << filters.join(QLatin1Char(','))
         << outPath;
    return args;
}

} // namespace movie
} // namespace ofd
