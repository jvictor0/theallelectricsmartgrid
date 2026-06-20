#include "doctest.h"

#include "../support/GlobalEnv.hpp"

#include "AudioInputBuffer.hpp"
#include "SceneManager.hpp"
#include "SquiggleBoy.hpp"
#include "TheoryOfTime.hpp"

#include <memory>

DOCTEST_TEST_CASE("SquiggleBoy partial machine azimuth offset follows pan phase")
{
    GlobalEnv::ResetPerTest();

    SmartGrid::SceneManager sceneManager;
    TheoryOfTime theoryOfTime;
    auto squiggleBoy = std::make_unique<SquiggleBoyWithEncoderBank>();
    SquiggleBoyWithEncoderBank::Input input;
    AudioInputBuffer audioInput;

    squiggleBoy->m_theoryOfTime = &theoryOfTime;
    squiggleBoy->Init(&sceneManager);
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
