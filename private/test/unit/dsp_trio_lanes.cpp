#include "doctest.h"

#include "../support/GlobalEnv.hpp"
#include "../support/TimeRig.hpp"
#include "AudioBuffer.hpp"
#include "DualSampleSource.hpp"
#include "DualWaveShapingVCO.hpp"
#include "LadderFilter.hpp"
#include "Noise.hpp"
#include "PhasorPlayHead.hpp"
#include "PhysicalModelingSource.hpp"
#include "StateVariableFilter.hpp"
#include "TrioUtils.hpp"
#include <cmath>
#include <memory>
#include <vector>

// Each trio channel must match a mono instance fed that channel's inputs.
// Channels get distinct inputs so a channel mix-up shows as a mismatch.
//
namespace
{

struct ChannelDifference
{
    double m_maxDifference[3] = {0.0, 0.0, 0.0};
    double m_maxMagnitude[3] = {0.0, 0.0, 0.0};

    void Add(size_t channel, double trio, double mono)
    {
        m_maxDifference[channel] = std::max(m_maxDifference[channel], std::abs(trio - mono));
        m_maxMagnitude[channel] = std::max(m_maxMagnitude[channel], std::abs(mono));
        if (!std::isfinite(trio) || !std::isfinite(mono))
        {
            m_maxDifference[channel] = INFINITY;
        }
    }

    // Every channel must also carry signal, so a silent pair cannot pass.
    //
    void Check(double tolerance) const
    {
        for (size_t i = 0; i < 3; ++i)
        {
            DOCTEST_INFO("channel=" << i << " difference=" << m_maxDifference[i] << " magnitude=" << m_maxMagnitude[i]);
            DOCTEST_CHECK(m_maxDifference[i] <= tolerance);
            DOCTEST_CHECK(m_maxMagnitude[i] > 1e-6);
        }
    }
};

std::unique_ptr<AdaptiveWaveTable> MakeWaveTable(int harmonic)
{
    auto table = std::make_unique<AdaptiveWaveTable>();
    for (size_t i = 0; i < BasicWaveTable::x_tableSize; ++i)
    {
        double phase = 2.0 * M_PI * static_cast<double>(i) / BasicWaveTable::x_tableSize;
        table->m_waveTable.m_table[i] = static_cast<float>(std::sin(phase) + 0.5 * std::sin(harmonic * phase));
    }

    table->Generate();
    return table;
}

std::shared_ptr<AudioBufferBank> MakeSineBank(size_t length, double cyclesPerSample)
{
    auto buffer = std::make_shared<AudioBuffer>();
    buffer->m_buffer.resize(length);
    for (size_t i = 0; i < length; ++i)
    {
        buffer->m_buffer[i] = static_cast<float>(std::sin(2.0 * M_PI * cyclesPerSample * static_cast<double>(i)));
    }

    buffer->ComputeSectionExtrema();
    auto bank = std::make_shared<AudioBufferBank>();
    bank->m_audioBuffers.push_back(buffer);
    return bank;
}

}

DOCTEST_TEST_CASE("MultiChannelNumber: fast Log2 tracks std::log2")
{
    double maxError = 0.0;
    for (int i = -2000; i <= 2000; ++i)
    {
        float x = std::exp2(static_cast<float>(i) * 0.01f + 0.003f);
        MultiChannelNumber<float, 3> value(x, x * 1.37f, x * 0.71f);
        MultiChannelNumber<float, 3> result = value.Log2();
        for (size_t j = 0; j < 3; ++j)
        {
            maxError = std::max(maxError, std::abs(static_cast<double>(result[j]) - std::log2(static_cast<double>(value[j]))));
        }
    }

    DOCTEST_INFO("maxError=" << maxError);
    DOCTEST_CHECK(maxError < 4e-6);
    DOCTEST_CHECK(MultiChannelNumber<float, 1>(1.0f).Log2() == 0.0f);
    DOCTEST_CHECK(MultiChannelNumber<float, 1>(8.0f).Log2() == 3.0f);
}

