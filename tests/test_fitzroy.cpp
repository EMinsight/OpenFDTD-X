// test_fitzroy.cpp — Fitzroy 残響式 (core/RoomAcoustics, rtFormula=2) の検証。
//
// 出典: D. Fitzroy, "Reverberation Formula Which Seems to Be More Accurate
// with Nonuniform Distribution of Absorption," J. Acoust. Soc. Am. 31(7),
// 893-897 (1959)。 T = 0.161·V/S² · Σᵢ Sᵢ/(−ln(1−ᾱᵢ))  (i = 直交3方向)
//
// 既知値 (10 m 立方体, V=1000 / S=600):
//   床+天井 α=0.8 (200 m²)・他 4 面 α=0.1 (400 m²)
//   → T = 4.472222e-4 × (124.267 + 2×1898.245) = 1.7534 s (許容 1e-3)
// 性質:
//   一様吸音では Eyring と 1e-9 一致 / 非均一では Fitzroy > Eyring /
//   occupancy 係数 (客席=z) / Air 行は分母へ加算 / 無効行は無視 /
//   吸音ゼロ → 0 / 方向情報なし (Other のみ) は Eyring フォールバック
#include <cmath>
#include <cstdio>

#include "core/Project.h"
#include "core/RoomAcoustics.h"

using namespace ofd;
using namespace ofd::roomac;

static int g_checks = 0;
static int g_failures = 0;

static void check(bool cond, const char *what)
{
    ++g_checks;
    if (!cond) {
        ++g_failures;
        std::fprintf(stderr, "FAIL: %s\n", what);
    }
}

// 全 6 帯域を同じ α で埋めた 1 行 (方向は role で指定)
static AbsorptionRow faceRow(int role, double area, double alpha)
{
    AbsorptionRow r;
    r.role = role;
    r.area = area;
    for (double &al : r.alpha) al = alpha;
    return r;
}

static AbsorptionRow airRow(double airA)
{
    AbsorptionRow r;
    r.role = AbsorptionRow::Air;
    r.airA = airA;
    return r;
}

// 10 m 立方体 (V=1000, S=600) の器
static AcousticOpts makeCube()
{
    AcousticOpts a;
    a.roomL = a.roomW = a.roomH = 10;
    a.volume = 1000;
    a.surface = 600;
    a.occupancy = 2;
    a.absorption.clear();
    return a;
}

