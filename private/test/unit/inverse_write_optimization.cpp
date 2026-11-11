#include "doctest.h"
#include "TimeWarpInverter.hpp"
#include "perf/reference/LandedTimeWarpInverter.hpp"
#include <atomic>
#include <random>
#include <thread>
#include <vector>

namespace
{
bool SameInverseValue(double a, double b)
{
    return (std::isnan(a) && std::isnan(b)) || a == b;
}

template <size_t Size>
void CompareInverseWrites(const std::vector<double>& phases, double origin)
{
    TimeWarpInverter<Size> current;
    LandedTimeWarpInverter<Size> landed;
    for (size_t n = 0; n < phases.size(); ++n)
    {
        double time = origin + n;
        current.Write(phases[n], time);
        landed.Write(phases[n], time);
        DOCTEST_REQUIRE(SameInverseValue(current.m_seedPhase.load(), landed.m_seedPhase.load()));
        DOCTEST_REQUIRE(SameInverseValue(current.m_frontierPhase.load(), landed.m_frontierPhase.load()));
        DOCTEST_REQUIRE(SameInverseValue(current.m_frontierTime.load(), landed.m_frontierTime.load()));
        DOCTEST_REQUIRE(current.m_oldestWarpedTime == landed.m_oldestWarpedTime);
        DOCTEST_REQUIRE(current.m_islandReassignments == landed.m_islandReassignments);
        for (size_t k = 0; k < 4 * (Size + 2); ++k)
        {
            double query = std::floor(phases[n]) - Size - 1 + .25 * k;
            for (double cutoff : {time, time - .5, time - 10})
            {
                DOCTEST_INFO("sample=" << n << " phase=" << phases[n] << " query=" << query << " cutoff=" << cutoff);
                DOCTEST_REQUIRE(SameInverseValue(current.ReadInverse(query, cutoff), landed.ReadInverse(query, cutoff)));
            }
        }

        DOCTEST_REQUIRE(SameInverseValue(current.GetRealTime(phases[n]), landed.GetRealTime(phases[n])));
    }
}
}

DOCTEST_TEST_CASE("Inverse writes: exact differential startup turns boundaries and ring aliases")
{
    const std::vector<double> phases = {-.4, -.1, 0, 1, .25, .75, 1, .5, .9,
        1, 1.1, .6, .8, 12, 12, 4.5, 4.25, 4.75, 4.5, 5, 40.5, -2.2, -1.8, 50.25};
    for (double origin : {0.0, 1e12})
    {
        CompareInverseWrites<1>(phases, origin);
        CompareInverseWrites<4>(phases, origin);
        CompareInverseWrites<32>(phases, origin);
    }
}

DOCTEST_TEST_CASE("Inverse writes: randomized differential includes large forward and reverse jumps")
{
    std::mt19937_64 random(20261111);
    std::vector<double> phases{-.375};
    for (size_t n = 0; n < 4000; ++n)
    {
        double step = static_cast<int>(random() % 321) / 8.0 - 16;
        phases.push_back(phases.back() + step);
    }

    CompareInverseWrites<4>(phases, 0);
    CompareInverseWrites<37>(phases, 1e12);
}

DOCTEST_TEST_CASE("Inverse writes: stale inactive payload cannot leak after replacement activation")
{
    TimeWarpInverter<16> inverse;
    inverse.Write(0, 0);
    inverse.Write(16, 16);
    for (size_t slot = 0; slot < 16; ++slot)
    {
        inverse.m_turnTime[slot].store(-1e30f);
        inverse.m_currentRightTime[slot].store(1e30f);
    }

    inverse.Write(0, 17);
    inverse.Write(4, 21);
    DOCTEST_CHECK(inverse.GetRealTime(2.5) == 19.5);
    inverse.Write(2.5, 22);
    inverse.Write(3.5, 23);
    DOCTEST_CHECK(inverse.GetRealTime(2.75) == 22.25);
    DOCTEST_CHECK(inverse.GetRealTime(2.25) == 19.25);
}

DOCTEST_TEST_CASE("Inverse writes: concurrent reads observe coherent batched traversal prefixes")
{
    TimeWarpInverter<8192> inverse;
    inverse.Write(0, 0);
    inverse.Write(1024, 4096);
    std::atomic<bool> done{false};
    std::atomic<bool> ready{false};
    std::atomic<size_t> reads{0};
    std::atomic<size_t> errors{0};
    std::thread reader([&]
    {
        ready.store(true);
        size_t i = 0;
        while (!done.load())
        {
            double q = 1023 + static_cast<double>(i++ % 6144) + .375;
            double value = inverse.ReadInverse(q, 32768);
            if (std::isfinite(value) && value != 4 * q)
            {
                ++errors;
            }

            ++reads;
        }
    });
    while (!ready.load())
    {
        std::this_thread::yield();
    }

    for (size_t n = 0; n < 100; ++n)
    {
        inverse.Write(1024, 4096);
        inverse.Write(7168, 28672);
    }

    done.store(true);
    reader.join();
    DOCTEST_CHECK(reads.load() > 0);
    DOCTEST_CHECK(errors.load() == 0);
}
