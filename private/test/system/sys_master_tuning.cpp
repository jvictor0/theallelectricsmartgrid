#include "doctest.h"
#include "support/SynthRig.hpp"

DOCTEST_TEST_CASE("Tuning: adjacent Nyquist suboctaves bracket middle C and tempo retains eight octaves")
{
    SquiggleBoyWithEncoderBank::Input input;
    DOCTEST_CHECK(SampleTimer::CycPerSampToHz(input.m_masterTune.m_expParam)
        == doctest::Approx(261.6256));
    double lowTune = SampleTimer::CycPerSampToHz(input.m_masterTune.Update(0.0f));
    double highTune = SampleTimer::CycPerSampToHz(input.m_masterTune.Update(1.0f));
    DOCTEST_CHECK(lowTune == doctest::Approx(187.5));
    DOCTEST_CHECK(highTune == doctest::Approx(375.0));
    DOCTEST_CHECK(highTune / lowTune == doctest::Approx(2.0));
    DOCTEST_CHECK(SampleTimer::CycPerSampToHz(input.m_masterTune.Update(0.5f))
        == doctest::Approx(std::sqrt(lowTune * highTune)));

    double lowTempo = SampleTimer::CycPerSampToHz(input.m_tempo.Update(0.0f));
    double highTempo = SampleTimer::CycPerSampToHz(input.m_tempo.Update(1.0f));
    DOCTEST_CHECK(lowTempo == doctest::Approx(0.011444091796875));
    DOCTEST_CHECK(highTempo == doctest::Approx(2.9296875));
    DOCTEST_CHECK(highTempo < 4.0);
    DOCTEST_CHECK(std::log2(highTempo / lowTempo) == doctest::Approx(8.0));
    DOCTEST_CHECK(std::log2(Tuning::x_nyquistHz / highTune) == doctest::Approx(6.0));
    DOCTEST_CHECK(std::log2(Tuning::x_nyquistHz / highTempo) == doctest::Approx(13.0));
}

DOCTEST_TEST_CASE("SynthRig: master tuning moves held notes in every trio and publishes sounding pitches")
{
    synthrig::SynthRig rig;
    auto& system = rig.Internal();
    auto& synth = system.m_squiggleBoy;
    auto& input = system.m_squiggleBoyState;
    rig.StopSequencer();
    rig.RunFrames(2);
    synth.m_encoders.SelectBank(SmartGridOneEncoders::Bank::TheoryOfTime);
    for (size_t voice = 0; voice < SquiggleBoy::x_numVoices; ++voice)
    {
        system.m_nonagon.m_nonagon.m_output.m_voltPerOct[voice] = static_cast<float>(voice) / 8.0f;
        system.m_nonagon.m_nonagon.m_multiPhasorGate.m_ahdControl[voice].m_trig = true;
    }

    system.SetSquiggleBoyInputs();
    for (auto& control : system.m_nonagon.m_nonagon.m_multiPhasorGate.m_ahdControl)
    {
        control.m_trig = false;
    }

    struct Setting
    {
        float m_knob;
        double m_hz;
    };

    for (const auto& setting : {Setting{0.0f, 187.5}, Setting{1.0f, 375.0},
        Setting{Tuning::x_defaultMasterTune, Tuning::x_middleCHz}})
    {
        rig.SetEncoder(0, 1, setting.m_knob);
        rig.RunFrames(24);
        DOCTEST_CHECK(SampleTimer::CycPerSampToHz(input.m_masterTune.m_expParam)
            == doctest::Approx(setting.m_hz).epsilon(0.0001));
        DOCTEST_CHECK(system.m_uiState.m_masterTuneHz.load()
            == doctest::Approx(setting.m_hz).epsilon(0.0001));
        for (size_t voice = 0; voice < SquiggleBoy::x_numVoices; ++voice)
        {
            double expected = setting.m_hz * std::exp2(static_cast<double>(voice) / 8.0);
            const auto& track = synth.m_trackState[voice / 3];
            DOCTEST_CHECK_FALSE(track.m_ahdControl[voice % 3].m_trig);
            DOCTEST_CHECK(SampleTimer::CycPerSampToHz(track.m_ampInput.m_vcoTargetFreq[voice % 3])
                == doctest::Approx(expected).epsilon(0.0001));
            DOCTEST_CHECK(SampleTimer::CycPerSampToHz(track.m_subInput.m_baseFreq[voice % 3])
                == doctest::Approx(expected).epsilon(0.0001));
            DOCTEST_CHECK(system.m_uiState.m_voiceFrequencyHz[voice].load()
                == doctest::Approx(expected).epsilon(0.0001));
        }
    }

    system.m_nonagon.m_state.m_theoryOfTimeInput.m_freq = SampleTimer::HzToCycPerSamp(0.625);
    system.PopulateUIState();
    DOCTEST_CHECK(system.m_uiState.m_tempoHz.load() == doctest::Approx(0.625));
}