int main()
{
    // ── 1) 既知値: 床+天井 α=0.8 (200 m²)・他 4 面 α=0.1 (400 m²) ──────────
    {
        AcousticOpts a = makeCube();
        a.absorption = {
            faceRow(AbsorptionRow::Floor,    100, 0.8),
            faceRow(AbsorptionRow::Ceiling,  100, 0.8),
            faceRow(AbsorptionRow::SideWall, 200, 0.1),
            faceRow(AbsorptionRow::RearWall, 200, 0.1),
        };
        const double T = rt60(a, 3, 2);
        // 4.472222e-4 × (200/(−ln 0.2) + 2 × 200/(−ln 0.9))
        //   = 4.472222e-4 × (124.267 + 2×1898.245) = 1.7534 s
        std::printf("Fitzroy cube: T = %.6f s (expected 1.7534)\n", T);
        check(std::fabs(T - 1.7534) < 1e-3, "cube known value 1.7534 s");

        // 非均一吸音では Fitzroy > Eyring
        check(rt60(a, 3, 2) > rt60(a, 3, 1),
              "non-uniform: Fitzroy > Eyring");

        // 旧 0/1 の挙動は不変 (Sabine = 0.161·1000/200 = 0.805)
        check(std::fabs(rt60(a, 3, 0) - 0.805) < 1e-9, "Sabine unchanged");

        // 無効行は無視される: 高吸音行を disabled で足しても不変
        AcousticOpts b = a;
        AbsorptionRow off = faceRow(AbsorptionRow::Ceiling, 500, 0.99);
        off.enabled = false;
        b.absorption.push_back(off);
        check(rt60(b, 3, 2) == rt60(a, 3, 2), "disabled row ignored");
    }

    // ── 2) 一様吸音では Eyring と 1e-9 一致 ────────────────────────────────
    {
        AcousticOpts a = makeCube();
        a.absorption = {
            faceRow(AbsorptionRow::Floor,    100, 0.3),
            faceRow(AbsorptionRow::Ceiling,  100, 0.3),
            faceRow(AbsorptionRow::SideWall, 200, 0.3),
            faceRow(AbsorptionRow::RearWall, 200, 0.3),
        };
        const double tf = rt60(a, 3, 2);
        const double te = rt60(a, 3, 1);
        check(std::fabs(tf - te) <= 1e-9 * te,
              "uniform absorption matches Eyring within 1e-9");
    }

    // ── 3) occupancy 係数: 客席行は z 方向 + occupancyFactor ───────────────
    {
        AcousticOpts a = makeCube();
        a.absorption = {
            faceRow(AbsorptionRow::Audience, 100, 0.8),   // 床 = 客席
            faceRow(AbsorptionRow::Ceiling,  100, 0.8),
            faceRow(AbsorptionRow::SideWall, 200, 0.1),
            faceRow(AbsorptionRow::RearWall, 200, 0.1),
        };
        a.occupancy = 2;   // 満席 (係数 1.0)
        const double tFull = rt60(a, 3, 2);
        a.occupancy = 0;   // 空席 (係数 0.70) → 吸音減 → RT 延び
        const double tEmpty = rt60(a, 3, 2);
        check(tEmpty > tFull, "empty seats lengthen Fitzroy RT");

        // 満席では Audience は Floor と同値 (係数 1.0)
        AcousticOpts f = makeCube();
        f.absorption = a.absorption;
        f.absorption[0] = faceRow(AbsorptionRow::Floor, 100, 0.8);
        f.occupancy = 2;
        a.occupancy = 2;
        check(std::fabs(rt60(a, 3, 2) - rt60(f, 3, 2)) < 1e-12,
              "occupied audience equals floor row");
    }

    // ── 4) Air 行は分母へ加算 → RT 短縮 ────────────────────────────────────
    {
        AcousticOpts a = makeCube();
        a.absorption = {
            faceRow(AbsorptionRow::Floor,    100, 0.8),
            faceRow(AbsorptionRow::Ceiling,  100, 0.8),
            faceRow(AbsorptionRow::SideWall, 200, 0.1),
            faceRow(AbsorptionRow::RearWall, 200, 0.1),
        };
        const double dry = rt60(a, 3, 2);
        a.absorption.push_back(airRow(50));
        const double humid = rt60(a, 3, 2);
        check(humid < dry, "air absorption shortens Fitzroy RT");
    }

    // ── 5) 吸音ゼロ → 0 ────────────────────────────────────────────────────
    {
        AcousticOpts a = makeCube();
        a.absorption = {
            faceRow(AbsorptionRow::Floor,    100, 0.0),
            faceRow(AbsorptionRow::Ceiling,  100, 0.0),
            faceRow(AbsorptionRow::SideWall, 200, 0.0),
            faceRow(AbsorptionRow::RearWall, 200, 0.0),
        };
        check(rt60(a, 3, 2) == 0.0, "zero absorption returns 0");
    }

    // ── 6) 方向情報なし (Other のみ) → Eyring フォールバック ───────────────
    {
        AcousticOpts a = makeCube();
        a.absorption = { faceRow(AbsorptionRow::Other, 600, 0.3) };
        check(rt60(a, 3, 2) == rt60(a, 3, 1),
              "Other-only budget falls back to Eyring");

        // Other 行は面積比配分: 方向行 + Other の一様 α も Eyring と一致
        AcousticOpts b = makeCube();
        b.absorption = {
            faceRow(AbsorptionRow::Floor,    100, 0.3),
            faceRow(AbsorptionRow::Ceiling,  100, 0.3),
            faceRow(AbsorptionRow::SideWall, 200, 0.3),
            faceRow(AbsorptionRow::RearWall, 100, 0.3),
            faceRow(AbsorptionRow::Other,    100, 0.3),   // 面積比配分
        };
        const double tf = rt60(b, 3, 2);
        const double te = rt60(b, 3, 1);
        check(std::fabs(tf - te) <= 1e-9 * te,
              "Other rows distributed by area ratio (uniform check)");
    }

    std::printf("fitzroy: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
