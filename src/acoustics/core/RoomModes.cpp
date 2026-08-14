// RoomModes.cpp — 直方体室 (剛壁) の音響固有モード (実装)。
// 式の出典はヘッダのコメントを参照。
#include "RoomModes.h"

#include <algorithm>
#include <cmath>

namespace ofd {
namespace acoustics {
namespace roommodes {

const int kEnumerationLimit = 20000;

namespace {

bool isFinitePositive(double v)
{
    return v == v && v > 0.0 && v < 1e300;
}

// 周波数の昇順、同順位は次数の辞書順 (決定性のため)
bool byFrequency(const Mode &a, const Mode &b)
{
    if (a.freqHz != b.freqHz) return a.freqHz < b.freqHz;
    if (a.nx != b.nx) return a.nx < b.nx;
    if (a.ny != b.ny) return a.ny < b.ny;
    return a.nz < b.nz;
}

} // namespace

double soundSpeed(double tempC)
{
    // ISO 9613-1:1993 (乾燥空気, 0 ℃ で 331.3 m/s)
    const double ratio = 1.0 + tempC / 273.15;
    if (!(ratio > 0.0)) return 0.0;
    return 331.3 * std::sqrt(ratio);
}

double modeFrequency(int nx, int ny, int nz,
                     double lengthM, double widthM, double heightM,
                     double soundSpeedMs)
{
    if (!isFinitePositive(lengthM) || !isFinitePositive(widthM)
        || !isFinitePositive(heightM) || !isFinitePositive(soundSpeedMs))
        return 0.0;
    if (nx < 0 || ny < 0 || nz < 0) return 0.0;
    if (nx == 0 && ny == 0 && nz == 0) return 0.0;

    const double ax = nx / lengthM;
    const double ay = ny / widthM;
    const double az = nz / heightM;
    return 0.5 * soundSpeedMs * std::sqrt(ax * ax + ay * ay + az * az);
}

std::vector<Mode> rectangularModes(double lengthM, double widthM,
                                   double heightM, double soundSpeedMs,
                                   double fMaxHz, int maxModes)
{
    std::vector<Mode> out;
    if (!isFinitePositive(lengthM) || !isFinitePositive(widthM)
        || !isFinitePositive(heightM) || !isFinitePositive(soundSpeedMs)
        || !isFinitePositive(fMaxHz))
        return out;

    // 各軸の最大次数: 単独で f_max に達する次数 n = 2·f_max·L/c
    const double half = 2.0 * fMaxHz / soundSpeedMs;
    const double nxMaxD = half * lengthM;
    const double nyMaxD = half * widthM;
    const double nzMaxD = half * heightM;
    // 1 軸あたりの上限 (病的な入力で無限ループにしない)
    const double kAxisCap = 512.0;
    const int nxMax = int(std::min(std::floor(nxMaxD), kAxisCap));
    const int nyMax = int(std::min(std::floor(nyMaxD), kAxisCap));
    const int nzMax = int(std::min(std::floor(nzMaxD), kAxisCap));

    for (int ix = 0; ix <= nxMax; ++ix) {
        for (int iy = 0; iy <= nyMax; ++iy) {
            for (int iz = 0; iz <= nzMax; ++iz) {
                if (ix == 0 && iy == 0 && iz == 0) continue;
                const double f = modeFrequency(ix, iy, iz, lengthM, widthM,
                                               heightM, soundSpeedMs);
                if (f <= 0.0 || f > fMaxHz) continue;
                Mode m;
                m.nx = ix; m.ny = iy; m.nz = iz;
                m.freqHz = f;
                m.kind = (ix > 0 ? 1 : 0) + (iy > 0 ? 1 : 0) + (iz > 0 ? 1 : 0);
                out.push_back(m);
                if (int(out.size()) >= kEnumerationLimit) {
                    // 上限に達したら打ち切る (低次側は既に網羅されている)
                    std::sort(out.begin(), out.end(), byFrequency);
                    if (maxModes > 0 && int(out.size()) > maxModes)
                        out.resize(size_t(maxModes));
                    return out;
                }
            }
        }
    }

    std::sort(out.begin(), out.end(), byFrequency);
    if (maxModes > 0 && int(out.size()) > maxModes)
        out.resize(size_t(maxModes));
    return out;
}


double schroederFrequency(double rt60S, double volumeM3)
{
    if (!isFinitePositive(rt60S) || !isFinitePositive(volumeM3)) return 0.0;
    return 2000.0 * std::sqrt(rt60S / volumeM3);
}

std::vector<NotchCandidate> notchCandidates(double lengthM, double widthM,
                                            double heightM,
                                            double soundSpeedMs,
                                            double rt60S, int maxCount)
{
    std::vector<NotchCandidate> out;
    if (!isFinitePositive(lengthM) || !isFinitePositive(widthM)
        || !isFinitePositive(heightM) || !isFinitePositive(soundSpeedMs)
        || !isFinitePositive(rt60S))
        return out;

    const double volume = lengthM * widthM * heightM;
    const double fs = schroederFrequency(rt60S, volume);
    if (!(fs > 0.0)) return out;

    // モードの半値幅 (これより近いモードは互いに重なって 1 つの山になる)
    const double bw = 2.2 / rt60S;

    const std::vector<Mode> modes =
        rectangularModes(lengthM, widthM, heightM, soundSpeedMs, fs, 0);
    if (modes.empty()) return out;

    // 半値幅の中で重なるものをまとめる。代表は最も低い次数のもの
    // (rectangularModes が周波数昇順・次数の辞書順で返すので先頭が代表)
    for (std::size_t i = 0; i < modes.size();) {
        std::size_t j = i + 1;
        while (j < modes.size() && (modes[j].freqHz - modes[i].freqHz) < bw)
            ++j;
        NotchCandidate c;
        c.mode = modes[i];
        c.freqHz = modes[i].freqHz;
        c.bandwidthHz = bw;
        c.q = (bw > 0.0) ? (modes[i].freqHz / bw) : 0.0;
        c.coincident = static_cast<int>(j - i);
        out.push_back(c);
        i = j;
        if (maxCount > 0 && static_cast<int>(out.size()) >= maxCount) break;
    }
    return out;
}

} // namespace roommodes
} // namespace acoustics
} // namespace ofd
