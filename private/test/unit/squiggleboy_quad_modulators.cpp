#include "doctest.h"

#include <iterator>

#include "../support/SynthRig.hpp"

DOCTEST_TEST_CASE("Quad random LFOs use the same timing presets as voices")
{
    synthrig::SynthRig rig;
    auto& synth = rig.Internal().m_squiggleBoy;
    constexpr double x_waitingSeconds[] = { 1.0, 4.0, 12.0, 32.0 };

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

        const double waitingIncrement = 1.0 / (48000.0 * x_waitingSeconds[slot]);
        const double movingIncrement = 2.0 / (48000.0 * x_waitingSeconds[slot]);
        DOCTEST_CHECK(voice.m_voices[0].m_waitingIncrement == doctest::Approx(waitingIncrement).epsilon(1e-10));
        DOCTEST_CHECK(voice.m_voices[0].m_movingIncrement == doctest::Approx(movingIncrement).epsilon(1e-10));
        for (const auto& lane : quad.m_voices)
        {
            DOCTEST_CHECK(lane.m_waitingIncrement == doctest::Approx(waitingIncrement).epsilon(1e-10));
            DOCTEST_CHECK(lane.m_movingIncrement == doctest::Approx(movingIncrement).epsilon(1e-10));
        }
    }
}

DOCTEST_TEST_CASE("Quad random LFOs drive and display all four modulation slots")
{
    synthrig::SynthRig rig;
    auto& synth = rig.Internal().m_squiggleBoy;
    auto& uiState = rig.UIState().m_squiggleBoyUIState;
    DOCTEST_REQUIRE(std::size(synth.m_quadGangedRandomLFO) == 4);
    DOCTEST_REQUIRE(std::size(uiState.m_quadGangedRandomLFOUIState) == 4);

    synth.m_encoders.SelectBank(SmartGridOneEncoders::Bank::QuadLFOs);
    for (size_t slot = 0; slot < 4; ++slot)
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
    const auto& values = synth.m_encoders.GetModulatorValues(SmartGridOneEncoders::BankMode::Quad);
    for (size_t slot = 0; slot < 4; ++slot)
    {
        DOCTEST_CAPTURE(slot);
        DOCTEST_CHECK(uiState.m_encoderBankUIState.GetModulationGlyph(slot) ==
            SmartGridOne::ModulationGlyphs::SmoothRandom);
        DOCTEST_CHECK(uiState.m_encoderBankUIState.GetModulationGlyphColor(slot) ==
            SmartGridOneEncoders::GetModulatorSkin(slot, SmartGridOneEncoders::BankMode::Voice).m_color);

        GangedRandomLFOSnapshot<4> snapshot;
        DOCTEST_REQUIRE(uiState.m_quadGangedRandomLFOUIState[slot].ReadSnapshot(snapshot));
        DOCTEST_CHECK(snapshot.m_sampleRate == 48000.0);
        DOCTEST_CHECK(snapshot.m_roundElapsedSamples == 2.0);
        for (size_t channel = 0; channel < 4; ++channel)
        {
            DOCTEST_CAPTURE(channel);
            const float source = 0.1f * static_cast<float>(slot + 1);
            const float step = 0.002f * static_cast<float>(channel + 1);
            DOCTEST_CHECK(values.m_value[slot][channel] == doctest::Approx(source + step));
            DOCTEST_CHECK(snapshot.m_voices[channel].m_output == doctest::Approx(source + 2.0f * step));
        }
    }
}
