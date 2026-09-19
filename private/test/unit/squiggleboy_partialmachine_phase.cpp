#include "doctest.h"

#include "../support/GlobalEnv.hpp"

#include "AudioInputBuffer.hpp"
#include "SceneManager.hpp"
#include "SquiggleBoy.hpp"
#include "TheoryOfTime.hpp"

#include <memory>

DOCTEST_TEST_CASE("SquiggleBoy effect returns reach unity and reverb covers the full gain curve")
{
    GlobalEnv::ResetPerTest();
    SmartGrid::SceneManager sceneManager;
    TheoryOfTime theoryOfTime;
    auto synth = std::make_unique<SquiggleBoyWithEncoderBank>(&sceneManager);
    SquiggleBoyWithEncoderBank::Input input;
    synth->m_theoryOfTime = &theoryOfTime;
    synth->Config(input);
    synth->m_encoders.Process();
    synth->SetEncoderParameters(input);

    for (size_t effect = 0; effect < 3; ++effect)
    {
        DOCTEST_CAPTURE(effect);
        DOCTEST_CHECK(synth->m_mixerState.m_returnGain[effect].m_expParam == doctest::Approx(1.0f));
    }

    auto* reverbReturn = synth->m_encoders.m_encoderBankBank.GetEncoder(static_cast<size_t>(SmartGridOneEncoders::Param::ReverbReturn));
    for (float value : {0.5f, 0.0f})
    {
        reverbReturn->SetValueAllScenesAllTracks(value);
        reverbReturn->SetForceUpdateRecursive();
        for (size_t frame = 0; frame < 512; ++frame)
        {
            synth->m_encoders.Process();
            synth->SetEncoderParameters(input);
        }

        float expectedGain = value == 0.0f ? 0.0f : 0.1827439976f;
        DOCTEST_CHECK(std::abs(synth->m_mixerState.m_returnGain[1].m_expParam - expectedGain) < 1e-6f);
    }
}

DOCTEST_TEST_CASE("SquiggleBoy partial machine azimuth offset follows pan phase")
{
    GlobalEnv::ResetPerTest();

    SmartGrid::SceneManager sceneManager;
    TheoryOfTime theoryOfTime;
    auto squiggleBoy = std::make_unique<SquiggleBoyWithEncoderBank>(&sceneManager);
    SquiggleBoyWithEncoderBank::Input input;
    AudioInputBuffer audioInput;

    squiggleBoy->m_theoryOfTime = &theoryOfTime;
    squiggleBoy->Config(input);

    for (size_t i = 0; i < 4096; ++i)
    {
        squiggleBoy->ProcessSample(input, 1.0f / 48000.0f, audioInput);
    }

    float panPhase = squiggleBoy->m_panPhase.m_phase;

    DOCTEST_REQUIRE(panPhase > 0.001f);
    DOCTEST_CHECK(squiggleBoy->m_state[0].m_panInput.m_input == doctest::Approx(panPhase).epsilon(1e-6));
    DOCTEST_CHECK(squiggleBoy->m_partialMachineState.m_synthesisContextInput.m_azimuthOffset == doctest::Approx(panPhase).epsilon(1e-6));
}
