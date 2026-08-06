// BellhopIO.cpp
#include "BellhopIO.h"
#include "../core/Project.h"

#include <QFileInfo>
#include <QTextStream>
#include <algorithm>
#include <cmath>

using namespace ofd;

// .ofd 系と同じ簡潔な数値表記 (指数の冗長なゼロを出さない)
static QString num(double v) { return QString::number(v, 'g', 10); }

QString BellhopIO::caseName(const Project &p)
{
    const QString path = p.filePath();
    return path.isEmpty() ? QStringLiteral("project")
                          : QFileInfo(path).completeBaseName();
}

namespace {

// 地形断面を距離昇順・単調増加に整える (BELLHOP は単調増加を要求し、
// そうでなければ実行時エラーになる)。距離が重複する点は後勝ちで潰す。
QVector<BathyPoint> cleanBathymetry(const QVector<BathyPoint> &in)
{
    QVector<BathyPoint> b = in;
    std::sort(b.begin(), b.end(), [](const BathyPoint &a, const BathyPoint &c) {
        return a.range_km < c.range_km;
    });
    QVector<BathyPoint> out;
    for (const BathyPoint &q : b) {
        if (!std::isfinite(q.range_km) || !std::isfinite(q.depth_m)) continue;
        if (q.depth_m <= 0.0) continue;   // 陸域・欠測は落とす
        if (!out.isEmpty() && out.last().range_km >= q.range_km)
            out.last() = q;
        else
            out.push_back(q);
    }
    return out;
}

// RunType 1 文字目 (BELLHOP: R=レイ / E=固有線 / C=コヒーレント TL /
// S=セミコヒーレント / I=インコヒーレント / A=到達時間 ASCII)
char runTypeChar(const QString &mode)
{
    if (mode == QLatin1String("ray"))          return 'R';
    if (mode == QLatin1String("eigenray"))     return 'E';
    if (mode == QLatin1String("incoherent"))   return 'I';
    if (mode == QLatin1String("semicoherent")) return 'S';
    if (mode == QLatin1String("arrivals"))     return 'A';
    return 'C';   // coherent (既定)
}

// RunType 2 文字目 = ビーム種別。既定 'G' は bellhopcuda の
// src/module/runtype.hpp Validate() が未指定時に入れる値と同じなので、
// 従来 (1 文字だけ書いていた頃) と挙動が変わらない。
char beamTypeChar(const QString &type)
{
    if (type == QLatin1String("gaussian"))     return 'B';  // 幾何ガウシアン
    if (type == QLatin1String("hat"))          return 'g';  // 幾何 hat (レイ中心)
    if (type == QLatin1String("cartesian"))    return 'C';  // Cerveny 直交
    if (type == QLatin1String("raycentered"))  return 'R';  // Cerveny レイ中心
    return 'G';   // geometric (幾何 hat / 直交座標) — 既定
}

} // namespace

QString BellhopIO::btyText(const Project &p)
{
    const QVector<BathyPoint> b = cleanBathymetry(p.underwater().bathymetry);
    if (b.size() < 2) return QString();

    QString text;
    QTextStream out(&text);
    // 補間種別 'L' = 区分線形。曲線補間 'C' は点が疎だと法線が暴れるので使わない。
    out << "'L'\n";
    out << b.size() << "\n";
    for (const BathyPoint &q : b)
        out << num(q.range_km) << " " << num(q.depth_m) << "\n";
    return text;
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

    // ── 海底地形 (.bty)。BELLHOP は「地形が SSP より深い」とエラーにするので
    //    (bellhopcuda src/module/boundary.hpp: "Bathymetry drops below the
    //    sound speed profile")、断面の最深点まで SSP を末尾の音速で延長する ──
    const QVector<BathyPoint> bathy = cleanBathymetry(u.bathymetry);
    const bool useBty = bathy.size() >= 2;
    double bottomDepth = ssp.last().depth_m;
    if (useBty) {
        double deepest = 0.0;
        for (const BathyPoint &q : bathy) deepest = std::max(deepest, q.depth_m);
        if (deepest > bottomDepth) {
            ssp.push_back({ deepest, ssp.last().c_mps });
            bottomDepth = deepest;
        }
    }

    // ── 音源深度: 既定 0 なら従来どおり自動 (最大深度の 10%、1 m 以上・底より上) ──
    const double srcDepth =
        (u.srcDepth_m > 0.0)
            ? std::min(u.srcDepth_m, bottomDepth - 1.0)
            : std::min(std::max(1.0, bottomDepth * 0.1), bottomDepth - 1.0);

    QString text;
    QTextStream out(&text);

    out << "'OpenFDTD-X underwater (" << u.bottomType << " bottom)'\t! TITLE\n";
    out << num(u.sonarFreq_kHz * 1000.0) << "\t\t\t! FREQ (Hz)\n";
    out << "1\t\t\t! NMEDIA\n";
    out << "'CVW'\t\t\t! SSPOPT (C-linear, vacuum surface, dB/wavelength)\n";
    out << "0 0.0 " << num(bottomDepth) << "\t\t! NMESH, SIGMA, DEPTH of bottom (m)\n";
    for (const SSPPoint &s : ssp)
        out << num(s.depth_m) << " " << num(s.c_mps) << " /\n";

    // 底面: 音響半無限層 ('A')。2 文字目 '~' は BTYFIL (.bty) を読ませる指示で、
    // これが無いと .bty があっても黙って無視され平坦海底になる
    // (bellhopcuda src/module/boundary.hpp の IsFile(): '~' または '*')。
    // 減衰は UnderwaterOpts::bottomAlpha_dBlambda [dB/λ] (SSPOPT 3 文字目 'W'
    // = dB/wavelength と整合)。既定 0.5 は砂〜シルト底の代表値 (従来の
    // ハードコード値 — 既定なら .env はバイト一致)。密度は kg/m^3 → g/cm^3。
    out << (useBty ? "'A~' 0.0\n" : "'A' 0.0\n");
    out << num(bottomDepth) << " " << num(u.bottomC_mps) << " 0.0 "
        << num(u.bottomRho_kgm3 / 1000.0) << " "
        << num(u.bottomAlpha_dBlambda) << " /\n";

    // 音源・受波器・距離レンジ
    out << "1\t\t\t! NSD\n";
    out << num(srcDepth) << " /\t\t\t! SD (m)\n";
    out << num(u.numRcvDepth) << "\t\t\t! NRD\n";
    out << "0.0 " << num(bottomDepth) << " /\t\t! RD (m)\n";
    out << num(u.numRcvRange) << "\t\t\t! NR\n";
    out << "0.0 " << num(u.rangeMax_km) << " /\t\t! R (km)\n";

    // RunType (1 文字目 = 計算モード, 2 文字目 = ビーム種別) と
    // ビーム本数・射出角。いずれも UnderwaterTab の設定から。
    const char rt[3] = { runTypeChar(u.runMode), beamTypeChar(u.beamType), 0 };
    out << "'" << QString::fromLatin1(rt, 2) << "'\t\t\t! RunType, beam type\n";
    out << num(u.numRays) << "\t\t\t! NBEAMS (0 = auto)\n";
    out << num(u.angleMin_deg) << " " << num(u.angleMax_deg)
        << " /\t\t! ALPHA1,2 (degrees)\n";
    out << "0.0 " << num(bottomDepth + 100.0) << " "
        << num(u.rangeMax_km + 1.0) << "\t! STEP (m), ZBOX (m), RBOX (km)\n";
    return text;
}
