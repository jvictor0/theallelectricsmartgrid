#include "doctest.h"

#include "../support/SynthRig.hpp"

#include <cmath>

DOCTEST_TEST_CASE("Sheafy modulators: independent voices and axes glide at the input slew cutoff")
{
    synthrig::SynthRig rig;
    auto& system = rig.Internal();
    auto& targets = system.m_nonagon.m_nonagon.m_output.m_extraTimbre;
    auto& outputs = system.m_squiggleBoyState.m_sheafyModulators;
    constexpr size_t x_stepSamples = SampleTimer::x_sampleRate / 1000;

    system.SetSquiggleBoyInputs();
    for (size_t voice = 0; voice < SquiggleBoy::x_numVoices; ++voice)
    {
        for (size_t axis = 0; axis < 3; ++axis)
        {
            DOCTEST_CHECK(outputs[voice][axis] == 0.0f);
        }
    }

    for (size_t step = 0; step < 2; ++step)
    {
        float initial[SquiggleBoy::x_numVoices][3];
        for (size_t voice = 0; voice < SquiggleBoy::x_numVoices; ++voice)
        {
            for (size_t axis = 0; axis < 3; ++axis)
            {
                initial[voice][axis] = outputs[voice][axis];
                float target = static_cast<float>(voice * 3 + axis) /
                    static_cast<float>(SquiggleBoy::x_numVoices * 3 - 1);
                targets[voice][axis] = step == 0 ? target : 1.0f - target;
            }
        }

        for (size_t sample = 1; sample <= x_stepSamples; ++sample)
        {
            system.SetSquiggleBoyInputs();
            if (sample == 1 || sample == x_stepSamples)
            {
                double decay = std::exp(-2.0 * M_PI *
                    TheNonagonSquiggleBoyInternal::x_inputSlewCutoff * static_cast<double>(sample));
                for (size_t voice = 0; voice < SquiggleBoy::x_numVoices; ++voice)
                {
                    for (size_t axis = 0; axis < 3; ++axis)
                    {
                        double expected = targets[voice][axis] +
                            (initial[voice][axis] - targets[voice][axis]) * decay;
                        DOCTEST_CHECK(outputs[voice][axis] == doctest::Approx(expected).epsilon(2.0e-6));
                    }
                }
            }
        }
    }
}
