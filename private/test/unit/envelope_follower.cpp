#include "doctest.h"
#include "../support/SynthRig.hpp"
#include "EnvelopeFollower.hpp"

namespace
{
using namespace SmartGridOneModulators;

constexpr QuadModulator x_envelopes[] =
{
    QuadModulator::DelayDryEnvelope, QuadModulator::ReverbDryEnvelope, QuadModulator::PartialMachineDryEnvelope,
    QuadModulator::DelayWetEnvelope, QuadModulator::ReverbWetEnvelope, QuadModulator::PartialMachineWetEnvelope,
    QuadModulator::OutputEnvelope
};
constexpr QuadModulator x_audio[] =
{
    QuadModulator::DelayDryAudio, QuadModulator::ReverbDryAudio, QuadModulator::PartialMachineDryAudio,
    QuadModulator::DelayWetAudio, QuadModulator::ReverbWetAudio, QuadModulator::PartialMachineWetAudio,
    QuadModulator::OutputAudio
};

template<size_t Size>
void CheckFollower()
{
    EnvelopeFollowerGeneric<Size> follower;
    using Vector = typename EnvelopeFollowerGeneric<Size>::Vector;
    auto writer = std::make_unique<ScopeWriterGeneric<Size, 2>>(1, 512);
    follower.m_audioScopeWriter = {writer.get(), 0};
    follower.m_envelopeScopeWriter = {writer.get(), 1};
    for (float seconds : {0.01f, 0.025f})
    {
        GlobalEnv::ResetPerTest();
        if (seconds != EnvelopeFollowerGeneric<Size>::x_defaultTimeConstantSeconds)
        {
            follower.SetTimeConstantSeconds(seconds);
        }

        follower.m_envelope = Vector::Splat(0);
        follower.m_meanSquare = Vector::Splat(0);
        const size_t duration = static_cast<size_t>(SampleTimer::SecondsToSamples(seconds));
        for (size_t sample = 0; sample < 2 * duration; ++sample)
        {
            Vector input;
            for (size_t lane = 0; lane < Size; ++lane)
            {
                input[lane] = sample < duration ? (sample % 2 ? -1.0f : 1.0f) * (lane + 1) : 0.0f;
            }

            Vector previousEnvelope = follower.m_envelope;
            follower.Process(input);
            for (size_t lane = 0; lane < Size; ++lane)
            {
                double time = (sample + 1) * SampleTimer::x_samplePeriod / seconds;
                double attack = std::min(sample + 1, duration) * SampleTimer::x_samplePeriod / seconds;
                double expected = (lane + 1) * (lane + 1) * (1.0 - std::exp(-attack)) * std::exp(attack - time);
                DOCTEST_CHECK(follower.m_meanSquare[lane] == doctest::Approx(expected).epsilon(0.00003));
                if (SampleTimer::IsHostFrame())
                {
                    DOCTEST_CHECK(follower.m_envelope[lane] == doctest::Approx(std::sqrt(expected)).epsilon(0.00003));
                }
                else
                {
                    DOCTEST_CHECK(follower.m_envelope[lane] == previousEnvelope[lane]);
                }

                DOCTEST_CHECK(writer->ReadSample(0, lane, writer->m_index) == input[lane]);
                DOCTEST_CHECK(writer->ReadSample(1, lane, writer->m_index) == follower.m_envelope[lane]);
            }

            writer->AdvanceIndex();
            SampleTimer::IncrementSample();
        }
    }
}
}

DOCTEST_TEST_CASE("Mixer followers: power slews at engine rate and RMS updates only on host frames")
{
    CheckFollower<1>();
    CheckFollower<3>();
    CheckFollower<4>();
}

DOCTEST_TEST_CASE("Mixer followers: sends include feedback and wet taps precede gain while final tap matches recording")
{
    GlobalEnv::ResetPerTest();
    SmartGridOneContext context;
    auto mixer = std::make_unique<QuadMixerInternal>(&context);
    QuadMixerInternal::Input input;
    input.m_masterChainInput.SetMasterGain(0.05f);
    auto& track = input.m_tracks[0];
    track.m_input = TrioFloat(0.2f, -0.4f, 0.7f);
    track.m_gain.m_expParam = TrioFloat::Splat(1.0f);
    track.m_x = TrioFloat(0.0f, 0.5f, 1.0f);
    track.m_y = TrioFloat(0.3f, 0.7f, 1.0f);
    for (size_t effect = 0; effect < 3; ++effect)
    {
        track.m_sendGain[effect].m_expParam = TrioFloat::Splat(0.1f * (effect + 1));
        input.m_returnGain[effect].m_expParam = 0.0f;
        input.m_returnSendGain[(effect + 1) % 3][effect].m_expParam = 0.25f;
    }

    QuadMixerInternal::QuadDecimator recordingReference;
    bool sawOutput = false;
    for (size_t sample = 0; sample < 256; ++sample)
    {
        for (size_t effect = 0; effect < 3; ++effect)
        {
            input.m_return[effect] = QuadFloat(0.2f, -0.3f, 0.4f, -0.5f) * (effect + 1) * (sample % 2 ? 1.0f : -1.0f);
        }

        mixer->Process(input);
        for (size_t effect = 0; effect < 3; ++effect)
        {
            QuadFloat send = input.m_return[(effect + 1) % 3] * 0.25f;
            for (size_t lane = 0; lane < 3; ++lane)
            {
                send += QuadFloat::Pan(track.m_x[lane], track.m_y[lane], 1.0f)
                    * track.m_input[lane] * track.m_sendGain[effect].m_expParam[lane];
            }

            for (size_t lane = 0; lane < 4; ++lane)
            {
                DOCTEST_CHECK(mixer->m_envelopeFollowers[effect].m_audio[lane] == doctest::Approx(send[lane]));
                DOCTEST_CHECK(mixer->m_envelopeFollowers[effect + 3].m_audio[lane] == input.m_return[effect][lane]);
                DOCTEST_CHECK(mixer->m_envelopeFollowers[effect + 3].m_envelope[lane] > 0.0f);
                DOCTEST_CHECK(mixer->m_returnOutput[effect][lane] == 0.0f);
            }
        }

        const auto& output = mixer->m_envelopeFollowers[6];
        QuadFloat recorded = recordingReference.Process(output.m_audio);
        for (size_t lane = 0; lane < 4; ++lane)
        {
            sawOutput |= std::abs(output.m_audio[lane]) > 0.00001f;
            DOCTEST_CHECK(output.m_audio[lane] == mixer->m_engineOutput.m_output[lane] * QuadMixerInternal::x_recordingGain);
            DOCTEST_CHECK(recorded[lane] == doctest::Approx(mixer->m_output.m_output[lane] * QuadMixerInternal::x_recordingGain));
        }

        SampleTimer::IncrementSample();
    }

    DOCTEST_CHECK(sawOutput);
}

