#include "doctest.h"
#include "../support/SynthRig.hpp"
#include "Saturator.hpp"

DOCTEST_TEST_CASE("LFO scopes: UI publication refreshes all voice, quad and warp axes")
{
    synthrig::SynthRig rig;
    rig.RunFrames(1);
    // RunFrames can finish after the last publication. Publish again without
    // advancing the clock so the snapshot and expected axis share a sample.
    //
    rig.Internal().ProcessFrame();
    auto& synth = rig.Internal().m_squiggleBoy;
    auto& state = rig.UIState().m_squiggleBoyUIState;
    auto& time = *synth.m_theoryOfTime;
    state.AcceptScopeSamples();
    auto checkLFO = [&](const auto& lfo)
    {
        auto periods = lfo.m_polyXFader.ComputeLogicalPeriods(time);
        const auto& holder = lfo.m_scopeWriter;
        for (size_t lane = 0; lane < holder.m_scopeWriter->m_voices; ++lane)
        {
            const auto& markers = holder.m_scopeWriter->GetAcceptedMarkers(holder.m_scopeIx, lane);
            DOCTEST_CHECK(markers.m_periodic.m_period == periods[lane]);
            DOCTEST_CHECK(markers.m_periodic.m_origin == time.m_phaseClock.m_offset);
            DOCTEST_CHECK(markers.m_startIndexIndex == 0);
        }
    };
    for (const auto& track : synth.m_tracks)
    {
        for (const auto& lfo : track.m_squiggleLFO)
        {
            checkLFO(lfo);
        }
    }

    for (const auto& lfo : synth.m_quadSquiggleLFO)
    {
        checkLFO(lfo);
    }

    const auto& warp = state.m_monoScopeWriter.GetAcceptedMarkers(time.m_scopeWriter.m_scopeIx, 0);
    DOCTEST_CHECK(warp.m_periodic.m_period == time.m_phaseModLFO.ComputeLogicalPeriods(time)[0]);
    DOCTEST_CHECK(warp.m_periodic.m_origin == time.m_unmodulatedPhaseClock.m_offset);
    DOCTEST_CHECK(warp.m_startIndexIndex == 0);
}

DOCTEST_TEST_CASE("Quad scopes: all effects follow return gain and saturation before mastering")
{
    synthrig::SynthRig rig;
    auto& synth = rig.Internal().m_squiggleBoy;
    auto& input = synth.m_mixerState;
    auto& writer = rig.UIState().m_squiggleBoyUIState.m_quadScopeWriter;
    for (auto& track : input.m_tracks)
    {
        track.m_input = TrioFloat();
        track.m_sub = TrioFloat();
    }

    for (auto& external : input.m_externals)
    {
        external.m_input = StereoFloat();
    }

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

        // Every engine sample is observable, including the mastered signal
        // before output decimation and the stereo mixdown before mastering.
        //
        const size_t index = writer.m_index;
        for (size_t sub = 0; sub < writer.m_decimation; ++sub)
        {
            synth.m_output = synth.m_mixer.Process(input);
            synth.WriteQuadScopes();
            writer.AdvanceIndex();
        }

        writer.Publish();

        writer.AcceptSamples();
        for (size_t speaker = 0; speaker < 4; ++speaker)
        {
            DOCTEST_CHECK(writer.ReadSample(static_cast<size_t>(SmartGridOne::QuadScopes::Master), speaker, index)
                == doctest::Approx(synth.m_mixer.m_engineOutput.m_output[speaker]));
            const QuadFloat stereo = synth.m_mixer.m_engineOutput.m_stereoOutput.CombineToQuad(
                synth.m_mixer.m_quadToStereoMixdown.m_output);
            DOCTEST_CHECK(writer.ReadSample(static_cast<size_t>(SmartGridOne::QuadScopes::Stereo), speaker, index)
                == doctest::Approx(stereo[speaker]));
        }

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
