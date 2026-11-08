#include "doctest.h"

#include <iterator>

#include "../support/SynthRig.hpp"

DOCTEST_TEST_CASE("Quad random LFOs use the same timing presets as voices")
{
    synthrig::SynthRig rig;
    auto& synth = rig.Internal().m_squiggleBoy;
    constexpr double x_waitingSeconds[] = { 12.0, 24.0 };

    DOCTEST_REQUIRE(std::size(synth.m_gangedRandomLFOInput) == 10);
    for (size_t random = 0; random < 10; ++random)
    {
        const auto& input = synth.m_gangedRandomLFOInput[random];
        DOCTEST_CHECK(input.m_waiting.m_muSeconds == x_waitingSeconds[random % 2]);
        DOCTEST_CHECK(input.m_targetInternalSigma == doctest::Approx(random % 2 == 0 ? 0.1f : 0.3f));
        for (auto& track : synth.m_tracks)
        {
            DOCTEST_CHECK(std::size(track.m_gangedRandomLFO) == 10);
        }
    }

    for (auto& input : synth.m_gangedRandomLFOInput)
    {
        input.m_waiting.m_sigmaSeconds = 0.0;
        input.m_waiting.m_internalSigmaHz = 0.0;
        input.m_moving.m_sigmaSeconds = 0.0;
        input.m_moving.m_internalSigmaHz = 0.0;
    }

    rig.RunSamples(1);
    rig.Internal().PopulateUIState();
    auto& uiState = rig.UIState().m_squiggleBoyUIState;

    for (size_t slot = 0; slot < std::size(uiState.m_quadGangedRandomLFOUIState); ++slot)
    {
        DOCTEST_CAPTURE(slot);
        GangedRandomLFOSnapshot<4> quad;
        GangedRandomLFOSnapshot<3> voice;
        DOCTEST_REQUIRE(uiState.m_quadGangedRandomLFOUIState[slot].ReadSnapshot(quad));
        DOCTEST_REQUIRE(uiState.m_gangedRandomLFOUIState[slot][0].ReadSnapshot(voice));

        const double waitingIncrement = 1.0 / (static_cast<double>(SampleTimer::x_sampleRate) * x_waitingSeconds[slot % 2]);
        const double movingIncrement = 2.0 / (static_cast<double>(SampleTimer::x_sampleRate) * x_waitingSeconds[slot % 2]);
        DOCTEST_CHECK(voice.m_voices[0].m_waitingIncrement == doctest::Approx(waitingIncrement).epsilon(1e-10));
        DOCTEST_CHECK(voice.m_voices[0].m_movingIncrement == doctest::Approx(movingIncrement).epsilon(1e-10));
        for (const auto& lane : quad.m_voices)
        {
            DOCTEST_CHECK(lane.m_waitingIncrement == doctest::Approx(waitingIncrement).epsilon(1e-10));
            DOCTEST_CHECK(lane.m_movingIncrement == doctest::Approx(movingIncrement).epsilon(1e-10));
        }
    }
}

DOCTEST_TEST_CASE("Modulation noise draws only on host samples for every source lane")
{
    synthrig::SynthRig rig;
    auto& synth = rig.Internal().m_squiggleBoy;
    SquiggleBoyWithEncoderBank::Input input;
    using namespace SmartGridOneModulators;
    auto& manager = synth.m_encoders.GetModulatorManager();
    std::array<float, 14> expectedValues{};
    for (size_t sample = 0; sample < SampleTimer::x_oversample * 4; ++sample)
    {
        SampleTimer::IncrementSample();
        auto expectedGenerator = RGen::s_gen;
        if (SampleTimer::IsHostFrame())
        {
            std::uniform_real_distribution<float> uniform(0.0f, 1.0f);
            for (auto& value : expectedValues)
            {
                value = uniform(expectedGenerator);
            }
        }

        synth.SetVoiceModulators(input);
        synth.SetGlobalModulators();
        synth.SetQuadModulators();
        DOCTEST_CHECK(RGen::s_gen == expectedGenerator);
        for (size_t voice = 0; voice < 9; ++voice)
        {
            DOCTEST_CHECK(manager.GetSource(GetSource(VoiceModulator::Noise, voice / 3)).m_value[voice % 3]
                == expectedValues[voice]);
        }

        DOCTEST_CHECK(manager.GetSource(GetSource(GlobalModulator::Noise)).m_value[0] == expectedValues[9]);
        for (size_t lane = 0; lane < 4; ++lane)
        {
            DOCTEST_CHECK(manager.GetSource(GetSource(QuadModulator::Noise)).m_value[lane] == expectedValues[10 + lane]);
        }
    }
}

