#include "doctest.h"
#include "../support/SynthRig.hpp"

DOCTEST_TEST_CASE("Quad scopes: all effects follow return gain and saturation before mastering")
{
    synthrig::SynthRig rig;
    auto& synth = rig.Internal().m_squiggleBoy;
    auto& input = synth.m_mixerState;
    auto& writer = rig.UIState().m_squiggleBoyUIState.m_quadScopeWriter;
    input.m_numInputs = 0;
    input.m_numMonoInputs = 0;
    input.m_masterChainInput.SetMasterGain(0.05f);
    const SmartGridOne::QuadScopes x_scopes[] =
    {
        SmartGridOne::QuadScopes::Delay,
        SmartGridOne::QuadScopes::Reverb,
        SmartGridOne::QuadScopes::PartialMachine
    };
    const float x_gains[] = {0.0f, 0.25f, 4.0f};
    for (size_t sample = 0; sample < 5; ++sample)
    {
        DOCTEST_CAPTURE(sample);
        input.m_noiseMode = sample == 3;
        for (size_t effect = 0; effect < QuadMixerInternal::x_numSends; ++effect)
        {
            const float level = static_cast<float>(effect + 1);
            input.m_return[effect] = QuadFloat(0.2f, -0.4f, 0.6f, -0.8f) * level;
            input.m_returnGain[effect].m_expParam = x_gains[(sample + effect) % 3];
        }

        const size_t index = writer.m_index;
        synth.m_output = synth.m_mixer.Process(input);
        synth.WriteQuadScopes();
        writer.AdvanceIndex();
        writer.Publish();
        for (size_t effect = 0; effect < QuadMixerInternal::x_numSends; ++effect)
        {
            DOCTEST_CAPTURE(effect);
            for (size_t speaker = 0; speaker < 4; ++speaker)
            {
                const float expected = input.m_noiseMode ? 0.0f : TanhSaturator<false>::Tanh(
                    input.m_return[effect][speaker] * input.m_returnGain[effect].m_expParam);
                DOCTEST_CHECK(writer.ReadSample(static_cast<size_t>(x_scopes[effect]), speaker, index)
                    == doctest::Approx(expected));
            }
        }
    }
}
