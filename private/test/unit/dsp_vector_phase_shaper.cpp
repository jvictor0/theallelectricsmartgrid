#include "doctest.h"

#include "../support/GlobalEnv.hpp"
#include "VectorPhaseShaper.hpp"

DOCTEST_TEST_CASE_TEMPLATE("VectorPhaseShaper D range targets a 19 kHz read rate until centered", Shaper,
    VectorPhaseShaperInternal, TrioVectorPhaseShaper)
{
    using Vector = typename Shaper::Vector;
    constexpr double x_frequenciesHz[] = {0, 100, 1000, 2375, 4000, 19000};
    constexpr double x_minimumD[] = {0, 4.0 / 190, 4.0 / 19, 0.5, 0.5, 0.5};
    Shaper shaper;
    for (size_t step = 0; step < 6; ++step)
    {
        for (size_t channel = 0; channel < Vector::x_numChannels; ++channel)
        {
            shaper.m_freq[channel] = SampleTimer::HzToCycPerSamp(x_frequenciesHz[(step + channel) % 6]);
        }

        shaper.SetDScale();
        for (size_t channel = 0; channel < Vector::x_numChannels; ++channel)
        {
            size_t index = (step + channel) % 6;
            double frequencyHz = x_frequenciesHz[index];
            double minimumD = 0.5 - 0.5 * shaper.m_dScale[channel];
            DOCTEST_INFO("frequencyHz=" << frequencyHz << " channel=" << channel);
            DOCTEST_CHECK(minimumD == doctest::Approx(x_minimumD[index]).epsilon(1e-6));
            if (frequencyHz > 0 && frequencyHz <= 2375)
            {
                DOCTEST_CHECK(4 * frequencyHz / minimumD == doctest::Approx(19000).epsilon(1e-6));
            }

            if (frequencyHz >= 2375)
            {
                DOCTEST_CHECK(shaper.m_dScale[channel] == 0);
            }
        }
    }
}