DOCTEST_TEST_CASE("Dual VCO morph generators own their six-second timing and wavetable visibility")
{
    synthrig::SynthRig rig;
    rig.RunSamples(1);
    auto& synth = rig.Internal().m_squiggleBoy;
    auto& vco = synth.m_tracks[0].m_source.m_dualWaveShapingVCO;
    DOCTEST_CHECK(vco.m_morphRandomInput.m_waiting.m_muSeconds == 6.0);
    DOCTEST_CHECK(vco.m_morphRandomInput.m_targetInternalSigma == doctest::Approx(0.1f));
    auto first = vco.m_morphRandom[0];
    auto second = vco.m_morphRandom[1];
    TrioDualWaveShapingVCO::Input input;
    input.m_baseFreq = TrioFloat::Splat(0.001f);
    for (size_t sample = 0; sample < 64; ++sample)
    {
        for (auto& random : synth.m_tracks[0].m_gangedRandomLFO)
        {
            random.m_output = TrioFloat::Splat(sample % 2);
        }

        vco.Process(input);
        DOCTEST_CHECK(vco.m_morphRandom[0].GetOutput() == first.Process(vco.m_morphRandomInput));
        DOCTEST_CHECK(vco.m_morphRandom[1].GetOutput() == second.Process(vco.m_morphRandomInput));
    }

    for (size_t oscillator = 0; oscillator < 2; ++oscillator)
    {
        synth.m_waveTableGenerator[oscillator].GenerateCompletely();
        synth.m_waveTableGenerator[oscillator].m_leftVisible[0] = true;
        synth.m_waveTableGenerator[oscillator].m_rightVisible[0] = true;
        vco.m_morphRandom[oscillator].m_output = TrioFloat::Splat(static_cast<float>(oscillator));
        synth.m_tracks[0].m_gangedRandomLFO[2 + oscillator].m_output = TrioFloat::Splat(1.0f - oscillator);
    }

    synth.ProcessWaveTableGenerators();
    DOCTEST_CHECK(synth.m_waveTableGenerator[0].m_leftVisible[0]);
    DOCTEST_CHECK_FALSE(synth.m_waveTableGenerator[0].m_rightVisible[0]);
    DOCTEST_CHECK_FALSE(synth.m_waveTableGenerator[1].m_leftVisible[0]);
    DOCTEST_CHECK(synth.m_waveTableGenerator[1].m_rightVisible[0]);
}

DOCTEST_TEST_CASE("Quad random LFOs publish all eight sources and each group displays its pair")
{
    synthrig::SynthRig rig;
    auto& synth = rig.Internal().m_squiggleBoy;
    auto& uiState = rig.UIState().m_squiggleBoyUIState;
    DOCTEST_REQUIRE(std::size(synth.m_quadGangedRandomLFO) == 8);
    DOCTEST_REQUIRE(std::size(uiState.m_quadGangedRandomLFOUIState) == 8);

    synth.m_encoders.SelectBank(SmartGridOneEncoders::Bank::QuadLFOs);
    for (size_t slot = 0; slot < 8; ++slot)
    {
        auto& lfo = synth.m_quadGangedRandomLFO[slot];
        for (size_t channel = 0; channel < 4; ++channel)
        {
            auto& voice = lfo.m_voices[channel];
            voice.m_state = GangedRandomLFOVoice::State::Moving;
            voice.m_source = 0.1f * static_cast<float>(slot + 1);
            voice.m_target = voice.m_source + 0.2f;
            voice.m_output = voice.m_source;
            lfo.m_voiceInputs[channel].m_movingIncrement = 0.01 * static_cast<double>(channel + 1);
            lfo.m_voiceInputs[channel].m_shape = 0.0f;
        }
    }

    rig.RunSamples(2);
    rig.Internal().PopulateUIState();
    for (size_t slot = 0; slot < 8; ++slot)
    {
        DOCTEST_CAPTURE(slot);
        const auto& sourceValue = synth.m_encoders.GetModulatorManager().GetSource(
            SmartGridOneModulators::GetSource(SmartGridOneModulators::GetQuadRandom(slot)));
        if (slot >= 6)
        {
            DOCTEST_CHECK(uiState.m_encoderBankUIState.GetModulationGlyph(0, 0, slot - 4) ==
                SmartGridOne::ModulationGlyphs::SmoothRandom);
            DOCTEST_CHECK(uiState.m_encoderBankUIState.GetModulationGlyphColor(0, 0, slot - 4) ==
                GetModulatorColor(SmartGridOneModulators::GetQuadRandom(slot)));
        }


        GangedRandomLFOSnapshot<4> snapshot;
        DOCTEST_REQUIRE(uiState.m_quadGangedRandomLFOUIState[slot].ReadSnapshot(snapshot));
        DOCTEST_CHECK(snapshot.m_sampleRate == static_cast<double>(SampleTimer::x_sampleRate));
        DOCTEST_CHECK(snapshot.m_roundElapsedSamples == 1.0);
        for (size_t channel = 0; channel < 4; ++channel)
        {
            DOCTEST_CAPTURE(channel);
            const float source = 0.1f * static_cast<float>(slot + 1);
            const float step = 0.2f * std::min(1.0f, 0.32f * static_cast<float>(channel + 1)) / 32.0f;
            DOCTEST_CHECK(sourceValue.m_value[channel] == doctest::Approx(source));
            DOCTEST_CHECK(snapshot.m_voices[channel].m_output == doctest::Approx(source + step));
        }
    }
}
