// test_array.cpp — ラインアレイ / サブアレイの指向性 (ArrayDirectivity)。
// 判定はすべて解析解: 一様直線アレイの 3 dB 幅 0.886·λ/(N·d)、第一サイド
// ローブ −13.26 dB、ステアリング方向、グレーティングローブの出現周波数
// c/(d(1+|sinθs|))、カージオイドサブの後方打消しと前方 +6 dB。
#include "../../src/acoustics/core/ArrayDirectivity.h"
#include "test_common.h"

#include <cmath>
#include <vector>

using namespace ofd::acoustics;

namespace {
const double kC = 343.0;
const double kPi = 3.14159265358979323846;
} // namespace

int main()
{
    // ── 1) ブロードサイドの一様直線アレイ ────────────────────────────────
    // 素子間隔 d = λ/2 (グレーティングローブが出ない上限) で、
    // 3 dB 幅は 0.886·λ/(N·d) [rad]。N を増やすほど式に近づく。
    {
        const double d = 0.35;
        const double f = kC / (2.0 * d);          // d = λ/2
        const double lambda = kC / f;
        const struct { int n; double tol; } cases[] = { { 8, 0.15 },
                                                        { 16, 0.02 },
                                                        { 32, 0.01 } };
        for (int i = 0; i < 3; ++i) {
            const int N = cases[i].n;
            const std::vector<ArrayElement> els =
                buildLineArray(N, d, std::vector<double>(), 0.0, kC);
            CHECK(int(els.size()) == N);
            const ArrayPattern p =
                beamPattern(els, f, kC, 0.0, -90.0, 90.0, 3601);
            CHECK(p.valid);
            CHECK_NEAR(p.peakDeg, 0.0, 1e-9);     // 正面
            const BeamMetrics m = beamMetrics(p);
            CHECK(m.hasHpbw);
            const double expect = 0.886 * lambda / (N * d) * 180.0 / kPi;
            CHECK_REL(m.hpbwDeg, expect, cases[i].tol);
            // 第一サイドローブは N が大きいほど −13.26 dB へ寄る
            CHECK(m.hasSll);
            CHECK(m.sllDb < -12.5 && m.sllDb > -13.5);
        }
        // N = 32 では教科書値に 0.2 dB まで寄る
        const ArrayPattern p32 =
            beamPattern(buildLineArray(32, d, std::vector<double>(), 0.0, kC),
                        f, kC, 0.0, -90.0, 90.0, 3601);
        CHECK_NEAR(beamMetrics(p32).sllDb, -13.26, 0.2);
    }

    // ── 2) ステアリング: 遅延だけでビームが指定角へ向く ──────────────────
    {
        const double d = 0.35, f = kC / (2.0 * d);
        const double angles[3] = { 10.0, 20.0, -15.0 };
        for (int i = 0; i < 3; ++i) {
            const ArrayPattern p =
                beamPattern(buildLineArray(12, d, std::vector<double>(),
                                           angles[i], kC),
                            f, kC, 0.0, -90.0, 90.0, 3601);
            CHECK(p.valid);
            CHECK_NEAR(p.peakDeg, angles[i], 0.1);
        }
        // 遅延は非負 (負の遅延は実現できない — 最小が 0 になるよう平行移動)
        const std::vector<ArrayElement> els =
            buildLineArray(12, d, std::vector<double>(), 20.0, kC);
        double minDelay = els[0].delay_s;
        for (std::size_t i = 0; i < els.size(); ++i)
            if (els[i].delay_s < minDelay) minDelay = els[i].delay_s;
        CHECK_NEAR(minDelay, 0.0, 1e-15);
    }

    // ── 3) グレーティングローブ ──────────────────────────────────────────
    // d = λ (= 出現周波数) ちょうどで、±90° に主極大と同じ高さのローブが立つ。
    {
        const double d = 0.35;
        const double fg = gratingLobeFreq(d, 0.0, kC);
        CHECK_REL(fg, kC / d, 1e-12);
        const ArrayPattern p =
            beamPattern(buildLineArray(8, d, std::vector<double>(), 0.0, kC),
                        fg, kC, 0.0, -90.0, 90.0, 3601);
        CHECK(p.valid);
        CHECK_NEAR(p.db.front(), 0.0, 1e-6);      // −90°
        CHECK_NEAR(p.db.back(), 0.0, 1e-6);       // +90°
        // その半分の周波数ならローブは 1 本だけ (端は十分低い)
        const ArrayPattern q =
            beamPattern(buildLineArray(8, d, std::vector<double>(), 0.0, kC),
                        0.5 * fg, kC, 0.0, -90.0, 90.0, 3601);
        CHECK(q.db.front() < -10.0 && q.db.back() < -10.0);
        // ステアリングすると出現周波数は下がる (θs = 30° で 2/3 倍)
        CHECK_REL(gratingLobeFreq(d, 30.0, kC), kC / (d * 1.5), 1e-12);
    }

    // ── 4) splay (J カーブ) はカバー範囲を広げる ─────────────────────────
    // 主ローブの −3 dB 幅ではなく、−6 dB が及ぶ角度範囲で見る
    // (J カーブでも主ローブ自体は細いまま — ここを取り違えない)。
    {
        const double d = 0.35, f = 1000.0;
        std::vector<double> splay;
        const double sp[12] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 14 };
        for (int i = 0; i < 12; ++i) splay.push_back(sp[i]);
        const ArrayPattern ps =
            beamPattern(buildLineArray(12, d, std::vector<double>(), 0.0, kC),
                        f, kC, d, -90.0, 90.0, 3601);
        const ArrayPattern pj =
            beamPattern(buildLineArray(12, d, splay, 0.0, kC),
                        f, kC, d, -90.0, 90.0, 3601);
        const BeamMetrics ms = beamMetrics(ps), mj = beamMetrics(pj);
        CHECK(ms.hasCoverage && mj.hasCoverage);
        const double spanS = ms.coverageMaxDeg - ms.coverageMinDeg;
        const double spanJ = mj.coverageMaxDeg - mj.coverageMinDeg;
        CHECK(spanJ > 3.0 * spanS);
        // splay は下向きなので、カバーは下 (正の角度) へ伸びる
        CHECK(mj.coverageMaxDeg > ms.coverageMaxDeg + 10.0);
    }

    // ── 5) カージオイドサブ (逆相 + τ = d/c) ─────────────────────────────
    {
        const double tau = 3.5e-3;
        const double d = kC * tau;                 // 遅延に見合う間隔
        const double fBest = kC / (4.0 * d);       // d = λ/4
        const EndfireResult r = endfire(d, tau, fBest, kC, true);
        CHECK(r.valid);
        CHECK_NEAR(r.frontDb, 6.0206, 1e-3);       // 2 倍 = +6.02 dB
        CHECK(r.backDb < -100.0);                  // 完全な打消し
        CHECK_NEAR(r.optimumDelay_s, tau, 1e-12);
        CHECK_NEAR(r.bestFreqHz, fBest, 1e-9);
        // 後方の打消しは周波数に依らない (これがカージオイドの利点)
        const double fs[4] = { 30.0, 50.0, 80.0, 120.0 };
        for (int i = 0; i < 4; ++i)
            CHECK(endfire(d, tau, fs[i], kC, true).backDb < -100.0);
        // 遅延が半分だと打ち消しきれない
        CHECK(endfire(d, 0.5 * tau, fBest, kC, true).backDb > -10.0);
    }

    // ── 6) 同相 (逆相なし) で遅延 0 なら前後対称 — 指向性は生まれない ────
    {
        const double d = 1.2;
        const EndfireResult r = endfire(d, 0.0, 60.0, kC, false);
        CHECK(r.valid);
        CHECK_NEAR(r.frontBackDb, 0.0, 1e-12);
        // 同相では「全周波数で後方を消す遅延」は存在しない
        CHECK_NEAR(r.optimumDelay_s, 0.0, 1e-15);
    }

    // ── 7) 壊れた入力からは何も作らない ──────────────────────────────────
    {
        CHECK(!beamPattern(std::vector<ArrayElement>(), 1000.0, kC, 0.0).valid);
        CHECK(buildLineArray(0, 0.35, std::vector<double>(), 0.0, kC).empty());
        CHECK(buildLineArray(8, 0.0, std::vector<double>(), 0.0, kC).empty());
        const std::vector<ArrayElement> ok =
            buildLineArray(8, 0.35, std::vector<double>(), 0.0, kC);
        CHECK(!beamPattern(ok, 0.0, kC, 0.0).valid);        // f = 0
        CHECK(!beamPattern(ok, 1000.0, 0.0, 0.0).valid);    // c = 0
        CHECK_NEAR(gratingLobeFreq(0.0, 0.0, kC), 0.0, 1e-15);
        CHECK(!endfire(1.2, 3.5e-3, 0.0, kC, true).valid);
        CHECK(!beamMetrics(ArrayPattern()).hasHpbw);
    }

    return testutil::summary("test_array");
}
