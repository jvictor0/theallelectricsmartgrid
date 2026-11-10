#include "doctest.h"
#include "TimeWarpInverter.hpp"
#include <cmath>
#include <memory>
#include <random>
#include <vector>

DOCTEST_TEST_CASE("Phase inverse branches: two-sided knots prevent interpolation across a rewind")
{
    TimeWarpInverter<32> inverse;
    inverse.Write(0.0, 0.0);
    inverse.Write(1.0, 1.0);
    inverse.Write(0.5, 2.0);
    inverse.Write(1.5, 3.0);
    DOCTEST_CHECK(inverse.GetRealTime(0.49) == doctest::Approx(0.49));
    DOCTEST_CHECK(inverse.GetRealTime(0.5) == 2.0);
    DOCTEST_CHECK(inverse.GetRealTime(0.51) == doctest::Approx(2.01));
    DOCTEST_CHECK(inverse.GetRealTime(1.4) == doctest::Approx(2.9));
    DOCTEST_CHECK(inverse.GetRealTime(1.5) == 3.0);
    inverse.Write(0.5, 4.0);
    inverse.Write(0.75, 5.0);
    DOCTEST_CHECK(inverse.GetRealTime(0.49) == doctest::Approx(0.49));
    DOCTEST_CHECK(inverse.GetRealTime(0.5) == 4.0);
    DOCTEST_CHECK(inverse.GetRealTime(0.6) == doctest::Approx(4.4));
}

DOCTEST_TEST_CASE("Phase inverse branches: nested turns keep the minimum cut within a visit")
{
    for (double offset : {0.0, -1.0})
    {
        TimeWarpInverter<32> inverse;
        const double phase[] = {0, 1, .2, .8, .4, .9, .6, .95};
        for (size_t i = 0; i < std::size(phase); ++i)
        {
            inverse.Write(offset + phase[i], static_cast<double>(i));
        }

        DOCTEST_CHECK(inverse.m_islandReassignments == 0);
        DOCTEST_CHECK(inverse.GetRealTime(offset + .1) == doctest::Approx(.1));
        DOCTEST_CHECK(inverse.GetRealTime(offset + .2) == 2);
        for (double q : {.3, .4, .5, .6, .7, .95})
        {
            DOCTEST_CHECK(inverse.GetRealTime(offset + q) == doctest::Approx(2 + (q - .2) / .75 * 5));
        }

    }
}

DOCTEST_TEST_CASE("Phase inverse branches: exact-grid turn keeps the previous cell's old endpoint")
{
    TimeWarpInverter<32> inverse;
    inverse.Write(0, 0);
    inverse.Write(2, 2);
    inverse.Write(1, 3);
    inverse.Write(1.5, 4);
    DOCTEST_CHECK(inverse.GetRealTime(.99) == doctest::Approx(.99));
    DOCTEST_CHECK(inverse.GetRealTime(1) == 3);
    DOCTEST_CHECK(inverse.GetRealTime(1.25) == 3.5);
}

DOCTEST_TEST_CASE("Phase inverse branches: forward frontier is readable and never follows descending time")
{
    TimeWarpInverter<32> inverse;
    inverse.Write(.1, 100);
    inverse.Write(.3, 102);
    DOCTEST_CHECK(std::isnan(inverse.GetRealTime(.05)));
    DOCTEST_CHECK(inverse.GetRealTime(.2) == doctest::Approx(101));
    DOCTEST_CHECK(inverse.GetRealTime(.3) == 102);
    TimeWarpView view{&inverse, 102, .3, inverse.m_oldestWarpedTime};
    DOCTEST_CHECK(view.GetRealTime(.2) == doctest::Approx(101));
    inverse.Write(.2, 103);
    DOCTEST_CHECK(inverse.GetRealTime(.2) == doctest::Approx(101));
    DOCTEST_CHECK(std::isnan(inverse.ReadInverse(.2, 100.5)));
    inverse.Write(.25, 104);
    DOCTEST_CHECK(inverse.GetRealTime(.2) == doctest::Approx(100 + 4 * .1 / .15));
    DOCTEST_CHECK(inverse.GetRealTime(.225) == doctest::Approx(100 + 4 * .125 / .15));
}