DOCTEST_TEST_CASE("SynthRig: master tuning precedes DV selection and the visualizer receives the external pitch")
{
    struct Setting
    {
        float m_knob;
        double m_tuneHz;
        double m_externalHz;
    };

    for (const auto& setting : {Setting{0.0f, 187.5, 190.0}, Setting{1.0f, 375.0, 370.0}})
    {
        DOCTEST_CAPTURE(setting.m_knob);
        synthrig::SynthRig rig;
        auto& system = rig.Internal();
        auto& synth = system.m_squiggleBoy;
        auto& input = system.m_squiggleBoyState;
        auto& nonagon = system.m_nonagon.m_nonagon;
        rig.StopSequencer();
        synth.m_encoders.SelectBank(SmartGridOneEncoders::Bank::TheoryOfTime);
        rig.SetEncoder(0, 1, setting.m_knob);
        rig.RunFrames(24);

        constexpr size_t x_voice = 6;
        auto& vocoderInput = synth.m_deepVocoderState;
        vocoderInput.m_enabled = true;
        auto& voiceInput = vocoderInput.m_voiceInput[x_voice];
        voiceInput.m_gainThreshold.m_expParam = 0.1f;
        voiceInput.m_slopeUp.m_expParam = 16.0f;
        voiceInput.m_slopeDown.m_expParam = 16.0f;
        voiceInput.m_pitchRatioPre.m_expParam = 1.0f;
        for (float hz : {190.0f, 370.0f})
        {
            float frequency = static_cast<float>(SampleTimer::HzToCycPerHostSamp(hz));
            synth.m_deepVocoder.m_spectralModel.m_atoms.Add(
                SpectralModel::Atom(frequency, 0.25f, {}, frequency, 0.25f, 0));
        }

        system.m_nonagon.m_state.m_trioOctaveSwitchesInput.m_octave[2] = 1;
        nonagon.m_output.m_voltPerOct[x_voice] = 1.0f;
        auto& control = nonagon.m_multiPhasorGate.m_ahdControl[x_voice];
        control.m_trig = true;
        system.SetSquiggleBoyInputs();
        DOCTEST_REQUIRE(control.m_trig);
        DOCTEST_CHECK(SampleTimer::CycPerSampToHz(voiceInput.m_pitchCenter)
            == doctest::Approx(setting.m_tuneHz).epsilon(0.0001));
        double expected = 2.0 * setting.m_externalHz;
        DOCTEST_CHECK(SampleTimer::CycPerSampToHz(input.m_baseFreq[x_voice])
            == doctest::Approx(expected).epsilon(0.0001));

        control.m_trig = false;
        for (int sample = 0; sample < 4096; ++sample)
        {
            synth.SetEncoderParameters(input);
        }

        system.PopulateUIState();
        DOCTEST_CHECK(SampleTimer::CycPerSampToHz(synth.m_trackState[2].m_ampInput.m_vcoTargetFreq[0])
            == doctest::Approx(expected).epsilon(0.0001));
        DOCTEST_CHECK(system.m_uiState.m_voiceFrequencyHz[x_voice].load()
            == doctest::Approx(expected).epsilon(0.0001));

        // Changing the tuning center leaves the held external pitch intact;
        // the next trigger selects a partial around the new center.
        //
        input.m_masterTune.Update(1.0f - setting.m_knob);
        system.SetSquiggleBoyInputs();
        DOCTEST_CHECK(SampleTimer::CycPerSampToHz(input.m_baseFreq[x_voice])
            == doctest::Approx(expected).epsilon(0.0001));
        control.m_trig = true;
        system.SetSquiggleBoyInputs();
        DOCTEST_REQUIRE(control.m_trig);
        double nextExpected = 2.0 * (setting.m_knob == 0.0f ? 370.0 : 190.0);
        DOCTEST_CHECK(SampleTimer::CycPerSampToHz(input.m_baseFreq[x_voice])
            == doctest::Approx(nextExpected).epsilon(0.0001));
    }
}

DOCTEST_TEST_CASE("SynthRig: master tuning survives patch save and load and resets to middle C")
{
    synthrig::SynthRig rig;
    auto& encoders = rig.Internal().m_squiggleBoy.m_encoders;
    encoders.SelectBank(SmartGridOneEncoders::Bank::TheoryOfTime);
    rig.RunFrames(2);
    rig.SetEncoder(0, 1, 0.75f);
    rig.RunFrames(24);
    std::string patch = rig.SavePatch();
    DOCTEST_REQUIRE(patch.find("\"MasterTune\"") != std::string::npos);
    rig.SetEncoder(0, 1, 0.0f);
    rig.RunFrames(24);
    DOCTEST_REQUIRE(rig.LoadPatch(patch));
    rig.RunFrames(24);
    DOCTEST_CHECK(encoders.GetValue(SmartGridOneEncoders::Param::MasterTune)
        == doctest::Approx(0.75f).epsilon(0.0001));
    rig.ResetToDefaults();
    rig.RunFrames(24);
    DOCTEST_CHECK(rig.Internal().m_uiState.m_masterTuneHz.load()
        == doctest::Approx(261.6256).epsilon(0.0001));
}
