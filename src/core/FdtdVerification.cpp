// FdtdVerification.cpp — FdtdVerification.h の実装 (Qt 非依存)
#include "FdtdVerification.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>

namespace ofd {
namespace verify {

namespace {

constexpr double kPi = 3.14159265358979323846;

// 有限かつ正か
bool posFinite(double v) { return std::isfinite(v) && v > 0.0; }

} // namespace

// ── ① メッシュ解像度の計画値 ───────────────────────────────────────────────
std::vector<MeshLevel> meshConvergenceLevels(const Grid &grid, double lambda_m,
                                             const std::vector<double> &refine)
{
    std::vector<MeshLevel> out;
    out.reserve(refine.size());

    for (double r : refine) {
        if (!posFinite(r)) continue;

        MeshLevel lv;
        lv.refine = r;

        long long total = 1;
        double dxMax = 0.0;
        bool any = false;
        for (int a = 0; a < 3; ++a) {
            const AxisGrid &ax = grid.axis[a];
            if (ax.cells <= 0) continue;
            any = true;
            // 分割数は各軸独立に丸める (最低 1 分割)
            long long n = static_cast<long long>(
                std::llround(static_cast<double>(ax.cells) * r));
            if (n < 1) n = 1;
            total *= n;
            // セル幅は分割数の逆比で縮む (区間長は不変)
            if (posFinite(ax.dxMax_m))
                dxMax = std::max(dxMax,
                                 ax.dxMax_m * static_cast<double>(ax.cells) /
                                     static_cast<double>(n));
        }
        if (!any) continue;

        lv.cells    = total;
        lv.dxMax_m  = dxMax;
        lv.memoryMB = static_cast<double>(total) * kBytesPerCell /
                      (1024.0 * 1024.0);
        lv.lambdaOverDx =
            (posFinite(lambda_m) && posFinite(dxMax)) ? lambda_m / dxMax : 0.0;
        out.push_back(lv);
    }
    return out;
}

// ── ② 吸収境界の設計反射率 ─────────────────────────────────────────────────
double pmlDesignReflection(double r0, double thetaDeg)
{
    if (!std::isfinite(r0) || r0 <= 0.0 || r0 >= 1.0) return 1.0;
    if (!std::isfinite(thetaDeg)) return 1.0;
    const double c = std::cos(thetaDeg * kPi / 180.0);
    if (c <= 0.0) return 1.0;              // 接線入射: 吸収されない
    // R(θ) = R0^cosθ  ([1] §7.7, [2] 式(26))
    return std::pow(r0, c);
}

double murDesignReflection(double thetaDeg)
{
    if (!std::isfinite(thetaDeg)) return 1.0;
    const double c = std::cos(thetaDeg * kPi / 180.0);
    if (c <= 0.0) return 1.0;              // 接線入射: 全反射
    if (c >= 1.0) return 0.0;              // 垂直入射: 1 次 Mur は無反射
    // R(θ) = (1 − cosθ)/(1 + cosθ)  ([3][4])
    return (1.0 - c) / (1.0 + c);
}

double toDb(double amplitudeRatio, double floorDb)
{
    if (!std::isfinite(amplitudeRatio) || amplitudeRatio <= 0.0) return floorDb;
    const double db = 20.0 * std::log10(amplitudeRatio);
    return (db < floorDb) ? floorDb : db;
}

// ── ③ ソルバー実行ログの収束履歴 ───────────────────────────────────────────
std::vector<ConvergencePoint> parseConvergenceLog(const std::string &text,
                                                  std::size_t maxPoints)
{
    std::vector<ConvergencePoint> out;
    long long lastStep = -1;

    std::size_t pos = 0;
    const std::size_t n = text.size();
    while (pos <= n) {
        std::size_t eol = text.find('\n', pos);
        if (eol == std::string::npos) eol = n;
        const std::string line = text.substr(pos, eol - pos);
        pos = eol + 1;

        // 空白区切りでちょうど 3 トークンの行だけを候補にする
        std::string tok[3];
        int ntok = 0;
        std::size_t i = 0;
        bool tooMany = false;
        while (i < line.size()) {
            while (i < line.size() &&
                   std::isspace(static_cast<unsigned char>(line[i]))) ++i;
            if (i >= line.size()) break;
            const std::size_t s = i;
            while (i < line.size() &&
                   !std::isspace(static_cast<unsigned char>(line[i]))) ++i;
            if (ntok >= 3) { tooMany = true; break; }
            tok[ntok++] = line.substr(s, i - s);
        }
        if (tooMany || ntok != 3) continue;

        // 第 1 トークン: 非負整数のみ
        if (tok[0].empty()) continue;
        bool digits = true;
        for (char ch : tok[0])
            if (!std::isdigit(static_cast<unsigned char>(ch))) { digits = false; break; }
        if (!digits) continue;

        // 第 2・第 3 トークン: 全体が有限の実数として読み切れること
        double val[2];
        bool okNum = true;
        for (int k = 0; k < 2 && okNum; ++k) {
            const char *b = tok[k + 1].c_str();
            char *end = nullptr;
            val[k] = std::strtod(b, &end);
            okNum = (end != nullptr) && (*end == '\0') && (end != b) &&
                    std::isfinite(val[k]);
        }
        if (!okNum) continue;

        char *endStep = nullptr;
        const long long step = std::strtoll(tok[0].c_str(), &endStep, 10);
        if (endStep == nullptr || *endStep != '\0') continue;
        if (step <= lastStep) continue;    // 単調増加でない行は収束履歴ではない

        lastStep = step;
        if (out.size() < maxPoints) {
            ConvergencePoint p;
            p.step = step;
            p.e    = val[0];
            p.h    = val[1];
            out.push_back(p);
        }
    }
    return out;
}

Verdict convergenceVerdict(const std::vector<ConvergencePoint> &history,
                           double threshold)
{
    if (history.empty()) return Verdict::Unknown;
    if (!posFinite(threshold)) return Verdict::Unknown;
    const ConvergencePoint &last = history.back();
    return (last.e <= threshold && last.h <= threshold) ? Verdict::Ok
                                                        : Verdict::Warn;
}

// ── ④ 自動診断 ─────────────────────────────────────────────────────────────
double courantNumber(double dt_s, double speed_mps, const double dxMin_m[3])
{
    if (!posFinite(dt_s) || !posFinite(speed_mps) || dxMin_m == nullptr)
        return 0.0;
    double s = 0.0;
    for (int a = 0; a < 3; ++a) {
        const double d = dxMin_m[a];
        if (!posFinite(d)) return 0.0;
        s += 1.0 / (d * d);
    }
    if (s <= 0.0) return 0.0;
    return speed_mps * dt_s * std::sqrt(s);
}

Verdict courantVerdict(double courant)
{
    if (!std::isfinite(courant) || courant <= 0.0) return Verdict::Unknown;
    if (courant <= 0.99) return Verdict::Ok;
    if (courant <= 1.00) return Verdict::Warn;
    return Verdict::Ng;
}

Verdict resolutionVerdict(double lambdaOverDx)
{
    if (!std::isfinite(lambdaOverDx) || lambdaOverDx <= 0.0)
        return Verdict::Unknown;
    if (lambdaOverDx >= 10.0) return Verdict::Ok;
    if (lambdaOverDx >= 6.0)  return Verdict::Warn;
    return Verdict::Ng;
}

Verdict absorbingBoundaryVerdict(bool pml, int layers)
{
    if (!pml) return Verdict::Warn;        // 1 次 Mur: 斜入射の反射が大きい
    if (layers >= 8) return Verdict::Ok;
    if (layers >= 5) return Verdict::Warn;
    return Verdict::Ng;
}

Verdict separationVerdict(double distanceOverLambda)
{
    if (!std::isfinite(distanceOverLambda) || distanceOverLambda <= 0.0)
        return Verdict::Unknown;
    if (distanceOverLambda >= 1.0) return Verdict::Ok;
    if (distanceOverLambda >= 1.0 / (2.0 * kPi)) return Verdict::Warn;
    return Verdict::Ng;
}

Verdict marginVerdict(double marginOverLambda)
{
    if (!std::isfinite(marginOverLambda)) return Verdict::Unknown;
    if (marginOverLambda >= 0.25)  return Verdict::Ok;
    if (marginOverLambda >= 0.125) return Verdict::Warn;
    return Verdict::Ng;
}

} // namespace verify
} // namespace ofd