DOCTEST_TEST_CASE("WhiteNoiseGeneric: zero mean, one third deviation, bounded, distinct channels")
{
    TrioWhiteNoise noise(7);
    constexpr size_t x_count = 200000;
    double sum[3] = {0.0, 0.0, 0.0};
    double squares[3] = {0.0, 0.0, 0.0};
    double cross = 0.0;
    float peak = 0.0f;
    for (size_t n = 0; n < x_count; ++n)
    {
        TrioFloat value = noise.Generate();
        for (size_t i = 0; i < 3; ++i)
        {
            sum[i] += value[i];
            squares[i] += static_cast<double>(value[i]) * value[i];
            peak = std::max(peak, std::abs(value[i]));
        }

        cross += static_cast<double>(value[0]) * value[1];
    }

    for (size_t i = 0; i < 3; ++i)
    {
        double mean = sum[i] / x_count;
        double deviation = std::sqrt(squares[i] / x_count - mean * mean);
        DOCTEST_INFO("channel=" << i << " mean=" << mean << " deviation=" << deviation);
        DOCTEST_CHECK(std::abs(mean) < 0.003);
        DOCTEST_CHECK(std::abs(deviation - 1.0 / 3.0) < 0.003);
    }

    DOCTEST_CHECK(peak <= 2.0f / std::sqrt(3.0f) + 1e-6f);
    DOCTEST_CHECK(std::abs(cross / x_count) < 0.003);

    WhiteNoiseGeneric<1> mono(8);
    TrioWhiteNoise trio(7);
    for (size_t n = 0; n < 100; ++n)
    {
        DOCTEST_CHECK(trio.Generate()[1] == mono.Generate()[0]);
    }
}

DOCTEST_TEST_CASE("PinkNoiseGeneric: bounded, zero mean, channels match mono generators")
{
    TrioPinkNoise trio(11);
    PinkNoiseGeneric<1> mono[3] = {PinkNoiseGeneric<1>(11), PinkNoiseGeneric<1>(12), PinkNoiseGeneric<1>(13)};
    ChannelDifference output;
    double sum[3] = {0.0, 0.0, 0.0};
    float peak = 0.0f;
    constexpr size_t x_count = 200000;
    for (size_t n = 0; n < x_count; ++n)
    {
        TrioFloat value = trio.Generate();
        for (size_t i = 0; i < 3; ++i)
        {
            output.Add(i, value[i], mono[i].Generate()[0]);
            sum[i] += value[i];
            peak = std::max(peak, std::abs(value[i]));
        }
    }

    output.Check(0.0);
    DOCTEST_CHECK(peak <= 1.0f);
    for (size_t i = 0; i < 3; ++i)
    {
        DOCTEST_INFO("channel=" << i << " mean=" << sum[i] / x_count);
        DOCTEST_CHECK(std::abs(sum[i] / x_count) < 0.01);
    }
}

DOCTEST_TEST_CASE("Trio: SVF channels match mono filters")
{
    TrioLinearStateVariableFilter trio;
    LinearStateVariableFilter mono[3];
    ChannelDifference morphed;
    ChannelDifference lowPass;
    const float morph[3] = {0.1f, 0.5f, 0.85f};
    for (size_t n = 0; n < 4000; ++n)
    {
        TrioFloat input;
        TrioFloat cutoff;
        TrioFloat resonance;
        for (size_t i = 0; i < 3; ++i)
        {
            input[i] = static_cast<float>(std::sin(0.01 * (i + 1) * n) + 0.3 * std::sin(0.37 * n));
            cutoff[i] = 0.01f * (i + 1) + 0.005f * static_cast<float>(std::sin(0.001 * n));
            resonance[i] = 0.3f * i;
        }

        trio.SetCutoffAndResonance(cutoff, resonance);
        trio.Process(input);
        TrioFloat trioMorphed = trio.GetMorphedOutput(TrioFloat(morph[0], morph[1], morph[2]));
        for (size_t i = 0; i < 3; ++i)
        {
            mono[i].SetCutoffAndResonance(cutoff[i], resonance[i]);
            mono[i].Process(input[i]);
            morphed.Add(i, trioMorphed[i], mono[i].GetMorphedOutput(morph[i])[0]);
            lowPass.Add(i, trio.m_lowPass[i], mono[i].m_lowPass[0]);
        }
    }

    morphed.Check(1e-6);
    lowPass.Check(1e-6);
}

