#include "doctest.h"
#include "../support/GlobalEnv.hpp"
#include "MorphingWaveTable.hpp"
#include <memory>

template<typename Number, size_t Size>
void CheckIndependentWaveTablePhases()
{
    using Table = BasicWaveTableGeneric<Number, Size, 5>;
    using Vector = typename Table::Vector;
    Table table;
    BasicWaveTableGeneric<Number, 1, 5> mono[Size];
    for (size_t index = 0; index < Table::x_tableSize; ++index)
    {
        for (size_t lane = 0; lane < Size; ++lane)
        {
            Number value = static_cast<Number>(std::sin(0.7 * index + lane));
            table.m_table[index][lane] = value;
            mono[lane].m_table[index] = value;
        }
    }

    for (size_t sample = 0; sample < 500; ++sample)
    {
        Vector phase;
        for (size_t lane = 0; lane < Size; ++lane)
        {
            phase[lane] = static_cast<Number>(-2.0 + sample * 0.013 + lane * 0.21);
        }

        Vector linear = table.EvaluateLinear(phase);
        Vector cubic = table.EvaluateCubic(phase);
        for (size_t lane = 0; lane < Size; ++lane)
        {
            Number wrapped = phase[lane] - std::floor(phase[lane]);
            DOCTEST_CHECK(linear[lane] == doctest::Approx(mono[lane].EvaluateLinear(wrapped)[0]).epsilon(2e-6).scale(1));
            DOCTEST_CHECK(cubic[lane] == doctest::Approx(mono[lane].EvaluateCubic(wrapped)[0]).epsilon(2e-6).scale(1));
        }
    }
}

DOCTEST_TEST_CASE("BasicWaveTableGeneric evaluates independent wrapped phases")
{
    CheckIndependentWaveTablePhases<float, 3>();
    CheckIndependentWaveTablePhases<float, 4>();
    CheckIndependentWaveTablePhases<double, 3>();
    CheckIndependentWaveTablePhases<double, 4>();
}

// Scalar reference of the original band interpolation, with a linear band
// search independent of the generic lookup table and packed evaluator.
//
template<typename Number>
Number ReferenceAdaptiveSample(const AdaptiveWaveTableGeneric<Number, 1>& table,
    Number phase, Number frequency, Number limit)
{
    using Table = AdaptiveWaveTableGeneric<Number, 1>;
    if (limit < frequency)
    {
        return 0;
    }

    Number components = limit / frequency;
    size_t level = 0;
    while (level + 1 < Table::x_maxLevels && Table::s_levelComponents[level + 1] <= components)
    {
        ++level;
    }

    Number lower = table.m_levels[level].EvaluateLinear(phase)[0];
    if (level + 1 == Table::x_maxLevels)
    {
        return lower;
    }

    Number weight = (components - Table::s_levelComponents[level]) /
        static_cast<Number>(Table::s_levelComponents[level + 1] - Table::s_levelComponents[level]);
    Number upper = table.m_levels[level + 1].EvaluateLinear(phase)[0];
    return lower * (1 - weight) + upper * weight;
}

