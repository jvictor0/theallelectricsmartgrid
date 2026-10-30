#include "doctest.h"
#include "../support/SynthRig.hpp"

namespace
{
using Amp = SquiggleBoyVoice::AmpSection;
using Encoders = SmartGridOneEncoders;

struct SubAmpRig
{
    Amp m_amp;
    Amp::Input m_input;
    SquiggleBoyVoice::VoiceConfig m_config;

    SubAmpRig()
    {
        m_input.m_voiceConfig = &m_config;
        m_input.m_vcoTargetFreq = 50.0f / SampleTimer::x_sampleRate;
        m_input.m_gain.Update(1.0f);
        m_input.m_subGain.Update(1.0f);
        m_input.m_ahdInput.m_amplitudePolarity = false;
        m_input.m_ahdInput.m_amplitude = 0.0f;
        m_amp.m_subOut = 0.0f;
        m_amp.m_subRunning = true;
        m_amp.m_freqDependentGainTarget = 1.0f;
        m_amp.m_freqDependentGainSlew.m_output = 1.0f;
    }

    void RunSamples(size_t samples)
    {
        for (size_t sample = 0; sample < samples; ++sample)
        {
            m_amp.Process(m_input, 1.0f, 0.25f);
        }
    }
};
}

DOCTEST_TEST_CASE("Sub: envelope rise and fall have independent exponential time ranges")
{
    GlobalEnv::ResetPerTest();
    const float x_settings[][2] = {{0.0f, 1.0f}, {1.0f, 0.0f}, {0.5f, 0.5f}};
    for (const auto& setting : x_settings)
    {
        SubAmpRig rig;
        rig.m_input.m_subRiseSeconds.Update(setting[0]);
        rig.m_input.m_subFallSeconds.Update(setting[1]);
        const double riseSeconds = 0.001 * std::pow(1000.0, setting[0]);
        const double fallSeconds = 0.001 * std::pow(5000.0, setting[1]);
        DOCTEST_CHECK(rig.m_input.m_subRiseSeconds.m_expParam == doctest::Approx(riseSeconds));
        DOCTEST_CHECK(rig.m_input.m_subFallSeconds.m_expParam == doctest::Approx(fallSeconds));
        rig.RunSamples(static_cast<size_t>(std::lround(riseSeconds * SampleTimer::x_sampleRate)));
        DOCTEST_CHECK(rig.m_amp.m_subSlew.m_output == doctest::Approx(1.0 - std::exp(-1.0)).epsilon(0.002));
        DOCTEST_CHECK(rig.m_amp.m_output == doctest::Approx(1.0f));
        float peak = rig.m_amp.m_subOut;
        float envelope = rig.m_amp.m_subSlew.m_output;
        rig.m_amp.m_subRunning = false;
        rig.RunSamples(static_cast<size_t>(std::lround(fallSeconds * SampleTimer::x_sampleRate)));
        DOCTEST_CHECK(rig.m_amp.m_subSlew.m_output == doctest::Approx(envelope * std::exp(-1.0)).epsilon(0.002));
        DOCTEST_CHECK(rig.m_amp.m_subOut == doctest::Approx(peak * std::exp(-1.0)).epsilon(0.002));
        DOCTEST_CHECK(rig.m_amp.m_output == doctest::Approx(1.0f));
    }
}

DOCTEST_TEST_CASE("Sub: changing slew times retains the envelope and its direction")
{
    GlobalEnv::ResetPerTest();
    SubAmpRig rig;
    rig.m_input.m_subRiseSeconds.Update(1.0f);
    rig.m_input.m_subFallSeconds.Update(1.0f);
    rig.RunSamples(4800);
    const float rising = rig.m_amp.m_subSlew.m_output;
    const float fallAlpha = rig.m_amp.m_subSlew.m_slewDown.m_alpha;
    rig.m_input.m_subRiseSeconds.Update(0.0f);
    DOCTEST_CHECK(rig.m_amp.m_subSlew.m_output == rising);
    rig.RunSamples(48);
    DOCTEST_CHECK(rig.m_amp.m_subSlew.m_slewDown.m_alpha == fallAlpha);
    DOCTEST_CHECK(rig.m_amp.m_subSlew.m_output ==
        doctest::Approx(1.0 - (1.0 - rising) * std::exp(-1.0)));
    rig.m_amp.m_subRunning = false;
    rig.RunSamples(4800);
    const float falling = rig.m_amp.m_subSlew.m_output;
    const float riseAlpha = rig.m_amp.m_subSlew.m_slewUp.m_alpha;
    rig.m_input.m_subFallSeconds.Update(0.0f);
    DOCTEST_CHECK(rig.m_amp.m_subSlew.m_output == falling);
    rig.RunSamples(48);
    DOCTEST_CHECK(rig.m_amp.m_subSlew.m_slewUp.m_alpha == riseAlpha);
    DOCTEST_CHECK(rig.m_amp.m_subSlew.m_output == doctest::Approx(falling * std::exp(-1.0)));
}

