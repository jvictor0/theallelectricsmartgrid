#include "doctest.h"

#include "../support/GlobalEnv.hpp"
#include "VectorPhaseShaper.hpp"

DOCTEST_TEST_CASE("VectorPhaseShaper wraps a negative elbow value that rounds to one")
{
    GlobalEnv::ResetPerTest();
    VectorPhaseShaperInternal shaper;
    VectorPhaseShaperInternal::Input input;
    AdaptiveWaveTable table;
    for (size_t sample = 0; sample < BasicWaveTable::x_tableSize; ++sample)
    {
        table.m_waveTable.m_table[sample] = std::sin(2.0 * M_PI * sample / BasicWaveTable::x_tableSize);
    }

    table.Generate();
    shaper.SetLeft(&table);
    shaper.SetRight(&table);
    shaper.m_dScale = 1.0f;
    shaper.m_d = 0.01f;
    shaper.m_v = 1.01f;
    shaper.m_freq = 0.001f;
    shaper.m_phase = 0.0f;
    shaper.Evaluate(input);
    float atBoundary = shaper.m_out[0];

    shaper.m_phase = 1e-10f;
    shaper.Evaluate(input);
    DOCTEST_CHECK(std::isfinite(shaper.m_out[0]));
    DOCTEST_CHECK(std::abs(shaper.m_out[0] - atBoundary) < 1e-5f);
}

DOCTEST_TEST_CASE("VectorPhaseShaper applies D and V changes on the current engine sample")
{
    GlobalEnv::ResetPerTest();
    TrioVectorPhaseShaper shaper;
    TrioVectorPhaseShaper::Input input;
    AdaptiveWaveTable table;
    for (size_t sample = 0; sample < BasicWaveTable::x_tableSize; ++sample)
    {
        table.m_waveTable.m_table[sample] = std::sin(2.0 * M_PI * sample / BasicWaveTable::x_tableSize);
    }

    table.Generate();
    for (size_t channel = 0; channel < 3; ++channel)
    {
        shaper.SetLeft(channel, &table);
        shaper.SetRight(channel, &table);
        input.m_freq[channel] = 0.001f + 0.0002f * channel;
    }

    for (size_t step = 0; step < 4; ++step)
    {
        TrioVectorPhaseShaper::Vector expectedPhase;
        for (size_t channel = 0; channel < 3; ++channel)
        {
            input.m_d[channel] = step == 0 ? 0.25f + 0.25f * channel : 0.75f - 0.25f * channel;
            input.m_v[channel] = step == 2 ? 0.25f + 0.125f * channel : 0.125f;
            if (step == 3)
            {
                input.m_d[channel] = 0.5f;
            }

            double d = 0.5 - 0.5 * std::cos(M_PI * input.m_d[channel]);
            d = (d - 0.5) * (1.0 - 16.0 * input.m_freq[channel]) + 0.5;

            // Stay between the wrap elbow and the inflection elbow.
            // D changes alone on step 1; V changes alone on step 2.
            //
            double phase = 0.4 * d;
            double delta = 0.25 * std::min(d, 1.0 - d);
            DOCTEST_REQUIRE(phase > delta);
            DOCTEST_REQUIRE(phase < d - delta);
            shaper.m_phase[channel] = static_cast<float>(phase) - input.m_freq[channel];
            expectedPhase[channel] = static_cast<float>(phase * 4.0 * input.m_v[channel] / d);
        }

        auto expected = -shaper.m_morphingWaveTable.Evaluate(
            expectedPhase, input.m_freq, input.m_morphHarmonics, input.m_wtBlend);
        shaper.Process(input);
        for (size_t channel = 0; channel < 3; ++channel)
        {
            DOCTEST_INFO("step=" << step << " channel=" << channel);
            DOCTEST_CHECK(std::abs(shaper.m_out[channel] - expected[channel]) < 1e-5f);
        }
    }
}