template<typename Number, size_t Size>
void CheckAdaptiveWaveTableLanes()
{
    using Table = AdaptiveWaveTableGeneric<Number, Size>;
    using Mono = AdaptiveWaveTableGeneric<Number, 1>;
    using Vector = typename Table::Vector;
    auto table = std::make_unique<Table>();
    auto monos = std::make_unique<Mono[]>(Size);
    for (size_t index = 0; index < Table::WaveTable::x_tableSize; ++index)
    {
        for (size_t lane = 0; lane < Size; ++lane)
        {
            double phase = 2.0 * M_PI * index / Table::WaveTable::x_tableSize;
            Number value = static_cast<Number>(std::sin(phase + lane) + 0.3 * std::cos((lane + 3) * phase));
            table->m_waveTable.m_table[index][lane] = value;
            monos[lane].m_waveTable.m_table[index] = value;
        }
    }

    table->Generate();
    for (size_t lane = 0; lane < Size; ++lane)
    {
        monos[lane].Generate();
        for (size_t level = 0; level < Table::x_maxLevels; ++level)
        {
            for (size_t index = 0; index < Table::WaveTable::x_tableSize; ++index)
            {
                DOCTEST_CHECK(table->m_levels[level].m_table[index][lane] ==
                    doctest::Approx(monos[lane].m_levels[level].m_table[index][0]).epsilon(2e-6).scale(1));
            }
        }
    }

    MorphingWaveTableGeneric<Number, Size> morph;
    for (size_t lane = 0; lane < Size; ++lane)
    {
        DOCTEST_CHECK(morph.SetLeft(lane, &monos[lane]) == nullptr);
        DOCTEST_CHECK(morph.SetRight(lane, &monos[(lane + 1) % Size]) == nullptr);
    }

    for (size_t sample = 0; sample < 300; ++sample)
    {
        Vector phase, freq, limit, blend;
        for (size_t lane = 0; lane < Size; ++lane)
        {
            phase[lane] = static_cast<Number>(std::fmod(0.013 * sample + lane * 0.21, 1.0));
            freq[lane] = static_cast<Number>(0.001 * (lane + 1));
            limit[lane] = freq[lane] * static_cast<Number>(sample * 2.13 + lane * 0.7);
            blend[lane] = static_cast<Number>((sample + lane) % 17) / 16;
        }

        typename Table::EvalSite site;
        Table::EvaluateSite(freq, limit, site);
        Vector packed = table->Evaluate(phase, site);
        Vector morphed = morph.Evaluate(phase, freq, limit, blend);
        for (size_t lane = 0; lane < Size; ++lane)
        {
            Number left = ReferenceAdaptiveSample(monos[lane], phase[lane], freq[lane], limit[lane]);
            Number right = ReferenceAdaptiveSample(monos[(lane + 1) % Size], phase[lane], freq[lane], limit[lane]);
            DOCTEST_CHECK(packed[lane] == doctest::Approx(left).epsilon(2e-6).scale(1));
            DOCTEST_CHECK(morphed[lane] == doctest::Approx(left * (1 - blend[lane]) + right * blend[lane]).epsilon(2e-6).scale(1));
        }
    }
}

DOCTEST_TEST_CASE("Adaptive and morphing generic tables preserve independent lane spectra and table ownership")
{
    GlobalEnv::ResetPerTest();
    CheckAdaptiveWaveTableLanes<float, 3>();
    CheckAdaptiveWaveTableLanes<double, 4>();
}

DOCTEST_TEST_CASE("AdaptiveWaveTableGeneric selects level boundaries and mutes above the frequency limit")
{
    using Table = AdaptiveWaveTableGeneric<float, 4>;
    Table::EvalSite site;
    for (size_t level = 0; level + 1 < Table::x_maxLevels; ++level)
    {
        float lower = static_cast<float>(Table::s_levelComponents[level]);
        float upper = static_cast<float>(Table::s_levelComponents[level + 1]);
        if (lower == upper)
        {
            continue;
        }

        Table::EvaluateSite(QuadFloat::Splat(1), QuadFloat(lower, (lower + upper) * 0.5f, 0.5f, 10000.0f), site);
        DOCTEST_CHECK(site.m_lower[0] == static_cast<int>(level));
        DOCTEST_CHECK(site.m_wayThrough[0] == 0.0f);
        DOCTEST_CHECK(site.m_lower[1] == static_cast<int>(level));
        DOCTEST_CHECK(site.m_upper[1] == static_cast<int>(level + 1));
        DOCTEST_CHECK(site.m_wayThrough[1] == 0.5f);
        DOCTEST_CHECK(site.m_lower[2] == -1);
        DOCTEST_CHECK(site.m_upper[2] == -1);
        DOCTEST_CHECK(site.m_wayThrough[2] == 0.0f);
        DOCTEST_CHECK(Table::s_levelComponents[site.m_lower[3]] == Table::DFT::x_maxComponents);
    }

    Table::EvaluateSite(QuadFloat::Splat(0), QuadFloat::Splat(0.5f), site);
    for (size_t lane = 0; lane < 4; ++lane)
    {
        DOCTEST_CHECK(Table::s_levelComponents[site.m_lower[lane]] == Table::DFT::x_maxComponents);
        DOCTEST_CHECK(site.m_wayThrough[lane] == 0.0f);
    }
}
