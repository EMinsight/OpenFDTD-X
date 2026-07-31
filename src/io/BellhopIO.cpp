// BellhopIO.cpp
#include "BellhopIO.h"
#include "../core/Project.h"

#include <QFileInfo>
#include <QTextStream>
#include <algorithm>

using namespace ofd;

// .ofd 系と同じ簡潔な数値表記 (指数の冗長なゼロを出さない)
static QString num(double v) { return QString::number(v, 'g', 10); }

QString BellhopIO::caseName(const Project &p)
{
    const QString path = p.filePath();
    return path.isEmpty() ? QStringLiteral("project")
                          : QFileInfo(path).completeBaseName();
}

QString BellhopIO::envText(const Project &p)
{
    const UnderwaterOpts &u = p.underwater();

    // ── SSP: 深度昇順に整列。2 点未満なら等速 1500 m/s の既定プロファイル ──
    QVector<SSPPoint> ssp = u.ssp;
    std::sort(ssp.begin(), ssp.end(),
              [](const SSPPoint &a, const SSPPoint &b) {
                  return a.depth_m < b.depth_m;
              });
    if (ssp.size() < 2) {
        ssp = { { 0.0, 1500.0 }, { 100.0, 1500.0 } };
    }
    if (ssp.first().depth_m > 0.0) {
        // BELLHOP は海面 (0 m) からのプロファイルを要求する
        ssp.prepend({ 0.0, ssp.first().c_mps });
    }
    const double bottomDepth = ssp.last().depth_m;

    // ── 音源深度: 海面直下 (最大深度の 10%、1 m 以上・底より上) ──
    const double srcDepth =
        std::min(std::max(1.0, bottomDepth * 0.1), bottomDepth - 1.0);

    QString text;
    QTextStream out(&text);

    out << "'OpenFDTD-X underwater (" << u.bottomType << " bottom)'\t! TITLE\n";
    out << num(u.sonarFreq_kHz * 1000.0) << "\t\t\t! FREQ (Hz)\n";
    out << "1\t\t\t! NMEDIA\n";
    out << "'CVW'\t\t\t! SSPOPT (C-linear, vacuum surface, dB/wavelength)\n";
    out << "0 0.0 " << num(bottomDepth) << "\t\t! NMESH, SIGMA, DEPTH of bottom (m)\n";
    for (const SSPPoint &s : ssp)
        out << num(s.depth_m) << " " << num(s.c_mps) << " /\n";

    // 底面: 音響半無限層 ('A')。減衰 0.5 dB/λ は砂〜シルト底の代表値。
    // 密度は kg/m^3 → g/cm^3。
    out << "'A' 0.0\n";
    out << num(bottomDepth) << " " << num(u.bottomC_mps) << " 0.0 "
        << num(u.bottomRho_kgm3 / 1000.0) << " 0.5 /\n";

    // 音源・受波器・距離レンジ
    out << "1\t\t\t! NSD\n";
    out << num(srcDepth) << " /\t\t\t! SD (m)\n";
    out << "201\t\t\t! NRD\n";
    out << "0.0 " << num(bottomDepth) << " /\t\t! RD (m)\n";
    out << "501\t\t\t! NR\n";
    out << "0.0 " << num(u.rangeMax_km) << " /\t\t! R (km)\n";

    // コヒーレント伝搬損失 (TL)。ビーム数 0 = カーネル自動。
    out << "'C'\t\t\t! RunType (coherent TL)\n";
    out << "0\t\t\t! NBEAMS (0 = auto)\n";
    out << "-45.0 45.0 /\t\t! ALPHA1,2 (degrees)\n";
    out << "0.0 " << num(bottomDepth + 100.0) << " "
        << num(u.rangeMax_km + 1.0) << "\t! STEP (m), ZBOX (m), RBOX (km)\n";
    return text;
}
