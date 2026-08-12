// EyeDiagram.cpp
#include "EyeDiagram.h"

#include <algorithm>
#include <cmath>

#include "../acoustics/core/Fft.h"

namespace ofd {
namespace eye {

namespace {

// 原始多項式のタップ (x^n + x^k + 1 の形が取れる次数を使う)。
// 数を写し間違えると周期が 2^n − 1 より短くなるので、selftest が
// **周期を実際に数えて**判定している。
int secondTap(int order)
{
    switch (order) {
    case 2:  return 1;
    case 3:  return 2;
    case 4:  return 3;
    case 5:  return 3;
    case 6:  return 5;
    case 7:  return 6;
    case 9:  return 5;
    case 10: return 7;
    case 11: return 9;
    case 15: return 14;
    default: return 0;   // 2 タップでは組めない次数 (8/12/13/14/16)
    }
}

// 4 タップが要る次数 (x^n + x^a + x^b + x^c + 1)
bool fourTaps(int order, int *a, int *b, int *c)
{
    switch (order) {
    case 8:  *a = 6; *b = 5; *c = 4; return true;
    case 12: *a = 6; *b = 4; *c = 1; return true;
    case 13: *a = 4; *b = 3; *c = 1; return true;
    case 14: *a = 5; *b = 3; *c = 1; return true;
    case 16: *a = 5; *b = 3; *c = 2; return true;
    default: return false;
    }
}

} // namespace

std::vector<int> prbs(int order)
{
    std::vector<int> out;
    if (order < 2 || order > 16) return out;
    const unsigned mask = (1u << order) - 1u;
    unsigned reg = mask;                 // 全 1 から始める (0 は禁制)
    const unsigned period = mask;        // 2^n − 1
    out.reserve(period);
    const int t2 = secondTap(order);
    int a = 0, b = 0, c = 0;
    const bool four = fourTaps(order, &a, &b, &c);
    for (unsigned i = 0; i < period; ++i) {
        // 出力は最下位ビット
        out.push_back(static_cast<int>(reg & 1u));
        unsigned fb;
        if (four) {
            fb = ((reg >> (order - 1)) ^ (reg >> (a - 1)) ^ (reg >> (b - 1))
                  ^ (reg >> (c - 1))) & 1u;
        } else {
            fb = ((reg >> (order - 1)) ^ (reg >> (t2 - 1))) & 1u;
        }
        reg = ((reg << 1) | fb) & mask;
    }
    return out;
}

std::vector<double> transmit(const Config &c, const std::vector<int> &bits)
{
    std::vector<double> x;
    if (!c.valid() || bits.empty()) return x;
    const std::size_t spb = static_cast<std::size_t>(c.samplesPerBit);
    const double dt = 1.0 / (c.bitRate_bps * c.samplesPerBit);
    // 遷移はビット境界を中心にした直線 (0 なら矩形)。遷移時間が 1 UI を
    // 超えると意味を成さないので UI で頭打ちにする。
    const double tr = std::min(c.riseTime_s, 1.0 / c.bitRate_bps);
    x.assign(bits.size() * spb, 0.0);
    const double A = c.amplitude_V;
    for (std::size_t i = 0; i < bits.size(); ++i) {
        const double lvl  = bits[i] ? A : -A;
        const double prev = bits[(i + bits.size() - 1) % bits.size()] ? A : -A;
        for (std::size_t k = 0; k < spb; ++k) {
            const double t = static_cast<double>(k) * dt;   // ビート内の時刻
            double v = lvl;
            if (tr > 0.0 && t < 0.5 * tr && prev != lvl) {
                // 境界の前半分 (後半分は前のビットの末尾で処理済み)
                const double u = 0.5 + t / tr;              // 0.5 → 1
                v = prev + (lvl - prev) * u;
            }
            if (tr > 0.0) {
                const double tEnd = static_cast<double>(spb - k) * dt;
                const double next =
                    bits[(i + 1) % bits.size()] ? A : -A;
                if (tEnd <= 0.5 * tr && next != lvl) {
                    const double u = 0.5 * (1.0 - tEnd / (0.5 * tr));  // 0 → 0.5
                    v = lvl + (next - lvl) * u;
                }
            }
            x[i * spb + k] = v;
        }
    }
    return x;
}

std::vector<double> impulseResponse(const Transfer &H, double fs_Hz,
                                    int minLength, double *tailOut)
{
    std::vector<double> h;
    if (fs_Hz <= 0.0 || minLength < 2) return h;
    const std::size_t M =
        acoustics::nextPowerOfTwo(static_cast<std::size_t>(minLength));
    std::vector<std::complex<double>> Hk(M, std::complex<double>(0.0, 0.0));
    const double df = fs_Hz / static_cast<double>(M);

    if (!H) {                       // 理想チャネル
        Hk[0] = std::complex<double>(1.0, 0.0);
        for (std::size_t k = 1; k < M; ++k) Hk[k] = Hk[0];
    } else {
        // 直流は実数でなければ h が実にならない。**f = 0 をそのまま評価する**
        // (最小周波数での値で代用すると h 全体に一定の誤差が乗る — 純遅延で
        // 単位標本にならなくなる)。0 で定義できないチャネルは呼び手が極限を
        // 決める約束。有限でなければ最小周波数へ退避する。
        {
            const std::complex<double> h0 = H(0.0);
            Hk[0] = std::isfinite(h0.real())
                        ? std::complex<double>(h0.real(), 0.0)
                        : std::complex<double>(H(df).real(), 0.0);
        }
        const std::size_t half = M / 2;
        for (std::size_t k = 1; k <= half; ++k) {
            const std::complex<double> v = H(static_cast<double>(k) * df);
            Hk[k] = v;
            if (k != half && k != 0) Hk[M - k] = std::conj(v);
        }
        // ナイキストも実数に (エルミート対称の要請)
        Hk[half] = std::complex<double>(Hk[half].real(), 0.0);
    }

    if (!acoustics::fftInverse(Hk)) return h;
    h.resize(M);
    for (std::size_t i = 0; i < M; ++i) h[i] = Hk[i].real();

    // 打ち切りの目安: 後半 1/4 に残っている電力の割合
    if (tailOut) {
        double tot = 0.0, tail = 0.0;
        for (std::size_t i = 0; i < M; ++i) {
            const double e = h[i] * h[i];
            tot += e;
            if (i >= 3 * M / 4) tail += e;
        }
        *tailOut = (tot > 0.0) ? tail / tot : 0.0;
    }
    return h;
}

std::vector<double> convolveCyclic(const std::vector<double> &x,
                                   const std::vector<double> &h)
{
    std::vector<double> y;
    if (x.empty() || h.empty()) return y;
    const std::size_t N = x.size();
    y.assign(N, 0.0);
    // y[n] = Σ_m h[m]·x[(n − m) mod N] — x が周期 N の周期信号なので厳密。
    for (std::size_t n = 0; n < N; ++n) {
        double s = 0.0;
        for (std::size_t m = 0; m < h.size(); ++m) {
            const std::size_t idx = (n + N - (m % N)) % N;
            s += h[m] * x[idx];
        }
        y[n] = s;
    }
    return y;
}

Result build(const Config &c, const Transfer &H)
{
    Result r;
    if (!c.valid()) return r;
    const std::vector<int> bits = prbs(c.prbsOrder);
    if (bits.size() < 2) return r;
    const std::vector<double> x = transmit(c, bits);
    if (x.empty()) return r;

    const double fs = c.bitRate_bps * c.samplesPerBit;
    r.dt_s = 1.0 / fs;
    r.samplesPerBit = c.samplesPerBit;

    // 標本化がチャネルを解像できているか (ナイキストでの応答の残り)
    if (H) {
        const std::complex<double> h0 = H(0.0);
        const std::complex<double> hn = H(0.5 * fs);
        const double d = std::abs(h0);
        r.nyquistMag = (std::isfinite(d) && d > 0.0) ? std::abs(hn) / d
                                                     : std::abs(hn);
    }

    const std::vector<double> h =
        impulseResponse(H, fs, c.impulseSamples, &r.tailFraction);
    if (h.empty()) return r;
    const std::vector<double> y = convolveCyclic(x, h);
    if (y.empty()) return r;

    // ── 判定時刻をチャネルの遅延に合わせる ───────────────────────────────
    // 線路は伝搬遅延を持つので、送信ビット i は受信側では遅れて現れる。
    // 遅延を無視して「ビット i の中央」で判定すると**別のビットを見て**
    // しまい、絵は開いているのに開口が負になる (実際に踏んだ)。
    //   1) 主到達を h の最大値の位置で拾う (整数ビット分のずれを吸収)
    //   2) 残りの 1 UI 以内の位置は**開口が最大になる点**を選ぶ
    //      (受信器が最良の位置で判定するのと同じ — アイ高さは普通この
    //       最適判定点での値を指す)
    const std::size_t spb = static_cast<std::size_t>(c.samplesPerBit);
    const std::size_t N = y.size();
    std::size_t peak = 0;
    for (std::size_t m = 1; m < h.size(); ++m)
        if (std::fabs(h[m]) > std::fabs(h[peak])) peak = m;

    const auto opening = [&](std::size_t sample) {
        double lo = 0.0, hi = 0.0;
        bool f1 = true, f0 = true;
        for (std::size_t i = 0; i < bits.size(); ++i) {
            const double v = y[(i * spb + sample) % N];
            if (bits[i]) { lo = f1 ? v : std::min(lo, v); f1 = false; }
            else         { hi = f0 ? v : std::max(hi, v); f0 = false; }
        }
        return lo - hi;
    };
    std::size_t best = peak + spb / 2;
    double bestOpen = opening(best);
    for (std::size_t o = 0; o < spb; ++o) {
        const std::size_t cand = peak + o;
        const double v = opening(cand);
        if (v > bestOpen) { bestOpen = v; best = cand; }
    }
    r.sampleIndex = best;

    // 判定時刻が窓の中央に来るように 2 UI 分を折り返す。こうすると両側の
    // 遷移が窓の内側に入るので、開口幅を「交差から交差まで」で測れる。
    const std::size_t span = 2 * spb;
    r.traces.reserve(bits.size());
    for (std::size_t i = 0; i < bits.size(); ++i) {
        std::vector<double> tr(span);
        for (std::size_t k = 0; k < span; ++k)
            tr[k] = y[(i * spb + best + N * 2 - spb + k) % N];
        r.traces.push_back(std::move(tr));
    }

    const std::size_t mid = spb;      // 窓の中央 = 判定時刻
    double lo1 = 0.0, hi0 = 0.0;      // 1 の最小値 / 0 の最大値
    bool first1 = true, first0 = true;
    for (std::size_t i = 0; i < bits.size(); ++i) {
        const double v = r.traces[i][mid];
        if (bits[i]) { lo1 = first1 ? v : std::min(lo1, v); first1 = false; }
        else         { hi0 = first0 ? v : std::max(hi0, v); first0 = false; }
    }
    r.height_V = lo1 - hi0;          // 閉じていれば負になる (止めない)

    // 交差時刻: 各トレースが 0 V を横切る時刻を線形補間で拾い、
    // 中央の判定時刻より前の最も遅い交差と、後の最も早い交差を採る。
    double lastRise = -1.0, firstFall = -1.0;
    double crossMin = 0.0, crossMax = 0.0;
    bool anyCross = false;
    for (const std::vector<double> &tr : r.traces) {
        for (std::size_t k = 1; k < tr.size(); ++k) {
            const double a = tr[k - 1], b = tr[k];
            if ((a < 0.0 && b >= 0.0) || (a >= 0.0 && b < 0.0)) {
                const double frac = (b != a) ? (0.0 - a) / (b - a) : 0.0;
                const double t = (static_cast<double>(k - 1) + frac) * r.dt_s;
                const double tc = static_cast<double>(mid) * r.dt_s;  // 中央
                if (t <= tc) lastRise = std::max(lastRise, t);
                else if (firstFall < 0.0 || t < firstFall) firstFall = t;
                if (!anyCross) { crossMin = crossMax = t; anyCross = true; }
                else { crossMin = std::min(crossMin, t);
                       crossMax = std::max(crossMax, t); }
            }
        }
    }
    if (lastRise >= 0.0 && firstFall > lastRise) r.width_s = firstFall - lastRise;
    // ジッタは「中央より前の交差」のばらつきで測る (前後を混ぜない)
    if (anyCross) {
        double jMin = 0.0, jMax = 0.0;
        bool any = false;
        const double tc = static_cast<double>(mid) * r.dt_s;
        for (const std::vector<double> &tr : r.traces) {
            for (std::size_t k = 1; k < tr.size(); ++k) {
                const double a = tr[k - 1], b = tr[k];
                if ((a < 0.0 && b >= 0.0) || (a >= 0.0 && b < 0.0)) {
                    const double frac = (b != a) ? (0.0 - a) / (b - a) : 0.0;
                    const double t = (static_cast<double>(k - 1) + frac) * r.dt_s;
                    if (t > tc) continue;
                    if (!any) { jMin = jMax = t; any = true; }
                    else { jMin = std::min(jMin, t); jMax = std::max(jMax, t); }
                }
            }
        }
        r.jitter_s = any ? (jMax - jMin) : 0.0;
    }
    return r;
}

} // namespace eye
} // namespace ofd