DOCTEST_TEST_CASE("Mixer followers: Quad groups bind dry wet and shared final signals with four empty LFO slots")
{
    constexpr Group x_effects[] = {Group::QuadDelay, Group::QuadReverb, Group::QuadPartialMachine};
    for (size_t effect = 0; effect < 3; ++effect)
    {
        auto group = static_cast<size_t>(x_effects[effect]);
        DOCTEST_CHECK(x_groups[group][0] == GetSource(x_envelopes[effect]));
        DOCTEST_CHECK(x_groups[group][1] == GetSource(x_audio[effect]));
        DOCTEST_CHECK(x_groups[group][4] == GetSource(x_envelopes[effect + 3]));
        DOCTEST_CHECK(x_groups[group][5] == GetSource(x_audio[effect + 3]));
    }

    for (Group group : {Group::QuadDelay, Group::QuadReverb, Group::QuadPartialMachine, Group::QuadLFO})
    {
        DOCTEST_CHECK(x_groups[static_cast<size_t>(group)][12] == GetSource(QuadModulator::OutputEnvelope));
        DOCTEST_CHECK(x_groups[static_cast<size_t>(group)][13] == GetSource(QuadModulator::OutputAudio));
    }

    for (size_t slot : {0, 1, 4, 5})
    {
        DOCTEST_CHECK(x_groups[static_cast<size_t>(Group::QuadLFO)][slot] == Source::NumSources);
    }
}

DOCTEST_TEST_CASE("Mixer followers: full synth publishes causal unclipped sources and matching scope histories")
{
    synthrig::SynthRig rig;
    auto& synth = rig.Internal().m_squiggleBoy;
    auto& ui = rig.UIState();
    auto& state = ui.m_squiggleBoyUIState;
    auto& manager = synth.m_encoders.GetModulatorManager();
    for (size_t tap = 0; tap < 7; ++tap)
    {
        auto& follower = synth.m_mixer.m_envelopeFollowers[tap];
        follower.m_audio = QuadFloat(-2.0f, 0.0f, 0.5f, 3.0f);
        follower.m_envelope = QuadFloat(1.1f, 0.3f, 0.7f, 2.0f);
    }

    for (size_t sample = 0; sample < 32; ++sample)
    {
        QuadFloat previousAudio[7], previousEnvelope[7];
        for (size_t tap = 0; tap < 7; ++tap)
        {
            previousAudio[tap] = synth.m_mixer.m_envelopeFollowers[tap].m_audio * 0.5f + 0.5f;
            previousEnvelope[tap] = synth.m_mixer.m_envelopeFollowers[tap].m_envelope;
        }

        size_t index = state.m_quadScopeWriter.m_index;
        rig.RunSamples(1);
        state.m_quadScopeWriter.Publish();
        state.m_voiceScopeWriter.Publish();
        state.AcceptScopeSamples();
        for (size_t tap = 0; tap < 7; ++tap)
        {
            for (auto source : {x_envelopes[tap], x_audio[tap]})
            {
                const auto& published = manager.GetSource(GetSource(source));
                const auto& follower = synth.m_mixer.m_envelopeFollowers[tap];
                bool audio = source == x_audio[tap];
                DOCTEST_CHECK(published.m_requiresDownsampling == audio);
                for (size_t lane = 0; lane < 4; ++lane)
                {
                    DOCTEST_CHECK(published.m_value[lane] == (audio ? previousAudio[tap][lane] : previousEnvelope[tap][lane]));
                    if (!audio && SampleTimer::IsHostFrame())
                    {
                        DOCTEST_CHECK(published.m_hostValue[lane] == previousEnvelope[tap][lane]);
                    }

                    double time = state.m_firstScopeSample.load() + index;
                    DOCTEST_CHECK(ui.GetModulatorValue(ModulatorRef(source, lane), time)
                        == doctest::Approx(audio ? follower.m_audio[lane] * 0.5f + 0.5f : follower.m_envelope[lane]));
                }
            }
        }
    }
}
