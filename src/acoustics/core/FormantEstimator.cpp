// FormantEstimator.cpp — LPC フォルマント推定の実装。
//
// 出典:
//   LPC / Levinson-Durbin : Makhoul (1975) "Linear prediction: A tutorial
//                           review", Proc. IEEE 63(4).
//   フォルマント抽出       : Markel & Gray (1976) "Linear Prediction of
//                           Speech" — A(z) の根の角度から共鳴周波数、
//                           半径から帯域幅を得る標準手法。
//   Durand-Kerner 法      : Kerner (1966) / Durand (1960)。全根同時反復。
//
// 乱数・現在時刻は使用しない (再現性)。Durand-Kerner の初期値は
// 決定的な (0.4+0.9i)^k を用いる。
#include "FormantEstimator.h"

#include <algorithm>
#include <cmath>

namespace ofd {
namespace acoustics {

namespace {

const double kPi = 3.14159265358979323846;

double medianOf(std::vector<double> v) {
    if (v.empty()) return 0.0;
    const std::size_t mid = v.size() / 2;
    std::nth_element(v.begin(), v.begin() + mid, v.end());
    double m = v[mid];
    if (v.size() % 2 == 0) {
        std::nth_element(v.begin(), v.begin() + mid - 1, v.begin() + mid);
        m = 0.5 * (m + v[mid - 1]);
    }
    return m;
}

// 反エイリアス FIR (ハミング窓 sinc、線形位相・奇数タップ) を設計する。
// カットオフ cutoffHz は元 fs 基準。DC 利得 1 に正規化。
std::vector<double> designLowpassFir(double cutoffHz, double sampleRateHz,
                                     std::size_t taps) {
    if (taps % 2 == 0) ++taps; // 奇数タップ (整数群遅延)
    std::vector<double> h(taps);
    const double fc = cutoffHz / sampleRateHz; // 正規化カットオフ (0..0.5)
    const std::ptrdiff_t center = static_cast<std::ptrdiff_t>(taps / 2);
    double sum = 0.0;
    for (std::size_t k = 0; k < taps; ++k) {
        const double m =
            static_cast<double>(static_cast<std::ptrdiff_t>(k) - center);
        // sinc ローパス: 2fc·sinc(2fc·m)
        double v;
        if (std::fabs(m) < 1e-12) {
            v = 2.0 * fc;
        } else {
            v = std::sin(2.0 * kPi * fc * m) / (kPi * m);
        }
        // ハミング窓
        v *= 0.54 - 0.46 * std::cos(2.0 * kPi * static_cast<double>(k) /
                                    static_cast<double>(taps - 1));
        h[k] = v;
        sum += v;
    }
    if (sum != 0.0) {
        for (std::size_t k = 0; k < taps; ++k) h[k] /= sum; // DC 利得 1
    }
    return h;
}

// FIR フィルタ + 整数間引き。出力は群遅延補償済み (出力サンプル m は
// 元信号の時刻 m·factor に対応する)。端はゼロ詰め扱い。
std::vector<double> filterAndDecimate(ArrayView<const double> x,
                                      const std::vector<double> &h,
                                      std::size_t factor) {
    const std::size_t n = x.size();
    const std::size_t nd = n / factor;
    std::vector<double> y(nd, 0.0);
    if (factor == 1 && h.empty()) {
        for (std::size_t i = 0; i < n; ++i) y[i] = x[i];
        return y;
    }
    const std::ptrdiff_t center = static_cast<std::ptrdiff_t>(h.size() / 2);
    const std::ptrdiff_t sn = static_cast<std::ptrdiff_t>(n);
    for (std::size_t m = 0; m < nd; ++m) {
        const std::ptrdiff_t c = static_cast<std::ptrdiff_t>(m * factor);
        double acc = 0.0;
        for (std::size_t k = 0; k < h.size(); ++k) {
            const std::ptrdiff_t idx =
                c + static_cast<std::ptrdiff_t>(k) - center;
            if (idx >= 0 && idx < sn) acc += h[k] * x[static_cast<std::size_t>(idx)];
        }
        y[m] = acc;
    }
    return y;
}

// フォルマント候補 (周波数昇順に整列して使う)
struct PoleCandidate {
    double freqHz;
    double bandwidthHz;
};

bool poleCandidateLess(const PoleCandidate &a, const PoleCandidate &b) {
    return a.freqHz < b.freqHz;
}

} // namespace

// ── Levinson-Durbin 再帰 ──
bool levinsonDurbin(const std::vector<double> &autocorr, int order,
                    std::vector<double> &lpc, double &predictionError) {
    lpc.clear();
    predictionError = 0.0;
    if (order < 1 ||
        autocorr.size() < static_cast<std::size_t>(order) + 1)
        return false;
    if (!(autocorr[0] > 0.0)) return false;

    std::vector<double> a(static_cast<std::size_t>(order) + 1, 0.0);
    std::vector<double> prev(a);
    a[0] = 1.0;
    double err = autocorr[0];
    for (int i = 1; i <= order; ++i) {
        double acc = autocorr[static_cast<std::size_t>(i)];
        for (int j = 1; j < i; ++j)
            acc += a[static_cast<std::size_t>(j)] *
                   autocorr[static_cast<std::size_t>(i - j)];
        const double k = -acc / err;
        prev = a;
        for (int j = 1; j < i; ++j)
            a[static_cast<std::size_t>(j)] =
                prev[static_cast<std::size_t>(j)] +
                k * prev[static_cast<std::size_t>(i - j)];
        a[static_cast<std::size_t>(i)] = k;
        err *= (1.0 - k * k);
        if (!(err > 0.0) || !std::isfinite(err)) return false;
    }
    lpc = a;
    predictionError = err;
    return true;
}

// ── Durand-Kerner 法 ──
bool durandKernerRoots(const std::vector<double> &coeffs,
                       std::vector<std::complex<double> > &roots,
                       int maxIterations, double tolerance) {
    roots.clear();

    // 先頭の 0 係数を除去して実効次数を決める
    std::size_t lead = 0;
    while (lead < coeffs.size() && coeffs[lead] == 0.0) ++lead;
    if (coeffs.size() - lead < 2) return false; // 次数 0 以下
    const std::size_t n = coeffs.size() - lead - 1; // 次数

    // モニック化した係数 c[0..n] (c[0] = 1)
    std::vector<double> c(n + 1);
    for (std::size_t i = 0; i <= n; ++i) {
        c[i] = coeffs[lead + i] / coeffs[lead];
        if (!std::isfinite(c[i])) return false;
    }

    // 決定的初期値: (0.4 + 0.9i)^(k+1)。|0.4+0.9i| ≈ 0.985 で互いに重ならず、
    // 実軸上にも乗らない (Kerner 1966 で慣用の初期配置)。
    std::vector<std::complex<double> > z(n);
    const std::complex<double> seed(0.4, 0.9);
    std::complex<double> pw(1.0, 0.0);
    for (std::size_t k = 0; k < n; ++k) {
        pw *= seed;
        z[k] = pw;
    }

    // モニック多項式の Horner 評価
    struct Eval {
        static std::complex<double> poly(const std::vector<double> &c,
                                         const std::complex<double> &x) {
            std::complex<double> acc(c[0], 0.0);
            for (std::size_t i = 1; i < c.size(); ++i)
                acc = acc * x + std::complex<double>(c[i], 0.0);
            return acc;
        }
    };

    bool converged = false;
    for (int it = 0; it < maxIterations; ++it) {
        double maxUpdate = 0.0;
        for (std::size_t k = 0; k < n; ++k) {
            std::complex<double> den(1.0, 0.0);
            for (std::size_t j = 0; j < n; ++j) {
                if (j == k) continue;
                den *= (z[k] - z[j]);
            }
            if (std::abs(den) < 1e-300)
                den = std::complex<double>(1e-300, 0.0);
            const std::complex<double> delta = Eval::poly(c, z[k]) / den;
            z[k] -= delta;
            const double upd = std::abs(delta);
            if (upd > maxUpdate) maxUpdate = upd;
        }
        if (maxUpdate < tolerance) {
            converged = true;
            break;
        }
    }

    if (!converged) {
        // 重根では更新量が tolerance まで縮まないことがある。
        // 残差が係数スケールに対して十分小さければ収束扱いにする。
        double scale = 0.0;
        for (std::size_t i = 0; i <= n; ++i) scale += std::fabs(c[i]);
        for (std::size_t k = 0; k < n; ++k) {
            const double zAbs = std::abs(z[k]);
            double pw2 = 1.0;
            for (std::size_t i = 0; i < n; ++i)
                pw2 *= (zAbs > 1.0) ? zAbs : 1.0;
            const double residual = std::abs(Eval::poly(c, z[k]));
            if (!(residual <= 1e-6 * scale * pw2)) return false;
        }
    }
    for (std::size_t k = 0; k < n; ++k) {
        if (!std::isfinite(z[k].real()) || !std::isfinite(z[k].imag()))
            return false;
    }
    roots = z;
    return true;
}

// ── フォルマント推定本体 ──
FormantEstimator::FormantEstimator(const FormantEstimatorConfig &config)
    : m_config(config) {}

FormantTrackResult
FormantEstimator::estimate(ArrayView<const double> x, double sampleRateHz,
                           const std::vector<unsigned char> &voicedFlags,
                           std::size_t frameLength,
                           std::size_t hopLength) const {
    FormantTrackResult res;
    if (x.empty() || !(sampleRateHz > 0.0) || frameLength < 4 ||
        hopLength < 1) {
        res.warning = "フォルマント推定の入力が不正です";
        res.f1MedianHz = makeInvalidMetric(res.warning);
        res.f2MedianHz = makeInvalidMetric(res.warning);
        res.f3MedianHz = makeInvalidMetric(res.warning);
        return res;
    }

    // ── 整数間引き率と内部 fs ──
    std::size_t factor = static_cast<std::size_t>(
        sampleRateHz / m_config.targetInternalRateHz + 0.5);
    if (factor < 1) factor = 1;
    const double fsInt = sampleRateHz / static_cast<double>(factor);
    res.decimationFactor = factor;
    res.internalRateHz = fsInt;

    // ── LPC 次数: p = 2 + round(内部 fs / 1000)、上限 maxLpcOrder ──
    int p = 2 + static_cast<int>(fsInt / 1000.0 + 0.5);
    if (p < 4) p = 4;
    if (p > m_config.maxLpcOrder) p = m_config.maxLpcOrder;
    res.lpcOrder = p;

    // ── 反エイリアス FIR + 間引き (factor == 1 なら素通し) ──
    std::vector<double> xd;
    if (factor == 1) {
        xd = filterAndDecimate(x, std::vector<double>(), 1);
    } else {
        // カットオフは内部ナイキストの 90% (0.45·内部fs)。タップ数は
        // 間引き率に比例させ遷移帯域を確保する (10·factor + 1)。
        const std::vector<double> h =
            designLowpassFir(0.45 * fsInt, sampleRateHz, 10 * factor + 1);
        xd = filterAndDecimate(x, h, factor);
    }

    const std::size_t frameInt = frameLength / factor; // 内部 fs でのフレーム長
    res.frames.assign(voicedFlags.size(), FormantFrame());

    std::vector<double> f1s, f2s, f3s;
    std::size_t voicedCount = 0;

    std::vector<double> work;   // プリエンファシス + 窓済みフレーム
    std::vector<double> r;      // 自己相関
    std::vector<double> lpc;    // LPC 係数
    std::vector<std::complex<double> > roots;

    for (std::size_t i = 0; i < voicedFlags.size(); ++i) {
        FormantFrame &fr = res.frames[i];
        const std::size_t start = i * hopLength;
        fr.timeSeconds = (static_cast<double>(start) +
                          0.5 * static_cast<double>(frameLength)) /
                         sampleRateHz;
        fr.voiced = (voicedFlags[i] != 0);
        if (!fr.voiced) continue;
        ++voicedCount;

        const std::size_t startInt = start / factor;
        std::size_t len = frameInt;
        if (startInt >= xd.size()) continue;
        if (startInt + len > xd.size()) len = xd.size() - startInt;
        if (len < static_cast<std::size_t>(2 * (p + 1))) continue;

        // プリエンファシス (0.97) + ハミング窓
        work.resize(len);
        for (std::size_t j = 0; j < len; ++j) {
            const double prev = (startInt + j > 0) ? xd[startInt + j - 1] : 0.0;
            const double s = xd[startInt + j] - m_config.preEmphasis * prev;
            const double w =
                0.54 - 0.46 * std::cos(2.0 * kPi * static_cast<double>(j) /
                                       static_cast<double>(len - 1));
            work[j] = s * w;
        }

        // 自己相関 r[0..p]
        r.assign(static_cast<std::size_t>(p) + 1, 0.0);
        for (int tau = 0; tau <= p; ++tau) {
            double acc = 0.0;
            for (std::size_t j = static_cast<std::size_t>(tau); j < len; ++j)
                acc += work[j] * work[j - static_cast<std::size_t>(tau)];
            r[static_cast<std::size_t>(tau)] = acc;
        }
        if (!(r[0] > 0.0)) continue;

        // Levinson-Durbin → A(z) の根
        double err = 0.0;
        if (!levinsonDurbin(r, p, lpc, err)) continue;
        if (!durandKernerRoots(lpc, roots)) continue;

        // 上半平面の極から候補 (F ≥ min、0 < B ≤ max) を集めて昇順に F1..F3
        std::vector<PoleCandidate> cands;
        for (std::size_t k = 0; k < roots.size(); ++k) {
            const std::complex<double> &zk = roots[k];
            if (!(zk.imag() > 1e-9)) continue; // 上半平面 (共役対の片側) のみ
            const double mag = std::abs(zk);
            if (!(mag > 0.0) || mag >= 1.0) continue; // 安定極のみ
            const double freq =
                std::atan2(zk.imag(), zk.real()) * fsInt / (2.0 * kPi);
            const double bw = -std::log(mag) * fsInt / kPi;
            if (freq < m_config.minFormantHz) continue;
            if (bw > m_config.maxBandwidthHz) continue;
            PoleCandidate pc;
            pc.freqHz = freq;
            pc.bandwidthHz = bw;
            cands.push_back(pc);
        }
        if (cands.empty()) continue;
        std::sort(cands.begin(), cands.end(), poleCandidateLess);

        fr.valid = true;
        fr.f1Hz = cands[0].freqHz;
        fr.b1Hz = cands[0].bandwidthHz;
        f1s.push_back(fr.f1Hz);
        if (cands.size() >= 2) {
            fr.f2Hz = cands[1].freqHz;
            fr.b2Hz = cands[1].bandwidthHz;
            f2s.push_back(fr.f2Hz);
        }
        if (cands.size() >= 3) {
            fr.f3Hz = cands[2].freqHz;
            fr.b3Hz = cands[2].bandwidthHz;
            f3s.push_back(fr.f3Hz);
        }
    }

    // ── 代表値: 時間中央値 ──
    if (voicedCount == 0) {
        res.warning = "有声フレームがないためフォルマントを推定できません";
        res.f1MedianHz = makeInvalidMetric(res.warning);
        res.f2MedianHz = makeInvalidMetric(res.warning);
        res.f3MedianHz = makeInvalidMetric(res.warning);
        return res;
    }

    const std::string noneWhy =
        "フォルマント候補が得られたフレームがありません";
    const std::string fewWhy =
        "フォルマントが得られた有声フレームが 50% 未満です";
    const double half = 0.5 * static_cast<double>(voicedCount);

    if (f1s.empty()) {
        res.f1MedianHz = makeInvalidMetric(noneWhy);
        res.warning = noneWhy;
    } else if (static_cast<double>(f1s.size()) < half) {
        res.f1MedianHz = makeWarningMetric(medianOf(f1s), fewWhy);
    } else {
        res.f1MedianHz = makeValidMetric(medianOf(f1s));
    }
    if (f2s.empty()) {
        res.f2MedianHz = makeInvalidMetric(noneWhy);
    } else if (static_cast<double>(f2s.size()) < half) {
        res.f2MedianHz = makeWarningMetric(medianOf(f2s), fewWhy);
    } else {
        res.f2MedianHz = makeValidMetric(medianOf(f2s));
    }
    if (f3s.empty()) {
        res.f3MedianHz = makeInvalidMetric(noneWhy);
    } else if (static_cast<double>(f3s.size()) < half) {
        res.f3MedianHz = makeWarningMetric(medianOf(f3s), fewWhy);
    } else {
        res.f3MedianHz = makeValidMetric(medianOf(f3s));
    }
    return res;
}

} // namespace acoustics
} // namespace ofd