DOCTEST_TEST_CASE("Sub: volume and drive independently control sub audio without changing the main output")
{
    GlobalEnv::ResetPerTest();
    SubAmpRig rig;
    rig.m_amp.m_subSlew.m_output = 1.0f;
    rig.RunSamples(1);
    const float lowDrive = rig.m_amp.m_subOut;
    rig.m_input.m_subTanhGain.Update(1.0f);
    rig.RunSamples(1);
    DOCTEST_CHECK(rig.m_amp.m_subOut > lowDrive * 2.0f);
    DOCTEST_CHECK(rig.m_amp.m_subSlew.m_output == 1.0f);
    DOCTEST_CHECK(rig.m_input.m_subGain.m_expParam == 1.0f);
    DOCTEST_CHECK(rig.m_amp.m_output == 1.0f);
    rig.m_input.m_subGain.Update(0.0f);
    rig.RunSamples(1000);
    DOCTEST_CHECK(std::abs(rig.m_amp.m_subOut) < 1e-6f);
    DOCTEST_CHECK(rig.m_input.m_subTanhGain.m_expParam == 8.0f);
    DOCTEST_CHECK(rig.m_amp.m_output == 1.0f);
}

DOCTEST_TEST_CASE("Sub: page controls reach each trio independently and round trip in patches")
{
    synthrig::SynthRig rig;
    auto& synth = rig.Internal().m_squiggleBoy;
    const float x_settings[3][4] =
    {
        {0.25f, 1.0f, 0.0f, 1.0f},
        {0.5f, 0.5f, 0.5f, 0.5f},
        {0.75f, 0.0f, 1.0f, 0.0f}
    };

    for (size_t trio = 0; trio < Encoders::x_numTrios; ++trio)
    {
        rig.Internal().SetActiveTrio(static_cast<TheNonagonSmartGrid::Trio>(trio));
        synth.m_encoders.SelectBank(Encoders::Bank::Sub);
        for (int x = 0; x < 4; ++x)
        {
            rig.SetEncoder(x, 0, x_settings[trio][x]);
        }

        // Let the input bus's 20 ms latency elapse before selecting another trio.
        //
        rig.RunFrames(2);
    }

    const std::string patch = rig.SavePatch();
    DOCTEST_REQUIRE_FALSE(patch.empty());
    for (bool reload : {false, true})
    {
        if (reload)
        {
            rig.ResetToDefaults();
            DOCTEST_REQUIRE(rig.LoadPatch(patch));
            rig.RunFrames(2);
        }

        for (size_t voice = 0; voice < SquiggleBoy::x_numVoices; ++voice)
        {
            const auto& input = synth.m_state[voice].m_ampInput;
            const float* setting = x_settings[voice / Encoders::x_voicesPerTrio];
            DOCTEST_CHECK(input.m_subGain.m_expParam ==
                doctest::Approx((std::pow(20.0f, setting[0]) - 1.0f) / 19.0f).epsilon(0.001));
            DOCTEST_CHECK(input.m_subTanhGain.m_expParam ==
                doctest::Approx(0.125f * std::pow(64.0f, setting[1])).epsilon(0.001));
            DOCTEST_CHECK(input.m_subRiseSeconds.m_expParam ==
                doctest::Approx(0.001f * std::pow(1000.0f, setting[2])).epsilon(0.001));
            DOCTEST_CHECK(input.m_subFallSeconds.m_expParam ==
                doctest::Approx(0.001f * std::pow(5000.0f, setting[3])).epsilon(0.001));
        }
    }

    DOCTEST_CHECK_FALSE(rig.SawNaN());
}

DOCTEST_TEST_CASE("Sub: analyzer feed separates the shaped sub from main audio and retains its tail")
{
    GlobalEnv::ResetPerTest();
    auto writer = std::make_unique<ScopeWriter>(1, static_cast<size_t>(SmartGridOne::AudioScopes::NumScopes));
    auto voice = std::make_unique<SquiggleBoyVoice>();
    voice->SetupAudioScopeWriters(writer.get(), 0);
    SubAmpRig rig;
    rig.m_amp.m_scopeWriter = voice->m_amp.m_scopeWriter;
    rig.m_amp.m_subScopeWriter = voice->m_amp.m_subScopeWriter;
    rig.m_input.m_gain.m_expParam = 0.5f;
    rig.m_amp.m_subSlew.m_output = 1.0f;
    const size_t mainScope = static_cast<size_t>(SmartGridOne::AudioScopes::PostAmp);
    const size_t subScope = static_cast<size_t>(SmartGridOne::AudioScopes::Sub);
    rig.RunSamples(1);
    DOCTEST_CHECK(writer->ReadSample(mainScope, 0, 0) == doctest::Approx(1.0f));
    DOCTEST_CHECK(writer->ReadSample(subScope, 0, 0) == doctest::Approx(rig.m_amp.m_subOut / 0.5f));
    DOCTEST_CHECK(writer->ReadSample(subScope, 0, 0) > 0.0f);
    rig.m_amp.m_subRunning = false;
    rig.m_input.m_subFallSeconds.Update(0.0f);
    for (size_t sample = 1; sample <= 600; ++sample)
    {
        writer->AdvanceIndex();
        rig.RunSamples(1);
        if (sample == 48)
        {
            DOCTEST_CHECK(rig.m_amp.m_ahd.m_state == AHD::State::Idle);
            DOCTEST_CHECK(writer->ReadSample(subScope, 0, sample) > 0.0f);
        }
    }

    DOCTEST_CHECK(std::abs(writer->ReadSample(subScope, 0, 600)) < 0.00001f);
}