DOCTEST_TEST_CASE("Trio: ladder channels match mono filters")
{
    TrioLadderFilterLP trio;
    LadderFilterLP mono[3];
    ChannelDifference output;
    for (size_t n = 0; n < 4000; ++n)
    {
        TrioFloat input;
        TrioFloat cutoff;
        TrioFloat resonance;
        for (size_t i = 0; i < 3; ++i)
        {
            input[i] = static_cast<float>(0.5 * std::sin(0.02 * (i + 1) * n));
            cutoff[i] = 0.005f * (i + 1) + 0.002f * static_cast<float>(std::sin(0.002 * n));
            resonance[i] = 0.4f * i;
        }

        trio.SetCutoff(cutoff);
        trio.SetResonance(resonance);
        TrioFloat trioOutput = trio.Process(input);
        for (size_t i = 0; i < 3; ++i)
        {
            mono[i].SetCutoff(cutoff[i]);
            mono[i].SetResonance(resonance[i]);
            output.Add(i, trioOutput[i], mono[i].Process(input[i])[0]);
        }
    }

    output.Check(1e-6);
}

DOCTEST_TEST_CASE("Trio: dual wave shaping VCO channels match mono cores")
{
    auto left = MakeWaveTable(3);
    auto right = MakeWaveTable(5);

    auto trio = std::make_unique<TrioDualWaveShapingVCO>();
    std::unique_ptr<DualWaveShapingVCOGeneric<1>> mono[3];
    for (size_t i = 0; i < 3; ++i)
    {
        mono[i] = std::make_unique<DualWaveShapingVCOGeneric<1>>();
        for (size_t j = 0; j < 2; ++j)
        {
            trio->m_vco[j].SetLeft(i, left.get());
            trio->m_vco[j].SetRight(i, right.get());
            mono[i]->m_vco[j].SetLeft(left.get());
            mono[i]->m_vco[j].SetRight(right.get());
        }
    }

    TrioDualWaveShapingVCO::Input trioInput;
    DualWaveShapingVCOGeneric<1>::Input monoInput[3];
    ChannelDifference output;
    ChannelDifference tops;
    size_t topCount[3] = {0, 0, 0};
    for (size_t n = 0; n < 8000; ++n)
    {
        for (size_t i = 0; i < 3; ++i)
        {
            float wobble = static_cast<float>(std::sin(0.0007 * n + i));
            for (size_t j = 0; j < 2; ++j)
            {
                trioInput.m_v[j][i] = 0.3f + 0.2f * i + 0.1f * wobble;
                trioInput.m_d[j][i] = 0.2f + 0.25f * i + 0.05f * j;
                trioInput.m_wtBlend[j][i] = 0.3f * i;
                trioInput.m_morphHarmonics[j][i] = 0.4f + 0.1f * j;
                trioInput.m_crossModIndex[j][i] = 0.15f * i;
            }

            trioInput.m_baseFreq[i] = 0.002f * (i + 1);
            trioInput.m_offsetFreqFactor[i] = 2.0f;
            trioInput.m_detune[i] = 1.01f;
            trioInput.m_fade[i] = 0.3f * i + 0.1f * wobble;
            trioInput.m_bitCrushAmount[i] = 0.25f * i;

            for (size_t j = 0; j < 2; ++j)
            {
                monoInput[i].m_v[j] = trioInput.m_v[j][i];
                monoInput[i].m_d[j] = trioInput.m_d[j][i];
                monoInput[i].m_wtBlend[j] = trioInput.m_wtBlend[j][i];
                monoInput[i].m_morphHarmonics[j] = trioInput.m_morphHarmonics[j][i];
                monoInput[i].m_crossModIndex[j] = trioInput.m_crossModIndex[j][i];
            }

            monoInput[i].m_baseFreq = trioInput.m_baseFreq[i];
            monoInput[i].m_offsetFreqFactor = trioInput.m_offsetFreqFactor[i];
            monoInput[i].m_detune = trioInput.m_detune[i];
            monoInput[i].m_fade = trioInput.m_fade[i];
            monoInput[i].m_bitCrushAmount = trioInput.m_bitCrushAmount[i];
        }

        TrioFloat trioOutput = trio->Process(trioInput);
        for (size_t i = 0; i < 3; ++i)
        {
            float monoOutput = mono[i]->Process(monoInput[i]);
            output.Add(i, trioOutput[i], monoOutput);

            const SampleTop& trioTop = trio->m_vco[0].m_top[i];
            const SampleTop& monoTop = mono[i]->m_vco[0].m_top[0];
            DOCTEST_REQUIRE(trioTop.m_triggered == monoTop.m_triggered);
            if (monoTop.m_triggered)
            {
                ++topCount[i];
                tops.Add(i, trioTop.m_offset, monoTop.m_offset);
            }
        }
    }

    output.Check(1e-6);
    tops.Check(1e-9);
    for (size_t i = 0; i < 3; ++i)
    {
        DOCTEST_CHECK(topCount[i] > 10);
    }
}

