// AimDirection.cpp
#include "AimDirection.h"

#include <QRegularExpression>
#include <QStringList>

#include <cmath>

namespace ofd {

namespace {

const double kPi = 3.14159265358979323846;

void normalize(double v[3], bool *ok)
{
    const double n = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (!(n > 0.0) || !std::isfinite(n)) { *ok = false; return; }
    v[0] /= n; v[1] /= n; v[2] /= n;
    *ok = true;
}

} // namespace

bool parseAim(const QString &aim, double out[3])
{
    const QString s = aim.trimmed();
    if (s.isEmpty()) return false;

    // ── ③ 明示ベクトル ("0,0,-1" / "1 0 0") ────────────────────────────
    // カンマか空白で 3 つの数に割れるならベクトルとして読む。
    {
        static const QRegularExpression sep(QStringLiteral("[,\\s]+"));
        const QStringList tok = s.split(sep, Qt::SkipEmptyParts);
        if (tok.size() == 3) {
            bool a = false, b = false, c = false;
            double v[3] = { tok[0].toDouble(&a), tok[1].toDouble(&b),
                            tok[2].toDouble(&c) };
            if (a && b && c) {
                bool ok = false;
                normalize(v, &ok);
                if (ok) { out[0] = v[0]; out[1] = v[1]; out[2] = v[2]; }
                return ok;
            }
        }
    }

    // ── ①② 軸トークン (+ 角度) ────────────────────────────────────────
    static const QRegularExpression axisRe(
        QStringLiteral("^\\s*([+-]?)\\s*([XYZxyz])\\s*"
                       "(?:([-+]?[0-9]*\\.?[0-9]+)\\s*(?:°|deg|DEG)?)?\\s*$"));
    const QRegularExpressionMatch m = axisRe.match(s);
    if (!m.hasMatch()) return false;

    const QString sign = m.captured(1);
    const QChar axis = m.captured(2).toUpper().at(0);
    const double dir = (sign == QLatin1String("-")) ? -1.0 : 1.0;

    double v[3] = { 0, 0, 0 };
    int ai = (axis == QLatin1Char('X')) ? 0 : (axis == QLatin1Char('Y')) ? 1 : 2;
    v[ai] = dir;

    const QString angStr = m.captured(3);
    if (!angStr.isEmpty()) {
        bool ok = false;
        const double deg = angStr.toDouble(&ok);
        if (!ok) return false;
        const double rad = deg * kPi / 180.0;
        // 傾ける先: 水平軸なら下向き (−Z)、軸が ±Z なら +X
        // (下向きが定義できないため。任意だが一意な基準)
        double tilt[3] = { 0, 0, -1 };
        if (ai == 2) { tilt[0] = 1; tilt[2] = 0; }
        for (int i = 0; i < 3; ++i)
            v[i] = v[i] * std::cos(rad) + tilt[i] * std::sin(rad);
    }

    bool ok = false;
    normalize(v, &ok);
    if (ok) { out[0] = v[0]; out[1] = v[1]; out[2] = v[2]; }
    return ok;
}

} // namespace ofd