DOCTEST_TEST_CASE_TEMPLATE("VectorPhaseShaper is C2 at every elbow join and the cycle wrap", Shaper,
    VectorPhaseShaperInternal, TrioVectorPhaseShaper)
{
    using Vector = typename Shaper::Vector;
    constexpr float x_duties[] = {1.0f / 64, 0.125f, 0.5f, 0.875f, 63.0f / 64};
    constexpr float x_heights[] = {0, 0.25f, 0.75f, 1, 2, 4};

    // Six-point one-sided differences are exact for quintics. Binary steps
    // keep the sample positions exact; derivatives are scaled by delta and
    // delta squared to keep the tolerances meaningful for narrow elbows.
    //
    constexpr double x_slopeWeights[] = {-137.0 / 60, 5, -5, 10.0 / 3, -1.25, 0.2};
    constexpr double x_curvatureWeights[] = {3.75, -77.0 / 6, 107.0 / 6, -13, 61.0 / 12, -5.0 / 6};
    for (size_t duty = 0; duty < 5; ++duty)
    {
        for (size_t height = 0; height < 6; ++height)
        {
            Vector d;
            Vector v;
            for (size_t channel = 0; channel < Vector::x_numChannels; ++channel)
            {
                d[channel] = x_duties[(duty + channel) % 5];
                v[channel] = x_heights[(height + channel) % 6];
            }

            Vector delta = Vector::Min(d, Vector::Splat(1) - d) * 0.25f;
            Vector joins[] = {Vector::Splat(0), delta, d - delta, d + delta, Vector::Splat(1) - delta};
            for (size_t join = 0; join < 5; ++join)
            {
                Vector before;
                Vector after;
                for (size_t channel = 0; channel < Vector::x_numChannels; ++channel)
                {
                    size_t channelJoin = (join + channel) % 5;
                    before[channel] = channelJoin == 0 ? 1 : joins[channelJoin][channel];
                    after[channel] = joins[channelJoin][channel];
                }

                Vector incoming[6];
                Vector outgoing[6];
                for (size_t sample = 0; sample < 6; ++sample)
                {
                    Vector offset = delta * (static_cast<float>(sample) / 8);
                    incoming[sample] = Shaper::PhiVPSQuinticElbow(before - offset, v, d);
                    outgoing[sample] = Shaper::PhiVPSQuinticElbow(after + offset, v, d);
                }

                for (size_t channel = 0; channel < Vector::x_numChannels; ++channel)
                {
                    size_t channelJoin = (join + channel) % 5;
                    DOCTEST_INFO("d=" << d[channel] << " v=" << v[channel]
                        << " join=" << channelJoin << " channel=" << channel);
                    double cycleOffset = channelJoin == 0 ? 1 : 0;
                    DOCTEST_CHECK(std::abs(incoming[0][channel] - outgoing[0][channel] - cycleOffset) < 3e-6);
                    double incomingSlope = 0;
                    double outgoingSlope = 0;
                    double incomingCurvature = 0;
                    double outgoingCurvature = 0;
                    for (size_t sample = 1; sample < 6; ++sample)
                    {
                        double left = static_cast<double>(incoming[sample][channel]) - incoming[0][channel];
                        double right = static_cast<double>(outgoing[sample][channel]) - outgoing[0][channel];
                        incomingSlope -= 8 * x_slopeWeights[sample] * left;
                        outgoingSlope += 8 * x_slopeWeights[sample] * right;
                        incomingCurvature += 64 * x_curvatureWeights[sample] * left;
                        outgoingCurvature += 64 * x_curvatureWeights[sample] * right;
                    }

                    DOCTEST_CHECK(std::abs(incomingSlope - outgoingSlope) < 2e-4);
                    DOCTEST_CHECK(std::abs(incomingCurvature - outgoingCurvature) < 5e-3);
                    if (channelJoin != 0)
                    {
                        double slope = channelJoin < 3 ? static_cast<double>(v[channel]) / d[channel]
                            : (1.0 - v[channel]) / (1.0 - d[channel]);
                        double value = channelJoin < 3 ? slope * after[channel]
                            : v[channel] + slope * (after[channel] - d[channel]);
                        DOCTEST_CHECK(std::abs(outgoing[0][channel] - value) < 3e-6);
                        DOCTEST_CHECK(std::abs(outgoingSlope - delta[channel] * slope) < 2e-4);
                        DOCTEST_CHECK(std::abs(outgoingCurvature) < 5e-3);
                    }
                }
            }
        }
    }
}

DOCTEST_TEST_CASE_TEMPLATE("VectorPhaseShaper matched inflections leave phase unchanged", Shaper,
    VectorPhaseShaperInternal, TrioVectorPhaseShaper)
{
    using Vector = typename Shaper::Vector;
    for (float duty : {1.0f / 64, 0.125f, 0.5f, 0.875f, 63.0f / 64})
    {
        Vector d = Vector::Splat(duty);
        for (size_t sample = 0; sample <= 256; ++sample)
        {
            Vector phase = Vector::Splat(static_cast<float>(sample) / 256);
            Vector warped = Shaper::PhiVPSQuinticElbow(phase, d, d);
            for (size_t channel = 0; channel < Vector::x_numChannels; ++channel)
            {
                DOCTEST_CHECK(std::abs(warped[channel] - phase[channel]) < 1e-6f);
            }
        }
    }
}

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
    shaper.m_d = 0.5f;
    shaper.m_v = 0.5f - 1e-7f;
    shaper.m_freq = 0.001f;
    shaper.m_phase = 0.0f;
    float phi = VectorPhaseShaperInternal::PhiVPSQuinticElbow(shaper.m_phase, shaper.m_v, shaper.m_d);
    DOCTEST_REQUIRE(phi < 0);
    DOCTEST_REQUIRE(phi - std::floor(phi) == 1.0f);
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
            double minimumD = std::min(0.5, 4 * SampleTimer::CycPerSampToHz(input.m_freq[channel]) / 19000);
            d = minimumD + d * (1 - 2 * minimumD);

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
