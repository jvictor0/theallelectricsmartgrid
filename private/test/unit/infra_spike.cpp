// WP-0 infrastructure spike.
//
// Proves the standalone, JUCE-free test target can compile and instantiate
// both basic DSP headers and the whole-system header TheNonagonSquiggleBoy.hpp.
//
// NOTE: the test target defines DOCTEST_CONFIG_NO_SHORT_MACRO_NAMES (so doctest
// does not collide with the codebase's INFO/WARN macros), hence the DOCTEST_
// prefixed test macros below.

#include "doctest.h"

#include <cmath>
#include <memory>

#include "../support/GlobalEnv.hpp"
#include "../support/SystemFixture.hpp"

#include "Filter.hpp"
#include "TheoryOfTime.hpp"
#include "AudioInputBuffer.hpp"
#include "QuadMasterChain.hpp"
#include "TheNonagonSquiggleBoy.hpp"

DOCTEST_TEST_CASE("trivial assertion passes")
{
    DOCTEST_CHECK(1 + 1 == 2);
}

DOCTEST_TEST_CASE("OPLowPassFilter constructs and processes samples")
{
    GlobalEnv::ResetPerTest();

    OPLowPassFilter filter;
    filter.SetAlphaFromNatFreq(0.1f);

    // Feed a step input; the filtered output should settle and stay finite.
    float out = 0.0f;
    for (int i = 0; i < 64; ++i)
    {
        out = filter.Process(1.0f);
        DOCTEST_CHECK(std::isfinite(out));
    }

    // A low-pass fed a constant 1.0 should approach 1.0.
    DOCTEST_CHECK(out > 0.5f);
    DOCTEST_CHECK(out <= 1.0f + 1e-3f);
}

DOCTEST_TEST_CASE("TheoryOfTime constructs")
{
    GlobalEnv::ResetPerTest();

    TheoryOfTime tot;
    DOCTEST_CHECK(tot.m_masterLoopSamples == doctest::Approx(1.0));
}

DOCTEST_TEST_CASE("TheNonagonSquiggleBoyInternal constructs and processes samples")
{
    // NOTE: There is exactly ONE TheNonagonSquiggleBoyInternal in the whole test
    // binary, owned by SystemFixture::SharedSystem() (the global grid-id
    // allocator SmartGrid::g_gridIds may not support repeated construction).
    // This and the EncoderSet tests share that singleton.
    //
    // Construction does NOT require an IoTaskThread: all m_ioTaskThread pointers
    // default to nullptr and are only dereferenced during recording, which we do
    // not trigger. Mirrors the minimal single-threaded ProcessFrame/ProcessSample
    // sequence from JUCE/SmartGridOne/Source/NonagonWrapper.hpp::Process.

    GlobalEnv::ResetPerTest();

    TheNonagonSquiggleBoyInternal* nonagon = &SystemFixture::SharedSystem();
    DOCTEST_REQUIRE(nonagon != nullptr);

    AudioInputBuffer audioIn;  // zeroed input
    audioIn.m_numInputs = 0;

    bool sawFiniteOutput = false;

    // Drive a couple of process frames worth of samples. SampleTimer::IncrementSample
    // returns true at frame boundaries, at which point ProcessFrame is run -- the
    // same cadence the host uses.
    const int numSamples = 2 * static_cast<int>(SampleTimer::x_samplesPerProcessFrame) + 16;
    for (int i = 0; i < numSamples; ++i)
    {
        if (SampleTimer::IncrementSample())
        {
            nonagon->ProcessFrame();
        }

        QuadFloatWithStereoAndSub output = nonagon->ProcessSample(audioIn);

        for (int c = 0; c < 4; ++c)
        {
            DOCTEST_CHECK(std::isfinite(output.m_output[c]));
        }
        DOCTEST_CHECK(std::isfinite(output.m_stereoOutput[0]));
        DOCTEST_CHECK(std::isfinite(output.m_stereoOutput[1]));
        DOCTEST_CHECK(std::isfinite(output.m_sub));
        sawFiniteOutput = true;
    }

    DOCTEST_CHECK(sawFiniteOutput);
}