DOCTEST_TEST_CASE("Trio: physical modeling channels match mono cores through shared comb transitions")
{
    auto trio = std::make_unique<TrioPhysicalModelingSource>(42);
    std::unique_ptr<PhysicalModelingSourceGeneric<1>> mono[3];
    for (size_t i = 0; i < 3; ++i)
    {
        mono[i] = std::make_unique<PhysicalModelingSourceGeneric<1>>(42 + static_cast<uint32_t>(i));
    }

    const float decay[3] = {0.2f, 0.5f, 0.9f};
    const float morph[3] = {0.1f, 0.5f, 0.9f};
    const float reducer[3] = {1.0f, 0.3f, 0.05f};
    TrioPhysicalModelingSource::Input trioInput;
    PhysicalModelingSourceGeneric<1>::Input monoInput[3];
    ChannelDifference output;
    for (size_t n = 0; n < 12000; ++n)
    {
        if (n % 32 == 0)
        {
            // Only the last channel's pitch moves after the first third, so
            // the others ride through its comb transitions.
            //
            TrioFloat frequency;
            TrioFloat damp;
            for (size_t i = 0; i < 3; ++i)
            {
                frequency[i] = 0.002f * (i + 1);
                damp[i] = 0.3f * i + 0.1f;
            }

            if (n > 4000)
            {
                frequency[2] = 0.006f + 0.001f * static_cast<float>(std::sin(0.001 * n));
            }

            trio->SetCombParams(frequency, TrioFloat(decay[0], decay[1], decay[2]), damp, 192000.0);
            for (size_t i = 0; i < 3; ++i)
            {
                mono[i]->SetCombParams(frequency[i], decay[i], damp[i], 192000.0);
            }
        }

        for (size_t i = 0; i < 3; ++i)
        {
            trioInput.m_mainSVFCutoff[i] = 0.01f * (i + 1);
            trioInput.m_mainSVFResonance[i] = 0.3f * i;
            trioInput.m_mainSVFMorph[i] = morph[i];
            trioInput.m_sampleRateReducerFreq[i] = reducer[i];
            trioInput.m_envelope[i] = 0.5f + 0.5f * static_cast<float>(std::sin(0.0005 * n * (i + 1)));

            monoInput[i].m_mainSVFCutoff = trioInput.m_mainSVFCutoff[i];
            monoInput[i].m_mainSVFResonance = trioInput.m_mainSVFResonance[i];
            monoInput[i].m_mainSVFMorph = trioInput.m_mainSVFMorph[i];
            monoInput[i].m_sampleRateReducerFreq = trioInput.m_sampleRateReducerFreq[i];
            monoInput[i].m_envelope = trioInput.m_envelope[i];
        }

        TrioFloat trioOutput = trio->Process(trioInput);
        for (size_t i = 0; i < 3; ++i)
        {
            output.Add(i, trioOutput[i], mono[i]->Process(monoInput[i])[0]);
        }
    }

    output.Check(1e-6);
}