DOCTEST_TEST_CASE("Phase inverse branches: partial negative startup completes its own fallback")
{
    TimeWarpInverter<32> inverse;
    static_assert(decltype(inverse)::x_payloadBytes == 32 * 32);
    inverse.Write(-.4, 100);
    inverse.Write(-.1, 103);
    DOCTEST_CHECK(std::isnan(inverse.GetRealTime(-.5)));
    DOCTEST_CHECK(inverse.GetRealTime(-.25) == doctest::Approx(101.5));
    DOCTEST_CHECK(inverse.GetRealTime(-.1) == 103);
    inverse.Write(0, 104);
    DOCTEST_CHECK(inverse.GetRealTime(-.25) == doctest::Approx(101.5));
}

DOCTEST_TEST_CASE("Phase inverse branches: relative times retain precision at a large absolute origin")
{
    constexpr double x_origin = 1e12;
    TimeWarpInverter<32> inverse;
    inverse.Write(0, x_origin);
    inverse.Write(1, x_origin + 1);
    inverse.Write(.2, x_origin + 10000);
    inverse.Write(.9, x_origin + 10007);
    DOCTEST_CHECK(std::abs(inverse.GetRealTime(.1) - (x_origin + .1)) < .001);
    DOCTEST_CHECK(std::abs(inverse.GetRealTime(.55) - (x_origin + 10003.5)) < .001);
}

DOCTEST_TEST_CASE("Phase inverse branches: thousands of nested turns use one replacement")
{
    TimeWarpInverter<16> inverse;
    inverse.Write(0, 0);
    inverse.Write(1, 1);
    for (size_t i = 1; i <= 2000; ++i)
    {
        double turn = static_cast<double>(i) / 4000;
        inverse.Write(turn, 2 * i);
        inverse.Write(.9, 2 * i + 1);
        DOCTEST_CHECK(inverse.GetRealTime(.000125) == doctest::Approx(.000125));
        DOCTEST_CHECK(inverse.GetRealTime(.00025) == 2);
        DOCTEST_CHECK(inverse.m_turnPhase[0].load() == .00025);
        DOCTEST_CHECK(std::isfinite(inverse.GetRealTime(.7)));
    }

    DOCTEST_CHECK(inverse.m_islandReassignments == 0);
    static_assert(decltype(inverse)::x_maxCellPieces == 2);
    static_assert(decltype(inverse)::x_payloadBytes == 16 * 32);
}

DOCTEST_TEST_CASE("Phase inverse branches: deeper minima replace the cut and equal minima keep their time")
{
    TimeWarpInverter<16> inverse;
    const double phase[] = {0, 1.1, .6, .8, .4, .7, .5, .9, .4, .8};
    for (size_t i = 0; i < std::size(phase); ++i)
    {
        inverse.Write(phase[i], static_cast<double>(i));
    }

    DOCTEST_CHECK(inverse.m_turnPhase[0].load() == .4);
    DOCTEST_CHECK(inverse.GetRealTime(.4) == 4);
    DOCTEST_CHECK(inverse.GetRealTime(.6) == doctest::Approx(6.5));
    DOCTEST_CHECK(inverse.GetRealTime(.3) == doctest::Approx(.3 / 1.1));
    DOCTEST_CHECK(std::isnan(inverse.ReadInverse(.6, 8)));
}

DOCTEST_TEST_CASE("Phase inverse branches: boundary touches coalesce but crossing outside ends the visit")
{
    TimeWarpInverter<16> inverse;
    const double phase[] = {0, 1.1, .25, 1, .5, .75, 1, .625, .875};
    for (size_t i = 0; i < std::size(phase); ++i)
    {
        inverse.Write(phase[i], static_cast<double>(i));
    }

    DOCTEST_CHECK(inverse.m_turnPhase[0].load() == .25);
    DOCTEST_CHECK(inverse.GetRealTime(.5) == doctest::Approx(4.4));
    DOCTEST_CHECK(std::isnan(inverse.m_currentRightTime[0].load()));
    inverse.Write(1.1, 9);
    inverse.Write(.75, 10);
    inverse.Write(.875, 11);
    DOCTEST_CHECK(inverse.m_turnPhase[0].load() == .75);
    DOCTEST_CHECK(inverse.GetRealTime(.5) == doctest::Approx(.5 / 1.1));
    DOCTEST_CHECK(inverse.GetRealTime(.75) == 10);
    inverse.Write(-.25, 12);
    inverse.Write(.5, 13);
    DOCTEST_CHECK(std::isnan(inverse.m_turnPhase[0].load()));
    DOCTEST_CHECK(inverse.GetRealTime(.25) == doctest::Approx(12 + 2.0 / 3));
}

