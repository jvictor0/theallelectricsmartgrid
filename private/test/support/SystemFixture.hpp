#pragma once

// SystemFixture: process-wide singleton owner for the one
// TheNonagonSquiggleBoyInternal instance the test binary is allowed to build.
//
// The global grid-id allocator (SmartGrid::g_gridIds) does not support repeated
// construction of the whole-system object, so every test that needs a live
// system shares a single instance via SharedSystem(). The instance is created
// lazily on first use (after GlobalEnv::ResetPerTest has reset the SampleTimer).
//
// Tests that mutate the shared system (e.g. pushing encoder messages) should
// treat it as shared, not pristine: assert on relative changes / specific cells
// rather than assuming a clean slate.

#include <memory>

#include "../support/GlobalEnv.hpp"

#include "AudioInputBuffer.hpp"
#include "TheNonagonSquiggleBoy.hpp"

namespace SystemFixture
{

// Returns the process-wide singleton system instance, constructing it on first
// call. GlobalEnv::ResetPerTest() should be called before the first use to
// ensure deterministic timing/RNG state at construction.
inline TheNonagonSquiggleBoyInternal& SharedSystem()
{
    static std::unique_ptr<TheNonagonSquiggleBoyInternal> s_instance =
        std::make_unique<TheNonagonSquiggleBoyInternal>();
    return *s_instance;
}

// Drive a single full process frame's worth of samples through the system,
// mirroring the host's ProcessFrame/ProcessSample cadence
// (NonagonWrapper::Process). This advances compute and (re)populates UIState.
inline void RunFrame(TheNonagonSquiggleBoyInternal& system)
{
    AudioInputBuffer audioIn;  // zeroed input
    audioIn.m_numInputs = 0;

    const int numSamples = static_cast<int>(SampleTimer::x_samplesPerProcessFrame);
    for (int i = 0; i < numSamples; ++i)
    {
        if (SampleTimer::IncrementSample())
        {
            system.ProcessFrame();
        }

        system.ProcessSample(audioIn);
    }
}

} // namespace SystemFixture
