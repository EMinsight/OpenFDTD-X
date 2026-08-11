// SeidelAberration.cpp — 3 次収差 (仕様と式は SeidelAberration.h)
#include "SeidelAberration.h"

#include <cmath>

namespace ofd {
namespace seidel {

namespace {
constexpr double kPi = 3.14159265358979323846;

// 1 面ぶんの近軸状態 (面へ入る前と出た後)
struct State {
    double y = 0.0;      // 面での高さ
    double uIn = 0.0;    // 面へ入る角 u
    double uOut = 0.0;   // 面を出た角 u'
    double nIn = 1.0;
    double nOut = 1.0;
    double c = 0.0;      // 曲率 1/R (平面は 0)
};

// y-nu 追跡。y0/u0 は第 1 面での高さと角。
std::vector<State> trace(const std::vector<paraxial::Surface> &s,
                         double y0, double u0)
{
    std::vector<State> out;
    out.reserve(s.size());
    double y = y0, u = u0, n = 1.0;
    for (std::size_t i = 0; i < s.size(); ++i) {
        const double nAfter = (s[i].nAfter > 0.0) ? s[i].nAfter : 1.0;
        State st;
        st.y = y;
        st.uIn = u;
        st.nIn = n;
        st.nOut = nAfter;
        st.c = (s[i].R != 0.0) ? 1.0 / s[i].R : 0.0;
        const double phi = (s[i].R != 0.0) ? (nAfter - n) / s[i].R : 0.0;
        st.uOut = (n * u - y * phi) / nAfter;
        out.push_back(st);
        u = st.uOut;
        n = nAfter;
        y += u * s[i].thickness;
    }
    return out;
}
} // namespace

Result analyze(const std::vector<paraxial::Surface> &surfaces,
               double epd, double fieldHalf_deg)
{
    Result r;
    if (surfaces.empty() || epd <= 0.0) return r;

    // 絞り面 (指定が無ければ第 1 面を絞りとみなす)
    int stop = -1;
    for (std::size_t i = 0; i < surfaces.size(); ++i)
        if (surfaces[i].stop) { stop = int(i); break; }
    if (stop < 0) stop = 0;
    r.stopIndex = stop;

    // 縁光線: 無限遠物体なので平行入射 (y = EPD/2, u = 0)
    const std::vector<State> mar = trace(surfaces, 0.5 * epd, 0.0);

    // 主光線: 視野角で入る光線のうち **絞りの中心を通る** もの。
    // 近軸追跡は線形なので、適当な 1 本 (y=0, u=tanθ) に縁光線を足して
    // 絞りでの高さを 0 にすればよい (縁光線は u = 0 なので入射角は変わらない)。
    const double ubar = (fieldHalf_deg > 0.0 && fieldHalf_deg < 90.0)
                            ? std::tan(fieldHalf_deg * kPi / 180.0)
                            : 0.0;
    const std::vector<State> aux = trace(surfaces, 0.0, ubar);
    if (std::fabs(mar[std::size_t(stop)].y) < 1e-12) return r;  // 退化
    const double alpha = -aux[std::size_t(stop)].y / mar[std::size_t(stop)].y;

    // ラグランジュ不変量 H = n(ū·y − u·ȳ) — 第 1 面で評価すれば十分
    const std::size_t n = surfaces.size();
    std::vector<State> chief(n);
    for (std::size_t i = 0; i < n; ++i) {
        chief[i] = aux[i];
        chief[i].y    = aux[i].y    + alpha * mar[i].y;
        chief[i].uIn  = aux[i].uIn  + alpha * mar[i].uIn;
        chief[i].uOut = aux[i].uOut + alpha * mar[i].uOut;
    }
    const double H = mar[0].nIn * (chief[0].uIn * mar[0].y
                                   - mar[0].uIn * chief[0].y);
    r.lagrange = H;
    r.hasField = std::fabs(H) > 0.0;

    r.perSurface.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        const State &m = mar[i];
        const State &b = chief[i];
        const double A    = m.nIn * (m.y * m.c + m.uIn);
        const double Abar = b.nIn * (b.y * b.c + b.uIn);
        const double dun  = m.uOut / m.nOut - m.uIn / m.nIn;   // Δ(u/n)
        const double dn   = 1.0 / m.nOut - 1.0 / m.nIn;        // Δ(1/n)

        SurfaceTerms t;
        t.sI   = -A * A * m.y * dun;
        t.sII  = -A * Abar * m.y * dun;
        t.sIII = -Abar * Abar * m.y * dun;
        t.sIV  = -H * H * m.c * dn;
        // 歪曲は教科書形 S_V = −(Ā/A)(S_III + S_IV) だと A = 0 の面
        // (平行光が当たる平面など) で 0/0 になる。屈折不変量の恒等式
        //   Δ(u/n) = A(1/n'² − 1/n²) − y·c·Δ(1/n),  H = Ā·y − A·ȳ
        // を代入すると A が約分でき、**分母の無い等価な形**になる:
        //   S_V = Ā[ Ā²·y·(1/n'² − 1/n²) + c·Δ(1/n)·(A·ȳ² − 2·Ā·y·ȳ) ]
        // こちらを使う (全面で定義できる)。
        const double dn2 = 1.0 / (m.nOut * m.nOut) - 1.0 / (m.nIn * m.nIn);
        t.sV = Abar * (Abar * Abar * m.y * dn2
                       + m.c * dn * (A * b.y * b.y
                                     - 2.0 * Abar * m.y * b.y));
        r.perSurface[i] = t;
        r.sI   += t.sI;
        r.sII  += t.sII;
        r.sIII += t.sIII;
        r.sIV  += t.sIV;
        r.sV   += t.sV;
        r.petzvalSum += m.c * dn;
    }
    // ペッツバール像面半径: 1/ρ = n'_last · Σ c·Δ(1/n) (単レンズで ρ = −n·f)
    const double nLast = mar[n - 1].nOut;
    if (std::fabs(r.petzvalSum) > 1e-15) {
        r.petzvalRadius = 1.0 / (nLast * r.petzvalSum);
        r.hasPetzval = true;
    }

    r.valid = std::isfinite(r.sI) && std::isfinite(r.sII)
              && std::isfinite(r.sIII) && std::isfinite(r.sIV);
    return r;
}

Waves toWaves(const Result &r, double lambda_mm)
{
    Waves w;
    if (!r.valid || !(lambda_mm > 0.0)) return w;
    w.valid = true;
    w.spherical    = r.sI   / (8.0 * lambda_mm);
    w.coma         = r.sII  / (2.0 * lambda_mm);
    w.astigmatism  = r.sIII / (2.0 * lambda_mm);
    w.fieldCurv    = r.sIV  / (4.0 * lambda_mm);
    w.distortion   = r.sV   / (2.0 * lambda_mm);
    return w;
}

} // namespace seidel
} // namespace ofd
