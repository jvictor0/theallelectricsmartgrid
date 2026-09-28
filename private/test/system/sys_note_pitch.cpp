#include "doctest.h"
#include "support/SynthRig.hpp"

DOCTEST_TEST_CASE("SynthRig: bypassed vocoder hands the current note to the synth before an FFT hop")
{
    synthrig::SynthRig rig;
    auto& internal = rig.Internal();
    auto& nonagon = internal.m_nonagon.m_nonagon;
    auto& vocoder = internal.m_squiggleBoy.m_deepVocoder;
    auto& input = internal.m_squiggleBoy.m_deepVocoderState;
    constexpr size_t x_voice = 6;
    input.m_enabled = false;

    struct Note
    {
        float m_pitch;
        float m_expectedHz;
        int m_octave;
        size_t m_samplesAfter;
    };

    const Note notes[] =
    {
        {0.0f, 261.625565f, 0, DeepVocoder::x_H},
        {0.5f, 369.994423f, 0, 1},
        {1.0f, 523.251131f, 1, 0},
        {0.25f, 311.126984f, -1, 0}
    };
    for (const auto& note : notes)
    {
        DOCTEST_CAPTURE(note.m_pitch);
        internal.m_nonagon.m_state.m_trioOctaveSwitchesInput.m_octave[2] = note.m_octave;
        nonagon.m_output.m_voltPerOct[x_voice] = note.m_pitch;
        nonagon.m_multiPhasorGate.m_ahdControl[x_voice].m_trig = true;

        internal.SetSquiggleBoyInputs();

        DOCTEST_CHECK(nonagon.m_multiPhasorGate.m_ahdControl[x_voice].m_trig);
        DOCTEST_CHECK(internal.m_squiggleBoyState.m_baseFreq[x_voice] * 48000.0f
            == doctest::Approx(note.m_expectedHz));
        for (size_t sample = 0; sample < note.m_samplesAfter; ++sample)
        {
            vocoder.Process(0.0f, input);
        }
    }
}

DOCTEST_TEST_CASE("SynthRig: enabled vocoder selects partials with the current note and ratios before an FFT hop")
{
    synthrig::SynthRig rig;
    auto& internal = rig.Internal();
    auto& nonagon = internal.m_nonagon.m_nonagon;
    auto& vocoder = internal.m_squiggleBoy.m_deepVocoder;
    auto& input = internal.m_squiggleBoy.m_deepVocoderState;
    constexpr size_t x_voice = 6;
    input.m_enabled = true;
    auto& voice = input.m_voiceInput[x_voice];
    voice.m_gainThreshold.m_expParam = 0.1f;
    voice.m_slopeUp.m_expParam = 16.0f;
    voice.m_slopeDown.m_expParam = 16.0f;
    voice.m_pitchRatioPre.m_expParam = 1.0f;
    vocoder.Process(0.0f, input);
    for (float hz : {261.625565f, 523.251131f})
    {
        float frequency = hz / 48000.0f;
        vocoder.m_spectralModel.m_atoms.Add(
            SpectralModel::Atom(frequency, 0.25f, {}, frequency, 0.25f, 0));
    }

    struct Note
    {
        float m_pitch;
        float m_ratioPre;
        int m_octave;
        float m_expectedHz;
    };

    const Note notes[] =
    {
        {0.0f, 1.0f, 0, 261.625565f},
        {1.0f, 1.0f, 0, 523.251131f},
        {0.0f, 2.0f, 0, 523.251131f},
        {1.0f, 2.0f, 1, 1046.502262f}
    };
    for (const auto& note : notes)
    {
        DOCTEST_CAPTURE(note.m_pitch);
        DOCTEST_CAPTURE(note.m_ratioPre);
        DOCTEST_CAPTURE(note.m_octave);
        voice.m_pitchRatioPre.m_expParam = note.m_ratioPre;
        internal.m_nonagon.m_state.m_trioOctaveSwitchesInput.m_octave[2] = note.m_octave;
        nonagon.m_output.m_voltPerOct[x_voice] = note.m_pitch;
        nonagon.m_multiPhasorGate.m_ahdControl[x_voice].m_trig = true;

        internal.SetSquiggleBoyInputs();

        DOCTEST_CHECK(nonagon.m_multiPhasorGate.m_ahdControl[x_voice].m_trig);
        DOCTEST_CHECK(internal.m_squiggleBoyState.m_baseFreq[x_voice] * 48000.0f
            == doctest::Approx(note.m_expectedHz));
    }

    voice.m_gainThreshold.m_expParam = 1.0f;
    nonagon.m_multiPhasorGate.m_ahdControl[x_voice].m_trig = true;
    internal.SetSquiggleBoyInputs();
    DOCTEST_CHECK_FALSE(nonagon.m_multiPhasorGate.m_ahdControl[x_voice].m_trig);
    DOCTEST_CHECK(internal.m_squiggleBoyState.m_baseFreq[x_voice] * 48000.0f
        == doctest::Approx(1046.502262f));
}

DOCTEST_TEST_CASE("SynthRig: enabled vocoder without partials suppresses the note and preserves the synth target")
{
    synthrig::SynthRig rig;
    auto& internal = rig.Internal();
    auto& nonagon = internal.m_nonagon.m_nonagon;
    auto& input = internal.m_squiggleBoy.m_deepVocoderState;
    constexpr size_t x_voice = 6;
    input.m_enabled = true;
    internal.m_squiggleBoy.m_deepVocoder.Process(0.0f, input);
    internal.m_squiggleBoyState.m_baseFreq[x_voice] = 440.0f / 48000.0f;
    nonagon.m_output.m_voltPerOct[x_voice] = 1.0f;
    nonagon.m_multiPhasorGate.m_ahdControl[x_voice].m_trig = true;

    internal.SetSquiggleBoyInputs();

    DOCTEST_CHECK_FALSE(nonagon.m_multiPhasorGate.m_ahdControl[x_voice].m_trig);
    DOCTEST_CHECK(internal.m_squiggleBoyState.m_baseFreq[x_voice] * 48000.0f
        == doctest::Approx(440.0f));
}