DOCTEST_TEST_CASE("Phase inverse branches: within-cell wiggles retain the recent traversal of an old cell")
{
    TimeWarpInverter<16> inverse;
    inverse.Write(0, 48);
    inverse.Write(1.1, 48.001);
    inverse.Write(.9375, 99);
    inverse.Write(.953125, 99.001);
    inverse.Write(.96875, 99.002);
    inverse.Write(.953125, 99.003);
    inverse.Write(.96875, 99.004);
    DOCTEST_CHECK(inverse.GetRealTime(.9375) == 99);
    inverse.Write(1, 99.005);
    inverse.Write(.984375, 99.006);
    inverse.Write(1, 99.007);
    inverse.Write(1.1, 99.008);
    DOCTEST_CHECK(inverse.GetRealTime(.9375) == 99);
    DOCTEST_CHECK(inverse.GetRealTime(.95) > 99);
    DOCTEST_CHECK(inverse.GetRealTime(.9) < 49);
}

DOCTEST_TEST_CASE("Phase inverse branches: exiting again preserves the first full traversal")
{
    TimeWarpInverter<16> inverse;
    const double phase[] = {0, 1.1, .25, 1.1, .5, 1.1};
    for (size_t i = 0; i < std::size(phase); ++i)
    {
        inverse.Write(phase[i], static_cast<double>(i));
    }

    DOCTEST_CHECK(inverse.GetRealTime(.3) == doctest::Approx(.3 / 1.1));
    DOCTEST_CHECK(inverse.GetRealTime(.5) == 4);
    DOCTEST_CHECK(inverse.GetRealTime(.8) == doctest::Approx(4.5));
    DOCTEST_CHECK(std::isfinite(inverse.ReadInverse(.3, 1)));
    DOCTEST_CHECK(std::isnan(inverse.ReadInverse(.8, 4.5)));
}

DOCTEST_TEST_CASE("Phase inverse branches: partial entry rewinds and ring retirement")
{
    TimeWarpInverter<4> inverse;
    inverse.Write(.4, 0);
    inverse.Write(.7, 1);
    inverse.Write(.2, 2);
    inverse.Write(.6, 3);
    DOCTEST_CHECK(std::isnan(inverse.GetRealTime(.1)));
    DOCTEST_CHECK(inverse.GetRealTime(.4) == doctest::Approx(2.5));
    inverse.Write(-.5, 4);
    inverse.Write(-.1, 5);
    DOCTEST_CHECK(std::isnan(inverse.GetRealTime(.4)));
    inverse.Write(4.5, 6);
    DOCTEST_CHECK(std::isnan(inverse.m_seedPhase.load()));
    DOCTEST_CHECK(inverse.GetRealTime(4.25) == doctest::Approx(5 + 4.35 / 4.6));
}

DOCTEST_TEST_CASE("Phase inverse branches: returned times are causal with a preimage and one-cell phase error")
{
    auto inverse = std::make_unique<TimeWarpInverter<1024>>(4.0);
    std::mt19937 random(19);
    std::vector<double> phase{0};
    inverse->Write(0, 0);
    for (size_t n = 1; n < 4000; ++n)
    {
        phase.push_back(phase.back() + (static_cast<int>(random() % 17) - 8) / 32.0);
        inverse->Write(phase.back(), static_cast<double>(n));
        for (size_t k = 0; k < 8; ++k)
        {
            double q = phase.back() - static_cast<double>(random() % 128) / 128.0;
            double actual = inverse->GetRealTime(q);
            size_t crossing = 0;
            for (size_t i = n; i > 0; --i)
            {
                if (phase[i - 1] <= q && q <= phase[i] && phase[i - 1] < phase[i])
                {
                    crossing = i;
                    break;
                }
            }

            DOCTEST_REQUIRE(std::isfinite(actual) == (crossing > 0));
            if (crossing == 0)
            {
                continue;
            }

            DOCTEST_CHECK(actual >= 0);
            DOCTEST_CHECK(actual <= static_cast<double>(n));
            size_t sample = static_cast<size_t>(std::clamp(std::floor(actual), 0.0, static_cast<double>(n - 1)));
            double reconstructed = phase[sample] + (actual - sample) * (phase[sample + 1] - phase[sample]);
            DOCTEST_CHECK(std::abs(reconstructed - q) <= .2501);
        }
    }
}