DOCTEST_TEST_CASE("Trio: phasor play head channels match mono heads")
{
    GlobalEnv::ResetPerTest();
    TimeRig rig;
    rig.SetGlobalPeriodSamples(1024.0);
    rig.SetRunning(true);
    rig.AdvanceControlFrame();

    TrioPhasorPlayHead trio;
    PhasorPlayHead mono[3];
    TrioPhasorPlayHead::Input trioInput;
    PhasorPlayHead::Input monoInput[3];
    const float start[3] = {0.0f, 0.25f, 0.6f};
    const float length[3] = {1.0f, 0.5f, 0.0f};
    const float speed[3] = {1.0f, -0.5f, 2.0f};
    trioInput.m_theoryOfTime = rig.Get();
    for (size_t i = 0; i < 3; ++i)
    {
        trioInput.m_start[i] = start[i];
        trioInput.m_length[i] = length[i];
        trioInput.m_speed[i] = speed[i];
        trioInput.m_loopIndex[i] = static_cast<int>(TimeRig::x_globalLoop);
        monoInput[i].m_theoryOfTime = rig.Get();
        monoInput[i].m_start = start[i];
        monoInput[i].m_length = length[i];
        monoInput[i].m_speed = speed[i];
        monoInput[i].m_loopIndex = static_cast<int>(TimeRig::x_globalLoop);
    }

    ChannelDifference position;
    double maxZeroLength = 0.0;
    for (size_t frame = 0; frame < 300; ++frame)
    {
        for (int sample = 0; sample < 8; ++sample)
        {
            trioInput.m_sampleIndex = sample;
            TrioFloat trioPosition = trio.Process(trioInput);
            for (size_t i = 0; i < 3; ++i)
            {
                monoInput[i].m_sampleIndex = sample;
                float monoPosition = mono[i].Process(monoInput[i]);
                if (length[i] > 0.0f)
                {
                    position.Add(i, trioPosition[i], monoPosition);
                }
                else
                {
                    maxZeroLength = std::max(maxZeroLength, std::abs(static_cast<double>(trioPosition[i])) + std::abs(monoPosition));
                }
            }
        }

        rig.AdvanceControlFrame();
    }

    for (size_t i = 0; i < 2; ++i)
    {
        DOCTEST_INFO("channel=" << i);
        DOCTEST_CHECK(position.m_maxDifference[i] == 0.0);
        DOCTEST_CHECK(position.m_maxMagnitude[i] > 0.1);
    }

    DOCTEST_CHECK(maxZeroLength == 0.0);
}

DOCTEST_TEST_CASE("Trio: sample source channels match mono cores with their own banks")
{
    GlobalEnv::ResetPerTest();
    TimeRig rig;
    rig.SetGlobalPeriodSamples(4096.0);
    rig.SetRunning(true);
    rig.AdvanceControlFrame();

    std::shared_ptr<AudioBufferBank> banks[3] = {
        MakeSineBank(16384, 0.01),
        MakeSineBank(12000, 0.023),
        MakeSineBank(20000, 0.004)};

    // Each grain manager holds a thousand grain buffers; keep them off the stack.
    //
    auto trio = std::make_unique<TrioSampleSource>();
    std::unique_ptr<SampleSourceGeneric<1>> mono[3];
    TrioSampleSource::Input trioInput;
    SampleSourceGeneric<1>::Input monoInput[3];
    const float start[3] = {0.0f, 0.3f, 0.5f};
    const float length[3] = {1.0f, 0.5f, 0.25f};
    const float speed[3] = {1.0f, 0.5f, -1.0f};
    trioInput.m_theoryOfTime = rig.Get();
    for (size_t i = 0; i < 3; ++i)
    {
        mono[i] = std::make_unique<SampleSourceGeneric<1>>();
        mono[i]->SetAudioBufferBank(0, banks[i].get());
        trio->SetAudioBufferBank(i, banks[i].get());

        trioInput.m_phasorPlayHeadInput.m_start[i] = start[i];
        trioInput.m_phasorPlayHeadInput.m_length[i] = length[i];
        trioInput.m_phasorPlayHeadInput.m_speed[i] = speed[i];
        trioInput.m_phasorPlayHeadInput.m_loopIndex[i] = static_cast<int>(TimeRig::x_globalLoop);
        monoInput[i].m_theoryOfTime = rig.Get();
        monoInput[i].m_phasorPlayHeadInput.m_start = start[i];
        monoInput[i].m_phasorPlayHeadInput.m_length = length[i];
        monoInput[i].m_phasorPlayHeadInput.m_speed = speed[i];
        monoInput[i].m_phasorPlayHeadInput.m_loopIndex = static_cast<int>(TimeRig::x_globalLoop);
    }

    ChannelDifference output;
    for (size_t frame = 0; frame < 1500; ++frame)
    {
        for (int sample = 0; sample < 8; ++sample)
        {
            TrioFloat trioOutput = trio->Process(trioInput, sample);
            for (size_t i = 0; i < 3; ++i)
            {
                output.Add(i, trioOutput[i], mono[i]->Process(monoInput[i], sample)[0]);
                DOCTEST_REQUIRE(trio->m_top[i].m_triggered == mono[i]->m_top[0].m_triggered);
            }
        }

        rig.AdvanceControlFrame();
    }

    output.Check(1e-5);
}
